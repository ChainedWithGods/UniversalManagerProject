#include "ModManagerInternal.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QVector>

namespace {

constexpr auto kBundlesFileName = "mod_bundles.json";

QString sanitizedBundleFolder(const QString& raw)
{
    QString name = raw.trimmed();
    static const QRegularExpression invalidChars(QStringLiteral("[\\\\/:*?\"<>|\\r\\n\\t]"));
    name.remove(invalidChars);
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')))
        name.chop(1);
    if (name == QLatin1String(".") || name == QLatin1String(".."))
        return {};
    return name.trimmed();
}

QString normalizedFolder(const QString& path)
{
    QString value = QDir::cleanPath(path);
    value.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return value;
}

bool isFolderUnder(const QString& modFolderPath, const QString& bundleFolder)
{
    const QString modPath = normalizedFolder(modFolderPath);
    const QString bundlePath = normalizedFolder(bundleFolder);
    if (bundlePath.isEmpty())
        return false;
    return modPath.compare(bundlePath, Qt::CaseInsensitive) == 0
        || modPath.startsWith(bundlePath + QLatin1Char('/'), Qt::CaseInsensitive);
}

int bundleIndexById(const QVariantList& bundles, const QString& bundleId)
{
    for (int i = 0; i < bundles.size(); ++i) {
        if (bundles.at(i).toMap().value("id").toString() == bundleId)
            return i;
    }
    return -1;
}

QString bundleRootKey(const QVariantMap& bundle)
{
    return normalizedFolder(bundle.value("root").toString());
}

bool bundleInRoot(const QVariantMap& bundle, const QString& rootKey)
{
    const QString stored = bundleRootKey(bundle);
    return stored.isEmpty() || stored.compare(rootKey, Qt::CaseInsensitive) == 0;
}

int bundleIndexByRootFolder(const QVariantList& bundles, const QString& rootKey,
                            const QString& folder)
{
    const QString wanted = normalizedFolder(folder);
    for (int i = 0; i < bundles.size(); ++i) {
        const QVariantMap bundle = bundles.at(i).toMap();
        if (!bundleInRoot(bundle, rootKey))
            continue;
        if (normalizedFolder(bundle.value("folder").toString())
                .compare(wanted, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

QStringList bundleMemberNames(const QVariantMap& bundle)
{
    QStringList names;
    for (const QVariant& value : bundle.value("memberNames").toList()) {
        const QString name = value.toString();
        if (name.isEmpty())
            continue;
        bool seen = false;
        for (const QString& existing : names) {
            if (existing.compare(name, Qt::CaseInsensitive) == 0) {
                seen = true;
                break;
            }
        }
        if (!seen)
            names.append(name);
    }
    return names;
}

QStringList bundleMemberIds(const QVariantMap& bundle)
{
    QStringList ids;
    for (const QVariant& value : bundle.value("members").toList()) {
        const QString id = value.toString();
        if (!id.isEmpty() && !ids.contains(id))
            ids.append(id);
    }
    return ids;
}

}

QString ModManager::modBundlesStoragePath() const
{
    const int index = findPresetIndexById(m_activePresetId);
    if (index < 0)
        return {};

    QString presetName = m_gamePresets.at(index).toMap().value("name").toString();
    if (presetName.isEmpty())
        presetName = QStringLiteral("Default");

    return getPresetFolderPath(presetName) + QLatin1Char('/')
        + QLatin1String(kBundlesFileName);
}

void ModManager::loadModBundles()
{
    m_modBundles.clear();

    const QString path = modBundlesStoragePath();
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonArray stored =
        QJsonDocument::fromJson(file.readAll()).object().value("bundles").toArray();
    file.close();

    const QString currentRoot = normalizedFolder(
        m_modsPath.isEmpty() ? m_modsPathInternal : m_modsPath);
    bool needsSave = false;

    QSet<QString> seenIds;
    for (const QJsonValue& value : stored) {
        const QJsonObject object = value.toObject();
        QVariantMap bundle;

        QString id = object.value("id").toString();
        if (id.isEmpty() || seenIds.contains(id))
            id = mmdetail::createRandomModId();
        seenIds.insert(id);

        QString name = object.value("name").toString().trimmed();
        const QString folder = sanitizedBundleFolder(object.value("folder").toString());
        if (name.isEmpty())
            name = folder;
        if (folder.isEmpty() || name.isEmpty())
            continue;

        QVariantList members;
        QSet<QString> seenMembers;
        for (const QJsonValue& member : object.value("members").toArray()) {
            const QString memberId = member.toString();
            if (memberId.isEmpty() || seenMembers.contains(memberId))
                continue;
            seenMembers.insert(memberId);
            members.append(memberId);
        }

        QVariantList memberNames;
        QSet<QString> seenMemberNames;
        for (const QJsonValue& memberName : object.value("memberNames").toArray()) {
            const QString member = memberName.toString().trimmed();
            const QString key = member.toLower();
            if (member.isEmpty() || seenMemberNames.contains(key))
                continue;
            seenMemberNames.insert(key);
            memberNames.append(member);
        }
        if (members.isEmpty() && memberNames.isEmpty())
            continue;

        QString root = normalizedFolder(object.value("root").toString());
        if (root.isEmpty()) {

            root = currentRoot;
            needsSave = true;
        }

        bundle["id"] = id;
        bundle["name"] = name;
        bundle["folder"] = folder;
        bundle["root"] = root;
        bundle["members"] = members;
        bundle["memberNames"] = memberNames;
        bundle["source"] = object.value("source").toObject().toVariantMap();
        bundle["createdAt"] = object.value("createdAt").toInt();

        QVariantList tags;
        QSet<QString> seenTags;
        for (const QJsonValue& tagValue : object.value("tags").toArray()) {
            const QString tag = tagValue.toString().trimmed();
            const QString key = tag.toLower();
            if (tag.isEmpty() || seenTags.contains(key))
                continue;
            seenTags.insert(key);
            tags.append(tag);
        }
        bundle["tags"] = tags;

        const int existing = bundleIndexByRootFolder(m_modBundles, root, folder);
        if (existing >= 0) {
            QVariantMap merged = m_modBundles.at(existing).toMap();
            QStringList mergedIds = bundleMemberIds(merged);
            for (const QVariant& member : members) {
                const QString memberId = member.toString();
                if (!mergedIds.contains(memberId))
                    mergedIds.append(memberId);
            }
            QVariantList mergedMembers;
            for (const QString& memberId : mergedIds)
                mergedMembers.append(memberId);
            merged["members"] = mergedMembers;

            QStringList mergedNames = bundleMemberNames(merged);
            for (const QVariant& memberName : memberNames) {
                const QString member = memberName.toString();
                bool hasName = false;
                for (const QString& existingName : mergedNames) {
                    if (existingName.compare(member, Qt::CaseInsensitive) == 0) {
                        hasName = true;
                        break;
                    }
                }
                if (!hasName)
                    mergedNames.append(member);
            }
            QVariantList mergedMemberNames;
            for (const QString& member : mergedNames)
                mergedMemberNames.append(member);
            merged["memberNames"] = mergedMemberNames;

            QVariantList mergedTags = merged.value("tags").toList();
            for (const QVariant& tagValue : tags) {
                const QString tag = tagValue.toString();
                bool hasTag = false;
                for (const QVariant& existingTag : mergedTags) {
                    if (existingTag.toString().compare(tag, Qt::CaseInsensitive) == 0) {
                        hasTag = true;
                        break;
                    }
                }
                if (!hasTag)
                    mergedTags.append(tag);
            }
            merged["tags"] = mergedTags;

            m_modBundles[existing] = merged;
            continue;
        }

        m_modBundles.append(bundle);
    }

    if (needsSave)
        saveModBundles();

    emit modBundlesChanged();
}

void ModManager::saveModBundles() const
{
    const QString path = modBundlesStoragePath();
    if (path.isEmpty())
        return;

    const QString folder = QFileInfo(path).absolutePath();
    QDir().mkpath(folder);

    QJsonArray bundles;
    for (const QVariant& value : m_modBundles) {
        const QVariantMap bundle = value.toMap();
        QJsonObject object;
        object["id"] = bundle.value("id").toString();
        object["name"] = bundle.value("name").toString();
        object["folder"] = bundle.value("folder").toString();
        object["root"] = bundle.value("root").toString();
        object["createdAt"] = bundle.value("createdAt").toInt();

        QJsonArray members;
        for (const QVariant& member : bundle.value("members").toList())
            members.append(member.toString());
        object["members"] = members;

        QJsonArray memberNames;
        for (const QVariant& member : bundle.value("memberNames").toList())
            memberNames.append(member.toString());
        object["memberNames"] = memberNames;

        QJsonArray tags;
        for (const QVariant& tag : bundle.value("tags").toList())
            tags.append(tag.toString());
        object["tags"] = tags;
        object["source"] = QJsonObject::fromVariantMap(
            bundle.value("source").toMap());
        bundles.append(object);
    }

    QJsonObject root;
    root["version"] = 1;
    root["bundles"] = bundles;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void ModManager::copyModBundles(const QString& fromPresetName, const QString& toPresetName) const
{
    const QString sourceFolder = getPresetFolderPath(
        fromPresetName.isEmpty() ? QStringLiteral("Default") : fromPresetName);
    const QString targetFolder = getPresetFolderPath(toPresetName);
    QDir().mkpath(targetFolder);

    const QString source = sourceFolder + QLatin1Char('/') + QLatin1String(kBundlesFileName);
    if (!QFile::exists(source))
        return;

    const QString target = targetFolder + QLatin1Char('/') + QLatin1String(kBundlesFileName);
    QFile::remove(target);
    QFile::copy(source, target);
}

QVariantList ModManager::buildModBundleView() const
{
    QVariantList view;
    if (m_modBundles.isEmpty())
        return view;

    const QString rootKey = normalizedFolder(m_modsPathInternal);
    QSet<QString> liveNames;
    QHash<QString, QString> nameById;
    QHash<QString, QString> idByName;
    QHash<QString, QVariantMap> liveRows;
    for (const QVariant& modValue : m_modList) {
        const QVariantMap mod = modValue.toMap();
        const QString name = mod.value("name").toString();
        const QString id = m_modIds.value(name);
        liveNames.insert(name);
        liveRows.insert(name, mod);
        if (!id.isEmpty()) {
            nameById.insert(id, name);
            idByName.insert(name, id);
        }
    }

    for (const QVariant& value : m_modBundles) {
        const QVariantMap bundle = value.toMap();
        const QString folder = bundle.value("folder").toString();
        if (folder.isEmpty() || !bundleInRoot(bundle, rootKey))
            continue;

        QStringList memberNames;
        QSet<QString> seenMemberNames;
        for (const QString& name : bundleMemberNames(bundle)) {
            if (liveNames.contains(name) && !seenMemberNames.contains(name)) {
                seenMemberNames.insert(name);
                memberNames.append(name);
            }
        }
        for (const QString& memberId : bundleMemberIds(bundle)) {
            const QString name = nameById.value(memberId);
            if (!name.isEmpty() && !seenMemberNames.contains(name)) {
                seenMemberNames.insert(name);
                memberNames.append(name);
            }
        }

        int activeCount = 0;
        qint64 sizeBytes = 0;
        QString newestDate;
        QVariantList memberIds;

        for (const QString& name : memberNames) {
            const QString id = idByName.value(name);
            if (!id.isEmpty())
                memberIds.append(id);

            const QVariantMap row = liveRows.value(name);
            if (row.value("status").toString() == QLatin1String("Active"))
                ++activeCount;

            const QVariantMap details = m_modDetailsCache.value(name);
            sizeBytes += details.value("sizeBytes").toLongLong();
            const QString date = details.value("installDate").toString();
            if (date > newestDate)
                newestDate = date;
        }

        if (memberNames.isEmpty())
            continue;

        QVariantMap entry = bundle;
        entry["members"] = memberNames;
        entry["memberIds"] = memberIds;
        entry["memberCount"] = memberNames.size();
        entry["activeCount"] = activeCount;
        entry["sizeBytes"] = sizeBytes;
        entry["size"] = formatBytes(sizeBytes);
        entry["date"] = newestDate;
        entry["folderPath"] = QDir(m_modsPathInternal).filePath(folder);
        entry["tags"] = bundle.value("tags").toList();

        const QVariantMap source = bundle.value("source").toMap();
        const int modId = source.value("modId").toInt();
        entry["sourceLabel"] = modId > 0
            ? QStringLiteral("Nexus #%1").arg(modId)
            : QStringLiteral("Local");

        view.append(entry);
    }

    return view;
}

QVariantList ModManager::modBundles() const
{
    return buildModBundleView();
}

void ModManager::refreshBundleMembership()
{
    if (m_modList.isEmpty())
        return;

    const QString rootKey = normalizedFolder(m_modsPathInternal);
    QHash<QString, QString> bundleIdByMemberName;
    QHash<QString, QString> bundleIdByMemberId;
    for (const QVariant& value : m_modBundles) {
        const QVariantMap bundle = value.toMap();
        if (!bundleInRoot(bundle, rootKey))
            continue;
        const QString bundleId = bundle.value("id").toString();
        for (const QString& name : bundleMemberNames(bundle))
            bundleIdByMemberName.insert(name, bundleId);
        for (const QString& memberId : bundleMemberIds(bundle))
            bundleIdByMemberId.insert(memberId, bundleId);
    }

    bool changed = false;
    QVariantList updated = m_modList;
    for (QVariant& value : updated) {
        QVariantMap row = value.toMap();
        const QString name = row.value("name").toString();
        const QString uniqueId = row.value("uniqueId").toString();
        QString modId = m_modIds.value(name);
        if (modId.isEmpty() && !uniqueId.isEmpty())
            modId = m_modIds.value(uniqueId);
        QString bundleId = bundleIdByMemberName.value(name);
        if (bundleId.isEmpty())
            bundleId = bundleIdByMemberId.value(modId);
        if (row.value("bundleId").toString() == bundleId)
            continue;

        row["bundleId"] = bundleId;
        value = row;
        changed = true;

        auto detailsIt = m_modDetailsCache.find(name);
        if (detailsIt != m_modDetailsCache.end())
            (*detailsIt)["bundleId"] = bundleId;
    }

    if (changed) {
        m_modList = updated;
        emit modListChanged();
    }
}

void ModManager::pruneModBundles()
{
    const QString rootKey = normalizedFolder(m_modsPathInternal);

    QSet<QString> liveNames;
    QHash<QString, QString> idByName;
    QHash<QString, QString> nameById;
    for (const QVariant& modValue : m_modList) {
        const QVariantMap mod = modValue.toMap();
        const QString name = mod.value("name").toString();
        const QString uniqueId = mod.value("uniqueId").toString();
        liveNames.insert(name);
        if (!uniqueId.isEmpty())
            liveNames.insert(uniqueId);
        const QString id = m_modIds.value(name);
        if (!id.isEmpty()) {
            idByName.insert(name, id);
            nameById.insert(id, name);
        }
    }

    bool changed = false;
    QVariantList kept;
    kept.reserve(m_modBundles.size());

    for (const QVariant& value : m_modBundles) {
        QVariantMap bundle = value.toMap();

        if (bundleRootKey(bundle).isEmpty()) {
            bundle["root"] = rootKey;
            changed = true;
        }
        if (!bundleInRoot(bundle, rootKey)) {
            kept.append(bundle);
            continue;
        }

        QStringList memberNames;
        QSet<QString> seenNames;
        for (const QString& name : bundleMemberNames(bundle)) {
            if (liveNames.contains(name) && !seenNames.contains(name)) {
                seenNames.insert(name);
                memberNames.append(name);
            }
        }
        for (const QString& memberId : bundleMemberIds(bundle)) {
            const QString name = nameById.value(memberId);
            if (!name.isEmpty() && !seenNames.contains(name)) {
                seenNames.insert(name);
                memberNames.append(name);
            }
        }

        if (memberNames.isEmpty()) {
            changed = true;
            if (!m_modsPathInternal.isEmpty()) {
                const QString folderPath = QDir(m_modsPathInternal)
                    .filePath(bundle.value("folder").toString());
                QDir(folderPath).rmdir(folderPath);
            }
            continue;
        }

        QVariantList memberIds;
        QVariantList storedNames;
        for (const QString& name : memberNames) {
            storedNames.append(name);
            const QString id = idByName.value(name);
            if (!id.isEmpty())
                memberIds.append(id);
        }

        if (memberIds != bundle.value("members").toList()
                || storedNames != bundle.value("memberNames").toList()) {
            bundle["members"] = memberIds;
            bundle["memberNames"] = storedNames;
            changed = true;
        }
        kept.append(bundle);
    }

    if (!changed)
        return;

    m_modBundles = kept;
    saveModBundles();
    emit modBundlesChanged();
}

void ModManager::removeModIdsFromBundles(const QStringList& ids)
{
    if (ids.isEmpty() || m_modBundles.isEmpty())
        return;

    const QString rootKey = normalizedFolder(m_modsPathInternal);
    QSet<QString> removed;
    for (const QString& id : ids)
        removed.insert(id);
    bool changed = false;
    QVariantList kept;
    kept.reserve(m_modBundles.size());

    for (const QVariant& value : m_modBundles) {
        QVariantMap bundle = value.toMap();
        if (!bundleInRoot(bundle, rootKey)) {
            kept.append(bundle);
            continue;
        }

        const QStringList storedNames = bundleMemberNames(bundle);
        const QVariantList storedIds = bundle.value("members").toList();
        QVariantList keptIds;
        QVariantList keptNames;
        bool bundleChanged = false;

        for (int i = 0; i < storedIds.size(); ++i) {
            const QString id = storedIds.at(i).toString();
            if (removed.contains(id)) {
                bundleChanged = true;
                continue;
            }
            keptIds.append(id);
            if (i < storedNames.size())
                keptNames.append(storedNames.at(i));
        }

        if (keptIds.isEmpty()) {
            changed = true;
            if (!m_modsPathInternal.isEmpty()) {
                const QString folderPath = QDir(m_modsPathInternal)
                    .filePath(bundle.value("folder").toString());
                QDir(folderPath).rmdir(folderPath);
            }
            continue;
        }

        if (bundleChanged) {
            bundle["members"] = keptIds;
            bundle["memberNames"] = keptNames;
            kept.append(bundle);
            changed = true;
        } else {
            kept.append(bundle);
        }
    }

    if (!changed)
        return;

    m_modBundles = kept;
    saveModBundles();
    emit modBundlesChanged();
}

void ModManager::adoptBundleFoldersFromScan()
{
    if (m_modsPathInternal.isEmpty() || m_modList.isEmpty())
        return;

    const QString rootKey = normalizedFolder(m_modsPathInternal);
    QStringList topOrder;
    QHash<QString, QStringList> namesByTop;
    QHash<QString, QStringList> idsByTop;
    QSet<QString> modsAtTop;

    for (const QVariant& modValue : m_modList) {
        const QVariantMap mod = modValue.toMap();
        if (mod.value("type").toString() != QLatin1String("folder"))
            continue;

        const QString relative = normalizedFolder(
            mod.value("folderPath").toString());
        if (relative.isEmpty())
            continue;

        const int slash = relative.indexOf(QLatin1Char('/'));
        if (slash <= 0) {
            modsAtTop.insert(relative);
            continue;
        }

        const QString top = relative.left(slash);
        const QString name = mod.value("name").toString();
        const QString id = m_modIds.value(name);
        if (name.isEmpty() || id.isEmpty())
            continue;

        if (!namesByTop.contains(top)) {
            topOrder.append(top);
            namesByTop.insert(top, QStringList());
            idsByTop.insert(top, QStringList());
        }
        QStringList& names = namesByTop[top];
        if (names.contains(name))
            continue;
        names.append(name);
        idsByTop[top].append(id);
    }

    bool changed = false;
    for (const QString& top : topOrder) {
        if (modsAtTop.contains(top))
            continue;
        const QStringList names = namesByTop.value(top);
        if (names.size() < 2)
            continue;
        if (bundleIndexByRootFolder(m_modBundles, rootKey, top) >= 0)
            continue;

        QVariantMap bundle;
        bundle["id"] = mmdetail::createRandomModId();
        bundle["name"] = top;
        bundle["folder"] = top;
        bundle["root"] = rootKey;
        bundle["source"] = QVariantMap();
        bundle["tags"] = QVariantList();
        bundle["createdAt"] = QDateTime::currentSecsSinceEpoch();

        QVariantList memberIds;
        QVariantList memberNames;
        const QStringList ids = idsByTop.value(top);
        for (int i = 0; i < names.size(); ++i) {
            memberNames.append(names.at(i));
            memberIds.append(ids.value(i));
        }
        bundle["members"] = memberIds;
        bundle["memberNames"] = memberNames;
        m_modBundles.append(bundle);
        changed = true;
    }

    if (!changed)
        return;

    refreshBundleMembership();
    saveModBundles();
    emit modBundlesChanged();
}

QString ModManager::createBundleFromInstalledFolder(const QString& folder,
                                                    const QString& archivePath)
{
    const QString bundleFolder = sanitizedBundleFolder(folder);
    if (bundleFolder.isEmpty() || m_modsPathInternal.isEmpty())
        return {};

    const QString rootKey = normalizedFolder(m_modsPathInternal);
    QStringList members;
    QStringList memberNames;
    QSet<QString> memberIds;
    for (const QVariant& modValue : m_modList) {
        const QVariantMap mod = modValue.toMap();
        if (mod.value("type").toString() != QLatin1String("folder"))
            continue;
        if (!isFolderUnder(mod.value("folderPath").toString(), bundleFolder))
            continue;

        const QString name = mod.value("name").toString();
        QString id = m_modIds.value(name);
        if (id.isEmpty()) {
            id = mmdetail::createRandomModId();
            m_modIds[name] = id;
        }
        if (memberIds.contains(id))
            continue;
        memberIds.insert(id);
        members.append(id);
        memberNames.append(name);
    }

    if (members.isEmpty())
        return {};

    int index = bundleIndexByRootFolder(m_modBundles, rootKey, bundleFolder);
    QVariantMap bundle = index >= 0 ? m_modBundles.at(index).toMap() : QVariantMap();

    if (index < 0) {
        bundle["id"] = mmdetail::createRandomModId();
        bundle["folder"] = bundleFolder;
        bundle["name"] = bundleFolder;
        bundle["members"] = QVariantList();
        bundle["memberNames"] = QVariantList();
        bundle["tags"] = QVariantList();
        bundle["source"] = QVariantMap();
        bundle["createdAt"] = QDateTime::currentSecsSinceEpoch();
    }
    bundle["root"] = rootKey;

    QVariantMap source = bundle.value("source").toMap();
    const QVariantMap archive = nexusArchiveForFile(archivePath);
    if (!archive.isEmpty()) {
        source["gameDomain"] = archive.value("gameDomain").toString();
        source["modId"] = archive.value("modId").toInt();
        QVariantList fileIds = source.value("fileIds").toList();
        const int fileId = archive.value("fileId").toInt();
        bool hasFileId = false;
        for (const QVariant& value : fileIds) {
            if (value.toInt() == fileId) {
                hasFileId = true;
                break;
            }
        }
        if (fileId > 0 && !hasFileId)
            fileIds.append(fileId);
        source["fileIds"] = fileIds;
    }
    bundle["source"] = source;

    QStringList merged = bundleMemberIds(bundle);
    QStringList mergedNames = bundleMemberNames(bundle);
    for (int i = 0; i < members.size(); ++i) {
        if (merged.contains(members.at(i)))
            continue;
        merged.append(members.at(i));
        mergedNames.append(memberNames.value(i));
    }
    QVariantList storedMembers;
    for (const QString& id : merged)
        storedMembers.append(id);
    bundle["members"] = storedMembers;
    QVariantList storedMemberNames;
    for (const QString& name : mergedNames)
        storedMemberNames.append(name);
    bundle["memberNames"] = storedMemberNames;

    if (index >= 0)
        m_modBundles[index] = bundle;
    else
        m_modBundles.append(bundle);

    refreshBundleMembership();
    saveModBundles();
    emit modBundlesChanged();
    return bundle.value("id").toString();
}

QString ModManager::groupModsIntoBundle(const QStringList& modNames,
                                        const QString& bundleName)
{
    if (m_modsPathInternal.isEmpty())
        return QStringLiteral("The Mods folder is not configured.");

    const QString targetName = sanitizedBundleFolder(bundleName);
    if (targetName.isEmpty())
        return QStringLiteral("Enter a name for the bundle.");

    QStringList names = modNames;
    names.removeDuplicates();
    names.removeAll(QString());
    if (names.isEmpty())
        return QStringLiteral("Select at least one mod to create a bundle.");

    const QDir modsRoot(m_modsPathInternal);
    const QString rootKey = normalizedFolder(m_modsPathInternal);
    const QString modsRootPath = QDir::cleanPath(modsRoot.absolutePath());
    const QString targetPath = modsRoot.filePath(targetName);
    const int existingBundleIndex = bundleIndexByRootFolder(m_modBundles, rootKey, targetName);

    if (QFileInfo::exists(targetPath) && existingBundleIndex < 0)
        return QStringLiteral("A folder named \"%1\" already exists and is not a bundle.").arg(targetName);

    QHash<QString, QVariantMap> rowsByName;
    for (const QVariant& modValue : m_modList) {
        const QVariantMap row = modValue.toMap();
        rowsByName.insert(row.value("name").toString(), row);
    }

    QSet<QString> memberIds;
    for (const QVariant& value : m_modBundles) {
        const QVariantMap bundle = value.toMap();
        if (!bundleInRoot(bundle, rootKey))
            continue;
        for (const QString& id : bundleMemberIds(bundle))
            memberIds.insert(id);
    }

    struct PlannedMove {
        QString name;
        QString sourcePath;
        QString folderName;
        QString id;
    };
    QVector<PlannedMove> moves;

    for (const QString& name : names) {
        const QVariantMap row = rowsByName.value(name);
        if (row.isEmpty())
            return QStringLiteral("\"%1\" is no longer installed.").arg(name);
        if (row.value("type").toString() != QLatin1String("folder"))
            return QStringLiteral("\"%1\" is a loose file. Bundles only hold folder mods.").arg(name);

        const QString folderPath = QDir(modsRoot).filePath(row.value("folderPath").toString());
        const QFileInfo folderInfo(folderPath);
        if (!folderInfo.exists() || !folderInfo.isDir())
            return QStringLiteral("\"%1\" folder is missing.").arg(name);
        if (QDir::cleanPath(folderInfo.absolutePath())
                .compare(modsRootPath, Qt::CaseInsensitive) != 0)
            return QStringLiteral("\"%1\" is already inside another folder.").arg(name);

        const QString id = m_modIds.value(name);
        if (!id.isEmpty() && memberIds.contains(id))
            return QStringLiteral("\"%1\" is already in a bundle.").arg(name);

        const QString folderName = folderInfo.fileName();
        if (folderName.compare(targetName, Qt::CaseInsensitive) == 0)
            return QStringLiteral("The bundle name cannot match a member folder.");

        const QString destination = QDir(targetPath).filePath(folderName);
        if (QFileInfo::exists(destination))
            return QStringLiteral("\"%1\" already exists inside the bundle folder.").arg(folderName);

        PlannedMove move;
        move.name = name;
        move.sourcePath = folderInfo.absoluteFilePath();
        move.folderName = folderName;
        move.id = id;
        moves.append(move);
    }

    if (!modsRoot.mkpath(targetName))
        return QStringLiteral("Could not create the bundle folder.");

    const bool wasBatchOperation = m_batchOperationInProgress;
    m_batchOperationInProgress = true;

    QStringList movedFolders;
    for (const PlannedMove& move : moves) {
        const QString destination = QDir(targetPath).filePath(move.folderName);
        if (!QDir().rename(move.sourcePath, destination)) {
            for (const QString& folder : movedFolders) {
                const QString rollback = QDir(targetPath).filePath(folder);
                QDir().rename(rollback, modsRoot.filePath(folder));
            }
            QDir(modsRoot.filePath(targetName)).rmdir(modsRoot.filePath(targetName));
            m_batchOperationInProgress = wasBatchOperation;
            return QStringLiteral("Could not move \"%1\" into the bundle folder.").arg(move.folderName);
        }
        movedFolders.append(move.folderName);
    }

    m_modsWatcherIgnoreUntilMs = QDateTime::currentMSecsSinceEpoch() + 1000;
    scanMods(m_modsPathInternal);
    m_batchOperationInProgress = wasBatchOperation;

    QStringList ids;
    for (const QString& name : names) {
        QString id = m_modIds.value(name);
        if (id.isEmpty()) {
            id = mmdetail::createRandomModId();
            m_modIds[name] = id;
        }
        ids.append(id);
    }

    int index = bundleIndexByRootFolder(m_modBundles, rootKey, targetName);
    QVariantMap bundle = index >= 0 ? m_modBundles.at(index).toMap() : QVariantMap();
    if (index < 0) {
        bundle["id"] = mmdetail::createRandomModId();
        bundle["folder"] = targetName;
        bundle["name"] = targetName;
        bundle["members"] = QVariantList();
        bundle["memberNames"] = QVariantList();
        bundle["source"] = QVariantMap();
        bundle["tags"] = QVariantList();
        bundle["createdAt"] = QDateTime::currentSecsSinceEpoch();
    }
    bundle["root"] = rootKey;

    QStringList merged = bundleMemberIds(bundle);
    QStringList mergedNames = bundleMemberNames(bundle);
    for (int i = 0; i < ids.size(); ++i) {
        if (merged.contains(ids.at(i)))
            continue;
        merged.append(ids.at(i));
        mergedNames.append(names.value(i));
    }
    QVariantList storedMembers;
    for (const QString& id : merged)
        storedMembers.append(id);
    bundle["members"] = storedMembers;
    QVariantList storedMemberNames;
    for (const QString& name : mergedNames)
        storedMemberNames.append(name);
    bundle["memberNames"] = storedMemberNames;

    if (bundle.value("source").toMap().isEmpty()) {
        QVariantMap source;
        bool found = false;
        for (const QVariant& trackedValue : m_nexusTrackedMods) {
            const QVariantMap tracked = trackedValue.toMap();
            for (const QVariant& fileValue : tracked.value("files").toList()) {
                const QVariantMap file = fileValue.toMap();
                for (const QVariant& idValue : file.value("installedModIds").toList()) {
                    if (!ids.contains(idValue.toString()))
                        continue;
                    source["gameDomain"] = tracked.value("gameDomain").toString();
                    source["modId"] = tracked.value("modId").toInt();
                    QVariantList fileIds = source.value("fileIds").toList();
                    const int fileId = file.value("fileId").toInt();
                    if (fileId > 0 && !fileIds.contains(fileId))
                        fileIds.append(fileId);
                    source["fileIds"] = fileIds;
                    found = true;
                    break;
                }
                if (found)
                    break;
            }
            if (found)
                break;
        }
        if (found)
            bundle["source"] = source;
    }

    if (index >= 0)
        m_modBundles[index] = bundle;
    else
        m_modBundles.append(bundle);

    refreshBundleMembership();
    saveModBundles();
    emit modBundlesChanged();
    return {};
}

QString ModManager::addModsToBundle(const QString& bundleId, const QStringList& modNames)
{
    if (m_modsPathInternal.isEmpty())
        return QStringLiteral("The Mods folder is not configured.");

    const QString rootKey = normalizedFolder(m_modsPathInternal);
    int index = bundleIndexById(m_modBundles, bundleId);
    if (index < 0)
        return QStringLiteral("Bundle not found.");

    const QVariantMap originalBundle = m_modBundles.at(index).toMap();
    if (!bundleInRoot(originalBundle, rootKey))
        return QStringLiteral("This bundle does not belong to the active Mods folder.");

    const QString bundleFolder = originalBundle.value("folder").toString();
    if (bundleFolder.isEmpty())
        return QStringLiteral("The bundle folder is missing.");

    const QDir modsRoot(m_modsPathInternal);
    const QString modsRootPath = QDir::cleanPath(modsRoot.absolutePath());
    const QString bundlePath = QDir::cleanPath(modsRoot.filePath(bundleFolder));
    if (!QFileInfo(bundlePath).isDir())
        return QStringLiteral("The bundle folder no longer exists.");

    QStringList names = modNames;
    names.removeDuplicates();
    names.removeAll(QString());
    if (names.isEmpty())
        return QStringLiteral("Select at least one mod.");

    QHash<QString, QVariantMap> rowsByName;
    for (const QVariant& modValue : m_modList) {
        const QVariantMap row = modValue.toMap();
        rowsByName.insert(row.value("name").toString(), row);
    }

    struct PlannedMove {
        QString name;
        QString sourcePath;
        QString folderName;
    };
    QVector<PlannedMove> moves;

    for (const QString& name : names) {
        const QVariantMap row = rowsByName.value(name);
        if (row.isEmpty())
            return QStringLiteral("\"%1\" is no longer installed.").arg(name);
        if (row.value("type").toString() != QLatin1String("folder"))
            return QStringLiteral("\"%1\" is a loose file. Bundles only hold folder mods.").arg(name);

        const QString folderPath = modsRoot.filePath(row.value("folderPath").toString());
        const QFileInfo folderInfo(folderPath);
        if (!folderInfo.exists() || !folderInfo.isDir())
            return QStringLiteral("\"%1\" folder is missing.").arg(name);

        const QString parentPath = QDir::cleanPath(folderInfo.absolutePath());
        if (parentPath.compare(bundlePath, Qt::CaseInsensitive) == 0)
            continue;
        if (parentPath.compare(modsRootPath, Qt::CaseInsensitive) != 0)
            return QStringLiteral("\"%1\" is already inside another folder.").arg(name);

        const QString destination = QDir(bundlePath).filePath(folderInfo.fileName());
        if (QFileInfo::exists(destination))
            return QStringLiteral("\"%1\" already exists inside the bundle folder.").arg(folderInfo.fileName());

        PlannedMove move;
        move.name = name;
        move.sourcePath = folderInfo.absoluteFilePath();
        move.folderName = folderInfo.fileName();
        moves.append(move);
    }

    if (moves.isEmpty())
        return {};

    const bool wasBatchOperation = m_batchOperationInProgress;
    m_batchOperationInProgress = true;

    QStringList movedFolders;
    for (const PlannedMove& move : moves) {
        const QString destination = QDir(bundlePath).filePath(move.folderName);
        if (!QDir().rename(move.sourcePath, destination)) {
            for (const QString& folder : movedFolders)
                QDir().rename(QDir(bundlePath).filePath(folder), modsRoot.filePath(folder));
            m_batchOperationInProgress = wasBatchOperation;
            return QStringLiteral("Could not move \"%1\" into the bundle folder.").arg(move.folderName);
        }
        movedFolders.append(move.folderName);
    }

    m_modsWatcherIgnoreUntilMs = QDateTime::currentMSecsSinceEpoch() + 1000;
    scanMods(m_modsPathInternal);
    m_batchOperationInProgress = wasBatchOperation;

    QStringList ids;
    for (const PlannedMove& move : moves) {
        QString id = m_modIds.value(move.name);
        if (id.isEmpty()) {
            id = mmdetail::createRandomModId();
            m_modIds[move.name] = id;
        }
        ids.append(id);
    }

    index = bundleIndexById(m_modBundles, bundleId);
    QVariantMap updatedBundle = index >= 0 ? m_modBundles.at(index).toMap() : originalBundle;
    QStringList mergedIds = bundleMemberIds(updatedBundle);
    QStringList mergedNames = bundleMemberNames(updatedBundle);
    for (int i = 0; i < moves.size(); ++i) {
        if (mergedIds.contains(ids.at(i)))
            continue;
        mergedIds.append(ids.at(i));
        mergedNames.append(moves.at(i).name);
    }

    QVariantList storedIds;
    for (const QString& id : mergedIds)
        storedIds.append(id);
    QVariantList storedNames;
    for (const QString& name : mergedNames)
        storedNames.append(name);
    updatedBundle["members"] = storedIds;
    updatedBundle["memberNames"] = storedNames;
    if (index >= 0)
        m_modBundles[index] = updatedBundle;
    else
        m_modBundles.append(updatedBundle);

    refreshBundleMembership();
    saveModBundles();
    emit modBundlesChanged();
    return {};
}

QString ModManager::removeModsFromBundle(const QString& bundleId, const QStringList& modNames)
{
    if (m_modsPathInternal.isEmpty())
        return QStringLiteral("The Mods folder is not configured.");

    const QString rootKey = normalizedFolder(m_modsPathInternal);
    int index = bundleIndexById(m_modBundles, bundleId);
    if (index < 0)
        return QStringLiteral("Bundle not found.");

    const QVariantMap originalBundle = m_modBundles.at(index).toMap();
    if (!bundleInRoot(originalBundle, rootKey))
        return QStringLiteral("This bundle does not belong to the active Mods folder.");

    const QString bundleFolder = originalBundle.value("folder").toString();
    if (bundleFolder.isEmpty())
        return QStringLiteral("The bundle folder is missing.");

    const QDir modsRoot(m_modsPathInternal);
    const QString modsRootPath = QDir::cleanPath(modsRoot.absolutePath());
    const QString bundlePath = QDir::cleanPath(modsRoot.filePath(bundleFolder));
    if (!QFileInfo(bundlePath).isDir())
        return QStringLiteral("The bundle folder no longer exists.");

    QStringList names = modNames;
    names.removeDuplicates();
    names.removeAll(QString());
    if (names.isEmpty())
        return QStringLiteral("Select at least one mod.");

    QHash<QString, QVariantMap> rowsByName;
    for (const QVariant& modValue : m_modList) {
        const QVariantMap row = modValue.toMap();
        rowsByName.insert(row.value("name").toString(), row);
    }

    struct PlannedMove {
        QString name;
        QString sourcePath;
        QString folderName;
    };
    QVector<PlannedMove> moves;

    for (const QString& name : names) {
        const QVariantMap row = rowsByName.value(name);
        if (row.isEmpty())
            return QStringLiteral("\"%1\" is no longer installed.").arg(name);

        const QString folderPath = modsRoot.filePath(row.value("folderPath").toString());
        const QFileInfo folderInfo(folderPath);
        if (!folderInfo.exists() || !folderInfo.isDir())
            return QStringLiteral("\"%1\" folder is missing.").arg(name);

        const QString parentPath = QDir::cleanPath(folderInfo.absolutePath());
        if (parentPath.compare(modsRootPath, Qt::CaseInsensitive) == 0)
            continue;
        if (parentPath.compare(bundlePath, Qt::CaseInsensitive) != 0)
            return QStringLiteral("\"%1\" is not inside this bundle.").arg(name);

        const QString destination = modsRoot.filePath(folderInfo.fileName());
        if (QFileInfo::exists(destination))
            return QStringLiteral("\"%1\" already exists in the Mods folder.").arg(folderInfo.fileName());

        PlannedMove move;
        move.name = name;
        move.sourcePath = folderInfo.absoluteFilePath();
        move.folderName = folderInfo.fileName();
        moves.append(move);
    }

    if (moves.isEmpty())
        return {};

    const bool wasBatchOperation = m_batchOperationInProgress;
    m_batchOperationInProgress = true;

    QStringList movedFolders;
    for (const PlannedMove& move : moves) {
        const QString destination = modsRoot.filePath(move.folderName);
        if (!QDir().rename(move.sourcePath, destination)) {
            for (const QString& folder : movedFolders)
                QDir().rename(modsRoot.filePath(folder), QDir(bundlePath).filePath(folder));
            m_batchOperationInProgress = wasBatchOperation;
            return QStringLiteral("Could not move \"%1\" out of the bundle folder.").arg(move.folderName);
        }
        movedFolders.append(move.folderName);
    }

    m_modsWatcherIgnoreUntilMs = QDateTime::currentMSecsSinceEpoch() + 1000;
    scanMods(m_modsPathInternal);
    m_batchOperationInProgress = wasBatchOperation;

    QStringList removedIds;
    QStringList removedNames;
    for (const PlannedMove& move : moves) {
        const QString id = m_modIds.value(move.name);
        if (!id.isEmpty())
            removedIds.append(id);
        removedNames.append(move.name);
    }

    index = bundleIndexById(m_modBundles, bundleId);
    QVariantMap updatedBundle = index >= 0 ? m_modBundles.at(index).toMap() : originalBundle;
    QStringList mergedIds = bundleMemberIds(updatedBundle);
    QStringList mergedNames = bundleMemberNames(updatedBundle);
    for (const QString& id : removedIds)
        mergedIds.removeAll(id);
    for (const QString& name : removedNames)
        mergedNames.removeAll(name);

    if (mergedIds.isEmpty() && mergedNames.isEmpty()) {
        if (index >= 0)
            m_modBundles.removeAt(index);
        QDir().rmdir(bundlePath);
    } else {
        QVariantList storedIds;
        for (const QString& id : mergedIds)
            storedIds.append(id);
        QVariantList storedNames;
        for (const QString& name : mergedNames)
            storedNames.append(name);
        updatedBundle["members"] = storedIds;
        updatedBundle["memberNames"] = storedNames;
        if (index >= 0)
            m_modBundles[index] = updatedBundle;
        else
            m_modBundles.append(updatedBundle);
    }

    refreshBundleMembership();
    saveModBundles();
    emit modBundlesChanged();
    return {};
}

void ModManager::openBundleFolder(const QString& bundleId)
{
    const int index = bundleIndexById(m_modBundles, bundleId);
    if (index < 0)
        return;

    const QString folder = m_modBundles.at(index).toMap().value("folder").toString();
    if (folder.isEmpty() || m_modsPathInternal.isEmpty())
        return;

    const QString path = QDir(m_modsPathInternal).filePath(folder);
    if (QFileInfo::exists(path))
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QStringList ModManager::getModBundleTags(const QString& bundleId) const
{
    const int index = bundleIndexById(m_modBundles, bundleId);
    if (index < 0)
        return {};

    QStringList tags;
    for (const QVariant& value : m_modBundles.at(index).toMap().value("tags").toList()) {
        const QString tag = value.toString();
        if (!tag.isEmpty())
            tags.append(tag);
    }
    return tags;
}

bool ModManager::addModBundleTag(const QString& bundleId, const QString& rawTag)
{
    const QString tag = rawTag.trimmed();
    if (tag.isEmpty())
        return false;

    const int index = bundleIndexById(m_modBundles, bundleId);
    if (index < 0)
        return false;

    QVariantMap bundle = m_modBundles.at(index).toMap();
    QVariantList tags = bundle.value("tags").toList();
    for (const QVariant& value : tags) {
        if (value.toString().compare(tag, Qt::CaseInsensitive) == 0)
            return false;
    }

    tags.append(tag);
    bundle["tags"] = tags;
    m_modBundles[index] = bundle;
    saveModBundles();
    emit modBundlesChanged();
    return true;
}

bool ModManager::removeModBundleTag(const QString& bundleId, const QString& rawTag)
{
    const QString tag = rawTag.trimmed();
    if (tag.isEmpty())
        return false;

    const int index = bundleIndexById(m_modBundles, bundleId);
    if (index < 0)
        return false;

    QVariantMap bundle = m_modBundles.at(index).toMap();
    QVariantList kept;
    bool removed = false;
    for (const QVariant& value : bundle.value("tags").toList()) {
        if (value.toString().compare(tag, Qt::CaseInsensitive) == 0) {
            removed = true;
            continue;
        }
        kept.append(value);
    }
    if (!removed)
        return false;

    bundle["tags"] = kept;
    m_modBundles[index] = bundle;
    saveModBundles();
    emit modBundlesChanged();
    return true;
}
