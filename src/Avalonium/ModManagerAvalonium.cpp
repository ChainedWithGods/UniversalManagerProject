#include "ModManagerInternal.h"

using namespace mmdetail;

QString ModManager::resolveAvaloniumDllPath() const {
    const QStringList candidates = {
        "Tools/Avalonium/Runtime/DLL/Avalonium.dll",
        "Tools/Avalonium/Runtime/out/build/Avalonium/bin/Avalonium.dll",
        "Tools/Avalonium/Runtime/out/build/x64-Release/bin/Avalonium.dll",
        "Tools/Avalonium/Runtime/out/build/x64-Debug/bin/Avalonium.dll",
        "Tools/Avalonium/Runtime/cmake-build-release/bin/Avalonium.dll",
        "Tools/Avalonium/Runtime/cmake-build-debug/bin/Avalonium.dll",
        "Tools/Avalonium/Avalonium.dll"
    };

    for (const QString& rel : candidates) {
        const QString p = resolveToolPath(rel);
        if (!p.isEmpty()) {
            return p;
        }
    }

    return QString();
}

QString ModManager::avaloniumConfigPath() const {
    const QString dllPath = resolveAvaloniumDllPath();
    if (!dllPath.isEmpty()) {
        const QString iniBesideDll = QDir(QFileInfo(dllPath).absolutePath()).filePath("Avalonium.ini");
        if (QFileInfo::exists(iniBesideDll)) {
            return iniBesideDll;
        }
    }

    const QStringList candidates = {
        "Tools/Avalonium/Runtime/DLL/Avalonium.ini",
        "Tools/Avalonium/Runtime/out/build/Avalonium/bin/Avalonium.ini",
        "Tools/Avalonium/Runtime/out/build/x64-Release/bin/Avalonium.ini",
        "Tools/Avalonium/Runtime/out/build/x64-Debug/bin/Avalonium.ini",
        "Tools/Avalonium/Runtime/cmake-build-release/bin/Avalonium.ini",
        "Tools/Avalonium/Runtime/cmake-build-debug/bin/Avalonium.ini",
        "Tools/Avalonium/Avalonium.ini"
    };

    for (const QString& rel : candidates) {
        const QString p = resolveToolPath(rel);
        if (!p.isEmpty()) {
            return p;
        }
    }

    const QString projectRoot = findProjectRoot();
    if (!projectRoot.isEmpty()) {
        return QDir(projectRoot).filePath("Tools/Avalonium/Runtime/DLL/Avalonium.ini");
    }

    return QString();
}

QVariantMap ModManager::loadAvaloniumConfig() const {
    const QStringList boolKeys = {
        QStringLiteral("DumpAllDeveloperCommands"),
        QStringLiteral("UseWndProcHook"),
        QStringLiteral("UnrealEngineConsole"),
        QStringLiteral("InGameGUI"),
        QStringLiteral("SkipPatternScan"),
        QStringLiteral("EnableDiscoveryCache"),
        QStringLiteral("VerboseLog"),
        QStringLiteral("EnableLuaMods"),
        QStringLiteral("EnableCppMods"),
        QStringLiteral("SDKValidation"),
        QStringLiteral("GenerateBuildSystem"),
        QStringLiteral("SDKLibclangGeneration"),
        QStringLiteral("AutoDiscoverAESKey")
    };

    QVariantMap config;
    for (const QString& key : boolKeys) {
        config[key] = false;
    }
    config["InitDelaySeconds"] = 0;
    config["InitDelayMinutes"] = 0;
    config["SDKPath"] = QString();
    config["LibclangPath"] = QString();
    config["LuaModsPath"] = QString();
    config["CppModsPath"] = QString();

    config["WatchdogEnabled"] = false;
    config["WatchdogLogDirectory"] = QStringLiteral("CrashHandler/Crashes");
    config["WatchdogDumpRegisters"] = true;
    config["WatchdogCatchFastFail"] = true;
    config["WatchdogTerminationBreakpoint"] = true;
    config["WatchdogAttachDebugger"] = false;
    config["WatchdogWriteMiniDump"] = true;
    config["WatchdogMaxStackDepth"] = 64;
    config["WatchdogOtherThreadStackDepth"] = 8;

    const QString configPath = avaloniumConfigPath();
    config["path"] = configPath;

    QFile file(configPath);
    if (configPath.isEmpty() || !file.exists()) {
        config["error"] = QStringLiteral("Avalonium.ini was not found");
        return config;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        config["error"] = QStringLiteral("Could not read Avalonium.ini");
        return config;
    }

    const QString contents = QString::fromUtf8(file.readAll());
    const QStringList lines = contents.split(QRegularExpression("\\r\\n|\\n|\\r"));
    auto parseBool = [](QString value) {
        value = value.trimmed().toLower();
        return value == QStringLiteral("true") || value == QStringLiteral("1") ||
               value == QStringLiteral("yes") || value == QStringLiteral("on");
    };

    QString currentSection;
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(';')) {
            continue;
        }

        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            currentSection = trimmed.mid(1, trimmed.length() - 2).trimmed();
            continue;
        }

        const int separator = trimmed.indexOf('=');
        if (separator <= 0) {
            continue;
        }

        const QString key = trimmed.left(separator).trimmed();
        const QString value = trimmed.mid(separator + 1).trimmed();

        if (currentSection.compare(QStringLiteral("AggressiveDog"), Qt::CaseInsensitive) == 0) {
            if (key == QStringLiteral("Enabled")) {
                config["WatchdogEnabled"] = parseBool(value);
            } else if (key == QStringLiteral("LogDirectory")) {
                config["WatchdogLogDirectory"] = value;
            } else if (key == QStringLiteral("DumpRegisters")) {
                config["WatchdogDumpRegisters"] = parseBool(value);
            } else if (key == QStringLiteral("CatchFastFail")) {
                config["WatchdogCatchFastFail"] = parseBool(value);
            } else if (key == QStringLiteral("TerminationBreakpoint")) {
                config["WatchdogTerminationBreakpoint"] = parseBool(value);
            } else if (key == QStringLiteral("AttachDebugger")) {
                config["WatchdogAttachDebugger"] = parseBool(value);
            } else if (key == QStringLiteral("WriteMiniDump")) {
                config["WatchdogWriteMiniDump"] = parseBool(value);
            } else if (key == QStringLiteral("MaxStackDepth")) {
                bool ok = false;
                const int depth = value.toInt(&ok);
                if (ok) {
                    config["WatchdogMaxStackDepth"] = qBound(1, depth, 256);
                }
            } else if (key == QStringLiteral("OtherThreadStackDepth")) {
                bool ok = false;
                const int depth = value.toInt(&ok);
                if (ok) {
                    config["WatchdogOtherThreadStackDepth"] = qBound(0, depth, 64);
                }
            }
        }

        if (boolKeys.contains(key)) {
            config[key] = parseBool(value);
        } else if (key == QStringLiteral("InitDelaySeconds") || key == QStringLiteral("InitDelayMinutes")) {
            bool ok = false;
            const int delay = value.toInt(&ok);
            if (ok) {
                config[key] = qMax(0, delay);
            }
        } else if (key == QStringLiteral("SDKPath") || key == QStringLiteral("LibclangPath") || key == QStringLiteral("LuaModsPath") || key == QStringLiteral("CppModsPath")) {
            config[key] = value;
        }
    }

    return config;
}

