#include "ModManagerInternal.h"
#include "NexusMedia.h"
#include "NexusOAuth.h"
#include "NexusWebSession.h"

using namespace mmdetail;

ModManager::ModManager(QObject *parent)
    : QObject(parent)
{
    m_luaProfileManager = new LuaProfileManager(this, this);

    m_nexusWebSession = new NexusWebSession(this);

    m_nexusOAuth = new NexusOAuth(this);
    connect(m_nexusOAuth, &NexusOAuth::authenticationChanged,
            this, &ModManager::onNexusOAuthAuthenticationChanged);
    connect(m_nexusOAuth, &NexusOAuth::userInfoChanged,
            this, &ModManager::onNexusOAuthUserInfoChanged);
    connect(m_nexusOAuth, &NexusOAuth::signedOut, this, [this]() {
        if (m_nexusWebSession)
            m_nexusWebSession->clearWebsiteSession();
    });

    connect(m_nexusOAuth, &NexusOAuth::preferencesChanged, this, [this]() {
        if (m_nexusOAuth->isAuthenticated() && m_nexusEnabled) {
            fetchNexusMods(0);
        }
    }, Qt::QueuedConnection);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &ModManager::onModsDirectoryChanged);

    m_downloadsWatcher = new QFileSystemWatcher(this);
    connect(m_downloadsWatcher, &QFileSystemWatcher::directoryChanged, this, &ModManager::onDownloadsDirectoryChanged);

    m_greetingTimer = new QTimer(this);
    connect(m_greetingTimer, &QTimer::timeout, this, &ModManager::updateGreeting);
    m_greetingTimer->start(1000);

    m_systemStatsTimer = new QTimer(this);
    connect(m_systemStatsTimer, &QTimer::timeout, this, &ModManager::updateSystemStats);
    m_systemStatsTimer->start(5000);

    updateSystemStats();

    m_lastGreeting = greeting();

    loadSettings();
    loadNexusArchives();
    loadNexusTrackedMods();
    QTimer::singleShot(600, this, [this]() {
        if (m_nexusOAuth) {
            m_nexusOAuth->restoreSession();
        }
    });
#ifdef Q_OS_WIN
    applyManagerConsoleVisibility(m_managerHideConsole);
#endif
    loadPresets();
    loadCollections();
    parseCustomGameVersionsFromEGameCs();

    QGuiApplication* app = qobject_cast<QGuiApplication*>(QGuiApplication::instance());
    if (app) {
        connect(app, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState) {
            applyPauseOnDefocusState();
        });
    }
    applyPauseOnDefocusState();

    if ((m_gamePaksPath.isEmpty() || !QDir(m_gamePaksPath).exists()) && !m_gamePath.isEmpty()) {
        const QString autoPaksPath = detectGamePaksPathFromGameExecutable(m_gamePath);
        if (!autoPaksPath.isEmpty() && m_gamePaksPath != autoPaksPath) {
            m_gamePaksPath = autoPaksPath;
            emit gamePaksPathChanged();
            saveSettings();
        }
    }

    QFileInfoList drives = QDir::drives();
    for (const QFileInfo& drive : drives) {
        m_availableDrives.append(drive.absoluteFilePath());
    }
    emit availableDrivesChanged();

    QTimer::singleShot(500, this, [this]() {

    });
}

ModManager::~ModManager()
{

    delete m_nexusMedia;
    m_nexusMedia = nullptr;
}

QString ModManager::getSettingsFilePath() const {
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDir.isEmpty()) {
        QString appDirFallback = QCoreApplication::applicationDirPath();
        return appDirFallback + "/settings.json";
    }
    QDir dir(appDataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return appDataDir + "/settings.json";
}

QString ModManager::appDataPath() const {
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDir.isEmpty()) {
        return QCoreApplication::applicationDirPath();
    }
    return appDataDir;
}

void ModManager::openAppDataFolder() {
    const QString dirPath = appDataPath();
    if (!dirPath.isEmpty()) {
        QDir dir(dirPath);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    }
}

