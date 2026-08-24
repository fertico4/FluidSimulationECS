#pragma once
#include "Components.h"
#include "Physics/SpatialGrid.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <vector>

namespace FluidSimulationSystem {
inline Fluid::SpatialGrid g_SpatialGrid(SMOOTHING_LENGTH);

constexpr float H_POW_3 =
    SMOOTHING_LENGTH * SMOOTHING_LENGTH * SMOOTHING_LENGTH;
constexpr float H_POW_6 = H_POW_3 * H_POW_3;
constexpr float H_POW_9 = H_POW_6 * H_POW_3;

// Precalculated Kernels constants
constexpr float H2 = SMOOTHING_LENGTH * SMOOTHING_LENGTH;
constexpr float POLY6_FACTOR = 315.f / (64.f * PI * H_POW_9);
constexpr float SPIKY_GRAD_FACTOR = -45.f / (PI * H_POW_6);
constexpr float VISC_LAP_FACTOR = 45.f / (PI * H_POW_6);

const float SELF_DENSITY = PARTICLE_MASS * POLY6_FACTOR * H_POW_6;

inline std::vector<glm::vec3> s_Positions;
inline std::vector<glm::vec3> s_Velocities;
inline std::vector<float> s_Densities;
inline std::vector<float> s_Pressures;
inline std::vector<entt::entity> s_EntityMap;

// Old version of EnTT UpdateDensityAndPressure
inline void UpdateDensityAndPressure(entt::registry &registry) {
  auto view =
      registry.view<Fluid::Position, Fluid::Velocity, Fluid::FluidProperties>();
  size_t particleCount = view.size_hint();
  if (particleCount == 0)
    return;

  s_Positions.resize(particleCount);
  s_Velocities.resize(particleCount);
  s_Densities.resize(particleCount);
  s_Pressures.resize(particleCount);
  s_EntityMap.resize(particleCount);

  size_t idx = 0;
  for (const auto &entity : view) {
    const auto &pos = view.get<Fluid::Position>(entity);
    const auto &vel = view.get<Fluid::Velocity>(entity);

    s_Positions[idx] = glm::vec3(pos.x, pos.y, pos.z);
    s_Velocities[idx] = glm::vec3(vel.vx, vel.vy, vel.vz);
    s_EntityMap[idx] = entity;
    idx++;
  }

  g_SpatialGrid.Build(registry);

  static std::vector<entt::entity> neighborsCache;
  neighborsCache.reserve(300);

  for (size_t i = 0; i < particleCount; ++i) {
    glm::vec3 posI = s_Positions[i];
    float sumDensity = SELF_DENSITY;

    g_SpatialGrid.GetNeighbors(posI, neighborsCache);

    for (const auto &neighborEntity : neighborsCache) {
      if (s_EntityMap[i] == neighborEntity)
        continue;

      const auto &nPos = view.get<Fluid::Position>(neighborEntity);

      float rx = posI.x - nPos.x;
      float ry = posI.y - nPos.y;
      float rz = posI.z - nPos.z;
      float r2 = rx * rx + ry * ry + rz * rz;

      if (r2 < H2) {
        float term = H2 - r2;
        sumDensity += PARTICLE_MASS * POLY6_FACTOR * (term * term * term);
      }
    }

    s_Densities[i] = sumDensity;
    float pressure = GAS_CONSTANT * (sumDensity - REST_DENSITY);
    s_Pressures[i] = (pressure < 0.0f) ? 0.0f : pressure;
  }

  for (size_t i = 0; i < particleCount; ++i) {
    auto &prop = view.get<Fluid::FluidProperties>(s_EntityMap[i]);
    prop.density = s_Densities[i];
    prop.pressure = s_Pressures[i];
  }
}

inline void ApplyForces(entt::registry &registry, float deltaTime) {
  static float simulationTime = 0.f;
  simulationTime += deltaTime;

  auto view =
      registry.view<Fluid::Position, Fluid::Velocity, Fluid::FluidProperties>();
  size_t particleCount = view.size_hint();
  if (particleCount == 0)
    return;

  static std::vector<entt::entity> neighborsCache;
  neighborsCache.reserve(300);

  for (size_t i = 0; i < particleCount; ++i) {
    glm::vec3 posI = s_Positions[i];
    glm::vec3 velI = s_Velocities[i];
    float densityI = s_Densities[i];
    float pressureI = s_Pressures[i];

    glm::vec3 forcePressure(0.f);
    glm::vec3 forceViscosity(0.f);
    glm::vec3 forceGravity(0.f, GRAVITY * PARTICLE_MASS, 0.f);

    // Senoidal force
    float noiseX = std::sin(posI.y * 2.f + simulationTime) * 0.15f;
    float noiseZ = std::cos(posI.x * 2.f + simulationTime) * 0.15f;
    glm::vec3 forceStirring(noiseX * PARTICLE_MASS, 0.f,
                            noiseZ * PARTICLE_MASS);

    g_SpatialGrid.GetNeighbors(posI, neighborsCache);

    for (const auto &neighbor : neighborsCache) {
      if (s_EntityMap[i] == neighbor)
        continue;

      const auto &nPos = view.get<Fluid::Position>(neighbor);

      float rx = posI.x - nPos.x;
      float ry = posI.y - nPos.y;
      float rz = posI.z - nPos.z;
      float r2 = rx * rx + ry * ry + rz * rz;

      if (r2 > 0.f && r2 < H2) {
        float r = std::sqrt(r2);

        const auto &nVel = view.get<Fluid::Velocity>(neighbor);
        const auto &nProp = view.get<Fluid::FluidProperties>(neighbor);

        glm::vec3 diff(rx, ry, rz);
        glm::vec3 velJ(nVel.vx, nVel.vy, nVel.vz);

        glm::vec3 dir = diff / r;
        float pTerm = SMOOTHING_LENGTH - r;

        float gradW = SPIKY_GRAD_FACTOR * (pTerm * pTerm);
        forcePressure +=
            -PARTICLE_MASS *
            ((pressureI + nProp.pressure) / (2.f * nProp.density)) * gradW *
            dir;

        float lapW = VISC_LAP_FACTOR * (SMOOTHING_LENGTH - r);
        forceViscosity +=
            VISCOSITY * PARTICLE_MASS * ((velJ - velI) / nProp.density) * lapW;
      }
    }

    // Total sum of hydrodynamic forces of Navier-Stokes divided by the density
    // (Acceleration = Force / Density)
    glm::vec3 acceleration =
        (forcePressure + forceViscosity + forceGravity + forceStirring) /
        densityI;

    auto &vel = view.get<Fluid::Velocity>(s_EntityMap[i]);
    vel.vx += acceleration.x * deltaTime;
    vel.vy += acceleration.y * deltaTime;
    vel.vz += acceleration.z * deltaTime;
  }
}

inline void IntegratePositions(entt::registry &registry, float deltaTime) {
  auto view = registry.view<Fluid::Position, Fluid::Velocity>();

  // Boundaries 3D cube (Simulating Box Collider 5x5x5m)
  constexpr float BOUND_MIN_X = -2.5f, BOUND_MAX_X = 2.5f;
  constexpr float BOUND_MIN_Y = 0.f, BOUND_MAX_Y = 5.f;
  constexpr float BOUND_MIN_Z = -2.5f, BOUND_MAX_Z = 2.5f;
  constexpr float DAMPING =
      -0.2f; // Cinematic energy loss after impact against walls

  for (const auto &entity : view) {
    auto &pos = view.get<Fluid::Position>(entity);
    auto &vel = view.get<Fluid::Velocity>(entity);

    pos.x += vel.vx * deltaTime;
    pos.y += vel.vy * deltaTime;
    pos.z += vel.vz * deltaTime;

    // Minimal umbral
    if (std::abs(vel.vx) < 0.005f)
      vel.vx = 0.f;
    if (std::abs(vel.vy) < 0.005f)
      vel.vy = 0.f;
    if (std::abs(vel.vz) < 0.005f)
      vel.vz = 0.f;

    // --- Collisions ---
    // X Axis
    if (pos.x < BOUND_MIN_X) {
      pos.x = BOUND_MIN_X;
      vel.vx *= DAMPING;
      vel.vy *= 0.95f;
    }
    if (pos.x > BOUND_MAX_X) {
      pos.x = BOUND_MAX_X;
      vel.vx *= DAMPING;
      vel.vy *= 0.95f;
    }

    // Y Axis
    if (pos.y < BOUND_MIN_Y) {
      pos.y = BOUND_MIN_Y;
      vel.vy *= DAMPING;

      vel.vx *= 0.9f;
      vel.vz *= 0.9f;
    }
    if (pos.y > BOUND_MAX_Y) {
      pos.y = BOUND_MAX_Y;
      vel.vy *= DAMPING;
    }

    // Z Axis
    if (pos.z < BOUND_MIN_Z) {
      pos.z = BOUND_MIN_Z;
      vel.vz *= DAMPING;
      vel.vy *= 0.95f;
    }
    if (pos.z > BOUND_MAX_Z) {
      pos.z = BOUND_MAX_Z;
      vel.vz *= DAMPING;
      vel.vy *= 0.95f;
    }
  }
}

inline GLuint s_ComputeShaderID = 0;
inline GLuint s_ParticleSSBO = 0;
inline bool s_GPUInitialized = false;

inline GLuint s_GridShaderID = 0;
inline GLuint s_GridCountsSSBO = 0;
inline GLuint s_GridIndicesSSBO = 0;

struct GPUGridCell {
  unsigned int count;
  unsigned int indices[64];
};

inline void SetComputeShader(GLuint shaderID) { s_ComputeShaderID = shaderID; }

inline void SetGridShader(GLuint shaderID) { s_GridShaderID = shaderID; }

inline GLuint GetParticleSSBO() { return s_ParticleSSBO; }

inline void InitializeGPUSimulation(size_t maxParticles) {
  if (s_GPUInitialized)
    return;

  // SSBO Particles
  glGenBuffers(1, &s_ParticleSSBO);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_ParticleSSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
               maxParticles * sizeof(Fluid::GPUParticle), nullptr,
               GL_DYNAMIC_DRAW);

