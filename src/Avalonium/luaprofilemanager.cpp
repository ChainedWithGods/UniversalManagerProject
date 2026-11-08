#include "luaprofilemanager.h"
#include "modmanager.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QDebug>

namespace {

QStringList defaultModFolders()
{
    return {
        QStringLiteral("GenericFunctions"),
        QStringLiteral("BPModLoader"),
        QStringLiteral("shared"),
        QStringLiteral("UniversalHUDToggle"),
        QStringLiteral("UniversalOverlay")
    };
}

constexpr const char* kProfilesFolderName = "Universal Manager Lua Mods";

QString sanitizeFolderName(const QString& name)
{
    QString safe = name;
    safe.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")), QStringLiteral("_"));

    safe.remove(QRegularExpression(QStringLiteral("[\\x00-\\x1f]")));
    while (safe.endsWith(QLatin1Char('.')) || safe.endsWith(QLatin1Char(' '))) {
        safe.chop(1);
    }
    safe = safe.trimmed();
    if (safe.isEmpty()) {
        safe = QStringLiteral("Profile");
    }
    return safe;
}

bool copyRecursively(const QString& srcPath, const QString& dstPath)
{
    const QFileInfo srcInfo(srcPath);
    if (!srcInfo.exists()) {
        return false;
    }

    if (srcInfo.isFile()) {
        QDir().mkpath(QFileInfo(dstPath).absolutePath());
        if (QFileInfo(dstPath).exists()) {
            QFile::remove(dstPath);
        }
        return QFile::copy(srcPath, dstPath);
    }

    QDir().mkpath(dstPath);

    bool ok = true;
    QDirIterator it(srcPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        const QString entry = it.next();
        const QFileInfo entryInfo(entry);
        const QString target = QDir(dstPath).filePath(entryInfo.fileName());
        if (entryInfo.isDir()) {
            ok = copyRecursively(entry, target) && ok;
        } else {
            if (QFileInfo(target).exists()) {
                QFile::remove(target);
            }
            ok = QFile::copy(entry, target) && ok;
        }
    }
    return ok;
}

bool writeDefaultModsJson(const QString& folderPath, QString* errorOut)
{
    QJsonArray mods;
    for (const QString& modName : defaultModFolders()) {

        if (modName.compare(QStringLiteral("shared"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        QJsonObject entry;
        entry.insert(QStringLiteral("name"), modName);
        entry.insert(QStringLiteral("enabled"), false);
        mods.append(entry);
    }

    QJsonObject root;
    root.insert(QStringLiteral("mods"), mods);

    QJsonDocument doc(root);
    QFile file(QDir(folderPath).filePath(QStringLiteral("mods.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Could not create mods.json in \"%1\": %2")
                            .arg(QDir::toNativeSeparators(folderPath), file.errorString());
        }
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool disableAllModsInJson(const QString& jsonPath, QString* errorOut)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Could not read \"%1\"").arg(QDir::toNativeSeparators(jsonPath));
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut) {
            *errorOut = QStringLiteral("\"%1\" is not valid JSON").arg(QDir::toNativeSeparators(jsonPath));
        }
        return false;
    }

    QJsonArray mods = doc.object().value(QStringLiteral("mods")).toArray();
    for (int i = 0; i < mods.size(); ++i) {
        QJsonObject entry = mods.at(i).toObject();
        entry.insert(QStringLiteral("enabled"), false);
        mods.replace(i, entry);
    }

    QJsonObject root;
    root.insert(QStringLiteral("mods"), mods);

    QSaveFile out(jsonPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Could not write \"%1\"").arg(QDir::toNativeSeparators(jsonPath));
        }
        return false;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!out.commit()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Could not save \"%1\"").arg(QDir::toNativeSeparators(jsonPath));
        }
        return false;
    }
    return true;
}

}

LuaProfileManager::LuaProfileManager(ModManager* modManager, QObject* parent)
    : QObject(parent)
    , m_modManager(modManager)
{
}

QString LuaProfileManager::activeProfileId() const
{
    return m_activeProfileId;
}

void LuaProfileManager::setActiveProfileId(const QString& profileId)
{
    const QString id = profileId.trimmed();
    if (m_activeProfileId == id) {
        return;
    }
    m_activeProfileId = id;
    emit activeProfileIdChanged();
}

QStringList LuaProfileManager::profileIds() const
{
    QStringList ids;
    if (!m_modManager) {
        return ids;
    }
    const QVariantList presets = m_modManager->gamePresets();
    ids.reserve(presets.size());
    for (const QVariant& v : presets) {
        const QString id = v.toMap().value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            ids.append(id);
        }
    }
    return ids;
}

