#pragma once
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <entt/entt.hpp>
#include <memory>

#include "Render/Renderer.h"

namespace Fluid {
class Engine {
  void ProcessInput();
  void Update(float deltaTime);
  void Render();
  void RenderUI();
  void SetupDemoScene(int particleCount);

  GLFWwindow *m_window{nullptr};
  entt::registry m_registry;
  float m_lastFrame{0.0f};
  std::unique_ptr<Renderer> m_renderer;
  float m_lastFrameTime{0.f};

  // Matrix 3D
  glm::mat4 m_projectionMatrix{1.f};
  glm::mat4 m_viewMatrix{1.f};

  // Orbital camera parameters
  glm::vec3 m_cameraTarget{0.f, 2.f, 0.f};
  float m_cameraRadius{8.f};
  float m_cameraAzimuth{0.f};
  float m_cameraPolar{glm::radians(60.f)};

  // Mouse states
  bool m_isMousePressed{false};
  bool m_isRighMousePressed{false};
  double m_lastMouseX{0.f}, m_lastMouseY{0.f};

  void UpdateCameraMatrices();

    glm::vec3 m_gravity = glm::vec3(0.f, -9.81f, 0.f);
    float strength = 1500.f;

public:
  Engine();
  ~Engine();

  bool Initialize(int width, int height, const char *title);
  void Run();
  void Shutdown();

  // Callbacks mouse
  static void CursorPositionCallback(GLFWwindow *window, double xpos,
                                     double ypos);
  static void MouseButtonCallback(GLFWwindow *window, int button, int action,
                                  int mods);
  static void ScrollCallback(GLFWwindow *window, double xoffset,
                             double yoffset);

  glm::vec3 GetMouseWorldPos();

  GLuint LoadComputeShader(const char *filepath);

    glm::vec3 GetGravityDirection() { return m_gravity; };
};
} // namespace Fluid
