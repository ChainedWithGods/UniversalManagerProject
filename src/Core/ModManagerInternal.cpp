#include "ModManagerInternal.h"
#include <algorithm>

namespace mmdetail {

QString normalizeUEVersionCacheKey(const QString& gamePath) {
    QString normalized = QDir::cleanPath(gamePath);
#ifdef Q_OS_WIN
    normalized = normalized.toLower();
#endif
    return normalized;
}

QString canonicalOrCleanPath(const QString& path) {
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(path) : canonical;
}

QString findProjectRoot() {
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8 && dir.exists(); ++i) {
        if (QFileInfo::exists(dir.filePath("Tools")))
            return dir.absolutePath();
        if (!dir.cdUp())
            break;
    }
    return QCoreApplication::applicationDirPath();
}

QString resolveConfiguredOrKnownToolPath(const QString& configuredPath, const QStringList& possiblePaths) {
    const QString trimmedConfiguredPath = configuredPath.trimmed();
    if (!trimmedConfiguredPath.isEmpty()) {
        const QString cleanConfiguredPath = QDir::cleanPath(trimmedConfiguredPath);
        if (QFile::exists(cleanConfiguredPath)) {
            return QFileInfo(cleanConfiguredPath).absoluteFilePath();
        }
    }

    for (const QString& path : possiblePaths) {
        const QString cleanPath = QDir::cleanPath(path);
        if (QFile::exists(cleanPath)) {
            return QFileInfo(cleanPath).absoluteFilePath();
        }
    }

    return QString();
}

bool isUnrealModExtension(const QString& suffix) {
    const QString lower = suffix.trimmed().toLower();
    return lower == "pak" || lower == "pak-x" ||
           lower == "ucas" || lower == "ucas-x" ||
           lower == "utoc" || lower == "utoc-x";
}

QString normalizedModBaseName(const QFileInfo& fileInfo) {
    QString baseName = fileInfo.completeBaseName();
    if (baseName.toUpper().endsWith("_P")) {
        baseName = baseName.left(baseName.length() - 2);
    }
    return baseName;
}

QString downloadLibraryFolderName(bool updates) {
    return updates ? QStringLiteral("Universal Manager Updates")
                   : QStringLiteral("Universal Manager Downloads");
}

QString normalizedModKey(const QString& name) {
    QString key = name.trimmed().toLower();
    static const QStringList suffixes = {
        QStringLiteral(".pak"), QStringLiteral(".ucas"), QStringLiteral(".utoc")
    };
    for (const QString& suffix : suffixes) {
        if (key.endsWith(suffix + QStringLiteral("-x"))) {
            key.chop(suffix.size() + 2);
            break;
        }
        if (key.endsWith(suffix)) {
            key.chop(suffix.size());
            break;
        }
    }
    if (key.endsWith(QLatin1String("_p")))
        key.chop(2);
    return key;
}

QString findCommonPrefix(const QStringList& names) {
    if (names.isEmpty()) return QString();
    if (names.size() == 1) return names.first();

    QString prefix = names.first();
    for (int i = 1; i < names.size(); ++i) {
        const QString& current = names[i];
        int minLen = qMin(prefix.length(), current.length());
        int commonLen = 0;

        for (int j = 0; j < minLen; ++j) {
            if (prefix[j] == current[j]) {
                commonLen++;
            } else {
                break;
            }
        }

        prefix = prefix.left(commonLen);
        if (prefix.isEmpty()) break;
    }

    while (prefix.endsWith('_') || prefix.endsWith('-')) {
        prefix.chop(1);
    }

    return prefix;
}

QMap<QString, QStringList> groupByCommonPrefix(const QStringList& baseNames) {
    QMap<QString, QStringList> groups;
    QStringList sorted = baseNames;

    std::sort(sorted.begin(), sorted.end(), [](const QString& left, const QString& right) {
        if (left.size() != right.size())
            return left.size() < right.size();
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });

    QSet<QString> assigned;
    for (const QString& root : sorted) {
        if (assigned.contains(root))
            continue;

        QStringList related{root};
        assigned.insert(root);

        for (const QString& candidate : sorted) {
            if (candidate.size() <= root.size() || assigned.contains(candidate))
                continue;
            if (!candidate.startsWith(root, Qt::CaseInsensitive))
                continue;

            if (!candidate.at(root.size()).isLetterOrNumber()) {
                related.append(candidate);
                assigned.insert(candidate);
            }
        }

        groups.insert(root, related);
    }

    return groups;
}

QString createRandomModId() {
    QString modId;
    const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (int i = 0; i < 8; i++) {
        modId += chars[QRandomGenerator::global()->bounded(36)];
    }
    return modId;
}

