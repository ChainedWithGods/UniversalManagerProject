import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: modsUsingTab
    property string modsUsingFilter: ""
    readonly property var req: modManager.currentNexusModRequirements || ({})
    readonly property var usingRows: filteredModsUsing()
    readonly property int loaded: (req.using || []).length
    readonly property int total: req.usingTotal || 0
    readonly property bool loading: modManager.isLoadingNexusModRequirements
    readonly property bool failed: req.error === true

    function filteredModsUsing() {
        var all = (modManager.currentNexusModRequirements
            && modManager.currentNexusModRequirements.using) || []
        var query = modsUsingFilter.trim().toLowerCase()
        if (query.length === 0)
            return all
        var filtered = []
        for (var i = 0; i < all.length; ++i) {
            if ((all[i].modName || "").toLowerCase().indexOf(query) !== -1)
                filtered.push(all[i])
        }
        return filtered
    }

    function resetFilter() {
        modsUsingFilter = ""
        modsUsingFilterField.text = ""
    }

    Flickable {
        id: modsUsingFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: modsUsingCentered.implicitHeight + 60
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        property real scrollTarget: 0

        onContentYChanged: {
            if (!modsUsingScrollAnim.running)
                scrollTarget = contentY
        }

        onMovementStarted: modsUsingScrollAnim.stop()

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: (event) => {
                event.accepted = true
                const maxY = Math.max(0, modsUsingFlick.contentHeight - modsUsingFlick.height)
                const targetY = Math.max(0, Math.min(maxY,
                    modsUsingFlick.scrollTarget - event.angleDelta.y * 3.5))
                if (targetY === modsUsingFlick.scrollTarget)
                    return
                modsUsingFlick.scrollTarget = targetY
                const distance = Math.abs(targetY - modsUsingFlick.contentY)
                modsUsingScrollAnim.stop()
                modsUsingScrollAnim.to = targetY
                modsUsingScrollAnim.duration = Math.max(160, Math.min(500, distance * 0.35))
                modsUsingScrollAnim.start()
            }
        }

        NumberAnimation {
            id: modsUsingScrollAnim
            target: modsUsingFlick
            property: "contentY"
            easing.type: Easing.OutQuart
        }

        Item {
            id: modsUsingCentered
            width: parent.width
            anchors.horizontalCenter: parent.horizontalCenter
            implicitHeight: modsUsingCol.implicitHeight + 40

            ColumnLayout {
                id: modsUsingCol
                width: Math.min(parent.width - 48, Window.window.contentMaxWidth + 36)
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 20
                spacing: 14

                Text {
                    text: "Mods using this mod"
                    font.pixelSize: 15
                    font.weight: Font.Bold
                    color: Window.window.textColor
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: modsUsingCardColumn.implicitHeight + 36
                    radius: 8
                    color: Window.window.panelColor(Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.02))
                    border.width: 1
                    border.color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.08)

                    Column {
                        id: modsUsingCardColumn
                        anchors.top: parent.top
                        anchors.topMargin: 18
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.min(parent.width - 36, Window.window.contentMaxWidth)
                        spacing: 10

                        RowLayout {
                            width: parent.width
                            spacing: 8

                            TextField {
                                id: modsUsingFilterField
                                Layout.fillWidth: true
                                Layout.preferredHeight: 34
                                leftPadding: 10
                                rightPadding: 10
                                font.pixelSize: 12
                                color: Window.window.textColor
                                placeholderTextColor: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.4)
                                placeholderText: "Filter by mod name..."
                                selectByMouse: true
                                ContextMenu.menu: null
                                background: Rectangle {
                                    radius: 6
                                    color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.04)
                                    border.width: 1
                                    border.color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.15)
                                }
                                onAccepted: modsUsingFilter = text
                            }

                            Rectangle {
                                Layout.preferredWidth: 92
                                Layout.preferredHeight: 34
                                radius: 6
                                color: searchUsingMouse.containsMouse
                                    ? Qt.lighter(Window.window.primaryColor, 1.08)
                                    : Window.window.primaryColor
                                border.width: 1
                                border.color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.15)

                                Text {
                                    anchors.centerIn: parent
                                    text: "SEARCH"
                                    font.pixelSize: 11
                                    font.weight: Font.Bold
                                    color: Window.window.relativeLuminance(Window.window.primaryColor) > 0.5
                                        ? "#1A1A1A" : "#FFFFFF"
                                }

                                MouseArea {
                                    id: searchUsingMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: modsUsingFilter = modsUsingFilterField.text
                                }
                            }
                        }

                        RowLayout {
                            width: parent.width
                            spacing: 10
                            visible: modsUsingTab.loading

                            BusyIndicator {
                                Layout.preferredWidth: 18
                                Layout.preferredHeight: 18
                                running: modsUsingTab.loading
                            }

                            Text {
                                text: "Loading mods that use this mod…"
                                font.pixelSize: 12
                                color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.6)
                            }
                        }

                        Text {
                            width: parent.width
                            visible: !modsUsingTab.loading && modsUsingTab.usingRows.length === 0
                            text: (modsUsingTab.failed && modsUsingTab.loaded === 0)
                                ? "Could not load mods using this mod. Reopen the mod to retry."
                                : (modsUsingFilter.length > 0
                                    ? "No loaded entries match “" + modsUsingFilter + "”."
                                        + (modsUsingTab.loaded < modsUsingTab.total
                                            ? " Load more or change the filter." : "")
                                    : "No mods are using this mod yet.")
                            font.pixelSize: 12
                            color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.6)
                            wrapMode: Text.Wrap
                        }

                        NexusRequirementTable {
                            width: parent.width
                            visible: modsUsingTab.usingRows.length > 0
                            rows: modsUsingTab.usingRows
                        }

                        RowLayout {
                            width: parent.width
                            visible: modsUsingTab.loaded < modsUsingTab.total

                            Item { Layout.fillWidth: true }

                            Rectangle {
                                Layout.preferredWidth: 110
                                Layout.preferredHeight: 30
                                radius: 6
                                color: loadMoreUsingMouse.containsMouse
                                    ? Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.1)
                                    : Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.05)
                                border.width: 1
                                border.color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.18)

                                Text {
                                    anchors.centerIn: parent
                                    text: "LOAD MORE"
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    color: Window.window.textColor
                                }

                                MouseArea {
                                    id: loadMoreUsingMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: modManager.loadMoreNexusModsRequiring()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
