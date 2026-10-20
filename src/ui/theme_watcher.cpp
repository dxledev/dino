#include "theme_watcher.h"
#include <QFileInfo>

namespace dino {
ThemeWatcher::ThemeWatcher(QString path, Theme initial, QObject *parent)
    : QObject(parent), path_(std::move(path)), theme_(std::move(initial)) {
    debounce_.setSingleShot(true);
    debounce_.setInterval(40);
    connect(&watcher_, &QFileSystemWatcher::fileChanged, this, [this] { debounce_.start(); });
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, [this] { debounce_.start(); });
    connect(&debounce_, &QTimer::timeout, this, &ThemeWatcher::reload);
    // Polling also catches symlink retargets whose parent directory is replaced.
    fallback_.setInterval(1000);
    connect(&fallback_, &QTimer::timeout, this, &ThemeWatcher::reload);
    fallback_.start();
    watch();
    QTimer::singleShot(0, this, &ThemeWatcher::reload);
}
void ThemeWatcher::watch() {
    QStringList paths;
    for (const auto &path : {path_, theme_.source}) {
        if (path.isEmpty()) continue;
        const QFileInfo info(path);
        for (const auto &candidate : {path, info.absolutePath(), info.canonicalFilePath()})
            if (!candidate.isEmpty() && QFileInfo::exists(candidate)) paths.append(candidate);
    }
    paths.removeDuplicates();
    const auto current = watcher_.files() + watcher_.directories();
    for (const auto &path : current) if (!paths.contains(path)) watcher_.removePath(path);
    for (const auto &path : paths) if (!current.contains(path)) watcher_.addPath(path);
}
void ThemeWatcher::reload() {
    Theme next;
    QString error;
    if (!loadTheme(path_, &next, &error)) { rejected_ = true; emit rejected(error); watch(); return; }
    if (!next.source.isEmpty()) {
        if (QFileInfo(next.source).canonicalFilePath() == QFileInfo(path_).canonicalFilePath()) {
            rejected_ = true; emit rejected("Theme source cannot be its output file"); return;
        }
        const auto previous = next.toJson();
        if (!refreshFollowedTheme(&next, &error)) { rejected_ = true; emit rejected(error); watch(); return; }
        if (next.toJson() != previous && !saveTheme(path_, next, &error)) {
            rejected_ = true; emit rejected(error); return;
        }
    }
    if (next.toJson() != theme_.toJson()) { theme_ = std::move(next); emit changed(); }
    if (rejected_) { rejected_ = false; emit recovered(); }
    watch();
}
}