namespace {

QJsonArray readLuaModsJsonArray(const QString& modsDirectory, bool* parsedOk, QString* errorOut)
{
    *parsedOk = false;
    const QString jsonPath = QDir(modsDirectory).filePath(QStringLiteral("mods.json"));
    QFile file(jsonPath);
    if (!file.exists()) {

        *parsedOk = true;
        return QJsonArray();
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut) *errorOut = QStringLiteral("Could not read mods.json");
        return QJsonArray();
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut) *errorOut = QStringLiteral("mods.json is not valid JSON");
        return QJsonArray();
    }

    *parsedOk = true;
    return doc.object().value(QStringLiteral("mods")).toArray();
}

QString writeLuaModsJsonArray(const QString& modsDirectory, const QJsonArray& mods)
{
    QDir dir(modsDirectory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return QStringLiteral("Mods folder could not be created");
    }

    QJsonObject root;
    root.insert(QStringLiteral("mods"), mods);

    QSaveFile file(dir.filePath(QStringLiteral("mods.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QStringLiteral("Could not write mods.json");
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        return QStringLiteral("Could not save mods.json");
    }
    return QString();
}

bool luaModHasEntryScript(const QDir& modDir)
{
    return modDir.exists(QStringLiteral("main.lua"))
        || modDir.exists(QStringLiteral("scripts/main.lua"));
}

QString luaModsWriteBlockReason(const QString& modsDirectory, const ModManager* manager)
{
    if (modsDirectory.trimmed().isEmpty()) {
        return QStringLiteral("The Lua mods folder could not be resolved");
    }

    QDir dir(modsDirectory);
    if (!dir.exists()) {

        QDir parent(modsDirectory);
        if (parent.cdUp() && !parent.exists()) {
            return QStringLiteral("The Lua mods folder does not exist: %1").arg(modsDirectory);
        }
    }

    const QString driveLetter = QDir::fromNativeSeparators(modsDirectory).left(1).toUpper();
    const QString systemDrive = qEnvironmentVariableIsSet("SystemDrive")
        ? QString::fromUtf8(qgetenv("SystemDrive")).left(1).toUpper()
        : QStringLiteral("C");

    if (driveLetter == systemDrive && !manager->isRunningAsAdmin()) {
        return QStringLiteral("%1 is on the system drive (%2:). Restart Universal Manager as an administrator to save mod changes there.")
            .arg(modsDirectory, driveLetter);
    }

    const QString probePath = dir.filePath(QStringLiteral(".uam_write_probe"));
    QFile probe(probePath);
    if (!probe.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QStringLiteral("The Lua mods folder is write-protected (%1). Try running Universal Manager as an administrator.")
            .arg(modsDirectory);
    }
    probe.close();
    probe.remove();

    return QString();
}

}

QString ModManager::avaloniumConfigWriteBlockReason() const
{
    const QString configPath = avaloniumConfigPath();
    if (configPath.isEmpty()) {
        return QStringLiteral("The Avalonium.ini path could not be resolved");
    }

    const QDir dir = QFileInfo(configPath).absoluteDir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return QStringLiteral("The Avalonium.ini folder could not be created (%1)")
            .arg(QDir::toNativeSeparators(dir.absolutePath()));
    }

    const QString driveLetter = QDir::fromNativeSeparators(configPath).left(1).toUpper();
    const QString systemDrive = qEnvironmentVariableIsSet("SystemDrive")
        ? QString::fromUtf8(qgetenv("SystemDrive")).left(1).toUpper()
        : QStringLiteral("C");

    if (driveLetter == systemDrive && !isRunningAsAdmin()) {
        return QStringLiteral("%1 is on the system drive (%2:). Restart Universal Manager as an administrator to save runtime settings there.")
            .arg(QDir::toNativeSeparators(configPath), driveLetter);
    }

    const QString probePath = dir.filePath(QStringLiteral(".uam_write_probe"));
    QFile probe(probePath);
    if (!probe.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QStringLiteral("%1 is write-protected. Restart Universal Manager as an administrator to save runtime settings there.")
            .arg(QDir::toNativeSeparators(configPath));
    }
    probe.close();
    probe.remove();

    return QString();
}

QString ModManager::luaModsDirectory(const QString& explicitPath) const
{
    if (!explicitPath.trimmed().isEmpty()) {
        return QDir::fromNativeSeparators(explicitPath.trimmed());
    }

    const QVariantMap config = loadAvaloniumConfig();
    const QString configuredPath = config.value(QStringLiteral("LuaModsPath")).toString().trimmed();
    if (!configuredPath.isEmpty()) {
        return QDir::fromNativeSeparators(configuredPath);
    }

    return defaultLuaModsDirectory();
}

QString ModManager::avaloniumDirectory() const
{
    const QString dllPath = resolveAvaloniumDllPath();
    if (!dllPath.isEmpty()) {
        return QFileInfo(dllPath).absolutePath();
    }
    const QString projectRoot = findProjectRoot();
    if (!projectRoot.isEmpty()) {
        const QString candidate = QDir(projectRoot).filePath(QStringLiteral("Tools/Avalonium/Runtime/DLL"));
        if (QDir(candidate).exists()) {
            return candidate;
        }
    }
    return QString();
}

QString ModManager::defaultLuaModsDirectory() const
{

    const QString baseDir = avaloniumDirectory();
    if (!baseDir.isEmpty()) {
        const QString modsDir = QDir(baseDir).filePath(QStringLiteral("Mods"));
        const QString avaloniumDir = QDir(modsDir).filePath(QStringLiteral("Avalonium"));
        if (QDir(avaloniumDir).exists()) {
            return avaloniumDir;
        }
        return modsDir;
    }

    return QString();
}

QVariantList ModManager::listLuaMods(const QString& luaModsPath) const {
    QVariantList result;

    const QString modsDirectory = luaModsDirectory(luaModsPath);
    if (modsDirectory.isEmpty()) {
        QVariantMap errorEntry;
        errorEntry.insert(QStringLiteral("error"), QStringLiteral("Could not resolve the Lua mods folder"));
        result.append(errorEntry);
        return result;
    }

    bool parsedOk = false;
    QString errorText;
    const QJsonArray listed = readLuaModsJsonArray(modsDirectory, &parsedOk, &errorText);
    if (!parsedOk) {
        QVariantMap errorEntry;
        errorEntry.insert(QStringLiteral("error"), errorText);
        result.append(errorEntry);
        return result;
    }

    struct LuaModRow { QString name; bool enabled; bool listed; bool hasEntry; };
    QVector<LuaModRow> rows;
    QSet<QString> seen;

    for (const QJsonValue& value : listed) {
        const QJsonObject obj = value.toObject();
        const QString name = obj.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty() || seen.contains(name)) {
            continue;
        }
        seen.insert(name);
        LuaModRow row;
        row.name = name;
        row.enabled = obj.value(QStringLiteral("enabled")).toBool(false);
        row.listed = true;
        row.hasEntry = luaModHasEntryScript(QDir(modsDirectory + QLatin1Char('/') + name));
        rows.append(row);
    }

    const QFileInfoList diskDirs = QDir(modsDirectory)
        .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::LocaleAware);
    for (const QFileInfo& fi : diskDirs) {
        const QString name = fi.fileName();
        if (name.compare(QStringLiteral("shared"), Qt::CaseInsensitive) == 0
                || name.compare(QStringLiteral("Cache"), Qt::CaseInsensitive) == 0
                || name.compare(QStringLiteral("Universal Manager Lua Mods"), Qt::CaseInsensitive) == 0
                || name.compare(QStringLiteral("Lua Profiles"), Qt::CaseInsensitive) == 0
                || seen.contains(name)) {
            continue;
        }
        if (!luaModHasEntryScript(QDir(fi.absoluteFilePath()))) {
            continue;
        }
        seen.insert(name);
        LuaModRow row;
        row.name = name;

        row.enabled = false;
        row.listed = false;
        row.hasEntry = true;
        rows.append(row);
    }

    for (const LuaModRow& row : rows) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), row.name);
        entry.insert(QStringLiteral("enabled"), row.enabled);
        entry.insert(QStringLiteral("listed"), row.listed);
        entry.insert(QStringLiteral("hasEntry"), row.hasEntry);
        entry.insert(QStringLiteral("modsFolder"), modsDirectory);
        entry.insert(QStringLiteral("readOnly"), !luaModsWriteBlockReason(modsDirectory, this).isEmpty());
        result.append(entry);
    }
    return result;
}

