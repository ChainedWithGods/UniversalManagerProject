#include "ModManagerInternal.h"

using namespace mmdetail;

#ifdef Q_OS_WIN
static bool pathIsOnCDrive(const QString& path)
{
    if (path.isEmpty())
        return false;
    const QString cleaned = QDir::cleanPath(path);
    return cleaned.left(2).compare("C:", Qt::CaseInsensitive) == 0
        && (cleaned.length() == 2 || cleaned.at(2) == '/');
}
#else
static bool pathIsOnCDrive(const QString&)
{
    return false;
}
#endif

static QString normalizeModsFolderType(const QString& folderType)
{
    const QString trimmed = folderType.trimmed();
    if (trimmed.compare("~mods", Qt::CaseInsensitive) == 0
        || trimmed.compare("mods", Qt::CaseInsensitive) == 0
        || trimmed.compare("logicmods", Qt::CaseInsensitive) == 0) {
        return trimmed;
    }
    return QStringLiteral("mods");
}

QString ModManager::getInstalledGamesCacheBasePath() const {
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDir.isEmpty()) {
        appDataDir = QCoreApplication::applicationDirPath();
    }
    QString cachePath = appDataDir + "/Cache";
    QDir dir(cachePath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return cachePath;
}

QString ModManager::getInstalledGamesCachePath() const {
    return getInstalledGamesCacheBasePath() + "/installed_games.bin";
}

QString ModManager::getInstalledGamesLegacyJsonCachePath() const {
    return getInstalledGamesCacheBasePath() + "/InstalledGames.json";
}

bool ModManager::saveInstalledGamesCache() {
    const QString basePath = getInstalledGamesCacheBasePath();
    const QString binPath = getInstalledGamesCachePath();
    const QString jsonPath = getInstalledGamesLegacyJsonCachePath();

    QSaveFile binFile(binPath);
    if (binFile.open(QIODevice::WriteOnly)) {
        QDataStream out(&binFile);
        out.setVersion(QDataStream::Qt_6_5);

        const quint32 magic = 0x554D4D47;
        const quint32 version = 2;
        const qint64 timestamp = QDateTime::currentMSecsSinceEpoch();

        out << magic;
        out << version;
        out << timestamp;

        const quint32 count = static_cast<quint32>(m_installedGamesList.size());
        out << count;

        for (const QVariant& var : m_installedGamesList) {
            const QVariantMap game = var.toMap();
            const QString name = game.value("name").toString();
            const QString path = game.value("path").toString();
            const QString paksPath = game.value("paksPath").toString();

            QString ueVersion = game.value("ueVersion").toString();
            if (ueVersion.isEmpty()) {
                ueVersion = getUEVersionForGame(path);
            }

            const bool customGame = game.value("customGame", false).toBool() || isCustomGameDetectedForGame(path);
            const QString customDisplay = getCustomGameDisplayNameForGame(path);
            const QString customEnum = getCustomGameForGame(path);
            const QString customBase = getCustomGameBaseVersionForGame(path);
            const QString exePath = game.value("exePath").toString();
            const qint64 lastModified = game.value("lastModified", 0).toLongLong();

            out << name << path << paksPath << ueVersion << customGame << customDisplay << customEnum << customBase << exePath << lastModified;
        }

        out << m_ueVersionCache;

        if (binFile.commit()) {
            qDebug() << "[Cache] Successfully saved" << count << "games to binary cache:" << binPath;
        } else {
            qWarning() << "[Cache] Failed to commit binary cache file:" << binPath;
        }
    }

    QJsonObject rootObj;
    QJsonArray gamesArr;
    for (const QVariant& var : m_installedGamesList) {
        const QVariantMap game = var.toMap();
        QJsonObject gObj;
        gObj["name"] = game.value("name").toString();
        gObj["path"] = game.value("path").toString();
        gObj["paksPath"] = game.value("paksPath").toString();
        QString ueVer = game.value("ueVersion").toString();
        if (ueVer.isEmpty()) {
            ueVer = getUEVersionForGame(game.value("path").toString());
        }
        if (!ueVer.isEmpty()) {
            gObj["ueVersion"] = ueVer;
        }
        gamesArr.append(gObj);
    }
    rootObj["games"] = gamesArr;
    rootObj["lastScanMs"] = QDateTime::currentMSecsSinceEpoch();
    rootObj["version"] = 2;

    QSaveFile jsonFile(jsonPath);
    if (jsonFile.open(QIODevice::WriteOnly)) {
        jsonFile.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
        jsonFile.commit();
    }

    QString legacyRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (legacyRoot.contains("UniversalManager", Qt::CaseInsensitive)) {
        legacyRoot.replace("UniversalManager", "Universal Mod Manager", Qt::CaseInsensitive);
        const QString oldLegacyCache = legacyRoot + "/Cache";
        if (oldLegacyCache != basePath && QDir(oldLegacyCache).exists()) {
            QDir(oldLegacyCache).removeRecursively();
            QDir(legacyRoot).rmdir("Cache");

            QDir(legacyRoot).rmdir(".");
        }
    }

    return true;
}

