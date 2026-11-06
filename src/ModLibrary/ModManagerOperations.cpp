#include "ModManagerInternal.h"
#include <QDateTime>

using namespace mmdetail;

void ModManager::applyModEnabledState(const QStringList& modKeys, bool enabled) {
    if (modKeys.isEmpty())
        return;

    QSet<QString> keys;
    keys.reserve(modKeys.size());
    for (const QString& key : modKeys)
        keys.insert(key);
    QVariantList updated = m_modList;
    QStringList changed;
    int newActiveCount = m_modsActive;

    for (QVariant& value : updated) {
        QVariantMap mod = value.toMap();
        const QString name = mod.value("name").toString();
        const QString uniqueId = mod.value("uniqueId").toString();
        if (!keys.contains(name) && (uniqueId.isEmpty() || !keys.contains(uniqueId)))
            continue;

        const QString status = enabled ? QStringLiteral("Active") : QStringLiteral("Disabled");
        if (mod.value("status").toString() == status)
            continue;

        mod["status"] = status;
        value = mod;
        changed.append(name);
        newActiveCount += enabled ? 1 : -1;

        auto detailsIt = m_modDetailsCache.find(name);
        if (detailsIt != m_modDetailsCache.end())
            (*detailsIt)["enabled"] = enabled;
    }

    if (changed.isEmpty())
        return;

    m_modList = updated;
    setModsActive(newActiveCount);
    for (const QString& name : changed)
        emit modStatusChanged(name, enabled);
    emit modBundlesChanged();
}

int ModManager::applyModRemoval(const QStringList& modKeys) {
    if (modKeys.isEmpty())
        return 0;

    QSet<QString> keys;
    keys.reserve(modKeys.size());
    for (const QString& key : modKeys)
        keys.insert(key);

    QVariantList updated;
    updated.reserve(m_modList.size());
    QStringList removedNames;
    QStringList removedIds;
    int removedInstalled = 0;
    int removedActive = 0;

    for (const QVariant& value : m_modList) {
        const QVariantMap mod = value.toMap();
        const QString name = mod.value("name").toString();
        const QString uniqueId = mod.value("uniqueId").toString();
        const bool matches = keys.contains(name)
            || (!uniqueId.isEmpty() && keys.contains(uniqueId));
        if (!matches) {
            updated.append(value);
            continue;
        }

        ++removedInstalled;
        if (mod.value("status").toString() == QLatin1String("Active"))
            ++removedActive;
        removedNames.append(name);
        const QString modUniqueId = m_modIds.value(name);
        if (!modUniqueId.isEmpty() && !removedIds.contains(modUniqueId))
            removedIds.append(modUniqueId);
        m_modDetailsCache.remove(name);
        m_modImages.remove(name);
        m_modIds.remove(name);
        m_modTags.remove(name);
        m_modDescriptions.remove(name);
        if (!uniqueId.isEmpty()) {
            const QString looseId = m_modIds.value(uniqueId);
            if (!looseId.isEmpty() && !removedIds.contains(looseId))
                removedIds.append(looseId);
            m_modImages.remove(uniqueId);
            m_modIds.remove(uniqueId);
            m_modTags.remove(uniqueId);
            m_modDescriptions.remove(uniqueId);
        }
    }

    if (removedNames.isEmpty())
        return 0;

    m_modList = updated;
    setModsInstalled(m_modsInstalled - removedInstalled);
    setModsActive(m_modsActive - removedActive);
    for (const QString& name : removedNames)
        emit modRemoved(name);
    emit modListChanged();
    removeModIdsFromBundles(removedIds);
    return removedNames.size();
}

void ModManager::toggleModStatus(const QString& modName) {
    QDir modsRoot(m_modsPathInternal);
    m_batchOperationInProgress = true;

    bool enabling = false;
    bool filesTouched = false;
    bool renameFailed = false;

    QString actualFileName = getActualFileName(modName);

    bool isLooseFile = false;
    QString modFolderPath;

    for (const QVariant& modVar : m_modList) {
        QVariantMap mod = modVar.toMap();
        if (mod["name"].toString() == modName) {
            if (mod["type"].toString() == "loose") {
                isLooseFile = true;
            } else if (mod["type"].toString() == "folder") {

                modFolderPath = modsRoot.filePath(mod["folderPath"].toString());
            }
            break;
        }
    }

    if (isLooseFile) {

        QString filePath = modsRoot.filePath(actualFileName);

        if (QFileInfo::exists(filePath)) {
            filesTouched = true;
            if (filePath.endsWith("-x", Qt::CaseInsensitive)) {

                enabling = true;
                QString newFilePath = filePath;
                newFilePath.chop(2);
                if (!QFile::rename(filePath, newFilePath))
                    renameFailed = true;
            } else {

                enabling = false;
                QFileInfo fileInfo(filePath);
                QString ext = fileInfo.suffix().toLower();
                if (ext == "pak" || ext == "ucas" || ext == "utoc") {
                    if (!QFile::rename(filePath, filePath + "-x"))
                        renameFailed = true;
                }
            }
        }
    } else if (!modFolderPath.isEmpty()) {

        QDir modDir(modFolderPath);

        QDirIterator it(modFolderPath, { "*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x" },
                        QDir::Files, QDirIterator::Subdirectories);
        QStringList modFiles;
        while (it.hasNext()) {
            modFiles.append(it.next());
        }

        bool isCurrentlyDisabled = false;
        for (const QString& file : modFiles) {
            if (file.endsWith("-x", Qt::CaseInsensitive)) {
                isCurrentlyDisabled = true;
                break;
            }
        }

        if (!modFiles.isEmpty()) {
            filesTouched = true;
            enabling = isCurrentlyDisabled;
        }

        for (const QString& fullPath : modFiles) {
            if (isCurrentlyDisabled) {

                if (fullPath.endsWith("-x", Qt::CaseInsensitive)) {
                    QString newPath = fullPath;
                    newPath.chop(2);
                    if (!QFile::rename(fullPath, newPath))
                        renameFailed = true;
                }
            } else {

                QFileInfo fileInfo(fullPath);
                QString ext = fileInfo.suffix().toLower();
                if (ext == "pak" || ext == "ucas" || ext == "utoc") {
                    if (!QFile::rename(fullPath, fullPath + "-x"))
                        renameFailed = true;
                }
            }
        }
    }

    m_batchOperationInProgress = false;

    m_modsWatcherIgnoreUntilMs = QDateTime::currentMSecsSinceEpoch() + 1000;

    if (!filesTouched)
        return;
    if (renameFailed) {
        scanMods(m_modsPathInternal);
        return;
    }

    applyModEnabledState(QStringList{modName}, enabling);
}

