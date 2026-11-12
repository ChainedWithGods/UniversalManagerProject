#include "ModManagerInternal.h"

#include <QThreadPool>

using namespace mmdetail;

QString ModManager::getPresetsBasePath() const {
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDir.isEmpty()) {
        appDataDir = QCoreApplication::applicationDirPath();
    }
    QString presetsPath = appDataDir + "/presets";
    QDir dir(presetsPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return presetsPath;
}

QString ModManager::getPresetsFilePath() const {
    return getPresetsBasePath() + "/presets.json";
}

QString ModManager::getPresetFolderPath(const QString& presetName) const {

    QString safeName = presetName;
    safeName.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");
    safeName = safeName.trimmed();
    if (safeName.isEmpty()) {
        safeName = "Unnamed";
    }
    return getPresetsBasePath() + "/" + safeName;
}

QVariantList ModManager::gamePresets() const {
    QVariantList list;
    list.reserve(m_gamePresets.size());
    for (const QVariant& v : m_gamePresets) {
        const QVariantMap preset = v.toMap();
        QVariantMap item;
        item["id"] = preset.value("id");
        item["name"] = preset.value("name");
        item["previewImage"] = preset.value("previewImage");
        item["blurBackground"] = preset.value("blurBackground", false);

        const QString modsPath = preset.value("modsPath").toString();
        item["modCount"] = countDisplayModsInPath(modsPath);

        list.append(item);
    }
    return list;
}

QString ModManager::activePresetId() const {
    return m_activePresetId;
}

QVariantMap ModManager::currentStateToPresetMap(const QString& id, const QString& name) const {
    QVariantMap preset;
    preset["id"] = id;
    preset["name"] = name;

    preset["theme"] = m_theme;
    preset["wallpaperPath"] = m_wallpaperPath;
    preset["glassBlur"] = m_glassBlur;
    preset["glassOpacity"] = m_glassOpacity;
    preset["glassDarkness"] = m_glassDarkness;
    preset["navBarColor"] = m_navBarColor;
    preset["navBarIconColor"] = m_navBarIconColor;
    preset["accentColor"] = m_accentColor;
    preset["cpuUsageBarColor"] = m_cpuUsageBarColor;
    preset["ramUsageBarColor"] = m_ramUsageBarColor;
    preset["driveUsageBarColor"] = m_driveUsageBarColor;
    preset["idColor"] = m_idColor;
    preset["toggleActiveColor"] = m_toggleActiveColor;
    preset["toggleDisabledColor"] = m_toggleDisabledColor;
    preset["enableButtonColor"] = m_enableButtonColor;
    preset["disableButtonColor"] = m_disableButtonColor;
    preset["folderModColor"] = m_folderModColor;
    preset["looseFileColor"] = m_looseFileColor;
    preset["tagColor"] = m_tagColor;
    preset["pakColor"] = m_pakColor;
    preset["ucasColor"] = m_ucasColor;
    preset["utocColor"] = m_utocColor;
    preset["gamePath"] = m_gamePath;
    preset["modsPath"] = m_modsPath;
    preset["downloadsPath"] = m_downloadsPath;
    preset["backupPath"] = m_backupPath;
    preset["runtimeOutputPath"] = m_runtimeOutputPath;
    preset["gamePaksPath"] = m_gamePaksPath;
    preset["usmapFilePath"] = m_usmapFilePath;
    preset["aesKey"] = m_aesKey;
    preset["ueVersion"] = m_ueVersion;

    QVariantMap modImages;
    for (auto it = m_modImages.constBegin(); it != m_modImages.constEnd(); ++it) {
        modImages.insert(it.key(), it.value());
    }
    preset["modImages"] = modImages;

    QVariantMap modTags;
    for (auto it = m_modTags.constBegin(); it != m_modTags.constEnd(); ++it) {
        modTags.insert(it.key(), it.value());
    }
    preset["modTags"] = modTags;

    QVariantMap modDescriptions;
    for (auto it = m_modDescriptions.constBegin(); it != m_modDescriptions.constEnd(); ++it) {
        modDescriptions.insert(it.key(), it.value());
    }
    preset["modDescriptions"] = modDescriptions;

    QVariantMap modIds;
    for (auto it = m_modIds.constBegin(); it != m_modIds.constEnd(); ++it) {
        modIds.insert(it.key(), it.value());
    }
    preset["modIds"] = modIds;

    QStringList favorites;
    favorites.reserve(m_favoriteMods.size());
    for (const QString& key : m_favoriteMods) {
        favorites.append(key);
    }
    preset["favorites"] = favorites;
    preset["modsFolderEnabledSnapshotByPath"] = m_modsFolderEnabledSnapshotByPath;
    preset["modsFolderMassDisabledByPath"] = m_modsFolderMassDisabledByPath;

    return preset;
}

int ModManager::findPresetIndexById(const QString& presetId) const {
    if (presetId.isEmpty()) {
        return -1;
    }
    for (int i = 0; i < m_gamePresets.size(); ++i) {
        const QVariantMap map = m_gamePresets[i].toMap();
        if (map.value("id").toString() == presetId) {
            return i;
        }
    }
    return -1;
}

