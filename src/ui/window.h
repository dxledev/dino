#pragma once
#include "game_view.h"
#include "theme_watcher.h"
#include <QMainWindow>

class QLabel;
namespace dino {
class Window : public QMainWindow {
    Q_OBJECT
public:
    Window(QString configDirectory, Theme theme, QWidget *parent = nullptr);
    GameView *gameView() const { return game_; }
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void createContent();
    void connectActions();
    void followTheme(Theme theme);
    void clearThemeError();
    void applyTheme();
    void recordRun();
    void loadRecord();
    QString configDirectory_;
    ThemeWatcher *themeWatcher_ = nullptr;
    QWidget *content_ = nullptr;
    GameView *game_ = nullptr;
    QLabel *status_ = nullptr;
    std::uint64_t best_ = 0;
    std::uint64_t runs_ = 0;
    bool recorded_ = false;
};
}
