import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Item {
    id: chip

    property color glowTop: window.accentColor
    property color glowBottom: window.primaryColor
    property string title: "Mod page"
    property string subtitle: "Minimized · click to restore"
    signal clicked()
    signal closed()

    width: 214
    height: 48

    property real waveAngle: 0
    NumberAnimation on waveAngle {
        running: chip.visible && !window.focusAnimationsPaused
        loops: Animation.Infinite
        from: 0
        to: 360
        duration: 5400
        easing.type: Easing.Linear
    }

    Item {
        id: outline
        anchors.fill: parent
        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: Rectangle {
                width: outline.width
                height: outline.height
                radius: 14
            }
        }

        ConicalGradient {
            anchors.fill: parent
            angle: chip.waveAngle
            gradient: Gradient {
                GradientStop { position: 0.0; color: chip.glowTop }
                GradientStop { position: 0.33; color: chip.glowBottom }
                GradientStop { position: 0.66; color: chip.glowTop }
                GradientStop { position: 1.0; color: chip.glowBottom }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1.5
        radius: 12.5

        color: modManager.theme === "Glassmorphic"
            ? Qt.rgba(0.09, 0.09, 0.11, 1.0)
            : Qt.rgba(window.surfaceColor.r, window.surfaceColor.g,
                      window.surfaceColor.b, 1.0)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            radius: 8
            color: Qt.rgba(chip.glowTop.r, chip.glowTop.g, chip.glowTop.b, 0.16)

            Image {
                anchors.centerIn: parent
                width: 15
                height: 15
                source: "qrc:/UniversalModManager/Lucide Icons/app-window.svg"
                sourceSize: Qt.size(15, 15)
                smooth: true
                layer.enabled: true
                layer.effect: ColorOverlay {
                    color: chip.glowTop
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Text {
                Layout.fillWidth: true
                text: chip.title
                font.pixelSize: 12
                font.weight: Font.DemiBold
                color: window.textColor
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: chip.subtitle
                font.pixelSize: 10
                color: window.textSecondaryColor
                elide: Text.ElideRight
            }
        }
    }

    MouseArea {
        id: chipMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: chip.clicked()
    }

    Rectangle {
        id: closeButton
        z: 2
        width: 22
        height: 22
        radius: 6
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        visible: chipMouse.containsMouse || closeMouse.containsMouse
        color: closeMouse.containsMouse
            ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.12)
            : "transparent"

        Image {
            anchors.centerIn: parent
            width: 12
            height: 12
            source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
            sourceSize: Qt.size(12, 12)
            smooth: true
            layer.enabled: true
            layer.effect: ColorOverlay {
                color: closeMouse.containsMouse ? window.textColor : window.textSecondaryColor
            }
        }

        MouseArea {
            id: closeMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: chip.closed()
        }
    }
}
