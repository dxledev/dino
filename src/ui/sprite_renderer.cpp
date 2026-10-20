#include "sprite_renderer.h"
#include <QImage>
#include <array>

namespace dino {
namespace {
struct Raster {
    QColor color;
    QImage image;
};
const QImage &spriteImage(SpriteId id, const QColor &color) {
    static std::array<Raster, static_cast<int>(SpriteId::Cloud) + 1> cache;
    auto &raster = cache[static_cast<int>(id)];
    if (!raster.image.isNull() && raster.color == color) return raster.image;
    const auto &shape = sprite(id);
    raster.color = color;
    raster.image = QImage(shape.width / 2, shape.height / 2, QImage::Format_ARGB32_Premultiplied);
    raster.image.fill(Qt::transparent);
    for (int y = 0; y < static_cast<int>(shape.rows.size()); ++y)
        for (int x = 0; x < static_cast<int>(shape.rows[y].size()); ++x)
            if (shape.rows[y][x] == '#') raster.image.setPixelColor(x, y, color);
    return raster.image;
}
}
void drawSprite(QPainter &painter, SpriteId id, const QRectF &bounds, const QColor &color) {
    painter.save();
    // Scale one bitmap so adjacent art rows share rasterized edges at fractional display scales.
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(bounds, spriteImage(id, color));
    painter.restore();
}
}
