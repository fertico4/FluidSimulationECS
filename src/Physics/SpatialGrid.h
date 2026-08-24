#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <entt/entt.hpp>

namespace Fluid {
    struct GridCell {
        std::vector<entt::entity> entities;
    };

    // Spatial Hash Grid
    class SpatialGrid {
        float m_cellSize;

        // From 3D world coordinate into an unique cell index (Hash)
        glm::ivec3 GetCellIndex(const glm::vec3& position) const;
        size_t HashCell(const glm::ivec3& cellIdx) const;

        // Plane hash table for special grid
        // Const big size to avoid heavy dynamic reallocations per frame
        static constexpr size_t TABLE_SIZE = 16381;
        std::vector<GridCell> m_gridTable;
    public:
        SpatialGrid(float cellSize);
        ~SpatialGrid() = default;

        void Build(entt::registry& registry);

        void GetNeighbors(const glm::vec3& position, std::vector<entt::entity>& out_neighbors) const;
    };
}