QString missingToolStatusText(const QString& configuredPath, const QString& toolName) {
    if (!configuredPath.trimmed().isEmpty()) {
        return QStringLiteral("Haven't set %1 at the configured path").arg(toolName);
    }

    return QStringLiteral("Haven't set %1").arg(toolName);
}

QVariantMap makeDetectedGame(const QString& displayName, const QString& gameDir, const QString& paksPath) {
    QVariantMap game;
    game["name"] = displayName.trimmed().isEmpty() ? QFileInfo(gameDir).fileName() : displayName.trimmed();
    game["path"] = QDir::cleanPath(gameDir);
    game["paksPath"] = QDir::cleanPath(paksPath);
    return game;
}

#ifdef Q_OS_WIN
void applyManagerConsoleVisibility(bool hideConsole) {

    if (HWND consoleWindow = GetConsoleWindow()) {
        ShowWindow(consoleWindow, hideConsole ? SW_HIDE : SW_SHOWNORMAL);
    }
}

bool directoryHasPakFilesFast(const QString& dirPath) {
    const QString nativeBase = QDir::toNativeSeparators(QDir::cleanPath(dirPath));
    const QStringList patterns = { "*.pak", "*.utoc" };

    for (const QString& pattern : patterns) {
        const QString searchPattern = nativeBase + "\\" + pattern;
        WIN32_FIND_DATAW findData;
        HANDLE handle = FindFirstFileExW(
            reinterpret_cast<LPCWSTR>(searchPattern.utf16()),
            FindExInfoBasic,
            &findData,
            FindExSearchNameMatch,
            nullptr,
            FIND_FIRST_EX_LARGE_FETCH);
        if (handle != INVALID_HANDLE_VALUE) {
            FindClose(handle);
            return true;
        }
    }

    return false;
}

bool shouldSkipDeepScanDirectoryName(const QString& name) {
    if (name.isEmpty() || name.startsWith('.')) {
        return true;
    }

    static const QSet<QString> skipSet = {

        "windows", "winsxs", "system32", "syswow64", "programdata", "$recycle.bin",
        "system volume information", "config.msi", "appdata", "microsoft", "boot",
        "recovery", "perflogs", "windowsapps", "users", "locallow",

        "node_modules", "vendor", "packages", ".git", ".github", ".svn", ".hg",
        ".vs", ".vscode", ".idea", ".gradle", ".cargo", ".rustup", ".m2",
        "target", "cmakefiles", "cmaketmp",

        "__pycache__", "venv", ".venv", "env", ".env", "site-packages", "pip-cache",

        "docker", ".docker", "wsl", "virtualbox vms", "vmware",

        "temp", "tmp", "cache", "caches", "localcache", "inetcache", "crashdumps"
    };

    return skipSet.contains(name.toLower());
}

QString normalizeScanPath(QString path) {
    path = QDir::cleanPath(path);
#ifdef Q_OS_WIN
    path = path.toLower();
#endif
    if (!path.endsWith("/")) {
        path.append("/");
    }
    return path;
}

void collectRegistryInstallLocations(const QString& registryPath, QStringList& out) {
    QSettings reg(registryPath, QSettings::NativeFormat);
    const QStringList groups = reg.childGroups();
    for (const QString& group : groups) {
        reg.beginGroup(group);
        const QString installLocation = reg.value("InstallLocation").toString().trimmed();
        reg.endGroup();
        if (!installLocation.isEmpty() && QDir(installLocation).exists()) {
            out.append(QDir::cleanPath(installLocation));
        }
    }
}

QStringList readEpicLauncherInstallPaths() {
    QStringList paths;

    QString programData = qEnvironmentVariable("ProgramData");
    if (programData.isEmpty()) {
        programData = "C:/ProgramData";
    }

    const QString manifestPath = QDir::fromNativeSeparators(programData)
        + "/Epic/UnrealEngineLauncher/LauncherInstalled.dat";
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return paths;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return paths;
    }

    const QJsonArray installs = doc.object().value("InstallationList").toArray();
    for (const QJsonValue& value : installs) {
        const QString installLocation = value.toObject().value("InstallLocation").toString().trimmed();
        if (!installLocation.isEmpty() && QDir(installLocation).exists()) {
            paths.append(QDir::cleanPath(installLocation));
        }
    }

    paths.removeDuplicates();
    return paths;
}

QStringList readWindowsInstallLocations() {
    QStringList paths;
    collectRegistryInstallLocations("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", paths);
    collectRegistryInstallLocations("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", paths);
    collectRegistryInstallLocations("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", paths);
    paths.removeDuplicates();
    return paths;
}
#else
bool directoryHasPakFilesFast(const QString& dirPath) {
    QDir dir(dirPath);
    return dir.exists() && !dir.entryList(QStringList() << "*.pak" << "*.utoc", QDir::Files).isEmpty();
}
#endif

