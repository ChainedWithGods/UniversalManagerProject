import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Dialog {
    id: dialog

    modal: true
    anchors.centerIn: Overlay.overlay
    width: 430
    padding: 0
    closePolicy: Popup.CloseOnEscape

    property int updateCount: 0

    readonly property bool hasUpdates: updateCount > 0
    readonly property bool darkTextOnPrimary:
        (0.2126 * window.primaryColor.r
         + 0.7152 * window.primaryColor.g
         + 0.0722 * window.primaryColor.b) > 0.55

    property Gradient dialogGradient: Gradient {
        orientation: Gradient.Vertical
        GradientStop {
            position: 0.0
            color: {
                if (modManager.theme === "Cozy") return window.cozyDialogGradientTop
                if (modManager.theme === "Crimson") return window.crimsonDialogGradientTop
                if (modManager.theme === "Wavey") return window.waveyDialogGradientTop
                return "transparent"
            }
        }
        GradientStop {
            position: 1.0
            color: {
                if (modManager.theme === "Cozy") return window.cozyDialogGradientBottom
                if (modManager.theme === "Crimson") return window.crimsonDialogGradientBottom
                if (modManager.theme === "Wavey") return window.waveyDialogGradientBottom
                return "transparent"
            }
        }
    }

    function show(count) {
        updateCount = Math.max(0, count)
        open()
    }

    Overlay.modal: Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.65)
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    enter: Transition {
        NumberAnimation { property: "scale"; from: 0.96; to: 1.0; duration: 220; easing.type: Easing.OutCubic }
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 }
    }

    exit: Transition {
        NumberAnimation { property: "scale"; from: 1.0; to: 0.97; duration: 160; easing.type: Easing.InCubic }
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 160 }
    }

    background: Rectangle {
        radius: 16
        color: modManager.theme === "Glassmorphic"
            ? Qt.rgba(0.1, 0.1, 0.12, 0.95)
            : window.surfaceColor
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.1)
        gradient: modManager.theme === "Glassmorphic" ? null : dialog.dialogGradient
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.5)
            shadowBlur: 1.0
            shadowVerticalOffset: 10
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 28
            spacing: 10

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                source: dialog.hasUpdates
                    ? "qrc:/UniversalModManager/Lucide Icons/cloud-download.svg"
                    : "qrc:/UniversalModManager/Lucide Icons/circle-check-big.svg"
                sourceSize: Qt.size(40, 40)
                fillMode: Image.PreserveAspectFit
                smooth: true
                layer.enabled: true
                layer.smooth: true
                layer.textureSize: Qt.size(width * 2, height * 2)
                layer.effect: ColorOverlay {
                    color: window.primaryColor
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.topMargin: 2
                horizontalAlignment: Text.AlignHCenter
                text: dialog.updateCount === 0
                    ? "No updates found"
                    : (dialog.updateCount === 1
                        ? "1 update available"
                        : dialog.updateCount + " updates available")
                font.pixelSize: 17
                font.weight: Font.DemiBold
                color: window.textColor
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: dialog.updateCount === 0
                    ? "You're up to date. No newer files were found for your tracked Nexus mods."
                    : (dialog.updateCount === 1
                        ? "One tracked mod has a newer file on Nexus Mods. It's marked with an update badge in the library."
                        : dialog.updateCount + " tracked mods have newer files on Nexus Mods. They're marked with an update badge in the library.")
                font.pixelSize: 12
                lineHeight: 1.3
                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.65)
                wrapMode: Text.Wrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 22
            Layout.bottomMargin: 20
            spacing: 10

            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: 100
                Layout.preferredHeight: 36
                radius: 8
                color: doneMouse.containsMouse
                    ? Qt.lighter(window.primaryColor, 1.1)
                    : window.primaryColor

                Text {
                    anchors.centerIn: parent
                    text: "Done"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: dialog.darkTextOnPrimary ? "#1A1A1A" : "#FFFFFF"
                }

                MouseArea {
                    id: doneMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: dialog.close()
                }
            }
        }
    }
}
