#pragma once

#include <QColor>
#include <QJsonObject>
#include <QMap>
#include <QString>

namespace dino {

struct Theme {
    QMap<QString, QColor> colors;
    QString name;
    QString source;

    QJsonObject toJson() const;
};

Theme defaultTheme();
bool parseTheme(const QJsonObject &data, Theme *theme, QString *error = nullptr);
bool loadTheme(const QString &path, Theme *theme, QString *error = nullptr);
bool saveTheme(const QString &path, const Theme &theme, QString *error = nullptr);
bool importTheme(const QString &sourcePath, Theme *theme, QString *error = nullptr);
bool followActiveTheme(Theme *theme, QString *error = nullptr);
bool refreshFollowedTheme(Theme *theme, QString *error = nullptr);

}
