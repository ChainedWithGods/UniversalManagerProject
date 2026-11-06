import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects
import Qt.labs.platform as Platform

Item {
    id: host

    property var hostWindow: null
    property var trayIcon: null
    property string title: ""
    property string message: ""
    property string iconName: ""
    property int displayDuration: 3200
    property bool trayWasVisible: false

    function push(title, message, iconName, duration) {
        if (!modManager.notificationsEnabled)
            return

        const safeTitle = (title || "").trim()
        const safeMessage = (message || "").replace(/\s+/g, " ").trim()
        if (safeTitle.length === 0 && safeMessage.length === 0)
            return

        host.displayDuration = duration !== undefined ? duration : 3200

        if (hostWindow && hostWindow.visible && hostWindow.active) {
            host.title = safeTitle
            host.message = safeMessage
            host.iconName = iconName || ""
            showAnimation.restart()
        } else {
            showNative(safeTitle, safeMessage)
        }
    }

    function showNative(title, message) {
        if (!trayIcon)
            return

        host.trayWasVisible = trayIcon.visible
        trayIcon.visible = true
        trayIcon.showMessage(title.length > 0 ? title : "Universal Manager",
                             message,
                             Platform.SystemTrayIcon.Information,
                             host.displayDuration)

        if (!host.trayWasVisible)
            hideTrayTimer.restart()
    }

    Timer {
        id: hideTrayTimer
        interval: host.displayDuration + 1200
        repeat: false
        onTriggered: {
            if (!host.trayWasVisible && host.trayIcon
                    && (!host.hostWindow || host.hostWindow.visible))
                host.trayIcon.visible = false
        }
    }

    Connections {
        target: notificationManager
        function onNotificationPosted(title, message, icon, duration) {
            host.push(title, message, icon, duration)
        }
    }

    Item {
        id: toastLayer
        anchors.fill: parent
        z: 1000

        readonly property real toastTargetX: width - toastContent.width - 28
        readonly property real toastOffscreenX: width + 680

        Rectangle {
            id: toastContent
            width: Math.min(680, Math.max(320,
                Math.max(toastTitle.implicitWidth, toastText.implicitWidth)
                + (toastIcon.visible ? 64 : 40)))
            height: Math.max(56, toastColumn.implicitHeight + 28)
            radius: 16
            x: toastLayer.toastOffscreenX
            y: toastLayer.height - height - 28
            opacity: 0

            readonly property color toastTextColor: {
                if (modManager.theme === "Cozy") return window.cozyToastTextColor
                if (modManager.theme === "Crimson") return window.crimsonToastTextColor
                if (modManager.theme === "Wavey") return window.waveyToastTextColor
                if (modManager.theme === "Glassmorphic") return themeConfig.glassmorphic.toastTextColor
                return "#FFFFFF"
            }

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0.0
                    color: {
                        if (modManager.theme === "Cozy") return window.cozyToastGradientTop
                        if (modManager.theme === "Crimson") return window.crimsonToastGradientTop
                        if (modManager.theme === "Wavey") return window.waveyToastGradientTop
                        if (modManager.theme === "Glassmorphic") return themeConfig.glassmorphic.toastGradientTop
                        return Qt.rgba(0.12, 0.12, 0.15, 0.95)
                    }
                }
                GradientStop {
                    position: 1.0
                    color: {
                        if (modManager.theme === "Cozy") return window.cozyToastGradientBottom
                        if (modManager.theme === "Crimson") return window.crimsonToastGradientBottom
                        if (modManager.theme === "Wavey") return window.waveyToastGradientBottom
                        if (modManager.theme === "Glassmorphic") return themeConfig.glassmorphic.toastGradientBottom
                        return Qt.rgba(0.08, 0.08, 0.12, 0.95)
                    }
                }
            }

            border.width: 1
            border.color: {
                if (modManager.theme === "Cozy") return window.cozyToastBorderColor
                if (modManager.theme === "Crimson") return window.crimsonToastBorderColor
                if (modManager.theme === "Wavey") return window.waveyToastBorderColor
                if (modManager.theme === "Glassmorphic") return themeConfig.glassmorphic.toastBorderColor
                return Qt.rgba(1, 1, 1, 0.06)
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 24
                anchors.topMargin: 14
                anchors.bottomMargin: 14
                spacing: 14

                Image {
                    id: toastIcon
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                    visible: host.iconName.length > 0
                    source: visible ? "qrc:/UniversalModManager/Lucide Icons/" + host.iconName : ""
                    sourceSize: Qt.size(22, 22)
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: toastContent.toastTextColor
                    }
                }

                ColumnLayout {
                    id: toastColumn
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        id: toastTitle
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: host.title
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: toastContent.toastTextColor
                        opacity: 0.78
                        elide: Text.ElideRight
                    }

                    Text {
                        id: toastText
                        Layout.fillWidth: true
                        text: host.message
                        font.pixelSize: 13
                        font.weight: host.title.length > 0 ? Font.Normal : Font.DemiBold
                        color: toastContent.toastTextColor
                        wrapMode: Text.Wrap
                        maximumLineCount: 4
                    }
                }
            }

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                radius: parent.radius
                color: Qt.rgba(1, 1, 1, 0.04)
                visible: modManager.theme !== "Cozy"
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    showAnimation.stop()
                    toastContent.x = toastLayer.toastOffscreenX
                    toastContent.opacity = 0
                }
            }

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.5)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 8
                shadowBlur: 0.8
            }
        }

        SequentialAnimation {
            id: showAnimation

            ParallelAnimation {
                NumberAnimation {
                    target: toastContent
                    property: "x"
                    to: toastLayer.toastTargetX
                    duration: 350
                    easing.type: Easing.OutBack
                    easing.overshoot: 1.2
                }
                NumberAnimation {
                    target: toastContent
                    property: "opacity"
                    to: 1.0
                    duration: 250
                }
            }

            PauseAnimation { duration: host.displayDuration }

            ParallelAnimation {
                NumberAnimation {
                    target: toastContent
                    property: "x"
                    to: toastLayer.toastOffscreenX
                    duration: 300
                    easing.type: Easing.InCubic
                }
                NumberAnimation {
                    target: toastContent
                    property: "opacity"
                    to: 0
                    duration: 250
                }
            }
        }
    }
}
