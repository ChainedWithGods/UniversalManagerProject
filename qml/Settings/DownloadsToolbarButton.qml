import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Rectangle {
    id: control
    property string iconSource: ""
    property string label: ""
    property string tooltipText: ""
    property bool busy: false
    signal clicked()

    implicitWidth: label.length > 0 ? buttonRow.implicitWidth + 24 : 34
    implicitHeight: 34
    radius: 8
    color: hoverArea.containsMouse
        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)
        : (modManager.theme === "Glassmorphic"
            ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06)
            : "transparent")
    border.width: 1
    border.color: hoverArea.containsMouse
        ? window.primaryColor
        : (modManager.theme === "Glassmorphic"
            ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.28)
            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.14))

    Behavior on color { ColorAnimation { duration: 120 } }
    Behavior on border.color { ColorAnimation { duration: 120 } }

    RowLayout {
        id: buttonRow
        anchors.centerIn: parent
        spacing: 7

        Item {
            id: iconWrap
            width: 20
            height: 20

            BusyRing {
                anchors.centerIn: parent
                running: control.busy
                ringColor: window.primaryColor
            }

            Item {
                anchors.centerIn: parent
                width: 15
                height: 15

                visible: !control.busy

                Image {
                    id: iconImage
                    anchors.fill: parent
                    source: control.iconSource
                    sourceSize: Qt.size(16, 16)
                    visible: modManager.theme === "Cozy"
                }

                ColorOverlay {
                    anchors.fill: iconImage
                    source: iconImage
                    color: hoverArea.containsMouse ? window.primaryColor : window.iconColor
                    visible: modManager.theme !== "Cozy"
                }
            }
        }

        Text {
            visible: control.label.length > 0
            text: control.label
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: hoverArea.containsMouse ? window.primaryColor : window.textColor
        }
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: control.clicked()
    }

    ToolTip {
        id: tip
        visible: hoverArea.containsMouse && control.tooltipText.length > 0
        text: control.tooltipText
        delay: 600
        padding: 10

        contentItem: Text {
            text: tip.text
            color: window.textColor
            font.pixelSize: 11
        }

        background: Rectangle {
            radius: 8
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor
            border.width: 1
            border.color: window.borderColor
        }
    }
}
