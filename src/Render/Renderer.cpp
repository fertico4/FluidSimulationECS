#include "Renderer.h"
#include "ECS/Components.h"
#include <iostream>
#include <fstream>
#include <sstream>

#include "ECS/Systems.h"


namespace Fluid {
    Renderer::Renderer() {}
    Renderer::~Renderer() {}

    bool Renderer::Initialize(const std::string& vertexPath, const std::string& fragmentPath) {
        GLuint vs = LoadAndCompileShader(GL_VERTEX_SHADER, vertexPath);
        GLuint fs = LoadAndCompileShader(GL_FRAGMENT_SHADER, fragmentPath);
        if (vs == 0 || fs == 0) return false;

        LinkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);

        // Box shader
        GLuint boxVS = LoadAndCompileShader(GL_VERTEX_SHADER, "src/Render/shaders/box.vert");
        GLuint boxFS = LoadAndCompileShader(GL_FRAGMENT_SHADER, "src/Render/shaders/box.frag");
        if (boxVS == 0 || boxFS == 0) {
            std::cerr << "[Renderer] Error loading box shaders" << std::endl;
            return false;
        }

        m_boxShaderProgram = glCreateProgram();
        glAttachShader(m_boxShaderProgram, boxVS);
        glAttachShader(m_boxShaderProgram, boxFS);
        glLinkProgram(m_boxShaderProgram);

        GLint success;
        glGetProgramiv(m_boxShaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(m_boxShaderProgram, 512, NULL, infoLog);
            std::cerr << "[Renderer] Error linking box shaders" << std::endl;
            return false;
        }

        glDeleteShader(boxVS);
        glDeleteShader(boxFS);

        float boxVertices[] = {
            // Base (Y = 0)
            -2.5f, 0.0f, -2.5f,  2.5f, 0.0f, -2.5f,
             2.5f, 0.0f, -2.5f,  2.5f, 0.0f,  2.5f,
             2.5f, 0.0f,  2.5f, -2.5f, 0.0f,  2.5f,
            -2.5f, 0.0f,  2.5f, -2.5f, 0.0f, -2.5f,
            // Top (Y = 5)
            -2.5f, 5.0f, -2.5f,  2.5f, 5.0f, -2.5f,
             2.5f, 5.0f, -2.5f,  2.5f, 5.0f,  2.5f,
             2.5f, 5.0f,  2.5f, -2.5f, 5.0f,  2.5f,
            -2.5f, 5.0f,  2.5f, -2.5f, 5.0f, -2.5f,
            // Vertical columns
            -2.5f, 0.0f, -2.5f, -2.5f, 5.0f, -2.5f,
             2.5f, 0.0f, -2.5f,  2.5f, 5.0f, -2.5f,
             2.5f, 0.0f,  2.5f,  2.5f, 5.0f,  2.5f,
            -2.5f, 0.0f,  2.5f, -2.5f, 5.0f,  2.5f
        };
        // Box VAO config
        glGenVertexArrays(1, &m_boxVAO);
        glGenBuffers(1, &m_boxVBO);
        glBindVertexArray(m_boxVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_boxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(boxVertices), &boxVertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        glDisableVertexAttribArray(1);
        glBindVertexArray(0);

        // Quad VAO config
        float quadVertices[] = {
            -0.5f, 0.5f,
            -0.5f, -0.5f,
            0.5f, -0.5f,

            -0.5f, 0.5f,
            0.5f, -0.5f,
            0.5f, 0.5f
        };

        glGenVertexArrays(1, &m_quadVAO);
        glGenBuffers(1, &m_quadVBO);
        glGenBuffers(1, &m_instanceVBO);

        glBindVertexArray(m_quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

        // Position of Quad's vertices
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        glEnableVertexAttribArray(1);   // Instance's position
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glVertexAttribDivisor(1, 1);        // This attribute changes for every instance, not for every vertice

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        return true;
    }

    void Renderer::RenderParticles(entt::registry &registry, const glm::mat4& projection, const glm::mat4& view) {
        auto viewECS = registry.view<Position>();
        size_t particleCount = viewECS.size();
        if (particleCount == 0) return;

        glUseProgram(m_boxShaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(m_boxShaderProgram, "u_Projection"), 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(m_boxShaderProgram, "u_View"), 1, GL_FALSE, &view[0][0]);

        glBindVertexArray(m_boxVAO);
        glDisableVertexAttribArray(1);

        glLineWidth(2.f);
        glDrawArrays(GL_LINES, 0, 24);
        glBindVertexArray(0);

        glUseProgram(m_shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "u_Projection"), 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "u_View"), 1, GL_FALSE, &view[0][0]);

