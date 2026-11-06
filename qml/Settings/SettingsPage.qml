import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Effects
import Qt5Compat.GraphicalEffects
import QtMultimedia

Pane {
    id: settingsPage
    padding: 0
    background: Rectangle { color: "transparent" }

    Gradient {
        id: settingsDialogCozyGradient
        GradientStop { position: 0.0; color: window.cozyDialogGradientTop }
        GradientStop { position: 1.0; color: window.cozyDialogGradientBottom }
        orientation: Gradient.Vertical
    }
    Gradient {
        id: settingsDialogCrimsonGradient
        GradientStop { position: 0.0; color: window.crimsonDialogGradientTop }
        GradientStop { position: 1.0; color: window.crimsonDialogGradientBottom }
        orientation: Gradient.Vertical
    }
    Gradient {
        id: settingsDialogWaveyGradient
        GradientStop { position: 0.0; color: window.waveyDialogGradientTop }
        GradientStop { position: 1.0; color: window.waveyDialogGradientBottom }
        orientation: Gradient.Vertical
    }

    property int generalSectionIndex: 0
    property real archiveToolHighlightProgress: 0.0
    property bool showAdvancedUiColors: false
    property bool showUiAccentColors: false
    property bool showPerformanceBarColors: false
    property int advancedUiSegmentIndex: 0
    property int launchExpandedIndex: -1
    property alias settingsTabIndex: settingsTabBar.currentIndex

    property real navSidebarSceneX: 0
    readonly property real navSidebarWidth: settingsNavSidebar ? settingsNavSidebar.width : 230

    function syncNavSidebarGeometry() {
        if (settingsNavSidebar)
            navSidebarSceneX = settingsNavSidebar.mapToItem(null, 0, 0).x
    }

    onWidthChanged: Qt.callLater(syncNavSidebarGeometry)
    onHeightChanged: Qt.callLater(syncNavSidebarGeometry)
    onVisibleChanged: if (visible) Qt.callLater(syncNavSidebarGeometry)

    function scrollToCard(card) {
        if (!settingsScrollView || !settingsScrollView.contentItem || !card) {
            return;
        }

        Qt.callLater(function() {
            var flickable = settingsScrollView.contentItem;
            var cardY = card.mapToItem(flickable.contentItem, 0, 0).y;
            var cardHeight = card.height;
            var viewportHeight = flickable.height;

            var targetY = cardY + cardHeight - viewportHeight + 60;

            var maxY = flickable.contentHeight - flickable.height;
            targetY = Math.max(0, Math.min(targetY, maxY));

            scrollAnimation.to = targetY;
            scrollAnimation.start();
        });
    }

    NumberAnimation {
        id: scrollAnimation
        target: settingsScrollView.contentItem
        property: "contentY"
        duration: 300
        easing.type: Easing.OutCubic
    }

    Timer {
        id: scrollTimer
        interval: 220
        property var cardToScroll: null
        onTriggered: {
            if (cardToScroll) {
                settingsPage.scrollToCard(cardToScroll);
                cardToScroll = null;
            }
        }
    }

    SettingsPageDialogs { id: settingsDialogs }

    function collapseExpandedCards() {
        launchExpandedIndex = -1
        appOptionsCard.appExpandedIndex = -1
    }

    function highlightArchiveToolCard() {
        archiveToolHighlightDelay.restart()
    }

    function focusNexusMods() {
        settingsTabBar.currentIndex = 0
        generalSectionIndex = 6
        nexusModsHighlightDelay.restart()
    }

    function focusDownloadPaths() {
        settingsTabBar.currentIndex = 0
        generalSectionIndex = 1
    }

    Timer {
        id: nexusModsHighlightDelay
        interval: 220
        repeat: false
        onTriggered: {
            if (settingsScrollView && settingsScrollView.contentItem) {
                var flick = settingsScrollView.contentItem
                flick.contentY = Math.max(0, flick.contentHeight - flick.height)
            }
            nexusModsSection.highlightApiKey()
        }
    }

    onGeneralSectionIndexChanged: {
        collapseExpandedCards()
        if (settingsScrollView && settingsScrollView.contentItem) {
            settingsScrollView.contentItem.contentY = 0
        }
    }

    Timer {
        id: archiveToolHighlightDelay
        interval: 180
        repeat: false
        onTriggered: {
            archiveToolHighlightAnimation.stop()
            settingsPage.archiveToolHighlightProgress = 0.0
            archiveToolHighlightAnimation.start()
        }
    }

    SequentialAnimation {
        id: archiveToolHighlightAnimation
        running: false
        loops: 1

        NumberAnimation {
            target: settingsPage
            property: "archiveToolHighlightProgress"
            from: 0.0
            to: 1.0
            duration: 260
            easing.type: Easing.OutCubic
        }

        PauseAnimation { duration: 70 }

        NumberAnimation {
            target: settingsPage
            property: "archiveToolHighlightProgress"
            from: 1.0
            to: 0.0
            duration: 420
            easing.type: Easing.InOutCubic
        }

        PauseAnimation { duration: 90 }

        onStopped: settingsPage.archiveToolHighlightProgress = 0.0
    }

    ThemeConfig {
        id: themeConfig
    }

    Connections {
        target: modManager
        function onModsPathChanged() {
            console.log("Mods path changed to:", modManager.modsPath)
            modManager.scanMods(modManager.modsPath)
        }
    }

    Component.onCompleted: {
        applyTheme(modManager.theme)
        Qt.callLater(syncNavSidebarGeometry)
        if (modManager.modsPath) {
            modManager.scanMods(modManager.modsPath)
        }
        if (modManager) {
            if (!modManager.pakViewerPath) {
                modManager.autoDetectPakViewerPath()
            }
            if (!modManager.conflictDetectorPath) {
                modManager.autoDetectConflictDetectorPath()
            }
            if (!modManager.ueDetectorPath) {
                modManager.autoDetectUEDetectorPath()
            }
        }
    }

    Connections {
        target: modManager
        function onThemeChanged() {
            applyTheme(modManager.theme)
        }
        function onScanFinished(success) {
            scanningPopup.close()
            if (success) {
                scanSuccessPopup.open()
            } else {
                scanFailedPopup.open()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 20

        Column {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.topMargin: 20
            spacing: 6

            Text {
                text: {
                    switch (settingsTabBar.currentIndex) {
                    case 1:  return "Games Management"
                    case 2:  return "Advanced Conflicts"
                    case 3:  return "Runtime Generation & Scripting"
                    default: return "Settings"
                    }
                }
                font.pixelSize: 22
                font.weight: Font.DemiBold
                color: modManager.theme === "Wavey" ? themeConfig.wavey.settingsTitleColor : window.textColor
                font.family: "Segoe UI Variable Display"
            }

            Text {
                text: {
                    switch (settingsTabBar.currentIndex) {
                    case 1:  return "Manage your games into organized profiles"
                    case 2:  return "Inspect and manage conflicts"
                    case 3:  return "Inject Avalonium to bring what's impossible to life"
                    default: return "Configure your Universal Manager preferences"
                    }
                }
                font.pixelSize: 13
                color: modManager.theme === "Wavey" ? themeConfig.wavey.settingsSubtitleColor : window.textSecondaryColor
                opacity: 0.85
            }
        }

        Dialog {
            id: presetNameDialog
            modal: true
            closePolicy: Popup.NoAutoClose
            anchors.centerIn: Overlay.overlay
            width: 420
            padding: 22

            property string mode: "create"

            background: Rectangle {
                radius: 16
                border.width: 1
                border.color: modManager.theme === "Glassmorphic"
                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.20)
                    : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
                gradient: modManager.theme === "Cozy" ? settingsDialogCozyGradient
                        : modManager.theme === "Crimson" ? settingsDialogCrimsonGradient
                        : modManager.theme === "Wavey" ? settingsDialogWaveyGradient
                        : null

                color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"
            }

            contentItem: ColumnLayout {
                spacing: 16

                Text {
                    text: presetNameDialog.mode === "create" ? "Create Preset" : "Rename Preset"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: window.textColor
                }

                TextField {
                    id: presetNameField
                    Layout.fillWidth: true
                    placeholderText: "Preset name"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Cancel"
                        onClicked: presetNameDialog.close()
                    }

                    Button {
                        text: presetNameDialog.mode === "create" ? "Create" : "Rename"

            palette {
                windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            }
                        onClicked: {
                            if (presetNameField.text.trim() === "") return
                            if (presetNameDialog.mode === "create") {
                                modManager.createPreset(presetNameField.text.trim())
                            } else {
                                modManager.renamePreset(modManager.activePresetId, presetNameField.text.trim())
                            }
                            presetNameDialog.close()
                        }
                    }
                }
            }
        }

        Dialog {
            id: deletePresetDialog
            modal: true
            closePolicy: Popup.NoAutoClose
            anchors.centerIn: Overlay.overlay
            width: 420
            padding: 22

            background: Rectangle {
                radius: 16
                border.width: 1
                border.color: modManager.theme === "Glassmorphic"
                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.20)
                    : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
                gradient: modManager.theme === "Cozy" ? settingsDialogCozyGradient
                        : modManager.theme === "Crimson" ? settingsDialogCrimsonGradient
                        : modManager.theme === "Wavey" ? settingsDialogWaveyGradient
                        : null

                color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"
            }

            contentItem: ColumnLayout {
                spacing: 16

                Text {
                    text: "Delete preset?"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: window.textColor
                }

                Text {
                    text: "This will delete the current preset. You cannot delete the last preset."
                    wrapMode: Text.WordWrap
                    color: window.textSecondaryColor
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Cancel"
                        onClicked: deletePresetDialog.close()
                    }

                    Button {
                        text: "Delete"

            palette {
                windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            }
                        onClicked: {
                            modManager.deletePreset(modManager.activePresetId)
                            deletePresetDialog.close()
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 10 }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                spacing: 40

                Item {
                    Layout.preferredWidth: generalTabContent.width
                    Layout.preferredHeight: 56

                    ColumnLayout {
                        id: generalTabContent
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        spacing: 0

                        RowLayout {
                            Layout.fillHeight: true
                            spacing: 10

                            Item {
                                id: generalIconBg
                                width: modManager.theme === "Cozy" ? 48 : 44
                                height: modManager.theme === "Cozy" ? 48 : 44
                                y: generalMouseArea.containsMouse ? -1 : 0

                                Behavior on y { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

                                Image {
                                    anchors.centerIn: parent
                                    width: modManager.theme === "Cozy" ? 34 : 30
                                    height: modManager.theme === "Cozy" ? 34 : 30
                                    source: {
                                        if (modManager.theme === "Wavey") {
                                            return "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/General_header.png"
                                        } else if (modManager.theme === "Glassmorphic") {
                                            return "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/General_header.png"
                                        } else if (modManager.theme === "Crimson") {
                                            return "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/General_header.png"
                                        } else if (modManager.theme === "Cozy") {
                                            return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/General_header.png"
                                        } else {
                                            return "qrc:/UniversalModManager/Lucide Icons/settings.svg"
                                        }
                                    }
                                    sourceSize: Qt.size(width * 2, height * 2)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    mipmap: true
                                    antialiasing: true
                                    cache: false
                                    opacity: settingsTabBar.currentIndex === 0 ? 1.0 : 0.6

                                    layer.enabled: modManager.theme === "Glassmorphic" && settingsTabBar.currentIndex === 0
                                    layer.effect: ColorOverlay {
                                        color: window.accentColor
                                    }

                                    Behavior on opacity {
                                        NumberAnimation { duration: 200 }
                                    }
                                }
                            }

                            Text {
                                id: generalTabText
                                text: "General"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: {
                                    if (settingsTabBar.currentIndex === 0) {
                                        return modManager.theme === "Cozy" ? "#000000" : window.accentColor
                                    }
                                    return window.textColor
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: generalTabContent.width
                            Layout.preferredHeight: 3
                            color: settingsTabBar.currentIndex === 0 ? window.accentColor : "transparent"
                            radius: 1.5

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }
                    }

                    MouseArea {
                        id: generalMouseArea
                        enabled: !modManager.generating
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsTabBar.currentIndex = 0
                    }
                }

                Item {
                    Layout.preferredWidth: gamesTabContent.width
                    Layout.preferredHeight: 56

                    ColumnLayout {
                        id: gamesTabContent
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        spacing: 0

                        RowLayout {
                            Layout.fillHeight: true
                            spacing: 10

                            Item {
                                id: gamesIconBg
                                width: modManager.theme === "Cozy" ? 48 : 44
                                height: modManager.theme === "Cozy" ? 48 : 44
                                y: gamesMouseArea.containsMouse ? -1 : 0

                                Behavior on y { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

                                Image {
                                    anchors.centerIn: parent
                                    width: modManager.theme === "Cozy" ? 34 : 30
                                    height: modManager.theme === "Cozy" ? 34 : 30
                                    source: {
                                        if (modManager.theme === "Wavey") {
                                            return "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/controller.png"
                                        } else if (modManager.theme === "Glassmorphic") {
                                            return "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Controller_header.png"
                                        } else if (modManager.theme === "Crimson") {
                                            return "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Controller_header.png"
                                        } else if (modManager.theme === "Cozy") {
                                            return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Controller_header.png"
                                        } else {
                                            return "qrc:/UniversalModManager/Lucide Icons/gamepad-2.svg"
                                        }
                                    }
                                    sourceSize: Qt.size(width * 2, height * 2)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    mipmap: true
                                    antialiasing: true
                                    cache: false
                                    opacity: settingsTabBar.currentIndex === 1 ? 1.0 : 0.6

                                    layer.enabled: modManager.theme === "Glassmorphic" && settingsTabBar.currentIndex === 1
                                    layer.effect: ColorOverlay {
                                        color: window.accentColor
                                    }

                                    Behavior on opacity {
                                        NumberAnimation { duration: 200 }
                                    }
                                }
                            }

                            Text {
                                id: gamesTabText
                                text: "Games Manager"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: {
                                    if (settingsTabBar.currentIndex === 1) {
                                        return modManager.theme === "Cozy" ? "#000000" : window.accentColor
                                    }
                                    return window.textColor
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: gamesTabContent.width
                            Layout.preferredHeight: 3
                            color: settingsTabBar.currentIndex === 1 ? window.accentColor : "transparent"
                            radius: 1.5

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }
                    }

                    MouseArea {
                        id: gamesMouseArea
                        enabled: !modManager.generating
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsTabBar.currentIndex = 1
                    }
                }

                Item {
                    Layout.preferredWidth: conflictsTabContent.width
                    Layout.preferredHeight: 56

                    ColumnLayout {
                        id: conflictsTabContent
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        spacing: 0

                        RowLayout {
                            Layout.fillHeight: true
                            spacing: 10

                            Item {
                                id: conflictsIconBg
                                width: modManager.theme === "Cozy" ? 48 : 44
                                height: modManager.theme === "Cozy" ? 48 : 44
                                y: conflictsMouseArea.containsMouse ? -1 : 0

                                Behavior on y { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

                                Image {
                                    anchors.centerIn: parent
                                    width: modManager.theme === "Cozy" ? 45 : 30
                                    height: modManager.theme === "Cozy" ? 45 : 30
                                    source: {
                                        if (modManager.theme === "Wavey") {
                                            return "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Detector.png"
                                        } else if (modManager.theme === "Glassmorphic") {
                                            return "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Detect_header.png"
                                        } else if (modManager.theme === "Crimson") {
                                            return "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Detect_header.png"
                                        } else if (modManager.theme === "Cozy") {
                                            return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Detect_heder.png"
                                        } else {
                                            return "qrc:/UniversalModManager/Lucide Icons/scan-search.svg"
                                        }
                                    }
                                    sourceSize: Qt.size(width * 2, height * 2)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    mipmap: true
                                    antialiasing: true
                                    cache: false
                                    opacity: settingsTabBar.currentIndex === 2 ? 1.0 : 0.6

                                    layer.enabled: modManager.theme === "Glassmorphic" && settingsTabBar.currentIndex === 2
                                    layer.effect: ColorOverlay {
                                        color: window.accentColor
                                    }

                                    Behavior on opacity {
                                        NumberAnimation { duration: 200 }
                                    }
                                }
                            }

                            Text {
                                id: conflictsTabText
                                text: "Advanced Conflicts Detector"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: {
                                    if (settingsTabBar.currentIndex === 2) {
                                        return modManager.theme === "Cozy" ? "#000000" : window.accentColor
                                    }
                                    return window.textColor
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: conflictsTabContent.width
                            Layout.preferredHeight: 3
                            color: settingsTabBar.currentIndex === 2 ? window.accentColor : "transparent"
                            radius: 1.5

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }
                    }

                    MouseArea {
                        id: conflictsMouseArea
                        enabled: !modManager.generating
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsTabBar.currentIndex = 2
                    }
                }

                Item {
                    Layout.preferredWidth: runtimeTabContent.width
                    Layout.preferredHeight: 56

                    ColumnLayout {
                        id: runtimeTabContent
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        spacing: 0

                        RowLayout {
                            Layout.fillHeight: true
                            spacing: 10

                            Item {
                                id: runtimeIconBg
                                width: modManager.theme === "Cozy" ? 48 : 44
                                height: modManager.theme === "Cozy" ? 48 : 44
                                y: runtimeMouseArea.containsMouse ? -1 : 0

                                Behavior on y { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

                                Image {
                                    anchors.centerIn: parent
                                    width: modManager.theme === "Cozy" ? 34 : 30
                                    height: modManager.theme === "Cozy" ? 34 : 30
                                    source: {
                                        if (modManager.theme === "Wavey") {
                                            return "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Unreal Engine.png"
                                        } else if (modManager.theme === "Glassmorphic") {
                                            return "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Unreal Engine.png"
                                        } else if (modManager.theme === "Crimson") {
                                            return "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Unreal Engine.png"
                                        } else if (modManager.theme === "Cozy") {
                                            return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Unreal Engine.png"
                                        } else {
                                            return "qrc:/UniversalModManager/Lucide Icons/rocket.svg"
                                        }
                                    }
                                    sourceSize: Qt.size(width * 2, height * 2)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    mipmap: true
                                    antialiasing: true
                                    cache: false
                                    opacity: settingsTabBar.currentIndex === 3 ? 1.0 : 0.6

                                    layer.enabled: modManager.theme === "Glassmorphic" && settingsTabBar.currentIndex === 3
                                    layer.effect: ColorOverlay {
                                        color: window.accentColor
                                    }

                                    Behavior on opacity {
                                        NumberAnimation { duration: 200 }
                                    }
                                }
                            }

                            Text {
                                id: runtimeTabText
                                text: "Avalonium Runtime Engine"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: {
                                    if (settingsTabBar.currentIndex === 3) {
                                        return modManager.theme === "Cozy" ? "#000000" : window.accentColor
                                    }
                                    return window.textColor
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: runtimeTabContent.width
                            Layout.preferredHeight: 3
                            color: settingsTabBar.currentIndex === 3 ? window.accentColor : "transparent"
                            radius: 1.5

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }
                    }

                    MouseArea {
                        id: runtimeMouseArea
                        enabled: !modManager.generating
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsTabBar.currentIndex = 3
                    }
                }

                Item { Layout.fillWidth: true }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: window.borderColor
            }
        }

        TabBar {
            id: settingsTabBar
            visible: false
            background: Rectangle { color: "transparent" }
            TabButton { }
            TabButton { }
            TabButton { }
            TabButton { }

            onCurrentIndexChanged: {
                if (currentIndex === 1 && !modManager.gamesManagerWelcomeShown) {
                    gamesManagerWelcomeDialog.open()
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: settingsTabBar.currentIndex

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                RowLayout {
                    anchors.fill: parent
                    spacing: 24

                    Rectangle {
                        id: settingsNavSidebar
                        Layout.preferredWidth: 230
                        Layout.fillHeight: true
                        radius: window.cardRadius
                        color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
                        border.width: window.borderWidth
                        border.color: window.borderColor

                        gradient: modManager.theme === "Crimson" ? crimsonGradient : null

                        Gradient {
                            id: crimsonGradient
                            GradientStop { position: 0.0; color: window.crimsonGradientTop }
                            GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            SettingsSectionPill {
                                Layout.fillWidth: true
                                sectionIndex: 0
                                title: "Appearance"
                                icon: ""
                                currentIndex: generalSectionIndex
                                onClicked: generalSectionIndex = sectionIndex
                            }

                            SettingsSectionPill {
                                Layout.fillWidth: true
                                sectionIndex: 1
                                title: "Configuration Paths"
                                icon: ""
                                currentIndex: generalSectionIndex
                                onClicked: generalSectionIndex = sectionIndex
                            }

                            SettingsSectionPill {
                                Layout.fillWidth: true
                                sectionIndex: 2
                                title: "Advanced Tools"
                                icon: ""
                                currentIndex: generalSectionIndex
                                onClicked: generalSectionIndex = sectionIndex
                            }

                            SettingsSectionPill {
                                Layout.fillWidth: true
                                sectionIndex: 3
                                title: "Shortcuts & Deletion"
                                icon: ""
                                currentIndex: generalSectionIndex
                                onClicked: generalSectionIndex = sectionIndex
                            }

                            SettingsSectionPill {
                                Layout.fillWidth: true
                                sectionIndex: 4
                                title: "Additional Settings"
                                icon: ""
                                currentIndex: generalSectionIndex
                                onClicked: generalSectionIndex = sectionIndex
                            }

                            SettingsSectionPill {
                                Layout.fillWidth: true
                                sectionIndex: 5
                                title: "Downloads"
                                icon: ""
                                currentIndex: generalSectionIndex
                                onClicked: generalSectionIndex = sectionIndex
                            }

                            SettingsSectionPill {
                                Layout.fillWidth: true
                                sectionIndex: 6
                                title: "Nexus Mods"
                                icon: ""
                                currentIndex: generalSectionIndex
                                onClicked: generalSectionIndex = sectionIndex
                            }

                            SettingsSectionPill {
                                Layout.fillWidth: true
                                sectionIndex: 7
                                title: "Performance"
                                icon: ""
                                currentIndex: generalSectionIndex
                                onClicked: generalSectionIndex = sectionIndex
                            }

                            SettingsSectionPill {
                                Layout.fillWidth: true
                                sectionIndex: 8
                                title: "Guides"
                                icon: ""
                                currentIndex: generalSectionIndex
                                onClicked: generalSectionIndex = sectionIndex
                            }

                            SettingsSectionPill {
                                Layout.fillWidth: true
                                sectionIndex: 9
                                title: "About"
                                icon: ""
                                currentIndex: generalSectionIndex
                                onClicked: generalSectionIndex = sectionIndex
                            }

                            Item { Layout.fillHeight: true }
                        }
                    }

                    ScrollView {
                        id: settingsScrollView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        Component.onCompleted: {

                            if (contentItem) {
                                contentItem.boundsBehavior = Flickable.DragAndOvershootBounds
                                contentItem.flickDeceleration = 1500
                                contentItem.maximumFlickVelocity = 2500
                            }
                        }

                        ColumnLayout {
                            width: parent.parent.width

                            height: (generalSectionIndex === 3 || generalSectionIndex === 5 || generalSectionIndex === 6) ? settingsScrollView.height : implicitHeight
                            spacing: generalSectionIndex === 0 ? 40 : 16

            SettingsPageAppearance {
                id: appearanceSection
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                visible: generalSectionIndex === 0
            }

            SettingsPageTools {
                id: toolsSection
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                visible: generalSectionIndex === 1 || generalSectionIndex === 2
            }

            SettingsPageShortcuts {
                id: shortcutsSection
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                visible: generalSectionIndex === 3
            }

            SettingsPageModDeletion {
                id: modDeletionSection
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                visible: generalSectionIndex === 3
            }

            SettingsPageDownloads {
                id: downloadsSection
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                visible: generalSectionIndex === 5
            }

            SettingsPageNexusMods {
                id: nexusModsSection
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                visible: generalSectionIndex === 6
            }

            PerformanceTab {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                visible: generalSectionIndex === 7
            }

            SettingsPageAdvanced {
                id: advancedSection
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                visible: generalSectionIndex === 4
            }

            SettingsPageAbout {
                id: aboutSection
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                visible: generalSectionIndex === 9
            }

            Loader {
                Layout.fillWidth: true
                Layout.rightMargin: 10
                Layout.preferredHeight: item ? item.implicitHeight : 0
                active: generalSectionIndex === 8
                visible: active
                source: "qrc:/UniversalModManager/qml/Settings/Tutorials.qml"
            }

            }
        }
    }
}

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                MouseArea {
                    anchors.fill: parent
                    onClicked: forceActiveFocus()
                    z: -1
                }

                Loader {
                    id: gamePresetsLoader
                    anchors.fill: parent
                    source: "qrc:/UniversalModManager/qml/GamePresetsManager/GamePresetsManager.qml"
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                MouseArea {
                    anchors.fill: parent
                    onClicked: forceActiveFocus()
                    z: -1
                }

                Loader {
                    id: conflictDetectorLoader
                    anchors.fill: parent
                    source: "qrc:/UniversalModManager/qml/ConflictDetector/ConflictDetectorTab.qml"
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                MouseArea {
                    anchors.fill: parent
                    onClicked: forceActiveFocus()
                    z: -1
                }

                Loader {
                    id: unrealEngineRuntimeLoader
                    anchors.fill: parent
                    source: "qrc:/UniversalModManager/qml/AvaloniumEngine/UnrealEngineRuntimeTab.qml"
                }
            }
        }
    }

    component SettingsSectionPill: Rectangle {
        property int sectionIndex: 0
        property string title: "Section"
        property string icon: ""
        property int currentIndex: 0
        property bool hovered: false
        signal clicked()

        Layout.preferredHeight: 48
        Layout.fillWidth: true
        Layout.margins: { left: 10; right: 10; top: 0; bottom: 0 }
        color: (modManager.theme === "Crimson" && currentIndex !== sectionIndex) ? "transparent" :
               (currentIndex === sectionIndex
               ? window.primaryColor
               : (hovered ? Qt.lighter(window.surfaceColor, 1.1) : window.surfaceColor))
        radius: window.cardRadius
        border.width: window.borderWidth
        border.color: hovered && currentIndex !== sectionIndex ? window.primaryColor : window.borderColor
        x: hovered && currentIndex !== sectionIndex ? 4 : 0
        opacity: hovered && currentIndex !== sectionIndex ? 0.95 : 1.0

        gradient: (modManager.theme === "Crimson" && currentIndex !== sectionIndex) ? sidebarButtonGradient : null

        Gradient {
            id: sidebarButtonGradient
            GradientStop { position: 0.0; color: window.crimsonSidebarGradientTop }
            GradientStop { position: 1.0; color: window.crimsonSidebarGradientBottom }
        }

        Behavior on color { ColorAnimation { duration: 180 } }
        Behavior on border.color { ColorAnimation { duration: 180 } }
        Behavior on x { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 160 } }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 0
            spacing: 0

            Text {
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                text: title
                font.pixelSize: 15
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: currentIndex === sectionIndex ? "#FFFFFF" : window.textColor
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onEntered: {
                if (parent.currentIndex !== parent.sectionIndex) {
                    parent.hovered = true
                }
            }
            onExited: {
                if (parent.currentIndex !== parent.sectionIndex) {
                    parent.hovered = false
                }
            }
            onClicked: parent.clicked()
        }
    }

    component ThemeGridCard: Rectangle {
        property string themeName: "Theme"
        property string themeIcon: ""
        property color previewColor1: "#000000"
        property color previewColor2: "#666666"
        property bool isSelected: false
        property color hoverGlowColor: previewColor1
        signal clicked()

        implicitWidth: 260
        implicitHeight: 140
        radius: 18
        color: modManager.theme === "Glassmorphic" ?
                   (gridMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.20) : Qt.rgba(1, 1, 1, 0.14)) :
                   (gridMouse.containsMouse ? Qt.lighter(window.surfaceColor, 1.04) : window.surfaceColor)
        border.width: (isSelected || gridMouse.containsMouse) ? 3 : 2
        border.color: isSelected ? previewColor1 :
                      (gridMouse.containsMouse ? hoverGlowColor : window.borderColor)
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowHorizontalOffset: 0
            shadowVerticalOffset: (gridMouse.containsMouse || isSelected) ? 6 : 4
            shadowBlur: (gridMouse.containsMouse || isSelected) ? 0.7 : 0.45
            shadowColor: (gridMouse.containsMouse || isSelected)
                         ? Qt.rgba(hoverGlowColor.r, hoverGlowColor.g, hoverGlowColor.b,
                                   modManager.theme === "Glassmorphic" ? 0.9 : 0.7)
                         : Qt.rgba(0, 0, 0, 0.28)
        }

        Behavior on color { ColorAnimation { duration: 180 } }
        Behavior on border.color { ColorAnimation { duration: 200 } }
        Behavior on border.width { NumberAnimation { duration: 150 } }
        MouseArea {
            id: gridMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 68
                radius: 12
                color: modManager.theme === "Glassmorphic" ?
                           Qt.rgba(0, 0, 0, 0.45) :
                           Qt.darker(window.surfaceColor, 1.5)
                border.width: 1
                border.color: Qt.rgba(0, 0, 0, 0.4)

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Rectangle {
                        Layout.preferredWidth: 6
                        Layout.fillHeight: true
                        radius: 3
                        color: previewColor1
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 14
                            radius: 7
                            color: previewColor1
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 10
                            radius: 6
                            color: previewColor2
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 10
                            radius: 6
                            color: Qt.rgba(previewColor1.r, previewColor1.g, previewColor1.b, 0.65)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: themeName
                            font.pixelSize: 14
                            font.weight: 600
                            color: window.textColor
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    Text {
                        text: isSelected ? "Current theme" : ""
                        font.pixelSize: 11
                        color: window.textSecondaryColor
                        visible: isSelected
                    }
                }

            }
        }
    }

    component ThemePreviewButton: Rectangle {
        property string themeName: "Theme"
        property string themeIcon: ""
        property color previewColor1: "#000000"
        property color previewColor2: "#666666"
        property color previewColor3: "#CCCCCC"
        property bool isSelected: false
        signal clicked()

        implicitWidth: 140
        implicitHeight: 70
        color: isSelected ? Qt.lighter(previewColor1, 1.85) : window.surfaceColor
        radius: window.cardRadius
        border.width: isSelected ? 3 : 2
        border.color: isSelected ? previewColor1 : window.borderColor

        Behavior on color { ColorAnimation { duration: 200 } }
        Behavior on border.color { ColorAnimation { duration: 200 } }
        Behavior on scale { NumberAnimation { duration: 150 } }

        MouseArea {
            id: themeMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
            onPressed: parent.scale = 0.96
            onReleased: parent.scale = 1.0
            onExited: parent.scale = 1.0
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            Text {
                text: themeIcon
                font.pixelSize: 24
            }

            Column {
                Layout.fillWidth: true
                spacing: 4

                RowLayout {
                    spacing: 8

                    Text {
                        text: themeName
                        font.pixelSize: 14
                        font.weight: 600
                        color: window.textColor
                    }

                    Rectangle {
                        width: 16
                        height: 16
                        radius: 8
                        color: previewColor1
                        visible: isSelected
                        opacity: isSelected ? 1 : 0

                        Behavior on opacity { NumberAnimation { duration: 200 } }

                        Text {
                            anchors.centerIn: parent
                            text: ""
                            font.pixelSize: 10
                            font.weight: 700
                            color: "#FFFFFF"
                        }
                    }
                }

                Row {
                    spacing: 4

                    Repeater {
                        model: [previewColor1, previewColor2, previewColor3]
                        Rectangle {
                            width: 16
                            height: 16
                            radius: 4
                            color: modelData
                        }
                    }
                }
            }
        }
    }

    SettingsPageLegacyDialogs { id: legacyDialogs }

    SettingsPagePickers { id: settingsPickers }

    property string _pendingThemeSwitch: ""

    Timer {
        id: _themeSwitchTimer
        interval: 0
        repeat: false
        onTriggered: {
            if (_pendingThemeSwitch === "") return
            var theme = _pendingThemeSwitch
            _pendingThemeSwitch = ""
            var changed = modManager.theme !== theme
            if (changed)
                modManager.beginThemeChange()
            applyTheme(theme)
            modManager.setTheme(theme)
            if (changed)
                modManager.endThemeChange()
        }
    }

    function switchTheme(theme) {
        _pendingThemeSwitch = theme
        _themeSwitchTimer.restart()
    }

    function applyTheme(theme) {
        if (!themeConfig) return
        themeConfig.applyTheme(theme, window, modManager.accentColor)
    }

    readonly property alias gamesManagerWelcomeDialog: settingsDialogs.gamesManagerWelcomeDialog
    readonly property alias managerConsoleRestartDialog: settingsDialogs.managerConsoleRestartDialog
    readonly property alias pathsHelpDialog: settingsDialogs.pathsHelpDialog
    readonly property alias scanningPopup: settingsDialogs.scanningPopup
    readonly property alias scanSuccessPopup: settingsDialogs.scanSuccessPopup
    readonly property alias scanFailedPopup: settingsDialogs.scanFailedPopup
    readonly property alias aesKeyFieldNew: advancedSection.aesKeyFieldNew
    readonly property alias appOptionsCard: advancedSection.appOptionsCard
    readonly property alias scanTimer: settingsPickers.scanTimer
    readonly property alias gamePathDialog: settingsPickers.gamePathDialog
    readonly property alias modsPathDialog: settingsPickers.modsPathDialog
    readonly property alias downloadsPathDialog: settingsPickers.downloadsPathDialog
    readonly property alias backupPathDialog: settingsPickers.backupPathDialog
    readonly property alias sevenZipPathDialog: settingsPickers.sevenZipPathDialog
    readonly property alias pakViewerPathDialog: settingsPickers.pakViewerPathDialog
    readonly property alias conflictDetectorPathDialog: settingsPickers.conflictDetectorPathDialog
    readonly property alias ueDetectorPathDialog: settingsPickers.ueDetectorPathDialog
    readonly property alias gamePaksPathDialog: settingsPickers.gamePaksPathDialog
    readonly property alias usmapFileDialog: settingsPickers.usmapFileDialog
    readonly property alias wallpaperDialog: settingsPickers.wallpaperDialog
    readonly property alias colorPickerDialog: settingsPickers.colorPickerDialog
    readonly property alias navBarColorDialog: settingsPickers.navBarColorDialog
    readonly property alias cpuBarColorDialog: settingsPickers.cpuBarColorDialog
    readonly property alias ramBarColorDialog: settingsPickers.ramBarColorDialog
    readonly property alias driveBarColorDialog: settingsPickers.driveBarColorDialog
    readonly property alias navBarIconColorDialog: settingsPickers.navBarIconColorDialog

}
