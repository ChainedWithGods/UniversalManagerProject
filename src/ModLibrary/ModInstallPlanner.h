#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class ModInstallPlanner {
public:
    struct Context {
        QString modsPath;
        QString gamePaksPath;

        QString luaModsRoot;

        QString installName;
    };

    struct Result {
        bool recognized = false;
        bool valid = false;
        QString error;
        QString paksRoot;
        QString logicRoot;
        QString normalRoot;
        QVariantList groups;
    };

    static Result analyze(const QString& extractedRoot, const Context& context);

    static bool installGroup(const QVariantMap& group, const QStringList& selectedFiles,
                             const QString& folderName, const QString& bundleFolder,
                             int* copiedFiles, QString* error);
};