QString LuaProfileManager::presetsFolderRoot() const
{
    if (!m_modManager) {
        return QString();
    }

    const QString dllDir = m_modManager->avaloniumDirectory();
    if (dllDir.isEmpty()) {
        return QString();
    }

    const QString newRoot = QDir(dllDir).filePath(QString::fromLatin1(kProfilesFolderName));

    const QString modsDir = QDir(dllDir).filePath(QStringLiteral("Mods"));
    const QString legacyRootInMods = QDir(modsDir).filePath(QString::fromLatin1(kProfilesFolderName));
    const QString legacyProfilesInMods = QDir(modsDir).filePath(QStringLiteral("Lua Profiles"));

    if (!QDir(newRoot).exists()) {
        if (QDir(legacyRootInMods).exists()) {
            if (!QDir().rename(legacyRootInMods, newRoot)) {
                qWarning() << "[LuaProfiles] Failed to migrate folder from" << legacyRootInMods << "to" << newRoot;
            } else {
                qDebug() << "[LuaProfiles] Successfully migrated folder from" << legacyRootInMods << "to" << newRoot;
            }
        } else if (QDir(legacyProfilesInMods).exists()) {
            if (!QDir().rename(legacyProfilesInMods, newRoot)) {
                qWarning() << "[LuaProfiles] Failed to migrate legacy 'Lua Profiles' folder to" << newRoot;
            } else {
                qDebug() << "[LuaProfiles] Successfully migrated 'Lua Profiles' folder to" << newRoot;
            }
        }
    }

    return newRoot;
}

QString LuaProfileManager::folderNameForPresetId(const QString& presetId, const QString& explicitName) const
{
    if (!m_modManager || presetId.trimmed().isEmpty()) {
        return QString();
    }

    QString name = explicitName.trimmed();
    const QVariantList presets = m_modManager->gamePresets();
    if (name.isEmpty()) {
        for (const QVariant& v : presets) {
            const QVariantMap preset = v.toMap();
            if (preset.value(QStringLiteral("id")).toString() == presetId) {
                name = preset.value(QStringLiteral("name")).toString().trimmed();
                break;
            }
        }
    }
    if (name.isEmpty()) {
        name = presetId;
    }

    const QString safeBase = sanitizeFolderName(name);
    int collisions = 0;
    for (const QVariant& v : presets) {
        const QVariantMap preset = v.toMap();
        if (preset.value(QStringLiteral("id")).toString() == presetId) {
            continue;
        }
        const QString otherName = preset.value(QStringLiteral("name")).toString();
        if (sanitizeFolderName(otherName.isEmpty() ? preset.value(QStringLiteral("id")).toString()
                                                    : otherName) == safeBase) {
            ++collisions;
        }
    }
    if (collisions > 0 && presetId.length() >= 8) {
        return safeBase + QStringLiteral("_%1").arg(presetId.left(8));
    }
    return safeBase;
}

QString LuaProfileManager::folderPathForName(const QString& presetId, const QString& displayName) const
{
    const QString root = presetsFolderRoot();
    if (root.isEmpty()) {
        return QString();
    }
    const QString base = sanitizeFolderName(displayName.isEmpty() ? presetId : displayName);
    return QDir(root).filePath(base);
}

QString LuaProfileManager::profileFolderPath(const QString& presetId, const QString& explicitName) const
{
    const QString root = presetsFolderRoot();
    if (root.isEmpty()) {
        return QString();
    }
    const QString folderName = folderNameForPresetId(presetId, explicitName);
    if (folderName.isEmpty()) {
        return QString();
    }

    const QString targetPath = QDir(root).filePath(folderName);

    const QString uuidPath = QDir(root).filePath(presetId);
    if (uuidPath != targetPath && QDir(uuidPath).exists()) {
        if (!QDir(targetPath).exists()) {
            QDir().rename(uuidPath, targetPath);
        } else {
            QDir(uuidPath).removeRecursively();
        }
    }

    return targetPath;
}

bool LuaProfileManager::profileFolderExists(const QString& presetId) const
{
    const QString path = profileFolderPath(presetId);
    return !path.isEmpty() && QDir(path).exists();
}

bool LuaProfileManager::seedProfileFolder(const QString& folderPath, QString* errorOut)
{
    if (!m_modManager) {
        if (errorOut) {
            *errorOut = QStringLiteral("Lua profile manager is not attached to a manager instance");
        }
        return false;
    }

    const QString defaultMods = m_modManager->defaultLuaModsDirectory();
    return seedProfileFolderAt(folderPath, defaultMods, errorOut);
}