bool ModManager::isPresetUsingSharedLuaMods(const QString& presetId) const {
    const QString targetId = presetId.isEmpty() ? m_activePresetId : presetId;
    if (targetId.isEmpty()) {
        return false;
    }
    const int idx = findPresetIndexById(targetId);
    if (idx < 0) {
        return false;
    }
    return m_gamePresets[idx].toMap().value(QStringLiteral("useSharedLuaMods"), false).toBool();
}

bool ModManager::applyPresetToCurrentState(const QVariantMap& preset) {
    if (preset.isEmpty()) {
        return false;
    }

    const bool wasLoading = m_isLoading;
    m_isLoading = true;

    const auto setString = [&](QString& field, const QString& value, const std::function<void()>& signal) {
        if (field != value) {
            field = value;
            signal();
        }
    };

    const auto setDouble = [&](double& field, double value, const std::function<void()>& signal) {
        if (!qFuzzyCompare(field + 1.0, value + 1.0)) {
            field = value;
            signal();
        }
    };

    QString presetTheme = preset.contains("theme") ? preset.value("theme").toString() : m_theme;
    if (presetTheme == "Neon") presetTheme = "Crimson";
    setString(m_theme, presetTheme, [&]() { emit themeChanged(); });
    setString(m_wallpaperPath, preset.contains("wallpaperPath") ? preset.value("wallpaperPath").toString() : m_wallpaperPath, [&]() { emit wallpaperPathChanged(); });
    setDouble(m_glassBlur, preset.contains("glassBlur") ? preset.value("glassBlur").toDouble() : m_glassBlur, [&]() { emit glassBlurChanged(); });
    setDouble(m_glassOpacity, preset.contains("glassOpacity") ? preset.value("glassOpacity").toDouble() : m_glassOpacity, [&]() { emit glassOpacityChanged(); });
    setDouble(m_glassDarkness, preset.contains("glassDarkness") ? preset.value("glassDarkness").toDouble() : m_glassDarkness, [&]() { emit glassDarknessChanged(); });
    setString(m_navBarColor, preset.contains("navBarColor") ? preset.value("navBarColor").toString() : m_navBarColor, [&]() { emit navBarColorChanged(); });
    setString(m_navBarIconColor, preset.contains("navBarIconColor") ? preset.value("navBarIconColor").toString() : m_navBarIconColor, [&]() { emit navBarIconColorChanged(); });
    setString(m_accentColor, preset.contains("accentColor") ? preset.value("accentColor").toString() : m_accentColor, [&]() { emit accentColorChanged(); });
    setString(m_cpuUsageBarColor, preset.contains("cpuUsageBarColor") ? preset.value("cpuUsageBarColor").toString() : m_cpuUsageBarColor, [&]() { emit cpuUsageBarColorChanged(); });
    setString(m_ramUsageBarColor, preset.contains("ramUsageBarColor") ? preset.value("ramUsageBarColor").toString() : m_ramUsageBarColor, [&]() { emit ramUsageBarColorChanged(); });
    setString(m_driveUsageBarColor, preset.contains("driveUsageBarColor") ? preset.value("driveUsageBarColor").toString() : m_driveUsageBarColor, [&]() { emit driveUsageBarColorChanged(); });
    setString(m_idColor, preset.contains("idColor") ? preset.value("idColor").toString() : m_idColor, [&]() { emit idColorChanged(); });
    setString(m_toggleActiveColor, preset.contains("toggleActiveColor") ? preset.value("toggleActiveColor").toString() : m_toggleActiveColor, [&]() { emit toggleActiveColorChanged(); });
    setString(m_toggleDisabledColor, preset.contains("toggleDisabledColor") ? preset.value("toggleDisabledColor").toString() : m_toggleDisabledColor, [&]() { emit toggleDisabledColorChanged(); });
    setString(m_enableButtonColor, preset.contains("enableButtonColor") ? preset.value("enableButtonColor").toString() : m_enableButtonColor, [&]() { emit enableButtonColorChanged(); });
    setString(m_disableButtonColor, preset.contains("disableButtonColor") ? preset.value("disableButtonColor").toString() : m_disableButtonColor, [&]() { emit disableButtonColorChanged(); });
    setString(m_folderModColor, preset.contains("folderModColor") ? preset.value("folderModColor").toString() : m_folderModColor, [&]() { emit folderModColorChanged(); });
    setString(m_looseFileColor, preset.contains("looseFileColor") ? preset.value("looseFileColor").toString() : m_looseFileColor, [&]() { emit looseFileColorChanged(); });
    setString(m_tagColor, preset.contains("tagColor") ? preset.value("tagColor").toString() : m_tagColor, [&]() { emit tagColorChanged(); });
    setString(m_pakColor, preset.contains("pakColor") ? preset.value("pakColor").toString() : m_pakColor, [&]() { emit pakColorChanged(); });
    setString(m_ucasColor, preset.contains("ucasColor") ? preset.value("ucasColor").toString() : m_ucasColor, [&]() { emit ucasColorChanged(); });
    setString(m_utocColor, preset.contains("utocColor") ? preset.value("utocColor").toString() : m_utocColor, [&]() { emit utocColorChanged(); });

    setString(m_gamePath, preset.contains("gamePath") ? preset.value("gamePath").toString() : m_gamePath, [&]() { emit gamePathChanged(); });
    setString(m_modsPath, preset.contains("modsPath") ? preset.value("modsPath").toString() : m_modsPath, [&]() { emit modsPathChanged(); });
    setString(m_runtimeOutputPath, preset.contains("runtimeOutputPath") ? preset.value("runtimeOutputPath").toString() : m_runtimeOutputPath, [&]() { emit runtimeOutputPathChanged(); });

    QString newDownloadsPath = preset.contains("downloadsPath") ? preset.value("downloadsPath").toString() : m_downloadsPath;
    if (m_downloadsPath != newDownloadsPath) {

        if (!m_downloadsPath.isEmpty()) {
            m_downloadsWatcher->removePath(m_downloadsPath);
        }
        m_downloadsPath = newDownloadsPath;
        emit downloadsPathChanged();
    }

    if (!m_downloadsPath.isEmpty() && QDir(m_downloadsPath).exists()) {
        if (!m_downloadsWatcher->directories().contains(m_downloadsPath)) {
            m_downloadsWatcher->addPath(m_downloadsPath);
            qDebug() << "Monitoring downloads path at:" << m_downloadsPath;
        }
    }

    setString(m_backupPath, preset.contains("backupPath") ? preset.value("backupPath").toString() : m_backupPath, [&]() { emit backupPathChanged(); });
    setString(m_gamePaksPath, preset.contains("gamePaksPath") ? preset.value("gamePaksPath").toString() : m_gamePaksPath, [&]() { emit gamePaksPathChanged(); });
    setString(m_usmapFilePath, preset.contains("usmapFilePath") ? preset.value("usmapFilePath").toString() : m_usmapFilePath, [&]() { emit usmapFilePathChanged(); });
    setString(m_aesKey, preset.contains("aesKey") ? preset.value("aesKey").toString() : m_aesKey, [&]() { emit aesKeyChanged(); });
    setString(m_ueVersion, preset.contains("ueVersion") ? preset.value("ueVersion").toString() : m_ueVersion, [&]() { emit ueVersionChanged(); });

    if ((m_gamePaksPath.isEmpty() || !QDir(m_gamePaksPath).exists()) && !m_gamePath.isEmpty()) {
        const QString autoPaksPath = detectGamePaksPathFromGameExecutable(m_gamePath);
        if (!autoPaksPath.isEmpty() && m_gamePaksPath != autoPaksPath) {
            m_gamePaksPath = autoPaksPath;
            emit gamePaksPathChanged();
        }
    }

    m_modImages.clear();
    if (preset.contains("modImages")) {
        const QVariantMap modImages = preset.value("modImages").toMap();
        for (auto it = modImages.constBegin(); it != modImages.constEnd(); ++it) {
            m_modImages.insert(it.key(), it.value().toString());
        }
    }

    m_modTags.clear();
    if (preset.contains("modTags")) {
        const QVariantMap modTags = preset.value("modTags").toMap();
        for (auto it = modTags.constBegin(); it != modTags.constEnd(); ++it) {
            m_modTags.insert(it.key(), it.value().toStringList());
        }
    }

    m_modDescriptions.clear();
    if (preset.contains("modDescriptions")) {
        const QVariantMap modDescriptions = preset.value("modDescriptions").toMap();
        for (auto it = modDescriptions.constBegin(); it != modDescriptions.constEnd(); ++it) {
            m_modDescriptions.insert(it.key(), it.value().toString());
        }
    }

    m_modIds.clear();
    if (preset.contains("modIds")) {
        const QVariantMap modIds = preset.value("modIds").toMap();
        for (auto it = modIds.constBegin(); it != modIds.constEnd(); ++it) {
            m_modIds.insert(it.key(), it.value().toString());
        }
    }

    loadModAliases();
    loadModBundles();

    m_modsFolderEnabledSnapshotByPath.clear();
    if (preset.contains("modsFolderEnabledSnapshotByPath")) {
        m_modsFolderEnabledSnapshotByPath = preset.value("modsFolderEnabledSnapshotByPath").toMap();
    }

    m_modsFolderMassDisabledByPath.clear();
    if (preset.contains("modsFolderMassDisabledByPath")) {
        m_modsFolderMassDisabledByPath = preset.value("modsFolderMassDisabledByPath").toMap();
    }

    m_favoriteMods.clear();
    if (preset.contains("favorites")) {
        const QStringList favorites = preset.value("favorites").toStringList();
        for (const QString& key : favorites) {
            if (!key.isEmpty()) {
                m_favoriteMods.insert(key);
            }
        }
    }

    const QString presetId = preset.value(QStringLiteral("id")).toString();
    const QString presetName = preset.value(QStringLiteral("name")).toString();
    const bool useSharedLuaMods = preset.value(QStringLiteral("useSharedLuaMods")).toBool();

    QString targetLuaModsPath;
    if (useSharedLuaMods) {
        targetLuaModsPath = QString();
    } else if (preset.contains(QStringLiteral("luaModsPath")) && !preset.value(QStringLiteral("luaModsPath")).toString().trimmed().isEmpty()) {
        targetLuaModsPath = preset.value(QStringLiteral("luaModsPath")).toString().trimmed();
    } else if (!presetId.isEmpty() && m_luaProfileManager) {
        targetLuaModsPath = m_luaProfileManager->profileFolderPath(presetId);

        if (!targetLuaModsPath.isEmpty() && !QDir(targetLuaModsPath).exists() && !wasLoading) {
            m_luaProfileManager->createProfileFolder(presetId, presetName);
        }
    }

    if (m_luaProfileManager && !presetId.isEmpty()) {
        m_luaProfileManager->setActiveProfileId(useSharedLuaMods ? QString() : presetId);
    }

    if (!targetLuaModsPath.isEmpty() || useSharedLuaMods) {
        setAvaloniumLuaModsPath(targetLuaModsPath);
    }

    if (m_luaProfileManager && !presetId.isEmpty()) {
        const QString cppRoot = m_luaProfileManager->cppModsFolderForProfile(presetId);
        setAvaloniumCppModsPath(!cppRoot.isEmpty() && QDir(cppRoot).exists() ? cppRoot : QString());
    }

    m_isLoading = wasLoading;

    if (!m_modsPath.isEmpty() && QDir(m_modsPath).exists()) {
        scanMods(m_modsPath);
    }

    return true;
}

