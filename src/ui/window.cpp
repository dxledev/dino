#include "window.h"
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QLabel>
#include <QPainter>
#include <QSaveFile>
#include <QVBoxLayout>
#include <algorithm>

namespace dino {
namespace {
QString cssColor(QColor color) {
    return QString("rgba(%1,%2,%3,%4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
}
}
Window::Window(QString configDirectory, Theme theme, QWidget *parent)
    : QMainWindow(parent), configDirectory_(std::move(configDirectory)) {
    setWindowTitle("Dino");
    setAttribute(Qt::WA_TranslucentBackground);
    resize(1000, 620); setMinimumSize(480, 270);
    createContent(); connectActions(); followTheme(std::move(theme));
    applyTheme(); loadRecord(); game_->setHighScore(best_); game_->setFocus();
}
void Window::createContent() {
    content_ = new QWidget(this); content_->setObjectName("content"); setCentralWidget(content_);
    auto *layout = new QVBoxLayout(content_);
    layout->setContentsMargins(16, 12, 16, 12); layout->setSpacing(6);
    game_ = new GameView(content_); layout->addWidget(game_, 1);
    auto *keys = new QLabel("space / ↑ jump   ·   ↓ duck   ·   esc close", content_);
    keys->setObjectName("keyHints"); keys->setAlignment(Qt::AlignCenter);
    keys->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(keys);
    status_ = new QLabel(content_); status_->setObjectName("status");
    status_->setWordWrap(true); status_->hide(); layout->addWidget(status_);
}
void Window::connectActions() {
    connect(game_, &GameView::finished, this, &Window::recordRun);
    connect(game_, &GameView::startRequested, this, [this] {
        recorded_ = false; game_->startRun();
    });
    connect(game_, &GameView::closeRequested, this, &Window::close);
}
void Window::followTheme(Theme theme) {
    themeWatcher_ = new ThemeWatcher(configDirectory_ + "/theme.json", std::move(theme), this);
    connect(themeWatcher_, &ThemeWatcher::changed, this, &Window::applyTheme);
    connect(themeWatcher_, &ThemeWatcher::recovered, this, &Window::clearThemeError);
    connect(themeWatcher_, &ThemeWatcher::rejected, this, [this](const QString &error) {
        status_->setText("Theme unchanged: " + error); status_->show();
    });
}
void Window::applyTheme() {
    const auto &theme = themeWatcher_->theme();
    const auto color = [&theme](const char *key) { return cssColor(theme.colors[key]); };
    setStyleSheet(QString(
        "QMainWindow { background: transparent; }"
        "QWidget#content { background: %1; border: 1px solid %4; color: %2; }"
        "QWidget { font-size: 11px; color: %2; }"
        "QLabel, QWidget#gameView { background: transparent; border: none; }"
        "QLabel#keyHints { color: %3; }"
        "QLabel#status { color: %3; font-size: 11px; }")
        .arg(color("background"), color("text"), color("muted"), color("accent")));
    QPixmap icon(64, 64); icon.fill(Qt::transparent);
    { QPainter p(&icon); drawDinosaur(p, QRectF(7, 7, 50, 48), theme.colors["accent"], false, 0); }
    setWindowIcon(QIcon(icon)); game_->setTheme(theme); clearThemeError();
}
void Window::clearThemeError() {
    if (status_->text().startsWith("Theme unchanged:")) status_->hide();
}
void Window::loadRecord() {
    QFile file(configDirectory_ + "/record.json");
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4096) { status_->setText("Could not read your saved record."); status_->show(); return; }
    const auto data = QJsonDocument::fromJson(file.readAll()).object();
    const auto best = data["best"].toDouble(-1), runs = data["runs"].toDouble(-1);
    if (best >= 0 && best <= 1e12 && runs >= 0 && runs <= 1e12) { best_ = static_cast<std::uint64_t>(best); runs_ = static_cast<std::uint64_t>(runs); }
}
void Window::recordRun() {
    if (recorded_ || game_->engine().state() == GameState::Ready) return;
    best_ = std::max(best_, game_->engine().score()); ++runs_; recorded_ = true;
    QDir().mkpath(configDirectory_);
    QSaveFile file(configDirectory_ + "/record.json");
    const auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"best", static_cast<double>(best_)}, {"runs", static_cast<double>(runs_)}}).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        status_->setText("Could not save your record: " + file.errorString()); status_->show();
    }
    game_->setHighScore(best_);
}
void Window::closeEvent(QCloseEvent *event) { recordRun(); QMainWindow::closeEvent(event); }
}