void ModManager::toggleModStatusBatch(const QStringList& modNames, bool enable) {

    m_batchOperationInProgress = true;
    bool renameFailed = false;

    QDir modsRoot(m_modsPathInternal);

    auto findModFolder = [&](const QString& searchPath, const QString& modName) -> QString {
        QDirIterator it(searchPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString folderPath = it.next();
            QDir folder(folderPath);
            QStringList modFiles = folder.entryList({
                modName + ".pak", modName + "_P.pak", modName + ".pak-x", modName + "_P.pak-x",
                modName + ".ucas", modName + "_P.ucas", modName + ".ucas-x", modName + "_P.ucas-x",
                modName + ".utoc", modName + "_P.utoc", modName + ".utoc-x", modName + "_P.utoc-x"
            }, QDir::Files);
            if (!modFiles.isEmpty()) {
                return folderPath;
            }
        }
        return QString();
    };

    for (const QString& modName : modNames) {

        QString actualFileName = getActualFileName(modName);
        bool isLooseFile = false;
        QString modFolderPath;

        for (const QVariant& modVar : m_modList) {
            QVariantMap mod = modVar.toMap();
            const QString currentName = mod["name"].toString();
            const QString currentUniqueId = mod.value("uniqueId").toString();
            if (currentName == modName || (!currentUniqueId.isEmpty() && currentUniqueId == modName)) {
                if (mod["type"].toString() == "loose") {
                    isLooseFile = true;
                } else if (mod["type"].toString() == "folder") {
                    modFolderPath = modsRoot.filePath(mod["folderPath"].toString());
                }
                break;
            }
        }

        if (isLooseFile) {
            QString filePath = modsRoot.filePath(actualFileName);
            if (!QFileInfo::exists(filePath))
                continue;

            bool isCurrentlyDisabled = filePath.endsWith("-x", Qt::CaseInsensitive);

            if (enable && isCurrentlyDisabled) {

                QString newFilePath = filePath;
                newFilePath.chop(2);
                if (!QFile::rename(filePath, newFilePath))
                    renameFailed = true;
            } else if (!enable && !isCurrentlyDisabled) {

                QFileInfo fileInfo(filePath);
                QString ext = fileInfo.suffix().toLower();
                if (ext == "pak" || ext == "ucas" || ext == "utoc") {
                    if (!QFile::rename(filePath, filePath + "-x"))
                        renameFailed = true;
                }
            }
        } else {
            if (modFolderPath.isEmpty())
                modFolderPath = findModFolder(m_modsPathInternal, modName);

            if (modFolderPath.isEmpty())
                continue;

            QDirIterator it(modFolderPath, { "*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x" },
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString fullPath = it.next();
                bool isCurrentlyDisabled = fullPath.endsWith("-x", Qt::CaseInsensitive);

                if (enable && isCurrentlyDisabled) {
                    QString newPath = fullPath;
                    newPath.chop(2);
                    if (!QFile::rename(fullPath, newPath))
                        renameFailed = true;
                } else if (!enable && !isCurrentlyDisabled) {
                    QFileInfo fileInfo(fullPath);
                    QString ext = fileInfo.suffix().toLower();
                    if (ext == "pak" || ext == "ucas" || ext == "utoc") {
                        if (!QFile::rename(fullPath, fullPath + "-x"))
                            renameFailed = true;
                    }
                }
            }
        }
    }

    m_batchOperationInProgress = false;
    m_modsWatcherIgnoreUntilMs = QDateTime::currentMSecsSinceEpoch() + 1000;

    if (renameFailed) {
        scanMods(m_modsPathInternal);
        return;
    }

    applyModEnabledState(modNames, enable);
}

