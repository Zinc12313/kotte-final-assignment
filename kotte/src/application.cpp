#include "kotte/application.hpp"
#include "kotte/enemy_chaser.hpp"

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

                if (std::abs(static_cast<float>(x) * tile_size_ - player_pos.x) < 100.0f &&
                    std::abs(static_cast<float>(y) * tile_size_ - player_pos.y) < 100.0f) {
                    continue;
                }

                // Reduced spawn density (0.08f) for open tactical space and killable enemy counts
                if (dist_01(random_) < 0.08f) {
                    const float kind_roll = dist_01(random_);
                    EntityKind kind = EntityKind::crate;
                    if (kind_roll < 0.04f) {
                        kind = EntityKind::enemy;
                    }
                    else if (kind_roll < 0.08f) {
                        kind = EntityKind::enemy_chaser; // Spawns chaser enemy
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

                    if (kind == EntityKind::enemy || kind == EntityKind::enemy_chaser) {
                        enemy_handles_.push_back(handle);
                    }
                }
            }
        }
    }

    void Application::reset_round() {
        map_ = make_room(200, 120, random_);

        spatial_grid_ = SpatialGrid{
            Vector2{ static_cast<float>(map_.width()) * tile_size_, static_cast<float>(map_.height()) * tile_size_ },
            spatial_cell_size_
        };

        enemy_handles_.clear();
        active_bomb_handles_.clear();
        destroy_entity_requests_.clear();
        active_blasts_.clear();
        player_blast_range_tiles_ = 3.0f;

        populate_initial_world();
        round_state_ = RoundState::playing;
        update_camera();
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

        if (IsKeyPressed(KEY_R)) {
            reset_round();
        }

        update_round_state();

        if (round_state_ == RoundState::playing) {
            update_player_input(dt);
            update_enemies(dt);
            update_bombs(dt);
            check_powerup_collisions();

            apply_entity_destructions();

            for (auto& blast : active_blasts_) {
                blast.remaining_seconds -= dt;
            }
            std::erase_if(active_blasts_, [](const BlastEffect& b) {
                return b.remaining_seconds <= 0.0f;
                });
        }

        update_camera();
    }

    void Application::update_round_state() {
        if (round_state_ != RoundState::playing) return;

        if (player().is_dead) {
            round_state_ = RoundState::lost;
        }
        else if (enemy_handles_.empty()) {
            round_state_ = RoundState::won;
        }
    }

    void Application::update_player_input(float dt) {
        if (player().is_dead) return;

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

        std::size_t player_bomb_count = 0;
        for (const EntityHandle h : active_bomb_handles_) {
            const Entity* bomb = entities_.try_get(h);
            if (bomb && bomb->owner_slot == player_handle_.slot) {
                ++player_bomb_count;
            }
        }
        if (player_bomb_count >= 3) {
            return;
        }

        const Vector2 p_pos = player().world_position;
        const float snap_x = (std::floor(p_pos.x / tile_size_) + 0.5f) * tile_size_;
        const float snap_y = (std::floor(p_pos.y / tile_size_) + 1.0f) * tile_size_;

        const EntityHandle bomb_handle = entities_.spawn(Entity{
            .kind = EntityKind::bomb,
            .world_position = Vector2{ snap_x, snap_y },
            .fuse_seconds = 2.0f,
            .owner_slot = player_handle_.slot,
            .blast_range_tiles = player_blast_range_tiles_
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
        destroy_entity_requests_.push_back({ bomb_handle });

        const Vector2 center = bomb.world_position;
        const float blast_range = bomb.blast_range_tiles * tile_size_;

        const Rectangle horizontal_blast{ center.x - blast_range, center.y - (tile_size_ * 0.5f), blast_range * 2.0f, tile_size_ };
        const Rectangle vertical_blast{ center.x - (tile_size_ * 0.5f), center.y - blast_range, tile_size_, blast_range * 2.0f };

        active_blasts_.push_back(BlastEffect{ .bounds = horizontal_blast });
        active_blasts_.push_back(BlastEffect{ .bounds = vertical_blast });

        const SpatialQuery q1 = spatial_grid_.query(horizontal_blast);
        const SpatialQuery q2 = spatial_grid_.query(vertical_blast);

        std::vector<EntityHandle> targets = q1.entity_handles;
        targets.insert(targets.end(), q2.entity_handles.begin(), q2.entity_handles.end());

        std::uniform_real_distribution<float> dist_01(0.0f, 1.0f);

        for (const EntityHandle target_handle : targets) {
            Entity* target = entities_.try_get(target_handle);
            if (!target) continue;

            const Rectangle bounds = entity_collision_bounds(*target);
            if (CheckCollisionRecs(horizontal_blast, bounds) || CheckCollisionRecs(vertical_blast, bounds)) {
                if (target->kind == EntityKind::enemy || target->kind == EntityKind::enemy_chaser) {
                    destroy_entity_requests_.push_back({ target_handle });
                }
                else if (target->kind == EntityKind::crate) {
                    destroy_entity_requests_.push_back({ target_handle });

                    if (dist_01(random_) < 0.75f) {
                        const EntityHandle powerup_handle = entities_.spawn(Entity{
                            .kind = EntityKind::powerup,
                            .world_position = target->world_position
                            });
                        spatial_grid_.insert(powerup_handle, entity_world_bounds(*entities_.try_get(powerup_handle)));
                    }
                }
                else if (target->kind == EntityKind::bomb && target_handle != bomb_handle) {
                    target->fuse_seconds = 0.0f;
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

    void Application::apply_entity_destructions() {
        for (const auto& request : destroy_entity_requests_) {
            Entity* entity = entities_.try_get(request.entity);
            if (entity == nullptr) {
                continue;
            }

            const Rectangle world_bounds = entity_world_bounds(*entity);
            spatial_grid_.remove(request.entity, world_bounds);

            (void)entities_.destroy(request.entity);
        }

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

            if (candidate->kind == EntityKind::bomb && moving_handle.slot == candidate->owner_slot) {
                const Entity* moving_entity = entities_.try_get(moving_handle);
                if (moving_entity) {
                    const float prev_dist_sq =
                        (moving_entity->world_position.x - candidate->world_position.x) * (moving_entity->world_position.x - candidate->world_position.x) +
                        (moving_entity->world_position.y - candidate->world_position.y) * (moving_entity->world_position.y - candidate->world_position.y);
                    const float prop_dist_sq =
                        (proposed_position.x - candidate->world_position.x) * (proposed_position.x - candidate->world_position.x) +
                        (proposed_position.y - candidate->world_position.y) * (proposed_position.y - candidate->world_position.y);

                    if (prop_dist_sq >= prev_dist_sq) {
                        continue;
                    }
                }
            }

            if (CheckCollisionRecs(proposed_collision_bounds, entity_collision_bounds(*candidate))) {
                return true;
            }
        }

        return false;
    }

    void Application::check_powerup_collisions() {
        const Entity& p = player();
        if (p.is_dead) return;

        const Rectangle player_bounds = entity_collision_bounds(p);
        const SpatialQuery query = spatial_grid_.query(player_bounds);

        for (const EntityHandle handle : query.entity_handles) {
            Entity* entity = entities_.try_get(handle);
            if (!entity || entity->kind != EntityKind::powerup) continue;

            if (CheckCollisionRecs(player_bounds, entity_collision_bounds(*entity))) {
                player_blast_range_tiles_ += 1.0f;
                destroy_entity_requests_.push_back({ handle });
            }
        }
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
        const Vector2 player_pos = player().world_position;
        const Rectangle player_bounds = entity_collision_bounds(player());

        for (const EntityHandle enemy_handle : enemy_handles_) {
            Entity* enemy = entities_.try_get(enemy_handle);
            if (!enemy) continue;

            // touching an enemy costs 1 
            if (!player().is_dead && CheckCollisionRecs(player_bounds, entity_collision_bounds(*enemy))) {
                Entity& p = player();
                p.lives--;
                if (p.lives <= 0) {
                    p.is_dead = true;
                }
                else {
                    p.world_position = Vector2{
                        (static_cast<float>(map_.width()) * 0.5f) * tile_size_,
                        (static_cast<float>(map_.height()) * 0.5f) * tile_size_
                    };
                }
            }

            if (enemy->kind == EntityKind::enemy_chaser) {
                update_chaser_enemy(dt, enemy_handle, player_pos, spatial_grid_, entities_,
                    [this](EntityHandle h, Vector2 pos) { return entity_movement_is_blocked(h, pos); });
            }
            else {
                // Original Standard Patroller AI
                constexpr float enemy_speed = 70.0f;
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
                .layer = (entity->kind == EntityKind::bomb || entity->kind == EntityKind::powerup) ? RenderLayer::ground : RenderLayer::world,
                .depth = entity->world_position.y,
                .roundness = entity_roundness(*entity)
            };

            renderer_.submit(command);
        }

        renderer_.sort();
        renderer_.execute();

        DrawFPS(GetScreenWidth() - 100, 2);

        std::size_t current_player_bombs = 0;
        for (const EntityHandle h : active_bomb_handles_) {
            const Entity* bomb = entities_.try_get(h);
            if (bomb && bomb->owner_slot == player_handle_.slot) {
                ++current_player_bombs;
            }
        }

        DrawText(TextFormat("Active Bombs: %zu / 3", current_player_bombs), 10, 10, 20, YELLOW);
        DrawText(TextFormat("Enemies Remaining: %zu", enemy_handles_.size()), 10, 34, 20, ORANGE);
        DrawText(TextFormat("Player Lives: %i | Blast Range: %.0f", player().lives, player_blast_range_tiles_), 10, 58, 20, player().is_dead ? RED : GREEN);

        if (round_state_ == RoundState::won) {
            DrawText("VICTORY! ALL ENEMIES DEFEATED", window_.width() / 2 - 200, window_.height() / 2 - 40, 30, GREEN);
            DrawText("Press [R] to Restart", window_.width() / 2 - 100, window_.height() / 2 + 10, 20, LIGHTGRAY);
        }
        else if (round_state_ == RoundState::lost) {
            DrawText("GAME OVER - DEFEATED", window_.width() / 2 - 150, window_.height() / 2 - 40, 30, RED);
            DrawText("Press [R] to Restart", window_.width() / 2 - 100, window_.height() / 2 + 10, 20, LIGHTGRAY);
        }

        DrawText("WASD: move | Space: place bomb | R: restart | Q: quit", 10, window_.height() - 30, 20, LIGHTGRAY);
    }

    void Application::request_exit() noexcept {
        exit_requested_ = true;
    }
} // namespace kotte