bool ModManager::loadInstalledGamesCache() {
    const QString binPath = getInstalledGamesCachePath();
    QFile file(binPath);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QDataStream in(&file);
        in.setVersion(QDataStream::Qt_6_5);

        quint32 magic = 0;
        quint32 version = 0;
        qint64 timestamp = 0;
        in >> magic;
        in >> version;
        in >> timestamp;

        if (magic == 0x554D4D47 && version == 2) {
            quint32 count = 0;
            in >> count;

            QVariantList games;
            games.reserve(count);

            for (quint32 i = 0; i < count; ++i) {
                QString name, path, paksPath, ueVersion, customDisplay, customEnum, customBase, exePath;
                bool customGame = false;
                qint64 lastModified = 0;

                in >> name >> path >> paksPath >> ueVersion >> customGame >> customDisplay >> customEnum >> customBase >> exePath >> lastModified;

                if (!paksPath.isEmpty() && !QDir(paksPath).exists() && !QDir(path).exists()) {
                    continue;
                }

                QVariantMap g;
                g["name"] = name;
                g["path"] = path;
                g["paksPath"] = paksPath;
                if (!ueVersion.isEmpty()) {
                    g["ueVersion"] = ueVersion;
                    m_ueVersionCache[normalizeUEVersionCacheKey(path)] = ueVersion;
                }
                g["customGame"] = customGame;
                if (!customDisplay.isEmpty()) g["customGameDisplayName"] = customDisplay;
                if (!customEnum.isEmpty()) g["customGameEnum"] = customEnum;
                if (!customBase.isEmpty()) g["customGameBaseVersion"] = customBase;
                if (!exePath.isEmpty()) g["exePath"] = exePath;
                if (lastModified > 0) g["lastModified"] = lastModified;

                games.append(g);
            }

            if (!in.atEnd()) {
                QMap<QString, QString> extraCache;
                in >> extraCache;
                for (auto it = extraCache.constBegin(); it != extraCache.constEnd(); ++it) {
                    if (!it.value().isEmpty() && !m_ueVersionCache.contains(it.key())) {
                        m_ueVersionCache.insert(it.key(), it.value());
                    }
                }
            }

            if (!games.isEmpty()) {
                m_installedGamesList = games;
                emit installedGamesListChanged();
                qDebug() << "[Cache] Loaded" << games.size() << "games from binary cache (" << (QDateTime::currentMSecsSinceEpoch() - timestamp) / 1000 << "s old)";
                return true;
            }
        }
        file.close();
    }

    QStringList jsonPathsToTry;
    jsonPathsToTry << getInstalledGamesLegacyJsonCachePath();
    QString oldLegacyJson = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (oldLegacyJson.contains("UniversalManager", Qt::CaseInsensitive)) {
        oldLegacyJson.replace("UniversalManager", "Universal Mod Manager", Qt::CaseInsensitive);
        oldLegacyJson += "/Cache/InstalledGames.json";
        jsonPathsToTry << oldLegacyJson;
    }

    for (const QString& jsonCandidate : jsonPathsToTry) {
        QFile jsonFile(jsonCandidate);
        if (jsonFile.exists() && jsonFile.open(QIODevice::ReadOnly)) {
            QJsonParseError parseErr;
            const QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll(), &parseErr);
            jsonFile.close();
            if (parseErr.error == QJsonParseError::NoError && doc.isObject()) {
                const QJsonArray arr = doc.object().value("games").toArray();
                QVariantList migratedGames;
                for (const QJsonValue& val : arr) {
                    if (val.isObject()) {
                        const QJsonObject obj = val.toObject();
                        const QString path = obj.value("path").toString();
                        const QString paksPath = obj.value("paksPath").toString();
                        if (!paksPath.isEmpty() && !QDir(paksPath).exists() && !QDir(path).exists()) {
                            continue;
                        }
                        QVariantMap g;
                        g["name"] = obj.value("name").toString();
                        g["path"] = path;
                        g["paksPath"] = paksPath;
                        const QString ueVer = obj.value("ueVersion").toString();
                        if (!ueVer.isEmpty()) {
                            g["ueVersion"] = ueVer;
                            m_ueVersionCache[normalizeUEVersionCacheKey(path)] = ueVer;
                        }
                        migratedGames.append(g);
                    }
                }
                if (!migratedGames.isEmpty()) {
                    m_installedGamesList = migratedGames;
                    emit installedGamesListChanged();
                    qDebug() << "[Cache] Loaded" << migratedGames.size() << "games from JSON cache:" << jsonCandidate;
                    saveInstalledGamesCache();
                    return true;
                }
            }
        }
    }

    return false;
}

void ModManager::clearInstalledGamesCache() {
    const QString binPath = getInstalledGamesCachePath();
    if (QFile::exists(binPath)) {
        QFile::remove(binPath);
    }
    const QString jsonPath = getInstalledGamesLegacyJsonCachePath();
    if (QFile::exists(jsonPath)) {
        QFile::remove(jsonPath);
    }

    QString oldLegacyJson = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (oldLegacyJson.contains("UniversalManager", Qt::CaseInsensitive)) {
        oldLegacyJson.replace("UniversalManager", "Universal Mod Manager", Qt::CaseInsensitive);
        const QString oldLegacyCache = oldLegacyJson + "/Cache";
        if (QDir(oldLegacyCache).exists()) {
            QDir(oldLegacyCache).removeRecursively();
        }
    }

    m_installedGamesList.clear();
    m_ueVersionCache.clear();
    emit installedGamesListChanged();
    qDebug() << "[Cache] Installed games cache cleared";
}

void ModManager::autoDetectSevenZipPath() {
    const QStringList possiblePaths = {
        "C:/Program Files/7-Zip/7z.exe",
        "C:/Program Files (x86)/7-Zip/7z.exe",
        "C:/Program Files/WinRAR/WinRAR.exe",
        "C:/Program Files (x86)/WinRAR/WinRAR.exe"
    };
    const QString detectedPath = resolveConfiguredOrKnownToolPath(QString(), possiblePaths);
    if (!detectedPath.isEmpty()) {
        setSevenZipPath(detectedPath);
        emit scanFinished(true);
        return;
    }

    setSevenZipPath("");
    emit scanFinished(false);
}

QString ModManager::resolveExistingSevenZipPath() const {
    return resolveConfiguredOrKnownToolPath(m_sevenZipPath, {});
}

QVariantList ModManager::installedGamesList() const {
    return m_installedGamesList;
}

bool ModManager::isGameScanRunning() const {
    return m_isGameScanRunning;
}

QStringList ModManager::gameScanErrors() const {
    return m_gameScanErrors;
}

QStringList ModManager::availableDrives() const {
    return m_availableDrives;
}

QStringList ModManager::readSteamLibraryPaths() const {
    QStringList paths;
    QString steamPath;

    QSettings regUser("HKEY_CURRENT_USER\\Software\\Valve\\Steam", QSettings::NativeFormat);
    steamPath = regUser.value("SteamPath").toString();
    if (steamPath.isEmpty()) {
        steamPath = regUser.value("InstallPath").toString();
    }

    if (steamPath.isEmpty()) {
        QSettings regMachine("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam", QSettings::NativeFormat);
        steamPath = regMachine.value("InstallPath").toString();
    }

    if (!steamPath.isEmpty() && QDir(steamPath).exists()) {
        paths.append(steamPath);
        const QString vdfPath = QDir(steamPath).filePath("steamapps/libraryfolders.vdf");
        QFile vdfFile(vdfPath);
        if (vdfFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString content = QString::fromUtf8(vdfFile.readAll());
            QRegularExpression rx("\"path\"\\s*\"([^\"]+)\"");
            QRegularExpressionMatchIterator it = rx.globalMatch(content);
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                QString path = match.captured(1);
                path.replace("\\\\", "\\");
                if (!path.isEmpty() && QDir(path).exists()) {
                    paths.append(path);
                }
            }
        }
    }

    paths.removeDuplicates();
    return paths;
}

void ModManager::findUeGamesInRoot(const QString& rootPath, QVariantList& out, QSet<QString>& seen) const {
    QDir root(rootPath);
    if (!root.exists()) {
        return;
    }

    QFileInfoList entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        const QString gameDir = entry.absoluteFilePath();
        const QVariantMap game = detectUeGame(gameDir);
        if (game.isEmpty()) {
            continue;
        }

        const QString canonical = canonicalOrCleanPath(game.value("path").toString());
        if (seen.contains(canonical)) {
            continue;
        }

        seen.insert(canonical);
        out.append(game);
    }
}