QString ModManager::getShortcut(const QString& key, const QString& fallback) const
{
    const QString trimmedKey = key.trimmed();
    if (trimmedKey.isEmpty()) {
        return fallback;
    }

    const QString value = m_shortcuts.value(trimmedKey);
    if (!value.trimmed().isEmpty()) {
        return value;
    }

    const QString defaultValue = defaultShortcuts().value(trimmedKey);
    if (!defaultValue.isEmpty()) {
        return defaultValue;
    }

    return fallback;
}

void ModManager::setShortcut(const QString& key, const QString& shortcut)
{
    const QString trimmedKey = key.trimmed();
    const QString trimmedShortcut = shortcut.trimmed();
    if (trimmedKey.isEmpty() || trimmedShortcut.isEmpty()) {
        return;
    }

    if (m_shortcuts.value(trimmedKey) == trimmedShortcut) {
        return;
    }

    m_shortcuts[trimmedKey] = trimmedShortcut;
    saveSettings();
}

void ModManager::updateGreeting() {

    QString currentGreeting = greeting();
    if (currentGreeting != m_lastGreeting) {
        m_lastGreeting = currentGreeting;
        emit greetingChanged();
        emit currentDateChanged();
    }
}

double ModManager::cpuUsage() const {
    return m_cpuUsage;
}

double ModManager::ramUsage() const {
    return m_ramUsage;
}

QString ModManager::ramUsageText() const {
    const qint64 GB = 1024LL * 1024LL * 1024LL;
    double usedGB = m_usedRamBytes / static_cast<double>(GB);
    double totalGB = m_totalRamBytes / static_cast<double>(GB);
    return QString("%1 / %2 GB").arg(usedGB, 0, 'f', 1).arg(totalGB, 0, 'f', 1);
}

QVariantList ModManager::driveStats() const {
    return m_driveStats;
}

void ModManager::updateSystemStats() {
    if ((m_pauseStatsOnDefocus || m_pauseOnDefocus) && QGuiApplication::applicationState() != Qt::ApplicationActive) {
        return;
    }

#ifdef Q_OS_WIN

    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        quint64 idle = (static_cast<quint64>(idleTime.dwHighDateTime) << 32) | idleTime.dwLowDateTime;
        quint64 kernel = (static_cast<quint64>(kernelTime.dwHighDateTime) << 32) | kernelTime.dwLowDateTime;
        quint64 user = (static_cast<quint64>(userTime.dwHighDateTime) << 32) | userTime.dwLowDateTime;

        if (m_lastIdleTime != 0) {
            quint64 idleDiff = idle - m_lastIdleTime;
            quint64 kernelDiff = kernel - m_lastKernelTime;
            quint64 userDiff = user - m_lastUserTime;
            quint64 totalDiff = kernelDiff + userDiff;

            if (totalDiff > 0) {
                double newCpuUsage = 100.0 * (1.0 - static_cast<double>(idleDiff) / static_cast<double>(totalDiff));
                if (qAbs(m_cpuUsage - newCpuUsage) > 0.1) {
                    m_cpuUsage = newCpuUsage;
                    emit cpuUsageChanged();
                }
            }
        }

        m_lastIdleTime = idle;
        m_lastKernelTime = kernel;
        m_lastUserTime = user;
    }

    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        qint64 totalPhys = static_cast<qint64>(memInfo.ullTotalPhys);
        qint64 availPhys = static_cast<qint64>(memInfo.ullAvailPhys);
        qint64 usedPhys = totalPhys - availPhys;

        double newRamUsage = 100.0 * static_cast<double>(usedPhys) / static_cast<double>(totalPhys);

        const bool totalChanged = (m_totalRamBytes != totalPhys);
        const bool usedChangedEnough = (qAbs(usedPhys - m_usedRamBytes) > 64LL * 1024LL * 1024LL);
        const bool usageChangedEnough = (qAbs(m_ramUsage - newRamUsage) > 0.25);

        if (totalChanged || usedChangedEnough || usageChangedEnough) {
            m_totalRamBytes = totalPhys;
            m_usedRamBytes = usedPhys;
            m_ramUsage = newRamUsage;
            emit ramUsageChanged();
        }
    }
