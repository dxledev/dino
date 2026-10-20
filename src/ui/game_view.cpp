#include "game_view.h"
#include "sprite_renderer.h"
#include <QFocusEvent>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <cmath>

namespace dino {
namespace {
QRectF rectangle(const Rect &r) { return {r.x, r.y, r.width, r.height}; }
bool isJumpKey(int key) {
    return key == Qt::Key_Space || key == Qt::Key_Up || key == Qt::Key_W || key == Qt::Key_K;
}
bool isDuckKey(int key) { return key == Qt::Key_Down || key == Qt::Key_S || key == Qt::Key_J; }
QFont gameFont(int size, bool bold = false) {
    auto font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPixelSize(size); font.setBold(bold);
    return font;
}
}
void drawDinosaur(QPainter &p, const QRectF &bounds, const QColor &color, bool duck, int stride) {
    const auto id = duck ? (stride ? SpriteId::DuckB : SpriteId::DuckA) : SpriteId::Stand;
    drawSprite(p, id, bounds, color);
}
GameView::GameView(QWidget *parent) : QWidget(parent), theme_(defaultTheme()) {
    setObjectName("gameView");
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName("Dinosaur runner. Space to jump, Down to duck, Escape to close.");
    setMinimumSize(360, 90);
    idleClock_.start(); clock_.start();
    timer_.setInterval(16); timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &GameView::tick);
    timer_.start();
}
void GameView::setTheme(const Theme &theme) { theme_ = theme; update(); }
void GameView::setHighScore(std::uint64_t score) { highScore_ = score; update(); }
void GameView::clearInput() { input_ = {}; }
void GameView::startRun() {
    clearInput(); newHighScore_ = false;
    engine_.reset(QRandomGenerator::global()->generate());
    engine_.start(); clock_.restart();
    setFocus(); update(); emit frameChanged();
}
void GameView::tick() {
    const double delta = clock_.nsecsElapsed() / 1e9;
    clock_.restart();
    const auto before = engine_.state();
    engine_.update(delta, input_);
    input_.jumpPressed = false;
    if (before == GameState::Running) {
        emit frameChanged();
        if (engine_.state() == GameState::GameOver) {
            newHighScore_ = engine_.score() > highScore_;
            crashClock_.start(); clearInput(); emit finished();
        }
    }
    update();
}
void GameView::drawLandscape(QPainter &p, double worldHeight) {
    const double groundOffset = worldHeight - 150.;
    auto cloudColor = theme_.colors["muted"]; cloudColor.setAlphaF(cloudColor.alphaF() * .28);
    const double distance = engine_.distanceTravelled();
    for (int i = 0; i < 3; ++i) {
        double x = std::fmod(285. + i * 197. - distance * .2, worldWidth_ + 80.);
        if (x < 0) x += worldWidth_ + 80.;
        drawSprite(p, SpriteId::Cloud, QRectF(x - 46., worldHeight * (.16 + i * .05), 46., 12.), cloudColor);
    }
    if (engine_.night() > 0) {
        auto stars = theme_.colors["muted"]; stars.setAlphaF(stars.alphaF() * engine_.night() * .6);
        for (int i = 0; i < 7; ++i) {
            double x = std::fmod(i * 97. + 40. - distance * .01, worldWidth_);
            if (x < 0) x += worldWidth_;
            p.fillRect(QRectF(x, 12 + std::fmod(i * 37., worldHeight * .55), 2, 2), stars);
        }
    }
    p.fillRect(QRectF(0, 137 + groundOffset, worldWidth_, 1), theme_.colors["border"]);
    for (int i = 0; i < 26; ++i) {
        double x = std::fmod(i * 47. + i % 3 * 11. - distance, worldWidth_);
        if (x < 0) x += worldWidth_;
        p.fillRect(QRectF(x, 142 + groundOffset + i % 4 * 2, 2 + i % 2 * 2, 1), theme_.colors["muted"]);
        if (i % 7 == 0) p.fillRect(QRectF(x, 135 + groundOffset, 6, 2), theme_.colors["border"]);
    }
}
void GameView::drawObstacles(QPainter &p) {
    for (const auto &obstacle : engine_.obstacles())
        drawSprite(p, engine_.obstacleSprite(obstacle), rectangle(obstacle.bounds), theme_.colors["text"]);
}
void GameView::drawScore(QPainter &p) {
    p.setFont(gameFont(11, true));
    if (highScore_ > 0) {
        p.setPen(theme_.colors["muted"]);
        p.drawText(QRectF(worldWidth_ - 230, 5, 175, 18), Qt::AlignRight | Qt::AlignVCenter,
                   QString("HI %1").arg(highScore_, 5, 10, QChar('0')));
    }
    if (engine_.scoreVisible()) {
        p.setPen(theme_.colors["text"]);
        p.drawText(QRectF(worldWidth_ - 50, 5, 40, 18), Qt::AlignRight | Qt::AlignVCenter,
                   QString("%1").arg(engine_.score(), 5, 10, QChar('0')));
    }
}
void GameView::drawPrompt(QPainter &p, double worldHeight) {
    const double center = worldHeight * .45;
    const auto state = engine_.state();
    if (state == GameState::Running) return;
    p.setPen(theme_.colors["text"]); p.setFont(gameFont(13, true));
    if (state != GameState::Ready)
        p.drawText(QRectF((worldWidth_ - 400) / 2, center - 24, 400, 20), Qt::AlignCenter,
                   "G A M E   O V E R");
    p.setPen(theme_.colors["muted"]); p.setFont(gameFont(9));
    QString hint = "press space to start";
    if (state == GameState::GameOver) {
        if (crashClock_.isValid() && crashClock_.elapsed() < 500) return;
        hint = newHighScore_
            ? "New high score! · space to play again" : "space to play again";
    }
    p.drawText(QRectF((worldWidth_ - 480) / 2, state == GameState::Ready ? center - 10 : center - 2, 480, 18), Qt::AlignCenter, hint);
}
void GameView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    const double scale = std::min(width() / 600., height() / 150.);
    worldWidth_ = width() / scale;
    engine_.setWorldWidth(worldWidth_);
}
void GameView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    const double scale = std::min(width() / 600., height() / 150.);
    const double worldHeight = height() / scale;
    p.scale(scale, scale);
    drawLandscape(p, worldHeight);
    p.save(); p.translate(0, worldHeight - 150.);
    drawObstacles(p);
    auto id = engine_.playerSprite();
    if (engine_.state() == GameState::Ready && idleClock_.elapsed() % 3500 < 130) id = SpriteId::Blink;
    drawSprite(p, id, rectangle(engine_.playerBounds()), theme_.colors["accent"]);
    p.restore();
    drawScore(p); drawPrompt(p, worldHeight);
}
void GameView::jumpAction() {
    const auto state = engine_.state();
    if (state == GameState::GameOver && crashClock_.isValid() && crashClock_.elapsed() < 500) return;
    if (state == GameState::Ready || state == GameState::GameOver) emit startRequested();
    if (engine_.state() == GameState::Running) {
        input_.jumpPressed = true; input_.jumpHeld = true;
    }
}
void GameView::keyPressEvent(QKeyEvent *event) {
    const int key = event->key();
    if (event->isAutoRepeat()) {
        if (isJumpKey(key) && engine_.state() == GameState::Running) input_.jumpPressed = true;
        event->accept(); return;
    }
    if (isJumpKey(key)) jumpAction();
    else if ((key == Qt::Key_Return || key == Qt::Key_Enter) && engine_.state() != GameState::Running) jumpAction();
    else if (isDuckKey(key)) input_.duckHeld = true;
    else if (key == Qt::Key_Escape || key == Qt::Key_Q) emit closeRequested();
    else if (key == Qt::Key_R && engine_.state() == GameState::GameOver) jumpAction();
    else { QWidget::keyPressEvent(event); return; }
    event->accept();
}
void GameView::keyReleaseEvent(QKeyEvent *event) {
    if (event->isAutoRepeat()) { event->accept(); return; }
    if (isJumpKey(event->key())) input_.jumpHeld = false;
    else if (isDuckKey(event->key())) input_.duckHeld = false;
    else { QWidget::keyReleaseEvent(event); return; }
    event->accept();
}
void GameView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) { setFocus(); jumpAction(); event->accept(); }
    else QWidget::mousePressEvent(event);
}
void GameView::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) { input_.jumpHeld = false; event->accept(); }
    else QWidget::mouseReleaseEvent(event);
}
void GameView::focusOutEvent(QFocusEvent *event) {
    clearInput();
    QWidget::focusOutEvent(event);
}
}