QVariantMap ModManager::detectUeGame(const QString& gameDir) const {
    const QString gameName = QFileInfo(gameDir).fileName();
    if (gameName.compare("CrashReportClient", Qt::CaseInsensitive) == 0 ||
        gameDir.contains("Engine/Programs/CrashReportClient", Qt::CaseInsensitive)) {
        return QVariantMap();
    }

    const QString paksPath = QDir(gameDir).filePath("Content/Paks");
    if (directoryHasPakFilesFast(paksPath)) {
        return makeDetectedGame(gameName, gameDir, paksPath);
    }

    const QString directPaksPath = QDir(gameDir).filePath("Paks");
    if (directoryHasPakFilesFast(directPaksPath)) {
        return makeDetectedGame(gameName, gameDir, directPaksPath);
    }

    QDir rootDir(gameDir);
    const QFileInfoList firstLevelEntries = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& firstLevelEntry : firstLevelEntries) {
        const QString nestedGameDir = firstLevelEntry.absoluteFilePath();
        const QString nestedName = firstLevelEntry.fileName();
        if (nestedName.compare("Engine", Qt::CaseInsensitive) == 0 ||
            nestedName.compare("FeaturePacks", Qt::CaseInsensitive) == 0 ||
            nestedName.compare("Samples", Qt::CaseInsensitive) == 0) {
            continue;
        }

        const QString nestedPaks = QDir(nestedGameDir).filePath("Content/Paks");
        if (directoryHasPakFilesFast(nestedPaks)) {
            return makeDetectedGame(gameName, nestedGameDir, nestedPaks);
        }

        QDir nestedDir(nestedGameDir);
        const QFileInfoList secondLevelEntries = nestedDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& secondLevelEntry : secondLevelEntries) {
            const QString secondLevelGameDir = secondLevelEntry.absoluteFilePath();
            const QString secondLevelPaks = QDir(secondLevelGameDir).filePath("Content/Paks");
            if (directoryHasPakFilesFast(secondLevelPaks)) {
                return makeDetectedGame(gameName, secondLevelGameDir, secondLevelPaks);
            }
        }
    }

    return QVariantMap();
}

QString ModManager::resolveModsFolderBase(const QVariantMap& game) const {
    const QString gamePath = game.value("path").toString();
    const QString paksPath = game.value("paksPath").toString();
    if (!paksPath.isEmpty() && QDir(paksPath).exists()) {
        return paksPath;
    }

    const QString fallbackPaks = QDir(gamePath).filePath("Content/Paks");
    if (QDir(fallbackPaks).exists()) {
        return fallbackPaks;
    }

    return gamePath;
}

void ModManager::scanInstalledGames() {
    if (m_isGameScanRunning) {
        return;
    }

    m_isGameScanRunning = true;
    emit isGameScanRunningChanged();

    m_installedGamesList.clear();
    m_gameScanErrors.clear();
    emit installedGamesListChanged();
    emit gameScanErrorsChanged();

    m_gameScanCancelRequested.store(false);
    QStringList driveRoots;
    QFileInfoList drives = QDir::drives();
    for (const QFileInfo& drive : drives) {
        driveRoots.append(drive.absoluteFilePath());
    }
    (void)QtConcurrent::run([this, driveRoots]() {
        scanInstalledGamesInternal(driveRoots);
    });
}

void ModManager::scanInstalledGamesForDrives(const QStringList& drives) {
    if (m_isGameScanRunning) {
        return;
    }

    m_isGameScanRunning = true;
    emit isGameScanRunningChanged();

    m_installedGamesList.clear();
    m_gameScanErrors.clear();
    emit installedGamesListChanged();
    emit gameScanErrorsChanged();

    m_gameScanCancelRequested.store(false);
    QStringList driveRoots;
    for (const QString& drive : drives) {
        if (!drive.isEmpty() && QDir(drive).exists()) {
            driveRoots.append(QDir::cleanPath(drive) + "/");
        }
    }
    (void)QtConcurrent::run([this, driveRoots]() {
        scanInstalledGamesInternal(driveRoots);
    });
}

void ModManager::cancelGameScan() {
    m_gameScanCancelRequested.store(true);
}

void ModManager::scanInstalledGamesInternal(const QStringList& driveRoots) {
    QVariantList results;
    QSet<QString> seen;
    QMutex resultsMutex;
    QStringList preScannedRoots;

    QStringList normalizedDrives;
    for (const QString& drive : driveRoots) {
        if (drive.isEmpty()) continue;
        normalizedDrives.append(normalizeScanPath(drive));
    }

    auto isAllowedPath = [&](const QString& path) {
        if (normalizedDrives.isEmpty()) return true;
        const QString cleaned = normalizeScanPath(path);
        for (const QString& root : normalizedDrives) {
            if (cleaned.startsWith(root)) return true;
        }
        return false;
    };

    auto addIfDetected = [&](const QString& candidatePath) {
        if (m_gameScanCancelRequested.load()) return;
        if (candidatePath.isEmpty() || !isAllowedPath(candidatePath)) return;

        const QVariantMap game = detectUeGame(candidatePath);
        if (game.isEmpty()) return;

        const QString canonical = canonicalOrCleanPath(game.value("path").toString());
        QMutexLocker locker(&resultsMutex);
        if (seen.contains(canonical)) return;

        seen.insert(canonical);
        results.append(game);
    };

    auto addKnownRoot = [&](const QString& rootPath) {
        if (m_gameScanCancelRequested.load()) return;
        if (rootPath.isEmpty() || !QDir(rootPath).exists() || !isAllowedPath(rootPath)) return;

        QMutexLocker locker(&resultsMutex);
        findUeGamesInRoot(rootPath, results, seen);
        preScannedRoots.append(normalizeScanPath(rootPath));
    };

    QStringList steamLibraries = readSteamLibraryPaths();
    for (const QString& steamPath : steamLibraries) {
        if (m_gameScanCancelRequested.load()) break;
        if (!isAllowedPath(steamPath)) continue;
        const QString commonPath = QDir(steamPath).filePath("steamapps/common");
        addKnownRoot(commonPath);
    }

    for (const QString& driveRoot : normalizedDrives) {
        const QString drivePath = driveRoot.left(2);
        addKnownRoot(drivePath + "/Program Files/Epic Games");
        addKnownRoot(drivePath + "/Program Files (x86)/Epic Games");
        addKnownRoot(drivePath + "/GOG Games");
        addKnownRoot(drivePath + "/XboxGames");
    }

#ifdef Q_OS_WIN
    const QStringList epicInstallPaths = readEpicLauncherInstallPaths();
    for (const QString& installPath : epicInstallPaths) {
        if (m_gameScanCancelRequested.load()) break;
        addIfDetected(installPath);
    }

    const QStringList registryInstallPaths = readWindowsInstallLocations();
    for (const QString& installPath : registryInstallPaths) {
        if (m_gameScanCancelRequested.load()) return;
        addIfDetected(installPath);
    }
#endif

    QStringList normalizedPreScannedRoots;
    {
        QMutexLocker locker(&resultsMutex);
        for (const QString& root : preScannedRoots) {
            normalizedPreScannedRoots.append(normalizeScanPath(root));
        }
    }

#ifdef Q_OS_WIN
    std::function<void(const QString&, int)> scanRecursive;
    scanRecursive = [&](const QString& path, int currentDepth) {
        if (m_gameScanCancelRequested.load()) return;
        if (currentDepth > 8) return;

        if (currentDepth <= 2) {
            const QString normalizedPath = normalizeScanPath(path);
            for (const QString& preScannedRoot : normalizedPreScannedRoots) {
                if (normalizedPath == preScannedRoot || normalizedPath.startsWith(preScannedRoot)) {
                    return;
                }
            }
        }

        const QString searchPattern = QDir::toNativeSeparators(QDir::cleanPath(path)) + "\\*";
        WIN32_FIND_DATAW findData;
        HANDLE handle = FindFirstFileExW(
            reinterpret_cast<LPCWSTR>(searchPattern.utf16()),
            FindExInfoBasic,
            &findData,
            FindExSearchLimitToDirectories,
            nullptr,
            FIND_FIRST_EX_LARGE_FETCH);
        if (handle == INVALID_HANDLE_VALUE) {
            return;
        }

        do {
            if (m_gameScanCancelRequested.load()) {
                FindClose(handle);
                return;
            }

            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                continue;
            }
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                continue;
            }

            const QString name = QString::fromWCharArray(findData.cFileName);
            if (name == "." || name == ".." || shouldSkipDeepScanDirectoryName(name)) {
                continue;
            }

            const QString fullPath = QDir(path).filePath(name);

            if (name.compare("Paks", Qt::CaseInsensitive) == 0 &&
                QDir(path).dirName().compare("Content", Qt::CaseInsensitive) == 0 &&
                directoryHasPakFilesFast(fullPath)) {
                QDir gameRoot(path);
                gameRoot.cdUp();
                addIfDetected(gameRoot.absolutePath());
                continue;
            }

            scanRecursive(fullPath, currentDepth + 1);
        } while (FindNextFileW(handle, &findData));

        FindClose(handle);
    };