void ModManager::deleteMod(const QString& modName) {
    qDebug() << "deleteMod called for" << modName;

    qDebug() << "deleteMod: removing mod image (if any) for" << modName;
    removeModImageInternal(modName, false);

    m_batchOperationInProgress = true;

    QTimer::singleShot(0, this, [this, modName]() {
        QDir modsRoot(m_modsPathInternal);

        auto findModFolder = [&](const QString& searchPath, const QString& modNameInner) -> QString {
            QDirIterator it(searchPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString folderPath = it.next();
                QDir folder(folderPath);
                QStringList modFiles = folder.entryList({
                    modNameInner + ".pak", modNameInner + "_P.pak", modNameInner + ".pak-x", modNameInner + "_P.pak-x",
                    modNameInner + ".ucas", modNameInner + "_P.ucas", modNameInner + ".ucas-x", modNameInner + "_P.ucas-x",
                    modNameInner + ".utoc", modNameInner + "_P.utoc", modNameInner + ".utoc-x", modNameInner + "_P.utoc-x"
                }, QDir::Files);
                if (!modFiles.isEmpty()) {
                    return folderPath;
                }
            }
            return QString();
        };

        QString actualFileName = getActualFileName(modName);

        bool isLooseFile = false;
        bool deleted = false;
        for (const QVariant& modVar : m_modList) {
            QVariantMap mod = modVar.toMap();
            if (mod["type"].toString() == "loose") {
                QString name = mod["name"].toString();
                QString uniqueId = mod.value("uniqueId").toString();
                if (name == modName || uniqueId == modName) {
                    isLooseFile = true;
                    break;
                }
            }
        }

        if (isLooseFile) {

            QString filePath = modsRoot.filePath(actualFileName);
            qDebug() << "deleteMod: deleting loose file" << filePath;

            if (QFileInfo::exists(filePath)) {
                bool ok = false;
                if (m_deleteToRecycleBin) {
                    ok = QFile::moveToTrash(filePath);
                } else {
                    ok = QFile::remove(filePath);
                }
                deleted = ok;
                if (!ok) {
                    qDebug() << "deleteMod: FAILED to delete loose file" << filePath;
                }
            } else {
                qDebug() << "deleteMod: loose file does not exist" << filePath;
            }
        } else {

            QString modFolderPath = findModFolder(m_modsPathInternal, modName);

            if (!modFolderPath.isEmpty()) {
                QDir modDir(modFolderPath);
                qDebug() << "deleteMod: deleting folder mod at" << modFolderPath;
                if (m_deleteToRecycleBin) {
#ifdef Q_OS_WIN
                    QString path = QDir::toNativeSeparators(modFolderPath);
                    path.replace("/", "\\");

                    deleted = QFile::moveToTrash(modFolderPath);
                    if (!deleted) {
                        qDebug() << "deleteMod: FAILED to move folder to recycle bin" << modFolderPath;
                    }
#else
                    deleted = modDir.removeRecursively();
                    if (!deleted) {
                        qDebug() << "deleteMod: FAILED to remove folder recursively" << modFolderPath;
                    }
#endif
                } else {
                    deleted = modDir.removeRecursively();
                }
            }
        }

        const QString removedModId = m_modIds.value(modName);
        m_modIds.remove(modName);
        m_modTags.remove(modName);
        m_modDescriptions.remove(modName);

        QStringList removedModIds;
        if (!removedModId.isEmpty())
            removedModIds.append(removedModId);
        unlinkNexusUninstalledMods(QStringList() << modName, removedModIds);
        forgetModAliases(removedModId);

        cleanupDeletedModsFromCollections(QStringList() << modName);

        const int removed = deleted ? applyModRemoval(QStringList{modName}) : 0;
        if (!deleted || removed == 0)
            scanMods(m_modsPathInternal);
        else {

            removeModIdsFromBundles(removedModIds);
            saveSettings();
        }

        m_batchOperationInProgress = false;
        m_modsWatcherIgnoreUntilMs = QDateTime::currentMSecsSinceEpoch() + 1000;
    });
}

