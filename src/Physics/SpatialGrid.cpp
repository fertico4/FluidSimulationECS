#include "SpatialGrid.h"
#include "ECS/Components.h"
#include <cmath>

namespace Fluid {
    SpatialGrid::SpatialGrid(float cellSize) : m_cellSize(cellSize) {
        m_gridTable.resize(TABLE_SIZE);
    }

    glm::ivec3 SpatialGrid::GetCellIndex(const glm::vec3 &position) const {
        return glm::ivec3(
            static_cast<int>(std::floor(position.x / m_cellSize)),
            static_cast<int>(std::floor(position.y / m_cellSize)),
            static_cast<int>(std::floor(position.z / m_cellSize))
        );
    }

    size_t SpatialGrid::HashCell(const glm::ivec3 &cellIdx) const {
        // Big primes to spread tridimensional hash uniformly
        return (static_cast<size_t>(cellIdx.x * 73856093) ^
                static_cast<size_t>(cellIdx.y * 19349663) ^
                static_cast<size_t>(cellIdx.z * 83492791)) % TABLE_SIZE;
    }

    // ??: Por qué tengo que limpiar el contador de tamaño (aunque se mantiene en la memoria reservada) y luego asignar un ID a cada entidad en su celda cada frame??
    void SpatialGrid::Build(entt::registry &registry) {
        for (auto& cell : m_gridTable) {
            cell.entities.clear();
        }

        auto view = registry.view<Position>();
        for (const auto& entity : view) {
            const auto& pos = view.get<Position>(entity);
            glm::ivec3 cellIdx = GetCellIndex(glm::vec3(pos.x, pos.y, pos.z));
            size_t hash = HashCell(cellIdx);

            m_gridTable[hash].entities.push_back(entity);
        }
    }

    void SpatialGrid::GetNeighbors(const glm::vec3 &position, std::vector<entt::entity> &out_neighbors) const {
        out_neighbors.clear();
        glm::ivec3 centerCell = GetCellIndex(position);

        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                for (int z = -1; z <= 1; ++z) {
                    glm::ivec3 currentCell = centerCell + glm::ivec3(x, y, z);
                    size_t hash = HashCell(currentCell);

                    const auto& cell = m_gridTable[hash];
                    for (auto entity : cell.entities) {
                        out_neighbors.push_back(entity);
                    }
                    // out_neighbors.insert(out_neighbors.end(), cell.entities.begin(), cell.entities.end());
                }
            }
        }
    }
}
