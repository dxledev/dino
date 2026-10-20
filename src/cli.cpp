#include "cli.h"
#include "core/theme.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTextStream>
#include <stdexcept>

namespace dino {
namespace {
[[noreturn]] void fail(const QString &message) { throw std::runtime_error(message.toStdString()); }
}
Options parseOptions(const QStringList &arguments) {
    Options options;
    options.configDirectory = qEnvironmentVariable("DINO_CONFIG_DIR",
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/dino");
    for (int i = 0; i < arguments.size(); ++i) {
        const auto &arg = arguments[i];
        if (arg == "--config-dir") {
            if (++i >= arguments.size() || arguments[i].isEmpty()) fail("--config-dir requires a directory");
            options.configDirectory = arguments[i];
        } else if (arg == "--dry-run") options.dryRun = true;
        else if (arg == "--help" || arg == "-h") options.help = true;
        else if (arg == "--version") options.version = true;
        else if (arg.startsWith("--")) fail("Unknown option: " + arg);
        else options.command.append(arg);
    }
    options.configDirectory = QDir(options.configDirectory).absolutePath();
    if (options.dryRun && options.command.isEmpty() && !options.help) fail("--dry-run requires a theme command");
    return options;
}
void printHelp() {
    QTextStream(stdout) << "Dino 1.0.0\n"
        "Usage: dino [--config-dir DIRECTORY]\n"
        "       dino [--config-dir DIRECTORY] theme COMMAND [--dry-run]\n\n"
        "  show                       Print the saved palette\n"
        "  set KEY=COLOR [...]        Set colors and stop following a source\n"
        "  import FILE                Import a QML or JSON palette once\n"
        "  follow active              Follow the active palette\n"
        "  follow FILE                Follow a QML or JSON palette\n"
        "  follow static [NAME|DIR]   Follow a named palette (default: current)\n"
        "  sync                       Refresh the followed palette\n"
        "  reset                      Use the built-in palette\n\n"
        "Colors: background, surface, text, muted, accent, border, accent_text\n"
        "Formats: #RRGGBB, #AARRGGBB, Qt.rgba(r, g, b, a)\n"
        "Controls: Space/Up jump; Down duck; Esc/Q close; R retry.\n";
}
int runCommand(const Options &options) {
    const auto args = options.command;
    if (args.size() < 2 || args[0] != "theme") fail("Expected a theme command; see --help");
    const auto path = options.configDirectory + "/theme.json";
    const auto action = args[1];
    Theme theme = defaultTheme();
    QString error;
    if (QFileInfo::exists(path) && !loadTheme(path, &theme, &error)
        && action != "reset" && action != "import" && action != "follow") fail(error);
    if (action == "show" && args.size() == 2) {
        QTextStream(stdout) << QJsonDocument(theme.toJson()).toJson(); return 0;
    }
    if (action == "set" && args.size() >= 3) {
        auto data = theme.toJson();
        auto colors = data["colors"].toObject();
        for (const auto &assignment : args.mid(2)) {
            const auto separator = assignment.indexOf('=');
            const auto key = assignment.left(separator);
            if (separator < 1 || !theme.colors.contains(key)) fail("Unknown color assignment: " + assignment);
            colors[key] = assignment.mid(separator + 1);
        }
        data["colors"] = colors;
        data["name"] = "Custom";
        data.remove("source");
        // Validate custom expressions through the same parser as file imports.
        if (!parseTheme(data, &theme, &error)) fail(error);
        theme.source.clear();
    } else if (action == "import" && args.size() == 3) {
        if (!importTheme(QFileInfo(args[2]).absoluteFilePath(), &theme, &error)) fail(error);
        theme.source.clear();
    } else if (action == "follow" && args.size() >= 3 && args.size() <= 4) {
        if (args[2] == "active" && args.size() == 3) {
            if (!followActiveTheme(&theme, &error)) fail(error);
        } else {
            QString source;
            if (args[2] == "static") {
                source = args.size() == 4 ? args[3] : "current";
                if (!source.contains('/')) source = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/themes/" + source;
            } else if (args.size() == 3) source = args[2];
            else fail("Expected follow active, follow FILE, or follow static NAME");
            if (QFileInfo(source).isDir()) source += "/Colors.qml";
            source = QFileInfo(source).absoluteFilePath();
            if (source == path || (!QFileInfo(source).canonicalFilePath().isEmpty()
                && QFileInfo(source).canonicalFilePath() == QFileInfo(path).canonicalFilePath())) fail("Theme source cannot be its output file");
            if (!importTheme(source, &theme, &error)) fail(error);
            theme.source = source;
        }
    } else if (action == "sync" && args.size() == 2) {
        if (!refreshFollowedTheme(&theme, &error)) fail(error);
    } else if (action == "reset" && args.size() == 2) theme = defaultTheme();
    else fail("Invalid theme command; see --help");
    if (options.dryRun) QTextStream(stdout) << QJsonDocument(theme.toJson()).toJson();
    else if (!saveTheme(path, theme, &error)) fail(error);
    return 0;
}
}