void ModManager::deleteModsAsync(const QStringList& modNames) {
    qDebug() << "deleteModsAsync called for" << modNames.size() << "mods";

    (void)QtConcurrent::run([this, modNames]() {
        QMap<QString, QString> folderPathByMod;
        QMap<QString, QString> actualFileNameByMod;
        QMap<QString, QString> modIdByMod;

        QMetaObject::invokeMethod(this, [this, modNames, &folderPathByMod, &actualFileNameByMod,
                                         &modIdByMod]() {
            m_batchOperationInProgress = true;

            const QStringList watchedPaths = m_watcher->directories();
            if (!watchedPaths.isEmpty()) {
                m_watcher->removePaths(watchedPaths);
            }

            for (const QVariant& modVar : m_modList) {
                const QVariantMap mod = modVar.toMap();
                const QString modName = mod.value("name").toString();
                const QString modType = mod.value("type").toString();

                if (modType == "folder") {
                    folderPathByMod[modName] = mod.value("folderPath").toString();
                } else if (modType == "loose") {
                    const QString fileName = mod.value("fileName").toString();
                    actualFileNameByMod[modName] = fileName;

                    const QString uniqueId = mod.value("uniqueId").toString();
                    if (!uniqueId.isEmpty()) {
                        actualFileNameByMod[uniqueId] = fileName;
                    }
                }
            }

            for (const QString& modName : modNames) {
                m_modImages.remove(modName);
                modIdByMod.insert(modName, m_modIds.value(modName));
            }
        }, Qt::BlockingQueuedConnection);

        int total = modNames.size();
        int deletedCount = 0;
        QStringList deletedNames;
        QStringList deletedIds;
        QElapsedTimer progressTimer;
        progressTimer.start();
        qint64 lastProgressEmitMs = -1000;

        for (int i = 0; i < total; ++i) {
            const QString& modName = modNames[i];

            const qint64 elapsedMs = progressTimer.elapsed();
            const bool shouldEmitProgress =
                (i == 0) ||
                (i == total - 1) ||
                (elapsedMs - lastProgressEmitMs >= 75);
            if (shouldEmitProgress) {
                emit deleteProgress(i + 1, total, modName);
                lastProgressEmitMs = elapsedMs;
            }

            QDir modsRoot(m_modsPathInternal);

            const QString actualFileName = actualFileNameByMod.value(modName, modName);
            const bool isLooseFile = actualFileNameByMod.contains(modName);

            bool success = false;
            if (isLooseFile) {
                QString filePath = modsRoot.filePath(actualFileName);
                if (QFileInfo::exists(filePath)) {
                    if (m_deleteToRecycleBin) {
                        success = QFile::moveToTrash(filePath);
                    } else {
                        success = QFile::remove(filePath);
                    }
                }
            } else {
                QString modFolderPath = folderPathByMod.value(modName);
                if (!modFolderPath.isEmpty()) {
                    modFolderPath = modsRoot.filePath(modFolderPath);
                }

                if (modFolderPath.isEmpty() || !QFileInfo::exists(modFolderPath)) {

                    QDirIterator it(m_modsPathInternal, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
                    while (it.hasNext()) {
                        QString candidateFolderPath = it.next();
                        QDir folder(candidateFolderPath);
                        QStringList modFiles = folder.entryList({
                            modName + ".pak", modName + "_P.pak", modName + ".pak-x", modName + "_P.pak-x",
                            modName + ".ucas", modName + "_P.ucas", modName + ".ucas-x", modName + "_P.ucas-x",
                            modName + ".utoc", modName + "_P.utoc", modName + ".utoc-x", modName + "_P.utoc-x"
                        }, QDir::Files);
                        if (!modFiles.isEmpty()) {
                            modFolderPath = candidateFolderPath;
                            break;
                        }
                    }
                }

                if (!modFolderPath.isEmpty()) {
                    if (m_deleteToRecycleBin) {
                        success = QFile::moveToTrash(modFolderPath);
                    } else {
                        QDir modDir(modFolderPath);
                        success = modDir.removeRecursively();
                    }
                }
            }

            if (success) {
                deletedCount++;
                deletedNames.append(modName);
                const QString modId = modIdByMod.value(modName);
                if (!modId.isEmpty())
                    deletedIds.append(modId);

                m_modIds.remove(modName);
                m_modTags.remove(modName);
                m_modDescriptions.remove(modName);
            }
        }

        QMetaObject::invokeMethod(this, [this, modNames, deletedNames, deletedIds, deletedCount, total]() {

            cleanupDeletedModsFromCollections(modNames);
            unlinkNexusUninstalledMods(deletedNames, deletedIds);
            for (const QString& id : deletedIds)
                forgetModAliases(id);

            const int removed = applyModRemoval(deletedNames);

            if (!deletedIds.isEmpty())
                removeModIdsFromBundles(deletedIds);
            if (deletedCount != total || removed != deletedCount)
                scanMods(m_modsPathInternal);
            else
                saveSettings();

            m_batchOperationInProgress = false;
            m_modsWatcherIgnoreUntilMs = QDateTime::currentMSecsSinceEpoch() + 1000;
            emit deleteComplete(deletedCount == total, deletedCount, total);
        }, Qt::QueuedConnection);
    });
}

void ModManager::openModFolder(const QString& modName) {
    QDir modsRoot(m_modsPathInternal);

    auto findModFolder = [&](const QString& searchPath, const QString& modName) -> QString {
        QDirIterator it(searchPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString folderPath = it.next();
            QDir folder(folderPath);
            QStringList modFiles = folder.entryList({
                modName + ".pak", modName + "_P.pak", modName + ".pak-x", modName + "_P.pak-x",
                modName + ".ucas", modName + "_P.ucas", modName + ".ucas-x", modName + "_P.ucas-x",
                modName + ".utoc", modName + "_P.utoc", modName + ".utoc-x", modName + "_P.utoc-x"
            }, QDir::Files);
            if (!modFiles.isEmpty()) {
                return folderPath;
            }
        }
        return QString();
    };

    QString pathToOpen;

    QString actualFileName = getActualFileName(modName);

    bool isLooseFile = false;
    for (const QVariant& modVar : m_modList) {
        QVariantMap mod = modVar.toMap();
        if (mod["name"].toString() == modName && mod["type"].toString() == "loose") {
            isLooseFile = true;
            break;
        }
    }

    if (isLooseFile) {

        pathToOpen = modsRoot.filePath(actualFileName);
    } else {

        QString modFolderPath = findModFolder(m_modsPathInternal, modName);
        if (!modFolderPath.isEmpty()) {
            pathToOpen = modFolderPath;
        } else {
            pathToOpen = m_modsPathInternal;
        }
    }

#ifdef Q_OS_WIN
    QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(pathToOpen)});
#elif defined(Q_OS_MAC)
    QProcess::startDetached("open", {"-R", pathToOpen});
#else
    QProcess::startDetached("xdg-open", {QFileInfo(pathToOpen).absolutePath()});
#endif
}

void ModManager::openModsDirectory() {
    if (m_modsPathInternal.isEmpty()) {
        qDebug() << "Mods path not configured";
        return;
    }

#ifdef Q_OS_WIN
    QProcess::startDetached("explorer.exe", {QDir::toNativeSeparators(m_modsPathInternal)});
#elif defined(Q_OS_MAC)
    QProcess::startDetached("open", {m_modsPathInternal});
#else
    QProcess::startDetached("xdg-open", {m_modsPathInternal});
#endif
}