QString ModManager::setLuaModEnabled(const QString& modName, bool enabled, const QString& luaModsPath)
{
    const QString modsDirectory = luaModsDirectory(luaModsPath);
    if (modsDirectory.isEmpty()) {
        return QStringLiteral("Could not resolve the Lua mods folder");
    }

    const QString blockReason = luaModsWriteBlockReason(modsDirectory, this);
    if (!blockReason.isEmpty()) {
        return blockReason;
    }

    bool parsedOk = false;
    QString errorText;
    QJsonArray mods = readLuaModsJsonArray(modsDirectory, &parsedOk, &errorText);
    if (!parsedOk) {
        return errorText;
    }

    bool found = false;
    for (int i = 0; i < mods.size(); ++i) {
        QJsonObject obj = mods.at(i).toObject();
        if (obj.value(QStringLiteral("name")).toString() == modName) {
            obj.insert(QStringLiteral("enabled"), enabled);
            mods.replace(i, obj);
            found = true;
            break;
        }
    }
    if (!found) {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), modName);
        obj.insert(QStringLiteral("enabled"), enabled);
        mods.append(obj);
    }

    return writeLuaModsJsonArray(modsDirectory, mods);
}

QString ModManager::registerLuaModsDisabled(const QString& luaModsPath)
{
    const QString modsDirectory = luaModsDirectory(luaModsPath);
    if (modsDirectory.isEmpty()) {
        return QStringLiteral("Could not resolve the Lua mods folder");
    }

    const QString blockReason = luaModsWriteBlockReason(modsDirectory, this);
    if (!blockReason.isEmpty()) {
        return blockReason;
    }

    bool parsedOk = false;
    QString errorText;
    QJsonArray mods = readLuaModsJsonArray(modsDirectory, &parsedOk, &errorText);
    if (!parsedOk) {
        return errorText;
    }

    QSet<QString> listed;
    for (const QJsonValue& value : mods) {
        listed.insert(value.toObject().value(QStringLiteral("name")).toString().trimmed());
    }

    const QFileInfoList diskDirs = QDir(modsDirectory)
        .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::LocaleAware);
    bool changed = false;
    for (const QFileInfo& fi : diskDirs) {
        const QString name = fi.fileName();
        if (name.compare(QStringLiteral("shared"), Qt::CaseInsensitive) == 0
                || name.compare(QStringLiteral("Cache"), Qt::CaseInsensitive) == 0
                || name.compare(QStringLiteral("Universal Manager Lua Mods"), Qt::CaseInsensitive) == 0
                || name.compare(QStringLiteral("Lua Profiles"), Qt::CaseInsensitive) == 0
                || listed.contains(name)) {
            continue;
        }
        if (!luaModHasEntryScript(QDir(fi.absoluteFilePath()))) {
            continue;
        }

        QJsonObject obj;
        obj.insert(QStringLiteral("name"), name);
        obj.insert(QStringLiteral("enabled"), false);
        mods.append(obj);
        listed.insert(name);
        changed = true;
    }

    if (!changed) {
        return QString();
    }
    return writeLuaModsJsonArray(modsDirectory, mods);
}

QString ModManager::luaProfileFolderPath(const QString& presetId) const
{
    if (!m_luaProfileManager) {
        return QString();
    }
    return m_luaProfileManager->luaModsFolderForProfile(presetId);
}

QString ModManager::activeLuaModsRoot() const
{
    if (m_activePresetId.isEmpty() || isPresetUsingSharedLuaMods(m_activePresetId)) {
        return QString();
    }

    const int idx = findPresetIndexById(m_activePresetId);
    if (idx < 0) {
        return QString();
    }

    const QVariantMap preset = m_gamePresets[idx].toMap();
    const QString configuredPath = preset.value(QStringLiteral("luaModsPath")).toString().trimmed();
    if (!configuredPath.isEmpty()) {
        return QDir::fromNativeSeparators(configuredPath);
    }

    if (!m_luaProfileManager) {
        return QString();
    }

    return m_luaProfileManager->profileFolderPath(m_activePresetId);
}