bool LuaProfileManager::seedProfileFolderAt(const QString& folderPath, const QString& defaultMods,
                                            QString* errorOut)
{
    if (defaultMods.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Could not resolve the Avalonium Mods folder. Make sure the runtime is configured.");
        }
        return false;
    }

    QString stockSourceDir = defaultMods;
    if (QFileInfo(defaultMods).fileName().compare(QStringLiteral("Avalonium"), Qt::CaseInsensitive) != 0) {
        const QString candidate = QDir(defaultMods).filePath(QStringLiteral("Avalonium"));
        if (QDir(candidate).exists()) {
            stockSourceDir = candidate;
        }
    }

    if (!QDir().mkpath(folderPath)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Could not create the profile folder: %1")
                .arg(QDir::toNativeSeparators(folderPath));
        }
        return false;
    }

    QStringList missing;
    for (const QString& modName : defaultModFolders()) {
        const QString target = QDir(folderPath).filePath(modName);
        if (!QDir(target).exists()) {
            missing.append(modName);
        }
    }

    for (const QString& modName : missing) {
        const QString source = QDir(stockSourceDir).filePath(modName);
        if (!QFileInfo(source).exists()) {
            qWarning() << "[LuaProfiles] stock mod missing in source folder, skipping:" << source;
            continue;
        }
        const QString target = QDir(folderPath).filePath(modName);
        if (!copyRecursively(source, target)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Could not copy mod \"%1\" into the profile folder").arg(modName);
            }
            return false;
        }
    }

    const QString targetModsJson = QDir(folderPath).filePath(QStringLiteral("mods.json"));
    if (!QFileInfo(targetModsJson).exists()) {
        const QString sourceModsJson = QDir(stockSourceDir).filePath(QStringLiteral("mods.json"));
        if (QFileInfo(sourceModsJson).exists() && QFile::copy(sourceModsJson, targetModsJson)) {

            if (!disableAllModsInJson(targetModsJson, errorOut)) {
                return false;
            }
        } else {
            return writeDefaultModsJson(folderPath, errorOut);
        }
    }

    return true;
}

QString LuaProfileManager::createProfileFolder(const QString& presetId, const QString& presetName)
{
    const QString root = presetsFolderRoot();
    if (root.isEmpty()) {
        return QStringLiteral("Could not resolve the Avalonium Mods folder. Make sure the runtime is configured.");
    }

    if (!QDir().mkpath(root)) {
        return QStringLiteral("Could not create the Lua profiles folder: %1")
            .arg(QDir::toNativeSeparators(root));
    }

    const QString folderPath = profileFolderPath(presetId, presetName);
    if (folderPath.isEmpty()) {
        return QStringLiteral("Could not resolve the profile for the Lua folder");
    }

    QString error;
    if (!seedProfileFolder(folderPath, &error)) {
        return error;
    }

    emit profileFoldersChanged();
    return QString();
}

QString LuaProfileManager::deleteProfileFolder(const QString& presetId)
{
    const QString root = presetsFolderRoot();
    const QString folderPath = profileFolderPath(presetId);
    if (!folderPath.isEmpty() && QDir(folderPath).exists()) {
        if (!root.isEmpty() && QDir::cleanPath(folderPath) != QDir::cleanPath(root)) {
            if (!QDir(folderPath).removeRecursively()) {
                return QStringLiteral("Could not delete the Lua profile folder: %1")
                    .arg(QDir::toNativeSeparators(folderPath));
            }
        }
    }

    const QString uuidPath = QDir(root).filePath(presetId);
    if (QDir(uuidPath).exists() && (!root.isEmpty() && QDir::cleanPath(uuidPath) != QDir::cleanPath(root))) {
        QDir(uuidPath).removeRecursively();
    }

    emit profileFoldersChanged();
    return QString();
}

QString LuaProfileManager::renameProfileFolder(const QString& presetId, const QString& oldPresetName)
{
    const QString root = presetsFolderRoot();
    if (root.isEmpty()) {
        return QString();
    }

    const QString targetPath = profileFolderPath(presetId);
    if (targetPath.isEmpty() || QDir(targetPath).exists()) {
        return QString();
    }

    QString oldPath = folderPathForName(presetId, oldPresetName);
    if (oldPath.isEmpty() || !QDir(oldPath).exists()) {
        oldPath.clear();
        QString legacySuffix;
        if (presetId.length() >= 8) {
            legacySuffix = presetId.left(8);
        }
        if (!legacySuffix.isEmpty()) {
            const QString legacyTail = QStringLiteral("_%1").arg(legacySuffix);
            const QFileInfoList entries = QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo& fi : entries) {
                if (fi.fileName().endsWith(legacyTail)) {
                    oldPath = fi.absoluteFilePath();
                    break;
                }
            }
        }
    }
    if (oldPath.isEmpty() || !QDir(oldPath).exists()) {
        return QString();
    }

    if (!QDir().rename(oldPath, targetPath)) {
        return QStringLiteral("Could not rename the Lua profile folder to \"%1\"")
            .arg(QFileInfo(targetPath).fileName());
    }

    emit profileFoldersChanged();
    return QString();
}

QString LuaProfileManager::luaModsFolderForProfile(const QString& presetId) const
{
    if (presetId.trimmed().isEmpty()) {
        return QString();
    }
    const QString path = profileFolderPath(presetId);
    if (!path.isEmpty() && QDir(path).exists()) {
        return QDir::fromNativeSeparators(path);
    }
    return QString();
}
