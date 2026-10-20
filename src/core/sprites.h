#pragma once
#include <span>
#include <string_view>

namespace dino {
enum class SpriteId {
    Stand, RunA, RunB, Dead, Blink, DuckA, DuckB,
    CactusSmall, CactusLarge, BirdUp, BirdDown, Cloud
};
struct Sprite {
    std::span<const std::string_view> rows;
    int width;
    int height;
};
const Sprite &sprite(SpriteId id) noexcept;
bool spritesOverlap(const Sprite &a, double ax, double ay, double aw, double ah,
                    const Sprite &b, double bx, double by, double bw, double bh) noexcept;
}
