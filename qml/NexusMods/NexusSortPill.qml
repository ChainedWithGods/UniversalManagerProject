import QtQuick
import QtQuick.Controls

Rectangle {
    id: pill

    property var options: []
    property int currentIndex: 0
    signal selected(int index)

    implicitWidth: pillRow.implicitWidth + 20
    implicitHeight: 28
    width: implicitWidth
    height: implicitHeight
    radius: 5
    color: (pillMouse.containsMouse || menu.visible)
        ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.09)
        : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05)
    border.width: 1
    border.color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.22)

    Row {
        id: pillRow
        anchors.centerIn: parent
        spacing: 5

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: (pill.options[pill.currentIndex] && pill.options[pill.currentIndex].label)
                ? pill.options[pill.currentIndex].label : ""
            font.pixelSize: 11
            color: window.textColor
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "▾"
            font.pixelSize: 9
            color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.6)
        }
    }

    MouseArea {
        id: pillMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: menu.visible ? menu.close() : menu.open()
    }

    Popup {
        id: menu
        parent: pill
        x: 0
        y: pill.height + 4
        width: Math.max(pill.width, 136)
        padding: 4

        background: Rectangle {
            radius: 6
            color: window.surfaceColor
            border.width: 1
            border.color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.22)
        }

        contentItem: Column {
            spacing: 2

            Repeater {
                model: pill.options

                delegate: Rectangle {
                    id: menuItem
                    required property var modelData
                    required property int index

                    width: menu.width - 8
                    height: 26
                    radius: 4
                    color: menuMouse.containsMouse
                        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.16)
                        : "transparent"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: menuItem.modelData.label || ""
                        font.pixelSize: 11
                        color: window.textColor
                    }

                    MouseArea {
                        id: menuMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            pill.currentIndex = menuItem.index
                            pill.selected(menuItem.index)
                            menu.close()
                        }
                    }
                }
            }
        }
    }
}