void ModManager::renameMod(const QString& oldName, const QString& newName) {
    const QString trimmedOld = oldName.trimmed();
    const QString trimmedNew = newName.trimmed();

    if (trimmedOld.isEmpty() || trimmedNew.isEmpty()) {
        qDebug() << "renameMod: empty name not allowed";
        return;
    }

    if (trimmedOld == trimmedNew) {
        qDebug() << "renameMod: new name is identical to old name, nothing to do";
        return;
    }

    static const QRegularExpression invalidChars("[\\\\/:*?\"<>|\r\n\t]");
    if (trimmedNew.contains(invalidChars)) {
        qDebug() << "renameMod: new name contains invalid characters" << trimmedNew;
        return;
    }

    if (m_modsPathInternal.isEmpty()) {
        qDebug() << "renameMod: mods path not configured";
        return;
    }

    QVariantMap targetMod;
    for (const QVariant& modVar : m_modList) {
        QVariantMap mod = modVar.toMap();
        if (mod["name"].toString() == trimmedOld) {
            targetMod = mod;
            break;
        }
    }

    if (targetMod.isEmpty()) {
        qDebug() << "renameMod: mod not found in list:" << trimmedOld;
        return;
    }

    const QString modType = targetMod.value("type").toString();
    const QString folderPathRelative = targetMod.value("folderPath").toString();
    const QString storedFileName = targetMod.value("fileName").toString();

    auto stripModSuffix = [](const QString& in) -> QString {
        static const QStringList suffixes = {
            ".pak-x", ".ucas-x", ".utoc-x",
            ".pak", ".ucas", ".utoc"
        };
        QString base = in;
        for (const QString& s : suffixes) {
            if (base.endsWith(s)) {
                base.chop(s.length());
                if (base.endsWith("_P")) {
                    base.chop(2);
                }
                return base;
            }
        }

        int dotIdx = base.lastIndexOf(".pak");
        int dotIdx2 = base.lastIndexOf(".ucas");
        int dotIdx3 = base.lastIndexOf(".utoc");
        if (dotIdx2 > dotIdx) dotIdx = dotIdx2;
        if (dotIdx3 > dotIdx) dotIdx = dotIdx3;
        if (dotIdx > 0) {
            base = base.left(dotIdx);
        }
        if (base.endsWith("_P")) {
            base.chop(2);
        }
        return base;
    };

    QString actualFileName;
    if (!storedFileName.isEmpty()) {
        actualFileName = stripModSuffix(storedFileName);
    } else {
        const QString fromCache = getActualFileName(trimmedOld);
        actualFileName = fromCache.isEmpty() ? trimmedOld : stripModSuffix(fromCache);
    }
    if (actualFileName.isEmpty()) {
        actualFileName = trimmedOld;
    }

    QDir modsRoot(m_modsPathInternal);

    bool renamedSomething = false;

    const bool wasBatchInProgress = m_batchOperationInProgress;
    m_batchOperationInProgress = true;
    const QStringList watchedPaths = m_watcher->directories();
    if (!watchedPaths.isEmpty()) {
        m_watcher->removePaths(watchedPaths);
    }

    if (modType == "folder") {

        QString foundFolder;
        if (!folderPathRelative.isEmpty()) {
            const QString candidate = modsRoot.absoluteFilePath(folderPathRelative);
            if (QDir(candidate).exists()) {
                foundFolder = candidate;
            }
        }
        if (foundFolder.isEmpty()) {
            QDirIterator it(m_modsPathInternal, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString folderPath = it.next();
                QDir folder(folderPath);

                QStringList modFiles = folder.entryList({
                    actualFileName + ".pak", actualFileName + "_P.pak",
                    actualFileName + ".pak-x", actualFileName + "_P.pak-x",
                    actualFileName + ".ucas", actualFileName + "_P.ucas",
                    actualFileName + ".ucas-x", actualFileName + "_P.ucas-x",
                    actualFileName + ".utoc", actualFileName + "_P.utoc",
                    actualFileName + ".utoc-x", actualFileName + "_P.utoc-x"
                }, QDir::Files);
                if (!modFiles.isEmpty()) {
                    foundFolder = folderPath;
                    break;
                }
            }
        }

        if (foundFolder.isEmpty()) {
            qDebug() << "renameMod: could not locate folder for mod" << trimmedOld;
            m_batchOperationInProgress = wasBatchInProgress;
            return;
        }

        QDir folderDir(foundFolder);
        const QString folderBaseName = folderDir.dirName();

        QStringList innerFiles = folderDir.entryList({
            actualFileName + ".pak", actualFileName + "_P.pak",
            actualFileName + ".pak-x", actualFileName + "_P.pak-x",
            actualFileName + ".ucas", actualFileName + "_P.ucas",
            actualFileName + ".ucas-x", actualFileName + "_P.ucas-x",
            actualFileName + ".utoc", actualFileName + "_P.utoc",
            actualFileName + ".utoc-x", actualFileName + "_P.utoc-x"
        }, QDir::Files);

        if (innerFiles.isEmpty()) {
            qDebug() << "renameMod: no mod files inside folder for" << trimmedOld;
            m_batchOperationInProgress = wasBatchInProgress;
            return;
        }

        for (const QString& oldFileName : innerFiles) {
            QString newFileName = oldFileName;

            if (newFileName.startsWith(actualFileName)) {
                newFileName = trimmedNew + newFileName.mid(actualFileName.length());
            }
            const QString from = folderDir.absoluteFilePath(oldFileName);
            const QString to = folderDir.absoluteFilePath(newFileName);
            if (oldFileName == newFileName) continue;
            if (folderDir.exists(newFileName)) {
                qDebug() << "renameMod: target inner file already exists, skipping:" << newFileName;
                continue;
            }
            if (QFile::rename(from, to)) {
                renamedSomething = true;
                qDebug() << "renameMod: renamed file" << oldFileName << "->" << newFileName;
            } else {
                qDebug() << "renameMod: failed to rename file" << from << "->" << to;
            }
        }

        if (folderBaseName != trimmedNew) {
            QString parentPath = folderDir.absolutePath();
            parentPath.chop(folderBaseName.length());
            const QString newFolderPath = QDir(parentPath).filePath(trimmedNew);
            if (QDir(newFolderPath).exists()) {
                qDebug() << "renameMod: target folder already exists, skipping folder rename" << newFolderPath;
                m_batchOperationInProgress = wasBatchInProgress;
                return;
            }

            QDir parentDir(parentPath);
            if (!parentDir.rename(folderBaseName, trimmedNew)) {
                qDebug() << "renameMod: failed to rename folder" << foundFolder << "->" << newFolderPath;
                m_batchOperationInProgress = wasBatchInProgress;
                return;
            }
            renamedSomething = true;
            qDebug() << "renameMod: renamed folder" << foundFolder << "->" << newFolderPath;
            foundFolder = newFolderPath;
        }
    } else if (modType == "loose") {

        const QString searchDir = m_modsPathInternal;

        QStringList looseProbes = QDir(searchDir).entryList({
            actualFileName + ".pak", actualFileName + "_P.pak",
            actualFileName + ".pak-x", actualFileName + "_P.pak-x",
            actualFileName + ".ucas", actualFileName + "_P.ucas",
            actualFileName + ".ucas-x", actualFileName + "_P.ucas-x",
            actualFileName + ".utoc", actualFileName + "_P.utoc",
            actualFileName + ".utoc-x", actualFileName + "_P.utoc-x"
        }, QDir::Files);

        if (looseProbes.isEmpty()) {
            qDebug() << "renameMod: no physical files found for loose mod" << trimmedOld;
            m_batchOperationInProgress = wasBatchInProgress;
            return;
        }

        for (const QString& oldFileName : looseProbes) {
            QString newFileName = oldFileName;
            if (newFileName.startsWith(actualFileName)) {
                newFileName = trimmedNew + newFileName.mid(actualFileName.length());
            }
            const QString from = QDir(searchDir).absoluteFilePath(oldFileName);
            const QString to = QDir(searchDir).absoluteFilePath(newFileName);
            if (oldFileName == newFileName) continue;
            if (QDir(searchDir).exists(newFileName)) {
                qDebug() << "renameMod: target file already exists, skipping:" << newFileName;
                continue;
            }
            if (QFile::rename(from, to)) {
                renamedSomething = true;
                qDebug() << "renameMod: renamed file" << oldFileName << "->" << newFileName;
            } else {
                qDebug() << "renameMod: failed to rename file" << from << "->" << to;
            }
        }
    } else {
        qDebug() << "renameMod: unknown mod type for" << trimmedOld;
        m_batchOperationInProgress = wasBatchInProgress;
        return;
    }

    if (!renamedSomething) {

        m_batchOperationInProgress = wasBatchInProgress;
        return;
    }

    auto renameKey = [](QMap<QString, QString>& map, const QString& oldK, const QString& newK) {
        if (oldK == newK || !map.contains(oldK)) return;
        map.insert(newK, map.take(oldK));
    };

    auto renameListKey = [](QMap<QString, QStringList>& map, const QString& oldK, const QString& newK) {
        if (oldK == newK || !map.contains(oldK)) return;
        map.insert(newK, map.take(oldK));
    };

    renameKey(m_modIds, trimmedOld, trimmedNew);
    renameListKey(m_modTags, trimmedOld, trimmedNew);
    renameKey(m_modDescriptions, trimmedOld, trimmedNew);

    const QString renamedId = m_modIds.value(trimmedNew);
    if (!renamedId.isEmpty()) {
        QMap<QString, QStringList> aliases;
        aliases[renamedId] = QStringList{trimmedOld, trimmedNew};
        mergeModAliases(aliases);
    }

    relinkNexusRenamedMod(trimmedOld, trimmedNew);
    emit modBundlesChanged();

    if (m_modImages.contains(trimmedOld)) {
        QString imgUrl = m_modImages.value(trimmedOld);
        QString localPath = QUrl(imgUrl).toLocalFile();
        if (localPath.isEmpty()) {
            localPath = imgUrl;
        }
        QFileInfo imgInfo(localPath);
        if (imgInfo.exists()) {
            QString localPathFixed = imgInfo.absoluteFilePath();

            QString newLocal = localPathFixed;
            const QString sep = QStringLiteral("/");
            const int lastSlash = newLocal.lastIndexOf(sep);
            QString parentDir;
            QString fileName;
            if (lastSlash >= 0) {
                parentDir = newLocal.left(lastSlash);
                fileName = newLocal.mid(lastSlash + 1);
            } else {
                parentDir = ".";
                fileName = newLocal;
            }
            const QString parentBase = QFileInfo(parentDir).fileName();

            QString newFileName = fileName;
            if (fileName.startsWith(trimmedOld, Qt::CaseInsensitive)) {
                newFileName = trimmedNew + fileName.mid(trimmedOld.length());
                const QString newFilePath = QDir(parentDir).filePath(newFileName);
                if (!QFile::exists(newFilePath)) {
                    if (!QFile::rename(imgInfo.absoluteFilePath(), newFilePath)) {
                        qDebug() << "renameMod: failed to rename image file";
                    } else {
                        localPathFixed = newFilePath;
                        if (parentBase == trimmedOld) {

                            QString newParentDir = parentDir;
                            int idx = newParentDir.lastIndexOf(sep);
                            if (idx > 0) {
                                newParentDir = newParentDir.left(idx) + sep + trimmedNew;
                            } else {
                                newParentDir = trimmedNew;
                            }
                            localPathFixed = QDir(newParentDir).filePath(newFileName);
                        }
                    }
                }
            } else if (parentBase == trimmedOld) {

                int idx = parentDir.lastIndexOf(sep);
                QString newParentDir;
                if (idx > 0) {
                    newParentDir = parentDir.left(idx) + sep + trimmedNew;
                } else {
                    newParentDir = trimmedNew;
                }
                QString candidatePath = QDir(newParentDir).filePath(fileName);
                if (QFileInfo::exists(candidatePath)) {
                    localPathFixed = candidatePath;
                }
            }
            m_modImages.insert(trimmedNew, QUrl::fromLocalFile(localPathFixed).toString());
        } else {
            m_modImages.insert(trimmedNew, imgUrl);
        }
        m_modImages.remove(trimmedOld);
    }

    if (m_favoriteMods.contains(trimmedOld)) {
        m_favoriteMods.remove(trimmedOld);
        m_favoriteMods.insert(trimmedNew);
    }

    for (int i = 0; i < m_collections.size(); ++i) {
        QVariantMap collection = m_collections[i].toMap();
        QStringList mods = collection.value("mods").toStringList();
        bool changed = false;
        for (int j = 0; j < mods.size(); ++j) {
            if (mods[j] == trimmedOld) {
                mods[j] = trimmedNew;
                changed = true;
            }
        }
        if (changed) {
            collection["mods"] = mods;
            m_collections[i] = collection;
        }
    }

    saveSettings();
    saveCollections();
    scanMods(m_modsPathInternal);

    m_batchOperationInProgress = wasBatchInProgress;

    emit modRenamed(trimmedOld, trimmedNew);
    qDebug() << "renameMod: renamed mod" << trimmedOld << "->" << trimmedNew;
}

