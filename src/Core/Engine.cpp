#include "Engine.h"
#include "ECS/Systems.h"
#include "ECS/Components.h"
#include <iostream>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <fstream>


namespace Fluid {

Engine::Engine() {
  // Constructor
}

Engine::~Engine() { Shutdown(); }

bool Engine::Initialize(int width, int height, const char *title) {
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW." << std::endl;
    return false;
  }

  // OpenGL 4.1 Core Profile Context Config
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

#if defined(__APPLE__) || defined(__MACH__)
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
#endif

  m_window = glfwCreateWindow(width, height, title, NULL, NULL);
  if (!m_window) {
    std::cerr << "Failed to create GLFW window." << std::endl;
    glfwTerminate();
    return false;
  }

  glfwSetWindowUserPointer(m_window, this);
  glfwSetMouseButtonCallback(m_window, MouseButtonCallback);
  glfwSetCursorPosCallback(m_window, CursorPositionCallback);
  glfwSetScrollCallback(m_window, ScrollCallback);

  glfwMakeContextCurrent(m_window);
  glfwSwapInterval(1); // Activate VSync to avoid tearing

  if (!gladLoadGL((
          GLADloadfunc)glfwGetProcAddress)) { // Nota: en GLAD 2 la funcion es
                                              // gladLoadGL, no gladLoadGLLoader
    std::cerr << "Error al inicializar GLAD / OpenGL pointers" << std::endl;
    return false;
  }

  m_renderer = std::make_unique<Renderer>();
  if (!m_renderer->Initialize("src/Render/shaders/particle.vert",
                              "src/Render/shaders/particle.frag")) {
    std::cerr << "Error initializing Renderer" << std::endl;
    return false;
  }

  // Orthographic projection
  m_projectionMatrix = glm::perspective(
      glm::radians(45.f), static_cast<float>(width) / height, 0.1f, 100.f);
  UpdateCameraMatrices();
  glEnable(GL_DEPTH_TEST);

  std::cout << "[Engine] Projection matrix from camera initialized (20mx15m)"
            << std::endl;

  // Generate a scene with 5000 particles in
  SetupDemoScene(5000);

#if !defined(__APPLE__) && !defined(__MACH__)
  GLuint computeProg = LoadComputeShader("src/ECS/FluidSimulation.comp");
  if (computeProg != 0) {
    FluidSimulationSystem::SetComputeShader(computeProg);
    std::cout << "[Engine] Compute shader program initialized, ID: "
              << computeProg << std::endl;
  } else {
    std::cerr << "[Engine] Error to compile or link Compute Shader."
              << std::endl;
  }

  GLuint gridShaderProg = LoadComputeShader("src/ECS/GridBuilder.comp");
  if (gridShaderProg != 0) {
    FluidSimulationSystem::SetGridShader(gridShaderProg);
    std::cout << "[Engine] Grid shader program initialized, ID: "
              << gridShaderProg << std::endl;
  } else {
    std::cerr
        << "[Engine] Error to compile or link Grid Builder Compute Shader."
        << std::endl;
  }
#endif

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(m_window, true);
  ImGui_ImplOpenGL3_Init("#version 430 core");

  m_lastFrame = static_cast<float>(glfwGetTime());
  return true;
}

void Engine::SetupDemoScene(int particleCount) {
  srand(static_cast<unsigned int>(time(nullptr)));
  for (int i = 0; i < particleCount; ++i) {
    auto entity = m_registry.create();

    float randomX = ((rand() % 100) / 100.0f) * 2.f - 1.f;
    float randomY = 3.f + ((rand() % 100) / 100.0f) * 1.5f;
    float randomZ = ((rand() % 100) / 100.0f) * 2.f - 1.f;

    m_registry.emplace<Position>(entity, randomX, randomY, randomZ);
    m_registry.emplace<Velocity>(entity, 0.f, 0.f, 0.f);
    m_registry.emplace<FluidProperties>(entity);
  }
  std::cout << "ECS Initialized with " << particleCount << " particles."
            << std::endl;
}

void Engine::UpdateCameraMatrices() {
  // Lock the polar angle to avoid Gimbal lock
  if (m_cameraPolar < 0.01f)
    m_cameraPolar = 0.01f;
  if (m_cameraPolar > glm::pi<float>() - 0.01f)
    m_cameraPolar = glm::pi<float>() - 0.01f;

  // From spherical coords to 3D cartesian coords
  glm::vec3 cameraPosition;
  cameraPosition.x = m_cameraTarget.x +
                     m_cameraRadius * sin(m_cameraPolar) * sin(m_cameraAzimuth);
  cameraPosition.y = m_cameraTarget.y + m_cameraRadius * cos(m_cameraPolar);
  cameraPosition.z = m_cameraTarget.z +
                     m_cameraRadius * sin(m_cameraPolar) * cos(m_cameraAzimuth);

  m_viewMatrix =
      glm::lookAt(cameraPosition, m_cameraTarget, glm::vec3(0.f, 1.f, 0.f));
}