void ModManager::savePresets() {
    if (m_isLoading) {
        return;
    }

    for (const QVariant& v : m_gamePresets) {
        QVariantMap preset = v.toMap();

        preset["isActive"] = (preset.value("id").toString() == m_activePresetId);
        savePreset(preset);
    }
}

void ModManager::savePreset(const QVariantMap& preset) {
    QString presetName = preset.value("name").toString();
    if (presetName.isEmpty()) {
        presetName = "Default";
    }

    QString folderPath = getPresetFolderPath(presetName);
    QDir dir(folderPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString filePath = folderPath + "/preset.json";
    QJsonObject presetObj = QJsonObject::fromVariantMap(preset);
    QJsonDocument doc(presetObj);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

void ModManager::deletePresetFolder(const QString& presetName) {
    QString folderPath = getPresetFolderPath(presetName);
    QDir dir(folderPath);
    if (dir.exists()) {
        dir.removeRecursively();
    }
}

void ModManager::loadPresets() {
    m_isLoading = true;

    m_gamePresets.clear();
    m_activePresetId.clear();

    const QString presetsBasePath = getPresetsBasePath();

    const QString oldPresetsJsonPath = presetsBasePath + "/presets.json";
    QFile oldPresetsFile(oldPresetsJsonPath);
    if (oldPresetsFile.exists()) {

        if (oldPresetsFile.open(QIODevice::ReadOnly)) {
            const QByteArray data = oldPresetsFile.readAll();
            oldPresetsFile.close();

            const QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                const QJsonObject obj = doc.object();
                m_activePresetId = obj.value("activePresetId").toString();
                const QJsonArray presets = obj.value("presets").toArray();
                for (const QJsonValue& v : presets) {
                    if (v.isObject()) {
                        QVariantMap preset = v.toObject().toVariantMap();

                        preset["isActive"] = (preset.value("id").toString() == m_activePresetId);
                        m_gamePresets.append(preset);

                        savePreset(preset);
                    }
                }
            }

            oldPresetsFile.remove();
            qDebug() << "Migrated presets from presets.json to folder-based format";
        }
    } else {

        QDir presetsDir(presetsBasePath);
        QStringList presetFolders = presetsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& folderName : presetFolders) {
            QString presetFilePath = presetsBasePath + "/" + folderName + "/preset.json";
            QFile presetFile(presetFilePath);
            if (presetFile.open(QIODevice::ReadOnly)) {
                const QByteArray data = presetFile.readAll();
                presetFile.close();
                const QJsonDocument doc = QJsonDocument::fromJson(data);
                if (doc.isObject()) {
                    QVariantMap preset = doc.object().toVariantMap();
                    m_gamePresets.append(preset);

                    if (preset.value("isActive").toBool()) {
                        m_activePresetId = preset.value("id").toString();
                    }
                }
            }
        }

        const QString oldActivePath = presetsBasePath + "/active.json";
        QFile oldActiveFile(oldActivePath);
        if (oldActiveFile.exists()) {

            if (m_activePresetId.isEmpty()) {
                if (oldActiveFile.open(QIODevice::ReadOnly)) {
                    const QByteArray data = oldActiveFile.readAll();
                    oldActiveFile.close();
                    const QJsonDocument doc = QJsonDocument::fromJson(data);
                    if (doc.isObject()) {
                        m_activePresetId = doc.object().value("activePresetId").toString();
                    }
                }
            }

            oldActiveFile.remove();
            qDebug() << "Removed legacy active.json file";
        }
    }

    if (m_gamePresets.isEmpty()) {
        m_activePresetId.clear();

        m_gamePath = "";
        m_modsPath = "";
        if (!m_downloadsPath.isEmpty()) {
            m_downloadsWatcher->removePath(m_downloadsPath);
        }
        m_downloadsPath = "";
        m_backupPath = "";
        m_modImages.clear();
        m_modTags.clear();
        m_modDescriptions.clear();
        m_modIds.clear();
        m_favoriteMods.clear();
        m_modList.clear();
        m_downloadedModsList.clear();
        m_modsInstalled = 0;
        m_modsActive = 0;
        m_gamePaksPath = "";
        m_usmapFilePath = "";
        m_aesKey = "";

        m_isLoading = false;
        emit gamePresetsChanged();
        emit activePresetIdChanged();
        emit gamePathChanged();
        emit modsPathChanged();
        emit downloadsPathChanged();
        emit backupPathChanged();
        emit modListChanged();
        emit modsInstalledChanged();
        emit modsActiveChanged();
        emit downloadedModsListChanged();
        emit downloadedModsCountChanged();
        emit totalModsAllPresetsChanged();
        emit totalActiveModsAllPresetsChanged();
        return;
    }

    const int migrateIndex = findPresetIndexById(m_activePresetId);
    if (migrateIndex >= 0) {
        QVariantMap preset = m_gamePresets[migrateIndex].toMap();
        const bool missingMetadata = !preset.contains("modImages") || !preset.contains("modTags") ||
                                     !preset.contains("modDescriptions") || !preset.contains("modIds") ||
                                     !preset.contains("favorites");
        if (missingMetadata && (!m_modImages.isEmpty() || !m_modTags.isEmpty() || !m_modDescriptions.isEmpty() ||
                                !m_modIds.isEmpty() || !m_favoriteMods.isEmpty())) {
            QVariantMap migrated = currentStateToPresetMap(
                preset.value("id").toString(),
                preset.value("name").toString().isEmpty() ? QString("Default") : preset.value("name").toString()
            );

            for (auto it = preset.constBegin(); it != preset.constEnd(); ++it) {
                if (!migrated.contains(it.key())) {
                    migrated.insert(it.key(), it.value());
                }
            }
            m_gamePresets[migrateIndex] = migrated;
            savePresets();
        }
    }

    if (m_gamePresets.isEmpty()) {
        m_activePresetId.clear();
    }

    if (m_activePresetId.isEmpty() && !m_gamePresets.isEmpty()) {
        m_activePresetId = m_gamePresets.first().toMap().value("id").toString();
    }

    const int activeIndex = findPresetIndexById(m_activePresetId);
    if (activeIndex >= 0) {
        applyPresetToCurrentState(m_gamePresets[activeIndex].toMap());
    }

    m_isLoading = false;
    emit gamePresetsChanged();
    emit activePresetIdChanged();
    emit totalModsAllPresetsChanged();
    emit totalActiveModsAllPresetsChanged();
}

