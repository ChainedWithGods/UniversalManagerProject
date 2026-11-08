#pragma once

#include <QObject>
#include <atomic>
#include <QVariantList>
#include <QMap>
#include <QSet>
#include <QtConcurrent>

class QFileSystemWatcher;
class QTimer;
class QProcess;
class QNetworkRequest;
class LuaProfileManager;
class NexusMedia;
class DownloadManager;
class NexusOAuth;
class NexusWebSession;

class ModManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int modsInstalled READ modsInstalled WRITE setModsInstalled NOTIFY modsInstalledChanged)
    Q_PROPERTY(int modsActive READ modsActive WRITE setModsActive NOTIFY modsActiveChanged)
    Q_PROPERTY(QVariantList modList READ modList NOTIFY modListChanged)
    Q_PROPERTY(QVariantList modBundles READ modBundles NOTIFY modBundlesChanged)
    Q_PROPERTY(QString sevenZipPath READ sevenZipPath WRITE setSevenZipPath NOTIFY sevenZipPathChanged)
    Q_PROPERTY(bool sevenZipAvailable READ sevenZipAvailable NOTIFY sevenZipPathChanged)
    Q_PROPERTY(QString sevenZipStatusText READ sevenZipStatusText NOTIFY sevenZipPathChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString gamePath READ gamePath WRITE setGamePath NOTIFY gamePathChanged)
    Q_PROPERTY(QString modsPath READ modsPath WRITE setModsPath NOTIFY modsPathChanged)
    Q_PROPERTY(QString downloadsPath READ downloadsPath WRITE setDownloadsPath NOTIFY downloadsPathChanged)
    Q_PROPERTY(QString backupPath READ backupPath WRITE setBackupPath NOTIFY backupPathChanged)
    Q_PROPERTY(QString runtimeOutputPath READ runtimeOutputPath WRITE setRuntimeOutputPath NOTIFY runtimeOutputPathChanged)
    Q_PROPERTY(bool isAdvancedProcessMode READ isAdvancedProcessMode WRITE setAdvancedProcessMode NOTIFY advancedProcessModeChanged)
    Q_PROPERTY(qint64 selectedProcessPid READ selectedProcessPid NOTIFY selectedProcessChanged)
    Q_PROPERTY(QString selectedProcessName READ selectedProcessName NOTIFY selectedProcessChanged)
    Q_PROPERTY(QString selectedProcessPath READ selectedProcessPath NOTIFY selectedProcessChanged)
    Q_PROPERTY(QString appDataPath READ appDataPath CONSTANT)
    Q_PROPERTY(QString universalRuntimeLog READ universalRuntimeLog NOTIFY universalRuntimeLogChanged)
    Q_PROPERTY(QString avaloniumStatus READ avaloniumStatus NOTIFY avaloniumStatusChanged)
    Q_PROPERTY(bool generating READ generating NOTIFY generatingChanged)
    Q_PROPERTY(bool initializing READ initializing NOTIFY initializingChanged)
    Q_PROPERTY(bool runtimeReady READ runtimeReady NOTIFY runtimeReadyChanged)
    Q_PROPERTY(bool deleteToRecycleBin READ deleteToRecycleBin WRITE setDeleteToRecycleBin NOTIFY deleteToRecycleBinChanged)
    Q_PROPERTY(QString greeting READ greeting NOTIFY greetingChanged)
    Q_PROPERTY(QString currentDate READ currentDate NOTIFY currentDateChanged)
    Q_PROPERTY(QString wallpaperPath READ wallpaperPath WRITE setWallpaperPath NOTIFY wallpaperPathChanged)
    Q_PROPERTY(double glassBlur READ glassBlur WRITE setGlassBlur NOTIFY glassBlurChanged)
    Q_PROPERTY(double glassOpacity READ glassOpacity WRITE setGlassOpacity NOTIFY glassOpacityChanged)
    Q_PROPERTY(double glassDarkness READ glassDarkness WRITE setGlassDarkness NOTIFY glassDarknessChanged)
    Q_PROPERTY(QString navBarColor READ navBarColor WRITE setNavBarColor NOTIFY navBarColorChanged)
    Q_PROPERTY(QString navBarIconColor READ navBarIconColor WRITE setNavBarIconColor NOTIFY navBarIconColorChanged)
    Q_PROPERTY(QString accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
    Q_PROPERTY(QString cpuUsageBarColor READ cpuUsageBarColor WRITE setCpuUsageBarColor NOTIFY cpuUsageBarColorChanged)
    Q_PROPERTY(QString ramUsageBarColor READ ramUsageBarColor WRITE setRamUsageBarColor NOTIFY ramUsageBarColorChanged)
    Q_PROPERTY(QString driveUsageBarColor READ driveUsageBarColor WRITE setDriveUsageBarColor NOTIFY driveUsageBarColorChanged)
    Q_PROPERTY(QString idColor READ idColor WRITE setIdColor NOTIFY idColorChanged)
    Q_PROPERTY(QString toggleActiveColor READ toggleActiveColor WRITE setToggleActiveColor NOTIFY toggleActiveColorChanged)
    Q_PROPERTY(QString toggleDisabledColor READ toggleDisabledColor WRITE setToggleDisabledColor NOTIFY toggleDisabledColorChanged)
    Q_PROPERTY(QString enableButtonColor READ enableButtonColor WRITE setEnableButtonColor NOTIFY enableButtonColorChanged)
    Q_PROPERTY(QString disableButtonColor READ disableButtonColor WRITE setDisableButtonColor NOTIFY disableButtonColorChanged)
    Q_PROPERTY(QString folderModColor READ folderModColor WRITE setFolderModColor NOTIFY folderModColorChanged)
    Q_PROPERTY(QString looseFileColor READ looseFileColor WRITE setLooseFileColor NOTIFY looseFileColorChanged)
    Q_PROPERTY(QString tagColor READ tagColor WRITE setTagColor NOTIFY tagColorChanged)
    Q_PROPERTY(QString pakColor READ pakColor WRITE setPakColor NOTIFY pakColorChanged)
    Q_PROPERTY(QString ucasColor READ ucasColor WRITE setUcasColor NOTIFY ucasColorChanged)
    Q_PROPERTY(QString utocColor READ utocColor WRITE setUtocColor NOTIFY utocColorChanged)
    Q_PROPERTY(QString pakViewerPath READ pakViewerPath WRITE setPakViewerPath NOTIFY pakViewerPathChanged)
    Q_PROPERTY(bool pakViewerAvailable READ pakViewerAvailable NOTIFY pakViewerPathChanged)
    Q_PROPERTY(QString pakViewerStatusText READ pakViewerStatusText NOTIFY pakViewerPathChanged)
    Q_PROPERTY(QString gamePaksPath READ gamePaksPath WRITE setGamePaksPath NOTIFY gamePaksPathChanged)
    Q_PROPERTY(QString usmapFilePath READ usmapFilePath WRITE setUsmapFilePath NOTIFY usmapFilePathChanged)
    Q_PROPERTY(QString aesKey READ aesKey WRITE setAesKey NOTIFY aesKeyChanged)
    Q_PROPERTY(QString ueVersion READ ueVersion WRITE setUeVersion NOTIFY ueVersionChanged)
    Q_PROPERTY(QStringList customGameVersions READ customGameVersions NOTIFY customGameVersionsChanged)
    Q_PROPERTY(bool autoHideBottomNav READ autoHideBottomNav WRITE setAutoHideBottomNav NOTIFY autoHideBottomNavChanged)
    Q_PROPERTY(int bottomNavAutoHideDelaySeconds READ bottomNavAutoHideDelaySeconds WRITE setBottomNavAutoHideDelaySeconds NOTIFY bottomNavAutoHideDelaySecondsChanged)
    Q_PROPERTY(bool pakViewerHideConsole READ pakViewerHideConsole WRITE setPakViewerHideConsole NOTIFY pakViewerHideConsoleChanged)
    Q_PROPERTY(bool managerHideConsole READ managerHideConsole WRITE setManagerHideConsole NOTIFY managerHideConsoleChanged)
    Q_PROPERTY(bool pakViewerHideUI READ pakViewerHideUI WRITE setPakViewerHideUI NOTIFY pakViewerHideUIChanged)
    Q_PROPERTY(bool welcomeShown READ welcomeShown WRITE setWelcomeShown NOTIFY welcomeShownChanged)
    Q_PROPERTY(bool gamesManagerWelcomeShown READ gamesManagerWelcomeShown WRITE setGamesManagerWelcomeShown NOTIFY gamesManagerWelcomeShownChanged)
    Q_PROPERTY(bool showSplashScreen READ showSplashScreen WRITE setShowSplashScreen NOTIFY showSplashScreenChanged)
    Q_PROPERTY(bool runAsAdministrator READ runAsAdministrator WRITE setRunAsAdministrator NOTIFY runAsAdministratorChanged)
    Q_PROPERTY(bool isRunningAsAdmin READ isRunningAsAdmin NOTIFY isRunningAsAdminChanged)
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY notificationsEnabledChanged)
    Q_PROPERTY(bool minimizeToTray READ minimizeToTray WRITE setMinimizeToTray NOTIFY minimizeToTrayChanged)
    Q_PROPERTY(bool pauseOnDefocus READ pauseOnDefocus WRITE setPauseOnDefocus NOTIFY pauseOnDefocusChanged)
    Q_PROPERTY(bool pauseAnimationsOnDefocus READ pauseAnimationsOnDefocus WRITE setPauseAnimationsOnDefocus NOTIFY pauseAnimationsOnDefocusChanged)
    Q_PROPERTY(bool pauseStatsOnDefocus READ pauseStatsOnDefocus WRITE setPauseStatsOnDefocus NOTIFY pauseStatsOnDefocusChanged)
    Q_PROPERTY(bool limitFpsOnDefocusForRender READ limitFpsOnDefocusForRender NOTIFY limitFpsOnDefocusChanged)
    Q_PROPERTY(QVariantList gamePresets READ gamePresets NOTIFY gamePresetsChanged)
    Q_PROPERTY(QString activePresetId READ activePresetId NOTIFY activePresetIdChanged)
    Q_PROPERTY(int totalModsAllPresets READ totalModsAllPresets NOTIFY totalModsAllPresetsChanged)
    Q_PROPERTY(int totalActiveModsAllPresets READ totalActiveModsAllPresets NOTIFY totalActiveModsAllPresetsChanged)
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY cpuUsageChanged)
    Q_PROPERTY(double ramUsage READ ramUsage NOTIFY ramUsageChanged)
    Q_PROPERTY(QString ramUsageText READ ramUsageText NOTIFY ramUsageChanged)
    Q_PROPERTY(QVariantList driveStats READ driveStats NOTIFY driveStatsChanged)
    Q_PROPERTY(QVariantList downloadedModsList READ downloadedModsList NOTIFY downloadedModsListChanged)
    Q_PROPERTY(bool isScanningDownloads READ isScanningDownloads NOTIFY isScanningDownloadsChanged)
    Q_PROPERTY(int downloadedModsCount READ downloadedModsCount NOTIFY downloadedModsCountChanged)
    Q_PROPERTY(int lastInstalledModCount READ lastInstalledModCount NOTIFY lastInstalledModCountChanged)
    Q_PROPERTY(QString storageUsed READ storageUsed NOTIFY storageUsedChanged)
    Q_PROPERTY(QVariantList installedGamesList READ installedGamesList NOTIFY installedGamesListChanged)
    Q_PROPERTY(bool isGameScanRunning READ isGameScanRunning NOTIFY isGameScanRunningChanged)
    Q_PROPERTY(QStringList gameScanErrors READ gameScanErrors NOTIFY gameScanErrorsChanged)
    Q_PROPERTY(QStringList availableDrives READ availableDrives NOTIFY availableDrivesChanged)
    Q_PROPERTY(bool isUeDetectRunning READ isUeDetectRunning NOTIFY isUeDetectRunningChanged)

    Q_PROPERTY(bool isConflictScanning READ isConflictScanning NOTIFY isConflictScanningChanged)
    Q_PROPERTY(QString conflictScanProgress READ conflictScanProgress NOTIFY conflictScanProgressChanged)
    Q_PROPERTY(QVariantList conflictResults READ conflictResults NOTIFY conflictResultsChanged)
    Q_PROPERTY(QVariantList conflictGroups READ conflictGroups NOTIFY conflictGroupsChanged)
    Q_PROPERTY(QVariantList conflictModsData READ conflictModsData NOTIFY conflictModsDataChanged)
    Q_PROPERTY(QString lastConflictScanTime READ lastConflictScanTime NOTIFY lastConflictScanTimeChanged)
    Q_PROPERTY(QStringList conflictScanErrors READ conflictScanErrors NOTIFY conflictScanErrorsChanged)
    Q_PROPERTY(QString conflictDetectorPath READ conflictDetectorPath WRITE setConflictDetectorPath NOTIFY conflictDetectorPathChanged)
    Q_PROPERTY(bool conflictDetectorAvailable READ conflictDetectorAvailable NOTIFY conflictDetectorPathChanged)
    Q_PROPERTY(QString conflictDetectorStatusText READ conflictDetectorStatusText NOTIFY conflictDetectorPathChanged)
    Q_PROPERTY(QString ueDetectorPath READ ueDetectorPath WRITE setUeDetectorPath NOTIFY ueDetectorPathChanged)
    Q_PROPERTY(bool ueDetectorAvailable READ ueDetectorAvailable NOTIFY ueDetectorPathChanged)
    Q_PROPERTY(QString ueDetectorStatusText READ ueDetectorStatusText NOTIFY ueDetectorPathChanged)

    Q_PROPERTY(QVariantList collections READ collections NOTIFY collectionsChanged)

    Q_PROPERTY(bool nexusEnabled READ nexusEnabled WRITE setNexusEnabled NOTIFY nexusEnabledChanged)
    Q_PROPERTY(QString nexusApiKey READ nexusApiKey WRITE setNexusApiKey NOTIFY nexusApiKeyChanged)
    Q_PROPERTY(QString nexusGameDomain READ nexusGameDomain WRITE setNexusGameDomain NOTIFY nexusGameDomainChanged)
    Q_PROPERTY(QVariantList nexusGamesList READ nexusGamesList NOTIFY nexusGamesListChanged)
    Q_PROPERTY(QString nexusUsername READ nexusUsername NOTIFY nexusUserInfoChanged)
    Q_PROPERTY(bool nexusIsPremium READ nexusIsPremium NOTIFY nexusUserInfoChanged)
    Q_PROPERTY(QString nexusAvatarUrl READ nexusAvatarUrl NOTIFY nexusUserInfoChanged)
    Q_PROPERTY(int nexusHourlyRemaining READ nexusHourlyRemaining NOTIFY nexusRateLimitsChanged)
    Q_PROPERTY(int nexusHourlyLimit READ nexusHourlyLimit NOTIFY nexusRateLimitsChanged)
    Q_PROPERTY(int nexusDailyRemaining READ nexusDailyRemaining NOTIFY nexusRateLimitsChanged)
    Q_PROPERTY(int nexusDailyLimit READ nexusDailyLimit NOTIFY nexusRateLimitsChanged)
    Q_PROPERTY(QString nexusValidationStatus READ nexusValidationStatus NOTIFY nexusValidationStatusChanged)
    Q_PROPERTY(QString nexusValidationMessage READ nexusValidationMessage NOTIFY nexusValidationMessageChanged)
    Q_PROPERTY(QVariantList nexusModsList READ nexusModsList NOTIFY nexusModsListChanged)
    Q_PROPERTY(QVariantMap nexusLatestFileVersions READ nexusLatestFileVersions NOTIFY nexusLatestFileVersionsChanged)
    Q_PROPERTY(int nexusTotalModsCount READ nexusTotalModsCount NOTIFY nexusTotalModsCountChanged)
    Q_PROPERTY(bool nexusIsLoadingMods READ nexusIsLoadingMods NOTIFY nexusIsLoadingModsChanged)
    Q_PROPERTY(int nexusCurrentPage READ nexusCurrentPage NOTIFY nexusCurrentPageChanged)
    Q_PROPERTY(int nexusPageSize READ nexusPageSize NOTIFY nexusPageSizeChanged)
    Q_PROPERTY(QString nexusSortField READ nexusSortField WRITE setNexusSortField NOTIFY nexusSortFieldChanged)
    Q_PROPERTY(QString nexusSearchQuery READ nexusSearchQuery WRITE setNexusSearchQuery NOTIFY nexusSearchQueryChanged)
    Q_PROPERTY(bool nexusIsDownloading READ nexusIsDownloading NOTIFY nexusDownloadStateChanged)
    Q_PROPERTY(double nexusDownloadProgress READ nexusDownloadProgress NOTIFY nexusDownloadProgressChanged)
    Q_PROPERTY(QString nexusDownloadStatus READ nexusDownloadStatus NOTIFY nexusDownloadStatusChanged)
    Q_PROPERTY(QVariantList nexusTrackedMods READ nexusTrackedMods NOTIFY nexusTrackedModsChanged)
    Q_PROPERTY(bool isCheckingNexusUpdates READ isCheckingNexusUpdates NOTIFY isCheckingNexusUpdatesChanged)
    Q_PROPERTY(int nexusUpdatesRevision READ nexusUpdatesRevision NOTIFY nexusUpdatesRevisionChanged)
    Q_PROPERTY(int nexusUpdatesAvailable READ nexusUpdatesAvailable NOTIFY nexusUpdatesRevisionChanged)
    Q_PROPERTY(QString nexusTimeRange READ nexusTimeRange WRITE setNexusTimeRange NOTIFY nexusFiltersChanged)
    Q_PROPERTY(QStringList nexusSelectedCategories READ nexusSelectedCategories NOTIFY nexusFiltersChanged)
    Q_PROPERTY(QString nexusIncludeTag READ nexusIncludeTag WRITE setNexusIncludeTag NOTIFY nexusFiltersChanged)
    Q_PROPERTY(QString nexusExcludeTag READ nexusExcludeTag WRITE setNexusExcludeTag NOTIFY nexusFiltersChanged)
    Q_PROPERTY(QString nexusSearchTitle READ nexusSearchTitle WRITE setNexusSearchTitle NOTIFY nexusFiltersChanged)
    Q_PROPERTY(QString nexusSearchDescription READ nexusSearchDescription WRITE setNexusSearchDescription NOTIFY nexusFiltersChanged)
    Q_PROPERTY(QString nexusSearchAuthor READ nexusSearchAuthor WRITE setNexusSearchAuthor NOTIFY nexusFiltersChanged)
    Q_PROPERTY(QString nexusSearchUploader READ nexusSearchUploader WRITE setNexusSearchUploader NOTIFY nexusFiltersChanged)
    Q_PROPERTY(bool nexusHideTranslations READ nexusHideTranslations WRITE setNexusHideTranslations NOTIFY nexusFiltersChanged)
    Q_PROPERTY(QString nexusSelectedLanguage READ nexusSelectedLanguage WRITE setNexusSelectedLanguage NOTIFY nexusFiltersChanged)
    Q_PROPERTY(int nexusFilterActiveCount READ nexusFilterActiveCount NOTIFY nexusFiltersChanged)
    Q_PROPERTY(QVariantList nexusCategoriesList READ nexusCategoriesList NOTIFY nexusCategoriesListChanged)
    Q_PROPERTY(QVariantList nexusTagOptions READ nexusTagOptions NOTIFY nexusTagsListChanged)
    Q_PROPERTY(QVariantList nexusLanguagesList READ nexusLanguagesList NOTIFY nexusLanguagesListChanged)
    Q_PROPERTY(QVariantMap currentNexusModDetails READ currentNexusModDetails NOTIFY currentNexusModDetailsChanged)
    Q_PROPERTY(QVariantList currentNexusModFiles READ currentNexusModFiles NOTIFY currentNexusModFilesChanged)
    Q_PROPERTY(QVariantList nexusModPreviewImages READ nexusModPreviewImages NOTIFY nexusModPreviewImagesChanged)
    Q_PROPERTY(QVariantMap currentNexusModRequirements READ currentNexusModRequirements NOTIFY currentNexusModRequirementsChanged)
    Q_PROPERTY(bool isLoadingNexusModDetails READ isLoadingNexusModDetails NOTIFY isLoadingNexusModDetailsChanged)
    Q_PROPERTY(bool isLoadingNexusModFiles READ isLoadingNexusModFiles NOTIFY isLoadingNexusModFilesChanged)
    Q_PROPERTY(bool isLoadingNexusModPreviews READ isLoadingNexusModPreviews NOTIFY isLoadingNexusModPreviewsChanged)
    Q_PROPERTY(bool isLoadingNexusModRequirements READ isLoadingNexusModRequirements NOTIFY isLoadingNexusModRequirementsChanged)