#else
    std::function<void(const QString&, int)> scanRecursive;
    scanRecursive = [&](const QString& path, int currentDepth) {
        if (m_gameScanCancelRequested.load()) return;
        if (currentDepth > 8) return;

        QDir dir(path);
        if (!dir.exists() || !dir.isReadable()) return;

        const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::NoSymLinks);
        for (const QFileInfo& entry : entries) {
            if (m_gameScanCancelRequested.load()) return;

            const QString name = entry.fileName();
            if (shouldSkipDeepScanDirectoryName(name)) {
                continue;
            }

            const QString fullPath = entry.absoluteFilePath();
            if (name.compare("Paks", Qt::CaseInsensitive) == 0 &&
                QDir(path).dirName().compare("Content", Qt::CaseInsensitive) == 0 &&
                directoryHasPakFilesFast(fullPath)) {
                QDir gameRoot(path);
                gameRoot.cdUp();
                addIfDetected(gameRoot.absolutePath());
                continue;
            }

            scanRecursive(fullPath, currentDepth + 1);
        }
    };
#endif

    const QStringList drivesToScan = normalizedDrives.isEmpty() ? m_availableDrives : normalizedDrives;
    QList<QFuture<void>> scanFutures;
    for (const QString& drive : drivesToScan) {
        if (m_gameScanCancelRequested.load()) break;
        scanFutures.append(QtConcurrent::run([&scanRecursive, drive]() {
            scanRecursive(drive, 0);
        }));
    }
    for (QFuture<void>& future : scanFutures) {
        future.waitForFinished();
    }

    QMap<QString, QVariantMap> byPaksPath;
    for (const QVariant& v : results) {
        QVariantMap g = v.toMap();
        QString paksPath = g.value("paksPath").toString();
        if (paksPath.isEmpty()) continue;

        QString canonicalPaks = QFileInfo(paksPath).canonicalFilePath();
        if (canonicalPaks.isEmpty()) canonicalPaks = QFileInfo(paksPath).absoluteFilePath();

        canonicalPaks = QDir::cleanPath(canonicalPaks);
#ifdef Q_OS_WIN
        canonicalPaks = canonicalPaks.toLower();
#endif

        if (!byPaksPath.contains(canonicalPaks)) {
            byPaksPath.insert(canonicalPaks, g);
        } else {
            QVariantMap existing = byPaksPath.value(canonicalPaks);
            QString existingPath = existing.value("path").toString();
            QString currentPath = g.value("path").toString();

            if (currentPath.length() < existingPath.length()) {
                byPaksPath[canonicalPaks] = g;
            }
        }
    }

    QVariantList finalResults;
    for (auto it = byPaksPath.constBegin(); it != byPaksPath.constEnd(); ++it) {
        finalResults.append(it.value());
    }

    QVariantList filteredResults;
    for (int i = 0; i < finalResults.size(); ++i) {
        QVariantMap gameA = finalResults[i].toMap();
        QString pathA = QDir::cleanPath(gameA["path"].toString());
#ifdef Q_OS_WIN
        pathA = pathA.toLower();
#endif
        QString nameA = gameA["name"].toString();

        bool isNestedDuplicate = false;
        for (int j = 0; j < finalResults.size(); ++j) {
            if (i == j) continue;

            QVariantMap gameB = finalResults[j].toMap();
            QString pathB = QDir::cleanPath(gameB["path"].toString());
#ifdef Q_OS_WIN
            pathB = pathB.toLower();
#endif
            QString nameB = gameB["name"].toString();

            if (nameA.compare(nameB, Qt::CaseInsensitive) == 0) {

                 if (pathA.length() > pathB.length() && pathA.startsWith(pathB)) {

                     if (pathA[pathB.length()] == '/' || pathA[pathB.length()] == '\\') {
                         isNestedDuplicate = true;
                         break;
                     }
                 }
            }
        }

        if (!isNestedDuplicate) {
            filteredResults.append(gameA);
        }
    }

    const bool canceled = m_gameScanCancelRequested.load();
    QMetaObject::invokeMethod(this, [this, filteredResults, canceled]() {

        m_installedGamesList = filteredResults;
        emit installedGamesListChanged();
        saveInstalledGamesCache();

        m_isGameScanRunning = false;
        emit isGameScanRunningChanged();

        auto firePostDetection = [this, filteredResults, canceled]() {
            saveInstalledGamesCache();

            qDebug() << "[Scan] ---- Installed games summary (" << filteredResults.size() << ") ----";

            struct ScanEntry {
                QString name;
                QString path;
                QString ueVer;
                QString customEnum;
            };
            QList<ScanEntry> customEntries;
            QList<ScanEntry> otherEntries;
            customEntries.reserve(filteredResults.size());
            otherEntries.reserve(filteredResults.size());

            for (const QVariant& v : std::as_const(filteredResults)) {
                const QVariantMap m = v.toMap();
                ScanEntry e;
                e.name = m.value("name").toString();
                e.path = m.value("path").toString();
                e.ueVer = getUEVersionForGame(e.path);
                e.customEnum = isCustomGameDetectedForGame(e.path)
                                    ? getCustomGameForGame(e.path)
                                    : QString();
                if (e.customEnum.isEmpty())
                    otherEntries.append(e);
                else
                    customEntries.append(e);
            }

            const auto sortByName = [](const ScanEntry& a, const ScanEntry& b) {
                return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
            };
            std::sort(customEntries.begin(), customEntries.end(), sortByName);
            std::sort(otherEntries.begin(), otherEntries.end(), sortByName);

            const auto printOne = [this](const ScanEntry& e) {
                const QString customLine = e.customEnum.isEmpty()
                    ? QString()
                    : QStringLiteral(" * Custom: %1 [%2]").arg(getCustomGameDisplayNameForGame(e.path), e.customEnum);
                qDebug().noquote()
                    << QStringLiteral("  * %1  engine=%2%3")
                           .arg(e.name.isEmpty() ? QStringLiteral("?") : e.name,
                                e.ueVer.isEmpty() ? QStringLiteral("<unknown>") : e.ueVer,
                                customLine);
            };

            if (!customEntries.isEmpty()) {
                qDebug() << "[Scan] -- Custom games (" << customEntries.size() << ") --";
                for (const ScanEntry& e : std::as_const(customEntries)) {
                    printOne(e);
                }
            }
            if (!otherEntries.isEmpty()) {
                qDebug() << "[Scan] -- Other games (" << otherEntries.size() << ") --";
                for (const ScanEntry& e : std::as_const(otherEntries)) {
                    printOne(e);
                }
            }
            qDebug() << "[Scan] -----------------------------------------------------";
            if (canceled) {
                emit gameScanActionMessage("Scan canceled", false);
            }
        };

        connect(this, &ModManager::ueVersionsDetected,
                this, firePostDetection, Qt::SingleShotConnection);

        detectUEVersionsForAllGames();
    });
}

