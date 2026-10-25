#include "kotte/camera.hpp"

namespace kotte
{
    Camera::Camera(Vector2 viewport_size) noexcept
        : centre_{ 0.0f, 0.0f }
        , viewport_size_{ viewport_size }
    {
    }

    void Camera::set_centre(Vector2 world_position) noexcept {
        centre_ = world_position;
    }

    void Camera::set_viewport_size(Vector2 viewport_size) noexcept {
        viewport_size_ = viewport_size;
    }

    Vector2 Camera::world_to_screen(Vector2 world_position) const noexcept {
        const float top_left_x = centre_.x - (viewport_size_.x * 0.5f);
        const float top_left_y = centre_.y - (viewport_size_.y * 0.5f);

        return Vector2{
            world_position.x - top_left_x,
            world_position.y - top_left_y
        };
    }

    Rectangle Camera::world_view() const noexcept {
        return Rectangle{
            centre_.x - (viewport_size_.x * 0.5f),
            centre_.y - (viewport_size_.y * 0.5f),
            viewport_size_.x,
            viewport_size_.y
        };
    }
}
