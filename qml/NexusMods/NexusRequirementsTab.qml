import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: requirementsTab
    readonly property var req: modManager.currentNexusModRequirements || ({})
    readonly property var nexusRows: req.nexus || []
    readonly property var offsiteRows: req.offsite || []
    readonly property var dlcRows: req.dlc || []
    readonly property bool empty: nexusRows.length === 0
        && offsiteRows.length === 0 && dlcRows.length === 0
    readonly property bool loading: modManager.isLoadingNexusModRequirements
    readonly property bool failed: req.error === true

    Flickable {
        id: requirementsFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: requirementsCentered.implicitHeight + 60
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        property real scrollTarget: 0

        onContentYChanged: {
            if (!requirementsScrollAnim.running)
                scrollTarget = contentY
        }

        onMovementStarted: requirementsScrollAnim.stop()

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: (event) => {
                event.accepted = true
                const maxY = Math.max(0, requirementsFlick.contentHeight - requirementsFlick.height)
                const targetY = Math.max(0, Math.min(maxY,
                    requirementsFlick.scrollTarget - event.angleDelta.y * 3.5))
                if (targetY === requirementsFlick.scrollTarget)
                    return
                requirementsFlick.scrollTarget = targetY
                const distance = Math.abs(targetY - requirementsFlick.contentY)
                requirementsScrollAnim.stop()
                requirementsScrollAnim.to = targetY
                requirementsScrollAnim.duration = Math.max(160, Math.min(500, distance * 0.35))
                requirementsScrollAnim.start()
            }
        }

        NumberAnimation {
            id: requirementsScrollAnim
            target: requirementsFlick
            property: "contentY"
            easing.type: Easing.OutQuart
        }

        Item {
            id: requirementsCentered
            width: parent.width
            anchors.horizontalCenter: parent.horizontalCenter
            implicitHeight: requirementsCol.implicitHeight + 40

            ColumnLayout {
                id: requirementsCol
                width: Math.min(parent.width - 48, Window.window.contentMaxWidth + 36)
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 20
                spacing: 14

                Text {
                    text: "Requirements"
                    font.pixelSize: 15
                    font.weight: Font.Bold
                    color: Window.window.textColor
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: requirementsCardColumn.implicitHeight + 36
                    radius: 8
                    color: Window.window.panelColor(Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.02))
                    border.width: 1
                    border.color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.08)

                    Column {
                        id: requirementsCardColumn
                        anchors.top: parent.top
                        anchors.topMargin: 18
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.min(parent.width - 36, Window.window.contentMaxWidth)
                        spacing: 10

                        RowLayout {
                            width: parent.width
                            spacing: 10
                            visible: requirementsTab.loading

                            BusyIndicator {
                                Layout.preferredWidth: 18
                                Layout.preferredHeight: 18
                                running: requirementsTab.loading
                            }

                            Text {
                                text: "Loading requirements from Nexus…"
                                font.pixelSize: 12
                                color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.6)
                            }
                        }

                        Text {
                            width: parent.width
                            visible: !requirementsTab.loading && requirementsTab.empty
                            text: requirementsTab.failed
                                ? "Could not load requirements from Nexus. Reopen the mod to retry."
                                : "No requirements listed for this mod."
                            font.pixelSize: 12
                            color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.6)
                            wrapMode: Text.Wrap
                        }

                        Text {
                            width: parent.width
                            visible: requirementsTab.nexusRows.length > 0
                            text: "Nexus requirements"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: Window.window.textColor
                        }

                        NexusRequirementTable {
                            width: parent.width
                            visible: requirementsTab.nexusRows.length > 0
                            rows: requirementsTab.nexusRows
                        }

                        Text {
                            width: parent.width
                            visible: requirementsTab.offsiteRows.length > 0
                            text: "Off-site requirements"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: Window.window.textColor
                        }

                        NexusRequirementTable {
                            width: parent.width
                            visible: requirementsTab.offsiteRows.length > 0
                            rows: requirementsTab.offsiteRows
                        }

                        Text {
                            width: parent.width
                            visible: requirementsTab.dlcRows.length > 0
                            text: "DLC requirements"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: Window.window.textColor
                        }

                        NexusRequirementTable {
                            width: parent.width
                            visible: requirementsTab.dlcRows.length > 0
                            rows: requirementsTab.dlcRows
                            nameHeader: "DLC name"
                            notesHeader: ""
                        }
                    }
                }
            }
        }
    }
}
