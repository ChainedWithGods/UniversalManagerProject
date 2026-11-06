#include "ModManagerInternal.h"

using namespace mmdetail;

namespace {
struct UEDetectLogEntry {
    QString gamePath;
    QString version;
};

void logUEVersionDetection(const QString& gamePath, const QString& detectedVersion) {
    const QString displayName = QFileInfo(gamePath).fileName();
    qDebug().noquote()
        << QStringLiteral("[UEDetect] game=\"%1\" path=\"%2\" -> %3")
               .arg(displayName.isEmpty() ? QStringLiteral("?") : displayName,
                    gamePath,
                    detectedVersion.isEmpty() ? QStringLiteral("<no version detected>") : detectedVersion);
}

QString detectUEVersionForPathUncached(const QString& ueDetectorPath, const QString& gamePath) {
    QDir gameDir(gamePath);
    QString exePath;
    QString exeCandidateShipping;
    QString exeCandidateLargest;
    qint64 exeLargestSize = -1;
    const QStringList binariesPaths = {
        gameDir.filePath("Binaries/Win64"),
        gameDir.filePath("binaries/win64"),
    };

    for (const QString& binPath : binariesPaths) {
        QDir binDir(binPath);
        if (!binDir.exists()) continue;

        const QStringList exeFiles = binDir.entryList(QStringList() << "*.exe", QDir::Files);
        for (const QString& exe : exeFiles) {
            const QString lowerExe = exe.toLower();
            if (lowerExe.contains("crashreport") ||
                lowerExe.contains("crashreportclient") ||
                lowerExe.contains("crs-") ||
                lowerExe.contains("eac") ||
                lowerExe.contains("eaclauncher") ||
                lowerExe.contains("setup") ||
                lowerExe.contains("install") ||
                lowerExe.contains("unins") ||
                lowerExe.contains("vcredist") ||
                lowerExe.contains("dxsetup") ||
                lowerExe.contains("dotnet") ||
                lowerExe.contains("unitycrashhandler") ||
                lowerExe.contains("steam") ||
                lowerExe.contains("steamwebhelper") ||
                lowerExe.contains("steamservice") ||
                lowerExe.contains("steamerrorreporter") ||
                lowerExe.contains("gameoverlayui") ||
                lowerExe.contains("epicwebhelper") ||
                lowerExe.contains("unrealcefsubprocess") ||
                lowerExe.contains("easyanticheat") ||
                lowerExe.contains("eosoverlay") ||
                lowerExe.contains("eosoverlayrenderer") ||
                lowerExe.contains("eosbootstrapper") ||
                lowerExe.contains("epicgameslauncher") ||
                lowerExe.contains("goggalaxy") ||
                lowerExe.contains("galaxyclient") ||
                lowerExe.contains("ubisoftconnect") ||
                lowerExe.contains("battle.net") ||
                lowerExe.contains("battlenet") ||
                lowerExe.contains("origin") ||
                lowerExe.contains("eadesktop") ||
                lowerExe.contains("xboxapp") ||
                lowerExe.contains("gamingservices") ||
                lowerExe.contains("launcher") ||
                lowerExe.contains("installer") ||
                lowerExe.contains("redistributable") ||
                lowerExe.contains("prereq")) {
                continue;
            }

            const QString fullPath = binDir.filePath(exe);
            const QString lowerPath = QDir::cleanPath(fullPath).toLower();
            if (lowerPath.contains("/engine/binaries/") || lowerPath.contains("\\engine\\binaries\\")) {
                continue;
            }

            if (lowerExe.contains("-win64-shipping")) {
                exeCandidateShipping = fullPath;
                break;
            }

            const QFileInfo fi(fullPath);
            const qint64 size = fi.exists() ? fi.size() : -1;
            if (size > exeLargestSize) {
                exeLargestSize = size;
                exeCandidateLargest = fullPath;
            }
        }
        if (!exeCandidateShipping.isEmpty()) break;
    }

    exePath = !exeCandidateShipping.isEmpty() ? exeCandidateShipping : exeCandidateLargest;

    if (exePath.isEmpty()) {
        QDirIterator it(gamePath, QStringList() << "*.exe", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString lowerPath = path.toLower();
            if (!(lowerPath.contains("win64") || lowerPath.contains("binaries"))) {
                continue;
            }
            if (lowerPath.contains("\\engine\\binaries\\") || lowerPath.contains("/engine/binaries/")) {
                continue;
            }
            if (lowerPath.contains("crashreport") ||
                lowerPath.contains("crs-handler") ||
                lowerPath.contains("crs-uploader") ||
                lowerPath.contains("crs-client") ||
                lowerPath.contains("unrealcefsubprocess") ||
                lowerPath.contains("epicwebhelper") ||
                lowerPath.contains("easyanticheat")) {
                continue;
            }
            exePath = path;
            break;
        }
    }

    if (exePath.isEmpty()) {
        return QString();
    }

    constexpr int kMaxAttempts = 3;
    constexpr int kPerAttemptTimeoutMs = 20000;
    constexpr int kPostKillWaitMs = 2000;
    constexpr int kRetryDelayMs = 1500;

    for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
        QProcess process;
        process.setProgram(ueDetectorPath);
        process.setArguments(QStringList() << exePath);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start();
        process.closeWriteChannel();

        if (process.waitForFinished(kPerAttemptTimeoutMs)) {
            if (process.exitStatus() == QProcess::NormalExit) {
                const QByteArray output = process.readAllStandardOutput();
                const QString outputStr = QString::fromUtf8(output);
                const QRegularExpression re("UE\\s*([45])\\.(\\d+)");
                const QRegularExpressionMatch match = re.match(outputStr);
                if (match.hasMatch()) {
                    return "UE " + match.captured(1) + "." + match.captured(2);
                }

                return QString();
            }

        } else {

            process.kill();
            process.waitForFinished(kPostKillWaitMs);
        }

        if (attempt < kMaxAttempts) {
            QThread::msleep(kRetryDelayMs);
        }
    }