public:
    explicit ModManager(QObject *parent = nullptr);
    ~ModManager() override;
    int modsInstalled() const;
    int totalModsAllPresets() const;
    int modsActive() const;
    QVariantList modList() const;
    QVariantList modBundles() const;
    QString sevenZipPath() const;
    bool sevenZipAvailable() const;
    QString sevenZipStatusText() const;
    QString theme() const;
    QString gamePath() const;
    QString modsPath() const;
    QString downloadsPath() const;
    QString backupPath() const;
    QString appDataPath() const;
    QString runtimeOutputPath() const;
    bool deleteToRecycleBin() const;
    Q_INVOKABLE void trimMemory();
    Q_INVOKABLE void applyPauseOnDefocusState();

    Q_INVOKABLE void beginThemeChange();
    Q_INVOKABLE void endThemeChange();

    QString greeting() const;
    QString currentDate() const;
    QString wallpaperPath() const;
    double glassBlur() const;
    double glassOpacity() const;
    double glassDarkness() const;
    QString navBarColor() const;
    QString navBarIconColor() const;
    QString accentColor() const;
    QString cpuUsageBarColor() const;
    QString ramUsageBarColor() const;
    QString driveUsageBarColor() const;
    QString idColor() const;
    QString toggleActiveColor() const;
    QString toggleDisabledColor() const;
    QString enableButtonColor() const;
    QString disableButtonColor() const;
    QString folderModColor() const;
    QString looseFileColor() const;
    QString tagColor() const;
    QString pakColor() const;
    QString ucasColor() const;
    QString utocColor() const;
    QString pakViewerPath() const;
    QString gamePaksPath() const;
    QString usmapFilePath() const;
    QString aesKey() const;
    QString ueVersion() const;
    QStringList customGameVersions() const;
    bool autoHideBottomNav() const;
    int bottomNavAutoHideDelaySeconds() const;
    bool pakViewerHideConsole() const;
    bool managerHideConsole() const;
    bool pakViewerHideUI() const;
    bool welcomeShown() const;
    bool gamesManagerWelcomeShown() const;
    bool showSplashScreen() const;
    bool runAsAdministrator() const;
    bool isRunningAsAdmin() const;
    bool notificationsEnabled() const;
    bool minimizeToTray() const;
    bool pauseOnDefocus() const;
    bool pauseAnimationsOnDefocus() const;
    bool pauseStatsOnDefocus() const;
    bool limitFpsOnDefocusForRender() const;
    QVariantList gamePresets() const;
    QString activePresetId() const;
    int totalActiveModsAllPresets() const;
    double cpuUsage() const;
    double ramUsage() const;
    QString ramUsageText() const;
    QVariantList driveStats() const;
    QVariantList downloadedModsList() const;
    bool isScanningDownloads() const;
    int downloadedModsCount() const;
    int lastInstalledModCount() const;
    QString storageUsed() const;
    QVariantList installedGamesList() const;
    bool isGameScanRunning() const;
    QStringList gameScanErrors() const;
    QStringList availableDrives() const;

    bool isConflictScanning() const;
    QString conflictScanProgress() const;
    QVariantList conflictResults() const;
    QVariantList conflictGroups() const;
    QVariantList conflictModsData() const;
    QString lastConflictScanTime() const;
    QStringList conflictScanErrors() const;
    QString conflictDetectorPath() const;
    bool conflictDetectorAvailable() const;
    QString conflictDetectorStatusText() const;
    QString ueDetectorPath() const;
    bool ueDetectorAvailable() const;
    QString ueDetectorStatusText() const;
    bool pakViewerAvailable() const;
    QString pakViewerStatusText() const;

    QVariantList collections() const;

    bool nexusEnabled() const;
    QString nexusApiKey() const;
    QString nexusGameDomain() const;
    QVariantList nexusGamesList() const;
    QString nexusUsername() const;
    bool nexusIsPremium() const;
    QString nexusAvatarUrl() const;
    int nexusHourlyRemaining() const;
    int nexusHourlyLimit() const;
    int nexusDailyRemaining() const;
    int nexusDailyLimit() const;
    QString nexusValidationStatus() const;
    QString nexusValidationMessage() const;
    QVariantList nexusModsList() const;
    QVariantMap nexusLatestFileVersions() const;
    int nexusTotalModsCount() const;
    bool nexusIsLoadingMods() const;
    int nexusCurrentPage() const;
    int nexusPageSize() const;
    QString nexusSortField() const;
    QString nexusSearchQuery() const;
    bool nexusIsDownloading() const;
    double nexusDownloadProgress() const;
    QString nexusDownloadStatus() const;
    QString nexusTimeRange() const;
    QStringList nexusSelectedCategories() const;
    QString nexusIncludeTag() const;
    QString nexusExcludeTag() const;
    QString nexusSearchTitle() const;
    QString nexusSearchDescription() const;
    QString nexusSearchAuthor() const;
    QString nexusSearchUploader() const;
    bool nexusHideTranslations() const;
    QString nexusSelectedLanguage() const;
    int nexusFilterActiveCount() const;
    QVariantList nexusCategoriesList() const;
    QVariantList nexusTagOptions() const;
    QVariantList nexusLanguagesList() const;
    QVariantMap currentNexusModDetails() const;
    QVariantList currentNexusModFiles() const;
    QVariantList nexusModPreviewImages() const;
    QVariantMap currentNexusModRequirements() const;
    bool isLoadingNexusModDetails() const;
    bool isLoadingNexusModFiles() const;
    bool isLoadingNexusModPreviews() const;
    bool isLoadingNexusModRequirements() const;

