#include "ModManagerInternal.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUrl>

namespace {

QString normalizedDownloadKey(const QString& fileName)
{
    QString base = QFileInfo(fileName).completeBaseName().toLower();
    base.remove(QRegularExpression(QStringLiteral("\\s*\\(\\d+\\)$")));
    return base;
}

QString normalizedArchivePath(const QString& path)
{
    if (path.isEmpty())
        return {};
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    const QString absolute = canonical.isEmpty() ? info.absoluteFilePath() : canonical;
    return QDir::cleanPath(absolute).toLower();
}

QString trackedKey(const QString& gameDomain, int modId)
{
    return gameDomain.toLower() + QLatin1Char('/') + QString::number(modId);
}

QString normalizedModName(const QString& name)
{
    return mmdetail::normalizedModKey(name);
}

QString fileCategoryName(const QJsonObject& file)
{
    return file.value("category_name").toString("MAIN").toUpper();
}

bool isDeletedUpload(const QJsonObject& file)
{
    const QString category = fileCategoryName(file);
    return category == QLatin1String("ARCHIVED") || category == QLatin1String("REMOVED");
}

bool isInactiveUpload(const QJsonObject& file)
{
    const QString category = fileCategoryName(file);
    return category == QLatin1String("OLD_VERSION")
        || category == QLatin1String("ARCHIVED")
        || category == QLatin1String("REMOVED");
}

int trackedIndexByKey(const QVariantList& list, const QString& key)
{
    for (int i = 0; i < list.size(); ++i) {
        const QVariantMap entry = list.at(i).toMap();
        if (trackedKey(entry.value("gameDomain").toString(), entry.value("modId").toInt()) == key)
            return i;
    }
    return -1;
}

int archiveIndexForFile(const QVariantList& list, const QString& filePath,
                        const QString& fileName)
{
    const QString path = normalizedArchivePath(filePath);
    const QString name = fileName.toLower();

    if (!path.isEmpty()) {
        for (int i = 0; i < list.size(); ++i) {
            const QVariantMap record = list.at(i).toMap();
            if (normalizedArchivePath(record.value("archivePath").toString()) == path)
                return i;
        }
    }
    if (!name.isEmpty()) {
        for (int i = 0; i < list.size(); ++i) {
            if (list.at(i).toMap().value("fileName").toString().toLower() == name)
                return i;
        }
    }
    return -1;
}

int fileGroupIndex(const QVariantList& files, int fileId)
{
    for (int i = 0; i < files.size(); ++i) {
        if (files.at(i).toMap().value("fileId").toInt() == fileId)
            return i;
    }
    return -1;
}

QVariantMap updateInfo(const QVariantMap& entry, const QVariantMap& file)
{
    return {
        {"available", true},
        {"modId", entry.value("modId")},
        {"gameDomain", entry.value("gameDomain")},
        {"name", entry.value("name")},
        {"installedVersion", file.value("version")},
        {"latestVersion", file.value("latestVersion")},
        {"latestFileId", file.value("latestFileId")},
        {"latestFileName", file.value("latestFileName")},
    };
}

}

QString ModManager::nexusTrackedStoragePath() const
{
    return appDataPath() + QStringLiteral("/nexus_updates.json");
}

QString ModManager::nexusArchivesStoragePath() const
{
    return appDataPath() + QStringLiteral("/nexus_archives.json");
}

