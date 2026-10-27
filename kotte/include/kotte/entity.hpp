#pragma once

#include "kotte/random.hpp"
#include "kotte/tile_map.hpp"

#include <raylib.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace kotte
{
    enum class EntityKind : std::uint8_t {
        player,
        crate,
        bomb,
        enemy,
        powerup
    };

    struct Entity final {
        EntityKind kind{ EntityKind::crate };
        Vector2 world_position{};
        Vector2 heading{ 0.0f, 1.0f };
        bool alert{ false };
        float fuse_seconds{ 2.0f };
        std::size_t owner_slot{ 0 };
        float blast_range_tiles{ 3.0f };//range

        int lives{ 3 };
        bool is_dead{ false };
    };

    [[nodiscard]] float entity_width(EntityKind kind) noexcept;
    [[nodiscard]] float entity_height(EntityKind kind) noexcept;
    [[nodiscard]] float entity_roundness(const Entity& entity) noexcept;
    [[nodiscard]] Color entity_color(const Entity& entity) noexcept;
    [[nodiscard]] bool is_solid(EntityKind kind) noexcept;
    [[nodiscard]] Rectangle entity_collision_bounds(const Entity& entity) noexcept;
} // namespace kotte
