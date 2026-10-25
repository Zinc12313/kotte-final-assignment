#pragma once

#include "kotte/entity_store.hpp"

#include <algorithm>
#include <cstddef>
#include <raylib.h>
#include <vector>

namespace kotte
{
    struct CellRange final {
        int first_column{ 0 };
        int past_last_column{ 0 };
        int first_row{ 0 };
        int past_last_row{ 0 };

        constexpr bool operator==(const CellRange&) const noexcept = default;
    };

    struct SpatialQuery final {
        std::size_t cells_visited{ 0 };
        std::size_t candidate_references{ 0 };
        std::vector<EntityHandle> entity_handles{};
    };

    class SpatialGrid final {
    public:
        SpatialGrid(Vector2 world_size, float cell_size);

        void insert(EntityHandle handle, Rectangle bounds);
        void remove(EntityHandle handle, Rectangle bounds);
        void update(EntityHandle handle, Rectangle old_bounds, Rectangle new_bounds);

        [[nodiscard]] SpatialQuery query(Rectangle world_bounds) const;
        [[nodiscard]] CellRange cells_overlapping(Rectangle bounds) const noexcept;

        [[nodiscard]] int columns() const noexcept { return columns_; }
        [[nodiscard]] int rows() const noexcept { return rows_; }
        [[nodiscard]] float cell_size() const noexcept { return cell_size_; }

    private:
        [[nodiscard]] const std::vector<EntityHandle>& at(int column, int row) const;
        [[nodiscard]] std::vector<EntityHandle>& at(int column, int row);

        int columns_{ 0 };
        int rows_{ 0 };
        float cell_size_{ 320.0f };
        std::vector<std::vector<EntityHandle>> cells_{};
    };
} // namespace kotte
