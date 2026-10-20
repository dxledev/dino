#include "cli.h"
#include "core/theme.h"
#include "ui/window.h"
#include <QApplication>
#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>
#include <stdexcept>

int main(int argc, char **argv) {
    QCoreApplication::setApplicationName("dino");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCoreApplication::setOrganizationName("Dino");
    try {
        QStringList arguments;
        for (int i = 1; i < argc; ++i) arguments.append(QString::fromLocal8Bit(argv[i]));
        const auto options = dino::parseOptions(arguments);
        if (options.help || options.version || !options.command.isEmpty()) {
            QCoreApplication app(argc, argv);
            if (options.help) { dino::printHelp(); return 0; }
            if (options.version) { QTextStream(stdout) << "Dino 1.0.0\n"; return 0; }
            return dino::runCommand(options);
        }
        if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "wayland");
        // The raster renderer needs no GPU context or dedicated render process.
        if (qEnvironmentVariableIsEmpty("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION"))
            qputenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION", "shm");
        QApplication app(argc, argv);
        app.setStyle("Fusion"); app.setDesktopFileName("io.github.dino_runner");
        const auto path = options.configDirectory + "/theme.json";
        dino::Theme theme = dino::defaultTheme();
        QString error;
        if (QFileInfo::exists(path)) {
            if (!dino::loadTheme(path, &theme, &error)) QTextStream(stderr) << "dino: " << error << '\n';
            else if (!theme.source.isEmpty()) {
                auto refreshed = theme;
                if (dino::refreshFollowedTheme(&refreshed, &error)) {
                    if (refreshed.toJson() != theme.toJson() && !dino::saveTheme(path, refreshed, &error))
                        QTextStream(stderr) << "dino: " << error << '\n';
                    theme = std::move(refreshed);
                }
            }
        } else {
            dino::followActiveTheme(&theme, &error);
            if (!dino::saveTheme(path, theme, &error)) throw std::runtime_error(error.toStdString());
        }
        dino::Window window(options.configDirectory, theme); window.show();
        return app.exec();
    } catch (const std::exception &error) {
        QTextStream(stderr) << "dino: " << error.what() << '\n'; return 1;
    }
}