public slots:

    void setNexusEnabled(bool enabled);
    void setNexusApiKey(const QString& key);
    void setNexusGameDomain(const QString& domain);
    void setNexusSortField(const QString& field);
    void setNexusSearchQuery(const QString& query);
    void setNexusTimeRange(const QString& range);
    void toggleNexusCategory(const QString& category);
    void clearNexusCategories();
    void setNexusIncludeTag(const QString& tag);
    void setNexusExcludeTag(const QString& tag);
    void setNexusSearchTitle(const QString& title);
    void setNexusSearchDescription(const QString& desc);
    void setNexusSearchAuthor(const QString& author);
    void setNexusSearchUploader(const QString& uploader);
    void applyNexusSearchParameters(const QString& title, const QString& desc, const QString& author, const QString& uploader);
    void setNexusHideTranslations(bool hide);
    void setNexusSelectedLanguage(const QString& lang);
    void resetAllNexusFilters();
    void fetchNexusMetadata();
    void fetchNexusGames();
    void fetchNexusGameTags();
    void validateNexusApiKey();
    void fetchNexusMods(int page = 0);
    void nextNexusPage();
    void previousNexusPage();
    void downloadNexusMod(int modId, int fileId = 0, const QString& modName = "");
    void requestDownloadsPathSetup();
    void setDownloadManager(DownloadManager* manager);
    void handleNxmUrl(const QString& url);
    void openNexusModDetails(int modId, const QVariantMap& initialData = QVariantMap());
    void closeNexusModDetails();
    void fetchNexusModFiles(int modId);
    QVariantList nexusTrackedMods() const;
    bool isCheckingNexusUpdates() const;
    int nexusUpdatesRevision() const;
    int nexusUpdatesAvailable() const;
    void checkNexusUpdates();
    QVariantMap nexusUpdateForArchive(const QString& filePath) const;
    QVariantMap nexusUpdateForMod(const QString& modName) const;
    QVariantMap nexusUpdateTargetForArchive(const QString& filePath) const;
    bool isNexusUpdateDownload(const QString& gameDomain, int modId, int fileId) const;
    void loadMoreNexusModsRequiring();
    void openNexusUrl(const QString& url);
    bool registerNxmProtocol();
    bool isNxmProtocolRegistered() const;
    bool unregisterNxmProtocol();
    void cancelNexusDownload();

    void scanMods(const QString& path);
    void setModsInstalled(int count);
    void setModsActive(int count);
    void setSevenZipPath(const QString& path);
    void autoDetectSevenZipPath();
    void setTheme(const QString& theme);
    void setGamePath(const QString& path);
    void setModsPath(const QString& path);
    void setDownloadsPath(const QString& path);
    void setBackupPath(const QString& path);
    void setRuntimeOutputPath(const QString& path);
    void setDeleteToRecycleBin(bool enabled);
    void setWallpaperPath(const QString& path);
    void setGlassBlur(double blur);
    void setGlassOpacity(double opacity);
    void setGlassDarkness(double darkness);
    void setNavBarColor(const QString& color);
    void setNavBarIconColor(const QString& color);
    void setAccentColor(const QString& color);
    void setCpuUsageBarColor(const QString& color);
    void setRamUsageBarColor(const QString& color);
    void setDriveUsageBarColor(const QString& color);
    void setIdColor(const QString& color);
    void setToggleActiveColor(const QString& color);
    void setToggleDisabledColor(const QString& color);
    void setEnableButtonColor(const QString& color);
    void setDisableButtonColor(const QString& color);
    void setFolderModColor(const QString& color);
    void setLooseFileColor(const QString& color);
    void setTagColor(const QString& color);
    void setPakColor(const QString& color);
    void setUcasColor(const QString& color);
    void setUtocColor(const QString& color);
    void setPakViewerPath(const QString& path);
    void autoDetectPakViewerPath();
    void setGamePaksPath(const QString& path);
    void setUsmapFilePath(const QString& path);
    void setAesKey(const QString& key);
    void setUeVersion(const QString& version);
    void setAutoHideBottomNav(bool enabled);
    void setBottomNavAutoHideDelaySeconds(int seconds);
    void setPakViewerHideConsole(bool enabled);
    void setManagerHideConsole(bool enabled);
    void setPakViewerHideUI(bool enabled);
    void setWelcomeShown(bool shown);
    void setGamesManagerWelcomeShown(bool shown);
    void setShowSplashScreen(bool show);
    void setRunAsAdministrator(bool runAsAdmin);
    void setNotificationsEnabled(bool enabled);
    void setMinimizeToTray(bool enabled);
    void setPauseOnDefocus(bool enabled);
    void setPauseAnimationsOnDefocus(bool enabled);
    void setPauseStatsOnDefocus(bool enabled);
    void setLimitFpsOnDefocus(bool enabled);
    bool restartAsAdministrator();
    Q_INVOKABLE bool restartApplication();
    void saveSettings();

    void scheduleSettingsSave();
    void loadSettings();
    void loadNexusTrackedMods();
    void saveNexusTrackedMods() const;
    QString nexusTrackedStoragePath() const;
    void loadNexusArchives();
    void saveNexusArchives() const;
    bool pruneMissingNexusArchives();
    QString nexusArchivesStoragePath() const;
    QVariantMap nexusArchiveForFile(const QString& filePath) const;
    QVariantMap nexusArchiveFallbackForFile(const QString& filePath) const;
    void mergeNexusArchiveRecord(const QVariantMap& record);
    bool hasNexusArchiveForFileId(const QString& gameDomain, int modId, int fileId) const;
    void loadModAliases();
    void saveModAliases() const;
    QString modAliasesStoragePath() const;
    void mergeModAliases(const QMap<QString, QStringList>& aliases);
    void forgetModAliases(const QString& uniqueId);
    void copyModAliases(const QString& fromPresetName, const QString& toPresetName) const;
    void pruneModAliases(const QSet<QString>& knownIds);
    void loadModBundles();
    void saveModBundles() const;
    QString modBundlesStoragePath() const;
    void copyModBundles(const QString& fromPresetName, const QString& toPresetName) const;
    void pruneModBundles();
    void adoptBundleFoldersFromScan();
    void removeModIdsFromBundles(const QStringList& ids);
    void refreshBundleMembership();
    QVariantList buildModBundleView() const;
    QString createBundleFromInstalledFolder(const QString& folder, const QString& archivePath);
    QMap<QString, QString> modInstallTargets() const;
    void removeLooseUpdateTargets(const QStringList& targets);
    QMap<QString, QStringList> looseModExtensions() const;
    void queueNexusDownloadProvenance(const QString& gameDomain, int modId, int fileId,
                                      const QString& fileName, const QString& modName,
                                      const QString& version, const QString& category);
    void recordCompletedNexusDownload(const QString& filePath);
    void linkNexusInstalledMods(const QStringList& archives, const QStringList& installedMods);
    void relinkNexusRenamedMod(const QString& oldName, const QString& newName);
    void unlinkNexusUninstalledMods(const QStringList& modNames, const QStringList& modIds);
    void reconcileNexusInstalledLinks(const QSet<QString>& knownIds);
    void checkNextNexusUpdate(int index);
    void loadPresets();
    void setActivePreset(const QString& presetId);
    void createPreset(const QString& name);
    void deletePreset(const QString& presetId);
    void renamePreset(const QString& presetId, const QString& newName);
    void duplicatePreset(const QString& presetId);
    void setPresetPreview(const QString& presetId, const QString& imagePath);
    QString getPresetPreview(const QString& presetId);
    void removePresetPreview(const QString& presetId);
    void togglePresetBlur(const QString& presetId);
    void createPresetAndReset(const QString& name);

    void ensureLuaProfileFolders();
    void backupMods(const QStringList& modNames);
    void backupModsAsync(const QStringList& modNames);
    void installMods(const QStringList& filePaths, const QMap<QString, QString>& updateTargets,
                     const QMap<QString, QString>& installTargets,
                     const QMap<QString, QStringList>& looseExtensions);
    void installModsAsync(const QStringList& filePaths);
    void confirmModInstallation(const QVariantList& modMappings);
    void cancelModInstallation();
    QVariantMap getModDetails(const QString& modName);
    void toggleModStatus(const QString& modName);
    void toggleModStatusBatch(const QStringList& modNames, bool enable);
    void deleteMod(const QString& modName);
    void deleteModsAsync(const QStringList& modNames);
    void openModFolder(const QString& modName);
    void renameMod(const QString& oldName, const QString& newName);
    QString groupModsIntoBundle(const QStringList& modNames, const QString& bundleName);
    QString addModsToBundle(const QString& bundleId, const QStringList& modNames);
    QString removeModsFromBundle(const QString& bundleId, const QStringList& modNames);
    QVariantMap nexusUpdateForBundle(const QString& bundleId) const;
    void openBundleFolder(const QString& bundleId);
    QStringList getModBundleTags(const QString& bundleId) const;
    bool addModBundleTag(const QString& bundleId, const QString& tag);
    bool removeModBundleTag(const QString& bundleId, const QString& tag);
    void openModsDirectory();
    void openAppDataFolder();

    void openFolderPath(const QString& path);
    void setModImage(const QString& modName, const QString& imagePath);
    QString getModImage(const QString& modName);
    void removeModImage(const QString& modName);
    void addModTag(const QString& modName, const QString& tag);
    void removeModTag(const QString& modName, const QString& tag);
    QStringList getModTags(const QString& modName);
    void setModDescription(const QString& modName, const QString& description);
    QString getModDescription(const QString& modName);
    QString launchGame();

    Q_INVOKABLE QString injectAvaloniumDll();
    Q_INVOKABLE bool checkAvaloniumConflict();
    Q_INVOKABLE bool deleteConflictingAvaloniumDll();
    Q_INVOKABLE void killGameProcess();
    Q_INVOKABLE bool isGameProcessRunning() const;
    Q_INVOKABLE bool isAvaloniumInjected() const;
    Q_INVOKABLE bool isGameOnCDrive();
    Q_INVOKABLE QString queryIocpStatus();
    Q_INVOKABLE void generateUsmap();
    Q_INVOKABLE void generateSdk();
    Q_INVOKABLE QVariantList getRunningProcesses() const;
    Q_INVOKABLE void setSelectedProcess(qint64 pid, const QString& name, const QString& path);
    Q_INVOKABLE void clearSelectedProcess();
    bool isAdvancedProcessMode() const { return m_isAdvancedProcessMode; }
    void setAdvancedProcessMode(bool enabled);
    qint64 selectedProcessPid() const { return m_selectedProcessPid; }
    QString selectedProcessName() const { return m_selectedProcessName; }
    QString selectedProcessPath() const { return m_selectedProcessPath; }
    Q_INVOKABLE QVariantMap loadAvaloniumConfig() const;
    Q_INVOKABLE QString saveAvaloniumConfig(const QVariantMap& config);
    Q_INVOKABLE QString setAvaloniumLuaModsPath(const QString& luaModsPath);
    Q_INVOKABLE void updatePresetLuaModsConfig(const QString& presetId, const QString& luaModsPath, bool useSharedLuaMods);
    Q_INVOKABLE bool isPresetUsingSharedLuaMods(const QString& presetId = QString()) const;
    Q_INVOKABLE QString avaloniumConfigWriteBlockReason() const;
    Q_INVOKABLE QVariantList listLuaMods(const QString& luaModsPath = QString()) const;
    Q_INVOKABLE QString setLuaModEnabled(const QString& modName, bool enabled, const QString& luaModsPath = QString());

    Q_INVOKABLE QString registerLuaModsDisabled(const QString& luaModsPath = QString());

    Q_INVOKABLE QString luaProfileFolderPath(const QString& presetId) const;

    Q_INVOKABLE QString avaloniumDirectory() const;

    Q_INVOKABLE QString defaultLuaModsDirectory() const;

    LuaProfileManager* luaProfileManager() const { return m_luaProfileManager; }

    NexusOAuth* nexusOAuth() const { return m_nexusOAuth; }
    NexusWebSession* nexusWebSession() const { return m_nexusWebSession; }
    Q_INVOKABLE QString getShortcut(const QString& key, const QString& fallback = QString()) const;
    Q_INVOKABLE void setShortcut(const QString& key, const QString& shortcut);

    bool generating() const;
    void setGenerating(bool v);

    bool initializing() const;
    void setInitializing(bool v);

    bool runtimeReady() const;
    void setRuntimeReady(bool v);

    QString getModPakPath(const QString& modName);
    void exportModStructure(const QString& modName);

    QString universalRuntimeLog() const;
    QString avaloniumStatus() const;

    bool isModFavorite(const QString& modKey) const;
    void setModFavorite(const QString& modKey, bool favorite);

    void scanDownloadedMods();
    void openDownloadsFolder();

    void scanInstalledGames();
    void scanInstalledGamesForDrives(const QStringList& drives);
    void cancelGameScan();
    void createModsFoldersForGames(const QVariantList& games, const QString& folderType);
    void openModsFoldersForGames(const QVariantList& games, const QString& folderType);
    void deleteModsFoldersForGames(const QVariantList& games, const QString& folderType);
    QVariantList getModsFoldersForGame(const QVariantMap& game) const;
    Q_INVOKABLE QVariantList getAvailableModsFoldersForCurrentPreset() const;
    Q_INVOKABLE bool switchCurrentPresetModsFolder(const QString& newModsPath, bool disableAllMods, bool disableAllCollections, const QStringList& modsToDisable, const QStringList& collectionsToDisable);
    Q_INVOKABLE QVariantMap checkModsFolderForCurrentPreset(const QString& folderType) const;
    Q_INVOKABLE QVariantMap createModsFolderForCurrentPreset(const QString& folderType);
    void addGamesToPresets(const QVariantList& games, const QString& folderType);
    void openGameFolder(const QString& gamePath);
    void copyTextToClipboard(const QString& text);
    bool modsFolderExistsForGame(const QVariantMap& game, const QString& folderType) const;

    Q_INVOKABLE QString detectUEVersionForGame(const QString& gamePath);
    Q_INVOKABLE void detectUEVersionsForAllGames();
    Q_INVOKABLE QString getUEVersionForGame(const QString& gamePath) const;

    Q_INVOKABLE bool loadInstalledGamesCache();
    Q_INVOKABLE bool saveInstalledGamesCache();
    Q_INVOKABLE void clearInstalledGamesCache();
    QString getInstalledGamesCachePath() const;
    QString getInstalledGamesCacheBasePath() const;
    QString getInstalledGamesLegacyJsonCachePath() const;

    Q_INVOKABLE QString getCustomGameForGame(const QString& gamePath) const;
    Q_INVOKABLE QString getCustomGameDisplayNameForGame(const QString& gamePath) const;
    Q_INVOKABLE QString getCustomGameBaseVersionForGame(const QString& gamePath) const;
    Q_INVOKABLE bool isCustomGameDetectedForGame(const QString& gamePath) const;
    Q_INVOKABLE QString getCustomGameSummaryForGame(const QString& gamePath) const;

    Q_INVOKABLE void clearUniversalRuntimeLog();

    bool isUeDetectRunning() const;

    void scanConflicts(bool includeDisabled = false);
    void cancelConflictScan();
    void setConflictDetectorPath(const QString& path);
    void autoDetectConflictDetectorPath();
    void setUeDetectorPath(const QString& path);
    void autoDetectUEDetectorPath();
    void disableConflictingMod(const QString& modName);
    void exportConflictReport(const QString& filePath);
    QString copyConflictSummary();
    void launchPakViewerForMod(const QString& modName);
    void launchPakViewer(const QString& pakFilePath = QString(), const QStringList& extraArgs = QStringList());
    void launchPakViewerStandalone();

    void createCollection(const QString& name, const QString& description);
    void deleteCollection(const QString& collectionId);
    void addModToCollection(const QString& collectionId, const QString& modName);
    void removeModFromCollection(const QString& collectionId, const QString& modName);
    void viewCollection(const QString& collectionId);
    void renameCollection(const QString& collectionId, const QString& newName);
    void updateCollectionDescription(const QString& collectionId, const QString& newDescription);
    Q_INVOKABLE void setCollectionPreview(const QString& collectionId, const QString& imagePath);
    Q_INVOKABLE QString getCollectionPreview(const QString& collectionId);
    Q_INVOKABLE void toggleCollectionStatus(const QString& collectionId, bool enable);

