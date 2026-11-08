cmake_minimum_required(VERSION 3.16)
project(UniversalManager VERSION 1.0 LANGUAGES CXX)

if(WIN32)
    if(NOT CMAKE_RC_COMPILER OR NOT EXISTS "${CMAKE_RC_COMPILER}")
        file(GLOB _windows_rc_candidates
            "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/rc.exe")
        if(_windows_rc_candidates)
            list(SORT _windows_rc_candidates)
            list(REVERSE _windows_rc_candidates)
            list(GET _windows_rc_candidates 0 _windows_rc_compiler)
            set(CMAKE_RC_COMPILER "${_windows_rc_compiler}" CACHE FILEPATH
                "Windows resource compiler" FORCE)
            message(STATUS "Using Windows rc.exe: ${_windows_rc_compiler}")
        endif()
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SHOW_CONSOLE OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2 LabsPlatform Concurrent Multimedia Network WebEngineCore WebEngineQuick REQUIRED)

if(WIN32 AND NOT SHOW_CONSOLE)
    qt_add_executable(UniversalManager WIN32
        src/App/main.cpp
        src/App/resourcepaths.h
        src/App/UpdateChecker.cpp
        src/App/UpdateChecker.h

        src/Core/modmanager.cpp
        src/Core/modmanager.h
        src/Core/ModManagerInternal.cpp
        src/Core/ModManagerInternal.h
        src/Core/ModManagerProperties.cpp

        src/ModLibrary/ModManagerScan.cpp
        src/ModLibrary/ModManagerInstaller.cpp
        src/ModLibrary/ModManagerAliases.cpp
        src/ModLibrary/ModManagerBundles.cpp
        src/ModLibrary/ModInstallPlanner.cpp
        src/ModLibrary/ModInstallPlanner.h
        src/ModLibrary/ModManagerOperations.cpp
        src/ModLibrary/ModManagerPakViewer.cpp
        src/ModLibrary/ModManagerCollections.cpp

        src/GamePresets/ModManagerPresets.cpp
        src/GamePresets/ModManagerGameScanner.cpp
        src/GamePresets/ModManagerCustomGames.cpp

        src/Avalonium/ModManagerAvalonium.cpp
        src/Avalonium/ModManagerUeDetection.cpp
        src/Avalonium/luaprofilemanager.cpp
        src/Avalonium/luaprofilemanager.h

        src/ConflictDetector/ModManagerConflicts.cpp
        src/Downloads/ModManagerDownloads.cpp
        src/Downloads/DownloadManager.cpp
        src/Downloads/DownloadManager.h
        src/NexusMods/ModManagerNexus.cpp
        src/NexusMods/ModManagerNexusDownloads.cpp
        src/NexusMods/ModManagerNexusUpdates.cpp
        src/NexusMods/NexusNxmRequest.cpp
        src/NexusMods/NexusNxmRequest.h
        src/NexusMods/NexusMedia.cpp
        src/NexusMods/NexusMedia.h
        src/NexusMods/NexusOAuth.cpp
        src/NexusMods/NexusOAuth.h
        src/NexusMods/NexusToken.cpp
        src/NexusMods/NexusToken.h
        src/NexusMods/NexusWebSession.cpp
        src/NexusMods/NexusWebSession.h
        src/Launcher/ModManagerLauncher.cpp

        src/Platform/windowstitlebarcontroller.cpp
        src/Platform/windowstitlebarcontroller.h
        src/Platform/AggressiveRam.cpp
        src/Platform/AggressiveRam.h
        src/Platform/TrayHelper.cpp
        src/Platform/TrayHelper.h
        src/Platform/AnimatedImageStore.cpp
        src/Platform/AnimatedImageStore.h
        src/Platform/NotificationManager.cpp
        src/Platform/NotificationManager.h
    )