void ModManager::createModsFoldersForGames(const QVariantList& games, const QString& folderType) {
    const QString normalized = normalizeModsFolderType(folderType);

    QStringList failures;
    QStringList created;
    QStringList existing;
    QStringList cDriveFailures;
    bool hadCaseOnlyMismatch = false;
    for (const QVariant& gameVar : games) {
        QVariantMap game = gameVar.toMap();
        QString basePath = resolveModsFolderBase(game);
        if (basePath.isEmpty()) {
            failures.append(game.value("name").toString());
            continue;
        }
        const QString target = QDir(basePath).filePath(normalized);
        if (QDir(target).exists()) {
#ifdef Q_OS_WIN
            {
                QDir baseDir(basePath);
                const QStringList dirNames = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QString& dirName : dirNames) {
                    if (dirName.compare(normalized, Qt::CaseInsensitive) == 0 && dirName != normalized) {
                        hadCaseOnlyMismatch = true;
                        break;
                    }
                }
            }
#endif
            existing.append(game.value("name").toString());
            continue;
        }
        QDir dir;
        if (dir.mkpath(target)) {
            created.append(game.value("name").toString());
        } else if (pathIsOnCDrive(target)) {
            cDriveFailures.append(game.value("name").toString());
        } else {
            failures.append(game.value("name").toString());
        }
    }

    QStringList parts;
    if (!created.isEmpty()) {
        parts.append("Created Mods folder for: " + created.join(", "));
    }
    if (!existing.isEmpty()) {
        parts.append("Mods folder already exists for: " + existing.join(", "));
    }
    if (hadCaseOnlyMismatch) {

        parts.append("[note] Note: Windows ignores folder name casing (Mods/mods and ~Mods/~mods and LogicMods/logicmods are the same)");
    }
    if (!failures.isEmpty()) {
        parts.append("Failed for: " + failures.join(", "));
    }
    if (cDriveFailures.size() == 1) {
        parts.append("[note] " + cDriveFailures.first() + " is on the C: drive. Run Universal Manager as administrator to create its mods folder.");
    } else if (cDriveFailures.size() > 1) {
        parts.append(QString("%1 games on the C drive need administrator permissions to create their mods folders: ").arg(cDriveFailures.size())
                     + cDriveFailures.join(", "));
    }
    if (parts.isEmpty()) {
        parts.append("No Game(s) Selected");
    }

    emit gameScanActionMessage(parts.join("\n"), failures.isEmpty() && cDriveFailures.isEmpty());
}

void ModManager::openModsFoldersForGames(const QVariantList& games, const QString& folderType) {
    const QString normalized = normalizeModsFolderType(folderType);

    QStringList failures;
    for (const QVariant& gameVar : games) {
        QVariantMap game = gameVar.toMap();
        QString basePath = resolveModsFolderBase(game);
        if (basePath.isEmpty()) {
            failures.append(game.value("name").toString());
            continue;
        }
        QString target;
        if (!normalized.isEmpty()) {
            const QString requested = QDir(basePath).filePath(normalized);
            if (QDir(requested).exists()) {
                target = requested;
            }
        }

        if (target.isEmpty()) {
            const QStringList candidates = {"mods", "~mods", "LogicMods", "~Mods", "Mods"};
            for (const QString& candidate : candidates) {
                const QString path = QDir(basePath).filePath(candidate);
                if (QDir(path).exists()) {
                    target = path;
                    break;
                }
            }
        }

        if (target.isEmpty()) {
            failures.append(game.value("name").toString());
            continue;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(target));
    }

    if (failures.isEmpty()) {
        emit gameScanActionMessage("Opened Mods folders", true);
    } else {
        emit gameScanActionMessage("Missing Mods folder for: " + failures.join(", "), false);
    }
}

void ModManager::deleteModsFoldersForGames(const QVariantList& games, const QString& folderType) {
    const QString normalized = normalizeModsFolderType(folderType);

    bool rescanActiveMods = false;
    const QString activeModsRoot = QDir::cleanPath(m_modsPathInternal);
    const auto removalAffectsActiveRoot = [&](const QString& removedPath) {
        if (activeModsRoot.isEmpty())
            return false;
        const QString removed = QDir::cleanPath(removedPath);
        if (removed.isEmpty())
            return false;
        return activeModsRoot.compare(removed, Qt::CaseInsensitive) == 0
            || activeModsRoot.startsWith(removed + '/', Qt::CaseInsensitive)
            || removed.startsWith(activeModsRoot + '/', Qt::CaseInsensitive);
    };

    QStringList deleted;
    QStringList failures;
    QStringList notFound;
    QStringList cDriveFailures;
    for (const QVariant& gameVar : games) {
        QVariantMap game = gameVar.toMap();
        QString basePath = resolveModsFolderBase(game);
        if (basePath.isEmpty()) {
            failures.append(game.value("name").toString());
            continue;
        }

        const QString explicitModsFolderPath = game.value("modsFolderPath").toString();
        if (!explicitModsFolderPath.isEmpty() && QDir(explicitModsFolderPath).exists()) {
            bool ok = false;
            if (m_deleteToRecycleBin) {
                ok = QFile::moveToTrash(explicitModsFolderPath);
            } else {
                ok = QDir(explicitModsFolderPath).removeRecursively();
            }

            if (ok) {
                deleted.append(game.value("name").toString());
                if (removalAffectsActiveRoot(explicitModsFolderPath))
                    rescanActiveMods = true;
            } else if (pathIsOnCDrive(explicitModsFolderPath)) {
                cDriveFailures.append(game.value("name").toString());
            } else {
                failures.append(game.value("name").toString());
            }
            continue;
        }

        QString target;
        const QString requested = QDir(basePath).filePath(normalized);
        if (QDir(requested).exists()) {
            target = requested;
        }

        if (target.isEmpty()) {
            const QStringList candidates = {"mods", "~mods", "LogicMods", "~Mods", "Mods"};
            for (const QString& candidate : candidates) {
                const QString path = QDir(basePath).filePath(candidate);
                if (QDir(path).exists()) {
                    target = path;
                    break;
                }
            }
        }

        if (target.isEmpty()) {
            notFound.append(game.value("name").toString());
            continue;
        }

        bool ok = false;
        if (m_deleteToRecycleBin) {
            ok = QFile::moveToTrash(target);
        } else {
            ok = QDir(target).removeRecursively();
        }

        if (ok) {
            deleted.append(game.value("name").toString());
            if (removalAffectsActiveRoot(target))
                rescanActiveMods = true;
        } else if (pathIsOnCDrive(target)) {
            cDriveFailures.append(game.value("name").toString());
        } else {
            failures.append(game.value("name").toString());
        }
    }

    if (rescanActiveMods) {
        scanMods(m_modsPathInternal);
    }

    QStringList parts;
    if (!deleted.isEmpty()) {
        parts.append("Deleted Mods folder for: " + deleted.join(", "));
    }
    if (!notFound.isEmpty()) {
        parts.append("No Mods folder found for: " + notFound.join(", "));
    }
    if (!failures.isEmpty()) {
        parts.append("Failed to delete for: " + failures.join(", "));
    }
    if (cDriveFailures.size() == 1) {
        parts.append("[note] " + cDriveFailures.first() + " is on the C: drive. Run Universal Manager as administrator to delete its mods folder.");
    } else if (cDriveFailures.size() > 1) {
        parts.append(QString("%1 games on the C drive need administrator permissions to delete their mods folders: ").arg(cDriveFailures.size())
                     + cDriveFailures.join(", "));
    }
    if (parts.isEmpty()) {
        parts.append("No Game(s) Selected");
    }
    emit gameScanActionMessage(parts.join("\n"), failures.isEmpty() && cDriveFailures.isEmpty() && !deleted.isEmpty());
}