void ModManager::loadNexusTrackedMods()
{
    m_nexusTrackedMods.clear();

    QFile file(nexusTrackedStoragePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonArray mods =
        QJsonDocument::fromJson(file.readAll()).object().value("mods").toArray();

    bool migrated = false;
    for (const QJsonValue& value : mods) {
        QVariantMap entry = value.toObject().toVariantMap();

        const QString archivePath = entry.value("archivePath").toString();
        const int downloadedFileId = entry.value("downloadedFileId").toInt();
        if (!archivePath.isEmpty() || downloadedFileId > 0) {
            QVariantMap record;
            record["archivePath"] = archivePath;
            record["fileName"] = entry.value("downloadedFileName").toString();
            if (record.value("fileName").toString().isEmpty() && !archivePath.isEmpty())
                record["fileName"] = QFileInfo(archivePath).fileName();
            record["gameDomain"] = entry.value("gameDomain");
            record["modId"] = entry.value("modId");
            record["fileId"] = downloadedFileId > 0 ? QVariant(downloadedFileId)
                                                    : entry.value("fileId");
            record["version"] = entry.value("downloadedVersion").toString().isEmpty()
                ? entry.value("version") : entry.value("downloadedVersion");
            record["category"] = entry.value("downloadedCategory").toString().isEmpty()
                ? entry.value("category") : entry.value("downloadedCategory");
            record["name"] = entry.value("downloadedName").toString().isEmpty()
                ? entry.value("name") : entry.value("downloadedName");
            record["downloadedAt"] = entry.value("downloadedAt");
            if (!record.value("fileName").toString().isEmpty())
                mergeNexusArchiveRecord(record);

            for (const char* key : {"archivePath", "downloadedFileId", "downloadedFileName",
                                    "downloadedName", "downloadedVersion", "downloadedCategory",
                                    "downloadedAt"})
                entry.remove(QString::fromLatin1(key));
            migrated = true;
        }

        QVariantList files = entry.value("files").toList();
        if (files.isEmpty()) {

            QVariantMap group;
            group["fileId"] = entry.value("fileId");
            group["version"] = entry.value("version");
            group["category"] = entry.value("category");
            group["installedAt"] = entry.value("installedAt");
            group["installedMods"] = entry.value("installedMods");
            group["installedModIds"] = entry.value("installedModIds");
            group["latestFileId"] = entry.value("latestFileId");
            group["latestVersion"] = entry.value("latestVersion");
            group["latestFileName"] = entry.value("latestFileName");
            group["updateAvailable"] = entry.value("updateAvailable");
            group["lastChecked"] = entry.value("lastChecked");

            if (group.value("updateAvailable").toBool()
                && group.value("lastChecked").toLongLong() == 0
                && group.value("latestFileId").toInt() != group.value("fileId").toInt()) {
                group["latestFileId"] = group.value("fileId");
                group["latestVersion"] = entry.value("version");
                group["latestFileName"] = QString();
                group["updateAvailable"] = false;
            }

            if (group.value("installedMods").toList().isEmpty()
                && group.value("installedModIds").toList().isEmpty()) {
                migrated = true;
                continue;
            }

            files.append(group);
            for (const char* key : {"fileId", "version", "category", "installedAt",
                                    "installedMods", "installedModIds", "latestFileId",
                                    "latestVersion", "latestFileName", "updateAvailable",
                                    "lastChecked"})
                entry.remove(QString::fromLatin1(key));
            entry["files"] = files;
            migrated = true;
        } else {
            bool hasInstalled = false;
            for (const QVariant& fileValue : files) {
                const QVariantMap file = fileValue.toMap();
                if (!file.value("installedMods").toList().isEmpty()
                    || !file.value("installedModIds").toList().isEmpty()) {
                    hasInstalled = true;
                    break;
                }
            }
            if (!hasInstalled) {
                migrated = true;
                continue;
            }
        }

        m_nexusTrackedMods.append(entry);
    }

    if (migrated) {
        saveNexusArchives();
        saveNexusTrackedMods();
    }
}

void ModManager::saveNexusTrackedMods() const
{
    QJsonArray mods;
    for (const QVariant& value : m_nexusTrackedMods)
        mods.append(QJsonObject::fromVariantMap(value.toMap()));

    QJsonObject root;
    root["version"] = 4;
    root["mods"] = mods;

    QDir().mkpath(appDataPath());
    QFile file(nexusTrackedStoragePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void ModManager::loadNexusArchives()
{
    m_nexusArchives.clear();

    QFile file(nexusArchivesStoragePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonArray archives =
        QJsonDocument::fromJson(file.readAll()).object().value("archives").toArray();
    for (const QJsonValue& value : archives)
        m_nexusArchives.append(value.toObject().toVariantMap());

    if (pruneMissingNexusArchives())
        saveNexusArchives();
}

bool ModManager::pruneMissingNexusArchives()
{
    QVariantList kept;
    kept.reserve(m_nexusArchives.size());

    bool changed = false;
    for (const QVariant& value : m_nexusArchives) {
        const QVariantMap record = value.toMap();
        const QString archivePath = record.value("archivePath").toString();

        if (archivePath.isEmpty()) {
            kept.append(record);
            continue;
        }

        const QFileInfo archiveInfo(archivePath);

        if (archiveInfo.exists() || !QFileInfo(archiveInfo.absolutePath()).exists()) {
            kept.append(record);
            continue;
        }

        changed = true;
    }

    if (changed)
        m_nexusArchives = kept;
    return changed;
}

void ModManager::saveNexusArchives() const
{
    QJsonArray archives;
    for (const QVariant& value : m_nexusArchives)
        archives.append(QJsonObject::fromVariantMap(value.toMap()));

    QJsonObject root;
    root["version"] = 1;
    root["archives"] = archives;

    QDir().mkpath(appDataPath());
    QFile file(nexusArchivesStoragePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QVariantMap ModManager::nexusArchiveForFile(const QString& filePath) const
{
    if (filePath.isEmpty())
        return {};
    const int index = archiveIndexForFile(m_nexusArchives, filePath,
                                          QFileInfo(filePath).fileName());
    return index >= 0 ? m_nexusArchives.at(index).toMap() : QVariantMap();
}

QVariantMap ModManager::nexusArchiveFallbackForFile(const QString& filePath) const
{

    const QString fileName = QFileInfo(filePath).fileName();
    if (fileName.isEmpty())
        return {};

    for (const QVariant& value : m_nexusTrackedMods) {
        const QVariantMap entry = value.toMap();
        for (const QVariant& fileValue : entry.value("files").toList()) {
            const QVariantMap file = fileValue.toMap();
            if (file.value("latestFileName").toString().compare(fileName, Qt::CaseInsensitive) != 0)
                continue;
            const int fileId = file.value("latestFileId").toInt();
            if (fileId <= 0)
                continue;
            return {
                {"archivePath", QFileInfo(filePath).absoluteFilePath()},
                {"fileName", fileName},
                {"gameDomain", entry.value("gameDomain")},
                {"modId", entry.value("modId")},
                {"fileId", fileId},
                {"version", file.value("latestVersion")},
                {"category", QString()},
                {"name", entry.value("name")},
            };
        }
    }
    return {};
}

void ModManager::mergeNexusArchiveRecord(const QVariantMap& record)
{
    const QString path = record.value("archivePath").toString();
    const QString fileName = record.value("fileName").toString();
    const int index = archiveIndexForFile(m_nexusArchives, path, fileName);

    QVariantMap merged = index >= 0 ? m_nexusArchives.at(index).toMap() : QVariantMap();
    for (auto it = record.constBegin(); it != record.constEnd(); ++it)
        merged.insert(it.key(), it.value());

    if (index >= 0)
        m_nexusArchives[index] = merged;
    else
        m_nexusArchives.append(merged);
}

bool ModManager::hasNexusArchiveForFileId(const QString& gameDomain, int modId, int fileId) const
{
    if (modId <= 0 || fileId <= 0)
        return false;

    for (const QVariant& value : m_nexusArchives) {
        const QVariantMap record = value.toMap();
        if (record.value("modId").toInt() == modId
            && record.value("fileId").toInt() == fileId
            && record.value("gameDomain").toString().compare(gameDomain, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

QVariantList ModManager::nexusTrackedMods() const
{
    return m_nexusTrackedMods;
}

bool ModManager::isCheckingNexusUpdates() const
{
    return m_isCheckingNexusUpdates;
}

int ModManager::nexusUpdatesRevision() const
{
    return m_nexusUpdatesRevision;
}

int ModManager::nexusUpdatesAvailable() const
{
    int count = 0;
    for (const QVariant& value : m_nexusTrackedMods) {
        for (const QVariant& fileValue : value.toMap().value("files").toList()) {
            if (fileValue.toMap().value("updateAvailable").toBool()) {
                ++count;
                break;
            }
        }
    }
    return count;
}

void ModManager::queueNexusDownloadProvenance(const QString& gameDomain, int modId, int fileId,
                                              const QString& fileName, const QString& modName,
                                              const QString& version, const QString& category)
{
    if (modId <= 0 || fileName.trimmed().isEmpty())
        return;

    QVariantMap meta;
    meta["gameDomain"] = gameDomain.isEmpty() ? m_nexusGameDomain : gameDomain;
    meta["modId"] = modId;
    meta["fileId"] = fileId;
    meta["fileName"] = QFileInfo(fileName).fileName();
    meta["name"] = modName.isEmpty() ? QFileInfo(fileName).completeBaseName() : modName;
    meta["version"] = version;
    meta["category"] = category;
    m_pendingNexusDownloads.insert(normalizedDownloadKey(fileName), meta);
}

void ModManager::recordCompletedNexusDownload(const QString& filePath)
{
    const QString key = normalizedDownloadKey(QFileInfo(filePath).fileName());
    auto pending = m_pendingNexusDownloads.find(key);
    if (pending == m_pendingNexusDownloads.end())
        return;

    const QVariantMap meta = pending.value();
    m_pendingNexusDownloads.erase(pending);

    const QString gameDomain = meta.value("gameDomain").toString();
    const int modId = meta.value("modId").toInt();
    if (gameDomain.isEmpty() || modId <= 0)
        return;

    QVariantMap record;
    record["archivePath"] = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
    record["fileName"] = QFileInfo(filePath).fileName();
    record["size"] = QFileInfo(filePath).size();
    record["gameDomain"] = gameDomain;
    record["modId"] = modId;
    record["fileId"] = meta.value("fileId");
    record["version"] = meta.value("version");
    record["category"] = meta.value("category");
    record["name"] = meta.value("name");
    record["downloadedAt"] = QDateTime::currentSecsSinceEpoch();

    mergeNexusArchiveRecord(record);
    saveNexusArchives();

    ++m_nexusUpdatesRevision;
    emit nexusUpdatesRevisionChanged();
}

void ModManager::linkNexusInstalledMods(const QStringList& archives, const QStringList& installedMods)
{
    if (archives.isEmpty())
        return;

    bool changed = false;
    for (const QString& archive : archives) {
        QVariantMap record = nexusArchiveForFile(archive);
        if (record.isEmpty()) {
            record = nexusArchiveFallbackForFile(archive);
            if (!record.isEmpty()) {

                mergeNexusArchiveRecord(record);
                saveNexusArchives();
            }
        }
        if (record.isEmpty())
            continue;

        const QString gameDomain = record.value("gameDomain").toString();
        const int modId = record.value("modId").toInt();
        const int fileId = record.value("fileId").toInt();
        if (gameDomain.isEmpty() || modId <= 0 || fileId <= 0)
            continue;

        const QString identity = trackedKey(gameDomain, modId);
        const int index = trackedIndexByKey(m_nexusTrackedMods, identity);

        if (index < 0 && installedMods.isEmpty())
            continue;

        QVariantMap entry = index >= 0 ? m_nexusTrackedMods.at(index).toMap() : QVariantMap();
        if (index < 0) {
            entry["gameDomain"] = gameDomain;
            entry["modId"] = modId;
            entry["name"] = record.value("name");
            entry["files"] = QVariantList();
        }

        QVariantList files = entry.value("files").toList();
        const int fileIndex = fileGroupIndex(files, fileId);
        QVariantMap group = fileIndex >= 0 ? files.at(fileIndex).toMap() : QVariantMap();

        if (fileIndex < 0) {
            group["fileId"] = fileId;
            group["version"] = record.value("version");
            group["category"] = record.value("category");
            group["latestFileId"] = fileId;
            group["latestVersion"] = record.value("version");
            group["latestFileName"] = record.value("fileName");
            group["updateAvailable"] = false;
            group["lastChecked"] = 0;
            group["installedMods"] = QVariantList();
            group["installedModIds"] = QVariantList();
        }

        QStringList current;
        for (const QVariant& value : group.value("installedMods").toList())
            current.append(value.toString());

        for (const QString& name : installedMods) {
            if (name.trimmed().isEmpty())
                continue;
            bool exists = false;
            for (const QString& existing : current) {
                if (normalizedModName(existing) == normalizedModName(name)) {
                    exists = true;
                    break;
                }
            }
            if (!exists)
                current.append(name);
        }

        QStringList currentIds;
        for (const QVariant& value : group.value("installedModIds").toList())
            currentIds.append(value.toString());
        for (const QString& name : current) {
            const QString uniqueId = m_modIds.value(name);
            if (!uniqueId.isEmpty() && !currentIds.contains(uniqueId))
                currentIds.append(uniqueId);
        }

        if (current.isEmpty() && currentIds.isEmpty())
            continue;

        QVariantList storedNames;
        QSet<QString> linkedNameKeys;
        for (const QString& name : current) {
            storedNames.append(name);
            linkedNameKeys.insert(normalizedModName(name));
        }
        group["installedMods"] = storedNames;

        QVariantList storedIds;
        QSet<QString> linkedIds;
        for (const QString& id : currentIds) {
            storedIds.append(id);
            linkedIds.insert(id);
        }
        group["installedModIds"] = storedIds;

        group["fileId"] = fileId;
        group["version"] = record.value("version");
        group["category"] = record.value("category");
        group["installedAt"] = QDateTime::currentSecsSinceEpoch();
        const int latestFileId = group.value("latestFileId").toInt();
        group["updateAvailable"] = latestFileId > 0 && latestFileId != fileId;

        QSet<QString> movedIds = linkedIds;
        QVariantList newFiles;
        for (int i = 0; i < files.size(); ++i) {
            if (i == fileIndex) {
                newFiles.append(group);
                continue;
            }

            QVariantMap other = files.at(i).toMap();
            QVariantList otherNames;
            for (const QVariant& nameValue : other.value("installedMods").toList()) {
                const QString name = nameValue.toString();
                if (linkedNameKeys.contains(normalizedModName(name))) {
                    const QString id = m_modIds.value(name);
                    if (!id.isEmpty())
                        movedIds.insert(id);
                    continue;
                }
                otherNames.append(name);
            }
            QVariantList otherIds;
            for (const QVariant& idValue : other.value("installedModIds").toList()) {
                if (!movedIds.contains(idValue.toString()))
                    otherIds.append(idValue);
            }

            if (otherNames.size() == other.value("installedMods").toList().size()
                && otherIds.size() == other.value("installedModIds").toList().size()) {
                newFiles.append(other);
                continue;
            }
            if (otherNames.isEmpty() && otherIds.isEmpty())
                continue;
            other["installedMods"] = otherNames;
            other["installedModIds"] = otherIds;
            newFiles.append(other);
        }
        if (fileIndex < 0)
            newFiles.append(group);

        entry["files"] = newFiles;
        if (!record.value("name").toString().isEmpty())
            entry["name"] = record.value("name");

        if (index >= 0)
            m_nexusTrackedMods[index] = entry;
        else
            m_nexusTrackedMods.append(entry);
        changed = true;
    }

    if (!changed)
        return;

    saveNexusTrackedMods();
    ++m_nexusUpdatesRevision;
    emit nexusUpdatesRevisionChanged();
    emit nexusTrackedModsChanged();
}

void ModManager::relinkNexusRenamedMod(const QString& oldName, const QString& newName)
{
    const QString oldKey = normalizedModName(oldName);
    const QString newKey = normalizedModName(newName);
    if (oldKey.isEmpty() || newKey.isEmpty() || oldKey == newKey)
        return;

    bool changed = false;
    for (int i = 0; i < m_nexusTrackedMods.size(); ++i) {
        QVariantMap entry = m_nexusTrackedMods.at(i).toMap();
        QVariantList files = entry.value("files").toList();
        bool entryChanged = false;

        for (int f = 0; f < files.size(); ++f) {
            QVariantMap group = files.at(f).toMap();
            const QVariantList installed = group.value("installedMods").toList();

            QVariantList updated;
            bool matched = false;
            for (const QVariant& value : installed) {
                const QString name = value.toString();
                const QString key = normalizedModName(name);
                if (key == oldKey) {
                    if (!matched)
                        updated.append(newName);
                    matched = true;
                    continue;
                }
                if (key == newKey)
                    continue;
                updated.append(name);
            }

            if (!matched)
                continue;

            group["installedMods"] = updated;

            const QString uniqueId = m_modIds.value(newName);
            if (!uniqueId.isEmpty()) {
                QVariantList ids = group.value("installedModIds").toList();
                bool hasId = false;
                for (const QVariant& id : ids) {
                    if (id.toString() == uniqueId) {
                        hasId = true;
                        break;
                    }
                }
                if (!hasId)
                    ids.append(uniqueId);
                group["installedModIds"] = ids;
            }

            files[f] = group;
            entryChanged = true;
        }

        if (!entryChanged && !files.isEmpty()
            && normalizedModName(entry.value("name").toString()) == oldKey) {
            QVariantMap group = files.first().toMap();
            QVariantList names = group.value("installedMods").toList();
            names.append(newName);
            group["installedMods"] = names;
            const QString uniqueId = m_modIds.value(newName);
            if (!uniqueId.isEmpty()) {
                QVariantList ids = group.value("installedModIds").toList();
                ids.append(uniqueId);
                group["installedModIds"] = ids;
            }
            files[0] = group;
            entryChanged = true;
        }

        if (!entryChanged)
            continue;

        entry["files"] = files;
        m_nexusTrackedMods[i] = entry;
        changed = true;
    }

    if (!changed)
        return;

    saveNexusTrackedMods();
    ++m_nexusUpdatesRevision;
    emit nexusUpdatesRevisionChanged();
    emit nexusTrackedModsChanged();
}

void ModManager::unlinkNexusUninstalledMods(const QStringList& modNames,
                                            const QStringList& modIds)
{
    QSet<QString> removedKeys;
    for (const QString& name : modNames) {
        const QString key = normalizedModName(name);
        if (!key.isEmpty())
            removedKeys.insert(key);
    }
    QSet<QString> removedIds;
    for (const QString& id : modIds) {
        if (!id.isEmpty())
            removedIds.insert(id);
    }
    if (removedKeys.isEmpty() && removedIds.isEmpty())
        return;

    bool changed = false;
    QVariantList kept;
    for (const QVariant& value : m_nexusTrackedMods) {
        QVariantMap entry = value.toMap();
        QVariantList files = entry.value("files").toList();
        QVariantList keptFiles;
        bool entryChanged = false;

        for (const QVariant& fileValue : files) {
            QVariantMap file = fileValue.toMap();
            const QVariantList installed = file.value("installedMods").toList();
            const QVariantList installedIds = file.value("installedModIds").toList();

            QVariantList remaining;
            for (const QVariant& name : installed) {
                if (!removedKeys.contains(normalizedModName(name.toString())))
                    remaining.append(name);
            }

            QVariantList remainingIds;
            for (const QVariant& id : installedIds) {
                if (!removedIds.contains(id.toString()))
                    remainingIds.append(id);
            }

            if (remaining.size() == installed.size()
                && remainingIds.size() == installedIds.size()) {
                keptFiles.append(file);
                continue;
            }

            entryChanged = true;

            if (remaining.isEmpty() && remainingIds.isEmpty())
                continue;

            file["installedMods"] = remaining;
            file["installedModIds"] = remainingIds;
            keptFiles.append(file);
        }

        if (!entryChanged) {
            kept.append(entry);
            continue;
        }

        changed = true;
        if (keptFiles.isEmpty())
            continue;

        entry["files"] = keptFiles;
        kept.append(entry);
    }

    if (!changed)
        return;

    m_nexusTrackedMods = kept;
    saveNexusTrackedMods();
    ++m_nexusUpdatesRevision;
    emit nexusUpdatesRevisionChanged();
    emit nexusTrackedModsChanged();
}

QVariantMap ModManager::nexusUpdateForArchive(const QString& filePath) const
{
    QVariantMap record = nexusArchiveForFile(filePath);
    if (record.isEmpty())
        record = nexusArchiveFallbackForFile(filePath);
    if (record.isEmpty())
        return {};

    const QString gameDomain = record.value("gameDomain").toString();
    const int modId = record.value("modId").toInt();
    const int archiveFileId = record.value("fileId").toInt();
    if (gameDomain.isEmpty() || modId <= 0)
        return {};

    const int index = trackedIndexByKey(m_nexusTrackedMods, trackedKey(gameDomain, modId));
    if (index < 0)
        return {};

    const QVariantMap entry = m_nexusTrackedMods.at(index).toMap();
    const QVariantList files = entry.value("files").toList();
    const int fileIndex = fileGroupIndex(files, archiveFileId);
    if (fileIndex < 0)
        return {};

    const QVariantMap file = files.at(fileIndex).toMap();
    if (!file.value("updateAvailable").toBool())
        return {};

    const int latestFileId = file.value("latestFileId").toInt();
    if (latestFileId > 0 && hasNexusArchiveForFileId(gameDomain, modId, latestFileId))
        return {};

    return updateInfo(entry, file);
}

QVariantMap ModManager::nexusUpdateForMod(const QString& modName) const
{
    if (modName.trimmed().isEmpty())
        return {};

    const QString wanted = normalizedModName(modName);
    const QString wantedId = m_modIds.value(modName);

    for (const QVariant& value : m_nexusTrackedMods) {
        const QVariantMap entry = value.toMap();
        const QVariantList files = entry.value("files").toList();

        for (const QVariant& fileValue : files) {
            const QVariantMap file = fileValue.toMap();
            if (!file.value("updateAvailable").toBool())
                continue;

            bool matched = false;
            if (!wantedId.isEmpty()) {
                for (const QVariant& id : file.value("installedModIds").toList()) {
                    if (id.toString() == wantedId) {
                        matched = true;
                        break;
                    }
                }
            }
            if (!matched) {
                for (const QVariant& installed : file.value("installedMods").toList()) {
                    if (normalizedModName(installed.toString()) == wanted) {
                        matched = true;
                        break;
                    }
                }
            }
            if (matched)
                return updateInfo(entry, file);
        }

        if (normalizedModName(entry.value("name").toString()) == wanted) {
            for (const QVariant& fileValue : files) {
                const QVariantMap file = fileValue.toMap();
                if (file.value("updateAvailable").toBool())
                    return updateInfo(entry, file);
            }
        }
    }
    return {};
}

QVariantMap ModManager::nexusUpdateForBundle(const QString& bundleId) const
{
    if (bundleId.isEmpty())
        return {};

    for (const QVariant& bundleValue : m_modBundles) {
        const QVariantMap bundle = bundleValue.toMap();
        if (bundle.value("id").toString() != bundleId)
            continue;

        for (const QVariant& memberValue : bundle.value("members").toList()) {
            const QString memberId = memberValue.toString();
            if (memberId.isEmpty())
                continue;

            QString memberName;
            for (auto it = m_modIds.constBegin(); it != m_modIds.constEnd(); ++it) {
                if (it.value() == memberId) {
                    memberName = it.key();
                    break;
                }
            }
            if (memberName.isEmpty())
                continue;

            const QVariantMap update = nexusUpdateForMod(memberName);
            if (update.value("available").toBool())
                return update;
        }
        return {};
    }
    return {};
}

QVariantMap ModManager::nexusUpdateTargetForArchive(const QString& filePath) const
{
    QVariantMap record = nexusArchiveForFile(filePath);
    if (record.isEmpty())
        record = nexusArchiveFallbackForFile(filePath);
    if (record.isEmpty())
        return {};

    const QString gameDomain = record.value("gameDomain").toString();
    const int modId = record.value("modId").toInt();
    if (gameDomain.isEmpty() || modId <= 0)
        return {};

    const int index = trackedIndexByKey(m_nexusTrackedMods, trackedKey(gameDomain, modId));
    if (index < 0)
        return {};

    const QVariantMap entry = m_nexusTrackedMods.at(index).toMap();
    const int archiveFileId = record.value("fileId").toInt();

    QVariantMap updateFile;
    for (const QVariant& fileValue : entry.value("files").toList()) {
        const QVariantMap file = fileValue.toMap();
        if (file.value("updateAvailable").toBool()
            && file.value("latestFileId").toInt() == archiveFileId) {
            updateFile = file;
            break;
        }
    }
    if (updateFile.isEmpty())
        return {};

    QSet<QString> liveMods;
    for (const QVariant& modValue : m_modList)
        liveMods.insert(modValue.toMap().value("name").toString());

    QString target;
    for (const QVariant& id : updateFile.value("installedModIds").toList()) {
        for (auto it = m_modIds.constBegin(); it != m_modIds.constEnd(); ++it) {
            if (it.value() == id.toString() && liveMods.contains(it.key())) {
                target = it.key();
                break;
            }
        }
        if (!target.isEmpty())
            break;
    }

    if (target.isEmpty()) {
        for (const QVariant& nameValue : updateFile.value("installedMods").toList()) {
            const QString name = nameValue.toString();
            if (liveMods.contains(name)) {
                target = name;
                break;
            }
        }
    }

    if (target.isEmpty())
        return {};

    return {
        {"available", true},
        {"name", target},
        {"modId", entry.value("modId")},
        {"gameDomain", entry.value("gameDomain")},
        {"latestFileId", updateFile.value("latestFileId")},
        {"latestVersion", updateFile.value("latestVersion")},
    };
}

void ModManager::reconcileNexusInstalledLinks(const QSet<QString>& knownIds)
{
    if (m_nexusTrackedMods.isEmpty())
        return;

    bool changed = false;
    QVariantList kept;
    QStringList droppedIds;

    for (const QVariant& value : m_nexusTrackedMods) {
        QVariantMap entry = value.toMap();
        QVariantList files = entry.value("files").toList();
        QVariantList keptFiles;
        bool entryChanged = false;

        for (const QVariant& fileValue : files) {
            QVariantMap file = fileValue.toMap();
            const QVariantList ids = file.value("installedModIds").toList();
            const QVariantList names = file.value("installedMods").toList();

            QVariantList keptIds;
            for (const QVariant& id : ids) {
                if (knownIds.contains(id.toString()))
                    keptIds.append(id);
                else if (!id.toString().isEmpty())
                    droppedIds.append(id.toString());
            }

            QVariantList keptNames = names;
            if (!ids.isEmpty() && keptIds.isEmpty())
                keptNames.clear();

            if (keptIds.size() == ids.size() && keptNames.size() == names.size()) {
                keptFiles.append(file);
                continue;
            }

            entryChanged = true;
            if (keptIds.isEmpty() && keptNames.isEmpty())
                continue;

            file["installedModIds"] = keptIds;
            file["installedMods"] = keptNames;
            keptFiles.append(file);
        }

        if (!entryChanged) {
            kept.append(entry);
            continue;
        }

        changed = true;
        if (keptFiles.isEmpty())
            continue;

        entry["files"] = keptFiles;
        kept.append(entry);
    }

    if (!changed)
        return;

    m_nexusTrackedMods = kept;
    saveNexusTrackedMods();
    ++m_nexusUpdatesRevision;
    emit nexusUpdatesRevisionChanged();
    emit nexusTrackedModsChanged();

    for (const QString& id : droppedIds)
        forgetModAliases(id);
}

bool ModManager::isNexusUpdateDownload(const QString& gameDomain, int modId, int fileId) const
{
    if (modId <= 0 || fileId <= 0)
        return false;

    const QString domain = gameDomain.isEmpty() ? m_nexusGameDomain : gameDomain;
    const int index = trackedIndexByKey(m_nexusTrackedMods, trackedKey(domain, modId));
    if (index < 0)
        return false;

    const QVariantList files = m_nexusTrackedMods.at(index).toMap().value("files").toList();
    for (const QVariant& fileValue : files) {
        if (fileValue.toMap().value("fileId").toInt() == fileId)
            return false;
    }
    for (const QVariant& fileValue : files) {
        const QVariantMap file = fileValue.toMap();
        if (file.value("updateAvailable").toBool()
            && file.value("latestFileId").toInt() == fileId)
            return true;
    }
    return false;
}

void ModManager::checkNexusUpdates()
{
    if (m_isCheckingNexusUpdates || m_nexusTrackedMods.isEmpty())
        return;
    if (!hasNexusCredentials())
        return;

    m_isCheckingNexusUpdates = true;
    m_nexusUpdateCheckIndex = 0;
    emit isCheckingNexusUpdatesChanged();

    checkNextNexusUpdate(0);
}

void ModManager::checkNextNexusUpdate(int index)
{
    if (index >= m_nexusTrackedMods.size()) {
        saveNexusTrackedMods();
        m_isCheckingNexusUpdates = false;
        ++m_nexusUpdatesRevision;
        emit nexusUpdatesRevisionChanged();
        emit nexusTrackedModsChanged();
        emit isCheckingNexusUpdatesChanged();
        return;
    }

    const QVariantMap entry = m_nexusTrackedMods.at(index).toMap();
    const QString gameDomain = entry.value("gameDomain").toString();
    const int modId = entry.value("modId").toInt();
    if (gameDomain.isEmpty() || modId <= 0) {
        QTimer::singleShot(0, this, [this, index]() { checkNextNexusUpdate(index + 1); });
        return;
    }

    const QString identity = trackedKey(gameDomain, modId);
    const QUrl url(QStringLiteral("https://api.nexusmods.com/v1/games/%1/mods/%2/files.json")
                       .arg(gameDomain, QString::number(modId)));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    applyNexusAuthHeaders(request);

    QNetworkReply* reply = ensureNetworkManager()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, index, identity]() {
        reply->deleteLater();
        parseNexusRateLimitHeaders(reply);

        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonArray files = root.value("files").toArray();
            const QJsonArray fileUpdates = root.value("file_updates").toArray();

            const int trackedIndex = trackedIndexByKey(m_nexusTrackedMods, identity);
            if (trackedIndex >= 0) {
                QVariantMap tracked = m_nexusTrackedMods.at(trackedIndex).toMap();
                QVariantList trackedFiles = tracked.value("files").toList();

                QHash<int, QJsonObject> filesById;
                for (const QJsonValue& value : files) {
                    const QJsonObject file = value.toObject();
                    filesById.insert(file.value("file_id").toInt(), file);
                }

                QList<int> activeFiles;
                for (const QJsonValue& value : files) {
                    const QJsonObject file = value.toObject();
                    if (!isInactiveUpload(file))
                        activeFiles.append(file.value("file_id").toInt());
                }

                const qint64 now = QDateTime::currentSecsSinceEpoch();
                for (int f = 0; f < trackedFiles.size(); ++f) {
                    QVariantMap group = trackedFiles.at(f).toMap();
                    const int installedFileId = group.value("fileId").toInt();

                    int latestFileId = 0;
                    QJsonObject latestFile;

                    if (installedFileId > 0) {

                        QList<int> chain;
                        QSet<int> seen;
                        seen.insert(installedFileId);
                        int currentFileId = installedFileId;
                        for (int step = 0; step <= fileUpdates.size(); ++step) {
                            int nextFileId = 0;
                            for (const QJsonValue& value : fileUpdates) {
                                const QJsonObject update = value.toObject();
                                if (update.value("old_file_id").toInt() == currentFileId) {
                                    nextFileId = update.value("new_file_id").toInt();
                                    break;
                                }
                            }
                            if (nextFileId <= 0 || seen.contains(nextFileId))
                                break;
                            seen.insert(nextFileId);
                            chain.append(nextFileId);
                            currentFileId = nextFileId;
                        }

                        while (!chain.isEmpty()) {
                            const QJsonObject file = filesById.value(chain.last());
                            if (!file.isEmpty() && !isDeletedUpload(file))
                                break;
                            chain.removeLast();
                        }

                        if (!chain.isEmpty()) {
                            latestFileId = chain.last();
                            latestFile = filesById.value(latestFileId);
                        } else if (activeFiles.size() == 1
                                   && activeFiles.first() != installedFileId) {

                            latestFileId = activeFiles.first();
                            latestFile = filesById.value(latestFileId);
                        }
                    }

                    if (latestFileId > 0) {
                        group["latestFileId"] = latestFileId;
                        group["latestVersion"] = latestFile.value("version").toString();
                        group["latestFileName"] = latestFile.value("file_name").toString();
                        group["updateAvailable"] = latestFileId != installedFileId;
                    } else {
                        group["latestFileId"] = 0;
                        group["latestVersion"] = QString();
                        group["latestFileName"] = QString();
                        group["updateAvailable"] = false;
                    }
                    group["lastChecked"] = now;
                    trackedFiles[f] = group;
                }

                tracked["files"] = trackedFiles;
                m_nexusTrackedMods[trackedIndex] = tracked;

                ++m_nexusUpdatesRevision;
                emit nexusUpdatesRevisionChanged();
                emit nexusTrackedModsChanged();
            }
        }

        QTimer::singleShot(150, this, [this, index]() { checkNextNexusUpdate(index + 1); });
    });
}