void ModManager::setModImage(const QString& modName, const QString& imagePath) {

    QString localPath = QUrl(imagePath).toLocalFile();
    if (localPath.isEmpty()) {
        localPath = imagePath;
    }

    QString finalImagePath = localPath;

    QVariantMap details = getModDetails(modName);
    QString modPath = details.value("path").toString();
    if (!modPath.isEmpty()) {
        QFileInfo modInfo(modPath);
        QString folderPath;
        if (modInfo.isDir()) {
            folderPath = modPath;
        } else {
            folderPath = modInfo.absolutePath();
        }

        QFileInfo srcInfo(localPath);
        QString ext = srcInfo.suffix();
        if (ext.isEmpty()) {
            ext = "png";
        }

        QString destName = QStringLiteral("preview.%1").arg(ext);
        QString destPath = QDir(folderPath).filePath(destName);

        if (QFile::exists(destPath)) {
            QFile::remove(destPath);
        }

        if (QFile::copy(localPath, destPath)) {
            finalImagePath = destPath;
        } else {
            qDebug() << "Failed to copy mod image to" << destPath << "using" << localPath;
        }
    } else {
        qDebug() << "setModImage: could not resolve mod path for" << modName << "- keeping original image path only";
    }

    if (QFileInfo(finalImagePath).exists()) {
        m_modImages[modName] = QUrl::fromLocalFile(finalImagePath).toString();
    } else {
        m_modImages[modName] = finalImagePath;
    }
    saveSettings();
    emit modImageChanged(modName);
}