QVariantList ModManager::getModsFoldersForGame(const QVariantMap& game) const {
    QVariantList result;
    const QString basePath = resolveModsFolderBase(game);
    if (basePath.isEmpty()) {
        return result;
    }
    QDir baseDir(basePath);
    const QStringList dirNames = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    struct FoundFolder {
        QString name;
        QString path;
        int count = 0;
        int order = 0;
    };

    const QHash<QString, int> orderByKey = {
        {"~mods", 0},
        {"mods", 1},
        {"logicmods", 2},
    };

    QVector<FoundFolder> found;
    found.reserve(dirNames.size());
    for (const QString& dirName : dirNames) {
        const QString key = dirName.toLower();
        if (!orderByKey.contains(key)) {
            continue;
        }

        const QString path = baseDir.filePath(dirName);
        FoundFolder f;
        f.name = dirName;
        f.path = QDir::cleanPath(path);
        f.count = countDisplayModsInPath(path);
        f.order = orderByKey.value(key, 999);
        found.push_back(std::move(f));
    }

    std::sort(found.begin(), found.end(), [](const FoundFolder& a, const FoundFolder& b) {
        if (a.order != b.order) return a.order < b.order;
        return a.name.toLower() < b.name.toLower();
    });

    for (const FoundFolder& f : found) {
        QVariantMap item;
        item["name"] = f.name;
        item["path"] = f.path;
        item["count"] = f.count;
        result.append(item);
    }
    return result;
}

QVariantList ModManager::getAvailableModsFoldersForCurrentPreset() const {
    QVariantList result;
    if (m_activePresetId.isEmpty()) {
        return result;
    }

    const int presetIndex = findPresetIndexById(m_activePresetId);
    if (presetIndex < 0) {
        return result;
    }

    const QVariantMap preset = m_gamePresets[presetIndex].toMap();
    QVariantMap game;
    game["path"] = preset.value("gamePath").toString();
    game["paksPath"] = preset.value("gamePaksPath").toString();
    game["modsFolderPath"] = preset.value("modsPath").toString();

    result = getModsFoldersForGame(game);

    auto normalizePathForCompare = [](const QString& maybeUrlOrPath) {
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
    };

    QString currentPath = preset.value("modsPath").toString();
    if (currentPath.isEmpty()) {
        currentPath = m_modsPath;
    }

    const QString normalizedCurrentPath = normalizePathForCompare(currentPath);
    bool foundCurrent = false;

    for (int i = 0; i < result.size(); ++i) {
        QVariantMap item = result[i].toMap();
        const bool isCurrent = normalizePathForCompare(item.value("path").toString()) == normalizedCurrentPath;
        item["current"] = isCurrent;
        if (isCurrent) {
            foundCurrent = true;
        }
        result[i] = item;
    }

    if (!normalizedCurrentPath.isEmpty() && !foundCurrent) {
        QVariantMap currentItem;
        currentItem["name"] = QFileInfo(currentPath).fileName();
        currentItem["path"] = QDir::cleanPath(currentPath);
        currentItem["count"] = QDir(currentPath).exists() ? countDisplayModsInPath(currentPath) : 0;
        currentItem["current"] = true;
        result.prepend(currentItem);
    }

    return result;
}

static QString modsFolderBaseForPreset(const QVariantMap& preset, const QString& activePaksPath)
{
    QString base = preset.value("gamePaksPath").toString();
    if (base.isEmpty() || !QDir(base).exists()) {
        base = activePaksPath;
    }
    if (base.isEmpty() || !QDir(base).exists()) {
        base = QDir(preset.value("gamePath").toString()).filePath("Content/Paks");
    }
    if (!QDir(base).exists()) {
        return QString();
    }
    return QDir::cleanPath(base);
}

QVariantMap ModManager::checkModsFolderForCurrentPreset(const QString& folderType) const {
    QVariantMap result;
    result["status"] = "error";
    result["basePath"] = "";
    result["path"] = "";
    result["existingName"] = "";
    result["currentPath"] = "";
    result["currentExists"] = false;
    result["message"] = "";

    if (m_activePresetId.isEmpty()) {
        result["message"] = "No profile is selected";
        return result;
    }
    const int presetIndex = findPresetIndexById(m_activePresetId);
    if (presetIndex < 0) {
        result["message"] = "No profile is selected";
        return result;
    }

    const QVariantMap preset = m_gamePresets[presetIndex].toMap();
    const QString basePath = modsFolderBaseForPreset(preset, m_gamePaksPath);
    if (basePath.isEmpty()) {
        result["message"] = "The game folder of this profile was not found";
        return result;
    }
    result["basePath"] = basePath;

    const QString currentModsPath = preset.value("modsPath").toString();
    result["currentPath"] = currentModsPath;
    result["currentExists"] = !currentModsPath.isEmpty() && QDir(currentModsPath).exists();

    const QString name = normalizeModsFolderType(folderType);
    const QStringList entries = QDir(basePath).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& entry : entries) {
        if (entry.compare(name, Qt::CaseInsensitive) != 0) {
            continue;
        }
        result["existingName"] = entry;
        result["path"] = QDir(basePath).filePath(entry);
        result["status"] = (entry == name) ? "exists" : "caseConflict";
        return result;
    }

    result["path"] = QDir(basePath).filePath(name);
    result["status"] = "missing";
    return result;
}

QVariantMap ModManager::createModsFolderForCurrentPreset(const QString& folderType) {
    QVariantMap result = checkModsFolderForCurrentPreset(folderType);
    const QString status = result.value("status").toString();
    if (status != "missing") {
        return result;
    }

    const QString path = result.value("path").toString();
    QDir dir;
    if (!dir.mkpath(path)) {
        result["status"] = "error";
        result["message"] = pathIsOnCDrive(path)
            ? QStringLiteral("Folders on the C: drive need administrator rights")
            : QStringLiteral("Could not create ") + path;
        return result;
    }

    result["status"] = "created";
    result["path"] = path;
    return result;
}

