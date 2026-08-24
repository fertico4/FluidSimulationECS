#pragma once
#include <glad/gl.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace Fluid {
    class Renderer {
        GLuint m_shaderProgram{0};
        GLuint m_boxShaderProgram{0};
        GLuint m_quadVAO{0};
        GLuint m_boxVAO{0};
        GLuint m_quadVBO{0};
        GLuint m_boxVBO{0};
        GLuint m_instanceVBO{0};

        std::vector<float> m_positionCache;

        GLuint LoadAndCompileShader(GLenum type, const std::string& path);
        void LinkProgram(GLuint vertexShader, GLuint fragmentShader);
    public:
        Renderer();
        ~Renderer();

        bool Initialize(const std::string& vertexPath, const std::string& fragmentPath);
        void RenderParticles(entt::registry& registry, const glm::mat4& projection, const glm::mat4& view);
        void Shutdown();
    };
}