private slots:
    void onModsDirectoryChanged();
    void onDownloadsDirectoryChanged();
    void updateGreeting();
    void updateSystemStats();

signals:
    void modsInstalledChanged();
    void modsActiveChanged();
    void modListChanged();
    void modBundlesChanged();
    void modStatusChanged(const QString& modName, bool enabled);
    void modRemoved(const QString& modName);
    void modRenamed(const QString& oldName, const QString& newName);
    void sevenZipPathChanged();
    void scanFinished(bool success);
    void themeChanged();
    void gamePathChanged();
    void modsPathChanged();
    void downloadsPathChanged();
    void downloadsPathRequired();
    void backupPathChanged();
    void runtimeOutputPathChanged();
    void advancedProcessModeChanged();
    void selectedProcessChanged();
    void universalRuntimeLogChanged();
    void avaloniumStatusChanged();
    void generatingChanged();
    void initializingChanged();
    void runtimeReadyChanged();
    void usmapGenerated(const QString& message);
    void sdkGenerated(const QString& message);
    void avaloniumConflictDetected();
    void cDriveAdminRequired();
    void deleteToRecycleBinChanged();
    void greetingChanged();
    void currentDateChanged();
    void wallpaperPathChanged();
    void glassBlurChanged();
    void glassOpacityChanged();
    void glassDarknessChanged();
    void navBarColorChanged();
    void navBarIconColorChanged();
    void accentColorChanged();
    void cpuUsageBarColorChanged();
    void ramUsageBarColorChanged();
    void driveUsageBarColorChanged();
    void idColorChanged();
    void toggleActiveColorChanged();
    void toggleDisabledColorChanged();
    void enableButtonColorChanged();
    void disableButtonColorChanged();
    void folderModColorChanged();
    void looseFileColorChanged();
    void tagColorChanged();
    void pakColorChanged();
    void ucasColorChanged();
    void utocColorChanged();
    void pakViewerPathChanged();
    void gamePaksPathChanged();
    void usmapFilePathChanged();
    void aesKeyChanged();
    void ueVersionChanged();
    void customGameVersionsChanged();
    void autoHideBottomNavChanged();
    void bottomNavAutoHideDelaySecondsChanged();
    void pakViewerHideConsoleChanged();
    void managerHideConsoleChanged();
    void pakViewerHideUIChanged();
    void welcomeShownChanged();
    void gamesManagerWelcomeShownChanged();
    void showSplashScreenChanged();
    void runAsAdministratorChanged();
    void isRunningAsAdminChanged();
    void notificationsEnabledChanged();
    void minimizeToTrayChanged();
    void pauseOnDefocusChanged();
    void pauseAnimationsOnDefocusChanged();
    void pauseStatsOnDefocusChanged();
    void limitFpsOnDefocusChanged();
    void gamePresetsChanged();
    void activePresetIdChanged();
    void totalModsAllPresetsChanged();
    void totalActiveModsAllPresetsChanged();
    void cpuUsageChanged();
    void ramUsageChanged();
    void driveStatsChanged();
    void installProgress(const QString& message);
    void installComplete(bool success, const QString& message);
    void archiveToolConfigurationRequired();
    void backupProgress(int current, int total, const QString& modName);
    void backupComplete(bool success, int successCount, int total);
    void deleteProgress(int current, int total, const QString& modName);
    void deleteComplete(bool success, int deletedCount, int total);
    void modImageChanged(const QString& modName);
    void modTagsChanged(const QString& modName);
    void modDescriptionChanged(const QString& modName);
    void modFavoriteChanged(const QString& modKey, bool favorite);
    void modStructureExported(bool success, const QString& filePath);
    void modsDetectedForRename(const QVariantList& modGroups);
    void presetPreviewChanged(const QString& presetId);
    void navigateToGeneralTab();
    void downloadedModsListChanged();
    void isScanningDownloadsChanged();
    void downloadedModsCountChanged();
    void lastInstalledModCountChanged();
    void downloadsScanFinished(bool success);
    void storageUsedChanged();
    void installedGamesListChanged();
    void isGameScanRunningChanged();
    void gameScanErrorsChanged();
    void gameScanActionMessage(const QString& message, bool success);

    void luaProfileFolderResult(const QString& presetName, const QString& error);

    void luaProfileCreateFailed(const QString& presetName, const QString& error);

    void luaProfileFoldersCreated(const QVariantList& profiles);
    void availableDrivesChanged();
    void gameUEVersionDetected(const QString& gamePath, const QString& version);
    void isUeDetectRunningChanged();

    void ueVersionsDetected();
    void aboutToRestart();

    void isConflictScanningChanged();
    void conflictScanProgressChanged();
    void conflictResultsChanged();
    void conflictGroupsChanged();
    void conflictModsDataChanged();
    void lastConflictScanTimeChanged();
    void conflictScanErrorsChanged();
    void conflictDetectorPathChanged();
    void ueDetectorPathChanged();
    void conflictScanComplete(bool success, int conflictCount);
    void conflictScanError(const QString& error);
    void conflictScanTimeoutWarning();

    void collectionsChanged();

    void nexusEnabledChanged();
    void nexusApiKeyChanged();
    void nexusGameDomainChanged();
    void nexusGamesListChanged();
    void nexusUserInfoChanged();
    void nexusRateLimitsChanged();
    void nexusValidationStatusChanged();
    void nexusValidationMessageChanged();
    void nexusModsListChanged();
    void nexusLatestFileVersionsChanged();
    void nexusTotalModsCountChanged();
    void nexusIsLoadingModsChanged();
    void nexusCurrentPageChanged();
    void nexusPageSizeChanged();
    void nexusSortFieldChanged();
    void nexusSearchQueryChanged();
    void nexusDownloadStateChanged();
    void nexusDownloadProgressChanged();
    void nexusDownloadStatusChanged();
    void nexusDownloadPageRequested(const QString& url, const QString& modName);
    void nexusTrackedModsChanged();
    void isCheckingNexusUpdatesChanged();
    void nexusUpdatesRevisionChanged();
    void nexusFiltersChanged();
    void nexusCategoriesListChanged();
    void nexusTagsListChanged();
    void nexusLanguagesListChanged();
    void currentNexusModDetailsChanged();
    void currentNexusModFilesChanged();
    void nexusModPreviewImagesChanged();
    void currentNexusModRequirementsChanged();
    void isLoadingNexusModDetailsChanged();
    void isLoadingNexusModFilesChanged();
    void isLoadingNexusModPreviewsChanged();
    void isLoadingNexusModRequirementsChanged();

