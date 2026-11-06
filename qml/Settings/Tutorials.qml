import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects
import QtMultimedia

Item {
    id: tutorialsPage
    width: parent ? parent.width : 0
    implicitHeight: pageLayout.implicitHeight

    property bool isGlassmorphic: modManager && modManager.theme === "Glassmorphic"
    property real glassOpacity: modManager ? modManager.glassOpacity : 0.25
    property string appDirPath: typeof applicationDirPath === "string" ? applicationDirPath : ""
    property bool playerMuted: false

    function localAssetUrl(relativePath) {
        if (!relativePath || appDirPath === "")
            return ""

        var cleanBase = appDirPath.replace(/\\/g, "/")
        var cleanRelative = relativePath.replace(/\\/g, "/")
        return "file:///" + cleanBase + "/" + cleanRelative
    }

    function formatMs(milliseconds) {
        if (!milliseconds || milliseconds <= 0)
            return "00:00"

        var totalSeconds = Math.floor(milliseconds / 1000)
        var hours = Math.floor(totalSeconds / 3600)
        var minutes = Math.floor((totalSeconds % 3600) / 60)
        var seconds = totalSeconds % 60

        function twoDigits(value) {
            return value < 10 ? "0" + value : value.toString()
        }

        if (hours > 0)
            return hours + ":" + twoDigits(minutes) + ":" + twoDigits(seconds)

        return twoDigits(minutes) + ":" + twoDigits(seconds)
    }

    property var tutorialsModel: [
        {
            "title": "How to set up a profile manually",
            "subtitle": "Create a profile from scratch and point Universal Manager to the right folders yourself.",
            "format":"MP4",
            "videoRelativePath": "assets/Tutorials_Guides/Profile_manually/Profile_manually.mp4"
        },
        {
            "title": "How to use categorize system",
            "subtitle": "Walk through the categorize system.",
            "format": "MP4",
            "videoRelativePath": "assets/Tutorials_Guides/Categorize_system/Categorize_system.mp4"
        },
        {
            "title": "How to inject Avalonium",
            "subtitle": "Inject Avalonium, a runtime dumper and scripting engine, into your game.",
            "format": "MP4",
            "videoRelativePath": "assets/Tutorials_Guides/Avalonium_injection/Avalonium_injection.mp4"
        },
        {
            "title": "How to inject Avalonium using advanced injection",
            "subtitle": "Use advanced injection to inject Avalonium, a runtime dumper and scripting engine, into your game.",
            "format": "MP4",
            "videoRelativePath": "assets/Tutorials_Guides/Advanced_injection/Advanced_injection.mp4"
        },
        {
            "title": "How to generate usmap & sdk",
            "subtitle": "Generate a .usmap file and SDK for modding.",
            "format": "MP4",
            "videoRelativePath": "assets/Tutorials_Guides/Runtime_generation/Runtime_generation.mp4"
         },
        {

            "title": "How to load Avaloium using dwmapi.dll",
            "subtitle": "Manual Avaloium loading using dwmapi.dll.",
            "format": "MP4",
            "videoRelativePath": "assets/Tutorials_Guides/HowToLoadAvaloniumWithdwmapi.dll/AvaloniumWithdwmapi.dll.mp4"
        },
        {

            "title": "How to install Lua mods",
            "subtitle": "Installing Lua mods and run them with Avalonium.",
            "format": "MP4",
            "videoRelativePath": "assets/Tutorials_Guides/Installing_Lua_Mods/LuaMods.mp4"
        },
        {

            "title": "How to install Blueprint mods",
            "subtitle": "Installing Blueprint mods and run them with Avalonium.",
            "format": "MP4",
            "videoRelativePath": "assets/Tutorials_Guides/Installing_BP_Mods/BPMods.mp4"
        }

    ]

    Dialog {
        id: videoPlayerDialog
        parent: Overlay.overlay
        modal: true
        focus: true
        padding: 0
        x: 0
        y: 0
        width: parent ? parent.width : tutorialsPage.width
        height: parent ? parent.height : tutorialsPage.height
        closePolicy: Popup.CloseOnEscape

        property var tutorialData: null
        property url videoSource: ""
        property real overlayMargin: Math.max(20, Math.min(width, height) * 0.035)
        property real contentPadding: 18
        property real headerHeight: 60
        property real controlsHeight: 88
        property real sourceVideoWidth: 1920
        property real sourceVideoHeight: 1080
        property real sourceVideoAspectRatio: sourceVideoWidth / sourceVideoHeight
        property real maxDialogWidth: Math.max(420, width - (overlayMargin * 2))
        property real maxDialogHeight: Math.max(320, height - (overlayMargin * 2))
        property real videoViewportWidth: Math.min(
            Math.min(sourceVideoWidth, maxDialogWidth - (contentPadding * 2)),
            Math.min(sourceVideoHeight, maxDialogHeight - headerHeight - controlsHeight - contentPadding) * sourceVideoAspectRatio
        )
        property real videoViewportHeight: videoViewportWidth / sourceVideoAspectRatio
        property real dialogContentWidth: Math.min(maxDialogWidth, videoViewportWidth + (contentPadding * 2))
        property real dialogContentHeight: Math.min(maxDialogHeight, headerHeight + videoViewportHeight + controlsHeight + contentPadding)

        function openWithTutorial(tutorial) {
            tutorialData = tutorial
            videoSource = localAssetUrl(tutorial.videoRelativePath || "")
            videoPlayer.source = videoSource
            videoPlayer.play()
            open()
        }

        onClosed: {
            videoPlayer.stop()
            videoPlayer.source = ""
        }

        background: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.78)
        }

        Item {
            anchors.fill: parent
            Rectangle {
                anchors.fill: parent
                color: "transparent"

                MouseArea {
                    anchors.fill: parent
                    onClicked: videoPlayerDialog.close()
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: videoPlayerDialog.dialogContentWidth
                height: videoPlayerDialog.dialogContentHeight
                color: modManager.theme === "Glassmorphic"
                    ? Qt.rgba(0.15, 0.15, 0.15, 0.94)
                    : (modManager.theme === "Crimson" ? "transparent" : window.surfaceColor)
                radius: 20
                border.width: 1
                border.color: modManager.theme === "Glassmorphic"
                    ? Qt.rgba(1, 1, 1, 0.1)
                    : window.borderColor
                clip: true

                gradient: {
                    if (modManager.theme === "Cozy") return videoDialogCozyGradient
                    if (modManager.theme === "Crimson") return videoDialogCrimsonGradient
                    if (modManager.theme === "Wavey") return videoDialogWaveyGradient
                    return null
                }
                Gradient {
                    id: videoDialogCozyGradient
                    GradientStop { position: 0.0; color: window.cozyDialogGradientTop }
                    GradientStop { position: 1.0; color: window.cozyDialogGradientBottom }
                }
                Gradient {
                    id: videoDialogCrimsonGradient
                    GradientStop { position: 0.0; color: window.crimsonDialogGradientTop }
                    GradientStop { position: 1.0; color: window.crimsonDialogGradientBottom }
                }
                Gradient {
                    id: videoDialogWaveyGradient
                    GradientStop { position: 0.0; color: window.waveyDialogGradientTop }
                    GradientStop { position: 1.0; color: window.waveyDialogGradientBottom }
                }

                MouseArea {
                    anchors.fill: parent
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: videoPlayerDialog.headerHeight
                        color: "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: videoPlayerDialog.contentPadding
                            anchors.rightMargin: Math.max(12, videoPlayerDialog.contentPadding - 2)
                            spacing: 16

                            Text {
                                text: videoPlayerDialog.tutorialData ? videoPlayerDialog.tutorialData.title : ""
                                font.pixelSize: 20
                                font.weight: Font.DemiBold
                                color: window.textColor
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Rectangle {
                                width: 40
                                height: 40
                                radius: 20
                                color: closeDialogMouse.containsMouse
                                    ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.12)
                                    : "transparent"

                                Text {
                                    anchors.centerIn: parent
                                    text: "×"
                                    font.pixelSize: 24
                                    font.weight: Font.Bold
                                    color: window.textColor
                                }

                                MouseArea {
                                    id: closeDialogMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: videoPlayerDialog.close()
                                }

                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: videoPlayerDialog.contentPadding
                        Layout.rightMargin: videoPlayerDialog.contentPadding
                        Layout.bottomMargin: videoPlayerDialog.contentPadding
                        color: "transparent"
                        radius: 16
                        clip: true

                        MediaPlayer {
                            id: videoPlayer
                            audioOutput: AudioOutput { id: videoAudioOutput; muted: playerMuted }
                            videoOutput: videoOutput

                            onErrorOccurred: (error, errorString) => {
                                console.error("Media playback error:", error, errorString)
                            }
                        }

                        Item {
                            id: videoSurfaceArea
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: playbackControls.top

                            Rectangle {
                                id: videoViewport
                                anchors.centerIn: parent
                                width: Math.min(videoSurfaceArea.width, videoPlayerDialog.videoViewportWidth)
                                height: Math.min(videoSurfaceArea.height, videoPlayerDialog.videoViewportHeight)
                                color: "#000000"
                                radius: 12
                                clip: true

                                VideoOutput {
                                    id: videoOutput
                                    anchors.fill: parent
                                    fillMode: VideoOutput.PreserveAspectFit
                                }
                            }

                            Rectangle {
                                anchors.centerIn: videoViewport
                                width: 88
                                height: 88
                                radius: 44
                                color: Qt.rgba(0, 0, 0, 0.6)
                                visible: videoPlayer.playbackState !== MediaPlayer.PlayingState

                                Image {
                                    id: playOverlayIcon
                                    anchors.centerIn: parent
                                    source: "qrc:/UniversalModManager/Lucide Icons/play.svg"
                                    sourceSize: Qt.size(40, 40)
                                    visible: modManager.theme === "Cozy"
                                }

                                ColorOverlay {
                                    source: playOverlayIcon
                                    color: "#FFFFFF"
                                    width: 40
                                    height: 40
                                    anchors.centerIn: parent
                                    visible: modManager.theme !== "Cozy"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: videoPlayer.play()
                                }
                            }
                        }

                        Rectangle {
                            id: playbackControls
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 88
                            color: Qt.rgba(0, 0, 0, 0.7)

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 12

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    Text {
                                        text: formatMs(videoPlayer.position)
                                        font.pixelSize: 11
                                        font.family: "Consolas"
                                        color: "#FFFFFF"
                                    }

                                    Slider {
                                        id: seekSlider
                                        Layout.fillWidth: true
                                        from: 0
                                        to: videoPlayer.duration
                                        value: videoPlayer.position

                                        onMoved: {
                                            if (videoPlayer.seekable) {
                                                videoPlayer.position = value
                                            }
                                        }

                                        background: Rectangle {
                                            x: seekSlider.leftPadding
                                            y: seekSlider.topPadding + seekSlider.availableHeight / 2 - height / 2
                                            implicitWidth: 200
                                            implicitHeight: 4
                                            width: seekSlider.availableWidth
                                            height: implicitHeight
                                            radius: 2
                                            color: Qt.rgba(1, 1, 1, 0.3)

                                            Rectangle {
                                                width: seekSlider.visualPosition * parent.width
                                                height: parent.height
                                                color: window.accentColor
                                                radius: 2
                                            }
                                        }

                                        handle: Rectangle {
                                            x: seekSlider.leftPadding + seekSlider.visualPosition * (seekSlider.availableWidth - width)
                                            y: seekSlider.topPadding + seekSlider.availableHeight / 2 - height / 2
                                            implicitWidth: 14
                                            implicitHeight: 14
                                            radius: 7
                                            color: seekSlider.pressed ? Qt.lighter(window.accentColor, 1.2) : window.accentColor
                                        }
                                    }

                                    Text {
                                        text: formatMs(videoPlayer.duration)
                                        font.pixelSize: 11
                                        font.family: "Consolas"
                                        color: "#FFFFFF"
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Rectangle {
                                        width: 36
                                        height: 36
                                        radius: 18
                                        color: playPauseMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)

                                        Image {
                                            id: playPauseIcon
                                            anchors.centerIn: parent
                                            source: videoPlayer.playbackState === MediaPlayer.PlayingState
                                                ? "qrc:/UniversalModManager/Lucide Icons/pause.svg"
                                                : "qrc:/UniversalModManager/Lucide Icons/play.svg"
                                            sourceSize: Qt.size(18, 18)
                                            visible: modManager.theme === "Cozy"
                                        }

                                        ColorOverlay {
                                            source: playPauseIcon
                                            color: "#FFFFFF"
                                            width: 18
                                            height: 18
                                            anchors.centerIn: parent
                                            visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                                        }

                                        MouseArea {
                                            id: playPauseMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (videoPlayer.playbackState === MediaPlayer.PlayingState) {
                                                    videoPlayer.pause()
                                                } else {
                                                    videoPlayer.play()
                                                }
                                            }
                                        }

                                        Behavior on color { ColorAnimation { duration: 150 } }
                                    }

                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        id: pageLayout
        width: parent ? parent.width : 0
        spacing: 24

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "Tutorials & Guides"
                font.pixelSize: 20
                font.weight: Font.Bold
                color: window.textColor
            }

            Rectangle {
                width: tutorialCountText.width + 20
                height: 26
                radius: 6
                color: Qt.rgba(0, 0, 0, 0.06)

                Text {
                    id: tutorialCountText
                    anchors.centerIn: parent
                    text: tutorialsPage.tutorialsModel.length.toString()
                    font.pixelSize: 13
                    font.weight: Font.Bold
                    color: "#666666"
                }
            }

            Item { Layout.fillWidth: true }
        }

        Text {
            Layout.fillWidth: true
            text: "Built-in tutorials to help you get started."
            font.pixelSize: 13
            color: window.textSecondaryColor
            wrapMode: Text.WordWrap
        }

        Flow {
            id: cardsFlow
            Layout.fillWidth: true
            spacing: 20

            readonly property int targetCardWidth: 350
            readonly property int columns: Math.max(1, Math.floor((width + spacing) / (targetCardWidth + spacing)))
            property int cardWidth: Math.floor((width - (columns - 1) * spacing) / columns)

            Repeater {
                model: tutorialsPage.tutorialsModel

                delegate: Item {
                    id: tutorialCardContainer
                    required property var modelData
                    required property int index

                    width: cardsFlow.cardWidth
                    height: 215

                    MouseArea {
                        id: tutorialCardMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        onClicked: videoPlayerDialog.openWithTutorial(tutorialCardContainer.modelData)
                    }

                    Rectangle {
                        id: tutorialCard
                        anchors.fill: parent

                        property bool isHovered: tutorialCardMouse.containsMouse
                        property bool isCozyTheme: modManager && modManager.theme === "Cozy"
                        property bool isCrimsonTheme: modManager && modManager.theme === "Crimson"

                        radius: 0
                        clip: true
                        color: modManager.theme === "Crimson" ? "transparent" :
                               (tutorialsPage.isGlassmorphic ? window.surfaceColor : window.surfaceColor)
                        border.width: tutorialsPage.isGlassmorphic
                                     ? 1
                                     : ((isCozyTheme || isCrimsonTheme) ? (isHovered ? 2 : 1) : 1)
                        border.color: tutorialsPage.isGlassmorphic
                                     ? Qt.rgba(1, 1, 1, isHovered ? 0.45 : 0.3)
                                     : (isCozyTheme
                                        ? (isHovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.7))
                                        : (isCrimsonTheme
                                           ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, isHovered ? 0.95 : 0.65)
                                           : Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.75)))

                        gradient: modManager.theme === "Crimson" ? tutorialCardGradient : null

                        Gradient {
                            id: tutorialCardGradient
                            GradientStop { position: 0.0; color: window.crimsonGradientTop }
                            GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                        }

                        Behavior on border.width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }

                        layer.enabled: !tutorialsPage.isGlassmorphic
                        layer.smooth: true
                        layer.samples: 4
                        layer.textureSize: Qt.size(width * 2, height * 2)
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowColor: Qt.rgba(0, 0, 0, 0.06)
                            shadowHorizontalOffset: 0
                            shadowVerticalOffset: 8
                            shadowBlur: 0.5
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"

                            Rectangle {
                                anchors.fill: parent
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18) }
                                    GradientStop { position: 1.0; color: Qt.rgba(window.surfaceColor.r, window.surfaceColor.g, window.surfaceColor.b, 0.04) }
                                }
                            }

                            Rectangle {
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.margins: 18
                                width: 36
                                height: 36
                                radius: 18
                                color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.14)
                                border.width: 1
                                border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.24)

                                Image {
                                    id: playIcon
                                    anchors.centerIn: parent
                                    source: "qrc:/UniversalModManager/Lucide Icons/play.svg"
                                    sourceSize: Qt.size(18, 18)
                                    visible: modManager.theme === "Cozy"
                                }

                                ColorOverlay {
                                    source: playIcon
                                    color: modManager.theme === "Crimson" ? "#FFFFFF" : window.primaryColor
                                    width: 18
                                    height: 18
                                    anchors.centerIn: parent
                                    visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                                }
                            }

                            ColumnLayout {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 20
                                anchors.topMargin: 80
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: tutorialCardContainer.modelData.title
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    color: window.textColor
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: tutorialCardContainer.modelData.subtitle
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                    opacity: 0.85
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
