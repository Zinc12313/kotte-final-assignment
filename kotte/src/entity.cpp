#include "kotte/entity.hpp"

namespace kotte
{
    float entity_width(EntityKind kind) noexcept {
        switch (kind) {
        case EntityKind::player:
            return 24.0f;
        case EntityKind::crate:
            return 36.0f;
        case EntityKind::bomb:
            return 20.0f;
        case EntityKind::enemy:
            return 28.0f;
        case EntityKind::powerup:
            return 22.0f;
        }
        return 20.0f;
    }

    float entity_height(EntityKind kind) noexcept {
        switch (kind) {
        case EntityKind::player:
            return 32.0f;
        case EntityKind::crate:
            return 48.0f;
        case EntityKind::bomb:
            return 24.0f;
        case EntityKind::enemy:
            return 36.0f;
        case EntityKind::powerup:
            return 22.0f;
        }
        return 20.0f;
    }

    float entity_roundness(const Entity& entity) noexcept {
        switch (entity.kind) {
        case EntityKind::player:
            return 0.3f;
        case EntityKind::crate:
            return 0.1f;
        case EntityKind::bomb:
            return 0.8f;
        case EntityKind::enemy:
            return 0.25f;
        case EntityKind::powerup:
            return 0.5f;
        }
        return 0.0f;
    }

    Color entity_color(const Entity& entity) noexcept {
        switch (entity.kind) {
        case EntityKind::player:
            return Color{ 0x00, 0x8a, 0x90, 0xff };
        case EntityKind::crate:
            return Color{ 0xeb, 0xa3, 0x54, 0xff };
        case EntityKind::bomb:
            return Color{ 0x30, 0x30, 0x30, 0xff };
        case EntityKind::enemy:
            return Color{ 0xc4, 0x3b, 0x3b, 0xff };
        case EntityKind::powerup:
            return Color{ 0x61, 0xaf, 0xef, 0xff }; // Bright Cyan/Blue for powerups
        }
        return WHITE;
    }

    bool is_solid([[maybe_unused]] EntityKind kind) noexcept {
        if (kind == EntityKind::powerup) {
            return false;
        }
        return true;
    }

    Rectangle entity_collision_bounds(const Entity& entity) noexcept {
        const float width = entity_width(entity.kind);
        const float foot_height = (entity.kind == EntityKind::powerup) ? entity_height(entity.kind) : 16.0f;
        return Rectangle{
            entity.world_position.x - (width * 0.5f),
            entity.world_position.y - foot_height,
            width,
            foot_height
        };
    }
} // namespace kotte