void Engine::ProcessInput() {
  if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(m_window, true);

  // Reset gravity by default (Spacabar)
  if (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS) {
    m_gravity = glm::vec3(0.f, -9.81f, 0.f);
  }

  // Rotate gravity with key arrows or WASD
  float tiltStrength = 5.f;
  if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_LEFT) == GLFW_PRESS)
    m_gravity.x = -tiltStrength;
  else if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_RIGHT) == GLFW_PRESS)
    m_gravity.x = tiltStrength;
  else
    m_gravity.x = 0.f;

  if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_UP) == GLFW_PRESS)
    m_gravity.z = -tiltStrength;
  else if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_DOWN) == GLFW_PRESS)
    m_gravity.z = tiltStrength;
  else
    m_gravity.z = 0.f;


}

// Inputs callbacks

void Engine::MouseButtonCallback(GLFWwindow *window, int button, int action,
                                 int mods) {
  auto *engine = static_cast<Engine *>(glfwGetWindowUserPointer(window));
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    if (action == GLFW_PRESS) {
      engine->m_isMousePressed = true;
      glfwGetCursorPos(window, &engine->m_lastMouseX, &engine->m_lastMouseY);
    } else if (action == GLFW_RELEASE) {
      engine->m_isMousePressed = false;
    }
  }

  if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    if (action == GLFW_PRESS) {
      engine->m_isRighMousePressed = true;
    }
    else if (action == GLFW_RELEASE) {
      engine->m_isRighMousePressed = false;
    }
  }
}

void Engine::CursorPositionCallback(GLFWwindow *window, double xpos,
                                    double ypos) {
  auto *engine = static_cast<Engine *>(glfwGetWindowUserPointer(window));
  if (engine->m_isMousePressed) {
    double deltaX = xpos - engine->m_lastMouseX;
    double deltaY = ypos - engine->m_lastMouseY;
    engine->m_lastMouseX = xpos;
    engine->m_lastMouseY = ypos;

    // Mouse Sensitivity
    float sensitivity = 0.005f;
    engine->m_cameraAzimuth -= static_cast<float>(deltaX) * sensitivity;
    engine->m_cameraPolar -= static_cast<float>(deltaY) * sensitivity;

    engine->UpdateCameraMatrices();
  }
}

void Engine::ScrollCallback(GLFWwindow *window, double xoffset,
                            double yoffset) {
  auto *engine = static_cast<Engine *>(glfwGetWindowUserPointer(window));

  engine->m_cameraRadius -= static_cast<float>(yoffset) * 0.5f;
  if (engine->m_cameraRadius < 1.f)
    engine->m_cameraRadius = 1.f;
  if (engine->m_cameraRadius > 30.f)
    engine->m_cameraRadius = 30.f;

  engine->UpdateCameraMatrices();
}

glm::vec3 Engine::GetMouseWorldPos() {
  double xpos, ypos;
  glfwGetCursorPos(m_window, &xpos, &ypos);

  int width, height;
  glfwGetWindowSize(m_window, &width, &height);

  float glX = static_cast<float>(xpos);
  float glY = static_cast<float>(height) - static_cast<float>(ypos);
  glm::vec4 viewport = glm::vec4(0.f, 0.f, static_cast<float>(width), static_cast<float>(height));

  glm::vec3 rayNear = glm::unProject(glm::vec3(glX, glY, 0.f), m_viewMatrix, m_projectionMatrix, viewport);
  glm::vec3 rayFar = glm::unProject(glm::vec3(glX, glY, 1.f), m_viewMatrix, m_projectionMatrix, viewport);

  glm::vec3 rayDir = glm::normalize(rayFar - rayNear);

  glm::vec3 planePoint = m_cameraTarget;
  glm::vec3 planeNormal = glm::vec3(0.f, 0.f, 1.f);

  float denominator = glm::dot(rayDir, planeNormal);
  if (std::abs(denominator) > 0.0001f) {
    float t = glm::dot(planePoint - rayNear, planeNormal) / denominator;
    return rayNear + rayDir * t;
  }

  return m_cameraTarget;
}