QString detectGamePaksPathFromGameExecutable(const QString& gameExePath) {
    const QString decodedExePath = QUrl::fromPercentEncoding(gameExePath.toUtf8());
    const QString cleanExePath = QDir::cleanPath(decodedExePath);
    if (cleanExePath.isEmpty()) {
        return QString();
    }

    const QFileInfo exeInfo(cleanExePath);
    if (!exeInfo.exists() || !exeInfo.isFile()) {
        return QString();
    }

    QString currentDirPath = exeInfo.absolutePath();
    for (int depth = 0; depth < 10; ++depth) {
        const QDir baseDir(currentDirPath);
        const QStringList candidates = {
            QDir::cleanPath(baseDir.filePath("Content/Paks")),
            QDir::cleanPath(baseDir.filePath("Paks"))
        };

        for (const QString& candidate : candidates) {
            QDir paksDir(candidate);
            if (!paksDir.exists()) {
                continue;
            }
            const QStringList paks = paksDir.entryList(QStringList() << "*.pak" << "*.utoc", QDir::Files);
            if (paks.isEmpty()) {
                continue;
            }

            const QString canonical = QFileInfo(candidate).canonicalFilePath();
            return canonical.isEmpty() ? candidate : canonical;
        }

        const QString parentDirPath = QDir(currentDirPath).filePath("..");
        const QString cleanParent = QDir::cleanPath(parentDirPath);
        if (cleanParent == currentDirPath || cleanParent.isEmpty()) {
            break;
        }
        currentDirPath = cleanParent;
    }

    return QString();
}