void ModManager::updateActivePresetFromCurrentState() {
    if (m_isLoading) {
        return;
    }
    if (m_activePresetId.isEmpty()) {
        return;
    }

    const int index = findPresetIndexById(m_activePresetId);
    if (index < 0) {
        return;
    }

    const QVariantMap oldPreset = m_gamePresets[index].toMap();
    QString name = oldPreset.value("name").toString();
    if (name.isEmpty()) {
        name = "Default";
    }

    QVariantMap newPreset = currentStateToPresetMap(m_activePresetId, name);

    if (oldPreset.contains("previewImage")) {
        newPreset["previewImage"] = oldPreset.value("previewImage");
    }
    if (oldPreset.contains("blurBackground")) {
        newPreset["blurBackground"] = oldPreset.value("blurBackground");
    }

    m_gamePresets[index] = newPreset;
    savePresets();
    saveModAliases();
    saveModBundles();
    emit gamePresetsChanged();
    emit totalModsAllPresetsChanged();
}

void ModManager::setActivePreset(const QString& presetId) {
    if (presetId.isEmpty() || presetId == m_activePresetId) {
        return;
    }

    const int presetIndex = findPresetIndexById(presetId);
    if (presetIndex < 0) {
        return;
    }

    const QVariantMap presetToApply = m_gamePresets[presetIndex].toMap();

    if (!m_activePresetId.isEmpty()) {
        saveModAliases();
        saveModBundles();
    }

    m_activePresetId = presetId;
    emit activePresetIdChanged();

    m_batchOperationInProgress = true;
    applyPresetToCurrentState(presetToApply);
    m_batchOperationInProgress = false;

    updateActivePresetFromCurrentState();
    emit collectionsChanged();
}

