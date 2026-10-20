#pragma once
#include <QStringList>
namespace dino {
struct Options {
    QString configDirectory;
    QStringList command;
    bool dryRun = false;
    bool help = false;
    bool version = false;
};
Options parseOptions(const QStringList &arguments);
int runCommand(const Options &options);
void printHelp();
}