bool ModManager::switchCurrentPresetModsFolder(const QString& newModsPath, bool disableAllMods, bool disableAllCollections, const QStringList& modsToDisable, const QStringList& collectionsToDisable) {
    if (m_activePresetId.isEmpty()) {
        return false;
    }

    QString targetPath = QUrl(newModsPath).toLocalFile();
    if (targetPath.isEmpty()) {
        targetPath = newModsPath;
    }
    targetPath = QDir::cleanPath(targetPath);

    if (targetPath.isEmpty() || !QDir(targetPath).exists()) {
        return false;
    }
    const QString sourceFolderKey = normalizePathForCompare(m_modsPath);

    if (normalizePathForCompare(targetPath) == normalizePathForCompare(m_modsPath)) {
        return true;
    }

    const bool wasLoading = m_isLoading;
    m_isLoading = true;
    setModsPath(targetPath);
    m_isLoading = wasLoading;

    scanMods(targetPath);

    const QString targetKey = normalizePathForCompare(targetPath);
    if (!disableAllMods && !disableAllCollections) {
        if (m_modsFolderMassDisabledByPath.value(targetKey).toBool()) {
            const QStringList modsToEnable = m_modsFolderEnabledSnapshotByPath.value(targetKey).toStringList();
            m_modsFolderMassDisabledByPath.remove(targetKey);
            if (!modsToEnable.isEmpty()) {
                toggleModStatusBatch(modsToEnable, true);
            }
        }
    }

    if (disableAllMods || disableAllCollections) {
        QStringList enabledBeforeDisable;
        enabledBeforeDisable.reserve(m_modList.size());
        for (const QVariant& modVar : m_modList) {
            const QVariantMap mod = modVar.toMap();
            if (mod.value("status").toString() != "Disabled") {
                const QString key = mod.value("uniqueId").toString().isEmpty()
                    ? mod.value("name").toString()
                    : mod.value("uniqueId").toString();
                if (!key.isEmpty()) {
                    enabledBeforeDisable.append(key);
                }
            }
        }
        m_modsFolderEnabledSnapshotByPath.insert(targetKey, enabledBeforeDisable);
        m_modsFolderMassDisabledByPath.insert(targetKey, true);
    } else if (!modsToDisable.isEmpty() || !collectionsToDisable.isEmpty()) {

        QStringList enabledBeforeDisable;
        enabledBeforeDisable.reserve(m_modList.size());
        for (const QVariant& modVar : m_modList) {
            const QVariantMap mod = modVar.toMap();
            if (mod.value("status").toString() != "Disabled") {
                const QString key = mod.value("uniqueId").toString().isEmpty()
                    ? mod.value("name").toString()
                    : mod.value("uniqueId").toString();
                if (!key.isEmpty()) {
                    enabledBeforeDisable.append(key);
                }
            }
        }
        m_modsFolderEnabledSnapshotByPath.insert(targetKey, enabledBeforeDisable);
        m_modsFolderMassDisabledByPath.insert(targetKey, true);
    }

    if (disableAllMods && !m_modList.isEmpty()) {
        QStringList modNames;
        modNames.reserve(m_modList.size());
        for (const QVariant& modVar : m_modList) {
            const QVariantMap mod = modVar.toMap();
            const QString modName = mod.value("uniqueId").toString().isEmpty()
                ? mod.value("name").toString()
                : mod.value("uniqueId").toString();
            if (!modName.isEmpty()) {
                modNames.append(modName);
            }
        }

        if (!modNames.isEmpty()) {
            toggleModStatusBatch(modNames, false);
        }
    } else if (disableAllCollections) {
        QSet<QString> collectionModNames;
        for (const QVariant& collectionVar : m_collections) {
            const QVariantMap collection = collectionVar.toMap();
            if (collection.value("presetId").toString() != m_activePresetId) {
                continue;
            }
            const QString collectionFolderKey = normalizePathForCompare(collection.value("modsPath").toString());
            if (!sourceFolderKey.isEmpty() && !collectionFolderKey.isEmpty() && collectionFolderKey != sourceFolderKey) {
                continue;
            }

            const QStringList mods = collection.value("mods").toStringList();
            for (const QString& modName : mods) {
                if (!modName.isEmpty()) {
                    collectionModNames.insert(modName);
                }
            }
        }

        if (!collectionModNames.isEmpty()) {
            toggleModStatusBatch(collectionModNames.values(), false);
        }
    } else {

        QSet<QString> modsToActuallyDisable;
        if (!modsToDisable.isEmpty()) {
            for (const QString& key : modsToDisable) {
                if (!key.isEmpty()) {
                    modsToActuallyDisable.insert(key);
                }
            }
        }

        if (!collectionsToDisable.isEmpty()) {
            const QSet<QString> collectionIdSet(collectionsToDisable.begin(), collectionsToDisable.end());
            for (const QVariant& collectionVar : m_collections) {
                const QVariantMap collection = collectionVar.toMap();
                if (collection.value("presetId").toString() != m_activePresetId) {
                    continue;
                }
                const QString collectionFolderKey = normalizePathForCompare(collection.value("modsPath").toString());
                if (!sourceFolderKey.isEmpty() && !collectionFolderKey.isEmpty() && collectionFolderKey != sourceFolderKey) {
                    continue;
                }
                if (!collectionIdSet.contains(collection.value("id").toString())) {
                    continue;
                }
                const QStringList mods = collection.value("mods").toStringList();
                for (const QString& modName : mods) {
                    if (!modName.isEmpty()) {
                        modsToActuallyDisable.insert(modName);
                    }
                }
            }
        }

        QSet<QString> knownKeys;
        for (const QVariant& modVar : m_modList) {
            const QVariantMap mod = modVar.toMap();
            const QString uniqueId = mod.value("uniqueId").toString();
            const QString name = mod.value("name").toString();
            if (!uniqueId.isEmpty()) knownKeys.insert(uniqueId);
            if (!name.isEmpty()) knownKeys.insert(name);
        }
        QSet<QString> filtered;
        for (const QString& key : modsToActuallyDisable) {
            if (knownKeys.contains(key)) {
                filtered.insert(key);
            }
        }

        if (!filtered.isEmpty()) {
            toggleModStatusBatch(filtered.values(), false);
        }
    }

    updateActivePresetFromCurrentState();
    emit collectionsChanged();
    emit totalModsAllPresetsChanged();
    emit totalActiveModsAllPresetsChanged();
    return true;
}