int countDisplayModsInPath(const QString& modsPath) {
    if (modsPath.isEmpty()) {
        return 0;
    }

    QDir modsDir(modsPath);
    if (!modsDir.exists()) {
        return 0;
    }

    const QStringList modFileFilters = {
        "*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x"
    };

    int modCount = 0;

    QDirIterator dirIt(modsPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (dirIt.hasNext()) {
        const QString modFolderPath = dirIt.next();
        QDir modDir(modFolderPath);
        if (!modDir.entryList(modFileFilters, QDir::Files).isEmpty()) {
            modCount++;
        }
    }

    modCount += modsDir.entryList(modFileFilters, QDir::Files).count();
    return modCount;
}

QString normalizedDisplayModName(QString fileName) {
    if (fileName.endsWith("-x", Qt::CaseInsensitive)) {
        fileName.chop(2);
    }

    const int lastDot = fileName.lastIndexOf('.');
    if (lastDot != -1) {
        fileName = fileName.left(lastDot);
    }

    if (fileName.toUpper().endsWith("_P")) {
        fileName = fileName.left(fileName.length() - 2);
    }

    return fileName.trimmed().toLower();
}

QString normalizePathForCompare(QString maybeUrlOrPath) {
    QString path = QUrl(maybeUrlOrPath).toLocalFile();
    if (path.isEmpty()) {
        path = maybeUrlOrPath;
    }

    path = QDir::cleanPath(path);
    path.replace('\\', '/');
#ifdef Q_OS_WIN
    path = path.toLower();
#endif
    return path;
}

QMap<QString, QString> defaultShortcuts()
{
    return {
        { "install", "Ctrl+I" },
        { "backup", "Ctrl+B" },
        { "refresh", "F5" },
        { "settings", "Ctrl+," },
        { "search", "Ctrl+F" },
        { "selectAll", "Ctrl+A" },
        { "delete", "Delete" },
        { "toggleMod", "Space" },
        { "openFolder", "Ctrl+O" },
        { "home", "Ctrl+H" },
        { "enableAll", "Ctrl+Shift+E" },
        { "disableAll", "Ctrl+Shift+D" },
        { "showInPakViewer", "Ctrl+P" },
        { "exportModStructure", "Ctrl+E" }
    };
}

QSet<QString> collectDisplayModNamesInPath(const QString& modsPath) {
    QSet<QString> modNames;
    if (modsPath.isEmpty()) {
        return modNames;
    }

    QDir modsDir(modsPath);
    if (!modsDir.exists()) {
        return modNames;
    }

    const QStringList modFileFilters = {
        "*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x"
    };

    QDirIterator dirIt(modsPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (dirIt.hasNext()) {
        const QString modFolderPath = dirIt.next();
        QDir modDir(modFolderPath);

        QString sourceFileName;
        const QStringList pakFiles = modDir.entryList({"*.pak", "*.pak-x"}, QDir::Files);
        const QStringList ucasFiles = modDir.entryList({"*.ucas", "*.ucas-x"}, QDir::Files);
        const QStringList utocFiles = modDir.entryList({"*.utoc", "*.utoc-x"}, QDir::Files);

        if (!pakFiles.isEmpty()) {
            sourceFileName = pakFiles.first();
        } else if (!ucasFiles.isEmpty()) {
            sourceFileName = ucasFiles.first();
        } else if (!utocFiles.isEmpty()) {
            sourceFileName = utocFiles.first();
        }

        if (!sourceFileName.isEmpty()) {
            modNames.insert(normalizedDisplayModName(sourceFileName));
        }
    }

    const QStringList rootFiles = modsDir.entryList(modFileFilters, QDir::Files);
    for (const QString& rootFile : rootFiles) {
        modNames.insert(normalizedDisplayModName(rootFile));
    }

    return modNames;
}

QString normalizeForExeMatch(const QString& s) {
    QString out;
    out.reserve(s.size());
    for (const QChar& ch : s) {
        if (ch.isLetterOrNumber()) {
            out.append(ch.toLower());
        }
    }
    return out;
}

bool isBlacklistedExecutableName(const QString& fileNameLower) {
    static const QStringList blacklist = {
        "crashreportclient.exe",
        "unrealeditor.exe",
        "unrealeditor-cmd.exe",
        "ue4editor.exe",
        "ue4editor-cmd.exe",
        "ue5editor.exe",
        "ue5editor-cmd.exe",
        "easyanticheat.exe",
        "easyanticheat_launcher.exe",
        "eosoverlayrenderer.exe",
        "eosoverlayrenderer-win64-shipping.exe",
        "epicwebhelper.exe",
        "steamerrorreporter.exe",
        "UE_Detector.exe",
    };

    for (const QString& bad : blacklist) {
        if (fileNameLower == bad) {
            return true;
        }
    }
    return false;
}

int scoreExecutableCandidate(const QFileInfo& info, const QString& gameNameNormalized, const QString& relativeLower) {
    const QString fileNameLower = info.fileName().toLower();
    if (isBlacklistedExecutableName(fileNameLower)) {
        return -100000;
    }

    int score = 0;

    if (relativeLower.startsWith("binaries/win64/") || relativeLower.contains("/binaries/win64/")) {
        score += 60;
    }
    if (relativeLower.startsWith("binaries/") || relativeLower.contains("/binaries/")) {
        score += 15;
    }

    const QString fileNameNormalized = normalizeForExeMatch(info.completeBaseName());
    if (!gameNameNormalized.isEmpty()) {
        if (fileNameNormalized == gameNameNormalized) {
            score += 80;
        } else if (fileNameNormalized.contains(gameNameNormalized) || gameNameNormalized.contains(fileNameNormalized)) {
            score += 45;
        }
    }

    if (fileNameLower.contains("win64-shipping") || fileNameLower.contains("shipping")) {
        score += 35;
    }
    if (fileNameLower.contains("game")) {
        score += 8;
    }

    const qint64 size = info.size();
    if (size >= 50LL * 1024 * 1024) {
        score += 18;
    } else if (size >= 5LL * 1024 * 1024) {
        score += 10;
    } else if (size > 0 && size < 512LL * 1024) {
        score -= 20;
    }

    if (relativeLower.contains("/engine/")) {
        score -= 40;
    }

    return score;
}

QString findGameExecutableFromFolder(const QString& gameFolder, const QString& gameName) {
#ifndef Q_OS_WIN
    Q_UNUSED(gameFolder)
    Q_UNUSED(gameName)
    return QString();
#else
    if (gameFolder.isEmpty()) {
        return QString();
    }

    const QFileInfo folderInfo(gameFolder);
    if (!folderInfo.exists()) {
        return QString();
    }
    if (folderInfo.isFile() && folderInfo.suffix().compare("exe", Qt::CaseInsensitive) == 0) {
        return QDir::cleanPath(folderInfo.absoluteFilePath());
    }

    const QString root = QDir::cleanPath(folderInfo.absoluteFilePath());
    const QString gameNameNormalized = normalizeForExeMatch(gameName.isEmpty() ? QFileInfo(root).fileName() : gameName);

    QString bestPath;
    int bestScore = -1000000;

    QDirIterator it(root, {"*.exe"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString absPath = it.next();
        QFileInfo info(absPath);
        if (!info.exists() || !info.isFile()) {
            continue;
        }

        QString relative = QDir(root).relativeFilePath(info.absoluteFilePath());
        relative.replace('\\', '/');
        const int depth = relative.count('/');
        if (depth > 7) {
            continue;
        }

        const QString relativeLower = relative.toLower();
        const int score = scoreExecutableCandidate(info, gameNameNormalized, relativeLower);
        if (score > bestScore) {
            bestScore = score;
            bestPath = info.absoluteFilePath();
        }
    }

    if (bestScore < 0) {
        return QString();
    }
    return QDir::cleanPath(bestPath);
#endif
}

}
