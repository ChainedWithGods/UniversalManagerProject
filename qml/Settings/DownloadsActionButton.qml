import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

Rectangle {
    id: root
    property string iconSource: ""
    property string tooltip: ""
    property color tint: window.textColor
    property bool danger: false
    signal clicked()

    implicitWidth: 26
    implicitHeight: 26
    radius: 6
    color: hoverArea.containsMouse
        ? (danger
            ? Qt.rgba(0.94, 0.27, 0.27, 0.14)
            : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12))
        : "transparent"

    Behavior on color { ColorAnimation { duration: 100 } }

    Item {
        anchors.centerIn: parent
        width: 13
        height: 13

        Image {
            id: iconImage
            anchors.fill: parent
            source: root.iconSource
            sourceSize: Qt.size(14, 14)
            visible: modManager.theme === "Cozy"
        }

        ColorOverlay {
            anchors.fill: iconImage
            source: iconImage
            color: root.danger
                ? "#EF4444"
                : (hoverArea.containsMouse ? window.primaryColor : root.tint)
            visible: modManager.theme !== "Cozy"
        }
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTip {
        id: tip
        visible: hoverArea.containsMouse && root.tooltip.length > 0
        text: root.tooltip
        delay: 500
        padding: 8

        contentItem: Text {
            text: tip.text
            color: window.textColor
            font.pixelSize: 11
        }

        background: Rectangle {
            radius: 7
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor
            border.width: 1
            border.color: window.borderColor
        }
    }
}