  // SSBO Grid
  size_t totalCells = 28 * 28 * 28;

  glGenBuffers(1, &s_GridCountsSSBO);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_GridCountsSSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER, totalCells * sizeof(unsigned int),
               nullptr, GL_DYNAMIC_DRAW);

  glGenBuffers(1, &s_GridIndicesSSBO);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_GridIndicesSSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER, totalCells * 64 * sizeof(unsigned int),
               nullptr, GL_DYNAMIC_DRAW);

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  s_GPUInitialized = true;
}

inline void UpdateFluidSimulationGPU(entt::registry &registry, float deltaTime,
                                     glm::vec3 interactionPos,
                                     float interactionStrength,
                                     float interactionRadius,
                                     glm::vec3 gravityDirection) {
  auto view =
      registry.view<Fluid::Position, Fluid::Velocity, Fluid::FluidProperties>();
  size_t particleCount = view.size_hint();
  if (particleCount == 0)
    return;

  if (!s_GPUInitialized)
    InitializeGPUSimulation(particleCount);

  static bool firstFrame = true;
  if (firstFrame) {
    std::vector<Fluid::GPUParticle> gpuParticles(particleCount);
    size_t idx = 0;
    for (const auto &entity : view) {
      const auto &pos = view.get<Fluid::Position>(entity);
      const auto &vel = view.get<Fluid::Velocity>(entity);
      const auto &prop = view.get<Fluid::FluidProperties>(entity);

      gpuParticles[idx].position = glm::vec4(pos.x, pos.y, pos.z, 1.f);
      gpuParticles[idx].velocity = glm::vec4(vel.vx, vel.vy, vel.vz, 0.f);
      gpuParticles[idx].properties = glm::vec2(prop.density, prop.pressure);
      idx++;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_ParticleSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    particleCount * sizeof(Fluid::GPUParticle),
                    gpuParticles.data());
    firstFrame = false;
  }

  static float totalTime = 0.0f;
  totalTime += deltaTime;

  size_t totalCells = 28 * 28 * 28;
  GLuint numGroupsGrid = (static_cast<GLuint>(totalCells) + 63) / 64;
  GLuint numGroupsParticles = (static_cast<GLuint>(particleCount) + 63) / 64;

  // Clean Cells
  glUseProgram(s_GridShaderID);
  glUniform1i(glGetUniformLocation(s_GridShaderID, "mode"), 0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, s_GridCountsSSBO);
  glDispatchCompute(numGroupsGrid, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // Insert particles
  glUniform1i(glGetUniformLocation(s_GridShaderID, "mode"), 1);
  glUniform1i(glGetUniformLocation(s_GridShaderID, "particleCount"),
              static_cast<int>(particleCount));
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, s_ParticleSSBO);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, s_GridCountsSSBO);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, s_GridIndicesSSBO);
  glDispatchCompute(numGroupsParticles, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                  GL_ATOMIC_COUNTER_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

  // Fluid execution: Pass 0 (Density & Pressure)
  glUseProgram(s_ComputeShaderID);

  glUniform1i(glGetUniformLocation(s_ComputeShaderID, "particleCount"),
              static_cast<int>(particleCount));
  glUniform1f(glGetUniformLocation(s_ComputeShaderID, "deltaTime"), deltaTime);
  glUniform1f(glGetUniformLocation(s_ComputeShaderID, "simulationTime"),
              totalTime);

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, s_ParticleSSBO);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, s_GridCountsSSBO);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, s_GridIndicesSSBO);

  glUniform1i(glGetUniformLocation(s_ComputeShaderID, "pass"), 0);
  glDispatchCompute(numGroupsParticles, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // Fluid execution: Pass 1 (Forces, Integration & Collisions)
  glUniform1i(glGetUniformLocation(s_ComputeShaderID, "pass"), 1);
  glUniform3fv(glGetUniformLocation(s_ComputeShaderID, "interactionPos"), 1,
               &interactionPos[0]);
  glUniform1f(glGetUniformLocation(s_ComputeShaderID, "interactionRadius"),
              interactionRadius);
  glUniform1f(glGetUniformLocation(s_ComputeShaderID, "interactionStrength"),
              interactionStrength);
  glUniform3fv(glGetUniformLocation(s_ComputeShaderID, "gravityDirection"), 1,
               &gravityDirection[0]);

  glDispatchCompute(numGroupsParticles, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // Optional (Debug or render need CPU data)
  // Download data back to EnTT. Doing it invalidate GPU optimization. Slow down
  // the graphic line. Ideal stay the data in GPU and render from SSBO
  /*
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_ParticleSSBO);
  GPUParticle* ptr = (GPUParticle*)glMapBuffer(GL_SHADER_STORAGE_BUFFER,
  GL_READ_ONLY); size_t idx = 0; for (const auto& entity : view) { auto& pos =
  view.get<Fluid::Position>(entity); auto& vel =
  view.get<Fluid::Velocity>(entity);

      pos.x = ptr[idx].position.x;
      pos.y = ptr[idx].position.y;
      pos.z = ptr[idx].position.z;

      vel.vx = ptr[idx].velocity.x;
      vel.vy = ptr[idx].velocity.y;
      vel.vz = ptr[idx].velocity.z;
      idx++;
  }
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  */
}

} // namespace FluidSimulationSystem