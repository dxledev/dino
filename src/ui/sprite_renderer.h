#pragma once
#include "core/sprites.h"
#include <QColor>
#include <QPainter>
#include <QRectF>

namespace dino {
void drawSprite(QPainter &painter, SpriteId id, const QRectF &bounds, const QColor &color);
}