QString ModManager::getModImage(const QString& modName) {
    QString imagePath = m_modImages.value(modName, "");
    if (imagePath.isEmpty()) {
        return "";
    }

    QString localPath = QUrl(imagePath).toLocalFile();
    if (localPath.isEmpty()) {
        localPath = imagePath;
    }

    QFileInfo fileInfo(localPath);
    if (!fileInfo.exists() || !fileInfo.isReadable()) {

        qDebug() << "Removing stale image path for mod" << modName << ":" << imagePath;
        m_modImages.remove(modName);
        saveSettings();
        return "";
    }

    return imagePath;
}

void ModManager::removeModImageInternal(const QString& modName, bool deleteFile) {

    QString imagePath = m_modImages.value(modName, "");
    if (!imagePath.isEmpty()) {
        QString localPath = QUrl(imagePath).toLocalFile();
        if (localPath.isEmpty()) {
            localPath = imagePath;
        }
        QFileInfo info(localPath);
        if (info.exists() && info.isFile()) {

            if (deleteFile && !m_modsPathInternal.isEmpty() && localPath.startsWith(m_modsPathInternal)) {
                qDebug() << "removeModImage: deleting image file" << localPath << "for mod" << modName;
                if (!QFile::remove(localPath)) {
                    qDebug() << "removeModImage: FAILED to delete image file" << localPath;
                }
            } else if (!deleteFile) {
                qDebug() << "removeModImageInternal: skipping file delete for mod" << modName << "at" << localPath;
            } else {
                qDebug() << "removeModImage: not deleting external image file" << localPath
                         << "for mod" << modName << "(outside mods path)";
            }
        } else {
            qDebug() << "removeModImage: image file does not exist on disk" << localPath << "for mod" << modName;
        }
    } else {
        qDebug() << "removeModImage: no image path stored for mod" << modName;
    }

    m_modImages.remove(modName);
    saveSettings();
    emit modImageChanged(modName);
}

void ModManager::removeModImage(const QString& modName) {

    removeModImageInternal(modName, true);
}

void ModManager::addModTag(const QString& modName, const QString& tag) {
    if (tag.trimmed().isEmpty()) return;

    QStringList tags = m_modTags.value(modName, QStringList());
    QString trimmedTag = tag.trimmed();

    if (!tags.contains(trimmedTag, Qt::CaseInsensitive)) {
        tags.append(trimmedTag);
        m_modTags[modName] = tags;
        saveSettings();
        emit modTagsChanged(modName);
    }
}

void ModManager::removeModTag(const QString& modName, const QString& tag) {
    if (!m_modTags.contains(modName)) return;

    QStringList tags = m_modTags[modName];
    tags.removeAll(tag);

    if (tags.isEmpty()) {
        m_modTags.remove(modName);
    } else {
        m_modTags[modName] = tags;
    }

    saveSettings();
    emit modTagsChanged(modName);
}

QStringList ModManager::getModTags(const QString& modName) {
    return m_modTags.value(modName, QStringList());
}

void ModManager::setModDescription(const QString& modName, const QString& description) {
    const QString key = modName.trimmed();
    if (key.isEmpty()) {
        return;
    }

    const QString trimmed = description.trimmed();
    const QString current = m_modDescriptions.value(key, "");
    if (trimmed == current) {
        return;
    }

    if (trimmed.isEmpty()) {
        m_modDescriptions.remove(key);
    } else {
        m_modDescriptions[key] = trimmed;
    }

    saveSettings();
    emit modDescriptionChanged(key);
}

