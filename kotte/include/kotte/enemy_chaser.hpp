#pragma once

#include "kotte/entity.hpp"
#include "kotte/entity_store.hpp"
#include "kotte/spatial_grid.hpp"
#include <raylib.h>
#include <functional>

namespace kotte
{
    void update_chaser_enemy(
        float dt,
        EntityHandle handle,
        const Vector2& player_pos,
        SpatialGrid& spatial_grid,
        EntityStore& entities,
        const std::function<bool(EntityHandle, Vector2)>& is_blocked_fn);
} // namespace kotte
