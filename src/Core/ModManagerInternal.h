#pragma once

#include "modmanager.h"
#include "luaprofilemanager.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QDebug>
#include <QVariantMap>
#include <QGuiApplication>
#include <QFileSystemWatcher>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QSaveFile>
#include <QDataStream>
#include <QMutex>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QDateTime>
#include <QProcess>
#include <QtConcurrent/QtConcurrent>
#include <QStorageInfo>
#include <QTextStream>
#include <QTimer>
#include <QUuid>
#include <QUrl>
#include <QRandomGenerator>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QClipboard>
#include <QSettings>
#include <QElapsedTimer>
#include <functional>
#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

namespace mmdetail {
    QString normalizeUEVersionCacheKey(const QString& gamePath);
    QString canonicalOrCleanPath(const QString& path);
    QString findProjectRoot();
    QString resolveConfiguredOrKnownToolPath(const QString& configuredPath, const QStringList& possiblePaths);
    bool isUnrealModExtension(const QString& suffix);
    QString normalizedModBaseName(const QFileInfo& fileInfo);
    QString normalizedModKey(const QString& name);
    QString downloadLibraryFolderName(bool updates);
    QString findCommonPrefix(const QStringList& names);
    QMap<QString, QStringList> groupByCommonPrefix(const QStringList& baseNames);
    QString createRandomModId();
    QString missingToolStatusText(const QString& configuredPath, const QString& toolName);
    QVariantMap makeDetectedGame(const QString& displayName, const QString& gameDir, const QString& paksPath);
    void applyManagerConsoleVisibility(bool hideConsole);
    bool directoryHasPakFilesFast(const QString& dirPath);
    bool shouldSkipDeepScanDirectoryName(const QString& name);
    QString normalizeScanPath(QString path);
    void collectRegistryInstallLocations(const QString& registryPath, QStringList& out);
    QStringList readEpicLauncherInstallPaths();
    QStringList readWindowsInstallLocations();
    QString detectGamePaksPathFromGameExecutable(const QString& gameExePath);
    int countDisplayModsInPath(const QString& modsPath);
    QString normalizedDisplayModName(QString fileName);
    QString normalizePathForCompare(QString maybeUrlOrPath);
    QMap<QString, QString> defaultShortcuts();
    QSet<QString> collectDisplayModNamesInPath(const QString& modsPath);
    QString findGameExecutableFromFolder(const QString& gameFolder, const QString& gameName);
}