QString ModManager::saveAvaloniumConfig(const QVariantMap& config) {
    const QString configPath = avaloniumConfigPath();
    if (configPath.isEmpty()) {
        return QStringLiteral("Avalonium.ini path could not be resolved");
    }

    const QStringList orderedKeys = {
        QStringLiteral("InGameGUI"),
        QStringLiteral("UnrealEngineConsole"),
        QStringLiteral("SkipPatternScan"),
        QStringLiteral("EnableDiscoveryCache"),
        QStringLiteral("VerboseLog"),
        QStringLiteral("UseWndProcHook"),
        QStringLiteral("DumpAllDeveloperCommands"),
        QStringLiteral("EnableLuaMods"),
        QStringLiteral("LuaModsPath"),
        QStringLiteral("EnableCppMods"),
        QStringLiteral("CppModsPath"),
        QStringLiteral("SDKValidation"),
        QStringLiteral("SDKPath"),
        QStringLiteral("GenerateBuildSystem"),
        QStringLiteral("SDKLibclangGeneration"),
        QStringLiteral("LibclangPath"),
        QStringLiteral("AutoDiscoverAESKey"),
        QStringLiteral("InitDelayMinutes"),
        QStringLiteral("InitDelaySeconds")
    };

    auto boolText = [](bool value) {
        return value ? QStringLiteral("true") : QStringLiteral("false");
    };

    QMap<QString, QString> replacements;
    replacements["DumpAllDeveloperCommands"] = boolText(config.value("DumpAllDeveloperCommands").toBool());
    replacements["UseWndProcHook"] = boolText(config.value("UseWndProcHook").toBool());
    replacements["UnrealEngineConsole"] = boolText(config.value("UnrealEngineConsole").toBool());
    replacements["InGameGUI"] = boolText(config.value("InGameGUI").toBool());
    replacements["SkipPatternScan"] = boolText(config.value("SkipPatternScan").toBool());
    replacements["EnableDiscoveryCache"] = boolText(config.value("EnableDiscoveryCache").toBool());
    replacements["VerboseLog"] = boolText(config.value("VerboseLog").toBool());
    replacements["EnableLuaMods"] = boolText(config.value("EnableLuaMods").toBool());
    replacements["LuaModsPath"] = config.value("LuaModsPath").toString();
    replacements["EnableCppMods"] = boolText(config.value("EnableCppMods").toBool());
    replacements["CppModsPath"] = config.value("CppModsPath").toString();
    replacements["SDKValidation"] = boolText(config.value("SDKValidation").toBool());
    replacements["GenerateBuildSystem"] = boolText(config.value("GenerateBuildSystem").toBool());
    replacements["SDKLibclangGeneration"] = boolText(config.value("SDKLibclangGeneration").toBool());
    replacements["SDKPath"] = config.value("SDKPath").toString();
    replacements["LibclangPath"] = config.value("LibclangPath").toString();
    replacements["AutoDiscoverAESKey"] = boolText(config.value("AutoDiscoverAESKey").toBool());
    replacements["InitDelaySeconds"] = QString::number(qMax(0, config.value("InitDelaySeconds").toInt()));
    replacements["InitDelayMinutes"] = QString::number(qMax(0, config.value("InitDelayMinutes").toInt()));

    const QStringList watchdogKeys = {
        QStringLiteral("Enabled"),
        QStringLiteral("LogDirectory"),
        QStringLiteral("DumpRegisters"),
        QStringLiteral("CatchFastFail"),
        QStringLiteral("TerminationBreakpoint"),
        QStringLiteral("AttachDebugger"),
        QStringLiteral("MaxStackDepth"),
        QStringLiteral("OtherThreadStackDepth"),
        QStringLiteral("WriteMiniDump")
    };

    QMap<QString, QString> watchdogReplacements;
    watchdogReplacements["Enabled"] = boolText(config.value("WatchdogEnabled").toBool());
    watchdogReplacements["LogDirectory"] = config.value("WatchdogLogDirectory").toString();
    watchdogReplacements["DumpRegisters"] = boolText(config.value("WatchdogDumpRegisters").toBool());
    watchdogReplacements["CatchFastFail"] = boolText(config.value("WatchdogCatchFastFail").toBool());
    watchdogReplacements["TerminationBreakpoint"] = boolText(config.value("WatchdogTerminationBreakpoint").toBool());
    watchdogReplacements["AttachDebugger"] = boolText(config.value("WatchdogAttachDebugger").toBool());
    watchdogReplacements["WriteMiniDump"] = boolText(config.value("WatchdogWriteMiniDump").toBool());
    watchdogReplacements["MaxStackDepth"] = QString::number(qBound(1, config.value("WatchdogMaxStackDepth").toInt(), 256));
    watchdogReplacements["OtherThreadStackDepth"] = QString::number(qBound(0, config.value("WatchdogOtherThreadStackDepth").toInt(), 64));

    QStringList lines;
    QFile readFile(configPath);
    if (readFile.exists() && readFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString contents = QString::fromUtf8(readFile.readAll());
        lines = contents.split(QRegularExpression("\\r\\n|\\n|\\r"));
    }

    if (lines.isEmpty()) {
        lines = {
            QStringLiteral(";Avalonium runtime configuration"),
            QString(),
            QStringLiteral("InGameGUI=false"),
            QStringLiteral(";Enable/Disable InGame Overlay"),
            QString(),
            QStringLiteral("UnrealEngineConsole=false"),
            QStringLiteral(";Disable/Enable Unreal Engine Console"),
            QString(),
            QStringLiteral("UseWndProcHook=false"),
            QStringLiteral(";Enable if you're experiencing crashes when unreal engine console is enabled"),
            QString(),
            QStringLiteral("DumpAllDeveloperCommands=false"),
            QStringLiteral(";Dump all developer commands"),
            QString(),
            QStringLiteral("EnableLuaMods=false"),
            QStringLiteral(";Enable Lua scripting and mod loader runtime"),
            QString(),
            QStringLiteral("LuaModsPath="),
            QStringLiteral(";Custom path for Lua mods folder (leave empty for default)"),
            QString(),
            QStringLiteral("EnableCppMods=false"),
            QStringLiteral(";Enable native C++ mods"),
            QString(),
            QStringLiteral("CppModsPath="),
            QStringLiteral(";Custom path for C++ mods folder (leave empty for default)"),
            QString(),
            QStringLiteral("SDKValidation=false"),
            QStringLiteral(";Run the advanced SDK validation"),
            QString(),
            QStringLiteral("VerboseLog=false"),
            QStringLiteral(";Enable verbose logging for diagnostics"),
            QString(),
            QStringLiteral("SDKPath="),
            QStringLiteral(";Folder containing SDKMetadata.json for validation"),
            QString(),
            QStringLiteral("GenerateBuildSystem=false"),
            QStringLiteral(";Automatically run GenerateBuild.py after dumping the SDK"),
            QString(),
            QStringLiteral("InitDelayMinutes=0"),
            QStringLiteral(";Set Initialization Delay in minutes"),
            QString(),
            QStringLiteral("InitDelaySeconds=0"),
            QStringLiteral(";Set Initialization Delay in seconds"),
            QString(),
            QStringLiteral("AutoDiscoverAESKey=false"),
            QStringLiteral(";Auto-discover and verify AES-256 encryption key from live memory at startup."),
            QString(),
            QStringLiteral("[AggressiveDog]"),
            QStringLiteral(";External crash watchdog for Avalonium. When enabled, Avalonium launches"),
            QStringLiteral(";CrashHandler\\AggressiveDog.exe beside Avalonium.dll to monitor this process."),
            QStringLiteral(";It records crash details for diagnosis; it does not prevent or fix a crash."),
            QStringLiteral(";A successful normal exit does not create a crash report."),
            QStringLiteral("Enabled=false"),
            QStringLiteral(";Folder for watchdog text reports, status log, and optional minidumps."),
            QStringLiteral("LogDirectory=CrashHandler/Crashes"),
            QStringLiteral(";Include the faulting thread's CPU registers in the text report."),
            QStringLiteral("DumpRegisters=true"),
            QStringLiteral(";Capture Windows fast-fail events in addition to standard fatal exceptions."),
            QStringLiteral("CatchFastFail=true"),
            QStringLiteral(";Arm a hardware breakpoint on NtTerminateProcess so abnormal terminations are caught."),
            QStringLiteral(";Disable when a protected game treats debug registers as tampering."),
            QStringLiteral("TerminationBreakpoint=true"),
            QStringLiteral(";Attach as a debugger for exception-level crash reports. Off by default because"),
            QStringLiteral(";protected games can treat an attached debugger as tampering and terminate."),
            QStringLiteral("AttachDebugger=false"),
            QStringLiteral(";Maximum call-stack frames for the faulting thread (1-256)."),
            QStringLiteral("MaxStackDepth=64"),
            QStringLiteral(";Maximum stack frames per other thread (0-64; 0 disables them)."),
            QStringLiteral("OtherThreadStackDepth=8"),
            QStringLiteral(";Write a minidump alongside the text report (can use substantial disk space)."),
            QStringLiteral("WriteMiniDump=true")
        };
    }

    int firstSectionLine = lines.size();
    int watchdogHeaderLine = -1;
    int watchdogEndLine = lines.size();
    for (int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines[i].trimmed();
        if (!(trimmed.startsWith('[') && trimmed.endsWith(']'))) {
            continue;
        }
        const QString section = trimmed.mid(1, trimmed.length() - 2).trimmed();
        if (firstSectionLine == lines.size()) {
            firstSectionLine = i;
        }
        if (watchdogHeaderLine < 0) {
            if (section.compare(QStringLiteral("AggressiveDog"), Qt::CaseInsensitive) == 0) {
                watchdogHeaderLine = i;
                watchdogEndLine = lines.size();
            }
        } else if (watchdogEndLine == lines.size()) {
            watchdogEndLine = i;
        }
    }

    QSet<QString> writtenKeys;
    QSet<QString> writtenWatchdogKeys;
    QString currentSection;
    for (QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            currentSection = trimmed.mid(1, trimmed.length() - 2).trimmed();
            continue;
        }
        if (trimmed.isEmpty() || trimmed.startsWith(';')) {
            continue;
        }

        const int separator = trimmed.indexOf('=');
        if (separator <= 0) {
            continue;
        }

        const QString key = trimmed.left(separator).trimmed();
        if (currentSection.compare(QStringLiteral("AggressiveDog"), Qt::CaseInsensitive) == 0
                && watchdogReplacements.contains(key)) {
            line = key + QStringLiteral("=") + watchdogReplacements.value(key);
            writtenWatchdogKeys.insert(key);
        } else if (replacements.contains(key)) {
            line = key + QStringLiteral("=") + replacements.value(key);
            writtenKeys.insert(key);
        }
    }

    QStringList missingRootKeys;
    for (const QString& key : orderedKeys) {
        if (!writtenKeys.contains(key)) {
            missingRootKeys.append(key + QStringLiteral("=") + replacements.value(key));
        }
    }
    if (!missingRootKeys.isEmpty()) {
        QStringList block;
        if (firstSectionLine > 0 && !lines[firstSectionLine - 1].trimmed().isEmpty()) {
            block.append(QString());
        }
        block += missingRootKeys;
        if (firstSectionLine < lines.size()) {
            block.append(QString());
        }
        const int inserted = block.size();
        for (int i = inserted - 1; i >= 0; --i) {
            lines.insert(firstSectionLine, block[i]);
        }
        if (watchdogHeaderLine >= 0) {
            watchdogHeaderLine += inserted;
            watchdogEndLine += inserted;
        }
        firstSectionLine += inserted;
    }

    QStringList missingWatchdogKeys;
    for (const QString& key : watchdogKeys) {
        if (!writtenWatchdogKeys.contains(key)) {
            missingWatchdogKeys.append(key + QStringLiteral("=") + watchdogReplacements.value(key));
        }
    }
    if (!missingWatchdogKeys.isEmpty()) {
        if (watchdogHeaderLine < 0) {
            if (!lines.isEmpty() && !lines.last().trimmed().isEmpty()) {
                lines.append(QString());
            }
            lines.append(QStringLiteral("[AggressiveDog]"));
            lines.append(QStringLiteral(";External crash watchdog for Avalonium."));
            lines.append(QStringLiteral(";Enable or disable the watchdog."));
            lines.append(QString());
            lines += missingWatchdogKeys;
        } else {
            for (int i = missingWatchdogKeys.size() - 1; i >= 0; --i) {
                lines.insert(watchdogEndLine, missingWatchdogKeys[i]);
            }
        }
    }

    QDir configDir = QFileInfo(configPath).absoluteDir();
    if (!configDir.exists() && !configDir.mkpath(QStringLiteral("."))) {
        return QStringLiteral("Could not create the Avalonium.ini folder at %1")
            .arg(QDir::toNativeSeparators(configPath));
    }

    QFile writeFile(configPath);
    if (!writeFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return QStringLiteral("Could not save Avalonium.ini at %1. Restart Universal Manager as an administrator and try again.")
            .arg(QDir::toNativeSeparators(configPath));
    }

    writeFile.write(lines.join('\n').toUtf8());
    writeFile.write("\n");
    return QString();
}

