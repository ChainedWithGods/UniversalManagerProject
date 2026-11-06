#include "ModManagerInternal.h"

using namespace mmdetail;

QString ModManager::sevenZipPath() const { return m_sevenZipPath; }

bool ModManager::sevenZipAvailable() const { return !resolveExistingSevenZipPath().isEmpty(); }

QString ModManager::sevenZipStatusText() const {
    const QString resolvedPath = resolveExistingSevenZipPath();
    return resolvedPath.isEmpty()
        ? missingToolStatusText(m_sevenZipPath, "7-Zip or WinRAR")
        : resolvedPath;
}

QString ModManager::theme() const { return m_theme; }

QString ModManager::gamePath() const { return m_gamePath; }

QString ModManager::modsPath() const { return m_modsPath; }

QString ModManager::downloadsPath() const { return m_downloadsPath; }

QString ModManager::backupPath() const { return m_backupPath; }

QString ModManager::runtimeOutputPath() const { return m_runtimeOutputPath; }

bool ModManager::deleteToRecycleBin() const { return m_deleteToRecycleBin; }

QString ModManager::wallpaperPath() const { return m_wallpaperPath; }

double ModManager::glassBlur() const { return m_glassBlur; }

double ModManager::glassOpacity() const { return m_glassOpacity; }

double ModManager::glassDarkness() const { return m_glassDarkness; }

QString ModManager::accentColor() const { return m_accentColor; }

QString ModManager::cpuUsageBarColor() const { return m_cpuUsageBarColor; }

QString ModManager::ramUsageBarColor() const { return m_ramUsageBarColor; }

QString ModManager::driveUsageBarColor() const { return m_driveUsageBarColor; }

void ModManager::setSevenZipPath(const QString& path) {
    if (m_sevenZipPath != path) {
        m_sevenZipPath = path;
        emit sevenZipPathChanged();
        saveSettings();
    }
}

void ModManager::setTheme(const QString& theme) {
    if (m_theme != theme) {
        m_theme = theme;
        emit themeChanged();
        saveSettings();
    }
}

void ModManager::setGamePath(const QString& path) {
    if (m_gamePath != path) {
        const QString oldGamePath = m_gamePath;
        const QString oldAutoPaksPath = detectGamePaksPathFromGameExecutable(oldGamePath);

        m_gamePath = path;
        emit gamePathChanged();

        const QString newAutoPaksPath = detectGamePaksPathFromGameExecutable(m_gamePath);
        const bool canAutoUpdate =
            m_gamePaksPath.isEmpty() ||
            !QDir(m_gamePaksPath).exists() ||
            (!oldAutoPaksPath.isEmpty() && QDir::cleanPath(m_gamePaksPath) == QDir::cleanPath(oldAutoPaksPath));

        if (canAutoUpdate && !newAutoPaksPath.isEmpty() && m_gamePaksPath != newAutoPaksPath) {
            m_gamePaksPath = newAutoPaksPath;
            emit gamePaksPathChanged();
        }

        saveSettings();
    }
}

void ModManager::setModsPath(const QString& path) {
    if (m_modsPath != path) {
        m_modsPath = path;
        emit modsPathChanged();
        emit collectionsChanged();
        saveSettings();
    }
}

void ModManager::setDownloadsPath(const QString& path) {
    if (m_downloadsPath != path) {

        if (!m_downloadsPath.isEmpty()) {
            m_downloadsWatcher->removePath(m_downloadsPath);
        }

        m_downloadsPath = path;

        if (!path.isEmpty() && QDir(path).exists()) {
            m_downloadsWatcher->addPath(path);
            qDebug() << "setDownloadsPath: Added downloads path:" << path;
        }

        emit downloadsPathChanged();
        saveSettings();
    }
}

void ModManager::setBackupPath(const QString& path) {
    if (m_backupPath != path) {
        m_backupPath = path;
        emit backupPathChanged();
        saveSettings();
        qDebug() << "setBackupPath: Added backup path:" << path;
    }
}

void ModManager::setRuntimeOutputPath(const QString& path) {
    if (m_runtimeOutputPath != path) {
        m_runtimeOutputPath = path;
        emit runtimeOutputPathChanged();
        saveSettings();
    }
}

void ModManager::setDeleteToRecycleBin(bool enabled) {
    if (m_deleteToRecycleBin != enabled) {
        m_deleteToRecycleBin = enabled;
        emit deleteToRecycleBinChanged();
        saveSettings();
    }
}

