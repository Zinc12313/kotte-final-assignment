#include "kotte/renderer.hpp"

#include <algorithm>

namespace kotte
{
    void Renderer::clear() noexcept {
        commands_.clear();
    }

    void Renderer::submit(RenderCommand command) {
        commands_.push_back(command);
    }

    void Renderer::sort() {
        std::ranges::sort(commands_, [](const RenderCommand& a, const RenderCommand& b) {
            // 1. Ground layer draws before World layer
            if (a.layer != b.layer) {
                return a.layer < b.layer;
            }
            // 2. Smaller depth (higher on screen) draws before larger depth
            return a.depth < b.depth;
            });
    }

    void Renderer::execute() const {
        constexpr int segments = 4;
        for (const auto& cmd : commands_) {
            DrawRectangleRounded(
                cmd.screen_bounds,
                cmd.roundness,
                segments,
                cmd.color
            );
        }
    }

    std::size_t Renderer::command_count() const noexcept {
        return commands_.size();
    }
}