void ModManager::createPreset(const QString& name) {
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString presetName = name.trimmed().isEmpty() ? "New Preset" : name.trimmed();

    QVariantMap newPreset;
    newPreset["id"] = id;
    newPreset["name"] = presetName;
    newPreset["previewImage"] = "";
    newPreset["blurBackground"] = false;

    newPreset["gamePath"] = "";
    newPreset["modsPath"] = "";
    newPreset["downloadsPath"] = "";
    newPreset["backupPath"] = "";
    newPreset["runtimeOutputPath"] = "";
    newPreset["gamePaksPath"] = "";
    newPreset["usmapFilePath"] = "";
    newPreset["aesKey"] = "";
    newPreset["ueVersion"] = "UE 5.3";

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
        const QString luaError = m_luaProfileManager->createProfileFolder(id, presetName);
        emit luaProfileFolderResult(presetName, luaError);
        if (!luaError.isEmpty())
            emit luaProfileCreateFailed(presetName, luaError);
        luaFolderPath = m_luaProfileManager->profileFolderPath(id, presetName);
        m_luaProfileManager->setActiveProfileId(id);
    }
    newPreset["luaModsPath"] = luaFolderPath;
    newPreset["useSharedLuaMods"] = false;

    m_gamePresets.append(newPreset);
    emit gamePresetsChanged();
    emit totalModsAllPresetsChanged();
    savePresets();
    setActivePreset(id);

    if (!luaFolderPath.isEmpty()) {
        setAvaloniumLuaModsPath(luaFolderPath);
    }
}