void ModManager::setWallpaperPath(const QString& path) {
    if (m_wallpaperPath != path) {
        m_wallpaperPath = path;
        emit wallpaperPathChanged();
        saveSettings();
    }
}

void ModManager::setGlassBlur(double blur) {
    if (m_glassBlur != blur) {
        m_glassBlur = blur;
        emit glassBlurChanged();
        scheduleSettingsSave();
    }
}

void ModManager::setGlassOpacity(double opacity) {
    if (m_glassOpacity != opacity) {
        m_glassOpacity = opacity;
        emit glassOpacityChanged();
        scheduleSettingsSave();
    }
}

void ModManager::setGlassDarkness(double darkness) {
    if (m_glassDarkness != darkness) {
        m_glassDarkness = darkness;
        emit glassDarknessChanged();
        scheduleSettingsSave();
    }
}

QString ModManager::navBarColor() const {
    return m_navBarColor;
}

void ModManager::setNavBarColor(const QString& color) {
    if (m_navBarColor != color) {
        m_navBarColor = color;
        emit navBarColorChanged();
        saveSettings();
    }
}

QString ModManager::navBarIconColor() const {
    return m_navBarIconColor;
}

void ModManager::setNavBarIconColor(const QString& color) {
    if (m_navBarIconColor != color) {
        m_navBarIconColor = color;
        emit navBarIconColorChanged();
        saveSettings();
    }
}

void ModManager::setAccentColor(const QString& color) {
    if (m_accentColor != color) {
        m_accentColor = color;
        emit accentColorChanged();
        saveSettings();
    }
}

void ModManager::setCpuUsageBarColor(const QString& color) {
    if (m_cpuUsageBarColor != color) {
        m_cpuUsageBarColor = color;
        emit cpuUsageBarColorChanged();
        saveSettings();
    }
}

void ModManager::setRamUsageBarColor(const QString& color) {
    if (m_ramUsageBarColor != color) {
        m_ramUsageBarColor = color;
        emit ramUsageBarColorChanged();
        saveSettings();
    }
}

void ModManager::setDriveUsageBarColor(const QString& color) {
    if (m_driveUsageBarColor != color) {
        m_driveUsageBarColor = color;
        emit driveUsageBarColorChanged();
        saveSettings();
    }
}

QString ModManager::idColor() const {
    return m_idColor;
}

void ModManager::setIdColor(const QString& color) {
    if (m_idColor != color) {
        m_idColor = color;
        emit idColorChanged();
        saveSettings();
    }
}

QString ModManager::toggleActiveColor() const {
    return m_toggleActiveColor;
}

void ModManager::setToggleActiveColor(const QString& color) {
    if (m_toggleActiveColor != color) {
        m_toggleActiveColor = color;
        emit toggleActiveColorChanged();
        saveSettings();
    }
}

QString ModManager::toggleDisabledColor() const {
    return m_toggleDisabledColor;
}

void ModManager::setToggleDisabledColor(const QString& color) {
    if (m_toggleDisabledColor != color) {
        m_toggleDisabledColor = color;
        emit toggleDisabledColorChanged();
        saveSettings();
    }
}

QString ModManager::enableButtonColor() const {
    return m_enableButtonColor;
}

void ModManager::setEnableButtonColor(const QString& color) {
    if (m_enableButtonColor != color) {
        m_enableButtonColor = color;
        emit enableButtonColorChanged();
        saveSettings();
    }
}

QString ModManager::disableButtonColor() const {
    return m_disableButtonColor;
}

void ModManager::setDisableButtonColor(const QString& color) {
    if (m_disableButtonColor != color) {
        m_disableButtonColor = color;
        emit disableButtonColorChanged();
        saveSettings();
    }
}

QString ModManager::folderModColor() const {
    return m_folderModColor;
}

void ModManager::setFolderModColor(const QString& color) {
    if (m_folderModColor != color) {
        m_folderModColor = color;
        emit folderModColorChanged();
        saveSettings();
    }
}

QString ModManager::looseFileColor() const {
    return m_looseFileColor;
}

void ModManager::setLooseFileColor(const QString& color) {
    if (m_looseFileColor != color) {
        m_looseFileColor = color;
        emit looseFileColorChanged();
        saveSettings();
    }
}