#endif

    QVariantList newDriveStats;
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady() && !storage.isReadOnly()) {
            QVariantMap drive;
            drive["name"] = storage.rootPath();
            drive["label"] = storage.name();

            qint64 total = storage.bytesTotal();
            qint64 available = storage.bytesAvailable();
            qint64 used = total - available;

            drive["totalBytes"] = total;
            drive["usedBytes"] = used;
            drive["availableBytes"] = available;

            double usagePercent = total > 0 ? (static_cast<double>(used) / static_cast<double>(total) * 100.0) : 0.0;
            drive["usagePercent"] = usagePercent;

            const qint64 GB = 1024LL * 1024LL * 1024LL;
            drive["usageText"] = QString("%1 / %2 GB")
                .arg(used / static_cast<double>(GB), 0, 'f', 1)
                .arg(total / static_cast<double>(GB), 0, 'f', 1);

            newDriveStats.append(drive);
        }
    }

    if (m_driveStats != newDriveStats) {
        m_driveStats = newDriveStats;
        emit driveStatsChanged();
    }
}

QString ModManager::greeting() const {
    QDateTime currentDateTime = QDateTime::currentDateTime();
    int dayOfWeek = currentDateTime.date().dayOfWeek();

    switch (dayOfWeek) {
        case 1: return "Happy Monday";
        case 2: return "Happy Tuesday";
        case 3: return "Happy Wednesday";
        case 4: return "Happy Thursday";
        case 5: return "Happy Friday";
        case 6: return "Happy Saturday";
        case 7: return "Happy Sunday";
        default: return "Happy Day";
    }
}

QString ModManager::currentDate() const {

    return "";
}

void ModManager::beginThemeChange() {
    ++m_themeChangeDepth;
}

void ModManager::endThemeChange() {
    if (m_themeChangeDepth <= 0) {
        return;
    }
    if (--m_themeChangeDepth == 0) {
        saveSettings();
    }
}

void ModManager::scheduleSettingsSave() {
    if (m_settingsSavePending) {
        return;
    }
    m_settingsSavePending = true;
    QTimer::singleShot(250, this, [this]() {
        m_settingsSavePending = false;
        saveSettings();
    });
}

void ModManager::saveSettings() {
    if (m_isLoading) {
        return;
    }
    if (m_themeChangeDepth > 0) {
        return;
    }

    QJsonObject settingsObj;
    settingsObj["sevenZipPath"] = m_sevenZipPath;
    settingsObj["deleteToRecycleBin"] = m_deleteToRecycleBin;
    settingsObj["autoHideBottomNav"] = m_autoHideBottomNav;
    settingsObj["bottomNavAutoHideDelaySeconds"] = m_bottomNavAutoHideDelaySeconds;
    settingsObj["pakViewerHideConsole"] = m_pakViewerHideConsole;
    settingsObj["managerHideConsole"] = m_managerHideConsole;
    settingsObj["pakViewerHideUI"] = m_pakViewerHideUI;
    settingsObj["welcomeShown"] = m_welcomeShown;
    settingsObj["gamesManagerWelcomeShown"] = m_gamesManagerWelcomeShown;
    settingsObj["showSplashScreen"] = m_showSplashScreen;
    settingsObj["runAsAdministrator"] = m_runAsAdministrator;
    settingsObj["notificationsEnabled"] = m_notificationsEnabled;
    settingsObj["minimizeToTray"] = m_minimizeToTray;
    settingsObj["pauseOnDefocus"] = m_pauseOnDefocus;
    settingsObj["pauseAnimationsOnDefocus"] = m_pauseAnimationsOnDefocus;
    settingsObj["pauseStatsOnDefocus"] = m_pauseStatsOnDefocus;
    settingsObj["limitFpsOnDefocus"] = m_limitFpsOnDefocus;
    settingsObj["cpuUsageBarColor"] = m_cpuUsageBarColor;
    settingsObj["ramUsageBarColor"] = m_ramUsageBarColor;
    settingsObj["driveUsageBarColor"] = m_driveUsageBarColor;

    settingsObj["theme"] = m_theme;
    settingsObj["wallpaperPath"] = m_wallpaperPath;
    settingsObj["glassBlur"] = m_glassBlur;
    settingsObj["glassOpacity"] = m_glassOpacity;
    settingsObj["glassDarkness"] = m_glassDarkness;
    settingsObj["navBarColor"] = m_navBarColor;
    settingsObj["navBarIconColor"] = m_navBarIconColor;
    settingsObj["accentColor"] = m_accentColor;

    settingsObj["idColor"] = m_idColor;
    settingsObj["toggleActiveColor"] = m_toggleActiveColor;
    settingsObj["toggleDisabledColor"] = m_toggleDisabledColor;
    settingsObj["enableButtonColor"] = m_enableButtonColor;
    settingsObj["disableButtonColor"] = m_disableButtonColor;
    settingsObj["folderModColor"] = m_folderModColor;
    settingsObj["looseFileColor"] = m_looseFileColor;
    settingsObj["tagColor"] = m_tagColor;
    settingsObj["pakColor"] = m_pakColor;
    settingsObj["ucasColor"] = m_ucasColor;
    settingsObj["utocColor"] = m_utocColor;

    QJsonObject conflictDetectorObj;
    conflictDetectorObj["lastScanTime"] = m_lastConflictScanTime;
    conflictDetectorObj["cliPath"] = m_conflictDetectorPath;
    settingsObj["conflictDetector"] = conflictDetectorObj;

    QJsonObject ueDetectorObj;
    ueDetectorObj["cliPath"] = m_ueDetectorPath;
    settingsObj["ueDetector"] = ueDetectorObj;

    settingsObj["pakViewerPath"] = m_pakViewerPath;

    settingsObj["nexusEnabled"] = m_nexusEnabled;
    settingsObj["nexusApiKey"] = m_nexusApiKey;
    settingsObj["nexusGameDomain"] = m_nexusGameDomain;

    QJsonObject shortcutsObj;
    for (auto it = m_shortcuts.cbegin(); it != m_shortcuts.cend(); ++it) {
        shortcutsObj[it.key()] = it.value();
    }
    settingsObj["shortcuts"] = shortcutsObj;

    QJsonDocument doc(settingsObj);
    QString settingsPath = getSettingsFilePath();

    QFile file(settingsPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }

    updateActivePresetFromCurrentState();
}

