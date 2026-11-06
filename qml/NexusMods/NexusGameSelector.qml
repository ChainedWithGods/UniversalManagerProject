import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Rectangle {
    property color panelColor: window.surfaceColor
    id: gameSelector
    Layout.preferredWidth: 190
    Layout.preferredHeight: 34
    radius: 8
    color: gameSelectorMouse.containsMouse
        ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08)
        : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.04)
    border.width: 1
    border.color: gamePopup.visible ? window.primaryColor : window.borderColor
    Behavior on border.color { ColorAnimation { duration: 150 } }

    property string currentName: {
        const games = modManager.nexusGamesList
        for (let i = 0; i < games.length; ++i) {
            if (games[i].domain === modManager.nexusGameDomain) return games[i].name
        }
        return modManager.nexusGameDomain
    }

    property var filteredGames: {
        const all = modManager.nexusGamesList
        const query = gameSearchField.text.trim().toLowerCase()
        if (query.length === 0) return all

        const matches = []
        for (let i = 0; i < all.length; ++i) {
            if (all[i].name.toLowerCase().indexOf(query) !== -1) matches.push(all[i])
        }
        return matches
    }

    Text {
        anchors.left: parent.left
        anchors.right: gameChevron.left
        anchors.leftMargin: 10
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        text: gameSelector.currentName
        font.pixelSize: 12
        font.weight: Font.Medium
        color: window.textColor
        elide: Text.ElideRight
    }

    Image {
        id: gameChevron
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        width: 12
        height: 12
        source: "qrc:/UniversalModManager/Lucide Icons/chevron-down.svg"
        sourceSize: Qt.size(12, 12)
        smooth: true
        rotation: gamePopup.visible ? 180 : 0
        Behavior on rotation { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

        layer.enabled: true
        layer.effect: ColorOverlay {
            color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.6)
        }
    }

    MouseArea {
        id: gameSelectorMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            gameSearchField.text = ""
            gamePopup.open()
        }
    }

    Popup {
        id: gamePopup
        x: gameSelector.width - width
        y: gameSelector.height + 6
        width: 300
        padding: 8
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: gameSearchField.forceActiveFocus()

        background: Rectangle {
            radius: 10
            color: panelColor
            border.width: 1
            border.color: window.borderColor

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.15)
                shadowVerticalOffset: 4
                shadowBlur: 0.8
            }
        }

        contentItem: ColumnLayout {
            spacing: 6

            NexusFilterTextField {
                id: gameSearchField
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                placeholderText: "Search games..."
            }

            ListView {
                id: gameList
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                clip: true
                model: gameSelector.filteredGames
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: Rectangle {
                    width: gameList.width
                    height: 30
                    radius: 6
                    color: (gameRowMouse.containsMouse || modManager.nexusGameDomain === modelData.domain)
                        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)
                        : "transparent"

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.name
                        font.pixelSize: 12
                        color: window.textColor
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        id: gameRowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            modManager.setNexusGameDomain(modelData.domain)
                            gamePopup.close()
                        }
                    }
                }
            }
        }
    }
}
