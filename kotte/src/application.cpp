#include "kotte/application.hpp"

#include <raylib.h>
#include <cmath>
#include <random>

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

        [[nodiscard]] Vector2 turn_right(Vector2 heading) noexcept {
            if (heading.y < 0.0f) return Vector2{ 1.0f, 0.0f };
            if (heading.x > 0.0f) return Vector2{ 0.0f, 1.0f };
            if (heading.y > 0.0f) return Vector2{ -1.0f, 0.0f };
            return Vector2{ 0.0f, -1.0f };
        }
    } // namespace

    Application::Application(
        int width,
        int height,
        std::string_view title,
        std::uint64_t seed,
        int target_fps)
        : window_{ width, height, title, target_fps }
        , random_{ seed }
        , map_{ make_room(200, 120, random_) }
        , seed_{ seed }
        , camera_{ window_.size() }
        , spatial_grid_{
            Vector2{ static_cast<float>(map_.width()) * tile_size_, static_cast<float>(map_.height()) * tile_size_ },
            spatial_cell_size_
        }
    {
        populate_initial_world();
        update_camera();
    }

    void Application::populate_initial_world() {
        const Vector2 player_pos{
            (static_cast<float>(map_.width()) * 0.5f) * tile_size_,
            (static_cast<float>(map_.height()) * 0.5f) * tile_size_
        };

        player_handle_ = entities_.spawn(Entity{
            .kind = EntityKind::player,
            .world_position = player_pos
            });
        spatial_grid_.insert(player_handle_, entity_world_bounds(*entities_.try_get(player_handle_)));

        std::uniform_real_distribution<float> dist_01(0.0f, 1.0f);
        std::uniform_real_distribution<float> dist_offset(10.0f, 30.0f);

        for (int y = 1; y < map_.height() - 1; ++y) {
            for (int x = 1; x < map_.width() - 1; ++x) {
                if (map_.at(x, y).type == TileType::wall) {
                    continue;
                }

                if (std::abs(static_cast<float>(x) * tile_size_ - player_pos.x) < 80.0f &&
                    std::abs(static_cast<float>(y) * tile_size_ - player_pos.y) < 80.0f) {
                    continue;
                }

                if (dist_01(random_) < 0.18f) {
                    const float kind_roll = dist_01(random_);
                    EntityKind kind = EntityKind::crate;
                    if (kind_roll < 0.25f) {
                        kind = EntityKind::enemy;
                    }

                    const Vector2 pos{
                        static_cast<float>(x) * tile_size_ + dist_offset(random_),
                        static_cast<float>(y) * tile_size_ + dist_offset(random_)
                    };

                    const EntityHandle handle = entities_.spawn(Entity{
                        .kind = kind,
                        .world_position = pos,
                        .heading = Vector2{ 0.0f, 1.0f }
                        });

                    spatial_grid_.insert(handle, entity_world_bounds(*entities_.try_get(handle)));

                    if (kind == EntityKind::enemy) {
                        enemy_handles_.push_back(handle);
                    }
                }
            }
        }
    }

    const Entity& Application::player() const noexcept {
        return *entities_.try_get(player_handle_);
    }

    Entity& Application::player() noexcept {
        return *entities_.try_get(player_handle_);
    }

    void Application::update_camera() {
        camera_.set_viewport_size(window_.size());
        camera_.set_centre(player().world_position);
    }

    void Application::run() {
        while (!window_.should_close() && !exit_requested_) {
            update(GetFrameTime());
            render();
        }
    }

    void Application::update(float dt) {
        if (IsKeyDown(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) {
            request_exit();
        }

        update_player_input(dt);
        update_enemies(dt);
        update_bombs(dt);

        // Week 6 Structural Mutation Phase: Execute deferred removals
        apply_entity_destructions();

        // Update active blast visual timers
        for (auto& blast : active_blasts_) {
            blast.remaining_seconds -= dt;
        }
        std::erase_if(active_blasts_, [](const BlastEffect& b) {
            return b.remaining_seconds <= 0.0f;
            });

        update_camera();
    }

    void Application::update_player_input(float dt) {
        if (player().is_dead) return; // Stop input if dead

        constexpr float player_speed = 200.0f;
        float move_x = 0.0f;
        float move_y = 0.0f;

        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) move_x -= 1.0f;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) move_x += 1.0f;
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) move_y -= 1.0f;
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) move_y += 1.0f;

        if (move_x != 0.0f || move_y != 0.0f) {
            try_move_player(move_x * player_speed * dt, move_y * player_speed * dt);
        }

        if (IsKeyPressed(KEY_SPACE)) {
            try_place_bomb();
        }
    }

    void Application::try_place_bomb() {
        if (player().is_dead) return;

        // Enforce 1 active bomb per player
        for (const EntityHandle h : active_bomb_handles_) {
            const Entity* bomb = entities_.try_get(h);
            if (bomb && bomb->owner_slot == player_handle_.slot) {
                return;
            }
        }

        // Align bomb to nearest tile center
        const Vector2 p_pos = player().world_position;
        const float snap_x = (std::floor(p_pos.x / tile_size_) + 0.5f) * tile_size_;
        const float snap_y = (std::floor(p_pos.y / tile_size_) + 1.0f) * tile_size_;

        const EntityHandle bomb_handle = entities_.spawn(Entity{
            .kind = EntityKind::bomb,
            .world_position = Vector2{ snap_x, snap_y },
            .fuse_seconds = 2.0f,
            .owner_slot = player_handle_.slot
            });

        spatial_grid_.insert(bomb_handle, entity_world_bounds(*entities_.try_get(bomb_handle)));
        active_bomb_handles_.push_back(bomb_handle);
    }

    void Application::update_bombs(float dt) {
        for (const EntityHandle bomb_handle : active_bomb_handles_) {
            Entity* bomb = entities_.try_get(bomb_handle);
            if (!bomb) continue;

            bomb->fuse_seconds -= dt;
            if (bomb->fuse_seconds <= 0.0f) {
                detonate_bomb(bomb_handle, *bomb);
            }
        }
    }

    void Application::detonate_bomb(EntityHandle bomb_handle, const Entity& bomb) {
        // Request destruction of the bomb itself
        destroy_entity_requests_.push_back({ bomb_handle });

        const Vector2 center = bomb.world_position;
        constexpr float blast_range = 3.0f * tile_size_;

        // Create cross blast zone
        const Rectangle horizontal_blast{ center.x - blast_range, center.y - (tile_size_ * 0.5f), blast_range * 2.0f, tile_size_ };
        const Rectangle vertical_blast{ center.x - (tile_size_ * 0.5f), center.y - blast_range, tile_size_, blast_range * 2.0f };

        active_blasts_.push_back(BlastEffect{ .bounds = horizontal_blast });
        active_blasts_.push_back(BlastEffect{ .bounds = vertical_blast });

        // Query spatial grid for blast targets
        const SpatialQuery q1 = spatial_grid_.query(horizontal_blast);
        const SpatialQuery q2 = spatial_grid_.query(vertical_blast);

        std::vector<EntityHandle> targets = q1.entity_handles;
        targets.insert(targets.end(), q2.entity_handles.begin(), q2.entity_handles.end());

        for (const EntityHandle target_handle : targets) {
            Entity* target = entities_.try_get(target_handle);
            if (!target || target_handle == bomb_handle) continue;

            const Rectangle bounds = entity_collision_bounds(*target);
            if (CheckCollisionRecs(horizontal_blast, bounds) || CheckCollisionRecs(vertical_blast, bounds)) {
                if (target->kind == EntityKind::crate || target->kind == EntityKind::enemy) {
                    destroy_entity_requests_.push_back({ target_handle });
                }
                else if (target->kind == EntityKind::player) {
                    target->lives--;
                    if (target->lives <= 0) {
                        target->is_dead = true;
                    }
                }
            }
        }
    }

    // Week 6 Core: Apply deferred destructions safely
    void Application::apply_entity_destructions() {
        for (const auto& request : destroy_entity_requests_) {
            Entity* entity = entities_.try_get(request.entity);
            if (entity == nullptr) {
                continue;
            }

            // Remove from spatial grid using full world bounds before clearing memory
            const Rectangle world_bounds = entity_world_bounds(*entity);
            spatial_grid_.remove(request.entity, world_bounds);

            (void)entities_.destroy(request.entity);
        }

        // Clean stale handles from role lists
        std::erase_if(enemy_handles_, [this](EntityHandle handle) {
            return !entities_.contains(handle);
            });

        std::erase_if(active_bomb_handles_, [this](EntityHandle handle) {
            return !entities_.contains(handle);
            });

        destroy_entity_requests_.clear();
    }

    bool Application::entity_movement_is_blocked(EntityHandle moving_handle, Vector2 proposed_position) {
        const int tile_x = static_cast<int>(proposed_position.x / tile_size_);
        const int tile_y = static_cast<int>(proposed_position.y / tile_size_);

        if (!map_.contains(tile_x, tile_y) || map_.at(tile_x, tile_y).type == TileType::wall) {
            return true;
        }

        Entity proposed_entity = *entities_.try_get(moving_handle);
        proposed_entity.world_position = proposed_position;

        const Rectangle proposed_collision_bounds = entity_collision_bounds(proposed_entity);
        const SpatialQuery spatial_query = spatial_grid_.query(proposed_collision_bounds);

        for (const EntityHandle candidate_handle : spatial_query.entity_handles) {
            if (candidate_handle == moving_handle) {
                continue;
            }

            const Entity* candidate = entities_.try_get(candidate_handle);
            if (!candidate || !is_solid(candidate->kind)) {
                continue;
            }

            if (CheckCollisionRecs(proposed_collision_bounds, entity_collision_bounds(*candidate))) {
                return true;
            }
        }

        return false;
    }

    void Application::try_move_player(float delta_x, float delta_y) {
        Entity& p = player();
        if (p.is_dead) return;

        const Rectangle old_bounds = entity_world_bounds(p);

        if (delta_x != 0.0f) {
            const Vector2 proposed{ p.world_position.x + delta_x, p.world_position.y };
            if (!entity_movement_is_blocked(player_handle_, proposed)) {
                p.world_position.x = proposed.x;
            }
        }

        if (delta_y != 0.0f) {
            const Vector2 proposed{ p.world_position.x, p.world_position.y + delta_y };
            if (!entity_movement_is_blocked(player_handle_, proposed)) {
                p.world_position.y = proposed.y;
            }
        }

        const Rectangle new_bounds = entity_world_bounds(p);
        spatial_grid_.update(player_handle_, old_bounds, new_bounds);
    }

    void Application::update_enemies(float dt) {
        constexpr float enemy_speed = 70.0f;
        const Vector2 player_pos = player().world_position;

        for (const EntityHandle enemy_handle : enemy_handles_) {
            Entity* enemy = entities_.try_get(enemy_handle);
            if (!enemy) continue;

            const Vector2 to_player{
                player_pos.x - enemy->world_position.x,
                player_pos.y - enemy->world_position.y
            };
            const float dist_sqr = (to_player.x * to_player.x) + (to_player.y * to_player.y);
            const float detection_radius = enemy->alert ? 320.0f : 240.0f;
            enemy->alert = (dist_sqr < (detection_radius * detection_radius));

            const Vector2 proposed_pos{
                enemy->world_position.x + (enemy->heading.x * enemy_speed * dt),
                enemy->world_position.y + (enemy->heading.y * enemy_speed * dt)
            };

            if (entity_movement_is_blocked(enemy_handle, proposed_pos)) {
                enemy->heading = turn_right(enemy->heading);
            }
            else {
                const Rectangle old_bounds = entity_world_bounds(*enemy);
                enemy->world_position = proposed_pos;
                const Rectangle new_bounds = entity_world_bounds(*enemy);
                spatial_grid_.update(enemy_handle, old_bounds, new_bounds);
            }
        }
    }

    void Application::render() {
        Frame frame{ background_color_ };

        constexpr Color floor_colors[]{
            Color{0x38, 0x49, 0x52, 0xff},
            Color{0x3d, 0x4f, 0x59, 0xff},
            Color{0x42, 0x55, 0x60, 0xff}
        };

        const Rectangle view = camera_.world_view();
        const TileRange range = map_.tiles_overlapping(view, static_cast<int>(tile_size_));

        std::size_t tiles_submitted = 0;

        for (int y = range.first_row; y < range.past_last_row; ++y) {
            for (int x = range.first_column; x < range.past_last_column; ++x) {
                const Tile& tile = map_.at(x, y);
                const Color color = (tile.type == TileType::wall)
                    ? Color{ 0x78, 0x5f, 0x47, 0xff }
                : floor_colors[tile.variation % 3];

                const Vector2 world_pos{
                    static_cast<float>(x) * tile_size_,
                    static_cast<float>(y) * tile_size_
                };

                const Vector2 screen_pos = camera_.world_to_screen(world_pos);

                DrawRectangle(
                    static_cast<int>(screen_pos.x),
                    static_cast<int>(screen_pos.y),
                    static_cast<int>(tile_size_ - 1.0f),
                    static_cast<int>(tile_size_ - 1.0f),
                    color);

                ++tiles_submitted;
            }
        }

        renderer_.clear();

        // Draw active blast visual flashes (Ground layer)
        for (const auto& blast : active_blasts_) {
            const Vector2 screen_pos = camera_.world_to_screen(Vector2{ blast.bounds.x, blast.bounds.y });
            DrawRectangleRec(
                Rectangle{ screen_pos.x, screen_pos.y, blast.bounds.width, blast.bounds.height },
                Color{ 0xff, 0x8a, 0x00, 0xaa }
            );
        }

        const SpatialQuery spatial_query = spatial_grid_.query(view);

        for (const EntityHandle handle : spatial_query.entity_handles) {
            const Entity* entity = entities_.try_get(handle);
            if (!entity) continue;

            const Rectangle world_bounds = entity_world_bounds(*entity);
            if (!CheckCollisionRecs(view, world_bounds)) continue;

            const Vector2 screen_top_left = camera_.world_to_screen(Vector2{ world_bounds.x, world_bounds.y });
            const Rectangle screen_bounds{
                screen_top_left.x,
                screen_top_left.y,
                world_bounds.width,
                world_bounds.height
            };

            const Color draw_color = (entity->kind == EntityKind::enemy && entity->alert)
                ? ORANGE
                : entity_color(*entity);

            RenderCommand command{
                .screen_bounds = screen_bounds,
                .color = draw_color,
                .layer = (entity->kind == EntityKind::bomb) ? RenderLayer::ground : RenderLayer::world,
                .depth = entity->world_position.y,
                .roundness = entity_roundness(*entity)
            };

            renderer_.submit(command);
        }

        renderer_.sort();
        renderer_.execute();

        // Screen HUD
        DrawFPS(GetScreenWidth() - 100, 2);
        DrawText(TextFormat("entities alive: %zu", entities_.size()), 10, 10, 20, RAYWHITE);
        DrawText(TextFormat("active bombs: %zu / 1", active_bomb_handles_.size()), 10, 34, 20, YELLOW);
        DrawText(TextFormat("Player Lives: %i", player().lives), 10, 58, 20, player().is_dead ? RED : GREEN);

        if (player().is_dead) {
            DrawText("GAME OVER - YOU DIED", window_.width() / 2 - 150, window_.height() / 2 - 20, 30, RED);
        }

        DrawText("WASD: move | Space: place bomb | Q: quit", 10, window_.height() - 30, 20, LIGHTGRAY);
    }

    void Application::request_exit() noexcept {
        exit_requested_ = true;
    }
} // namespace kotte