void ModManager::loadSettings() {
    m_isLoading = true;
    m_shortcuts = defaultShortcuts();

    QString settingsPath = getSettingsFilePath();
    QFile file(settingsPath);

    if (!file.exists()) {
        m_theme = "Cozy";
        m_gamePath = "";
        m_modsPath = "";
        m_downloadsPath = "";
        m_backupPath = "";
        m_runtimeOutputPath = "";
        m_sevenZipPath = "";
        m_deleteToRecycleBin = true;
        m_autoHideBottomNav = false;
        m_bottomNavAutoHideDelaySeconds = 5;
        m_pakViewerHideConsole = false;
        m_managerHideConsole = true;
        m_pakViewerHideUI = false;
        m_pauseOnDefocus = false;
        m_pauseAnimationsOnDefocus = false;
        m_pauseStatsOnDefocus = false;
        m_wallpaperPath = "";
        m_glassBlur = 0.5;
        m_glassOpacity = 0.05;
        m_glassDarkness = 1.0;
        m_navBarColor = "#5100CC";
        m_navBarIconColor = "#FFFFFF";
        m_accentColor = "#9135FF";
        m_cpuUsageBarColor = "#10B981";
        m_ramUsageBarColor = "#3B82F6";
        m_driveUsageBarColor = "#8B5CF6";
        m_idColor = "#FF6B35";
        m_toggleActiveColor = "#00D084";
        m_toggleDisabledColor = "#6B7280";
        m_enableButtonColor = "#00D084";
        m_disableButtonColor = "#6B7280";
        m_folderModColor = "#3B82F6";
        m_looseFileColor = "#EC4899";
        m_tagColor = "#F59E0B";
        m_pakColor = "#F97316";
        m_ucasColor = "#D946EF";
        m_utocColor = "#3B82F6";
        m_nexusEnabled = false;
        m_nexusApiKey = "";
        m_nexusGameDomain = "stellarblade";
        m_isLoading = false;
        return;
    }

    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();

            m_theme = obj.value("theme").toString("Cozy");
            if (m_theme == "Neon") m_theme = "Crimson";
            m_gamePath = obj.value("gamePath").toString("");
            m_modsPath = obj.value("modsPath").toString("");
            m_downloadsPath = obj.value("downloadsPath").toString("");
            m_backupPath = obj.value("backupPath").toString("");
            m_runtimeOutputPath = obj.value("runtimeOutputPath").toString("");
            m_sevenZipPath = obj.value("sevenZipPath").toString("");
            m_deleteToRecycleBin = obj.value("deleteToRecycleBin").toBool(true);
            m_wallpaperPath = obj.value("wallpaperPath").toString("");
            m_glassBlur = obj.value("glassBlur").toDouble(0.5);
            m_glassOpacity = obj.value("glassOpacity").toDouble(0.05);
            m_glassDarkness = obj.value("glassDarkness").toDouble(1.0);
            m_navBarColor = obj.value("navBarColor").toString("#5100CC");
            m_navBarIconColor = obj.value("navBarIconColor").toString("#FFFFFF");
            m_accentColor = obj.value("accentColor").toString("#9135FF");
            m_cpuUsageBarColor = obj.value("cpuUsageBarColor").toString("#10B981");
            m_ramUsageBarColor = obj.value("ramUsageBarColor").toString("#3B82F6");
            m_driveUsageBarColor = obj.value("driveUsageBarColor").toString("#8B5CF6");
            m_idColor = obj.value("idColor").toString("#FF6B35");
            m_toggleActiveColor = obj.value("toggleActiveColor").toString("#00D084");
            m_toggleDisabledColor = obj.value("toggleDisabledColor").toString("#6B7280");
            m_enableButtonColor = obj.value("enableButtonColor").toString("#00D084");
            m_disableButtonColor = obj.value("disableButtonColor").toString("#6B7280");
            m_folderModColor = obj.value("folderModColor").toString("#3B82F6");
            m_looseFileColor = obj.value("looseFileColor").toString("#EC4899");
            m_tagColor = obj.value("tagColor").toString("#F59E0B");
            m_pakColor = obj.value("pakColor").toString("#F97316");
            m_ucasColor = obj.value("ucasColor").toString("#D946EF");
            m_utocColor = obj.value("utocColor").toString("#3B82F6");
            m_pakViewerPath = obj.value("pakViewerPath").toString("");
            m_gamePaksPath = obj.value("gamePaksPath").toString("");
            m_usmapFilePath = obj.value("usmapFilePath").toString("");
            m_aesKey = obj.value("aesKey").toString("");
            m_ueVersion = obj.value("ueVersion").toString("UE 5.3");
            m_autoHideBottomNav = obj.value("autoHideBottomNav").toBool(false);
            m_bottomNavAutoHideDelaySeconds = qBound(1, obj.value("bottomNavAutoHideDelaySeconds").toInt(5), 120);
            m_pakViewerHideConsole = obj.value("pakViewerHideConsole").toBool(false);
            m_managerHideConsole = obj.value("managerHideConsole").toBool(true);
            m_pakViewerHideUI = obj.value("pakViewerHideUI").toBool(false);
            m_welcomeShown = obj.value("welcomeShown").toBool(false);
            m_gamesManagerWelcomeShown = obj.value("gamesManagerWelcomeShown").toBool(false);
            m_showSplashScreen = obj.value("showSplashScreen").toBool(true);
            m_runAsAdministrator = obj.value("runAsAdministrator").toBool(false);
            m_notificationsEnabled = obj.value("notificationsEnabled").toBool(true);
            m_minimizeToTray = obj.value("minimizeToTray").toBool(false);
            m_pauseOnDefocus = obj.value("pauseOnDefocus").toBool(false);
            m_pauseAnimationsOnDefocus = obj.value("pauseAnimationsOnDefocus").toBool(false);
            m_pauseStatsOnDefocus = obj.value("pauseStatsOnDefocus").toBool(false);
            m_limitFpsOnDefocus = obj.value("limitFpsOnDefocus").toBool(false);
            m_limitFpsOnDefocusAtomic.store(m_limitFpsOnDefocus, std::memory_order_relaxed);

            const QJsonObject shortcutsObj = obj.value("shortcuts").toObject();
            for (auto it = shortcutsObj.begin(); it != shortcutsObj.end(); ++it) {
                const QString shortcut = it.value().toString().trimmed();
                if (!shortcut.isEmpty()) {
                    m_shortcuts[it.key()] = shortcut;
                }
            }

            if (m_runAsAdministrator && !isRunningAsAdmin()) {
                m_runAsAdministrator = false;
            }

            m_modImages.clear();
            QJsonObject modImagesObj = obj.value("modImages").toObject();
            for (auto it = modImagesObj.begin(); it != modImagesObj.end(); ++it) {
                m_modImages[it.key()] = it.value().toString();
            }

            m_modTags.clear();
            QJsonObject modTagsObj = obj.value("modTags").toObject();
            for (auto it = modTagsObj.begin(); it != modTagsObj.end(); ++it) {
                QJsonArray tagsArray = it.value().toArray();
                QStringList tags;
                for (const QJsonValue& val : tagsArray) {
                    tags.append(val.toString());
                }
                m_modTags[it.key()] = tags;
            }

            m_modIds.clear();
            QJsonObject modIdsObj = obj.value("modIds").toObject();
            for (auto it = modIdsObj.begin(); it != modIdsObj.end(); ++it) {
                m_modIds[it.key()] = it.value().toString();
            }

            m_favoriteMods.clear();
            QJsonArray favoritesArray = obj.value("favorites").toArray();
            for (const QJsonValue& val : favoritesArray) {
                QString key = val.toString();
                if (!key.isEmpty()) {
                    m_favoriteMods.insert(key);
                }
            }

            QJsonObject conflictDetectorObj = obj.value("conflictDetector").toObject();
            m_lastConflictScanTime = conflictDetectorObj.value("lastScanTime").toString("");
            m_conflictDetectorPath = conflictDetectorObj.value("cliPath").toString("");

            QJsonObject ueDetectorObj = obj.value("ueDetector").toObject();
            m_ueDetectorPath = ueDetectorObj.value("cliPath").toString("");

            if (!m_lastConflictScanTime.isEmpty()) {
                emit lastConflictScanTimeChanged();
            }
            if (!m_conflictDetectorPath.isEmpty()) {
                emit conflictDetectorPathChanged();
            }
            if (!m_ueDetectorPath.isEmpty()) {
                emit ueDetectorPathChanged();
            }
            if (!m_pakViewerPath.isEmpty()) {
                emit pakViewerPathChanged();
            }

            if (!m_downloadsPath.isEmpty() && QDir(m_downloadsPath).exists()) {
                m_downloadsWatcher->addPath(m_downloadsPath);
            }

            m_nexusEnabled = obj.value("nexusEnabled").toBool(false);
            m_nexusApiKey = obj.value("nexusApiKey").toString("");
            m_nexusGameDomain = obj.value("nexusGameDomain").toString("stellarblade");

            if (m_nexusEnabled && !m_nexusApiKey.isEmpty()) {
                QTimer::singleShot(1000, this, [this]() {
                    validateNexusApiKey();
                });
            }
        }
    }

    m_isLoading = false;
}

