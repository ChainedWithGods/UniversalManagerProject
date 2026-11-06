#include "ModManagerInternal.h"
#include <QDateTime>

using namespace mmdetail;

void ModManager::onModsDirectoryChanged() {

    if (m_batchOperationInProgress) {
        qDebug() << "Mods directory changed, but batch operation in progress - skipping rescan";
        return;
    }

    if (QDateTime::currentMSecsSinceEpoch() < m_modsWatcherIgnoreUntilMs) {
        qDebug() << "Mods directory changed by the app - skipping rescan";
        return;
    }
    qDebug() << "Mods directory changed, rescanning...";
    scanMods(m_modsPathInternal);
}

int ModManager::modsInstalled() const { return m_modsInstalled; }

int ModManager::modsActive() const { return m_modsActive; }

QVariantList ModManager::modList() const { return m_modList; }

int ModManager::totalModsAllPresets() const {
    int totalMods = 0;
    QSet<QString> countedPaths;

    for (const QVariant& v : m_gamePresets) {
        const QVariantMap preset = v.toMap();
        const QString modsPath = preset.value("modsPath").toString();

        if (modsPath.isEmpty() || countedPaths.contains(modsPath)) {
            continue;
        }

        countedPaths.insert(modsPath);

        totalMods += countDisplayModsInPath(modsPath);
    }

    return totalMods;
}

int ModManager::totalActiveModsAllPresets() const {
    int totalActive = 0;
    QSet<QString> countedPaths;

    for (const QVariant& v : m_gamePresets) {
        const QVariantMap preset = v.toMap();
        const QString modsPath = preset.value("modsPath").toString();

        if (modsPath.isEmpty() || countedPaths.contains(modsPath)) {
            continue;
        }

        countedPaths.insert(modsPath);

        QDir modsDir(modsPath);
        if (!modsDir.exists()) {
            continue;
        }

        QStringList folders = modsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& folder : folders) {
            QString folderPath = modsDir.filePath(folder);

            QDirIterator modFileIt(folderPath, {"*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x"},
                                   QDir::Files, QDirIterator::Subdirectories);

            bool hasModFiles = false;
            bool isDisabled = false;

            while (modFileIt.hasNext()) {
                QString filePath = modFileIt.next();
                hasModFiles = true;

                if (filePath.endsWith("-x", Qt::CaseInsensitive)) {
                    isDisabled = true;
                    break;
                }
            }

            if (hasModFiles && !isDisabled) {
                totalActive++;
            }
        }

        QStringList looseFiles = modsDir.entryList({"*.pak", "*.ucas", "*.utoc"}, QDir::Files);
        totalActive += looseFiles.count();
    }

    return totalActive;
}

QString ModManager::formatBytes(qint64 bytes) const {
    if (bytes == 0) return "0 B";

    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = bytes;

    while (size >= 1024.0 && unitIndex < units.size() - 1) {
        size /= 1024.0;
        unitIndex++;
    }

    return QString::number(size, 'f', 2) + " " + units[unitIndex];
}

