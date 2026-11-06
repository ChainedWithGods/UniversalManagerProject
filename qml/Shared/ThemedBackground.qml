import QtQuick
import QtQuick.Effects

Item {
    id: root

    ThemeConfig {
        id: themeConfig
    }

    property color cozyColor: "#FFFAF5"

    property var hostWindow: null

    readonly property bool animationsPaused: hostWindow ? hostWindow.animationsPaused === true : false
    readonly property bool blurReduced: hostWindow ? hostWindow.blurReduced === true : false

    readonly property var waveyTheme: {
        var base = themeConfig.wavey
        var copy = {}
        for (var key in base)
            copy[key] = base[key]
        copy.backgroundBlurAmount = Math.min(1, (base.backgroundBlurAmount || 0.6) + 0.3)
        return copy
    }

    Rectangle {
        anchors.fill: parent
        color: modManager.theme === "Glassmorphic"
            ? "transparent"
            : (modManager.theme === "Wavey"
                ? (root.animationsPaused ? "#1A0A2E" : "transparent")
                : (modManager.theme === "Crimson" ? "transparent" : root.cozyColor))
        clip: true
    }

    Loader {
        anchors.fill: parent
        active: modManager.theme === "Wavey" && !root.animationsPaused
        sourceComponent: WaveyBackground {
            theme: root.waveyTheme
            hostWindow: root.hostWindow
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: modManager.theme === "Wavey"
        color: Qt.rgba(0.02, 0.015, 0.05, 0.18)
    }

    Image {
        anchors.fill: parent
        anchors.margins: -20
        source: modManager.theme === "Glassmorphic" && modManager.wallpaperPath !== ""
            ? "file:///" + modManager.wallpaperPath : ""
        fillMode: Image.PreserveAspectCrop
        visible: source != ""
        asynchronous: true
        cache: true
        smooth: false
        mipmap: false

        layer.enabled: !root.animationsPaused
        layer.smooth: false
        layer.effect: MultiEffect {
            blurEnabled: true
            blur: root.blurReduced
                ? Math.max(0.05, modManager.glassBlur * 0.25)
                : modManager.glassBlur
            blurMax: root.blurReduced ? 20 : 100
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
