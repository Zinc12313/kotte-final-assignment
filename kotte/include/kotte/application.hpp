//application.hpp

#pragma once

#include "kotte/camera.hpp"
#include "kotte/entity.hpp"
#include "kotte/entity_store.hpp"
#include "kotte/random.hpp"
#include "kotte/renderer.hpp"
#include "kotte/spatial_grid.hpp"
#include "kotte/tile_map.hpp"
#include "kotte/window.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace kotte
{
    struct DestroyEntityRequest final {
        EntityHandle entity{};
    };

    struct BlastEffect final {
        Rectangle bounds{};
        float remaining_seconds{ 0.4f };
    };

    class Application final {
    public:
        Application(
            int width,
            int height,
            std::string_view title,
            std::uint64_t seed,
            int target_fps = 60);

        void run();

    private:
        void update(float dt);
        void update_player_input(float dt);
        void update_enemies(float dt);
        void update_bombs(float dt);
        void apply_entity_destructions();
        void try_place_bomb();
        void detonate_bomb(EntityHandle bomb_handle, const Entity& bomb);
        void render();

        void try_move_player(float delta_x, float delta_y);
        [[nodiscard]] bool entity_movement_is_blocked(EntityHandle moving_handle, Vector2 proposed_position);
        void update_camera();
        void populate_initial_world();
        void request_exit() noexcept;

        [[nodiscard]] const Entity& player() const noexcept;
        [[nodiscard]] Entity& player() noexcept;

        Window window_;
        Random random_;
        TileMap map_;
        std::uint64_t seed_{};
        bool exit_requested_{ false };
        Color background_color_{ 0x18, 0x18, 0x18, 0xff };

        EntityHandle player_handle_{};
        EntityStore entities_{};
        std::vector<EntityHandle> enemy_handles_{};
        std::vector<EntityHandle> active_bomb_handles_{};
        std::vector<DestroyEntityRequest> destroy_entity_requests_{};
        std::vector<BlastEffect> active_blasts_{};

        static constexpr float tile_size_{ 40.0f };
        static constexpr float spatial_cell_size_{ 320.0f };

        Camera camera_;
        Renderer renderer_;
        SpatialGrid spatial_grid_;
    };
} // namespace kotte
