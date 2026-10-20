#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace dino {
namespace {

constexpr qint64 maximumInputSize = 1024 * 1024;
const QStringList requiredKeys{
    "background", "surface", "text", "muted", "accent", "border", "accent_text"
};

[[noreturn]] void fail(const QString &message) {
    throw std::invalid_argument(message.toStdString());
}

void setError(QString *error, const QString &message) {
    if (error)
        *error = message;
}

QString expandedPath(QString path) {
    if (path == "~")
        path = QDir::homePath();
    else if (path.startsWith("~/"))
        path = QDir::homePath() + path.mid(1);
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString configHome() {
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return path.isEmpty() ? QDir::homePath() + "/.config" : path;
}

QString canonicalComparisonPath(const QString &path) {
    const QString absolute = expandedPath(path);
    const QFileInfo info(absolute);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty())
        return QDir::cleanPath(canonical);

    const QString canonicalParent = info.dir().canonicalPath();
    if (!canonicalParent.isEmpty())
        return QDir::cleanPath(QDir(canonicalParent).filePath(info.fileName()));
    return absolute;
}

bool samePath(const QString &left, const QString &right) {
    return !left.isEmpty() && !right.isEmpty()
        && canonicalComparisonPath(left) == canonicalComparisonPath(right);
}

QByteArray readBounded(const QString &path) {
    if (!QFileInfo(path).isFile())
        fail("Theme source must be a regular file: " + path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        fail("Cannot read theme source: " + path);
    if (file.size() > maximumInputSize)
        fail("Theme input exceeds 1 MiB: " + path);
    const QByteArray bytes = file.read(maximumInputSize + 1);
    if (bytes.size() > maximumInputSize)
        fail("Theme input exceeds 1 MiB: " + path);
    if (file.error() != QFileDevice::NoError)
        fail("Cannot read theme source: " + path);
    return bytes;
}

QColor parseRgba(const QString &expression);

QColor parseColor(QString value) {
    value = value.trimmed();
    if (value.startsWith("Qt.rgba(") && value.endsWith(')'))
        return parseRgba(value.mid(8, value.size() - 9));
    static const QRegularExpression colorPattern(R"(^#?([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$)");
    const auto match = colorPattern.match(value);
    if (!match.hasMatch())
        fail("Unsupported palette color: " + value);
    const QColor color("#" + match.captured(1));
    if (!color.isValid())
        fail("Unsupported palette color: " + value);
    return color;
}

double channel(QString expression) {
    const QStringList parts = expression.trimmed().split('/');
    if (parts.size() > 2)
        fail("Unsupported Qt.rgba channel: " + expression);

    bool valid = false;
    double value = parts.at(0).trimmed().toDouble(&valid);
    if (!valid)
        fail("Invalid Qt.rgba channel: " + expression);

    if (parts.size() == 2) {
        bool denominatorValid = false;
        const double denominator = parts.at(1).trimmed().toDouble(&denominatorValid);
        if (!denominatorValid || denominator <= 0)
            fail("Invalid Qt.rgba divisor: " + expression);
        value /= denominator;
    }

    if (!std::isfinite(value) || value < 0 || value > 1)
        fail("Qt.rgba channels must be between 0 and 1");
    return value;
}

QColor parseRgba(const QString &expression) {
    const QStringList channels = expression.split(',');
    if (channels.size() != 4)
        fail("Qt.rgba needs four channels");
    return QColor::fromRgbF(channel(channels.at(0)), channel(channels.at(1)),
                            channel(channels.at(2)), channel(channels.at(3)));
}

QMap<QString, QColor> qmlPalette(const QString &path) {
    QString contents = QString::fromUtf8(readBounded(path));
    contents.remove(QRegularExpression("/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption));
    contents.remove(QRegularExpression("//[^\\n]*"));

    static const QRegularExpression propertyPattern(
        R"REGEX(property\s+color\s+(\w+)\s*:\s*(?:"(#[\da-fA-F]{6}(?:[\da-fA-F]{2})?)"|'(#[\da-fA-F]{6}(?:[\da-fA-F]{2})?)'|Qt\.rgba\(([^)]*)\)))REGEX");
    QMap<QString, QColor> palette;
    auto matches = propertyPattern.globalMatch(contents);
    while (matches.hasNext()) {
        const auto match = matches.next();
        const QString key = match.captured(1);
        if (!match.captured(2).isEmpty())
            palette.insert(key, parseColor(match.captured(2)));
        else if (!match.captured(3).isEmpty())
            palette.insert(key, parseColor(match.captured(3)));
        else
            palette.insert(key, parseRgba(match.captured(4)));
    }
    if (palette.isEmpty())
        fail("No supported color properties in " + path);
    return palette;
}

QJsonObject readJsonObject(const QString &path) {
    const QByteArray bytes = readBounded(path);
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        fail("Invalid theme JSON in " + path + ": " + parseError.errorString());
    return document.object();
}

QString normalizedKey(QString key) {
    key = key.toLower();
    key.remove(QRegularExpression("[^a-z0-9]"));
    return key;
}

QJsonObject paletteObject(QJsonObject data) {
    for (int depth = 0; depth < 4; ++depth) {
        if (data.value("colors").isObject()) {
            data = data.value("colors").toObject();
            continue;
        }
        if (data.value("colours").isObject()) {
            data = data.value("colours").toObject();
            continue;
        }
        if (data.value("palette").isObject()) {
            data = data.value("palette").toObject();
            continue;
        }
        if (data.value("dark").isObject() || data.value("light").isObject()) {
            QString mode = data.value("mode").toString().toLower();
            if (mode != "dark" && mode != "light")
                mode = "dark";
            if (!data.value(mode).isObject())
                mode = mode == "dark" ? "light" : "dark";
            data = data.value(mode).toObject();
            continue;
        }
        break;
    }

    QJsonObject result;
    for (auto entry = data.constBegin(); entry != data.constEnd(); ++entry) {
        if (!entry.value().isString())
            continue;
        const QString value = entry.value().toString().trimmed();
        static const QRegularExpression colorPattern(R"(^#?[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?$)");
        if (colorPattern.match(value).hasMatch()
            || (value.startsWith("Qt.rgba(") && value.endsWith(')')))
            result.insert(entry.key(), value);
    }
    return result;
}

QMap<QString, QColor> jsonPalette(const QJsonObject &data) {
    const QJsonObject values = paletteObject(data);
    QMap<QString, QColor> palette;
    for (auto entry = values.constBegin(); entry != values.constEnd(); ++entry)
        palette.insert(entry.key(), parseColor(entry.value().toString()));
    if (palette.isEmpty())
        fail("No supported colors in palette JSON");
    return palette;
}

QColor mix(const QColor &base, const QColor &tint, double amount) {
    return QColor::fromRgbF(base.redF() * (1 - amount) + tint.redF() * amount,
                            base.greenF() * (1 - amount) + tint.greenF() * amount,
                            base.blueF() * (1 - amount) + tint.blueF() * amount,
                            base.alphaF() * (1 - amount) + tint.alphaF() * amount);
}

double luminance(const QColor &color) {
    const auto linear = [](double component) {
        return component <= .04045 ? component / 12.92 : std::pow((component + .055) / 1.055, 2.4);
    };
    return .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
}

QColor pick(const QMap<QString, QColor> &palette, const QStringList &aliases, const QColor &fallback = {}) {
    QMap<QString, QColor> normalized;
    for (auto entry = palette.constBegin(); entry != palette.constEnd(); ++entry)
        normalized.insert(normalizedKey(entry.key()), entry.value());
    for (const auto &alias : aliases) {
        const QString key = normalizedKey(alias);
        if (normalized.contains(key))
            return normalized.value(key);
    }
    if (fallback.isValid())
        return fallback;
    fail("Palette is missing " + aliases.join(" / "));
}

Theme mapPalette(const QMap<QString, QColor> &palette, const QString &name, const QString &source) {
    const QColor background = pick(palette, {"background", "base", "m3background", "mBackground", "mSurface", "surface"});
    const QColor text = pick(palette, {"foreground", "onBackground", "on_background", "text", "mOnSurface", "onSurface", "on_surface", "m3onBackground", "m3onSurface"});
    const QColor accent = pick(palette, {"primary", "accent", "mPrimary", "m3primary"});
    const QColor surface = pick(palette, {"surface_container_low", "surfaceContainerLow", "backgroundAlt", "mantle", "surface0", "surface"}, mix(background, text, .035));
    const QColor muted = pick(palette, {"foregroundInactive", "on_surface_variant", "onSurfaceVariant", "mOnSurfaceVariant", "muted"}, mix(background, text, .65));
    const QColor border = pick(palette, {"outline", "border", "mOutline"}, mix(surface, accent, .25));
    const QColor accentText = pick(palette, {"on_primary", "onPrimary", "mOnPrimary", "onAccent", "accentText"},
                                   luminance(accent) > .4 ? QColor("#151515") : QColor("#ffffff"));

    return Theme{{
        {"background", background}, {"surface", surface}, {"text", text}, {"muted", muted},
        {"accent", accent}, {"border", border}, {"accent_text", accentText}
    }, name, source};
}

QJsonObject canonicalJson(const QJsonObject &data) {
    if (data.value("version").toInt(-1) != 1)
        fail("Theme version must be 1");
    const QString name = data.value("name").toString().trimmed();
    if (name.isEmpty() || name.size() > 60)
        fail("Theme name must contain 1–60 characters");
    if (!data.value("colors").isObject())
        fail("Theme must contain a colors object");
    const QJsonObject colors = data.value("colors").toObject();
    for (auto entry = colors.constBegin(); entry != colors.constEnd(); ++entry) {
        if (!requiredKeys.contains(entry.key()))
            fail("Unknown color: " + entry.key());
    }
    for (const QString &key : requiredKeys) {
        if (!colors.value(key).isString())
            fail("Theme is missing color: " + key);
        parseColor(colors.value(key).toString());
    }

    const QJsonValue sourceValue = data.value("source");
    if (!sourceValue.isUndefined() && !sourceValue.isString())
        fail("Theme source must be an absolute path string");
    if (sourceValue.isString() && !sourceValue.toString().isEmpty()
        && !QFileInfo(sourceValue.toString()).isAbsolute())
        fail("Theme source must be an absolute path");
    return data;
}

Theme themeFromCanonicalJson(const QJsonObject &data) {
    const QJsonObject validated = canonicalJson(data);
    const QJsonObject colors = validated.value("colors").toObject();
    Theme theme;
    theme.name = validated.value("name").toString().trimmed();
    theme.source = validated.value("source").toString();
    for (const QString &key : requiredKeys)
        theme.colors.insert(key, parseColor(colors.value(key).toString()));
    return theme;
}

QString resolveSourceFile(const QString &requestedPath) {
    QString path = expandedPath(requestedPath);
    if (!QFileInfo::exists(path) && !requestedPath.contains('/') && !requestedPath.contains('\\'))
        path = expandedPath(configHome() + "/themes/" + requestedPath);

    const QFileInfo info(path);
    if (info.isDir()) {
        const QStringList names{"Colors.qml", "noctalia.json", "palette.json", "colors.json", "theme.json"};
        for (const QString &name : names) {
            const QString candidate = QDir(path).filePath(name);
            if (QFileInfo(candidate).isFile() || QFileInfo(candidate).isSymLink())
                return candidate;
        }
        fail("No supported palette found in: " + path);
    }
    if (!info.isFile() && !info.isSymLink())
        fail("Theme source does not exist: " + path);
    return path;
}

QString inferredName(const QString &path) {
    const QString directoryName = QFileInfo(path).dir().dirName();
    if (directoryName == ".caelestia-use")
        return "Active";
    if (!directoryName.isEmpty() && directoryName != ".")
        return directoryName.left(60);
    return QFileInfo(path).completeBaseName().left(60);
}

Theme lofiTheme() {
    return Theme{{
        {"background", QColor("#18212b")}, {"surface", QColor("#222e3a")},
        {"text", QColor("#d6e2e8")}, {"muted", QColor("#8a9ba8")},
        {"accent", QColor("#a8c7c0")}, {"border", QColor("#3a4a56")},
        {"accent_text", QColor("#18212b")}
    }, "Lo-fi", {}};
}

bool perform(QString *error, const std::function<void()> &operation) {
    try {
        operation();
        if (error)
            error->clear();
        return true;
    } catch (const std::exception &exception) {
        setError(error, QString::fromUtf8(exception.what()));
        return false;
    }
}

}

QJsonObject Theme::toJson() const {
    QJsonObject serializedColors;
    for (const QString &key : requiredKeys)
        serializedColors.insert(key, colors.value(key).name(QColor::HexArgb));
    QJsonObject result{{"version", 1}, {"name", name}, {"colors", serializedColors}};
    if (!source.isEmpty())
        result.insert("source", source);
    return result;
}

Theme defaultTheme() {
    return lofiTheme();
}

bool parseTheme(const QJsonObject &data, Theme *theme, QString *error) {
    if (!theme) {
        setError(error, "Theme output cannot be null");
        return false;
    }

    Theme parsed;
    if (!perform(error, [&] { parsed = themeFromCanonicalJson(data); }))
        return false;
    *theme = parsed;
    return true;
}

bool loadTheme(const QString &path, Theme *theme, QString *error) {
    if (!theme) {
        setError(error, "Theme output cannot be null");
        return false;
    }

    Theme loaded;
    if (!perform(error, [&] {
        const QString resolvedPath = expandedPath(path);
        loaded = themeFromCanonicalJson(readJsonObject(resolvedPath));
        if (!loaded.source.isEmpty() && samePath(resolvedPath, loaded.source))
            fail("Theme cannot use itself as a source");
    }))
        return false;
    *theme = loaded;
    return true;
}

bool saveTheme(const QString &path, const Theme &theme, QString *error) {
    if (path.trimmed().isEmpty()) {
        setError(error, "Theme path cannot be empty");
        return false;
    }

    return perform(error, [&] {
        const QString destination = expandedPath(path);
        const Theme validated = themeFromCanonicalJson(theme.toJson());
        if (!validated.source.isEmpty() && samePath(destination, validated.source))
            fail("Theme cannot be saved over its own source");

        const QString parentPath = QFileInfo(destination).absolutePath();
        if (!QDir().mkpath(parentPath))
            fail("Cannot create theme directory: " + parentPath);
        if (!validated.source.isEmpty() && samePath(destination, validated.source))
            fail("Theme cannot be saved over its own source");

        QSaveFile file(destination);
        if (!file.open(QIODevice::WriteOnly))
            fail("Cannot write theme: " + destination);
        const QByteArray bytes = QJsonDocument(validated.toJson()).toJson(QJsonDocument::Indented);
        if (file.write(bytes) != bytes.size()) {
            file.cancelWriting();
            fail("Cannot write theme: " + destination);
        }
        if (!file.commit())
            fail("Cannot commit theme: " + destination);
    });
}

bool importTheme(const QString &sourcePath, Theme *theme, QString *error) {
    if (!theme) {
        setError(error, "Theme output cannot be null");
        return false;
    }

    Theme imported;
    if (!perform(error, [&] {
        const QString path = resolveSourceFile(sourcePath);
        const QString absoluteSource = expandedPath(path);
        if (path.endsWith(".qml", Qt::CaseInsensitive)) {
            imported = mapPalette(qmlPalette(path), inferredName(path), absoluteSource);
            return;
        }

        const QJsonObject json = readJsonObject(path);
        const QJsonObject values = json.value("colors").toObject();
        bool canonical = json.value("version").toInt(-1) == 1
            && !json.value("name").toString().trimmed().isEmpty();
        for (const QString &key : requiredKeys)
            canonical = canonical && values.contains(key);
        if (canonical) {
            imported = themeFromCanonicalJson(json);
            imported.source = absoluteSource;
            return;
        }

        QString name = json.value("name").toString().trimmed();
        if (name.isEmpty())
            name = inferredName(path);
        imported = mapPalette(jsonPalette(json), name.left(60), absoluteSource);
    }))
        return false;
    *theme = imported;
    return true;
}

bool followActiveTheme(Theme *theme, QString *error) {
    if (!theme) {
        setError(error, "Theme output cannot be null");
        return false;
    }

    const QString path = QDir(configHome()).filePath("themes/.caelestia-use/Colors.qml");
    return importTheme(path, theme, error);
}

bool refreshFollowedTheme(Theme *theme, QString *error) {
    if (!theme) {
        setError(error, "Theme output cannot be null");
        return false;
    }
    if (theme->source.isEmpty()) {
        setError(error, "Theme does not have a followed source");
        return false;
    }

    Theme refreshed;
    if (!importTheme(theme->source, &refreshed, error))
        return false;
    refreshed.name = theme->name;
    *theme = refreshed;
    return true;
}

}