QString ModManager::setAvaloniumLuaModsPath(const QString& luaModsPath) {
    QVariantMap config = loadAvaloniumConfig();
    config[QStringLiteral("LuaModsPath")] = luaModsPath;
    return saveAvaloniumConfig(config);
}

void ModManager::updatePresetLuaModsConfig(const QString& presetId, const QString& luaModsPath, bool useSharedLuaMods) {
    if (presetId.trimmed().isEmpty()) {
        return;
    }
    const int idx = findPresetIndexById(presetId);
    if (idx < 0) {
        return;
    }
    QVariantMap preset = m_gamePresets[idx].toMap();
    preset[QStringLiteral("luaModsPath")] = luaModsPath;
    preset[QStringLiteral("useSharedLuaMods")] = useSharedLuaMods;
    m_gamePresets[idx] = preset;
    savePresets();
    emit gamePresetsChanged();
}

QString ModManager::universalRuntimeLogPath() const {
    if (!m_runtimeOutputPath.isEmpty()) {
        return QDir(m_runtimeOutputPath).filePath("Avalonium.log");
    }
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempDir.isEmpty()) {
        return QString();
    }
    return QDir(tempDir).filePath("Avalonium/Avalonium.log");
}

void ModManager::startRuntimeLogMonitor() {
    if (!m_runtimeLogPollTimer) {
        m_runtimeLogPollTimer = new QTimer(this);
        m_runtimeLogPollTimer->setInterval(500);
        connect(m_runtimeLogPollTimer, &QTimer::timeout, this, [this]() {
            pollRuntimeLogOnce();
        });
    }

    if (!m_runtimeLogPollTimer->isActive()) {
        m_runtimeLogPollTimer->start();
    }

    pollRuntimeLogOnce();
}

namespace {

bool runtimeLogHasMarker(const QString &log, const QString &marker) {
    if (log.isEmpty() || marker.isEmpty()) {
        return false;
    }
    const QStringList lines = log.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.trimmed().endsWith(marker)) {
            return true;
        }
    }
    return false;
}

bool runtimeLogShowsReady(const QString &log) {
    return runtimeLogHasMarker(log, QStringLiteral("Initialization successful"))
        && runtimeLogHasMarker(log, QStringLiteral("IOCP server started on port 20030"));
}

}

void ModManager::pollRuntimeLogOnce() {
    const QString logPath = universalRuntimeLogPath();
    if (logPath.isEmpty()) {
        return;
    }

    QFile f(logPath);
    if (!f.exists()) {
        return;
    }
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const qint64 size = f.size();
    if (m_runtimeLogReadPos > size) {
        m_runtimeLogReadPos = 0;
    }
    f.seek(m_runtimeLogReadPos);

    const QByteArray newData = f.readAll();
    m_runtimeLogReadPos = f.pos();
    f.close();

    if (!newData.isEmpty()) {
        m_universalRuntimeLog += QString::fromUtf8(newData);
        const int maxChars = 40000;
        if (m_universalRuntimeLog.size() > maxChars) {
            m_universalRuntimeLog = m_universalRuntimeLog.right(maxChars);
        }
        emit universalRuntimeLogChanged();
    }

    setRuntimeReady(runtimeLogShowsReady(m_universalRuntimeLog));

    if (m_initializing) {
        if (!isGameProcessRunning()) {
            m_lastLaunchedGamePid = 0;
            setInitializing(false);
            m_avaloniumStatus = "Game process closed";
            emit avaloniumStatusChanged();
        }
    }

    if (!m_iocpStatusRequested) {
        if (!m_initializing) {
            return;
        }

        if (runtimeReady()) {
            m_iocpStatusRequested = true;
            if (m_initializing) {
                setInitializing(false);
            }
            m_avaloniumStatus = "IOCP ready. Querying STATUS...";
            emit avaloniumStatusChanged();

            const QString status = queryIocpStatus().trimmed();
            if (status.isEmpty()) {
                m_avaloniumStatus = "STATUS returned no output";
            } else {
                m_avaloniumStatus = status;
            }
            emit avaloniumStatusChanged();
        }
    }
}

