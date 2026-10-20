#include "ui/window.h"
#include "ui/theme_watcher.h"
#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

namespace {
void palette(const QString &path, const QString &accent, const QString &background = "#18212b") {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QString("property color background: \"%1\"\nproperty color foreground: \"#d6e2e8\"\nproperty color primary: \"%2\"\n").arg(background, accent).toUtf8());
}
void screenshot(QWidget &widget, const QString &name) {
    const auto directory = qEnvironmentVariable("DINO_TEST_SCREENSHOTS");
    if (directory.isEmpty()) return;
    QDir().mkpath(directory);
    QVERIFY(widget.grab().save(directory + "/" + name + ".png"));
}
}
class UiTests : public QObject {
    Q_OBJECT
private slots:
    void keyboardControlsKeepRunGoing() {
        QTemporaryDir directory;
        const auto theme = dino::defaultTheme();
        QVERIFY(dino::saveTheme(directory.path() + "/theme.json", theme));
        dino::Window window(directory.path(), theme); window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(window.size(), QSize(1000, 620));
        screenshot(window, "ready");
        auto *game = window.gameView();
        QTest::keyClick(game, Qt::Key_Space);
        QCOMPARE(game->engine().state(), dino::GameState::Running);
        QTest::qWait(150);
        QVERIFY(!game->engine().playerGrounded());
        const auto elapsed = game->engine().elapsedSeconds();
        QTest::keyClick(game, Qt::Key_P);
        QTest::qWait(100);
        QCOMPARE(game->engine().state(), dino::GameState::Running);
        QVERIFY(game->engine().elapsedSeconds() > elapsed);
        QVERIFY(!window.findChild<QLabel *>("keyHints")->text().contains("pause"));
        QCOMPARE(QApplication::topLevelWidgets().size(), 1);
        window.resize(480, 270); QTest::qWait(20);
        QTest::qWait(600);
        QTest::keyPress(game, Qt::Key_J);
        QTest::qWait(25);
        QCOMPARE(game->engine().playerBounds().height, 22.0);
        QCOMPARE(game->engine().playerBounds().width, 60.0);
        QTest::keyRelease(game, Qt::Key_J);
        QTest::qWait(25);
        QCOMPARE(game->engine().playerBounds().height, 40.0);
        QTest::keyClick(game, Qt::Key_K);
        QTest::qWait(50);
        QVERIFY(!game->engine().playerGrounded());
        window.close();
    }
    void gameOverRetryAndPersistence() {
        QTemporaryDir directory;
        const auto theme = dino::defaultTheme();
        QVERIFY(dino::saveTheme(directory.path() + "/theme.json", theme));
        dino::Window window(directory.path(), theme); window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *game = window.gameView();
        QTest::mouseClick(game, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(game->engine().state(), dino::GameState::GameOver, 8000);
        QVERIFY(QFileInfo::exists(directory.path() + "/record.json"));
        QTest::keyClick(game, Qt::Key_Space);
        QCOMPARE(game->engine().state(), dino::GameState::GameOver);
        QTest::qWait(550);
        screenshot(window, "game-over");
        QTest::keyClick(game, Qt::Key_Space);
        QCOMPARE(game->engine().state(), dino::GameState::Running);
        window.close();
    }
    void mouseReleaseCutsJumpAndEscapeCloses() {
        QTemporaryDir directory;
        dino::Window window(directory.path(), dino::defaultTheme()); window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *game = window.gameView();
        QTest::mousePress(game, Qt::LeftButton);
        QTest::qWait(90);
        QTest::mouseRelease(game, Qt::LeftButton);
        QTest::qWait(20);
        QVERIFY(game->engine().playerVelocityY() >= -300.0);
        QTest::keyClick(game, Qt::Key_Escape);
        QVERIFY(!window.isVisible());
    }
    void sceneKeepsThemeRolesAndFillsWideWindow() {
        QTemporaryDir directory;
        auto theme = dino::defaultTheme();
        theme.colors["background"] = QColor("#101820");
        theme.colors["text"] = QColor("#f4e8ca");
        theme.colors["accent"] = QColor("#d58ac2");
        theme.colors["border"] = QColor("#648aa1");
        theme.colors["muted"] = QColor("#a0aaa5");
        QVERIFY(dino::saveTheme(directory.path() + "/theme.json", theme));
        dino::Window window(directory.path(), theme); window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto frame = window.grab().toImage();
        QCOMPARE(frame.pixelColor(20, 20), theme.colors["background"]);
        int accentPixels = 0, textPixels = 0, borderPixels = 0;
        for (int y = 0; y < frame.height(); ++y) {
            for (int x = 0; x < frame.width(); ++x) {
                const auto color = frame.pixelColor(x, y);
                accentPixels += color == theme.colors["accent"];
                textPixels += color == theme.colors["text"];
                borderPixels += color == theme.colors["border"];
            }
        }
        QVERIFY(accentPixels > 500);
        QVERIFY(textPixels > 20);
        QVERIFY(borderPixels > 500);
        window.resize(1400, 270); QTest::qWait(20);
        QVERIFY(window.gameView()->engine().config().worldWidth > 600.0);
        screenshot(window, "ready-wide");
        window.close();
    }
    void reloadKeepsGameAndLastValidColors() {
        QTemporaryDir directory;
        auto theme = dino::defaultTheme(); const auto path = directory.path() + "/theme.json";
        QVERIFY(dino::saveTheme(path, theme));
        dino::Window window(directory.path(), theme); window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.gameView()->startRun();
        QTest::qWait(50);
        auto next = theme; next.colors["accent"] = QColor("#d4a2c1");
        QVERIFY(dino::saveTheme(path, next));
        QTRY_VERIFY(window.styleSheet().contains("rgba(212,162,193,255)"));
        QCOMPARE(window.gameView()->engine().state(), dino::GameState::Running);
        const auto style = window.styleSheet();
        QFile invalid(path); QVERIFY(invalid.open(QIODevice::WriteOnly)); invalid.write("{"); invalid.close();
        QTest::qWait(1100); QCOMPARE(window.styleSheet(), style);
        QVERIFY(window.findChild<QLabel *>("status")->isVisible());
        QVERIFY(dino::saveTheme(path, next));
        QTRY_VERIFY(!window.findChild<QLabel *>("status")->isVisible());
        window.close();
    }
    void followsRetargetedPaletteAndPreservesAlpha() {
        QTemporaryDir directory;
        const auto a = directory.path() + "/a.qml", b = directory.path() + "/b.qml";
        const auto selector = directory.path() + "/active.qml", output = directory.path() + "/theme.json";
        palette(a, "#a8c7c0"); palette(b, "#d4a2c1", "#e00e1e36");
        QVERIFY(QFile::link(a, selector));
        dino::Theme theme; QVERIFY(dino::importTheme(selector, &theme)); theme.source = selector;
        QVERIFY(dino::saveTheme(output, theme));
        dino::ThemeWatcher watcher(output, theme); QSignalSpy changed(&watcher, &dino::ThemeWatcher::changed);
        QVERIFY(QFile::remove(selector)); QVERIFY(QFile::link(b, selector));
        QTRY_VERIFY_WITH_TIMEOUT(changed.size() >= 1, 2500);
        QCOMPARE(watcher.theme().colors["background"], QColor("#e00e1e36"));
        dino::Theme saved; QVERIFY(dino::loadTheme(output, &saved));
        QCOMPARE(saved.colors["accent"], QColor("#d4a2c1"));
        palette(b, "#aaccdd", "#e00e1e36");
        QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().colors["accent"], QColor("#aaccdd"), 2500);
        // Invalid replacement retains both the last good palette and its binding.
        QFile bad(b); QVERIFY(bad.open(QIODevice::WriteOnly)); bad.write("broken"); bad.close();
        QTest::qWait(1100); QCOMPARE(watcher.theme().colors["accent"], QColor("#aaccdd"));
        palette(b, "#ccddaa");
        QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().colors["accent"], QColor("#ccddaa"), 2500);
    }
    void focusLossKeepsRunningAndClearsHeldKeys() {
        QTemporaryDir directory;
        dino::Window window(directory.path(), dino::defaultTheme()); window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *game = window.gameView();
        QTest::keyPress(game, Qt::Key_Space);
        QTest::qWait(80);
        QFocusEvent lost(QEvent::FocusOut);
        QApplication::sendEvent(game, &lost);
        const auto elapsed = game->engine().elapsedSeconds();
        QTest::qWait(100);
        QVERIFY(game->engine().elapsedSeconds() > elapsed);
        QCOMPARE(game->engine().state(), dino::GameState::Running);
        QTest::qWait(650);
        QVERIFY(game->engine().playerGrounded());
        window.close();
    }
    void activePaletteTransitions() {
        QTemporaryDir directory;
        const auto themes = directory.path() + "/themes";
        for (const auto &name : {QString("static-a"), QString("static-b"), QString(".dynamic/caelestia"), QString(".dynamic/noctalia")})
            QVERIFY(QDir().mkpath(themes + "/" + name));
        palette(themes + "/static-a/Colors.qml", "#aabbcc");
        palette(themes + "/static-b/Colors.qml", "#ccbbaa");
        palette(themes + "/.dynamic/caelestia/Colors.qml", "#aaccbb");
        palette(themes + "/.dynamic/noctalia/Colors.qml", "#bbaacc");
        QVERIFY(QFile::link(themes + "/static-a", themes + "/current"));
        const auto selector = themes + "/.caelestia-use";
        QVERIFY(QFile::link(themes + "/current", selector));
        const auto output = directory.path() + "/theme.json";
        dino::Theme theme; QVERIFY(dino::importTheme(selector + "/Colors.qml", &theme));
        QVERIFY(dino::saveTheme(output, theme));
        dino::ThemeWatcher watcher(output, theme);
        const QList<QPair<QString, QString>> transitions{
            {".dynamic/caelestia", "#aaccbb"}, {".dynamic/noctalia", "#bbaacc"},
            {".dynamic/caelestia", "#aaccbb"}, {"current", "#aabbcc"},
            {".dynamic/noctalia", "#bbaacc"}, {"current", "#aabbcc"},
            {".dynamic/caelestia", "#aaccbb"}, {"current", "#aabbcc"}
        };
        for (const auto &[target, accent] : transitions) {
            QVERIFY(QFile::remove(selector)); QVERIFY(QFile::link(themes + "/" + target, selector));
            QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().colors["accent"], QColor(accent), 2000);
        }
        QVERIFY(QFile::remove(themes + "/current"));
        QVERIFY(QFile::link(themes + "/static-b", themes + "/current"));
        QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().colors["accent"], QColor("#ccbbaa"), 2000);
        QVERIFY(QFile::remove(selector)); QVERIFY(QFile::link(themes + "/.dynamic/noctalia", selector));
        QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().colors["accent"], QColor("#bbaacc"), 2000);
        palette(themes + "/.dynamic/noctalia/Colors.qml", "#bbccaa");
        QTRY_COMPARE_WITH_TIMEOUT(watcher.theme().colors["accent"], QColor("#bbccaa"), 2000);
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