QString ModManager::getModDescription(const QString& modName) {
    return m_modDescriptions.value(modName, "");
}

bool ModManager::isModFavorite(const QString& modKey) const {
    return m_favoriteMods.contains(modKey);
}

void ModManager::setModFavorite(const QString& modKey, bool favorite) {
    if (modKey.isEmpty()) {
        return;
    }

    bool changed = false;
    if (favorite) {
        if (!m_favoriteMods.contains(modKey)) {
            m_favoriteMods.insert(modKey);
            changed = true;
        }
    } else {
        if (m_favoriteMods.remove(modKey)) {
            changed = true;
        }
    }

    if (!changed) {
        return;
    }

    saveSettings();
    emit modFavoriteChanged(modKey, favorite);
}

QString ModManager::findModImage(const QString& modFolderPath) const {
    QDir modDir(modFolderPath);

    QStringList imageExtensions = {"png", "jpg", "jpeg", "gif", "bmp", "webp"};

    QStringList preferredNames = {"preview", "thumbnail", "icon", "image", "cover"};

    QStringList allFiles = modDir.entryList(QDir::Files);

    for (const QString& fileName : allFiles) {
        QString baseName = QFileInfo(fileName).completeBaseName().toLower();
        QString extension = QFileInfo(fileName).suffix().toLower();

        if (imageExtensions.contains(extension)) {
            for (const QString& preferred : preferredNames) {
                if (baseName == preferred) {
                    QString fullPath = modDir.absoluteFilePath(fileName);
                    QFileInfo fileInfo(fullPath);
                    if (fileInfo.exists() && fileInfo.isReadable()) {
                        return QUrl::fromLocalFile(fullPath).toString();
                    }
                }
            }
        }
    }

    for (const QString& fileName : allFiles) {
        QString extension = QFileInfo(fileName).suffix().toLower();
        if (imageExtensions.contains(extension)) {
            QString fullPath = modDir.absoluteFilePath(fileName);
            QFileInfo fileInfo(fullPath);
            if (fileInfo.exists() && fileInfo.isReadable()) {
                return QUrl::fromLocalFile(fullPath).toString();
            }
        }
    }

    return QString();
}

void ModManager::cleanupStaleModData(const QSet<QString>& currentModNames)
{
    bool changed = false;

    QStringList imageKeysToRemove;
    for (auto it = m_modImages.constBegin(); it != m_modImages.constEnd(); ++it) {
        if (!currentModNames.contains(it.key())) {
            imageKeysToRemove.append(it.key());
        }
    }
    for (const QString& key : imageKeysToRemove) {
        qDebug() << "Removing stale mod image entry:" << key;
        m_modImages.remove(key);
        changed = true;
    }

    QStringList tagKeysToRemove;
    for (auto it = m_modTags.constBegin(); it != m_modTags.constEnd(); ++it) {
        if (!currentModNames.contains(it.key())) {
            tagKeysToRemove.append(it.key());
        }
    }
    for (const QString& key : tagKeysToRemove) {
        qDebug() << "Removing stale mod tags entry:" << key;
        m_modTags.remove(key);
        changed = true;
    }

    QStringList descriptionKeysToRemove;
    for (auto it = m_modDescriptions.constBegin(); it != m_modDescriptions.constEnd(); ++it) {
        if (!currentModNames.contains(it.key())) {
            descriptionKeysToRemove.append(it.key());
        }
    }
    for (const QString& key : descriptionKeysToRemove) {
        qDebug() << "Removing stale mod description entry:" << key;
        m_modDescriptions.remove(key);
        changed = true;
    }

    QSet<QString> bundledIds;
    for (const QVariant& bundleValue : m_modBundles) {
        for (const QVariant& member : bundleValue.toMap().value("members").toList()) {
            const QString id = member.toString();
            if (!id.isEmpty())
                bundledIds.insert(id);
        }
    }
    QStringList idKeysToRemove;
    for (auto it = m_modIds.constBegin(); it != m_modIds.constEnd(); ++it) {
        if (!currentModNames.contains(it.key()) && !bundledIds.contains(it.value())) {
            idKeysToRemove.append(it.key());
        }
    }
    for (const QString& key : idKeysToRemove) {
        qDebug() << "Removing stale mod ID entry:" << key;
        m_modIds.remove(key);
        changed = true;
    }

    QSet<QString> favoritesToRemove;
    for (const QString& favKey : m_favoriteMods) {
        if (!currentModNames.contains(favKey)) {
            favoritesToRemove.insert(favKey);
        }
    }
    for (const QString& key : favoritesToRemove) {
        qDebug() << "Removing stale favorite entry:" << key;
        m_favoriteMods.remove(key);
        changed = true;
    }

    QSet<QString> knownIds;
    for (auto it = m_modIds.constBegin(); it != m_modIds.constEnd(); ++it)
        knownIds.insert(it.value());
    for (const QVariant& presetValue : m_gamePresets) {
        const QVariantMap preset = presetValue.toMap();
        if (preset.value("id").toString() == m_activePresetId)
            continue;
        const QVariantMap presetIds = preset.value("modIds").toMap();
        for (auto it = presetIds.constBegin(); it != presetIds.constEnd(); ++it) {
            const QString id = it.value().toString();
            if (!id.isEmpty())
                knownIds.insert(id);
        }
    }
    reconcileNexusInstalledLinks(knownIds);
    pruneModAliases(knownIds);

    pruneModBundles();

    if (changed) {
        qDebug() << "Cleaned up stale mod data entries";
    }
}