QString ModManager::tagColor() const {
    return m_tagColor;
}

void ModManager::setTagColor(const QString& color) {
    if (m_tagColor != color) {
        m_tagColor = color;
        emit tagColorChanged();
        saveSettings();
    }
}

QString ModManager::pakColor() const {
    return m_pakColor;
}

void ModManager::setPakColor(const QString& color) {
    if (m_pakColor != color) {
        m_pakColor = color;
        emit pakColorChanged();
        saveSettings();
    }
}

QString ModManager::ucasColor() const {
    return m_ucasColor;
}

void ModManager::setUcasColor(const QString& color) {
    if (m_ucasColor != color) {
        m_ucasColor = color;
        emit ucasColorChanged();
        saveSettings();
    }
}

QString ModManager::utocColor() const {
    return m_utocColor;
}

bool ModManager::autoHideBottomNav() const {
    return m_autoHideBottomNav;
}

int ModManager::bottomNavAutoHideDelaySeconds() const {
    return m_bottomNavAutoHideDelaySeconds;
}

void ModManager::setUtocColor(const QString& color) {
    if (m_utocColor != color) {
        m_utocColor = color;
        emit utocColorChanged();
        saveSettings();
    }
}

void ModManager::setAutoHideBottomNav(bool enabled) {
    if (m_autoHideBottomNav != enabled) {
        m_autoHideBottomNav = enabled;
        emit autoHideBottomNavChanged();
        saveSettings();
    }
}

void ModManager::setBottomNavAutoHideDelaySeconds(int seconds) {
    const int clamped = qBound(1, seconds, 120);
    if (m_bottomNavAutoHideDelaySeconds != clamped) {
        m_bottomNavAutoHideDelaySeconds = clamped;
        emit bottomNavAutoHideDelaySecondsChanged();
        scheduleSettingsSave();
    }
}

QString ModManager::ueVersion() const {
    return m_ueVersion;
}

QStringList ModManager::customGameVersions() const {
    return m_customGameVersions;
}

void ModManager::setUeVersion(const QString& version) {
    if (m_ueVersion != version) {
        m_ueVersion = version;
        emit ueVersionChanged();
        saveSettings();
    }
}

void ModManager::setModsInstalled(int count) {
    if (m_modsInstalled != count) {
        m_modsInstalled = count;
        emit modsInstalledChanged();
        emit totalModsAllPresetsChanged();
    }
}

void ModManager::setModsActive(int count) {
    if (m_modsActive != count) {
        m_modsActive = count;
        emit modsActiveChanged();
    }
}

QString ModManager::gamePaksPath() const {
    return m_gamePaksPath;
}

void ModManager::setGamePaksPath(const QString& path) {
    if (m_gamePaksPath != path) {
        m_gamePaksPath = path;
        emit gamePaksPathChanged();
        saveSettings();
        qDebug() << "setPaksPath: Added paks path:" << path;
    }
}

QString ModManager::usmapFilePath() const {
    return m_usmapFilePath;
}

void ModManager::setUsmapFilePath(const QString& path) {
    if (m_usmapFilePath != path) {
        m_usmapFilePath = path;
        emit usmapFilePathChanged();
        saveSettings();
    }
}

QString ModManager::aesKey() const {
    return m_aesKey;
}

void ModManager::setAesKey(const QString& key) {
    if (m_aesKey != key) {
        m_aesKey = key;
        emit aesKeyChanged();
        saveSettings();
    }
}

bool ModManager::pakViewerHideConsole() const {
    return m_pakViewerHideConsole;
}

void ModManager::setPakViewerHideConsole(bool enabled) {
    if (m_pakViewerHideConsole != enabled) {
        m_pakViewerHideConsole = enabled;
        emit pakViewerHideConsoleChanged();
        saveSettings();
    }
}

bool ModManager::managerHideConsole() const {
    return m_managerHideConsole;
}

void ModManager::setManagerHideConsole(bool enabled) {
    if (m_managerHideConsole != enabled) {
        m_managerHideConsole = enabled;
        emit managerHideConsoleChanged();
        saveSettings();
    }
}

bool ModManager::pakViewerHideUI() const {
    return m_pakViewerHideUI;
}

void ModManager::setPakViewerHideUI(bool enabled) {
    if (m_pakViewerHideUI != enabled) {
        m_pakViewerHideUI = enabled;
        emit pakViewerHideUIChanged();
        saveSettings();
    }
}