void ModManager::trimMemory() {
#ifdef Q_OS_WIN

    HANDLE hProcess = GetCurrentProcess();

    EmptyWorkingSet(hProcess);
    SetProcessWorkingSetSize(hProcess, (SIZE_T)-1, (SIZE_T)-1);
#endif
}

void ModManager::applyPauseOnDefocusState() {
    const bool appInactive = QGuiApplication::applicationState() != Qt::ApplicationActive;
    const bool shouldPauseAll = m_pauseOnDefocus && appInactive;
    const bool shouldPauseStats = (m_pauseStatsOnDefocus && appInactive) || shouldPauseAll;

    if (shouldPauseAll || shouldPauseStats) {
        trimMemory();
    }

    if (shouldPauseAll) {
        if (m_greetingTimer && m_greetingTimer->isActive()) {
            m_greetingTimer->stop();
        }
    }
    if (shouldPauseStats) {
        if (m_systemStatsTimer && m_systemStatsTimer->isActive()) {
            m_systemStatsTimer->stop();
        }
    }
    if (!shouldPauseAll) {
        if (m_greetingTimer && !m_greetingTimer->isActive()) {
            m_greetingTimer->start(1000);
        }
    }
    if (!shouldPauseStats) {
        if (m_systemStatsTimer && !m_systemStatsTimer->isActive()) {
            m_systemStatsTimer->start(5000);
        }
    }
}
