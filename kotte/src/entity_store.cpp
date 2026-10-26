//entity_store.cpp

#include "kotte/entity_store.hpp"

namespace kotte
{
    EntityHandle EntityStore::spawn(Entity entity) {
        std::size_t slot_index = 0;

        if (!free_slots_.empty()) {
            slot_index = free_slots_.back();
            free_slots_.pop_back();
        }
        else {
            slot_index = slots_.size();
            slots_.push_back(Slot{});
        }

        auto& slot = slots_[slot_index];
        slot.entity = std::move(entity);
        ++count_;

        return EntityHandle{
            .slot = slot_index,
            .generation = slot.generation
        };
    }

    bool EntityStore::destroy(EntityHandle handle) noexcept {
        if (handle.slot >= slots_.size()) {
            return false;
        }

        auto& slot = slots_[handle.slot];
        if (!slot.occupied() || slot.generation != handle.generation) {
            return false;
        }

        // Vacate slot and advance generation so stale handles are invalidated
        slot.entity.reset();
        ++slot.generation;
        free_slots_.push_back(handle.slot);
        --count_;
        return true;
    }

    bool EntityStore::contains(EntityHandle handle) const noexcept {
        return try_get(handle) != nullptr;
    }

    Entity* EntityStore::try_get(EntityHandle handle) noexcept {
        if (handle.slot >= slots_.size()) {
            return nullptr;
        }

        auto& slot = slots_[handle.slot];
        if (!slot.occupied() || slot.generation != handle.generation) {
            return nullptr;
        }

        return &slot.entity.value();
    }

    const Entity* EntityStore::try_get(EntityHandle handle) const noexcept {
        if (handle.slot >= slots_.size()) {
            return nullptr;
        }

        const auto& slot = slots_[handle.slot];
        if (!slot.occupied() || slot.generation != handle.generation) {
            return nullptr;
        }

        return &slot.entity.value();
    }
} // namespace kotte
