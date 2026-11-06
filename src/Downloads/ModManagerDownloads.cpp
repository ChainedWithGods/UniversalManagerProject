#include "ModManagerInternal.h"
#include "DownloadManager.h"

using namespace mmdetail;

void ModManager::onDownloadsDirectoryChanged() {
    qDebug() << "Downloads directory changed, rescanning...";
    scanDownloadedMods();
}

QString ModManager::getDownloadedFileType(const QString& extension) const {
    QString ext = extension.toLower();

    if (ext == "rar" || ext == "zip" || ext == "7z") {
        return "archive";
    }

    if (ext == "pak-x" || ext == "ucas-x" || ext == "utoc-x") {
        return "ue_mod_encrypted";
    }

    if (ext == "pak" || ext == "ucas" || ext == "utoc") {
        return "ue_mod";
    }

    return "unknown";
}

QVariantList ModManager::downloadedModsList() const {
    return m_downloadedModsList;
}

bool ModManager::isScanningDownloads() const {
    return m_isScanningDownloads;
}

int ModManager::downloadedModsCount() const {
    return m_downloadedModsList.count();
}

QString ModManager::storageUsed() const {
    return formatBytes(m_totalBytes);
}

void ModManager::scanDownloadedMods() {

    if (m_downloadsPath.isEmpty()) {
        qDebug() << "Downloads path not configured";
        m_downloadedModsList.clear();
        emit downloadedModsListChanged();
        emit downloadedModsCountChanged();
        emit downloadsScanFinished(false);
        return;
    }

    QDir downloadsDir(m_downloadsPath);
    if (!downloadsDir.exists()) {
        qDebug() << "Downloads path does not exist:" << m_downloadsPath;
        m_downloadedModsList.clear();
        emit downloadedModsListChanged();
        emit downloadedModsCountChanged();
        emit downloadsScanFinished(false);
        return;
    }

    m_isScanningDownloads = true;
    emit isScanningDownloadsChanged();

    qDebug() << "Scanning downloaded mods in:" << m_downloadsPath;

    QStringList supportedExtensions = {
        "rar", "zip", "7z",
        "pak", "ucas", "utoc",
        "pak-x", "ucas-x", "utoc-x"
    };

    QVariantList newList;

    QSet<QString> activeDownloadPaths;
    if (m_downloadManager) {
        const QVariantList activeTasks = m_downloadManager->snapshot();
        for (const QVariant& value : activeTasks) {
            const QString path = value.toMap().value("filePath").toString();
            if (!path.isEmpty())
                activeDownloadPaths.insert(normalizePathForCompare(path));
        }
    }

    struct ScanRoot {
        QString path;
        QString library;
    };
    QList<ScanRoot> roots;
    roots.append({m_downloadsPath, QStringLiteral("downloads")});

    const QString downloadsLibraryPath = downloadsDir.filePath(
        mmdetail::downloadLibraryFolderName(false));
    const QString updatesLibraryPath = downloadsDir.filePath(
        mmdetail::downloadLibraryFolderName(true));
    if (QDir(downloadsLibraryPath).exists())
        roots.append({downloadsLibraryPath, QStringLiteral("downloads")});
    if (QDir(updatesLibraryPath).exists())
        roots.append({updatesLibraryPath, QStringLiteral("updates")});

    for (const ScanRoot& root : roots) {
        if (m_downloadsWatcher && !m_downloadsWatcher->directories().contains(root.path))
            m_downloadsWatcher->addPath(root.path);

        const QFileInfoList fileList =
            QDir(root.path).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& fileInfo : fileList) {
            if (activeDownloadPaths.contains(normalizePathForCompare(fileInfo.absoluteFilePath())))
                continue;

            const QString fileName = fileInfo.fileName();
            QString extension = fileInfo.suffix().toLower();

            if (fileName.toLower().endsWith(".pak-x")) {
                extension = "pak-x";
            } else if (fileName.toLower().endsWith(".ucas-x")) {
                extension = "ucas-x";
            } else if (fileName.toLower().endsWith(".utoc-x")) {
                extension = "utoc-x";
            }

            if (!supportedExtensions.contains(extension))
                continue;

            QVariantMap fileData;
            fileData["name"] = fileName;
            fileData["path"] = fileInfo.absoluteFilePath();
            fileData["size"] = fileInfo.size();
            fileData["sizeFormatted"] = formatBytes(fileInfo.size());
            fileData["extension"] = extension;
            fileData["fileType"] = getDownloadedFileType(extension);
            fileData["library"] = root.library;
            fileData["modifiedDate"] = fileInfo.lastModified().toString("yyyy-MM-dd");
            fileData["modifiedTimestamp"] = fileInfo.lastModified().toSecsSinceEpoch();

            newList.append(fileData);
        }
    }

    std::sort(newList.begin(), newList.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap()["modifiedTimestamp"].toLongLong() > b.toMap()["modifiedTimestamp"].toLongLong();
    });

    m_downloadedModsList = newList;
    m_isScanningDownloads = false;

    if (pruneMissingNexusArchives()) {
        saveNexusArchives();
        ++m_nexusUpdatesRevision;
        emit nexusUpdatesRevisionChanged();
    }

    qDebug() << "Found" << m_downloadedModsList.count() << "downloaded mod files";

    emit isScanningDownloadsChanged();
    emit downloadedModsListChanged();
    emit downloadedModsCountChanged();
    emit downloadsScanFinished(true);
}

void ModManager::openDownloadsFolder() {
    if (m_downloadsPath.isEmpty()) {
        qWarning() << "Downloads path not configured";
        return;
    }

    QDir downloadsDir(m_downloadsPath);
    if (!downloadsDir.exists()) {
        qWarning() << "Downloads path does not exist:" << m_downloadsPath;
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadsPath));
}
