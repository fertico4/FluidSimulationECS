#pragma once
#include <glm/glm.hpp>

// Physics fluid constants SPH
constexpr float GRAVITY = -9.81f;
constexpr float SMOOTHING_LENGTH = 0.18f;    // Interaction's radius 'h' (match with cell size)
constexpr float REST_DENSITY = 300.f;    // Repose's fluid density (Target)
constexpr float GAS_CONSTANT = 80.f;    // Fluid rigid fact (K) for calculating pressure
constexpr float VISCOSITY = 0.8f;    // Viscosity coefficient (fluid friction)
constexpr float PARTICLE_MASS = 1.f;    // Particle mass
constexpr float PI = 3.1415926535f;

namespace Fluid {
    struct Position {
        float x{0.f}, y{0.f}, z{0.f};
    };

    struct Velocity {
        float vx{0.f}, vy{0.f}, vz{0.f};
    };

    // Specific component for physical properties from fluids (DOD)
    struct FluidProperties {
        float density{1.f};
        float pressure{0.f};
    };

    struct GPUParticle {
        glm::vec4 position;
        glm::vec4 velocity;
        glm::vec2 properties;
        glm::vec2 padding;      // Align structures to every 16 bytes (std430 rule)
    };


}