#pragma once
#include "core/theme.h"
#include <QFileSystemWatcher>
#include <QTimer>

namespace dino {
class ThemeWatcher : public QObject {
    Q_OBJECT
public:
    ThemeWatcher(QString path, Theme initial, QObject *parent = nullptr);
    const Theme &theme() const { return theme_; }
signals:
    void changed();
    void recovered();
    void rejected(const QString &message);
private:
    void reload();
    void watch();
    QString path_;
    Theme theme_;
    QFileSystemWatcher watcher_;
    QTimer debounce_;
    QTimer fallback_;
    bool rejected_ = false;
};
}
