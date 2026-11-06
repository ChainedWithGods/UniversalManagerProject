import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Rectangle {
    id: checkRow
    Layout.fillWidth: true
    Layout.preferredHeight: 30
    radius: 6
    color: rowMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05) : "transparent"

    property bool checked: false
    property bool dimmed: false
    property string label: ""
    property string countText: ""
    signal toggled()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 8
        spacing: 9

        Rectangle {
            Layout.preferredWidth: 17
            Layout.preferredHeight: 17
            radius: 5
            color: checkRow.checked ? window.primaryColor : "transparent"
            border.width: 1
            border.color: checkRow.checked
                ? window.primaryColor
                : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.35)

            Behavior on color { ColorAnimation { duration: 120 } }

            Image {
                anchors.centerIn: parent
                width: 11
                height: 11
                source: "qrc:/UniversalModManager/Lucide Icons/check.svg"
                sourceSize: Qt.size(11, 11)
                smooth: true
                visible: checkRow.checked

                layer.enabled: true
                layer.effect: ColorOverlay {
                    color: "#FFFFFF"
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: checkRow.label
            font.pixelSize: 12
            elide: Text.ElideRight
            font.weight: checkRow.checked ? Font.DemiBold : Font.Normal
            color: checkRow.dimmed
                ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.4)
                : (checkRow.checked
                    ? window.textColor
                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.85))
        }

        Text {
            visible: checkRow.countText.length > 0
            text: checkRow.countText
            font.pixelSize: 11
            color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.45)
        }
    }

    MouseArea {
        id: rowMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: checkRow.toggled()
    }
}