QString ModManager::injectAvaloniumDll() {
#ifndef Q_OS_WIN
    return "DLL injection is only supported on Windows";
#else

    if (checkAvaloniumConflict()) {
        emit avaloniumConflictDetected();
        return "Conflict detected: Avalonium.dll or UniversalManager.dll already exists in game directory";
    }

    if (m_isAdvancedProcessMode) {
        if (m_selectedProcessPid <= 0) {
            return "No process selected. Please choose a running process from the list";
        }
        m_lastLaunchedGamePid = m_selectedProcessPid;
    } else {
        if (m_lastLaunchedGamePid <= 0) {
            return "No launched game process found. Launch the game first";
        }
    }

    const QString injectorScript = resolveToolPath("Tools/Avalonium/Helpers/injector.py");
    if (injectorScript.isEmpty()) {
        return "Injector script not found (Tools/Avalonium/Helpers/injector.py)";
    }

    const QString dllPath = resolveAvaloniumDllPath();
    if (dllPath.isEmpty()) {
        return "Avalonium.dll not found.";
    }

    if (!m_universalRuntimeLog.isEmpty()) {
        m_universalRuntimeLog.clear();
        emit universalRuntimeLogChanged();
    }
    m_iocpStatusRequested = false;
    setRuntimeReady(false);
    m_runtimeLogReadPos = 0;

    const QString logPath = universalRuntimeLogPath();
    if (!logPath.isEmpty()) {
        QFile logFile(logPath);
        if (logFile.exists()) {
            (void)logFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
            logFile.close();
        }
    }

    setInitializing(true);

    if (!m_runtimeOutputPath.isEmpty()) {
        const QDir outDir(m_runtimeOutputPath);
        if (!outDir.exists()) {
            outDir.mkpath(".");
        }
        const QString logFilePath = QDir(m_runtimeOutputPath).filePath("Avalonium.log");
        const QFileInfo dllInfo(dllPath);
        const QString configPath = QDir(dllInfo.absolutePath()).filePath("injector_config.json");

        QJsonObject cfg;
        {
            QFile configRead(configPath);
            if (configRead.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QByteArray data = configRead.readAll();
                configRead.close();
                QJsonParseError perr;
                const QJsonDocument doc = QJsonDocument::fromJson(data, &perr);
                if (perr.error == QJsonParseError::NoError && doc.isObject()) {
                    cfg = doc.object();
                } else {
                    static const QRegularExpression kvRe(
                        R"FOO("\s*([A-Za-z_][A-Za-z0-9_]*)\s*"\s*:\s*(true|false|"[^"]*"))FOO",
                        QRegularExpression::CaseInsensitiveOption);
                    const QString text = QString::fromUtf8(data);
                    for (const QString &line : text.split(QLatin1Char('\n'))) {
                        const auto m = kvRe.match(line);
                        if (!m.hasMatch())
                            continue;
                        const QString key = m.captured(1);
                        const QString val = m.captured(2);
                        if (cfg.contains(key))
                            continue;
                        if (val == QLatin1String("true"))
                            cfg[key] = true;
                        else if (val == QLatin1String("false"))
                            cfg[key] = false;
                        else if (val.size() >= 2 && val.startsWith(QLatin1Char('"')) &&
                                 val.endsWith(QLatin1Char('"')))
                            cfg[key] = val.mid(1, val.length() - 2);
                    }
                }
            }
        }

        cfg["log_path"] = QDir::toNativeSeparators(logFilePath);

        cfg.remove("verbose_log");
        cfg.remove("debug_log");

        QFile configJson(configPath);
        if (configJson.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            configJson.write(QJsonDocument(cfg).toJson(QJsonDocument::Indented));
            configJson.close();
        }
    }

    startRuntimeLogMonitor();

    m_avaloniumStatus = "Injecting Avalonium...";
    emit avaloniumStatusChanged();

    QString targetArg;
    if (m_isAdvancedProcessMode && m_selectedProcessPid > 0) {
        targetArg = QString::number(m_selectedProcessPid);
    } else {
        QString executablePath = QUrl(m_gamePath).toLocalFile();
        if (executablePath.isEmpty()) {
            executablePath = m_gamePath;
        }
        executablePath = QDir::cleanPath(executablePath);
        const QString exeName = QFileInfo(executablePath).fileName();
        targetArg = exeName.isEmpty() ? QString::number(m_lastLaunchedGamePid) : exeName;
    }

    QProcess p;
    p.setProgram("python");
    p.setArguments({
        "-u",
        injectorScript,
        targetArg,
        dllPath
    });
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start();

    if (!p.waitForStarted(5000)) {
        setInitializing(false);
        return "Failed to start python injector (python not found?)";
    }
    if (!p.waitForFinished(30000)) {
        p.kill();
        p.waitForFinished(2000);
        setInitializing(false);
        return "Injector timed out";
    }

    const QString out = QString::fromUtf8(p.readAll()).trimmed();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        m_avaloniumStatus = out.isEmpty() ? "Injection failed" : out;
        emit avaloniumStatusChanged();
        setInitializing(false);
        return out.isEmpty() ? "Injection failed" : out;
    }

    m_avaloniumStatus = "Injection complete. Waiting for initialization...";
    emit avaloniumStatusChanged();
    return out.isEmpty() ? "Injected Avalonium DLL" : out;
#endif
}

bool ModManager::checkAvaloniumConflict() {
#ifndef Q_OS_WIN
    return false;
#else
    if (m_gamePath.isEmpty()) {
        return false;
    }

    QString executablePath = QUrl(m_gamePath).toLocalFile();
    if (executablePath.isEmpty()) {
        executablePath = m_gamePath;
    }
    executablePath = QDir::cleanPath(executablePath);

    QFileInfo exeInfo(executablePath);
    if (!exeInfo.exists() || !exeInfo.isFile()) {
        return false;
    }

    QDir binDir = exeInfo.absoluteDir();
    QString dllPath = binDir.filePath("Avalonium.dll");
    QString legacyDllPath = binDir.filePath("UniversalManager.dll");

    return QFile::exists(dllPath) || QFile::exists(legacyDllPath);
#endif
}

bool ModManager::deleteConflictingAvaloniumDll() {
#ifndef Q_OS_WIN
    return false;
#else
    if (m_gamePath.isEmpty()) {
        return false;
    }

    QString executablePath = QUrl(m_gamePath).toLocalFile();
    if (executablePath.isEmpty()) {
        executablePath = m_gamePath;
    }
    executablePath = QDir::cleanPath(executablePath);

    QFileInfo exeInfo(executablePath);
    if (!exeInfo.exists() || !exeInfo.isFile()) {
        return false;
    }

    QDir binDir = exeInfo.absoluteDir();
    const QStringList dllCandidates = { binDir.filePath("Avalonium.dll"), binDir.filePath("UniversalManager.dll") };
    bool removedAny = false;

    for (const QString& dllPath : dllCandidates) {
        if (QFile::exists(dllPath)) {
            HANDLE hFile = CreateFileW(
                dllPath.toStdWString().c_str(),
                DELETE,
                0,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );

            if (hFile == INVALID_HANDLE_VALUE) {
                DWORD error = GetLastError();
                if (error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION) {
                    qDebug() << dllPath << "is currently in use by the game";
                    return false;
                }
                qDebug() << "Failed to access" << dllPath << ", error code:" << error;
                return false;
            }

            CloseHandle(hFile);
            if (QFile::remove(dllPath)) {
                removedAny = true;
            }
        }
    }

    return removedAny;
#endif
}

