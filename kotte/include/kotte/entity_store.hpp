//entity_store.hpp

#pragma once

#include "kotte/entity.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace kotte
{
    struct EntityHandle final {
        std::size_t slot{ 0 };
        std::uint32_t generation{ 0 };

        constexpr bool operator==(const EntityHandle&) const noexcept = default;

        constexpr bool operator<(const EntityHandle& other) const noexcept {
            if (slot != other.slot) {
                return slot < other.slot;
            }
            return generation < other.generation;
        }
    };

    class EntityStore final {
    public:
        struct Slot final {
            std::optional<Entity> entity{};
            std::uint32_t generation{ 0 };

            [[nodiscard]] bool occupied() const noexcept { return entity.has_value(); }
        };

        [[nodiscard]] EntityHandle spawn(Entity entity);
        bool destroy(EntityHandle handle) noexcept;

        [[nodiscard]] bool contains(EntityHandle handle) const noexcept;
        [[nodiscard]] Entity* try_get(EntityHandle handle) noexcept;
        [[nodiscard]] const Entity* try_get(EntityHandle handle) const noexcept;

        [[nodiscard]] std::size_t size() const noexcept { return count_; }
        [[nodiscard]] const std::vector<Slot>& slots() const noexcept { return slots_; }

    private:
        std::vector<Slot> slots_{};
        std::vector<std::size_t> free_slots_{};
        std::size_t count_{ 0 };
    };
} // namespace kotte