void ModManager::ensureLuaProfileFolders() {
    if (!m_luaProfileManager) {
        return;
    }

    const QString defaultMods = defaultLuaModsDirectory();
    if (defaultMods.isEmpty()) {
        return;
    }

    struct PendingProfile {
        QString name;
        QString folderPath;
    };
    QList<PendingProfile> pending;

    for (const QVariant& value : m_gamePresets) {
        const QVariantMap preset = value.toMap();
        if (preset.value(QStringLiteral("useSharedLuaMods")).toBool()) {
            continue;
        }

        const QString id = preset.value(QStringLiteral("id")).toString();
        const QString name = preset.value(QStringLiteral("name")).toString();
        if (id.isEmpty() || name.isEmpty()) {
            continue;
        }

        const QString folderPath = m_luaProfileManager->profileFolderPath(id, name);
        if (folderPath.isEmpty() || QDir(folderPath).exists()) {
            continue;
        }

        const QString configuredPath = preset.value(QStringLiteral("luaModsPath")).toString().trimmed();
        if (!configuredPath.isEmpty()
            && QDir::cleanPath(configuredPath).compare(QDir::cleanPath(folderPath),
                                                       Qt::CaseInsensitive) != 0) {
            continue;
        }

        PendingProfile profile;
        profile.name = name;
        profile.folderPath = folderPath;
        pending.append(profile);
    }

    if (pending.isEmpty()) {
        return;
    }

    QThreadPool::globalInstance()->start([this, pending, defaultMods]() {
        QVariantList restored;
        for (const PendingProfile& profile : pending) {
            if (LuaProfileManager::seedProfileFolderAt(profile.folderPath, defaultMods, nullptr)) {
                QVariantMap entry;
                entry.insert(QStringLiteral("name"), profile.name);
                entry.insert(QStringLiteral("path"), profile.folderPath);
                restored.append(entry);
            }
        }

        if (restored.isEmpty()) {
            return;
        }

        QMetaObject::invokeMethod(this, [this, restored]() {
            if (m_luaProfileManager) {
                m_luaProfileManager->notifyProfileFoldersChanged();
            }
            emit luaProfileFoldersCreated(restored);
        }, Qt::QueuedConnection);
    });
}