void ModManager::killGameProcess() {
#ifdef Q_OS_WIN
    if (m_lastLaunchedGamePid > 0) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(m_lastLaunchedGamePid));
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
        m_lastLaunchedGamePid = 0;
    }

    if (!m_gamePath.isEmpty()) {
        QString executablePath = QUrl(m_gamePath).toLocalFile();
        if (executablePath.isEmpty()) {
            executablePath = m_gamePath;
        }
        executablePath = QDir::cleanPath(executablePath);
        const QString exeName = QFileInfo(executablePath).fileName();
        QString cleanBase = QFileInfo(executablePath).completeBaseName();
        cleanBase.remove(QRegularExpression("-Win64-Shipping$", QRegularExpression::CaseInsensitiveOption));
        cleanBase.remove(QRegularExpression("-Win64-Test$", QRegularExpression::CaseInsensitiveOption));
        cleanBase.remove(QRegularExpression("-Win64-Development$", QRegularExpression::CaseInsensitiveOption));

        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe;
            pe.dwSize = sizeof(PROCESSENTRY32W);
            if (Process32FirstW(hSnapshot, &pe)) {
                do {
                    const QString procName = QString::fromWCharArray(pe.szExeFile);
                    bool match = false;
                    if (!exeName.isEmpty() && procName.compare(exeName, Qt::CaseInsensitive) == 0) {
                        match = true;
                    } else if (!cleanBase.isEmpty() && procName.startsWith(cleanBase, Qt::CaseInsensitive) && procName.endsWith(".exe", Qt::CaseInsensitive)) {
                        match = true;
                    }
                    if (match) {
                        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                        if (hProc) {
                            TerminateProcess(hProc, 0);
                            CloseHandle(hProc);
                        }
                    }
                } while (Process32NextW(hSnapshot, &pe));
            }
            CloseHandle(hSnapshot);
        }
    }

    setInitializing(false);
    setRuntimeReady(false);
    m_avaloniumStatus = "Initialization cancelled - Game process terminated";
    emit avaloniumStatusChanged();
#endif
}

bool ModManager::isGameProcessRunning() const {
#ifndef Q_OS_WIN
    return false;
#else
    if (m_isAdvancedProcessMode && m_selectedProcessPid > 0) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(m_selectedProcessPid));
        if (hProcess) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                CloseHandle(hProcess);
                return true;
            }
            CloseHandle(hProcess);
        }
        return false;
    }

    if (m_lastLaunchedGamePid > 0) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(m_lastLaunchedGamePid));
        if (hProcess) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                CloseHandle(hProcess);
                return true;
            }
            CloseHandle(hProcess);
        }
    }

    if (m_gamePath.isEmpty()) {
        return false;
    }

    QString executablePath = QUrl(m_gamePath).toLocalFile();
    if (executablePath.isEmpty()) {
        executablePath = m_gamePath;
    }
    executablePath = QDir::cleanPath(executablePath);
    const QString exeName = QFileInfo(executablePath).fileName();
    QString cleanBase = QFileInfo(executablePath).completeBaseName();
    cleanBase.remove(QRegularExpression("-Win64-Shipping$", QRegularExpression::CaseInsensitiveOption));
    cleanBase.remove(QRegularExpression("-Win64-Test$", QRegularExpression::CaseInsensitiveOption));
    cleanBase.remove(QRegularExpression("-Win64-Development$", QRegularExpression::CaseInsensitiveOption));

    if (exeName.isEmpty() && cleanBase.isEmpty()) {
        return false;
    }

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    bool running = false;
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            const QString procName = QString::fromWCharArray(pe.szExeFile);
            if (!exeName.isEmpty() && procName.compare(exeName, Qt::CaseInsensitive) == 0) {
                running = true;
                break;
            }
            if (!cleanBase.isEmpty() && procName.startsWith(cleanBase, Qt::CaseInsensitive) && procName.endsWith(".exe", Qt::CaseInsensitive)) {
                running = true;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return running;
#endif
}

bool ModManager::isAvaloniumInjected() const {
#ifndef Q_OS_WIN
    return false;
#else
    auto checkPidHasDll = [](DWORD pid) -> bool {
        if (pid == 0) return false;

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (hSnap != INVALID_HANDLE_VALUE) {
            MODULEENTRY32W me;
            me.dwSize = sizeof(MODULEENTRY32W);
            if (Module32FirstW(hSnap, &me)) {
                do {
                    const QString modName = QString::fromWCharArray(me.szModule);
                    if (modName.compare(QStringLiteral("Avalonium.dll"), Qt::CaseInsensitive) == 0 ||
                        modName.compare(QStringLiteral("UniversalManager.dll"), Qt::CaseInsensitive) == 0) {
                        CloseHandle(hSnap);
                        return true;
                    }
                } while (Module32NextW(hSnap, &me));
            }
            CloseHandle(hSnap);
        }

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (hProcess) {
            HMODULE hMods[1024];
            DWORD cbNeeded = 0;
            if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
                const DWORD count = cbNeeded / sizeof(HMODULE);
                for (DWORD i = 0; i < count; i++) {
                    WCHAR szModName[MAX_PATH];
                    if (GetModuleBaseNameW(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(WCHAR))) {
                        if (_wcsicmp(szModName, L"Avalonium.dll") == 0 || _wcsicmp(szModName, L"UniversalManager.dll") == 0) {
                            CloseHandle(hProcess);
                            return true;
                        }
                    }
                }
            }
            CloseHandle(hProcess);
        }

        return false;
    };

    if (m_isAdvancedProcessMode && m_selectedProcessPid > 0) {
        return checkPidHasDll(static_cast<DWORD>(m_selectedProcessPid));
    }

    if (m_gamePath.isEmpty() && m_lastLaunchedGamePid <= 0) {
        return false;
    }

    if (m_lastLaunchedGamePid > 0) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(m_lastLaunchedGamePid));
        if (hProcess) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                CloseHandle(hProcess);
                if (checkPidHasDll(static_cast<DWORD>(m_lastLaunchedGamePid))) {
                    return true;
                }
            } else {
                CloseHandle(hProcess);
            }
        }
    }

    if (!m_gamePath.isEmpty()) {
        QString executablePath = QUrl(m_gamePath).toLocalFile();
        if (executablePath.isEmpty()) {
            executablePath = m_gamePath;
        }
        executablePath = QDir::cleanPath(executablePath);
        const QString exeName = QFileInfo(executablePath).fileName();
        QString cleanBase = QFileInfo(executablePath).completeBaseName();
        cleanBase.remove(QRegularExpression("-Win64-Shipping$", QRegularExpression::CaseInsensitiveOption));
        cleanBase.remove(QRegularExpression("-Win64-Test$", QRegularExpression::CaseInsensitiveOption));
        cleanBase.remove(QRegularExpression("-Win64-Development$", QRegularExpression::CaseInsensitiveOption));

        if (!exeName.isEmpty() || !cleanBase.isEmpty()) {
            HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnapshot != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32W pe;
                pe.dwSize = sizeof(PROCESSENTRY32W);
                if (Process32FirstW(hSnapshot, &pe)) {
                    do {
                        const QString procName = QString::fromWCharArray(pe.szExeFile);
                        bool match = false;
                        if (!exeName.isEmpty() && procName.compare(exeName, Qt::CaseInsensitive) == 0) {
                            match = true;
                        } else if (!cleanBase.isEmpty() && procName.startsWith(cleanBase, Qt::CaseInsensitive) && procName.endsWith(".exe", Qt::CaseInsensitive)) {
                            match = true;
                        }
                        if (match) {
                            if (checkPidHasDll(pe.th32ProcessID)) {
                                CloseHandle(hSnapshot);
                                return true;
                            }
                        }
                    } while (Process32NextW(hSnapshot, &pe));
                }
                CloseHandle(hSnapshot);
            }
        }
    }

    return false;