GLuint Engine::LoadComputeShader(const char *filepath) {
  // Intelligent recursive file search
  std::string shaderCode;
  std::ifstream shaderFile;
  shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

  std::string pathStr(filepath);
  std::vector<std::string> pathAttempts = {pathStr};

  size_t lastSlash = pathStr.find_last_of("/\\");
  std::string filename = (lastSlash == std::string::npos)
                             ? pathStr
                             : pathStr.substr(lastSlash + 1);

  if (filename != pathStr) {
    pathAttempts.push_back(filename);
    pathAttempts.push_back("../" + pathStr);
    pathAttempts.push_back("../../" + pathStr);
    pathAttempts.push_back("src/ECS/" + filename);
  }

  bool loaded = false;
  std::string triedPaths;
  for (const auto &attempt : pathAttempts) {
    try {
      shaderFile.open(attempt);
      std::stringstream shaderStream;
      shaderStream << shaderFile.rdbuf();
      shaderFile.close();
      shaderCode = shaderStream.str();
      loaded = true;
      break;
    } catch (std::ifstream::failure &e) {
      if (!triedPaths.empty())
        triedPaths += ", ";
      triedPaths += attempt;
      if (shaderFile.is_open()) {
        shaderFile.close();
      }
    }
  }

  if (!loaded) {
    std::cerr << "Failed to load compute shader. Tried paths: [" << triedPaths
              << "]" << std::endl;
    return 0;
  }

  const char *src = shaderCode.c_str();

  // Shader compilation
  GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(computeShader, 1, &src, nullptr);
  glCompileShader(computeShader);
  GLint success;
  glGetShaderiv(computeShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    GLchar infoLog[1024];
    glGetShaderInfoLog(computeShader, 1024, nullptr, infoLog);
    std::cerr << "Compute shader compilation failed: " << infoLog << std::endl;
    return 0;
  }

  GLuint program = glCreateProgram();
  glAttachShader(program, computeShader);
  glLinkProgram(program);

  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    GLchar infoLog[1024];
    glGetProgramInfoLog(program, 1024, nullptr, infoLog);
    std::cerr << "Program linking failed: " << infoLog << std::endl;
    return 0;
  }

  glDeleteShader(computeShader);
  return program;
}

void Engine::Update(float deltaTime) {
  glm::vec3 clickWorldPos = glm::vec3(0.0f);
  float currentStrength = 0.0f;
  float clickRadius = 1.2f;

  if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
    currentStrength = strength;
    clickWorldPos = GetMouseWorldPos();
  }

  auto startUpdate = std::chrono::high_resolution_clock::now();

  auto t0 = std::chrono::high_resolution_clock::now();
  FluidSimulationSystem::UpdateFluidSimulationGPU(
      m_registry, deltaTime, clickWorldPos, currentStrength, clickRadius, GetGravityDirection());
  auto t1 = std::chrono::high_resolution_clock::now();
  double msUpdate = std::chrono::duration<double, std::milli>(t1 - t0).count();

  auto endUpdate = std::chrono::high_resolution_clock::now();
  double msTotalUpdate =
      std::chrono::duration<double, std::milli>(endUpdate - startUpdate)
          .count();

  static int frameCount = 0;
  static double accumUpdate = 0.f, accumTotal = 0.f;

  accumUpdate += msUpdate;
  accumTotal += msTotalUpdate;
  frameCount++;

  if (frameCount >= 100) {
    std::cout << "\n--- PROFILER FÍSICO (Media de 100 frames) ---" << std::endl;
    std::cout << "Lo que tarda Update: " << accumUpdate / 100.0 << " ms"
              << std::endl;
    std::cout << "TOTAL PASO FÍSICO:           " << accumTotal / 100.0 << " ms"
              << std::endl;
    std::cout << "---------------------------------------------" << std::endl;

    frameCount = 0;
    accumUpdate = 0.0;
    accumTotal = 0.0;
  }
}

void Engine::RenderUI() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::Begin("Engine Control Panel & Profiler");

  ImGui::Text("Application Average: %.3f ms/frame (%.1f FPS)", 1000.f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
  ImGui::Separator();

  ImGui::Text("Fluid Parameters");
  
  ImGui::SliderFloat("Gravity Y", &m_gravity.y, -20.0f, 0.0f);
  ImGui::SliderFloat("Click Strength", &strength, 100.0f, 5000.0f);

  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Engine::Render() {
  glClearColor(0.1f, 0.1f, 0.1f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (m_renderer) {
    m_renderer->RenderParticles(m_registry, m_projectionMatrix, m_viewMatrix);
  }

  RenderUI();

  glfwSwapBuffers(m_window);
}

void Engine::Run() {
  while (!glfwWindowShouldClose(m_window)) {
    glfwPollEvents();

    float currentFrame = static_cast<float>(glfwGetTime());
    float deltaTime = currentFrame - m_lastFrame;
    m_lastFrame = currentFrame;

    if (deltaTime > 0.1f)
      deltaTime = 0.1f;

    ProcessInput();

    // FISICAL SUBSTEPPING
    // Instead of update with a big deltaTime once, we split up the frame into
    // fixed steps of 0.002 seconds
    constexpr float physicsTimeStep = 0.002f;
    static float accumulator = 0.f;
    accumulator += deltaTime;

    while (accumulator >= physicsTimeStep) {
      Update(physicsTimeStep);
      accumulator -= physicsTimeStep;
    }
    Render();
  }
}

void Engine::Shutdown() {

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (m_window) {
    glfwDestroyWindow(m_window);
    m_window = nullptr;
  }
  if (m_renderer) {
    m_renderer->Shutdown();
    m_renderer.reset();
  }
  glfwTerminate();
}
} // namespace Fluid
