import QtQuick
import QtQuick.Layouts

Column {
    id: tableRoot

    property var rows: []
    property string nameHeader: "Mod name"
    property string notesHeader: "Notes"

    property var appWindow: null
    readonly property var themeWindow: appWindow || Window.window
    readonly property bool showNotes: notesHeader.length > 0
    readonly property real nameWidth: showNotes
        ? Math.max(150, (width - 36) * 0.52)
        : width

    spacing: 0

    Rectangle {
        width: tableRoot.width
        height: 30
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 16

            Text {
                Layout.preferredWidth: tableRoot.showNotes ? tableRoot.nameWidth : 0
                Layout.fillWidth: !tableRoot.showNotes
                text: tableRoot.nameHeader
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: Qt.rgba(tableRoot.themeWindow.textColor.r, tableRoot.themeWindow.textColor.g, tableRoot.themeWindow.textColor.b, 0.5)
                elide: Text.ElideRight
            }

            Text {
                visible: tableRoot.showNotes
                Layout.fillWidth: true
                text: tableRoot.notesHeader
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: Qt.rgba(tableRoot.themeWindow.textColor.r, tableRoot.themeWindow.textColor.g, tableRoot.themeWindow.textColor.b, 0.5)
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Qt.rgba(tableRoot.themeWindow.textColor.r, tableRoot.themeWindow.textColor.g, tableRoot.themeWindow.textColor.b, 0.1)
        }
    }

    Repeater {
        model: tableRoot.rows

        delegate: Rectangle {
            id: rowRoot
            required property var modelData
            required property int index

            readonly property bool linked: !!modelData && !!modelData.url

            width: tableRoot.width
            height: rowLayout.implicitHeight + 20
            color: linked && rowHover.containsMouse
                ? Qt.rgba(tableRoot.themeWindow.textColor.r, tableRoot.themeWindow.textColor.g, tableRoot.themeWindow.textColor.b, 0.045)
                : "transparent"

            Behavior on color { ColorAnimation { duration: 90 } }

            RowLayout {
                id: rowLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 16

                Text {
                    Layout.preferredWidth: tableRoot.showNotes ? tableRoot.nameWidth : 0
                    Layout.fillWidth: !tableRoot.showNotes
                    text: (modelData && (modelData.modName || modelData.name)) || "Unnamed"
                    font.pixelSize: 13
                    font.underline: rowRoot.linked && rowHover.containsMouse
                    color: rowRoot.linked
                        ? tableRoot.themeWindow.readableColorHex(tableRoot.themeWindow.primaryColor, tableRoot.themeWindow.surfaceColor)
                        : tableRoot.themeWindow.textColor
                    wrapMode: Text.Wrap
                }

                Text {
                    visible: tableRoot.showNotes
                    Layout.fillWidth: true
                    text: (modelData && modelData.notes && modelData.notes.length > 0)
                        ? modelData.notes : "—"
                    font.pixelSize: 12
                    color: Qt.rgba(tableRoot.themeWindow.textColor.r, tableRoot.themeWindow.textColor.g, tableRoot.themeWindow.textColor.b, 0.7)
                    wrapMode: Text.Wrap
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Qt.rgba(tableRoot.themeWindow.textColor.r, tableRoot.themeWindow.textColor.g, tableRoot.themeWindow.textColor.b, 0.07)
            }

            MouseArea {
                id: rowHover
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton
                cursorShape: rowRoot.linked ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: {
                    if (rowRoot.linked)
                        tableRoot.themeWindow.handleDescriptionLink(rowRoot.modelData.url)
                }
            }
        }
    }
}