#endif
}

bool ModManager::isGameOnCDrive() {
#ifndef Q_OS_WIN
    return false;
#else
    if (m_gamePath.isEmpty()) {
        return false;
    }

    QString executablePath = QUrl(m_gamePath).toLocalFile();
    if (executablePath.isEmpty()) {
        executablePath = m_gamePath;
    }
    executablePath = QDir::cleanPath(executablePath);

    if (executablePath.length() >= 2) {
        QChar driveLetter = executablePath.at(0).toUpper();
        return (driveLetter == 'C' && executablePath.at(1) == ':');
    }

    return false;
#endif
}

QString ModManager::queryIocpStatus() {
#ifndef Q_OS_WIN
    return "IOCP status is only supported on Windows";
#else
    const QString clientScript = resolveToolPath("Tools/Avalonium/Helpers/iocp_client.py");
    if (clientScript.isEmpty()) {
        return "IOCP client script not found (Tools/Avalonium/Helpers/iocp_client.py)";
    }

    QProcess p;
    p.setProgram("python");
    p.setArguments({"-u", clientScript, "STATUS", "-q"});
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start();

    if (!p.waitForStarted(5000)) {
        return "Failed to start python IOCP client (python not found?)";
    }
    if (!p.waitForFinished(10000)) {
        p.kill();
        p.waitForFinished(2000);
        return "IOCP status request timed out";
    }

    const QString out = QString::fromUtf8(p.readAll());
    return out.trimmed();
#endif
}

bool ModManager::generating() const { return m_generating; }

void ModManager::setGenerating(bool v) {
    if (m_generating != v) {
        m_generating = v;
        emit generatingChanged();
    }
}

bool ModManager::initializing() const { return m_initializing; }

void ModManager::setInitializing(bool v) {
    if (m_initializing != v) {
        m_initializing = v;
        emit initializingChanged();
    }
}

bool ModManager::runtimeReady() const { return m_runtimeReady; }

void ModManager::setRuntimeReady(bool v) {
    if (m_runtimeReady != v) {
        m_runtimeReady = v;
        emit runtimeReadyChanged();
    }
}

void ModManager::generateUsmap() {
#ifndef Q_OS_WIN
    emit usmapGenerated("USMAP generation is only supported on Windows");
#else
    const QString clientScript = resolveToolPath("Tools/Avalonium/Helpers/iocp_client.py");
    if (clientScript.isEmpty()) {
        emit usmapGenerated("IOCP client script not found");
        return;
    }

    QStringList args = {"-u", clientScript, "-q", "-t", "600"};
    if (!m_runtimeOutputPath.isEmpty()) {
        const QDir outDir(m_runtimeOutputPath);
        if (!outDir.exists()) {
            outDir.mkpath(".");
        }
        const QString gameTitle = resolveRuntimeGameTitle();
        const QString gameDir = QDir(m_runtimeOutputPath).filePath(gameTitle);
        QDir().mkpath(gameDir);
        const QString usmapDir = QDir(gameDir).filePath("USMAP");
        QDir().mkpath(usmapDir);
        const QString usmapPath = QDir(usmapDir).filePath(gameTitle + ".usmap");
        args << "USMAP" << usmapPath;
    } else {
        args << "USMAP";
    }

    setGenerating(true);

    auto* p = new QProcess(this);
    p->setProgram("python");
    p->setArguments(args);
    p->setProcessChannelMode(QProcess::MergedChannels);

    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [this, p](int exitCode, QProcess::ExitStatus exitStatus) {
            const QString out = QString::fromUtf8(p->readAll()).trimmed();
            QString msg;
            if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                msg = out.isEmpty() ? "USMAP generation failed" : out;
            } else {
                msg = out.isEmpty() ? "USMAP generated" : out;
            }
            setGenerating(false);
            emit usmapGenerated(msg);
            p->deleteLater();
        });

    p->start();
    if (!p->waitForStarted(5000)) {
        setGenerating(false);
        emit usmapGenerated("Failed to start python IOCP client (python not found?)");
        p->deleteLater();
    }
#endif
}

void ModManager::generateSdk() {
#ifndef Q_OS_WIN
    emit sdkGenerated("SDK generation is only supported on Windows");
#else
    const QString clientScript = resolveToolPath("Tools/Avalonium/Helpers/iocp_client.py");
    if (clientScript.isEmpty()) {
        emit sdkGenerated("IOCP client script not found");
        return;
    }

    QStringList args = {"-u", clientScript, "-q", "-t", "600"};
    if (!m_runtimeOutputPath.isEmpty()) {
        const QDir outDir(m_runtimeOutputPath);
        if (!outDir.exists()) {
            outDir.mkpath(".");
        }
        const QString gameTitle = resolveRuntimeGameTitle();
        const QString gameDir = QDir(m_runtimeOutputPath).filePath(gameTitle);
        QDir().mkpath(gameDir);
        const QString sdkDir = QDir(gameDir).filePath("SDK");
        QDir().mkpath(sdkDir);
        args << "GENSDK" << sdkDir;
    } else {
        args << "GENSDK";
    }

    setGenerating(true);

    auto* p = new QProcess(this);
    p->setProgram("python");
    p->setArguments(args);
    p->setProcessChannelMode(QProcess::MergedChannels);

    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [this, p](int exitCode, QProcess::ExitStatus exitStatus) {
            const QString out = QString::fromUtf8(p->readAll()).trimmed();
            QString msg;
            if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                msg = out.isEmpty() ? "SDK generation failed" : out;
            } else {
                msg = out.isEmpty() ? "SDK generated" : out;
            }
            setGenerating(false);
            emit sdkGenerated(msg);
            p->deleteLater();
        });

    p->start();
    if (!p->waitForStarted(5000)) {
        setGenerating(false);
        emit sdkGenerated("Failed to start python IOCP client (python not found?)");
        p->deleteLater();
    }
#endif
}

QString ModManager::universalRuntimeLog() const {
    return m_universalRuntimeLog;
}

QString ModManager::avaloniumStatus() const {
    if (!m_avaloniumStatus.isEmpty()) {
        return m_avaloniumStatus;
    }
    return "Waiting for DLL injection...";
}

void ModManager::clearUniversalRuntimeLog() {
    if (!m_universalRuntimeLog.isEmpty()) {
        m_universalRuntimeLog.clear();
        emit universalRuntimeLogChanged();
    }

    m_runtimeLogReadPos = 0;
    m_iocpStatusRequested = false;

    const QString logPath = universalRuntimeLogPath();
    if (!logPath.isEmpty()) {
        QFile logFile(logPath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            logFile.close();
        }
    }
}
