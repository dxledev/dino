#pragma once
#include "core/runner.h"
#include "core/theme.h"
#include <QElapsedTimer>
#include <QPainter>
#include <QTimer>
#include <QWidget>

namespace dino {
void drawDinosaur(QPainter &painter, const QRectF &bounds, const QColor &color, bool duck, int stride);
class GameView : public QWidget {
    Q_OBJECT
public:
    explicit GameView(QWidget *parent = nullptr);
    const RunnerEngine &engine() const { return engine_; }
    void setTheme(const Theme &theme);
    void setHighScore(std::uint64_t score);
    void startRun();
signals:
    void frameChanged();
    void finished();
    void startRequested();
    void closeRequested();
protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
private:
    void tick();
    void clearInput();
    void jumpAction();
    void drawLandscape(QPainter &painter, double worldHeight);
    void drawObstacles(QPainter &painter);
    void drawScore(QPainter &painter);
    void drawPrompt(QPainter &painter, double worldHeight);
    RunnerEngine engine_;
    Theme theme_;
    QTimer timer_;
    QElapsedTimer clock_;
    QElapsedTimer idleClock_;
    QElapsedTimer crashClock_;
    Input input_;
    std::uint64_t highScore_{};
    bool newHighScore_{};
    double worldWidth_{600.0};
};
}