void ModManager::scanMods(const QString& path) {
    m_modsPathInternal = path;
    qDebug() << "Scanning mods in path:" << path;

    const QStringList watchedPaths = m_watcher->directories();
    if (!watchedPaths.isEmpty()) {
        m_watcher->removePaths(watchedPaths);
    }

    if (path.isEmpty() || !QDir(path).exists()) {
        setModsInstalled(0);
        setModsActive(0);
        if (!m_modList.isEmpty()) {
            m_modList.clear();
            m_modDetailsCache.clear();
            emit modListChanged();
        }
        return;
    }

    m_watcher->addPath(path);

    QVariantList newModList;
    QMap<QString, QVariantMap> newDetailsCache;
    int activeCount = 0;
    qint64 totalSize = 0;
    QSet<QString> processedFiles;
    QSet<QString> processedFolders;
    QDir modsRoot(path);

    auto bundleIdForMember = [this](const QString& memberId,
                                    const QString& memberName) -> QString {
        if (memberId.isEmpty() && memberName.isEmpty())
            return {};
        const QString currentRoot = QDir::cleanPath(m_modsPathInternal);
        for (const QVariant& bundleValue : m_modBundles) {
            const QVariantMap bundle = bundleValue.toMap();
            const QString bundleRoot = QDir::cleanPath(bundle.value("root").toString());
            if (!bundleRoot.isEmpty()
                    && bundleRoot.compare(currentRoot, Qt::CaseInsensitive) != 0) {
                continue;
            }
            if (!memberName.isEmpty()) {
                for (const QVariant& storedName : bundle.value("memberNames").toList()) {
                    if (storedName.toString().compare(memberName, Qt::CaseInsensitive) == 0)
                        return bundle.value("id").toString();
                }
            }
            for (const QVariant& member : bundle.value("members").toList()) {
                if (member.toString() == memberId)
                    return bundle.value("id").toString();
            }
        }
        return {};
    };

    QDirIterator dir_it(path, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (dir_it.hasNext()) {
        QString modFolderPath = dir_it.next();

        if (processedFolders.contains(modFolderPath)) continue;

        QDir modDir(modFolderPath);

        QStringList pakFiles = modDir.entryList({ "*.pak", "*.pak-x" }, QDir::Files);
        QStringList ucasFiles = modDir.entryList({ "*.ucas", "*.ucas-x" }, QDir::Files);
        QStringList utocFiles = modDir.entryList({ "*.utoc", "*.utoc-x" }, QDir::Files);

        if (pakFiles.isEmpty() && ucasFiles.isEmpty() && utocFiles.isEmpty()) continue;

        processedFolders.insert(modFolderPath);

        bool isDisabled = false;
        QDirIterator disabledCheckIt(modFolderPath, { "*.pak-x", "*.ucas-x", "*.utoc-x" }, QDir::Files, QDirIterator::Subdirectories);
        if (disabledCheckIt.hasNext()) {
            isDisabled = true;
        }

        if (!isDisabled) {
            activeCount++;
        }

        qint64 modSize = 0;
        int fileCount = 0;
        QDirIterator modFilesIt(modFolderPath, QDir::Files, QDirIterator::Subdirectories);
        while (modFilesIt.hasNext()) {
            QString filePath = modFilesIt.next();
            if (processedFiles.contains(filePath)) continue;

            QFileInfo fileInfo(filePath);
            modSize += fileInfo.size();
            totalSize += fileInfo.size();
            processedFiles.insert(filePath);
            fileCount++;
        }

        QString displayName;
        QString sourceFileName;

        if (!pakFiles.isEmpty()) {
            sourceFileName = pakFiles.first();
        } else if (!ucasFiles.isEmpty()) {
            sourceFileName = ucasFiles.first();
        } else if (!utocFiles.isEmpty()) {
            sourceFileName = utocFiles.first();
        }

        if (sourceFileName.endsWith("-x", Qt::CaseInsensitive)) {
            sourceFileName = sourceFileName.left(sourceFileName.length() - 2);
        }

        int lastDot = sourceFileName.lastIndexOf('.');
        if (lastDot != -1) {
            sourceFileName = sourceFileName.left(lastDot);
        }

        if (sourceFileName.toUpper().endsWith("_P")) {
            displayName = sourceFileName.left(sourceFileName.length() - 2);
        } else {
            displayName = sourceFileName;
        }

        if (!m_modImages.contains(displayName)) {
            QString foundImage = findModImage(modFolderPath);
            if (!foundImage.isEmpty()) {
                m_modImages[displayName] = foundImage;
            }
        }

        QString modId = m_modIds.value(displayName, "");
        if (modId.isEmpty()) {
            QString generatedId;
            const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            for (int i = 0; i < 8; ++i) {
                generatedId += chars[QRandomGenerator::global()->bounded(36)];
            }
            m_modIds[displayName] = generatedId;
            modId = generatedId;
        }
        const QString bundleId = bundleIdForMember(modId, displayName);

        QVariantMap modData;
        modData["name"] = displayName;
        modData["status"] = isDisabled ? "Disabled" : "Active";
        modData["fileCount"] = fileCount;
        modData["type"] = "folder";
        modData["folderPath"] = modsRoot.relativeFilePath(modFolderPath);
        modData["bundleId"] = bundleId;
        newModList.append(modData);

        QVariantMap details;
        QFileInfo dirInfo(modFolderPath);
        details["installDate"] = dirInfo.birthTime().toString("yyyy.MM.dd");
        details["path"] = modFolderPath;
        details["size"] = formatBytes(modSize);
        details["sizeBytes"] = modSize;
        details["fileCount"] = fileCount;
        details["enabled"] = !isDisabled;
        details["type"] = "folder";
        details["modId"] = modId;
        details["bundleId"] = bundleId;
        newDetailsCache[displayName] = details;
    }

    QDirIterator file_it(path, QDir::Files | QDir::NoDotAndDotDot);
    while (file_it.hasNext()) {
        QString filePath = file_it.next();
        if (processedFiles.contains(filePath)) continue;

        QFileInfo fileInfo(filePath);
        QString extension = fileInfo.suffix().toLower();

        if (extension == "pak" || extension == "pak-x" || extension == "ucas" || extension == "ucas-x" || extension == "utoc" || extension == "utoc-x") {
            QString fileName = fileInfo.fileName();
            bool isDisabled = fileName.endsWith("-x", Qt::CaseInsensitive);

            if (!isDisabled) {
                activeCount++;
            }

            totalSize += fileInfo.size();

            QString cleanExt = extension;
            if (cleanExt.endsWith("-x")) {
                cleanExt = cleanExt.left(cleanExt.length() - 2);
            }

            QString displayName = fileInfo.completeBaseName();

            if (displayName.toUpper().endsWith("_P")) {
                displayName = displayName.left(displayName.length() - 2);
            }

            QString uniqueId = displayName + "." + cleanExt;

            QString modIdLoose = m_modIds.value(displayName, "");
            if (modIdLoose.isEmpty()) {
                QString generatedId;
                const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
                for (int i = 0; i < 8; ++i) {
                    generatedId += chars[QRandomGenerator::global()->bounded(36)];
                }
                m_modIds[displayName] = generatedId;
                modIdLoose = generatedId;
            }
            const QString bundleIdLoose = bundleIdForMember(modIdLoose, displayName);

            QVariantMap modData;
            modData["name"] = displayName;
            modData["uniqueId"] = uniqueId;
            modData["fileName"] = fileName;
            modData["status"] = isDisabled ? "Disabled" : "Active";
            modData["fileCount"] = 1;
            modData["type"] = "loose";
            modData["extensions"] = cleanExt;
            modData["bundleId"] = bundleIdLoose;
            newModList.append(modData);

            QVariantMap details;
            details["installDate"] = fileInfo.birthTime().toString("yyyy.MM.dd");
            details["path"] = filePath;
            details["size"] = formatBytes(fileInfo.size());
            details["sizeBytes"] = fileInfo.size();
            details["fileCount"] = 1;
            details["enabled"] = !isDisabled;
            details["type"] = "loose";
            details["modId"] = modIdLoose;
            details["bundleId"] = bundleIdLoose;
            newDetailsCache[displayName] = details;
        }
    }

    m_modDetailsCache = newDetailsCache;
    m_modList = newModList;
    m_totalBytes = totalSize;
    emit modListChanged();
    emit storageUsedChanged();

    setModsInstalled(m_modList.count());
    setModsActive(activeCount);

    QSet<QString> currentModNames;
    for (const QVariant& modVar : m_modList) {
        QVariantMap mod = modVar.toMap();
        currentModNames.insert(mod["name"].toString());

        if (mod.contains("uniqueId")) {
            currentModNames.insert(mod["uniqueId"].toString());
        }
    }

    cleanupStaleModData(currentModNames);
    adoptBundleFoldersFromScan();
    refreshBundleMembership();
    emit modBundlesChanged();

    saveSettings();

    emit totalActiveModsAllPresetsChanged();
    emit totalModsAllPresetsChanged();
}

QString ModManager::ensurePSuffix(const QString& baseName) const {
    if (baseName.toUpper().endsWith("_P")) {
        return baseName;
    }

    return baseName + "_P";
}

QVariantMap ModManager::getModDetails(const QString& modName) {
    QString cacheKey = modName;

    if (!m_modDetailsCache.contains(cacheKey)) {
        const QString normalizedSearch = normalizedDisplayModName(modName);

        for (const QVariant& modVar : m_modList) {
            const QVariantMap mod = modVar.toMap();
            const QString displayName = mod.value("name").toString();
            const QString uniqueId = mod.value("uniqueId").toString();
            const QString fileName = mod.value("fileName").toString();

            const bool exactMatch =
                displayName.compare(modName, Qt::CaseInsensitive) == 0 ||
                uniqueId.compare(modName, Qt::CaseInsensitive) == 0 ||
                fileName.compare(modName, Qt::CaseInsensitive) == 0;

            const bool normalizedMatch =
                !normalizedSearch.isEmpty() &&
                (normalizedDisplayModName(displayName) == normalizedSearch ||
                 normalizedDisplayModName(uniqueId) == normalizedSearch ||
                 normalizedDisplayModName(fileName) == normalizedSearch);

            if (exactMatch || normalizedMatch) {
                cacheKey = displayName;
                break;
            }
        }
    }

    if (m_modDetailsCache.contains(cacheKey)) {
        QVariantMap details = m_modDetailsCache[cacheKey];
        details["description"] = getModDescription(cacheKey);
        return details;
    }

    QVariantMap details;
    details["installDate"] = "N/A";
    details["path"] = "";
    details["size"] = "N/A";
    details["enabled"] = false;
    details["type"] = "unknown";
    details["description"] = getModDescription(cacheKey);

    qDebug() << "Warning: getModDetails called for uncached mod:" << modName;
    return details;
}

QString ModManager::getActualFileName(const QString& modName) const {

    for (const QVariant& modVar : m_modList) {
        QVariantMap mod = modVar.toMap();
        if (mod["type"].toString() == "loose") {
            QString name = mod["name"].toString();
            QString uniqueId = mod.value("uniqueId").toString();
            if (name == modName || uniqueId == modName) {
                return mod["fileName"].toString();
            }
        }
    }
    return modName;
}
