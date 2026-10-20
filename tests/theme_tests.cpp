#include "core/theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

void writeFile(const QString &path, const QByteArray &contents) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "could not create fixture file");
    require(file.write(contents) == contents.size(), "could not write fixture file");
}

bool sameTheme(const dino::Theme &left, const dino::Theme &right) {
    return left.colors == right.colors && left.name == right.name && left.source == right.source;
}

void jsonRoundTripKeepsAlpha() {
    dino::Theme theme{
        {{"background", QColor("#80112233")}, {"surface", QColor("#90223344")},
         {"text", QColor("#a0334455")}, {"muted", QColor("#b0445566")},
         {"accent", QColor("#c0556677")}, {"border", QColor("#d0667788")},
         {"accent_text", QColor("#e0778899")}},
        "Round trip", {}};
    dino::Theme parsed;
    QString error;
    require(dino::parseTheme(theme.toJson(), &parsed, &error), "canonical JSON parse failed");
    require(sameTheme(theme, parsed), "canonical JSON changed theme colors");
    require(theme.toJson().value("colors").toObject().value("background").toString() == "#80112233",
            "serialized alpha channel was not kept first");

    QJsonObject expressionData = theme.toJson();
    QJsonObject expressionColors = expressionData.value("colors").toObject();
    expressionColors.insert("accent", "Qt.rgba(12 / 255, 34 / 255, 56 / 255, 0.5)");
    expressionData.insert("colors", expressionColors);
    require(dino::parseTheme(expressionData, &parsed, &error), "Qt.rgba custom color was rejected");
    require(parsed.colors.value("accent").red() == 12, "custom Qt.rgba red channel shifted");
    require(parsed.colors.value("accent").green() == 34, "custom Qt.rgba green channel shifted");
    require(parsed.colors.value("accent").blue() == 56, "custom Qt.rgba blue channel shifted");
    require(parsed.colors.value("accent").alpha() == 128, "custom Qt.rgba alpha was lost");

    QTemporaryDir directory;
    require(directory.isValid(), "could not create temporary directory");
    const QString path = directory.filePath("theme.json");
    require(dino::saveTheme(path, theme, &error), "theme save failed");
    dino::Theme loaded;
    require(dino::loadTheme(path, &loaded, &error), "theme load failed");
    require(sameTheme(theme, loaded), "saved theme did not round trip");
}

void qmlImportKeepsQtColorOrder() {
    QTemporaryDir directory;
    require(directory.isValid(), "could not create temporary directory");
    const QString path = directory.filePath("Colors.qml");
    writeFile(path, R"qml(
        QtObject {
            property color background: "#80112233"
            property color foreground: "#ffeeddcc"
            property color primary: Qt.rgba(12 / 255, 34 / 255, 56 / 255, 0.5)
            property color surfaceContainerLow: "#ff202020"
            property color onPrimary: "#ff010203"
            property color outline: "#ff404040"
            property color foregroundInactive: "#ffaaaaaa"
        }
    )qml");

    dino::Theme theme;
    QString error;
    require(dino::importTheme(path, &theme, &error), "QML theme import failed");
    require(theme.colors.value("background").alpha() == 0x80, "QML #AARRGGBB alpha was lost");
    require(theme.colors.value("background").red() == 0x11, "QML #AARRGGBB red channel shifted");
    require(theme.colors.value("accent").red() == 12, "Qt.rgba red channel shifted");
    require(theme.colors.value("accent").green() == 34, "Qt.rgba green channel shifted");
    require(theme.colors.value("accent").blue() == 56, "Qt.rgba blue channel shifted");
    require(theme.colors.value("accent").alpha() == 128, "Qt.rgba alpha was not preserved");
}

void jsonPaletteImportHandlesShellFormats() {
    QTemporaryDir directory;
    require(directory.isValid(), "could not create temporary directory");

    const QString caelestiaPath = directory.filePath("caelestia.json");
    writeFile(caelestiaPath, R"json({
        "colors": {
            "m3background": "#80112233",
            "m3onBackground": "#ffeeddcc",
            "m3primary": "Qt.rgba(171 / 255, 205 / 255, 239 / 255, 0.5)",
            "mSurfaceVariant": "#ff282828",
            "mOnSurfaceVariant": "#ffaaaaaa",
            "mOutline": "#ff555555",
            "mOnPrimary": "#ff101010"
        }
    })json");

    const QString noctaliaPath = directory.filePath("noctalia.json");
    writeFile(noctaliaPath, R"json({
        "mode": "dark",
        "dark": {
            "background": "#ff101820",
            "foreground": "#fff0e8df",
            "primary": "#ffcc9966",
            "surfaceContainerLow": "#ff202830",
            "onSurfaceVariant": "#ff9a9a9a",
            "outline": "#ff505860",
            "onPrimary": "#ff101010"
        },
        "light": { "background": "#ffffffff" }
    })json");

    dino::Theme caelestia;
    dino::Theme noctalia;
    QString error;
    require(dino::importTheme(caelestiaPath, &caelestia, &error), "Caelestia JSON import failed");
    require(dino::importTheme(noctaliaPath, &noctalia, &error), "Noctalia JSON import failed");
    require(caelestia.colors.value("background").alpha() == 0x80,
            "Caelestia palette alpha was lost");
    require(caelestia.colors.value("accent").alpha() == 128,
            "Caelestia Qt.rgba alpha was lost");
    require(caelestia.colors.value("accent").red() == 171,
            "Caelestia Qt.rgba channel order changed");
    require(caelestia.colors.value("accent_text").red() == 0x10,
            "Caelestia on-primary color was not mapped");
    require(noctalia.colors.value("background").red() == 0x10,
            "Noctalia dark palette mode was not selected");
}