private:
    QString getSettingsFilePath() const;
    QString getPresetsFilePath() const;
    QString getPresetsBasePath() const;
    QString getPresetFolderPath(const QString& presetName) const;

    LuaProfileManager* m_luaProfileManager = nullptr;
    friend class LuaProfileManager;
    void savePresets();
    void savePreset(const QVariantMap& preset);
    void deletePresetFolder(const QString& presetName);
    QVariantMap currentStateToPresetMap(const QString& id, const QString& name) const;
    void updateActivePresetFromCurrentState();
    bool applyPresetToCurrentState(const QVariantMap& preset);
    int findPresetIndexById(const QString& presetId) const;

    QString activeLuaModsRoot() const;

    QString formatBytes(qint64 bytes) const;
    bool copyRecursively(const QString& srcPath, const QString& dstPath);
    bool extractArchive(const QString& archivePath, const QString& destPath);
    void processModFile(const QString& filePath);
    QString ensurePSuffix(const QString& baseName) const;
    QString getActualFileName(const QString& modName) const;
    QString findModImage(const QString& modFolderPath) const;
    void removeModImageInternal(const QString& modName, bool deleteFile);
    void cleanupStaleModData(const QSet<QString>& currentModNames);
    void applyModEnabledState(const QStringList& modKeys, bool enabled);
    int applyModRemoval(const QStringList& modKeys);
    QString convertToEnumFormat(const QString& versionOrGameName) const;

    int m_modsInstalled = 0;
    int m_modsActive = 0;
    QVariantList m_modList;
    QString m_modsPathInternal;
    qint64 m_totalBytes = 0;
    QFileSystemWatcher* m_watcher;
    QFileSystemWatcher* m_downloadsWatcher;
    QString m_sevenZipPath;
    QString m_theme;
    QString m_gamePath;
    QString m_modsPath;
    QString m_downloadsPath;
    QString m_backupPath;
    QString m_runtimeOutputPath;

    qint64 m_lastLaunchedGamePid = 0;
    bool m_isAdvancedProcessMode = false;
    qint64 m_selectedProcessPid = 0;
    QString m_selectedProcessName;
    QString m_selectedProcessPath;
    QString m_gameWindowTitle;
    bool m_generating = false;
    bool m_initializing = false;
    bool m_runtimeReady = false;
    QString m_universalRuntimeLog;
    QString m_avaloniumStatus;
    qint64 m_runtimeLogReadPos = 0;
    bool m_iocpStatusRequested = false;

    class QTimer* m_runtimeLogPollTimer = nullptr;

    QString resolveToolPath(const QString& relativePath) const;
    QString resolveAvaloniumDllPath() const;
    QString avaloniumConfigPath() const;
    QString luaModsDirectory(const QString& explicitPath) const;
    QString universalRuntimeLogPath() const;
    static QString getGameWindowTitleFromPid(qint64 pid);
    static QString sanitizeFileName(const QString& name);
    QString resolveRuntimeGameTitle();
    void startRuntimeLogMonitor();
    void pollRuntimeLogOnce();
    bool m_deleteToRecycleBin = true;
    bool m_isLoading = false;
    int m_themeChangeDepth = 0;
    bool m_settingsSavePending = false;
    bool m_batchOperationInProgress = false;
    qint64 m_modsWatcherIgnoreUntilMs = 0;
    QTimer* m_greetingTimer;
    QString m_lastGreeting;
    QString m_wallpaperPath;
    double m_glassBlur = 0.5;
    double m_glassOpacity = 0.05;
    double m_glassDarkness = 1.0;
    QString m_navBarColor = "#5100CC";
    QString m_navBarIconColor = "#FFFFFF";
    QString m_accentColor = "#9135FF";
    QString m_cpuUsageBarColor = "#10B981";
    QString m_ramUsageBarColor = "#3B82F6";
    QString m_driveUsageBarColor = "#8B5CF6";
    QString m_idColor = "#FF6B35";
    QString m_toggleActiveColor = "#00D084";
    QString m_toggleDisabledColor = "#6B7280";
    QString m_enableButtonColor = "#00D084";
    QString m_disableButtonColor = "#6B7280";
    QString m_folderModColor = "#3B82F6";
    QString m_looseFileColor = "#EC4899";

    QVariantList m_gamePresets;
    QString m_activePresetId;
    QString m_tagColor = "#F59E0B";
    QString m_pakColor = "#F97316";
    QString m_ucasColor = "#D946EF";
    QString m_utocColor = "#3B82F6";
    QString m_pakViewerPath;
    QString m_gamePaksPath;
    QString m_usmapFilePath;
    QString m_aesKey;
    QString m_ueVersion = "UE 5.3";
    QStringList m_customGameVersions;
    QMap<QString, QString> m_customGameEnumNames;
    QMap<QString, QString> m_customGameBaseVersions;
    QMap<QString, QString> m_customGameEnumToDisplay;
    mutable QMap<QString, QString> m_customGameDetectedCache;
    int m_lastInstalledModCount = 0;
    bool m_autoHideBottomNav = false;
    int m_bottomNavAutoHideDelaySeconds = 5;
    bool m_pakViewerHideConsole = false;
    bool m_managerHideConsole = true;
    bool m_pakViewerHideUI = false;
    bool m_welcomeShown = false;
    bool m_gamesManagerWelcomeShown = false;
    bool m_showSplashScreen = true;
    bool m_runAsAdministrator = false;
    bool m_notificationsEnabled = true;
    bool m_minimizeToTray = false;
    bool m_pauseOnDefocus = false;
    bool m_pauseAnimationsOnDefocus = false;
    bool m_pauseStatsOnDefocus = false;
    bool m_limitFpsOnDefocus = false;
    std::atomic<bool> m_limitFpsOnDefocusAtomic{false};
    QMap<QString, QString> m_shortcuts;
    QMap<QString, QString> m_modImages;
    QMap<QString, QStringList> m_modTags;
    QMap<QString, QString> m_modDescriptions;
    QMap<QString, QVariantMap> m_modDetailsCache;
    QSet<QString> m_favoriteMods;
    QStringList m_pendingInstallFiles;
    QMap<QString, QString> m_modNameMappings;
    QMap<QString, QStringList> m_selectedFiles;
    QSet<QString> m_installSkipGroups;
    QMap<QString, QString> m_modIds;
    QMap<QString, QStringList> m_modAliases;
    QVariantList m_modBundles;
    QMap<QString, QString> m_installDestFolders;
    QMap<QString, QString> m_installBundleSources;
    QVariantMap m_modsFolderEnabledSnapshotByPath;
    QVariantMap m_modsFolderMassDisabledByPath;
    QString formatFileSize(qint64 size);

    QTimer* m_systemStatsTimer;
    double m_cpuUsage = 0.0;
    double m_ramUsage = 0.0;
    qint64 m_totalRamBytes = 0;
    qint64 m_usedRamBytes = 0;
    QVariantList m_driveStats;

    quint64 m_lastIdleTime = 0;
    quint64 m_lastKernelTime = 0;
    quint64 m_lastUserTime = 0;

    QVariantList m_downloadedModsList;
    bool m_isScanningDownloads = false;
    QVariantList m_nexusTrackedMods;
    QVariantList m_nexusArchives;
    QHash<QString, QVariantMap> m_pendingNexusDownloads;
    bool m_isCheckingNexusUpdates = false;
    int m_nexusUpdatesRevision = 0;
    int m_nexusUpdateCheckIndex = 0;
    QString getDownloadedFileType(const QString& extension) const;

    QVariantList m_installedGamesList;
    bool m_isGameScanRunning = false;
    QStringList m_gameScanErrors;
    std::atomic_bool m_gameScanCancelRequested = false;
    QStringList m_availableDrives;
    bool m_isUeDetectRunning = false;

    QProcess* m_conflictScanProcess = nullptr;
    QTimer* m_conflictScanTimeoutTimer = nullptr;
    bool m_isConflictScanning = false;
    bool m_conflictScanTimeoutWarningShown = false;
    QString m_conflictScanProgress;
    QVariantList m_conflictResults;
    QVariantList m_conflictGroups;
    QVariantList m_conflictModsData;
    QString m_lastConflictScanTime;
    QStringList m_conflictScanErrors;
    QString m_conflictDetectorPath;
    QString m_ueDetectorPath;

    QMap<QString, QString> m_ueVersionCache;

    QVariantList m_collections;
    void loadCollections();
    void saveCollections();
    QString getCollectionsFilePath() const;
    void cleanupDeletedModsFromCollections(const QStringList& deletedModNames);

    QStringList readSteamLibraryPaths() const;
    void findUeGamesInRoot(const QString& rootPath, QVariantList& out, QSet<QString>& seen) const;
    QVariantMap detectUeGame(const QString& gameDir) const;
    QString resolveModsFolderBase(const QVariantMap& game) const;
    void scanInstalledGamesInternal(const QStringList& driveRoots);
    QString resolveExistingSevenZipPath() const;
    QString resolveExistingPakViewerPath() const;
    QString resolveExistingConflictDetectorPath() const;
    QString resolveExistingUEDetectorPath() const;
    void parseCustomGameVersionsFromEGameCs();
    QString detectCustomGameForPath(const QString& gamePath) const;

    QString getSystemDriveLetter() const;

    bool isPathOnSystemDrive(const QString& path) const;

    bool launchDetachedElevated(const QString& exePath,
                                const QStringList& args,
                                const QString& workingDir);

    void parseConflictResults(const QByteArray& jsonData);
    void onConflictScanTimeout();
    void groupConflictsByCharacter();
    QString extractCharacterFromPath(const QString& path);
    QString determineSeverity(const QString& fileType, int modCount, const QString& path);
    QVariantMap generateSuggestion(const QVariantList& conflictingMods);
    int countModConflictFrequency(const QString& modName) const;

    class QNetworkAccessManager* m_networkManager = nullptr;
    DownloadManager* m_downloadManager = nullptr;
    class QNetworkReply* m_activeNexusDownloadReply = nullptr;
    class QFile* m_activeNexusDownloadFile = nullptr;
    class QNetworkAccessManager* ensureNetworkManager();
    NexusOAuth* m_nexusOAuth = nullptr;
    NexusWebSession* m_nexusWebSession = nullptr;

    void applyNexusAuthHeaders(QNetworkRequest& request) const;
    bool hasNexusCredentials() const;
    void onNexusOAuthAuthenticationChanged();
    void onNexusOAuthUserInfoChanged();
    void clearNexusCatalog();
    bool m_nexusEnabled = false;
    QString m_nexusApiKey;
    QString m_nexusGameDomain = "stellarblade";
    QString m_nexusUsername;
    bool m_nexusIsPremium = false;
    QString m_nexusAvatarUrl;
    int m_nexusHourlyRemaining = 2000;
    int m_nexusHourlyLimit = 2000;
    int m_nexusDailyRemaining = 20000;
    int m_nexusDailyLimit = 20000;
    QString m_nexusValidationStatus = "idle";
    QString m_nexusValidationMessage;
    QVariantList m_nexusModsList;
    QVariantMap m_nexusLatestFileVersions;
    int m_nexusTotalModsCount = 0;
    bool m_nexusIsLoadingMods = false;
    int m_nexusCurrentPage = 0;
    int m_nexusPageSize = 40;
    QString m_nexusSortField = "DOWNLOADS";
    QString m_nexusSearchQuery;
    bool m_nexusIsDownloading = false;
    double m_nexusDownloadProgress = 0.0;
    QString m_nexusDownloadStatus;
    QString m_nexusTimeRange = "all";
    QStringList m_nexusSelectedCategories;
    QString m_nexusIncludeTag;
    QString m_nexusExcludeTag;
    QString m_nexusSearchTitle;
    QString m_nexusSearchDescription;
    QString m_nexusSearchAuthor;
    QString m_nexusSearchUploader;
    bool m_nexusHideTranslations = false;
    QString m_nexusSelectedLanguage;
    QVariantList m_nexusCategoriesList;
    QVariantList m_nexusTagOptions;
    QVariantList m_nexusGamesList;
    bool m_nexusGamesLoaded = false;
    QVariantList m_nexusLanguagesList;
    QVariantMap m_currentNexusModDetails;
    QVariantList m_currentNexusModFiles;
    QVariantList m_nexusModPreviewImages;
    QVariantMap m_currentNexusModRequirements;
    NexusMedia* m_nexusMedia = nullptr;
    QString m_nexusMediaRequestKey;
    bool m_isLoadingNexusModDetails = false;
    bool m_isLoadingNexusModFiles = false;
    bool m_isLoadingNexusModPreviews = false;
    bool m_isLoadingNexusModRequirements = false;
    bool m_nexusModsRequiringLoadingMore = false;
    int m_nexusRequirementsModId = 0;
    QString m_nexusRequirementsGameDomain;
    int m_nexusModFilesModId = 0;
    QTimer* m_nexusScanRefreshTimer = nullptr;
    QMap<QString, int> m_nexusGameIds;
    QSet<QString> m_nexusGameIdLookupsInFlight;
    void fetchNexusModRequirements(int modId);
    void fetchNexusFileScanStatuses(int modId, int gameId, const QVariantList& files);
    void updateNexusScanRefresh();
    void fetchLatestNexusFileVersions(const QVariantList& mods);
    void resolveNexusGameId(const QString& gameDomain);
    void requestNexusModRequirementsPage(int offset);
    void parseNexusRateLimitHeaders(class QNetworkReply* reply);
    void startDownloadFromDirectUrl(const QUrl& url, const QString& targetFileName,
                                    const QString& modName, bool isUpdate);
    QString nexusDownloadDestination(bool isUpdate);
};
