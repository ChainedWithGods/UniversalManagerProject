import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Rectangle {
    id: root
    property string label: ""
    property string iconSource: ""
    property int count: 0
    property bool selected: false
    signal clicked()

    implicitHeight: 36
    radius: 8
    color: selected
        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.14)
        : (hoverArea.containsMouse
            ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05)
            : "transparent")

    Behavior on color { ColorAnimation { duration: 120 } }

    Rectangle {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 3
        height: 16
        radius: 1.5
        color: window.primaryColor
        visible: root.selected
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 10
        spacing: 9

        Item {
            width: 15
            height: 15

            Image {
                id: iconImage
                anchors.fill: parent
                source: root.iconSource
                sourceSize: Qt.size(16, 16)
                opacity: root.selected ? 1.0 : 0.7
                visible: modManager.theme === "Cozy"
            }

            ColorOverlay {
                anchors.fill: iconImage
                source: iconImage
                color: root.selected ? window.primaryColor : window.iconColor
                opacity: root.selected ? 1.0 : 0.8
                visible: modManager.theme !== "Cozy"
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.label
            font.pixelSize: 12
            font.weight: root.selected ? Font.DemiBold : Font.Normal
            color: root.selected ? window.primaryColor : window.textColor
            elide: Text.ElideRight
        }

        Text {
            text: root.count
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: root.selected ? window.primaryColor : window.textSecondaryColor
            opacity: root.selected ? 1.0 : 0.8
        }
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