void ModManager::deletePreset(const QString& presetId) {
    if (presetId.isEmpty()) {
        return;
    }

    const int indexToRemove = findPresetIndexById(presetId);
    if (indexToRemove < 0) {
        return;
    }

    const QVariantMap presetToDelete = m_gamePresets[indexToRemove].toMap();
    const QString presetName = presetToDelete.value("name").toString();

    if (m_luaProfileManager) {
        const QString luaError = m_luaProfileManager->deleteProfileFolder(presetId);
        if (!luaError.isEmpty()) {
            qWarning() << "[LuaProfiles] failed to delete folder for preset" << presetName << ":" << luaError;
            emit luaProfileFolderResult(presetName, luaError);
        }
    }

    if (m_gamePresets.size() <= 1) {

        if (!presetName.isEmpty()) {
            deletePresetFolder(presetName);
        }

        m_gamePresets.clear();
        m_activePresetId = "";
        if (m_luaProfileManager) {
            m_luaProfileManager->setActiveProfileId(QString());
        }
        setAvaloniumLuaModsPath(QString());

        m_theme = "Cozy";
        m_gamePath = "";
        m_modsPath = "";

        if (!m_downloadsPath.isEmpty()) {
            m_downloadsWatcher->removePath(m_downloadsPath);
        }
        m_downloadsPath = "";
        m_downloadedModsList.clear();

        m_modList.clear();
        m_modsInstalled = 0;
        m_modsActive = 0;

        m_backupPath = "";
        m_wallpaperPath = "";
        m_glassBlur = 0.5;
        m_glassOpacity = 0.15;
        m_glassDarkness = 1.0;
        m_navBarColor = "#000000";
        m_accentColor = "#9135FF";
        m_cpuUsageBarColor = "#8B7355";
        m_ramUsageBarColor = "#D4A574";
        m_driveUsageBarColor = "#A08060";
        m_idColor = "#8B7355";
        m_toggleActiveColor = "#8B7355";
        m_toggleDisabledColor = "#D4A574";
        m_enableButtonColor = "#8B7355";
        m_disableButtonColor = "#D4A574";
        m_folderModColor = "#8B7355";
        m_looseFileColor = "#D4A574";
        m_tagColor = "#8B7355";
        m_pakColor = "#8B7355";
        m_ucasColor = "#D4A574";
        m_utocColor = "#A08060";
        m_pakViewerPath = "";
        m_gamePaksPath = "";
        m_usmapFilePath = "";
        m_aesKey = "";
        m_ueVersion = "UE 5.3";
        m_modImages.clear();
        m_modTags.clear();
        m_modDescriptions.clear();
        m_modIds.clear();
        m_favoriteMods.clear();

        emit themeChanged();
        emit gamePathChanged();
        emit modsPathChanged();
        emit downloadsPathChanged();
        emit backupPathChanged();
        emit wallpaperPathChanged();
        emit glassBlurChanged();
        emit glassOpacityChanged();
        emit glassDarknessChanged();
        emit navBarColorChanged();
        emit accentColorChanged();
        emit cpuUsageBarColorChanged();
        emit ramUsageBarColorChanged();
        emit driveUsageBarColorChanged();
        emit idColorChanged();
        emit toggleActiveColorChanged();
        emit toggleDisabledColorChanged();
        emit enableButtonColorChanged();
        emit disableButtonColorChanged();
        emit folderModColorChanged();
        emit looseFileColorChanged();
        emit tagColorChanged();
        emit pakColorChanged();
        emit ucasColorChanged();
        emit utocColorChanged();
        emit gamePresetsChanged();
        emit activePresetIdChanged();
        emit totalModsAllPresetsChanged();
        emit totalActiveModsAllPresetsChanged();
        emit modListChanged();
        emit modsInstalledChanged();
        emit modsActiveChanged();
        emit downloadedModsListChanged();
        emit downloadedModsCountChanged();

        savePresets();
        return;
    }

    const bool removingActive = (presetId == m_activePresetId);
    m_gamePresets.removeAt(indexToRemove);

    if (!presetName.isEmpty()) {
        deletePresetFolder(presetName);
    }

    emit gamePresetsChanged();
    emit totalModsAllPresetsChanged();

    if (removingActive) {
        const QString nextId = m_gamePresets.first().toMap().value("id").toString();
        m_activePresetId = "";
        setActivePreset(nextId);
    } else {
        savePresets();
    }
}

void ModManager::renamePreset(const QString& presetId, const QString& newName) {
    const QString name = newName.trimmed();
    if (presetId.isEmpty() || name.isEmpty()) {
        return;
    }

    for (int i = 0; i < m_gamePresets.size(); ++i) {
        QVariantMap preset = m_gamePresets[i].toMap();
        if (preset.value("id").toString() == presetId) {

            const QString oldName = preset.value("name").toString();

            preset["name"] = name;
            m_gamePresets[i] = preset;

            if (!oldName.isEmpty() && oldName != name) {
                if (presetId == m_activePresetId) {
                    saveModAliases();
                    saveModBundles();
                } else {
                    copyModAliases(oldName, name);
                    copyModBundles(oldName, name);
                }
                deletePresetFolder(oldName);
            }

            if (m_luaProfileManager && !oldName.isEmpty() && oldName != name) {
                const QString luaError = m_luaProfileManager->renameProfileFolder(presetId, oldName);
                if (!luaError.isEmpty()) {
                    qWarning() << "[LuaProfiles] failed to rename folder for preset" << name << ":" << luaError;
                    emit luaProfileFolderResult(name, luaError);
                } else {
                    const QString newFolder = m_luaProfileManager->profileFolderPath(presetId);
                    preset["luaModsPath"] = newFolder;
                    m_gamePresets[i] = preset;
                    if (m_activePresetId == presetId && !preset.value("useSharedLuaMods").toBool()) {
                        setAvaloniumLuaModsPath(newFolder);
                    }
                }
            }

            emit gamePresetsChanged();
            savePresets();
            return;
        }
    }
}