void failuresKeepCallerTheme() {
    QTemporaryDir directory;
    require(directory.isValid(), "could not create temporary directory");
    const QString path = directory.filePath("broken.qml");
    writeFile(path, "QtObject { property color background: \"#xx112233\" }");

    dino::Theme current = dino::defaultTheme();
    current.name = "Keep this value";
    const dino::Theme original = current;
    QString error;
    require(!dino::importTheme(path, &current, &error), "malformed QML palette was accepted");
    require(sameTheme(current, original), "failed QML import modified caller theme");

    writeFile(path, QByteArray(1024 * 1024 + 1, ' '));
    require(!dino::importTheme(path, &current, &error), "oversized QML palette was accepted");
    require(sameTheme(current, original), "oversized QML import modified caller theme");
}

void saveRejectsItsOwnSource() {
    QTemporaryDir directory;
    require(directory.isValid(), "could not create temporary directory");
    const QString path = directory.filePath("theme.json");
    dino::Theme original = dino::defaultTheme();
    original.source.clear();
    QString error;
    require(dino::saveTheme(path, original, &error), "fixture theme save failed");

    dino::Theme imported;
    const QString alias = directory.filePath("theme-alias.json");
    require(QFile::link(path, alias), "could not create source symlink");
    require(dino::importTheme(alias, &imported, &error), "theme JSON import failed");
    require(imported.source == QFileInfo(alias).absoluteFilePath(), "import did not retain its source path");
    require(!dino::saveTheme(path, imported, &error), "theme was saved over its own source");

    dino::Theme loaded;
    require(dino::loadTheme(path, &loaded, &error), "self-source rejection damaged original file");
}

void refreshIsTransactional() {
    QTemporaryDir directory;
    require(directory.isValid(), "could not create temporary directory");
    const QString path = directory.filePath("Colors.qml");
    const QByteArray first = R"qml(QtObject {
        property color background: "#ff101010"
        property color foreground: "#fff0f0f0"
        property color primary: "#ff123456"
    })qml";
    const QByteArray second = R"qml(QtObject {
        property color background: "#ff202020"
        property color foreground: "#ffe0e0e0"
        property color primary: "#ff654321"
    })qml";
    writeFile(path, first);

    dino::Theme theme;
    QString error;
    require(dino::importTheme(path, &theme, &error), "initial QML import failed");
    const QString name = theme.name;
    writeFile(path, second);
    require(dino::refreshFollowedTheme(&theme, &error), "valid source refresh failed");
    require(theme.colors.value("accent").red() == 0x65, "source refresh did not update colors");
    require(theme.name == name, "source refresh changed the displayed theme name");

    const dino::Theme beforeFailure = theme;
    writeFile(path, "QtObject { property color primary: Qt.rgba(1.2, 0, 0, 1) }");
    require(!dino::refreshFollowedTheme(&theme, &error), "invalid refreshed palette was accepted");
    require(sameTheme(theme, beforeFailure), "failed refresh modified caller theme");
}

void defaultFallbackUsesLofiPalette() {
    const dino::Theme theme = dino::defaultTheme();
    if (!theme.source.isEmpty())
        return;
    require(theme.colors.value("background") == QColor("#18212b"), "default background changed");
    require(theme.colors.value("surface") == QColor("#222e3a"), "default surface changed");
    require(theme.colors.value("text") == QColor("#d6e2e8"), "default text changed");
    require(theme.colors.value("muted") == QColor("#8a9ba8"), "default muted color changed");
    require(theme.colors.value("accent") == QColor("#a8c7c0"), "default accent changed");
}

}

int main() {
    try {
        jsonRoundTripKeepsAlpha();
        qmlImportKeepsQtColorOrder();
        jsonPaletteImportHandlesShellFormats();
        failuresKeepCallerTheme();
        saveRejectsItsOwnSource();
        refreshIsTransactional();
        defaultFallbackUsesLofiPalette();
    } catch (const std::exception &exception) {
        std::cerr << "theme test failed: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
