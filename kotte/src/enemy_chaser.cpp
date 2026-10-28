#include "kotte/enemy_chaser.hpp"
#include "kotte/entity.hpp"
#include "kotte/spatial_grid.hpp"
#include <cmath>

namespace kotte
{
    namespace
    {
        [[nodiscard]] Rectangle entity_world_bounds(const Entity& entity) noexcept {
            const float width = entity_width(entity.kind);
            const float height = entity_height(entity.kind);

            return Rectangle{
                entity.world_position.x - (width * 0.5f),
                entity.world_position.y - height,
                width,
                height
            };
        }
    } // namespace

    void update_chaser_enemy(
        float dt,
        EntityHandle handle,
        const Vector2& player_pos,
        SpatialGrid& spatial_grid,
        EntityStore& entities,
        const std::function<bool(EntityHandle, Vector2)>& is_blocked_fn)
    {
        Entity* enemy = entities.try_get(handle);
        if (!enemy || enemy->is_dead) return;

        const Vector2 to_player{
            player_pos.x - enemy->world_position.x,
            player_pos.y - enemy->world_position.y
        };
        const float dist_sqr = (to_player.x * to_player.x) + (to_player.y * to_player.y);
        const float detection_radius = enemy->alert ? 350.0f : 250.0f;

        enemy->alert = (dist_sqr < (detection_radius * detection_radius));

        if (enemy->alert && dist_sqr > 1.0f) {
            constexpr float chaser_speed = 85.0f;
            const float len = static_cast<float>(std::sqrt(static_cast<double>(dist_sqr)));
            const Vector2 dir{ to_player.x / len, to_player.y / len };

            const Vector2 proposed_pos{
                enemy->world_position.x + (dir.x * chaser_speed * dt),
                enemy->world_position.y + (dir.y * chaser_speed * dt)
            };

            if (!is_blocked_fn(handle, proposed_pos)) {
                const Rectangle old_bounds = entity_world_bounds(*enemy);
                enemy->world_position = proposed_pos;
                const Rectangle new_bounds = entity_world_bounds(*enemy);
                spatial_grid.update(handle, old_bounds, new_bounds);
            }
        }
    }
} // namespace kotte