void ModManager::openGameFolder(const QString& gamePath) {
    if (gamePath.isEmpty()) return;

    QFileInfo info(gamePath);
    QString folderPath;

    if (info.isFile()) {
        folderPath = info.absolutePath();
    } else if (info.isDir()) {
        folderPath = info.absoluteFilePath();
    } else {
        folderPath = gamePath;
    }

#ifdef Q_OS_WIN
    QProcess::startDetached("explorer.exe", {QDir::toNativeSeparators(folderPath)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
#endif
}

void ModManager::copyTextToClipboard(const QString& text) {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(text);
    }
}

bool ModManager::modsFolderExistsForGame(const QVariantMap& game, const QString& folderType) const {
    QString basePath = resolveModsFolderBase(game);
    if (basePath.isEmpty()) {
        return false;
    }
    const QStringList candidates = {"mods", "~mods", "LogicMods", "~Mods", "Mods"};
    for (const QString& candidate : candidates) {
        const QString target = QDir(basePath).filePath(candidate);
        if (QDir(target).exists()) {
            return true;
        }
    }
    return false;
}

void ModManager::addGamesToPresets(const QVariantList& games, const QString& folderType) {
    const QString normalized = normalizeModsFolderType(folderType);

    int added = 0;
    for (const QVariant& gameVar : games) {
        QVariantMap game = gameVar.toMap();
        const QString gamePath = game.value("path").toString();
        if (gamePath.isEmpty()) {
            continue;
        }

        const auto normalizePathForCompare = [](const QString& maybeUrlOrPath) {
            QString p = QUrl(maybeUrlOrPath).toLocalFile();
            if (p.isEmpty()) {
                p = maybeUrlOrPath;
            }
            p = QDir::cleanPath(p);
            p.replace('\\', '/');
#ifdef Q_OS_WIN
            p = p.toLower();
#endif
            return p;
        };

        const QString scannedRoot = normalizePathForCompare(gamePath);

        bool exists = false;
        for (const QVariant& presetVar : m_gamePresets) {
            QVariantMap preset = presetVar.toMap();
            const QString existingPath = normalizePathForCompare(preset.value("gamePath").toString());
            if (existingPath == scannedRoot || existingPath.startsWith(scannedRoot + "/")) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        QVariantMap newPreset;
        newPreset["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        newPreset["name"] = game.value("name").toString().isEmpty() ? QFileInfo(gamePath).fileName() : game.value("name").toString();
        newPreset["previewImage"] = "";
        newPreset["blurBackground"] = false;

        const QString paksPath = game.value("paksPath").toString();
        const QString modsBase = resolveModsFolderBase(game);
        const QString explicitModsPath = game.value("modsFolderPath").toString();
        const QString exePath = findGameExecutableFromFolder(gamePath, newPreset.value("name").toString());
        newPreset["gamePath"] = exePath.isEmpty() ? gamePath : exePath;
        QString finalModsPath;
        if (!explicitModsPath.isEmpty()) {
            finalModsPath = explicitModsPath;
        } else if (!modsBase.isEmpty()) {
            const QString requestedPath = QDir(modsBase).filePath(normalized);
            if (QDir(requestedPath).exists()) {
                finalModsPath = requestedPath;
            } else {
                QStringList candidates;
                candidates << normalized << "~Mods" << "Mods" << "LogicMods" << "~mods" << "mods";
                QSet<QString> seen;
                for (const QString& candidate : candidates) {
                    if (seen.contains(candidate)) {
                        continue;
                    }
                    seen.insert(candidate);
                    const QString path = QDir(modsBase).filePath(candidate);
                    if (QDir(path).exists()) {
                        finalModsPath = path;
                        break;
                    }
                }
            }
        }
        newPreset["modsPath"] = finalModsPath;
        newPreset["downloadsPath"] = "";
        newPreset["backupPath"] = "";
        newPreset["pakViewerPath"] = "";
        newPreset["gamePaksPath"] = paksPath;
        newPreset["usmapFilePath"] = "";
        newPreset["aesKey"] = "";
        QString detectedUe = game.value("ueVersion").toString();

        const QString customGameDisplay = getCustomGameDisplayNameForGame(gamePath);
        const QString customGameEnum = getCustomGameForGame(gamePath);
        const QString customGameBase = getCustomGameBaseVersionForGame(gamePath);
        const QString gameNameForLog = QFileInfo(gamePath).fileName();
        if (!customGameDisplay.isEmpty()) {
            detectedUe = customGameDisplay;
            newPreset["customGame"] = true;
            qDebug().noquote()
                << QString::fromUtf8("[AddToProfile] game=\"%1\" path=\"%2\" -> Custom Game: %3 [%4, base %5]")
                       .arg(gameNameForLog, gamePath, customGameDisplay, customGameEnum,
                            customGameBase.isEmpty() ? QStringLiteral("?") : customGameBase);
        } else if (detectedUe.isEmpty()) {
            detectedUe = getUEVersionForGame(gamePath);
            if (detectedUe.isEmpty()) {
                detectedUe = detectUEVersionForGame(gamePath);
            }
        }
        newPreset["ueVersion"] = detectedUe.isEmpty() ? "UE 5.7" : detectedUe;
        if (customGameDisplay.isEmpty()) {
            newPreset["customGame"] = false;
        }

        newPreset["theme"] = m_theme;
        newPreset["wallpaperPath"] = m_wallpaperPath;
        newPreset["glassBlur"] = m_glassBlur;
        newPreset["glassOpacity"] = m_glassOpacity;
        newPreset["glassDarkness"] = m_glassDarkness;
        newPreset["navBarColor"] = m_navBarColor;
        newPreset["navBarIconColor"] = m_navBarIconColor;
        newPreset["accentColor"] = m_accentColor;
        newPreset["cpuUsageBarColor"] = m_cpuUsageBarColor;
        newPreset["ramUsageBarColor"] = m_ramUsageBarColor;
        newPreset["driveUsageBarColor"] = m_driveUsageBarColor;
        newPreset["idColor"] = m_idColor;
        newPreset["toggleActiveColor"] = m_toggleActiveColor;
        newPreset["toggleDisabledColor"] = m_toggleDisabledColor;
        newPreset["enableButtonColor"] = m_enableButtonColor;
        newPreset["disableButtonColor"] = m_disableButtonColor;
        newPreset["folderModColor"] = m_folderModColor;
        newPreset["looseFileColor"] = m_looseFileColor;
        newPreset["tagColor"] = m_tagColor;
        newPreset["pakColor"] = m_pakColor;
        newPreset["ucasColor"] = m_ucasColor;
        newPreset["utocColor"] = m_utocColor;

        newPreset["modImages"] = QVariantMap();
        newPreset["modTags"] = QVariantMap();
        newPreset["modIds"] = QVariantMap();
        newPreset["favorites"] = QStringList();

        QString luaFolderPath;
        if (m_luaProfileManager) {
            const QString newPresetId = newPreset.value("id").toString();
            const QString newPresetName = newPreset.value("name").toString();
            const QString luaError = m_luaProfileManager->createProfileFolder(newPresetId, newPresetName);
            if (!luaError.isEmpty()) {
                qWarning() << "[LuaProfiles] failed to create folder for preset" << newPresetName << ":" << luaError;
                emit luaProfileCreateFailed(newPresetName, luaError);
            }
            luaFolderPath = m_luaProfileManager->profileFolderPath(newPresetId, newPresetName);
        }
        newPreset["luaModsPath"] = luaFolderPath;
        newPreset["useSharedLuaMods"] = false;

        m_gamePresets.append(newPreset);

        added++;
    }

    if (added > 0) {
        emit gamePresetsChanged();
        emit totalModsAllPresetsChanged();
        savePresets();
    }

    emit gameScanActionMessage(QString("Added %1 Game(s) to Profiles").arg(added), added > 0);
}