        glBindVertexArray(m_quadVAO);
        // Hybrid pipeline
        GLuint ssbo = FluidSimulationSystem::GetParticleSSBO();
        if (ssbo != 0) {
            glBindBuffer(GL_ARRAY_BUFFER, ssbo);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Fluid::GPUParticle), (void*)0);
            glVertexAttribDivisor(1, 1);
        }
        else {
            m_positionCache.clear();
            m_positionCache.reserve(particleCount * 3);
            for (const auto& entity : viewECS) {
                const auto& pos = viewECS.get<Position>(entity);
                m_positionCache.push_back(pos.x);
                m_positionCache.push_back(pos.y);
                m_positionCache.push_back(pos.z);
            }
            glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
            glBufferData(GL_ARRAY_BUFFER, m_positionCache.size() * sizeof(float), m_positionCache.data(), GL_DYNAMIC_DRAW);

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glVertexAttribDivisor(1, 1);
        }

        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(particleCount));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    GLuint Renderer::LoadAndCompileShader(GLenum type, const std::string &path) {
        std::string shaderCode;
        std::ifstream shaderFile;

        // Check if ifstream objects can throw exceptions
        shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        std::vector<std::string> pathAttempts = { path };

        size_t lastSlash = path.find_last_of("/\\");
        std::string filename = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

        if (filename != path) {
            pathAttempts.push_back("shaders/" + filename);
            pathAttempts.push_back("../shaders/" + filename);
            pathAttempts.push_back("../" + path);
            pathAttempts.push_back("../../" + path);
        }

        bool loaded = false;
        std::string triedPaths;
        for (const auto& attempt : pathAttempts) {
            try {
                shaderFile.open(attempt);
                std::stringstream shaderStream;
                shaderStream << shaderFile.rdbuf();
                shaderFile.close();
                shaderCode = shaderStream.str();
                loaded = true;
                break;
            }
            catch (std::ifstream::failure& e) {
                if (!triedPaths.empty()) triedPaths += ", ";
                triedPaths += attempt;
                if (shaderFile.is_open()) {
                    shaderFile.close();
                }
            }
        }

        if (!loaded) {
            std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ. Tried paths: [" << triedPaths << "]" << std::endl;
            return 0;
        }

        const char* shaderSource = shaderCode.c_str();

        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &shaderSource, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Error compiling shader (" << path << "): " << infoLog << std::endl;
            return 0;
        }
        return shader;
    }

    void Renderer::LinkProgram(GLuint vertexShader, GLuint fragmentShader) {
        m_shaderProgram = glCreateProgram();
        glAttachShader(m_shaderProgram, vertexShader);
        glAttachShader(m_shaderProgram, fragmentShader);
        glLinkProgram(m_shaderProgram);

        GLint success;
        glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(m_shaderProgram, 512, nullptr, infoLog);
            std::cerr << "Error linking shader program: " << infoLog << std::endl;
        }
    }

    void Renderer::Shutdown() {
        if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
        if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
        if (m_instanceVBO) glDeleteBuffers(1, &m_instanceVBO);
        if (m_shaderProgram) glDeleteProgram(m_shaderProgram);
    }
}
