import QtQuick
import QtQuick.Effects
import QtMultimedia

Item {
    id: root
    anchors.fill: parent

    property var theme: null

    property var hostWindow: null

    property real backgroundBlurAmount: theme && theme.backgroundBlurAmount !== undefined ? theme.backgroundBlurAmount : 0.6
    property real acrylicOpacity: theme && theme.acrylicOpacity !== undefined ? theme.acrylicOpacity : 0.85
    property color acrylicTintColor: theme && theme.acrylicTintColor ? theme.acrylicTintColor : "#1e3a5f"
    property bool useAcrylicBlur: theme ? (theme.useAcrylicBlur !== undefined ? theme.useAcrylicBlur : true) : true
    property bool hostAnimationsPaused: hostWindow ? hostWindow.animationsPaused === true : false
    property bool hostBlurReduced: hostWindow ? hostWindow.blurReduced === true : false
    property bool animationsActive: root.visible && !root.hostAnimationsPaused
    property real effectiveBlurAmount: root.hostBlurReduced
        ? Math.max(0.05, root.backgroundBlurAmount * 0.25)
        : root.backgroundBlurAmount
    property int effectiveBlurMax: root.hostBlurReduced ? 20 : 100

    onAnimationsActiveChanged: {
        if (!mediaPlayer) return
        if (animationsActive) {
            mediaPlayer.play()
        } else {
            mediaPlayer.pause()
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: !root.animationsActive
        color: root.acrylicTintColor
    }

    Video {
        id: mediaPlayer
        anchors.fill: parent
        visible: root.animationsActive

        source: "qrc:/UniversalModManager/assets/Wavey.mp4"
        autoPlay: root.animationsActive
        loops: MediaPlayer.Infinite
        fillMode: VideoOutput.PreserveAspectCrop
        muted: true

        onVisibleChanged: {
            if (!visible) {
                pause()
            } else if (root.animationsActive) {
                play()
            }
        }
        Component.onCompleted: {
            if (root.animationsActive) {
                play()
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: mediaPlayer.error !== MediaPlayer.NoError
            color: "#1A0A2E"
        }
    }

    Item {
        anchors.fill: parent

        ShaderEffectSource {
            id: videoBlurSource
            anchors.fill: parent
            sourceItem: mediaPlayer
            live: root.animationsActive && root.useAcrylicBlur
            recursive: false
            hideSource: false
            visible: false
        }

        MultiEffect {
            anchors.fill: parent
            source: videoBlurSource
            blurEnabled: root.useAcrylicBlur
            blur: root.effectiveBlurAmount
            blurMax: root.effectiveBlurMax
            autoPaddingEnabled: false
            opacity: 0.8
            visible: root.useAcrylicBlur && root.animationsActive
        }

        Rectangle {
            anchors.fill: parent
            visible: root.animationsActive
            color: Qt.rgba(root.acrylicTintColor.r, root.acrylicTintColor.g, root.acrylicTintColor.b, root.acrylicOpacity * 0.3)

            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: Qt.rgba(root.acrylicTintColor.r * 1.2, root.acrylicTintColor.g * 1.2, root.acrylicTintColor.b * 1.2, root.acrylicOpacity * 0.25)
                }
                GradientStop {
                    position: 1.0
                    color: Qt.rgba(root.acrylicTintColor.r * 0.8, root.acrylicTintColor.g * 0.8, root.acrylicTintColor.b * 0.8, root.acrylicOpacity * 0.35)
                }
            }
        }
    }
}