    return QString();
}
}

QString ModManager::detectUEVersionForGame(const QString& gamePath) {
    if (m_ueDetectorPath.isEmpty()) {
        autoDetectUEDetectorPath();
    }
    if (m_ueDetectorPath.isEmpty() || !QFile::exists(m_ueDetectorPath)) {
        return QString();
    }

    const QString cleanedGamePath = QDir::cleanPath(gamePath);
    const QString cacheKey = normalizeUEVersionCacheKey(cleanedGamePath);
    if (cacheKey.isEmpty()) {
        return QString();
    }

    if (m_ueVersionCache.contains(cacheKey)) {
        return m_ueVersionCache.value(cacheKey);
    }

    const QString detectedVersion = detectUEVersionForPathUncached(m_ueDetectorPath, cleanedGamePath);
    m_ueVersionCache[cacheKey] = detectedVersion;
    logUEVersionDetection(cleanedGamePath, detectedVersion);
    if (!detectedVersion.isEmpty()) {
        emit gameUEVersionDetected(cleanedGamePath, detectedVersion);
    }
    saveInstalledGamesCache();
    return detectedVersion;
}

void ModManager::detectUEVersionsForAllGames() {
    if (m_ueDetectorPath.isEmpty()) {
        autoDetectUEDetectorPath();
    }
    if (m_ueDetectorPath.isEmpty() || !QFile::exists(m_ueDetectorPath) || m_isUeDetectRunning) {

        QMetaObject::invokeMethod(this, [this]() { emit ueVersionsDetected(); }, Qt::QueuedConnection);
        return;
    }

    const QString ueDetectorPath = m_ueDetectorPath;
    const QMap<QString, QString> existingCache = m_ueVersionCache;
    QStringList pendingGamePaths;
    pendingGamePaths.reserve(m_installedGamesList.size());
    for (const QVariant& gameVar : m_installedGamesList) {
        const QVariantMap game = gameVar.toMap();
        const QString cleanedGamePath = QDir::cleanPath(game.value("path").toString());
        const QString cacheKey = normalizeUEVersionCacheKey(cleanedGamePath);
        if (cacheKey.isEmpty() || existingCache.contains(cacheKey)) {
            continue;
        }
        pendingGamePaths.append(cleanedGamePath);
    }

    if (pendingGamePaths.isEmpty()) {

        QMetaObject::invokeMethod(this, [this]() { emit ueVersionsDetected(); }, Qt::QueuedConnection);
        return;
    }

    m_isUeDetectRunning = true;
    emit isUeDetectRunningChanged();

    (void)QtConcurrent::run([this, ueDetectorPath, existingCache, pendingGamePaths]() {
        QMap<QString, QString> mergedCache = existingCache;
        QList<UEDetectLogEntry> newlyResolved;
        QMutex resolveMutex;

        QList<QFuture<void>> futures;
        for (const QString& cleanedGamePath : pendingGamePaths) {
            futures.append(QtConcurrent::run([&, cleanedGamePath]() {
                const QString version = detectUEVersionForPathUncached(ueDetectorPath, cleanedGamePath);
                const QString cacheKey = normalizeUEVersionCacheKey(cleanedGamePath);
                QMutexLocker locker(&resolveMutex);
                mergedCache[cacheKey] = version;
                newlyResolved.append({cleanedGamePath, version});
            }));
        }

        for (QFuture<void>& future : futures) {
            future.waitForFinished();
        }

        QMetaObject::invokeMethod(this, [this, mergedCache, newlyResolved]() {
            m_ueVersionCache = mergedCache;
            for (const UEDetectLogEntry& entry : newlyResolved) {
                logUEVersionDetection(entry.gamePath, entry.version);
                if (!entry.version.isEmpty()) {
                    emit gameUEVersionDetected(entry.gamePath, entry.version);
                }
            }
            if (m_isUeDetectRunning) {
                m_isUeDetectRunning = false;
                emit isUeDetectRunningChanged();
            }
            if (!newlyResolved.isEmpty()) {
                for (int i = 0; i < m_installedGamesList.size(); ++i) {
                    QVariantMap g = m_installedGamesList[i].toMap();
                    const QString key = normalizeUEVersionCacheKey(g.value("path").toString());
                    if (m_ueVersionCache.contains(key)) {
                        g["ueVersion"] = m_ueVersionCache.value(key);
                        m_installedGamesList[i] = g;
                    }
                }
                emit installedGamesListChanged();
                saveInstalledGamesCache();
            }
            emit ueVersionsDetected();
        }, Qt::QueuedConnection);
    });
}

QString ModManager::getUEVersionForGame(const QString& gamePath) const {
    return m_ueVersionCache.value(normalizeUEVersionCacheKey(gamePath), QString());
}

bool ModManager::isUeDetectRunning() const {
    return m_isUeDetectRunning;
}