else()
    qt_add_executable(UniversalManager
        src/App/main.cpp
        src/App/resourcepaths.h
        src/App/UpdateChecker.cpp
        src/App/UpdateChecker.h

        src/Core/modmanager.cpp
        src/Core/modmanager.h
        src/Core/ModManagerInternal.cpp
        src/Core/ModManagerInternal.h
        src/Core/ModManagerProperties.cpp

        src/ModLibrary/ModManagerScan.cpp
        src/ModLibrary/ModManagerInstaller.cpp
        src/ModLibrary/ModManagerAliases.cpp
        src/ModLibrary/ModManagerBundles.cpp
        src/ModLibrary/ModInstallPlanner.cpp
        src/ModLibrary/ModInstallPlanner.h
        src/ModLibrary/ModManagerOperations.cpp
        src/ModLibrary/ModManagerPakViewer.cpp
        src/ModLibrary/ModManagerCollections.cpp

        src/GamePresets/ModManagerPresets.cpp
        src/GamePresets/ModManagerGameScanner.cpp
        src/GamePresets/ModManagerCustomGames.cpp

        src/Avalonium/ModManagerAvalonium.cpp
        src/Avalonium/ModManagerUeDetection.cpp
        src/Avalonium/luaprofilemanager.cpp
        src/Avalonium/luaprofilemanager.h

        src/ConflictDetector/ModManagerConflicts.cpp
        src/Downloads/ModManagerDownloads.cpp
        src/Downloads/DownloadManager.cpp
        src/Downloads/DownloadManager.h
        src/NexusMods/ModManagerNexus.cpp
        src/NexusMods/ModManagerNexusDownloads.cpp
        src/NexusMods/ModManagerNexusUpdates.cpp
        src/NexusMods/NexusNxmRequest.cpp
        src/NexusMods/NexusNxmRequest.h
        src/NexusMods/NexusMedia.cpp
        src/NexusMods/NexusMedia.h
        src/NexusMods/NexusOAuth.cpp
        src/NexusMods/NexusOAuth.h
        src/NexusMods/NexusToken.cpp
        src/NexusMods/NexusToken.h
        src/NexusMods/NexusWebSession.cpp
        src/NexusMods/NexusWebSession.h
        src/Launcher/ModManagerLauncher.cpp

        src/Platform/windowstitlebarcontroller.cpp
        src/Platform/windowstitlebarcontroller.h
        src/Platform/AggressiveRam.cpp
        src/Platform/AggressiveRam.h
        src/Platform/TrayHelper.cpp
        src/Platform/TrayHelper.h
        src/Platform/AnimatedImageStore.cpp
        src/Platform/AnimatedImageStore.h
        src/Platform/NotificationManager.cpp
        src/Platform/NotificationManager.h
    )
endif()

target_include_directories(UniversalManager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/App
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ModLibrary
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GamePresets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Avalonium
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ConflictDetector
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Downloads
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NexusMods
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Launcher
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Platform
)

if(WIN32)
    target_sources(UniversalManager PRIVATE version.rc)
    
    configure_file(
        "${CMAKE_SOURCE_DIR}/app.manifest"
        "${CMAKE_CURRENT_BINARY_DIR}/UniversalManager.exe.manifest"
        COPYONLY
    )
    
    set_target_properties(UniversalManager PROPERTIES
        LINK_FLAGS "/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA"
    )
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(UniversalManager PRIVATE
            /O2
            /Gy                    
            /Brepro                
            /bigobj                
            /d1nodatetime          
            /experimental:deterministic 
            /d1trimfile:${CMAKE_SOURCE_DIR}/ 
        )
        target_link_options(UniversalManager PRIVATE
            /OPT:REF       
            /OPT:ICF      
            /Brepro         
            /INCREMENTAL:NO 
            /DEBUG:NONE     
        )
    endif()

    add_custom_command(TARGET UniversalManager POST_BUILD
        COMMAND powershell -NoProfile -NonInteractive -Command
            "[System.IO.File]::WriteAllBytes('$<TARGET_FILE:UniversalManager>', [System.IO.File]::ReadAllBytes('$<TARGET_FILE:UniversalManager>')) ; $b=[System.IO.File]::ReadAllBytes('$<TARGET_FILE:UniversalManager>') ; $peOff=[System.BitConverter]::ToInt32($b,60) ; $b[$peOff+8]=$b[$peOff+9]=$b[$peOff+10]=$b[$peOff+11]=0 ; [System.IO.File]::WriteAllBytes('$<TARGET_FILE:UniversalManager>',$b)"
        COMMENT "Zeroing PE timestamp for reproducible hash"
        VERBATIM
    )