void ModManager::duplicatePreset(const QString& presetId) {

    const int sourceIndex = findPresetIndexById(presetId);
    if (sourceIndex < 0) {
        return;
    }

    QVariantMap source = m_gamePresets[sourceIndex].toMap();

    const QString newId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString sourceName = source.value("name").toString();
    const QString newName = (sourceName.isEmpty() ? QString("Preset") : sourceName) + " Copy";
    source["id"] = newId;
    source["name"] = newName;

    QString luaFolderPath;
    if (m_luaProfileManager) {
        const QString luaError = m_luaProfileManager->createProfileFolder(newId, newName);
        if (!luaError.isEmpty()) {
            qWarning() << "[LuaProfiles] failed to create folder for duplicated preset" << newName << ":" << luaError;
            emit luaProfileCreateFailed(newName, luaError);
        }
        luaFolderPath = m_luaProfileManager->profileFolderPath(newId, newName);
        m_luaProfileManager->setActiveProfileId(newId);
    }
    source["luaModsPath"] = luaFolderPath;
    source["useSharedLuaMods"] = false;

    m_gamePresets.append(source);
    emit gamePresetsChanged();
    savePresets();

    copyModAliases(sourceName, newName);
    copyModBundles(sourceName, newName);
    setActivePreset(newId);

    if (!luaFolderPath.isEmpty()) {
        setAvaloniumLuaModsPath(luaFolderPath);
    }
}

void ModManager::setPresetPreview(const QString& presetId, const QString& imagePath) {
    if (presetId.isEmpty()) {
        return;
    }

    QString localPath = QUrl(imagePath).toLocalFile();
    if (localPath.isEmpty()) {
        localPath = imagePath;
    }

    QString finalImagePath;
    if (!localPath.isEmpty() && QFileInfo(localPath).exists()) {
        finalImagePath = QUrl::fromLocalFile(localPath).toString();
    } else {
        finalImagePath = localPath;
    }

    for (int i = 0; i < m_gamePresets.size(); ++i) {
        QVariantMap preset = m_gamePresets[i].toMap();
        if (preset.value("id").toString() == presetId) {
            preset["previewImage"] = finalImagePath;
            m_gamePresets[i] = preset;
            emit gamePresetsChanged();
            emit presetPreviewChanged(presetId);
            savePresets();
            return;
        }
    }
}

QString ModManager::getPresetPreview(const QString& presetId) {
    if (presetId.isEmpty()) {
        return "";
    }

    for (const QVariant& v : m_gamePresets) {
        const QVariantMap preset = v.toMap();
        if (preset.value("id").toString() == presetId) {
            QString imagePath = preset.value("previewImage").toString();
            if (imagePath.isEmpty()) {
                return "";
            }

            QString localPath = QUrl(imagePath).toLocalFile();
            if (localPath.isEmpty()) {
                localPath = imagePath;
            }

            QFileInfo fileInfo(localPath);
            if (!fileInfo.exists() || !fileInfo.isReadable()) {
                return "";
            }

            return imagePath;
        }
    }
    return "";
}

void ModManager::removePresetPreview(const QString& presetId) {
    if (presetId.isEmpty()) {
        return;
    }

    for (int i = 0; i < m_gamePresets.size(); ++i) {
        QVariantMap preset = m_gamePresets[i].toMap();
        if (preset.value("id").toString() == presetId) {
            preset.remove("previewImage");
            m_gamePresets[i] = preset;
            emit gamePresetsChanged();
            emit presetPreviewChanged(presetId);
            savePresets();
            return;
        }
    }
}

void ModManager::togglePresetBlur(const QString& presetId) {
    if (presetId.isEmpty()) {
        return;
    }

    for (int i = 0; i < m_gamePresets.size(); ++i) {
        QVariantMap preset = m_gamePresets[i].toMap();
        if (preset.value("id").toString() == presetId) {
            bool currentBlur = preset.value("blurBackground", false).toBool();
            preset["blurBackground"] = !currentBlur;
            m_gamePresets[i] = preset;
            emit gamePresetsChanged();
            savePresets();
            return;
        }
    }
}

void ModManager::createPresetAndReset(const QString& name) {

    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QVariantMap newPreset;
    newPreset["id"] = id;
    newPreset["name"] = name;
    newPreset["modCount"] = 0;
    newPreset["previewImage"] = "";
    newPreset["blurBackground"] = false;

    newPreset["gamePath"] = "";
    newPreset["modsPath"] = "";
    newPreset["downloadsPath"] = "";
    newPreset["backupPath"] = "";
    newPreset["runtimeOutputPath"] = "";
    newPreset["gamePaksPath"] = "";
    newPreset["usmapFilePath"] = "";
    newPreset["aesKey"] = "";

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

    QString luaFolderPath;
    if (m_luaProfileManager) {
        const QString luaError = m_luaProfileManager->createProfileFolder(id, name);
        emit luaProfileFolderResult(name, luaError);
        if (!luaError.isEmpty())
            emit luaProfileCreateFailed(name, luaError);
        luaFolderPath = m_luaProfileManager->profileFolderPath(id, name);
        m_luaProfileManager->setActiveProfileId(id);
    }
    newPreset["luaModsPath"] = luaFolderPath;
    newPreset["useSharedLuaMods"] = false;

    m_gamePresets.append(newPreset);

    applyPresetToCurrentState(newPreset);

    m_activePresetId = id;
    emit activePresetIdChanged();
    emit gamePresetsChanged();

    savePresets();

    if (!luaFolderPath.isEmpty()) {
        setAvaloniumLuaModsPath(luaFolderPath);
    }

    emit navigateToGeneralTab();
}
