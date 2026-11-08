import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects
import Qt.labs.platform as Platform
import QtWebEngine

ApplicationWindow {
    id: window

    property bool _forceQuit: false
    onClosing: function(close) {
        if (_forceQuit) return
        if (modManager.minimizeToTray) {
            close.accepted = false
            window.hide()
            trayIcon.visible = true
        }
    }

    Platform.SystemTrayIcon {
        id: trayIcon
        visible: false
        tooltip: "𝚄𝚗𝚒𝚟𝚎𝚛𝚜𝚊𝚕 𝙼𝚊𝚗𝚊𝚐𝚎𝚛"
        icon.source: "qrc:/UniversalModManager/assets/ico.ico"
        onActivated: function(reason) {
            if (reason === Platform.SystemTrayIcon.Trigger || reason === Platform.SystemTrayIcon.DoubleClick) {
                window.show()
                window.raise()
                window.requestActivate()
                trayIcon.visible = false
            } else if (reason === Platform.SystemTrayIcon.Context) {
                trayMenu.open()
            }
        }
        menu: Platform.Menu {
            id: trayMenu
            Platform.Menu {
                id: profilesMenu
                title: "Profiles"
                enabled: modManager.gamePresets && modManager.gamePresets.length > 0
                Instantiator {
                    model: modManager.gamePresets
                    delegate: Platform.MenuItem {
                        required property var modelData
                        text: modelData.name || "Unnamed"
                        checkable: true
                        checked: modelData.id === modManager.activePresetId
                        onTriggered: modManager.setActivePreset(modelData.id)
                    }
                    onObjectAdded: function(index, object) { profilesMenu.insertItem(index, object) }
                    onObjectRemoved: function(index, object) { profilesMenu.removeItem(object) }
                }
            }
            Platform.MenuItem {
                text: "Show"
                onTriggered: {
                    window.show()
                    window.raise()
                    window.requestActivate()
                    trayIcon.visible = false
                }
            }
            Platform.MenuSeparator {}
            Platform.MenuItem {
                text: "Exit"
                onTriggered: {
                    window._forceQuit = true
                    window.show()
                    window.close()
                }
            }
        }
    }

    Connections {
        target: modManager
        function onAboutToRestart() {

            trayIcon.visible = false
            window._forceQuit = true

            trayHelper.cleanup()
        }
    }

    Connections {
        target: trayHelper
        function onAnotherInstanceStarted() {
            if (!window.visible || trayIcon.visible) {
                window.show()
                window.raise()
                window.requestActivate()
                trayIcon.visible = false
            }
        }
    }

    width: 1470
    minimumWidth: 1470
    minimumHeight: 944
    visible: false
    title: "𝚄𝚗𝚒𝚟𝚎𝚛𝚜𝚊𝚕 𝙼𝚊𝚗𝚊𝚐𝚎𝚛"
    flags: Qt.Window
    font.family: "Segoe UI Variable Text"

    readonly property string iconsBasePath: Qt.resolvedUrl("../Lucide Icons/")

    ThemeConfig {
        id: themeConfig
    }

    DownloadCompleteSound { }

    AppNotificationHost {
        id: appNotifications
        anchors.fill: parent
        z: 1000
        hostWindow: window
        trayIcon: trayIcon
    }

    WebEngineProfilePrototype {
        id: nexusWebProfilePrototype
        storageName: "UniversalManagerNexusWeb"
    }

    property var nexusWebSignInProfile: null

    property color bgColor: modManager.theme === "Glassmorphic" ? "transparent" : (modManager.theme === "Crimson" ? themeConfig.crimson.bgColor : (modManager.theme === "Wavey" ? "#1A0A2E" : "#efe7dd"))
    Behavior on bgColor { ColorAnimation { duration: 300 } }

    function glassSurfaceColor(opacity, darkness, alphaDelta) {
        var alpha = Math.max(0.02, Math.min(1,
            opacity + darkness * 0.4 + (alphaDelta || 0)))
        return Qt.rgba(1 - darkness * 0.96,
                       1 - darkness * 0.95,
                       1 - darkness * 0.92,
                       alpha)
    }

    property color surfaceColor: modManager.theme === "Glassmorphic"
        ? glassSurfaceColor(modManager.glassOpacity, modManager.glassDarkness, 0)
        : (modManager.theme === "Crimson" ? themeConfig.crimson.surfaceColor : (modManager.theme === "Wavey" ? Qt.rgba(0.12, 0.24, 0.37, 0.85) : "#FFFAF5"))
    Behavior on surfaceColor { ColorAnimation { duration: 300 } }

    readonly property color glassSurfaceHover: modManager.theme === "Glassmorphic"
        ? Qt.rgba(Math.min(1, surfaceColor.r + 0.05),
                  Math.min(1, surfaceColor.g + 0.05),
                  Math.min(1, surfaceColor.b + 0.05),
                  Math.min(1, surfaceColor.a + 0.06))
        : surfaceColor
    readonly property color glassSurfaceSubtle: modManager.theme === "Glassmorphic"
        ? Qt.rgba(surfaceColor.r, surfaceColor.g, surfaceColor.b,
                  Math.min(1, surfaceColor.a * 0.5))
        : surfaceColor

    property color primaryColor: {
        if (modManager.theme === "Glassmorphic") {
            return modManager.accentColor
        } else if (modManager.theme === "Crimson") {
            return themeConfig.crimson.primaryColor
        } else if (modManager.theme === "Wavey") {
            return "#E91E8C"
        } else {
            return "#BE9F7E"
        }
    }
    Behavior on primaryColor { ColorAnimation { duration: 300 } }

    property color accentColor: modManager.theme === "Glassmorphic" ? modManager.accentColor : (modManager.theme === "Crimson" ? themeConfig.crimson.accentColor : (modManager.theme === "Wavey" ? "#9B30FF" : "#D4A574"))
    Behavior on accentColor { ColorAnimation { duration: 300 } }

    property color iconColor: modManager.theme === "Glassmorphic" ? modManager.accentColor : (modManager.theme === "Crimson" ? themeConfig.crimson.iconColor : (modManager.theme === "Wavey" ? "#E91E8C" : "#BE9F7E"))
    Behavior on iconColor { ColorAnimation { duration: 300 } }

    property color archiveIconColor: modManager.theme === "Glassmorphic" ? modManager.accentColor : (modManager.theme === "Crimson" ? themeConfig.crimson.archiveIconColor : (modManager.theme === "Wavey" ? "#C9A7EB" : "#BE9F7E"))
    Behavior on archiveIconColor { ColorAnimation { duration: 300 } }

    property color textColor: modManager.theme === "Glassmorphic" ? modManager.accentColor : (modManager.theme === "Crimson" ? themeConfig.crimson.textColor : (modManager.theme === "Wavey" ? "#FFFFFF" : "#3E3E3E"))
    Behavior on textColor { ColorAnimation { duration: 300 } }

    property color textSecondaryColor: {
        if (modManager.theme === "Glassmorphic") {
            return Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.85)
        } else if (modManager.theme === "Crimson") {
            return themeConfig.crimson.textSecondaryColor
        } else if (modManager.theme === "Wavey") {
            return accentColor
        } else {
            return primaryColor
        }
    }
    Behavior on textSecondaryColor { ColorAnimation { duration: 300 } }

    property color borderColor: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.3) : (modManager.theme === "Crimson" ? themeConfig.crimson.borderColor : (modManager.theme === "Wavey" ? Qt.rgba(0.91, 0.12, 0.55, 0.4) : "#E0E0E0"))
    Behavior on borderColor { ColorAnimation { duration: 300 } }

    property int borderWidth: modManager.theme === "Wavey" ? 1.5 : 1
    Behavior on borderWidth { NumberAnimation { duration: 300 } }

    readonly property var themePalette: modManager.theme === "Glassmorphic" ? themeConfig.glassmorphic
        : modManager.theme === "Crimson" ? themeConfig.crimson
        : modManager.theme === "Wavey" ? themeConfig.wavey
        : themeConfig.cozy

    readonly property color secondaryButtonColor: themePalette.secondaryButtonColor
    readonly property color secondaryButtonHoverColor: themePalette.secondaryButtonHoverColor
    readonly property color secondaryButtonBorderColor: themePalette.secondaryButtonBorderColor
    readonly property color secondaryButtonTextColor: themePalette.secondaryButtonTextColor
    readonly property color secondaryButtonTextHoverColor: themePalette.secondaryButtonTextHoverColor
    readonly property color separatorColor: themePalette.separatorColor

    property color cardColor1: Qt.rgba(primaryColor.r, primaryColor.g, primaryColor.b, 0.2)
    Behavior on cardColor1 { ColorAnimation { duration: 300 } }

    property color cardColor2: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.2)
    Behavior on cardColor2 { ColorAnimation { duration: 300 } }

    property color cardColor3: Qt.rgba(primaryColor.r, primaryColor.g, primaryColor.b, 0.15)
    Behavior on cardColor3 { ColorAnimation { duration: 300 } }

    property color cardColor4: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.15)
    Behavior on cardColor4 { ColorAnimation { duration: 300 } }

    property int cardRadius: modManager.theme === "Wavey" ? 24 : 20
    Behavior on cardRadius { NumberAnimation { duration: 300 } }

    property color crimsonGradientTop: modManager.theme === "Crimson" ? themeConfig.crimson.cardGradientTop : "#1a0a3e"
    property color crimsonGradientBottom: modManager.theme === "Crimson" ? themeConfig.crimson.cardGradientBottom : "#0a0520"

    property color crimsonThemeCardGradientTop: modManager.theme === "Crimson" ? themeConfig.crimson.themeCardGradientTop : "#1a0a3e"
    property color crimsonThemeCardGradientBottom: modManager.theme === "Crimson" ? themeConfig.crimson.themeCardGradientBottom : "#0a0520"

    property color crimsonSidebarGradientTop: modManager.theme === "Crimson" ? themeConfig.crimson.sidebarButtonGradientTop : "#1a0a3e"
    property color crimsonSidebarGradientBottom: modManager.theme === "Crimson" ? themeConfig.crimson.sidebarButtonGradientBottom : "#0a0520"

    property color cozyNavBarGradientTop: modManager.theme === "Cozy" ? themeConfig.cozy.navBarGradientTop : "#FFFCF7"
    property color cozyNavBarGradientBottom: modManager.theme === "Cozy" ? themeConfig.cozy.navBarGradientBottom : "#FFEAD0"
    property color crimsonNavBarGradientTop: modManager.theme === "Crimson" ? themeConfig.crimson.navBarGradientTop : "#2a0821"
    property color crimsonNavBarGradientBottom: modManager.theme === "Crimson" ? themeConfig.crimson.navBarGradientBottom : "#09072f"
    property color waveyNavBarGradientTop: modManager.theme === "Wavey" ? themeConfig.wavey.navBarGradientTop : "#1e3a8a"
    property color waveyNavBarGradientBottom: modManager.theme === "Wavey" ? themeConfig.wavey.navBarGradientBottom : "#0c1e3f"

    property color cozyContextMenuGradientTop: modManager.theme === "Cozy" ? themeConfig.cozy.contextMenuGradientTop : "#FFFFFF"
    property color cozyContextMenuGradientBottom: modManager.theme === "Cozy" ? themeConfig.cozy.contextMenuGradientBottom : "#F0F0F0"
    property color crimsonContextMenuGradientTop: modManager.theme === "Crimson" ? themeConfig.crimson.themeCardGradientTop : "#1a0a3e"
    property color crimsonContextMenuGradientBottom: modManager.theme === "Crimson" ? themeConfig.crimson.themeCardGradientBottom : "#0a0520"

    property color cozyDialogGradientTop: modManager.theme === "Cozy" ? themeConfig.cozy.dialogGradientTop : "#FFFFFF"
    property color cozyDialogGradientBottom: modManager.theme === "Cozy" ? themeConfig.cozy.dialogGradientBottom : "#F0F0F0"
    property color crimsonDialogGradientTop: modManager.theme === "Crimson" ? themeConfig.crimson.dialogGradientTop : "#1a0a3e"
    property color crimsonDialogGradientBottom: modManager.theme === "Crimson" ? themeConfig.crimson.dialogGradientBottom : "#0a0520"
    property color waveyDialogGradientTop: modManager.theme === "Wavey" ? themeConfig.wavey.dialogGradientTop : "#1e3a5f"
    property color waveyDialogGradientBottom: modManager.theme === "Wavey" ? themeConfig.wavey.dialogGradientBottom : "#0a1a3f"

    property color cozyToastGradientTop: modManager.theme === "Cozy" ? themeConfig.cozy.toastGradientTop : "#FFFFFF"
    property color cozyToastGradientBottom: modManager.theme === "Cozy" ? themeConfig.cozy.toastGradientBottom : "#F0F0F0"
    property color cozyToastBorderColor: modManager.theme === "Cozy" ? themeConfig.cozy.toastBorderColor : "#E0E0E0"
    property color cozyToastTextColor: modManager.theme === "Cozy" ? themeConfig.cozy.toastTextColor : "#000000"
    property color cozyToastIconColor: modManager.theme === "Cozy" ? themeConfig.cozy.toastIconColor : "#D6A963"
    property color cozyToastAccentColor: modManager.theme === "Cozy" ? themeConfig.cozy.toastAccentColor : "#D6A963"

    property color crimsonToastGradientTop: modManager.theme === "Crimson" ? themeConfig.crimson.toastGradientTop : "#1a0a3e"
    property color crimsonToastGradientBottom: modManager.theme === "Crimson" ? themeConfig.crimson.toastGradientBottom : "#0a0520"
    property color crimsonToastBorderColor: modManager.theme === "Crimson" ? themeConfig.crimson.toastBorderColor : "#5a124f"
    property color crimsonToastTextColor: modManager.theme === "Crimson" ? themeConfig.crimson.toastTextColor : "#ffffff"
    property color crimsonToastIconColor: modManager.theme === "Crimson" ? themeConfig.crimson.toastIconColor : "#EC4899"
    property color crimsonToastAccentColor: modManager.theme === "Crimson" ? themeConfig.crimson.toastAccentColor : "#A78BFA"

    property color waveyToastGradientTop: modManager.theme === "Wavey" ? themeConfig.wavey.toastGradientTop : "#1e3a8a"
    property color waveyToastGradientBottom: modManager.theme === "Wavey" ? themeConfig.wavey.toastGradientBottom : "#0c1e3f"
    property color waveyToastBorderColor: modManager.theme === "Wavey" ? themeConfig.wavey.toastBorderColor : Qt.rgba(59, 130, 246, 0.4)
    property color waveyToastTextColor: modManager.theme === "Wavey" ? themeConfig.wavey.toastTextColor : "#e0f2fe"
    property color waveyToastIconColor: modManager.theme === "Wavey" ? themeConfig.wavey.toastIconColor : "#3b82f6"
    property color waveyToastAccentColor: modManager.theme === "Wavey" ? themeConfig.wavey.toastAccentColor : "#60a5fa"

    property color modWindowGlowTop: {
        if (modManager.theme === "Cozy")
            return themeConfig.cozy.windowGlowGradientTop
        if (modManager.theme === "Crimson")
            return themeConfig.crimson.windowGlowGradientTop
        if (modManager.theme === "Wavey")
            return themeConfig.wavey.windowGlowGradientTop
        return themeConfig.glassmorphic.windowGlowGradientTop || window.accentColor
    }
    property color modWindowGlowBottom: {
        if (modManager.theme === "Cozy")
            return themeConfig.cozy.windowGlowGradientBottom
        if (modManager.theme === "Crimson")
            return themeConfig.crimson.windowGlowGradientBottom
        if (modManager.theme === "Wavey")
            return themeConfig.wavey.windowGlowGradientBottom
        return themeConfig.glassmorphic.windowGlowGradientBottom || window.primaryColor
    }

    property bool shadowEnabled: true
    property bool appUnfocused: Qt.application.state !== Qt.ApplicationActive

    property bool masterPaused: modManager.pauseOnDefocus && appUnfocused
    property bool animationsPaused: (modManager.pauseAnimationsOnDefocus || modManager.pauseOnDefocus) && appUnfocused
    property bool statsPaused: (modManager.pauseStatsOnDefocus || modManager.pauseOnDefocus) && appUnfocused
    property bool blurReduced: modManager.pauseOnDefocus && appUnfocused
    property bool fpsLimited: modManager.pauseOnDefocus && appUnfocused

    onAppUnfocusedChanged: {

        if (!appUnfocused) {
            modManager.setLimitFpsOnDefocus(false)
        } else {
            modManager.setLimitFpsOnDefocus(fpsLimited)
        }

        if (appUnfocused && (modManager.pauseOnDefocus || modManager.pauseAnimationsOnDefocus || modManager.pauseStatsOnDefocus)) {
            modManager.trimMemory()
        }
    }

    function getAnimationDuration(baseDuration) {
        return animationsPaused ? 0 : baseDuration
    }

    Component.onCompleted: {

        x = (Screen.width - width) / 2
        y = (Screen.height - height) / 2
        initializeShortcuts()

        nexusWebSignInProfile = nexusWebProfilePrototype.instance()
        nexusWebSession.attachSignInProfile(nexusWebSignInProfile)

        if (!modManager || !modManager.showSplashScreen) {
            show()
        }
    }

    property color navBarIconColor: modManager.navBarIconColor

    property string shortcutInstall: "Ctrl+I"
    property string shortcutBackup: "Ctrl+B"
    property string shortcutRefresh: "F5"
    property string shortcutSettings: "Ctrl+,"
    property string shortcutSearch: "Ctrl+F"
    property string shortcutSelectAll: "Ctrl+A"
    property string shortcutDelete: "Delete"
    property string shortcutToggleMod: "Space"
    property string shortcutOpenFolder: "Ctrl+O"
    property string shortcutHome: "Ctrl+H"
    property string shortcutEnableAll: "Ctrl+Shift+E"
    property string shortcutDisableAll: "Ctrl+Shift+D"
    property string shortcutShowInPakViewer: "Ctrl+P"
    property string shortcutExportModStructure: "Ctrl+E"

    function initializeShortcuts() {
        shortcutInstall = modManager.getShortcut("install", shortcutInstall)
        shortcutBackup = modManager.getShortcut("backup", shortcutBackup)
        shortcutRefresh = modManager.getShortcut("refresh", shortcutRefresh)
        shortcutSettings = modManager.getShortcut("settings", shortcutSettings)
        shortcutSearch = modManager.getShortcut("search", shortcutSearch)
        shortcutSelectAll = modManager.getShortcut("selectAll", shortcutSelectAll)
        shortcutDelete = modManager.getShortcut("delete", shortcutDelete)
        shortcutToggleMod = modManager.getShortcut("toggleMod", shortcutToggleMod)
        shortcutOpenFolder = modManager.getShortcut("openFolder", shortcutOpenFolder)
        shortcutHome = modManager.getShortcut("home", shortcutHome)
        shortcutEnableAll = modManager.getShortcut("enableAll", shortcutEnableAll)
        shortcutDisableAll = modManager.getShortcut("disableAll", shortcutDisableAll)
        shortcutShowInPakViewer = modManager.getShortcut("showInPakViewer", shortcutShowInPakViewer)
        shortcutExportModStructure = modManager.getShortcut("exportModStructure", shortcutExportModStructure)
    }

    function updateShortcut(key, shortcut) {
        switch (key) {
        case "install":
            shortcutInstall = shortcut
            break
        case "backup":
            shortcutBackup = shortcut
            break
        case "refresh":
            shortcutRefresh = shortcut
            break
        case "settings":
            shortcutSettings = shortcut
            break
        case "search":
            shortcutSearch = shortcut
            break
        case "selectAll":
            shortcutSelectAll = shortcut
            break
        case "delete":
            shortcutDelete = shortcut
            break
        case "toggleMod":
            shortcutToggleMod = shortcut
            break
        case "openFolder":
            shortcutOpenFolder = shortcut
            break
        case "home":
            shortcutHome = shortcut
            break
        case "enableAll":
            shortcutEnableAll = shortcut
            break
        case "disableAll":
            shortcutDisableAll = shortcut
            break
        case "showInPakViewer":
            shortcutShowInPakViewer = shortcut
            break
        case "exportModStructure":
            shortcutExportModStructure = shortcut
            break
        default:
            return
        }

        modManager.setShortcut(key, shortcut)
    }

    function openArchiveToolSettings() {
        pageStack.currentIndex = 2
        settingsPage.settingsTabIndex = 0
        settingsPage.generalSectionIndex = 2
        settingsPage.highlightArchiveToolCard()
    }

    function openNexusSignIn() {
        nexusSignInDialog.begin()
    }

    function openNexusApiKeySettings() {
        pageStack.currentIndex = 2
        settingsPage.focusNexusMods()
    }

    function showDownloadsPathRequired() {
        if (!window.visible) {
            window.show()
            trayIcon.visible = false
        }
        window.raise()
        window.requestActivate()
        downloadsPathRequiredDialog.begin()
    }

    function openDownloadsPathSettings() {
        nexusModsPage.minimizeModDetailsWindow()
        if (!window.visible) {
            window.show()
            trayIcon.visible = false
        }
        window.raise()
        window.requestActivate()
        pageStack.currentIndex = 2
        settingsPage.focusDownloadPaths()
    }

    Shortcut {
        sequence: window.shortcutSettings
        onActivated: pageStack.currentIndex = 2
    }

    Shortcut {
        sequence: window.shortcutHome
        onActivated: pageStack.currentIndex = 0
    }

    Connections {
        target: modManager
        function onNavigateToGeneralTab() {
            pageStack.currentIndex = 2
            settingsPage.settingsTabIndex = 0
        }
        function onDownloadsPathRequired() {
            window.showDownloadsPathRequired()
        }
    }

    background: Rectangle {
        color: modManager.theme === "Glassmorphic"
            ? "transparent"
            : (modManager.theme === "Wavey"
                ? (window.focusAnimationsPaused ? "#1A0A2E" : "transparent")
                : (modManager.theme === "Crimson" ? "transparent" : bgColor))
        clip: true

        Loader {
            id: waveyBackgroundLoader
            anchors.fill: parent
            active: modManager.theme === "Wavey" && !window.focusAnimationsPaused
            sourceComponent: WaveyBackground {
                theme: themeConfig.wavey
                hostWindow: window
            }
        }

        Image {
            id: glassWallpaper
            anchors.fill: parent
            anchors.margins: -20
            source: modManager.theme === "Glassmorphic" && modManager.wallpaperPath !== "" ? "file:///" + modManager.wallpaperPath : ""
            fillMode: Image.PreserveAspectCrop
            visible: source != ""
            asynchronous: true
            cache: true
            smooth: false
            mipmap: false

            layer.enabled: !window.focusAnimationsPaused
            layer.smooth: false
            layer.effect: MultiEffect {
                blurEnabled: true
                blur: modManager.reduceBlurOnDefocus && Qt.application.state !== Qt.ApplicationActive
                    ? Math.max(0.05, modManager.glassBlur * 0.25)
                    : modManager.glassBlur
                blurMax: modManager.reduceBlurOnDefocus && Qt.application.state !== Qt.ApplicationActive ? 20 : 100
                autoPaddingEnabled: false
            }

            Rectangle {
                anchors.fill: parent
                color: "black"
                opacity: 0.201
                visible: modManager.theme === "Glassmorphic"
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -20
            visible: modManager.theme === "Glassmorphic" && modManager.wallpaperPath === ""
            color: "#000000"
        }

        Image {
            anchors.fill: parent
            source: modManager.theme === "Crimson" ? "qrc:/UniversalModManager/assets/Idk.png" : ""
            fillMode: Image.PreserveAspectCrop
            visible: source != ""
            sourceSize.width: 1920
            sourceSize.height: 1080
            cache: true
            asynchronous: true

            layer.enabled: modManager.theme === "Crimson"
            layer.effect: MultiEffect {
                blurEnabled: true
                blur: themeConfig.crimson.backgroundBlur
                blurMax: themeConfig.crimson.backgroundBlurMax
                autoPaddingEnabled: false
            }
        }
    }

    DropArea {
        anchors.fill: parent

        onEntered: (drag) => {
            if (drag.hasUrls) {
                drag.accept(Qt.CopyAction)
                dropOverlay.visible = true
            }
        }

        onExited: {
            dropOverlay.visible = false
        }

        onDropped: (drop) => {
            dropOverlay.visible = false
            if (drop.hasUrls) {
                var files = []
                for (var i = 0; i < drop.urls.length; i++) {
                    var filePath = drop.urls[i].toString().replace("file:///", "")
                    files.push(filePath)
                }

                modLibraryPage.installingModCount = files.length
                modManager.installModsAsync(files)
            }
        }
    }

    Rectangle {
        id: appContent
        anchors.fill: parent
        color: "transparent"

        ColumnLayout {
            id: mainColumn
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"

                StackLayout {
                    id: pageStack
                    anchors.fill: parent
                    currentIndex: 0

                    Component.onCompleted: {

                        if (downloadManager.count > 0) {
                            pageStack.currentIndex = 2
                            settingsPage.settingsTabIndex = 0
                            settingsPage.generalSectionIndex = 5
                        }
                    }

                    HomePage { }
                    ModLibraryPage {
                        id: modLibraryPage
                        objectName: "modLibraryPage"
                    }
                    SettingsPage { id: settingsPage }
                    NexusModsPage { id: nexusModsPage }

                    Connections {
                        target: modManager
                        function onNexusEnabledChanged() {
                            if (!modManager.nexusEnabled && pageStack.currentIndex === 3) {
                                pageStack.currentIndex = 0
                            }
                        }
                    }

                    Connections {
                        target: downloadManager
                        function onDownloadStarted() {
                            pageStack.currentIndex = 2
                            settingsPage.settingsTabIndex = 0
                            settingsPage.generalSectionIndex = 5
                            window.raise()
                            window.requestActivate()
                        }
                    }
                }
            }
        }

        Item {
            id: bottomNavBar
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            width: navRow.width + 32
            height: 64
            z: 10

            property bool navHidden: false

            function resetAutoHide() {
                if (!modManager.autoHideBottomNav) {
                    navHidden = false
                    return
                }
                navHidden = false
                autoHideTimer.restart()
            }

            opacity: navHidden ? 0.0 : 1.0
            anchors.bottomMargin: navHidden ? -36 : 16
            visible: true

            Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
            Behavior on anchors.bottomMargin { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

            Timer {
                id: autoHideTimer
                interval: Math.max(1000, modManager.bottomNavAutoHideDelaySeconds * 1000)
                repeat: false
                onTriggered: {
                    if (modManager.autoHideBottomNav) {
                        bottomNavBar.navHidden = true
                    }
                }
            }

            Connections {
                target: modManager
                function onAutoHideBottomNavChanged() {
                    bottomNavBar.resetAutoHide()
                }
                function onBottomNavAutoHideDelaySecondsChanged() {
                    bottomNavBar.resetAutoHide()
                }
            }

            Component.onCompleted: resetAutoHide()

            layer.enabled: false

            Rectangle {
                id: navBarBackground
                anchors.fill: parent
                radius: 25

                color: "transparent"

                property Gradient navBarThemeGradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop {
                        position: 0.0
                        color: {
                            if (modManager.theme === "Cozy") return window.cozyNavBarGradientTop
                            if (modManager.theme === "Crimson") return window.crimsonNavBarGradientTop
                            return window.waveyNavBarGradientTop
                        }
                    }
                    GradientStop {
                        position: 1.0
                        color: {
                            if (modManager.theme === "Cozy") return window.cozyNavBarGradientBottom
                            if (modManager.theme === "Crimson") return window.crimsonNavBarGradientBottom
                            return window.waveyNavBarGradientBottom
                        }
                    }
                }
                gradient: modManager.theme === "Glassmorphic" ? null : navBarThemeGradient
                clip: true

                property real blurFactor: Math.min(modManager.glassBlur, 1.0)
                property color navBarTintColor: modManager.theme === "Crimson" ? themeConfig.crimson.bgColor : Qt.color(modManager.navBarColor)
                Behavior on navBarTintColor { ColorAnimation { duration: 300 } }

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: "transparent"
                    visible: modManager.theme === "Glassmorphic"
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: Qt.rgba(
                                navBarBackground.navBarTintColor.r,
                                navBarBackground.navBarTintColor.g,
                                navBarBackground.navBarTintColor.b,
                                0.75
                            )
                        }
                        GradientStop {
                            position: 1.0
                            color: Qt.rgba(
                                navBarBackground.navBarTintColor.r,
                                navBarBackground.navBarTintColor.g,
                                navBarBackground.navBarTintColor.b,
                                0.55
                            )
                        }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: "transparent"

                    opacity: modManager.theme === "Glassmorphic" ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 300 } }
                    gradient: Gradient {
                        GradientStop { position: 0.0;  color: Qt.rgba(1, 1, 1, 0.22) }
                        GradientStop { position: 0.45; color: Qt.rgba(1, 1, 1, 0.05) }
                        GradientStop { position: 1.0;  color: Qt.rgba(1, 1, 1, 0.0) }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: "transparent"
                    border.width: 1
                    border.color: modManager.theme === "Glassmorphic"
                        ? Qt.rgba(1, 1, 1, 0.12 + 0.20 * navBarBackground.blurFactor)
                        : (modManager.theme === "Crimson" ? window.primaryColor : window.borderColor)
                    Behavior on border.color { ColorAnimation { duration: 300 } }
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: parent.radius - 1
                    color: "transparent"
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.06 + 0.16 * navBarBackground.blurFactor)
                    opacity: modManager.theme === "Glassmorphic" ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 300 } }
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.AllButtons
            }

            Row {
                id: navRow
                anchors.centerIn: parent
                spacing: 8

                TopNavButton {
                    iconSource: modManager.theme === "Glassmorphic"
                        ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/home-page.png"
                        : (modManager.theme === "Crimson"
                            ? "qrc:/UniversalModManager/Icons/Crimson Theme/Home page.png"
                            : (modManager.theme === "Wavey"
                                ? "qrc:/UniversalModManager/Icons/Wavey Theme/home-page.png"
                                : "qrc:/UniversalModManager/Icons/Cozy Theme/home page.png"))
                    label: "Home"
                    isActive: pageStack.currentIndex === 0
                    onClicked: pageStack.currentIndex = 0
                }

                TopNavButton {
                    iconSource: modManager.theme === "Glassmorphic"
                        ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/Mods library.png"
                        : (modManager.theme === "Crimson"
                            ? "qrc:/UniversalModManager/Icons/Crimson Theme/Mod library.png"
                            : (modManager.theme === "Wavey"
                                ? "qrc:/UniversalModManager/Icons/Wavey Theme/Mods library.png"
                                : "qrc:/UniversalModManager/Icons/Cozy Theme/Mods library.png"))
                    label: "Mods Library"
                    isActive: pageStack.currentIndex === 1
                    onClicked: pageStack.currentIndex = 1
                }

                TopNavButton {
                    iconSource: modManager.theme === "Glassmorphic"
                        ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/settings page.png"
                        : (modManager.theme === "Crimson"
                            ? "qrc:/UniversalModManager/Icons/Crimson Theme/Settings page.png"
                            : (modManager.theme === "Wavey"
                                 ? "qrc:/UniversalModManager/Icons/Wavey Theme/setting page.png"
                                : "qrc:/UniversalModManager/Icons/Cozy Theme/setting page.png"))
                    label: "Advanced"
                    isActive: pageStack.currentIndex === 2
                    onClicked: pageStack.currentIndex = 2
                }

                TopNavButton {
                    visible: modManager.nexusEnabled
                    iconSource: modManager.theme === "Glassmorphic"
                        ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Nexus.png"
                        : (modManager.theme === "Crimson"
                            ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Nexus.png"
                            : (modManager.theme === "Wavey"
                                ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Nexus.png"
                                : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Nexus.png"))
                    label: "Nexus Mods"
                    isActive: pageStack.currentIndex === 3
                    onClicked: pageStack.currentIndex = 3
                }

                Item {
                    id: nexusPagerSegment
                    readonly property bool expanded: pageStack.currentIndex === 3 && modManager.nexusTotalModsCount > 0

                    readonly property color pagerAccent: modManager.theme === "Glassmorphic"
                        ? window.navBarIconColor
                        : window.primaryColor
                    property real reveal: 0

                    function stagedOpacity(threshold) {
                        return Math.max(0, Math.min(1, (reveal - threshold) * 2.6))
                    }

                    enabled: expanded
                    visible: width > 0.5
                    clip: true
                    implicitHeight: 48
                    width: Math.max(0, reveal) * (nexusPagerRow.implicitWidth + 22)

                    states: State {
                        name: "open"
                        when: nexusPagerSegment.expanded
                        PropertyChanges { target: nexusPagerSegment; reveal: 1 }
                    }

                    transitions: [
                        Transition {
                            to: "open"
                            NumberAnimation {
                                target: nexusPagerSegment
                                property: "reveal"
                                duration: 420
                                easing.type: Easing.OutBack
                                easing.overshoot: 1.12
                            }
                        },
                        Transition {
                            from: "open"
                            NumberAnimation {
                                target: nexusPagerSegment
                                property: "reveal"
                                duration: 220
                                easing.type: Easing.InCubic
                            }
                        }
                    ]

                    Row {
                        id: nexusPagerRow
                        anchors.left: parent.left
                        anchors.leftMargin: 10 + 8 * (1 - nexusPagerSegment.stagedOpacity(0))
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8

                        Rectangle {
                            width: 1
                            height: 24
                            color: Qt.rgba(nexusPagerSegment.pagerAccent.r, nexusPagerSegment.pagerAccent.g, nexusPagerSegment.pagerAccent.b, 0.25)
                            opacity: nexusPagerSegment.stagedOpacity(0.02)
                        }

                        Item {
                            id: nexusPrevBtn
                            width: 24
                            height: 28
                            enabled: modManager.nexusCurrentPage > 0
                            opacity: (enabled ? 1.0 : 0.3) * nexusPagerSegment.stagedOpacity(0.18)

                            Image {
                                anchors.centerIn: parent
                                width: 20
                                height: 20
                                source: "qrc:/UniversalModManager/Lucide Icons/chevron-left.svg"
                                sourceSize: Qt.size(20, 20)
                                smooth: true
                                opacity: nexusPrevMouse.containsMouse && nexusPrevBtn.enabled ? 1.0 : 0.9
                                Behavior on opacity { NumberAnimation { duration: 120 } }

                                layer.enabled: true
                                layer.effect: ColorOverlay {
                                    color: nexusPagerSegment.pagerAccent
                                }
                            }

                            MouseArea {
                                id: nexusPrevMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: parent.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: if (parent.enabled) modManager.previousNexusPage()
                            }
                        }

                        Text {
                            id: nexusPageLabel
                            height: 28
                            verticalAlignment: Text.AlignVCenter
                            text: (modManager.nexusCurrentPage + 1) + " / " + Math.max(1, Math.ceil(modManager.nexusTotalModsCount / modManager.nexusPageSize))
                            font.pixelSize: 13
                            font.weight: Font.Bold
                            font.letterSpacing: 0.3
                            color: nexusPagerSegment.pagerAccent
                            opacity: nexusPagerSegment.stagedOpacity(0.32)
                        }

                        Item {
                            id: nexusNextBtn
                            width: 24
                            height: 28
                            enabled: (modManager.nexusCurrentPage + 1) * modManager.nexusPageSize < modManager.nexusTotalModsCount
                            opacity: (enabled ? 1.0 : 0.3) * nexusPagerSegment.stagedOpacity(0.46)

                            Image {
                                anchors.centerIn: parent
                                width: 20
                                height: 20
                                source: "qrc:/UniversalModManager/Lucide Icons/chevron-right.svg"
                                sourceSize: Qt.size(20, 20)
                                smooth: true
                                opacity: nexusNextMouse.containsMouse && nexusNextBtn.enabled ? 1.0 : 0.9
                                Behavior on opacity { NumberAnimation { duration: 120 } }

                                layer.enabled: true
                                layer.effect: ColorOverlay {
                                    color: nexusPagerSegment.pagerAccent
                                }
                            }

                            MouseArea {
                                id: nexusNextMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: parent.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: if (parent.enabled) modManager.nextNexusPage()
                            }
                        }
                    }
                }
            }

            HoverHandler {
                acceptedDevices: PointerDevice.Mouse
                onActiveChanged: {
                    if (active) {
                        bottomNavBar.resetAutoHide()
                    }
                }
                onPointChanged: bottomNavBar.resetAutoHide()
            }

            MouseArea {
                anchors.fill: parent
                enabled: modManager.generating
                z: 100
            }
        }

        Item {
            id: bottomNavRevealZone
            anchors.horizontalCenter: bottomNavBar.horizontalCenter
            anchors.bottom: parent.bottom
            width: bottomNavBar.width
            height: 12
            z: 11
            opacity: 0.0
            visible: modManager.autoHideBottomNav && bottomNavBar.navHidden

            HoverHandler {
                acceptedDevices: PointerDevice.Mouse
                onActiveChanged: {
                    if (active) {
                        bottomNavBar.resetAutoHide()
                    }
                }
                onPointChanged: bottomNavBar.resetAutoHide()
            }
        }

        ModWindowRestoreChip {
            id: modWindowRestoreChip
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            x: settingsPage.navSidebarSceneX
                + (settingsPage.navSidebarWidth - width) / 2
            z: 12
            visible: nexusModsPage.modDetailsMinimized || opacity > 0
            opacity: nexusModsPage.modDetailsMinimized ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
            glowTop: window.modWindowGlowTop
            glowBottom: window.modWindowGlowBottom
            onClicked: nexusModsPage.restoreModDetails()
            onClosed: nexusModsPage.closeParkedModDetails()
        }
    }

    Rectangle {
        id: dropOverlay
        anchors.fill: parent
        color: "#1A1A1A"
        opacity: 0.8
        visible: false
        radius: 20
        border.width: modManager.theme === "Glassmorphic" ? 3 : 4
        border.color: window.primaryColor
        z: 1000

        Behavior on opacity { NumberAnimation { duration: 200 } }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 15

            Item {
                Layout.alignment: Qt.AlignHCenter
                width: 64
                height: 64

                Image {
                    id: boxesIcon
                    anchors.fill: parent
                    source: window.iconsBasePath + "boxes.svg"
                    sourceSize: Qt.size(64, 64)
                    fillMode: Image.PreserveAspectFit
                    visible: false
                }

                ColorOverlay {
                    anchors.fill: parent
                    source: boxesIcon
                    color: "#FFFFFF"
                }
            }

            Text {
                text: "Drop mod files here to install"
                font.pixelSize: 24
                font.weight: 600
                color: "#FFFFFF"
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: "Supports folders, .zip, .rar, .7z, .pak, .ucas, .utoc"
                font.pixelSize: 14
                color: Qt.rgba(1, 1, 1, 0.85)
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    InstallationDialog {
        id: installationDialog

        onAccepted: {

            installationDialog.close()

            closeTimer.start()
        }

        onRejected: {

            modManager.cancelModInstallation()
        }
    }

    Timer {
        id: closeTimer
        interval: 250
        repeat: false
        onTriggered: {

            var mappings = installationDialog.collectMappings()
            modManager.confirmModInstallation(mappings)
            installProgressDialog.open()
        }
    }

    Dialog {
        id: installProgressDialog
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        width: 500
        height: 290
        padding: 0
        property color loaderAccentStart: window.primaryColor
        property color loaderAccentEnd: window.accentColor
        property color loaderHighlight: window.iconColor
        property color loaderTrackColor: modManager.theme === "Glassmorphic"
            ? Qt.rgba(window.iconColor.r, window.iconColor.g, window.iconColor.b, 0.18)
            : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)
        property double openedAtMs: 0
        property int minimumVisibleMs: 1700
        property int finishHoldMs: 520

        onOpened: openedAtMs = Date.now()
        onClosed: installFinishTimer.stop()

        Timer {
            id: installFinishTimer
            repeat: false
            onTriggered: installProgressDialog.close()
        }

        background: Rectangle {
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.15, 0.15, 0.15, 0.98) : window.surfaceColor
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25) : window.borderColor

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.6)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.85)

            Behavior on opacity {
                NumberAnimation { duration: 250 }
            }
        }

        enter: Transition {
            NumberAnimation {
                property: "scale"
                from: 0.9
                to: 1.0
                duration: 250
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "scale"
                from: 1.0
                to: 0.95
                duration: 200
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 100
                color: "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 32
                    spacing: 16

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "Installing Mods"
                            font.pixelSize: 24
                            font.weight: Font.DemiBold
                            color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                            font.family: "Segoe UI Variable Display"
                        }

                        Text {
                            id: installProgressText
                            text: "Preparing..."
                            font.pixelSize: 14
                            font.weight: Font.Normal
                            color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                            opacity: 0.85
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 24

                Rectangle {
                    id: installLoaderStage
                    anchors.fill: parent
                    radius: 20
                    clip: true
                    color: modManager.theme === "Glassmorphic"
                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.04)
                        : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.035)
                    border.width: 1
                    border.color: modManager.theme === "Glassmorphic"
                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08)
                        : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08)

                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        opacity: 0.95
                        gradient: Gradient {
                            GradientStop {
                                position: 0.0
                                color: modManager.theme === "Glassmorphic"
                                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.035)
                                    : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.04)
                            }
                            GradientStop {
                                position: 0.52
                                color: "transparent"
                            }
                            GradientStop {
                                position: 1.0
                                color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.035)
                            }
                        }
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width - 72
                        height: 2
                        radius: 1
                        opacity: 0.7
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 0.18; color: installProgressDialog.loaderTrackColor }
                            GradientStop { position: 0.5; color: Qt.rgba(installProgressDialog.loaderAccentEnd.r, installProgressDialog.loaderAccentEnd.g, installProgressDialog.loaderAccentEnd.b, 0.38) }
                            GradientStop { position: 0.82; color: installProgressDialog.loaderTrackColor }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }

                    Rectangle {
                        id: installTrackSweep
                        width: 60
                        height: 6
                        radius: 3
                        y: installLoaderStage.height / 2 - height / 2
                        x: -width
                        rotation: 0
                        opacity: 0.38
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 0.5; color: Qt.rgba(installProgressDialog.loaderHighlight.r, installProgressDialog.loaderHighlight.g, installProgressDialog.loaderHighlight.b, 0.28) }
                            GradientStop { position: 1.0; color: "transparent" }
                        }

                        SequentialAnimation on x {
                            running: installProgressDialog.visible
                            loops: Animation.Infinite
                            PauseAnimation { duration: 90 }
                            NumberAnimation {
                                from: -installTrackSweep.width
                                to: installLoaderStage.width
                                duration: 1220
                                easing.type: Easing.InOutQuad
                            }
                            PauseAnimation { duration: 150 }
                        }
                    }

                    Rectangle {
                        id: installCenterCard
                        width: 66
                        height: 82
                        radius: 12
                        anchors.centerIn: parent
                        z: 4
                        border.width: 1
                        border.color: Qt.rgba(
                            installProgressDialog.loaderHighlight.r,
                            installProgressDialog.loaderHighlight.g,
                            installProgressDialog.loaderHighlight.b,
                            modManager.theme === "Glassmorphic" ? 0.32 : 0.22
                        )
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: installProgressDialog.loaderAccentStart }
                            GradientStop { position: 1.0; color: installProgressDialog.loaderAccentEnd }
                        }

                        Rectangle {
                            x: 10
                            y: 11
                            width: 34
                            height: 5
                            radius: 2.5
                            color: Qt.rgba(1, 1, 1, 0.94)
                        }

                        Rectangle {
                            x: 10
                            y: 21
                            width: 22
                            height: 5
                            radius: 2.5
                            color: Qt.rgba(1, 1, 1, 0.76)
                        }

                    }

                    Repeater {
                        model: 7

                        delegate: Rectangle {
                            property int itemIndex: index
                            property real midX: installLoaderStage.width * 0.5
                            property real baseY: installLoaderStage.height / 2 - height / 2 + (itemIndex % 2 === 0 ? -9 : 7)
                            width: itemIndex % 2 === 0 ? 24 : 30
                            height: itemIndex % 2 === 0 ? 32 : 38
                            radius: 8
                            x: -width - 40
                            y: baseY + 4
                            opacity: 0
                            scale: 0.68
                            rotation: itemIndex % 2 === 0 ? -5 : 5
                            z: x < midX - width / 2 ? 2 : 5
                            border.width: 1
                            border.color: Qt.rgba(
                                installProgressDialog.loaderHighlight.r,
                                installProgressDialog.loaderHighlight.g,
                                installProgressDialog.loaderHighlight.b,
                                modManager.theme === "Glassmorphic" ? 0.24 : 0.18
                            )
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop {
                                    position: 0.0
                                    color: Qt.rgba(
                                               installProgressDialog.loaderAccentStart.r,
                                               installProgressDialog.loaderAccentStart.g,
                                               installProgressDialog.loaderAccentStart.b,
                                               0.22 + (itemIndex * 0.035)
                                           )
                                }
                                GradientStop {
                                    position: 1.0
                                    color: Qt.rgba(
                                               installProgressDialog.loaderAccentEnd.r,
                                               installProgressDialog.loaderAccentEnd.g,
                                               installProgressDialog.loaderAccentEnd.b,
                                               0.14 + (itemIndex * 0.028)
                                           )
                                }
                            }

                            Rectangle {
                                x: 6
                                y: 7
                                width: parent.width * 0.58
                                height: 3
                                radius: 1.5
                                color: Qt.rgba(1, 1, 1, 0.84)
                            }

                            Rectangle {
                                x: 6
                                y: 13
                                width: parent.width * 0.36
                                height: 3
                                radius: 1.5
                                color: Qt.rgba(1, 1, 1, 0.6)
                            }

                            SequentialAnimation on x {
                                running: installProgressDialog.visible
                                loops: Animation.Infinite
                                PauseAnimation { duration: itemIndex * 175 }
                                NumberAnimation {
                                    from: -width - 40
                                    to: installLoaderStage.width * 0.39
                                    duration: 540
                                    easing.type: Easing.OutQuad
                                }
                                NumberAnimation {
                                    to: installLoaderStage.width * 0.58
                                    duration: 340
                                    easing.type: Easing.InOutQuad
                                }
                                NumberAnimation {
                                    to: installLoaderStage.width + 48
                                    duration: 500
                                    easing.type: Easing.InQuad
                                }
                            }

                            SequentialAnimation on y {
                                running: installProgressDialog.visible
                                loops: Animation.Infinite
                                PauseAnimation { duration: itemIndex * 175 }
                                NumberAnimation {
                                    from: baseY + 5
                                    to: baseY - 4
                                    duration: 540
                                    easing.type: Easing.OutQuad
                                }
                                NumberAnimation {
                                    from: baseY - 4
                                    to: baseY + (itemIndex % 2 === 0 ? -1 : 1)
                                    duration: 340
                                    easing.type: Easing.InOutQuad
                                }
                                NumberAnimation {
                                    from: baseY + (itemIndex % 2 === 0 ? -1 : 1)
                                    to: baseY + 4
                                    duration: 500
                                    easing.type: Easing.InQuad
                                }
                            }

                            SequentialAnimation on scale {
                                running: installProgressDialog.visible
                                loops: Animation.Infinite
                                PauseAnimation { duration: itemIndex * 175 }
                                NumberAnimation { from: 0.68; to: 0.9; duration: 540; easing.type: Easing.OutQuad }
                                NumberAnimation { from: 0.9; to: 0.98; duration: 340; easing.type: Easing.InOutQuad }
                                NumberAnimation { from: 0.98; to: 0.52; duration: 500; easing.type: Easing.InQuad }
                            }

                            SequentialAnimation on rotation {
                                running: installProgressDialog.visible
                                loops: Animation.Infinite
                                PauseAnimation { duration: itemIndex * 175 }
                                NumberAnimation {
                                    from: itemIndex % 2 === 0 ? -5 : 5
                                    to: itemIndex % 2 === 0 ? -1 : 1
                                    duration: 540
                                    easing.type: Easing.OutQuad
                                }
                                NumberAnimation {
                                    to: itemIndex % 2 === 0 ? 2 : -2
                                    duration: 340
                                    easing.type: Easing.InOutQuad
                                }
                                NumberAnimation {
                                    to: itemIndex % 2 === 0 ? 5 : -5
                                    duration: 500
                                    easing.type: Easing.InQuad
                                }
                            }

                            SequentialAnimation on opacity {
                                running: installProgressDialog.visible
                                loops: Animation.Infinite
                                PauseAnimation { duration: itemIndex * 175 }
                                NumberAnimation { from: 0.0; to: 0.58; duration: 150; easing.type: Easing.OutQuad }
                                PauseAnimation { duration: 910 }
                                NumberAnimation { from: 0.58; to: 0.0; duration: 320; easing.type: Easing.InQuad }
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: archiveToolDialog
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        anchors.centerIn: Overlay.overlay
        width: 540
        padding: 0

        background: Rectangle {
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.15, 0.15, 0.15, 0.98) : window.surfaceColor
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25) : window.borderColor

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.6)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.8)

            Behavior on opacity {
                NumberAnimation { duration: 250 }
            }
        }

        enter: Transition {
            NumberAnimation {
                property: "scale"
                from: 0.9
                to: 1.0
                duration: 250
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "scale"
                from: 1.0
                to: 0.95
                duration: 200
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }

        contentItem: Item {
            implicitWidth: 480
            implicitHeight: 360

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 32
                spacing: 20

                Item { Layout.preferredHeight: 8 }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 80
                    height: 80
                    radius: 40
                    color: "transparent"

                    Image {
                        id: archiveAlertIcon
                        anchors.centerIn: parent
                        width: 56
                        height: 56
                        source: modManager.theme === "Glassmorphic" ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/alert.png" :
                                modManager.theme === "Crimson" ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/alert.png" :
                                modManager.theme === "Wavey" ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/alert.png" :
                                "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/alert.png"
                        fillMode: Image.PreserveAspectFit
                        sourceSize.width: 128
                        sourceSize.height: 128
                        smooth: true
                        antialiasing: true
                        mipmap: true
                        visible: modManager.theme !== "Glassmorphic"

                        layer.enabled: false
                    }

                    ColorOverlay {
                        anchors.centerIn: parent
                        width: archiveAlertIcon.width
                        height: archiveAlertIcon.height
                        source: archiveAlertIcon
                        color: window.primaryColor
                        visible: modManager.theme === "Glassmorphic"
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: "Couldn't install mod(s)"
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Hey you! go to Settings and set 7-Zip or WinRAR in the Tools section before installing archive mods."
                        font.pixelSize: 15
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        lineHeight: 1.4
                    }
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 16

                    Button {
                        id: cancelToolButton
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 48
                        text: "Got it"
                        flat: true
                        hoverEnabled: true

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 15
                            font.weight: Font.Medium
                            color: cancelToolButton.hovered ? window.secondaryButtonTextHoverColor : window.secondaryButtonTextColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        background: Rectangle {
                            color: cancelToolButton.hovered ? window.secondaryButtonHoverColor : window.secondaryButtonColor
                            radius: 26
                            border.width: 1
                            border.color: window.secondaryButtonBorderColor

                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: archiveToolDialog.close()
                    }

                    Button {
                        id: showMeButton
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 48
                        flat: true
                        hoverEnabled: true

                        contentItem: Text {
                            text: "Show me"
                            font.pixelSize: 15
                            font.weight: Font.Bold
                            color: showMeButton.hovered ? "#FFFFFF" : window.textColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        background: Rectangle {
                            color: showMeButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                            radius: 24
                            border.width: 1.5
                            border.color: showMeButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            archiveToolDialog.close()
                            openArchiveToolSettings()
                        }
                    }
                }

                Item { Layout.preferredHeight: 8 }
            }
        }
    }

    Dialog {
        id: welcomeDialog
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        width: 480
        padding: 0

        Component.onCompleted: {

            if (modManager && !modManager.welcomeShown && modManager.gamePresets && modManager.gamePresets.length === 0) {
                welcomeDialog.open()
            }
        }

        onClosed: {

            if (modManager && !modManager.welcomeShown) {
                modManager.welcomeShown = true
            }
        }

        background: Rectangle {
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.15, 0.15, 0.15, 0.98) : window.surfaceColor
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25) : window.borderColor

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.7)

            Behavior on opacity {
                NumberAnimation { duration: 250 }
            }
        }

        enter: Transition {
            NumberAnimation {
                property: "scale"
                from: 0.9
                to: 1.0
                duration: 300
                easing.type: Easing.OutBack
            }
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 250
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "scale"
                from: 1.0
                to: 0.95
                duration: 200
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 140
                color: "transparent"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 16

                    Image {
                        source: "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/hi.png"
                        width: 64
                        height: 64
                        sourceSize: Qt.size(64, 64)
                        fillMode: Image.PreserveAspectFit
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "𝚆𝚎𝚕𝚌𝚘𝚖𝚎 𝚝𝚘 𝚢𝚘𝚞𝚛 𝙼𝚘𝚍 𝙼𝚊𝚗𝚊𝚐𝚎𝚛!"
                        font.pixelSize: 25
                        font.weight: Font.Bold
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                        Layout.alignment: Qt.AlignHCenter
                        font.family: "Segoe UI Variable Display"
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                spacing: 20

                Text {
                    text: "𝚄𝚗𝚒𝚟𝚎𝚛𝚜𝚊𝚕 𝙼𝚊𝚗𝚊𝚐𝚎𝚛 — Powerful enough to be unfair."
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    lineHeight: 1.2
                }

                Text {
                    text: "**Built** so you spend less time configuring. Your time is precious."
                    font.pixelSize: 14
                    color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    lineHeight: 1.4
                    textFormat: Text.MarkdownText
                }
            }

            Item { Layout.preferredHeight: 28 }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.1) : window.borderColor
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 12

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        Layout.preferredWidth: getStartedText.width + 48
                        Layout.preferredHeight: 44
                        radius: 12
                        color: getStartedMouse.containsMouse ? Qt.darker(window.primaryColor, 1.1) : window.primaryColor

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: getStartedText
                            anchors.centerIn: parent
                            text: "Get Started"
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            id: getStartedMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {

                                modManager.welcomeShown = true
                                welcomeDialog.close()

                                pageStack.currentIndex = 2
                                settingsPage.settingsTabIndex = 1
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: modManager

        function onInstallProgress(message) {
            installProgressText.text = message
        }

        function onInstallComplete(success, message) {
            installProgressText.text = success ? "Finalizing installation..." : "Wrapping up installation..."
            var elapsed = Date.now() - installProgressDialog.openedAtMs
            installFinishTimer.interval = Math.max(
                installProgressDialog.finishHoldMs,
                installProgressDialog.minimumVisibleMs - elapsed
            )
            installFinishTimer.restart()
        }

        function onArchiveToolConfigurationRequired() {
            installFinishTimer.stop()
            installProgressDialog.close()
            archiveToolDialog.open()
        }

        function onModsDetectedForRename(modGroups) {

            installFinishTimer.stop()
            installProgressDialog.close()

            installationDialog.modGroups = modGroups

            installationDialog.open()
        }
    }

    component TopNavButton: Item {
        id: topNavRoot
        property url iconSource
        property string emojiText: ""
        property string label
        property bool isActive: false
        signal clicked()
        property real hoverProgress: 0.0

        implicitWidth: 56
        implicitHeight: 48

        Behavior on hoverProgress {
            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        Item {
            id: visualContent
            width: parent.width
            height: parent.height
            y: -6 * topNavRoot.hoverProgress

            Rectangle {
                id: buttonBackground
                anchors.fill: parent
                anchors.margins: 0
                radius: height / 2
                color: "transparent"

                property real glowStrength: topNavRoot.isActive ? 1.0 : topNavRoot.hoverProgress

                layer.enabled: false

                Behavior on color { ColorAnimation { duration: 150 } }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottomMargin: 6
                    width: topNavRoot.isActive ? 22 : (topNavRoot.hoverProgress > 0 ? 12 : 0)
                    height: 3
                    radius: 1.5
                    color: window.accentColor
                    opacity: topNavRoot.isActive ? 1.0 : (topNavRoot.hoverProgress > 0 ? 0.6 : 0.0)

                    Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 180 } }
                }

                Image {
                    id: navIcon
                    anchors.centerIn: parent
                    width: modManager.theme === "Wavey" ? 32 : 26
                    height: modManager.theme === "Wavey" ? 32 : 26
                    source: topNavRoot.iconSource
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    antialiasing: true
                    mipmap: true
                    cache: false
                    sourceSize.width: 64
                    sourceSize.height: 64
                    visible: topNavRoot.emojiText === "" && topNavRoot.iconSource.toString() !== ""
                }

                ColorOverlay {
                    anchors.centerIn: parent
                    width: navIcon.width
                    height: navIcon.height
                    source: navIcon
                    visible: topNavRoot.emojiText === "" && topNavRoot.iconSource.toString() !== "" && (modManager.theme === "Glassmorphic")
                    color: modManager.theme === "Glassmorphic" ? window.navBarIconColor : modManager.accentColor
                    opacity: (modManager.theme === "Glassmorphic" ? 1.0 : 0.0) * (topNavRoot.isActive ? 1.0 : (0.72 + 0.28 * topNavRoot.hoverProgress))
                    scale: (topNavRoot.pressed ? 0.9 : 1.0) * (1.0 + 0.12 * topNavRoot.hoverProgress)

                    Behavior on opacity { NumberAnimation { duration: 300 } }
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                }

                Text {
                    id: navEmoji
                    anchors.centerIn: parent
                    text: topNavRoot.emojiText
                    font.pixelSize: modManager.theme === "Wavey" ? 24 : 20
                    visible: topNavRoot.emojiText !== ""
                    scale: (topNavRoot.pressed ? 0.9 : 1.0) * (1.0 + 0.12 * topNavRoot.hoverProgress)

                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                }
            }

            Rectangle {
                id: navTooltip
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.top
                anchors.bottomMargin: 6
                implicitWidth: tooltipText.implicitWidth + 16
                implicitHeight: tooltipText.implicitHeight + 10
                radius: 999
                color: modManager.theme === "Glassmorphic"
                    ? Qt.rgba(0.12, 0.12, 0.12, 0.98)
                    : window.surfaceColor
                border.width: 1
                border.color: window.borderColor
                visible: tooltipTimer.showTooltip && !topNavRoot.isActive
                opacity: tooltipTimer.showTooltip ? topNavRoot.hoverProgress : 0
                z: 100

                layer.enabled: visible
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(0, 0, 0, 0.3)
                    shadowBlur: 0.5
                    shadowVerticalOffset: 4
                }

                Timer {
                    id: tooltipTimer
                    property bool showTooltip: false
                    interval: 400
                    onTriggered: showTooltip = true
                }

                Connections {
                    target: mouseArea
                    function onContainsMouseChanged() {
                        if (mouseArea.containsMouse) {
                            tooltipTimer.start()
                        } else {
                            tooltipTimer.stop()
                            tooltipTimer.showTooltip = false
                        }
                    }
                }

                Behavior on opacity { NumberAnimation { duration: 160 } }
                Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }

                scale: 0.9 + 0.1 * topNavRoot.hoverProgress

                Text {
                    id: tooltipText
                    anchors.centerIn: parent
                    text: topNavRoot.label
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    color: window.textColor
                }
            }
        }

        property bool pressed: mouseArea.pressed

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onEntered: topNavRoot.hoverProgress = 1.0
            onExited: topNavRoot.hoverProgress = 0.0
            onClicked: topNavRoot.clicked()
        }
    }

    MouseArea {
        anchors.fill: parent
        visible: window.animationsPaused
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        z: 9999
        onPressed: window.requestActivate()
    }

    DownloadsPathRequiredDialog {
        id: downloadsPathRequiredDialog
        appWindow: window
        onShowSettingsRequested: window.openDownloadsPathSettings()
    }

    AppUpdateDialog {
        id: appUpdateDialog
        appWindow: window
    }

    Connections {
        target: updateChecker
        function onUpdateAvailable() {
            appUpdateDialog.begin()
        }
    }

    LuaProfilesRestoredDialog {
        id: luaProfilesRestoredDialog
        appWindow: window
    }

    Connections {
        target: modManager
        function onLuaProfileFoldersCreated(profiles) {
            luaProfilesRestoredDialog.showProfiles(profiles)
        }
    }

    NexusSignInDialog {
        id: nexusSignInDialog
        appWindow: window
        webProfile: window.nexusWebSignInProfile
    }

    NexusDownloadWindow {
        id: nexusDownloadWindow
        appWindow: window
        webProfile: window.nexusWebSignInProfile
    }
}