bool ModManager::welcomeShown() const {
    return m_welcomeShown;
}

void ModManager::setWelcomeShown(bool shown) {
    if (m_welcomeShown != shown) {
        m_welcomeShown = shown;
        emit welcomeShownChanged();
        saveSettings();
    }
}

bool ModManager::gamesManagerWelcomeShown() const {
    return m_gamesManagerWelcomeShown;
}

void ModManager::setGamesManagerWelcomeShown(bool shown) {
    if (m_gamesManagerWelcomeShown != shown) {
        m_gamesManagerWelcomeShown = shown;
        emit gamesManagerWelcomeShownChanged();
        saveSettings();
    }
}

bool ModManager::showSplashScreen() const {
    return m_showSplashScreen;
}

void ModManager::setShowSplashScreen(bool show) {
    if (m_showSplashScreen != show) {
        m_showSplashScreen = show;
        emit showSplashScreenChanged();
        saveSettings();
    }
}

bool ModManager::runAsAdministrator() const {
    return m_runAsAdministrator;
}

void ModManager::setRunAsAdministrator(bool runAsAdmin) {
    if (m_runAsAdministrator != runAsAdmin) {
        m_runAsAdministrator = runAsAdmin;
        emit runAsAdministratorChanged();
        saveSettings();

        if (runAsAdmin && !isRunningAsAdmin()) {
            if (!restartAsAdministrator()) {

                m_runAsAdministrator = false;
                emit runAsAdministratorChanged();
                saveSettings();
            }
        }
    }
}

bool ModManager::isRunningAsAdmin() const {
#ifdef Q_OS_WIN
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            fRet = elevation.TokenIsElevated;
        }
    }
    if (hToken) {
        CloseHandle(hToken);
    }
    return fRet;
#else
    return false;
#endif
}

bool ModManager::pauseOnDefocus() const {
    return m_pauseOnDefocus;
}

bool ModManager::notificationsEnabled() const {
    return m_notificationsEnabled;
}

void ModManager::setNotificationsEnabled(bool enabled) {
    if (m_notificationsEnabled != enabled) {
        m_notificationsEnabled = enabled;
        emit notificationsEnabledChanged();
        saveSettings();
    }
}

bool ModManager::minimizeToTray() const {
    return m_minimizeToTray;
}

void ModManager::setMinimizeToTray(bool enabled) {
    if (m_minimizeToTray != enabled) {
        m_minimizeToTray = enabled;
        emit minimizeToTrayChanged();
        saveSettings();
    }
}

void ModManager::setPauseOnDefocus(bool enabled) {
    if (m_pauseOnDefocus != enabled) {
        m_pauseOnDefocus = enabled;
        emit pauseOnDefocusChanged();
        applyPauseOnDefocusState();

        if (m_pauseOnDefocus && QGuiApplication::applicationState() != Qt::ApplicationActive) {
            trimMemory();
        }

        saveSettings();
    }
}

bool ModManager::pauseAnimationsOnDefocus() const {
    return m_pauseAnimationsOnDefocus;
}

void ModManager::setPauseAnimationsOnDefocus(bool enabled) {
    if (m_pauseAnimationsOnDefocus != enabled) {
        m_pauseAnimationsOnDefocus = enabled;
        emit pauseAnimationsOnDefocusChanged();
        saveSettings();
    }
}

bool ModManager::pauseStatsOnDefocus() const {
    return m_pauseStatsOnDefocus;
}

void ModManager::setPauseStatsOnDefocus(bool enabled) {
    if (m_pauseStatsOnDefocus != enabled) {
        m_pauseStatsOnDefocus = enabled;
        emit pauseStatsOnDefocusChanged();
        applyPauseOnDefocusState();
        saveSettings();
    }
}

bool ModManager::limitFpsOnDefocusForRender() const {
    return m_limitFpsOnDefocusAtomic.load(std::memory_order_relaxed);
}

void ModManager::setLimitFpsOnDefocus(bool enabled) {
    if (m_limitFpsOnDefocus != enabled) {
        m_limitFpsOnDefocus = enabled;
        m_limitFpsOnDefocusAtomic.store(m_limitFpsOnDefocus, std::memory_order_relaxed);
        emit limitFpsOnDefocusChanged();
        saveSettings();
    }
}