endif()

if(DEFINED QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_qml_module(UniversalManager
    URI UniversalModManager
    VERSION 1.0
    NO_CACHEGEN
    RESOURCE_PREFIX "/"
    QML_FILES
        qml/main.qml
        qml/Shared/SplashScreen.qml
        qml/Shared/DownloadsPathRequiredDialog.qml
        qml/Shared/AppUpdateDialog.qml
        qml/Shared/LuaProfilesRestoredDialog.qml
        qml/Shared/ModWindowRestoreChip.qml
        qml/HomePage/HomePage.qml
        qml/HomePage/HomePageTips.qml
        qml/HomePage/HomePageNotifications.qml
        qml/ModLibrary/ModLibraryPage.qml
        qml/ModLibrary/ModLibraryPageBackupDialogs.qml
        qml/ModLibrary/BackupModsDialogs.qml
        qml/ModLibrary/ModLibraryPageModDialogs.qml
        qml/ModLibrary/ModLibraryPageCollectionDialogs.qml
        qml/ModLibrary/ModLibraryPageNotifications.qml
        qml/ModLibrary/ModLibraryPageShortcuts.qml
        qml/ModLibrary/ContextMenuItem.qml
        qml/ModLibrary/ModListItem.qml
        qml/ModLibrary/ModBundleCard.qml
        qml/ModLibrary/ModBundleListRow.qml
        qml/ModLibrary/ModBundleBreadcrumb.qml
        qml/ModLibrary/ModBundleUpdateBadge.qml
        qml/ModLibrary/ExpandingActionButton.qml
        qml/ModLibrary/ModLibraryFilters.qml
        qml/ModLibrary/ModLibraryUpdateResultDialog.qml
        qml/ModLibrary/BackupTypeCard.qml
        qml/ModLibrary/SwitchModsFolder.qml
        qml/ModLibrary/CreateModsFolderDialog.qml
        qml/Settings/SettingsPage.qml
        qml/Settings/SettingsPageAbout.qml
        qml/Settings/SettingsPageAdvanced.qml
        qml/Settings/SettingsPageAppearance.qml
        qml/Settings/SettingsPageDialogs.qml
        qml/Settings/SettingsPageDownloads.qml
        qml/Settings/GlassSurfaceSlider.qml
        qml/Settings/DownloadsSidebar.qml
        qml/Settings/DownloadsSidebarItem.qml
        qml/Settings/DownloadsToolbarButton.qml
        qml/Settings/DownloadsSearchField.qml
        qml/Settings/DownloadsActionButton.qml
        qml/Settings/DownloadTaskRow.qml
        qml/Settings/DownloadedFileRow.qml
        qml/Settings/SettingsPageNexusMods.qml
        qml/NexusMods/NexusModsPage.qml
        qml/NexusMods/NexusModsWindow.qml
        qml/NexusMods/NexusSignInDialog.qml
        qml/NexusMods/NexusDownloadWindow.qml
        qml/NexusMods/NexusPreviewViewer.qml
        qml/NexusMods/NexusVideoWindow.qml
        qml/NexusMods/NexusAnimatedViewer.qml
        qml/NexusMods/NexusRequirementTable.qml
        qml/NexusMods/NexusSortPill.qml
        qml/NexusMods/NexusRequirementsTab.qml
        qml/NexusMods/NexusModsUsingTab.qml
        qml/NexusMods/NexusFilesTab.qml
        qml/NexusMods/NexusDownloadRequirementsDialog.qml
        qml/NexusMods/NexusDescriptionTab.qml
        qml/NexusMods/NexusFilterComboBox.qml
        qml/NexusMods/NexusFilterPanel.qml
        qml/NexusMods/NexusFilterSectionLabel.qml
        qml/NexusMods/NexusFilterDivider.qml
        qml/NexusMods/NexusFilterTextField.qml
        qml/NexusMods/NexusFilterCheckBox.qml
        qml/NexusMods/NexusGameSelector.qml
        qml/Settings/SettingsPageLegacyDialogs.qml
        qml/Settings/SettingsPageModDeletion.qml
        qml/Settings/SettingsPagePickers.qml
        qml/Settings/SettingsPageShortcuts.qml
        qml/Settings/SettingsPageTools.qml
        qml/Settings/SettingsContextMenuItem.qml
        qml/GamePresetsManager/GamePresetsManager.qml
        qml/GamePresetsManager/GamePresetsManagerGameScanDialog.qml
        qml/GamePresetsManager/GamePresetsManagerContent.qml
        qml/GamePresetsManager/CrimsonActiveBadge.qml
        qml/GamePresetsManager/GlassActiveBadge.qml
        qml/GamePresetsManager/CozyActiveBadge.qml
        qml/GamePresetsManager/WaveyActiveBadge.qml
        qml/GamePresetsManager/GamePresetsManagerPresetDialogs.qml
        qml/GamePresetsManager/GamePresetsManagerModsFolderDialogs.qml
        qml/GamePresetsManager/RuntimeContextMenuItem.qml
        qml/Shared/InstallationDialog.qml
        qml/Shared/InstallDestinationSection.qml
        qml/Shared/InstallFileRow.qml
        qml/Shared/BusyRing.qml
        qml/Shared/AppNotificationHost.qml
        qml/Shared/AppTextContextMenu.qml
        qml/Shared/UpdateAvailableBadge.qml
        qml/Shared/DownloadCompleteSound.qml
        qml/Shared/ThemeConfig.qml
        qml/Shared/ThemedBackground.qml
        qml/Settings/Tutorials.qml
        qml/Settings/ColorPicker.qml
        qml/Settings/ColorPickerCard.qml
        qml/Settings/PerformanceTab.qml
        qml/Shared/WaveyBackground.qml
        qml/ConflictDetector/ConflictDetectorTab.qml
        qml/ConflictDetector/QuickConflictScanDialog.qml
        qml/AvaloniumEngine/UnrealEngineRuntimeTab.qml
        qml/AvaloniumEngine/UnrealEngineRuntimeTabDialogs.qml
        qml/AvaloniumEngine/UnrealEngineRuntimeTabAnimations.qml
        qml/AvaloniumEngine/UnrealEngineRuntimeTabNotifications.qml

    RESOURCES
        "assets/Splash.png"
        "assets/ico.ico"
        "assets/Idk.png"
        "assets/Notifications/initialized.wav"
        "assets/Notifications/downloaded.wav"
        "assets/Wavey.mp4"
        "Icons/Cozy Theme/home page.png"
        "Icons/Cozy Theme/Mods library.png"
        "Icons/Cozy Theme/setting page.png"
        "Icons/Cozy Theme/Cozy theme.png"
        "Icons/Cozy Theme/Home page/Installed Mods.png"
        "Icons/Cozy Theme/Home page/active mods.png"
        "Icons/Cozy Theme/Home page/inactive mods.png"
        "Icons/Cozy Theme/Home page/storage used.png"

        "Icons/Cozy Theme/General & Help/exe.png"
        "Icons/Cozy Theme/General & Help/Mods directory.png"
        "Icons/Cozy Theme/General & Help/Downloads.png"
        "Icons/Cozy Theme/General & Help/Backup.png"
        "Icons/Cozy Theme/General & Help/zip.png"
        "Icons/Cozy Theme/General & Help/rar.png"
        "Icons/Cozy Theme/General & Help/3d-model.png"
        "Icons/Cozy Theme/General & Help/paks.png"
        "Icons/Cozy Theme/General & Help/Mappings.usmap.png"
        "Icons/Cozy Theme/General & Help/AES key.png"
        "Icons/Cozy Theme/General & Help/shortcuts.png"
        "Icons/Cozy Theme/General & Help/Recycle bin.png"
        "Icons/Cozy Theme/General & Help/Delete forever.png"
        "Icons/Cozy Theme/General & Help/Delete Mode.png"
        "Icons/Cozy Theme/General & Help/Unreal Engine.png"
        "Icons/Cozy Theme/General & Help/hi.png"
        "Icons/Cozy Theme/General & Help/Checkmark.png"
        "Icons/Cozy Theme/General & Help/failed.png"
        "Icons/Cozy Theme/General & Help/Admin.png"
        "Icons/Cozy Theme/General & Help/Notifications.png"
        "Icons/Cozy Theme/General & Help/Minimize.png"
        "Icons/Cozy Theme/General & Help/alert.png"

        "Icons/Glassmorphic Theme/home-page.png"
        "Icons/Glassmorphic Theme/Mods library.png"
        "Icons/Glassmorphic Theme/settings page.png"
        "Icons/Glassmorphic Theme/Glassmorphic theme.png"
        "Icons/Glassmorphic Theme/Home page/Mods installed.png"
        "Icons/Glassmorphic Theme/Home page/Active mods.png"
        "Icons/Glassmorphic Theme/Home page/Inactive Mods.png"
        "Icons/Glassmorphic Theme/Home page/storage used.png"
        "Icons/Glassmorphic Theme/General & Help/exe.png"
        "Icons/Glassmorphic Theme/General & Help/Mods Dirctory.png"
        "Icons/Glassmorphic Theme/General & Help/downloads .png"
        "Icons/Glassmorphic Theme/General & Help/backup.png"
        "Icons/Glassmorphic Theme/General & Help/zip.png"
        "Icons/Glassmorphic Theme/General & Help/rar.png"
        "Icons/Glassmorphic Theme/General & Help/3d-model.png"
        "Icons/Glassmorphic Theme/General & Help/paks folder.png"
        "Icons/Glassmorphic Theme/General & Help/Mappings.usmap.png"
        "Icons/Glassmorphic Theme/General & Help/AES key.png"
        "Icons/Glassmorphic Theme/General & Help/keyboard.png"
        "Icons/Glassmorphic Theme/General & Help/delete mode.png"
        "Icons/Glassmorphic Theme/General & Help/Recycle bin.png"
        "Icons/Glassmorphic Theme/General & Help/Delete forever.png"
        "Icons/Glassmorphic Theme/General & Help/Unreal Engine.png"
        "Icons/Glassmorphic Theme/General & Help/star.svg"
        "Icons/Glassmorphic Theme/General & Help/sparkle.svg"
        "Icons/Glassmorphic Theme/General & Help/Checkmark.png"
        "Icons/Glassmorphic Theme/General & Help/failed.png"
        "Icons/Glassmorphic Theme/General & Help/failed2.png"
        "Icons/Glassmorphic Theme/General & Help/Admin.png"
        "Icons/Glassmorphic Theme/General & Help/Notifications.png"
        "Icons/Glassmorphic Theme/General & Help/Minimize.png"
        "Icons/Glassmorphic Theme/General & Help/alert.png"

        "Icons/Crimson Theme/Home page.png"
        "Icons/Crimson Theme/Mod library.png"
        "Icons/Crimson Theme/Settings page.png"
        "Icons/Crimson Theme/Crimson Theme.png"
        "Icons/Crimson Theme/Home page/Installed mods.png"
        "Icons/Crimson Theme/Home page/Active Mod.png"
        "Icons/Crimson Theme/Home page/Inactive mods.png"
        "Icons/Crimson Theme/Home page/Storage used.png"
        "Icons/Crimson Theme/General & Help/exe-file.png"
        "Icons/Crimson Theme/General & Help/Mods Dirctory.png"
        "Icons/Crimson Theme/General & Help/download.png"
        "Icons/Crimson Theme/General & Help/backup.png"
        "Icons/Crimson Theme/General & Help/zip.png"
        "Icons/Crimson Theme/General & Help/rar.png"
        "Icons/Crimson Theme/General & Help/3d-model.png"
        "Icons/Crimson Theme/General & Help/paks.png"
        "Icons/Crimson Theme/General & Help/Mappings.usmap.png"
        "Icons/Crimson Theme/General & Help/AES key.png"
        "Icons/Crimson Theme/General & Help/keyboard.png"
        "Icons/Crimson Theme/General & Help/Delete mode.png"
        "Icons/Crimson Theme/General & Help/Recycel-bin.png"
        "Icons/Crimson Theme/General & Help/Delete forever.png"
        "Icons/Crimson Theme/General & Help/Unreal Engine.png"
        "Icons/Crimson Theme/General & Help/Checkmark.png"
        "Icons/Crimson Theme/General & Help/Checkmark2.png"
        "Icons/Crimson Theme/General & Help/failed.png"
        "Icons/Crimson Theme/General & Help/failed2.png"
        "Icons/Crimson Theme/General & Help/Admin.png"
        "Icons/Crimson Theme/General & Help/Notifications.png"
        "Icons/Crimson Theme/General & Help/Minimize.png"
        "Icons/Crimson Theme/General & Help/alert.png"




        "Icons/Cozy Theme/General & Help/console.png"
        "Icons/Cozy Theme/General & Help/minimal mode.png"
        "Icons/Cozy Theme/General & Help/auto-hide.png"
        "Icons/Cozy Theme/General & Help/Rename before installation.png"

        "Icons/Glassmorphic Theme/General & Help/console.png"
        "Icons/Glassmorphic Theme/General & Help/minimal mode.png"
        "Icons/Glassmorphic Theme/General & Help/auto-hide.png"
        "Icons/Glassmorphic Theme/General & Help/Rename before installation.png"

        "Icons/Crimson Theme/General & Help/console.png"
        "Icons/Crimson Theme/General & Help/minimal mode.png"
        "Icons/Crimson Theme/General & Help/auto-hide.png"
        "Icons/Crimson Theme/General & Help/Rename before installation.png"

        "Icons/Wavey Theme/home-page.png"
        "Icons/Wavey Theme/Mods library.png"
        "Icons/Wavey Theme/setting page.png"
        "Icons/Wavey Theme/Home page/Installed Mods.png"
        "Icons/Wavey Theme/Home page/active mods.png"
        "Icons/Wavey Theme/Home page/inactive mods.png"
        "Icons/Wavey Theme/Home page/storage used.png"
        "Icons/Wavey Theme/General & Help/exe.png"
        "Icons/Wavey Theme/General & Help/Mods-directory.png"
        "Icons/Wavey Theme/General & Help/downloads.png"
        "Icons/Wavey Theme/General & Help/Backup.png"
        "Icons/Wavey Theme/General & Help/zip.png"
        "Icons/Wavey Theme/General & Help/rar.png"
        "Icons/Wavey Theme/General & Help/3d-model.png"
        "Icons/Wavey Theme/General & Help/paks.png"
        "Icons/Wavey Theme/General & Help/Mappings.usmap.png"
        "Icons/Wavey Theme/General & Help/AES key.png"
        "Icons/Wavey Theme/General & Help/shortcuts.png"
        "Icons/Wavey Theme/General & Help/Delete Mode.png"
        "Icons/Wavey Theme/General & Help/Recycle bin.png"
        "Icons/Wavey Theme/General & Help/Delete forever.png"
        "Icons/Wavey Theme/General & Help/Unreal Engine.png"
        "Icons/Wavey Theme/General & Help/console.png"
        "Icons/Wavey Theme/General & Help/minimal mode.png"
        "Icons/Wavey Theme/General & Help/auto-hide.png"
        "Icons/Wavey Theme/General & Help/Rename-before-installation.png"
        "Icons/Wavey Theme/General & Help/Detector.png"
        "Icons/Wavey Theme/General & Help/General_header.png"
        "Icons/Wavey Theme/General & Help/controller.png"
        "Icons/Wavey Theme/General & Help/Lock.png"
        "Icons/Wavey Theme/General & Help/Checkmark.png"
        "Icons/Wavey Theme/General & Help/Checkmark2.png"
        "Icons/Wavey Theme/General & Help/failed.png"
        "Icons/Wavey Theme/General & Help/failed2.png"
        "Icons/Wavey Theme/General & Help/Notifications.png"
        "Icons/Wavey Theme/General & Help/Minimize.png"
        "Icons/Wavey Theme/General & Help/alert.png"
        "Icons/Wavey Theme/General & Help/admin.png"
        "Icons/Wavey Theme/wave.png"

        "Icons/Cozy Theme/heart.svg"
        "Icons/Cozy Theme/heart-filled.svg"
        "Icons/Crimson Theme/heart.svg"
        "Icons/Crimson Theme/heart-filled.svg"
        "Icons/Wavey Theme/heart.svg"
        "Icons/Wavey Theme/heart-filled.svg"
        "Icons/Glassmorphic Theme/heart.svg"
        "Icons/Glassmorphic Theme/heart-filled.svg"


        "Icons/Cozy Theme/General & Help/Paint.png"
        "Icons/Cozy Theme/General & Help/General_header.png"
        "Icons/Cozy Theme/General & Help/Controller_header.png"
        "Icons/Cozy Theme/General & Help/Detect_heder.png"

        "Icons/Glassmorphic Theme/General & Help/General_header.png"
        "Icons/Glassmorphic Theme/General & Help/Controller_header.png"
        "Icons/Glassmorphic Theme/General & Help/Detect_header.png"

        "Icons/Crimson Theme/General & Help/General_header.png"
        "Icons/Crimson Theme/General & Help/Controller_header.png"
        "Icons/Crimson Theme/General & Help/Detect_header.png"

        "Lucide Icons/settings.svg"
        "Lucide Icons/search.svg"
        "Lucide Icons/gamepad-2.svg"
        "Lucide Icons/refresh-cw.svg"
        "Lucide Icons/folder-open.svg"
        "Lucide Icons/folder-cog.svg"
        "Lucide Icons/package-open.svg"
        "Lucide Icons/search-x.svg"
        "Lucide Icons/archive.svg"
        "Lucide Icons/lock.svg"
        "Lucide Icons/file.svg"
        "Lucide Icons/file-box.svg"
        "Lucide Icons/music.svg"
        "Lucide Icons/plus.svg"
        "Lucide Icons/library-big.svg"
        "Lucide Icons/shuffle.svg"
        "Lucide Icons/repeat.svg"
        "Lucide Icons/repeat-1.svg"
        "Lucide Icons/trash-2.svg"
        "Lucide Icons/trash.svg"
        "Lucide Icons/play.svg"
        "Lucide Icons/pause.svg"
        "Lucide Icons/skip-back.svg"
        "Lucide Icons/skip-forward.svg"
        "Lucide Icons/ellipsis.svg"
        "Lucide Icons/image-plus.svg"
        "Lucide Icons/external-link.svg"
        "Lucide Icons/feather.svg"
        "Lucide Icons/zap.svg"
        "Lucide Icons/boxes.svg"
        "Lucide Icons/check.svg"
        "Lucide Icons/Heart-Cozy.SVG"
        "Lucide Icons/Glassmorphic-Star.svg"
        "Lucide Icons/undo-2.svg"
        "Lucide Icons/redo-2.svg"
        "Lucide Icons/text-select.svg"
        "Lucide Icons/clipboard.svg"
        "Lucide Icons/scissors.svg"
        "Lucide Icons/x.svg"
        "Lucide Icons/play.svg"
        "Lucide Icons/circle-play.svg"
        "Lucide Icons/layout-list.svg"
        "Lucide Icons/activity.svg"
        "Lucide Icons/droplet.svg"
        "Lucide Icons/cpu.svg"
        "Lucide Icons/log-out.svg"
        "Lucide Icons/folder-tree.svg"
        "Lucide Icons/folder-plus.svg"
        "Lucide Icons/tag.svg"
        "Lucide Icons/folder-code.svg"
        "Lucide Icons/chevrons-down.svg"
        "Lucide Icons/chevrons-up.svg"
        "Lucide Icons/folder-minus.svg"

        "Lucide Icons/scan-search.svg"
        "Lucide Icons/scan.svg"
        "Lucide Icons/loader.svg"
        "Lucide Icons/x.svg"
        "Lucide Icons/check.svg"
        "Lucide Icons/circle-check.svg"
        "Lucide Icons/circle-alert.svg"
        "Lucide Icons/triangle-alert.svg"
        "Lucide Icons/chevron-down.svg"
        "Lucide Icons/chevron-right.svg"
        "Lucide Icons/maximize-2.svg"
        "Lucide Icons/package.svg"
        "Lucide Icons/funnel.svg"
        "Lucide Icons/arrow-up-down.svg"
        "Lucide Icons/download.svg"
        "Lucide Icons/arrow-down-to-line.svg"
        "Lucide Icons/cloud-download.svg"
        "Lucide Icons/thumbs-up.svg"
        "Lucide Icons/calendar.svg"
        "Lucide Icons/link.svg"
        "Lucide Icons/heart.svg"
        "Lucide Icons/star.svg"
        "Lucide Icons/sliders-horizontal.svg"
        "Lucide Icons/copy.svg"
        "Lucide Icons/folder-x.svg"
        "Lucide Icons/clock.svg"
        "Lucide Icons/type.svg"
        "Lucide Icons/hash.svg"
        "Lucide Icons/info.svg"
        "Lucide Icons/ban.svg"
        "Lucide Icons/eye.svg"
        "Lucide Icons/circle-slash.svg"
        "Lucide Icons/user.svg"
        "Lucide Icons/map.svg"
        "Lucide Icons/layout-dashboard.svg"
        "Lucide Icons/folder.svg"
        "Lucide Icons/image.svg"
        "Lucide Icons/images.svg"
        "Lucide Icons/palette.svg"
        "Lucide Icons/code.svg"
        "Lucide Icons/table.svg"
        "Lucide Icons/box.svg"
        "Lucide Icons/volume-2.svg"
        "Lucide Icons/languages.svg"
        "Lucide Icons/circle.svg"
        "Lucide Icons/mouse-pointer-click.svg"
        "Lucide Icons/file.svg"
        "Lucide Icons/play.svg"
        "Lucide Icons/minus.svg"
        "Lucide Icons/square.svg"
        "Lucide Icons/chevron-left.svg"
        "Lucide Icons/rocket.svg"
        "Lucide Icons/save.svg"
        "Lucide Icons/shield-check.svg"
        "Lucide Icons/circle-x.svg"
        "Lucide Icons/circle-question-mark.svg"
        "Lucide Icons/pencil.svg"
        "Lucide Icons/arrow-left.svg"
        "Lucide Icons/ellipsis-vertical.svg"
        "Lucide Icons/share.svg"
        "Lucide Icons/camera.svg"
        "Lucide Icons/copy.svg"
        "Lucide Icons/toggle-left.svg"
        "Lucide Icons/terminal.svg"
        "Lucide Icons/tags.svg"
        "Lucide Icons/layers.svg"
        "Lucide Icons/inbox.svg"
        "Lucide Icons/circle-check-big.svg"
        "Lucide Icons/folder-check.svg"

        "Lucide Icons/audio-lines.svg"
        "Lucide Icons/blocks.svg"
        "Lucide Icons/braces.svg"
        "Lucide Icons/globe.svg"
        "Lucide Icons/app-window.svg"
        "Lucide Icons/layout-panel-left.svg"
        "Lucide Icons/file-text.svg"
        "Lucide Icons/file-image.svg"
        "Lucide Icons/anchor.svg"
        "Lucide Icons/cable.svg"
        "Lucide Icons/wrench.svg"
        "Lucide Icons/search.svg"
        "Lucide Icons/package-search.svg"
        "Lucide Icons/brush.svg"

        "Icons/Cozy Theme/General & Help/Nexus.png"
        "Icons/Glassmorphic Theme/General & Help/Nexus.png"
        "Icons/Crimson Theme/General & Help/Nexus.png"
        "Icons/Wavey Theme/General & Help/Nexus.png"
)

target_link_libraries(UniversalManager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::LabsPlatform
    Qt6::Concurrent
    Qt6::Multimedia
    Qt6::Network
    Qt6::WebEngineCore
    Qt6::WebEngineQuick
)

if(WIN32)
    target_link_libraries(UniversalManager PRIVATE Crypt32 windowscodecs shlwapi ole32 version)
endif()

add_custom_command(TARGET UniversalManager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:UniversalManager>/assets"
    COMMENT "Copying assets folder to build directory"
)

if(WIN32)
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET UniversalManager POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --qmldir "${CMAKE_SOURCE_DIR}/qml"
                --no-translations
                --no-system-d3d-compiler
                --no-compiler-runtime
                --no-opengl-sw
                --skip-plugin-types position
                "$<TARGET_FILE:UniversalManager>"
            COMMENT "Running windeployqt to deploy Qt dependencies..."
        )
        message(STATUS "windeployqt found: ${WINDEPLOYQT_EXECUTABLE}")
    else()
        message(WARNING "windeployqt not found. Qt dependencies will not be automatically deployed.")
    endif()
endif()

