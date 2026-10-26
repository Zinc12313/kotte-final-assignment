//spatial_grid.cpp

#include "kotte/spatial_grid.hpp"

#include <algorithm>
#include <cmath>

namespace kotte
{
    SpatialGrid::SpatialGrid(Vector2 world_size, float cell_size)
        : columns_{ static_cast<int>(std::ceil(world_size.x / cell_size)) }
        , rows_{ static_cast<int>(std::ceil(world_size.y / cell_size)) }
        , cell_size_{ cell_size }
        , cells_(static_cast<std::size_t>(columns_* rows_))
    {
    }

    const std::vector<EntityHandle>& SpatialGrid::at(int column, int row) const {
        return cells_.at(static_cast<std::size_t>(row * columns_ + column));
    }

    std::vector<EntityHandle>& SpatialGrid::at(int column, int row) {
        return cells_.at(static_cast<std::size_t>(row * columns_ + column));
    }

    CellRange SpatialGrid::cells_overlapping(Rectangle bounds) const noexcept {
        if (bounds.width <= 0.0f || bounds.height <= 0.0f || cell_size_ <= 0.0f) {
            return CellRange{};
        }

        const int first_col = std::clamp(static_cast<int>(std::floor(bounds.x / cell_size_)), 0, columns_);
        const int past_last_col = std::clamp(static_cast<int>(std::ceil((bounds.x + bounds.width) / cell_size_)), 0, columns_);
        const int first_row = std::clamp(static_cast<int>(std::floor(bounds.y / cell_size_)), 0, rows_);
        const int past_last_row = std::clamp(static_cast<int>(std::ceil((bounds.y + bounds.height) / cell_size_)), 0, rows_);

        return CellRange{
            .first_column = first_col,
            .past_last_column = std::max(first_col, past_last_col),
            .first_row = first_row,
            .past_last_row = std::max(first_row, past_last_row)
        };
    }

    void SpatialGrid::insert(EntityHandle handle, Rectangle bounds) {
        const CellRange range = cells_overlapping(bounds);
        for (int row = range.first_row; row < range.past_last_row; ++row) {
            for (int col = range.first_column; col < range.past_last_column; ++col) {
                at(col, row).push_back(handle);
            }
        }
    }

    void SpatialGrid::remove(EntityHandle handle, Rectangle bounds) {
        const CellRange range = cells_overlapping(bounds);
        for (int row = range.first_row; row < range.past_last_row; ++row) {
            for (int col = range.first_column; col < range.past_last_column; ++col) {
                auto& cell = at(col, row);
                const auto it = std::ranges::find(cell, handle);
                if (it != cell.end()) {
                    cell.erase(it);
                }
            }
        }
    }

    void SpatialGrid::update(EntityHandle handle, Rectangle old_bounds, Rectangle new_bounds) {
        const CellRange old_range = cells_overlapping(old_bounds);
        const CellRange new_range = cells_overlapping(new_bounds);

        if (old_range == new_range) {
            return;
        }

        remove(handle, old_bounds);
        insert(handle, new_bounds);
    }

    SpatialQuery SpatialGrid::query(Rectangle world_bounds) const {
        SpatialQuery result{};
        const CellRange range = cells_overlapping(world_bounds);

        for (int row = range.first_row; row < range.past_last_row; ++row) {
            for (int col = range.first_column; col < range.past_last_column; ++col) {
                ++result.cells_visited;

                const auto& cell = at(col, row);
                result.candidate_references += cell.size();

                result.entity_handles.insert(
                    result.entity_handles.end(),
                    cell.begin(),
                    cell.end()
                );
            }
        }

        std::ranges::sort(result.entity_handles, [](const EntityHandle& a, const EntityHandle& b) noexcept {
            if (a.slot != b.slot) {
                return a.slot < b.slot;
            }
            return a.generation < b.generation;
            });
        const auto duplicate_tail = std::ranges::unique(result.entity_handles);
        result.entity_handles.erase(duplicate_tail.begin(), duplicate_tail.end());

        return result;
    }
} // namespace kotte
