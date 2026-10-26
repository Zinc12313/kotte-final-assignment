//tile_map.cpp


#include "kotte/tile_map.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace kotte
{
    TileMap::TileMap(int width, int height)
        : width_{ width }
        , height_{ height }
    {
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument{ "Tile map dimensions must be positive." };
        }

        tiles_.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    }

    int TileMap::width() const noexcept {
        return width_;
    }

    int TileMap::height() const noexcept {
        return height_;
    }

    std::size_t TileMap::tile_count() const noexcept {
        return tiles_.size();
    }

    bool TileMap::contains(int x, int y) const noexcept {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    }

    Tile& TileMap::at(int x, int y) {
        return tiles_.at(index(x, y));
    }

    const Tile& TileMap::at(int x, int y) const {
        return tiles_.at(index(x, y));
    }

    std::size_t TileMap::index(int x, int y) const {
        if (!contains(x, y)) {
            throw std::out_of_range{ "Tile coordinates are outside the map." };
        }

        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)
            + static_cast<std::size_t>(x);
    }

    TileMap make_room(int width, int height, Random& random) {
        TileMap room{ width, height };

        for (int y = 0; y < room.height(); ++y) {
            for (int x = 0; x < room.width(); ++x) {
                Tile& tile = room.at(x, y);
                const bool boundary = x == 0 || y == 0
                    || x == room.width() - 1 || y == room.height() - 1;

                tile.type = boundary ? TileType::wall : TileType::floor;
                tile.variation = random.next<3, std::uint8_t>();
            }
        }

        return room;
    }

    TileRange TileMap::tiles_overlapping(
        Rectangle world_view, int tile_size) const noexcept
    {
        if (tile_size <= 0) {
            return TileRange{};
        }

        const float tile_size_f = static_cast<float>(tile_size);

        // Calculate tile what is capture
        const int raw_first_col = static_cast<int>(std::floor(world_view.x / tile_size_f));
        const int raw_past_col = static_cast<int>(std::ceil((world_view.x + world_view.width) / tile_size_f));

        const int raw_first_row = static_cast<int>(std::floor(world_view.y / tile_size_f));
        const int raw_past_row = static_cast<int>(std::ceil((world_view.y + world_view.height) / tile_size_f));

        // Clamp to map boundaries 
        return TileRange{
            std::clamp(raw_first_col, 0, width_),
            std::clamp(raw_first_row, 0, height_),
            std::clamp(raw_past_col,  0, width_),
            std::clamp(raw_past_row,  0, height_)
        };
    }
} 
