import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Popup {
    id: dialog

    property string modName: ""
    property string fileName: ""

    property var appWindow: null

    readonly property color lineColor: appWindow
        ? Qt.rgba(appWindow.textColor.r, appWindow.textColor.g, appWindow.textColor.b, 0.16)
        : Qt.rgba(1, 1, 1, 0.16)

    signal downloadConfirmed()

    readonly property var requirementData: modManager.currentNexusModRequirements || ({})
    readonly property var nexusRows: requirementData.nexus || []
    readonly property var offsiteRows: requirementData.offsite || []
    readonly property var dlcRows: requirementData.dlc || []
    readonly property bool loading: modManager.isLoadingNexusModRequirements
    readonly property bool failed: requirementData.error === true
    readonly property bool hasRequirements: nexusRows.length > 0
        || offsiteRows.length > 0 || dlcRows.length > 0

    readonly property real maxScrollHeight: {
        var available = dialog.parent ? dialog.parent.height - 240 : 520
        return Math.max(160, Math.min(560, available))
    }

    function openFor(modTitle, fileTitle) {
        modName = modTitle || ""
        fileName = fileTitle || ""
        open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 80 : 560, 560)
    modal: true
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.55)
    }

    background: Rectangle {
        radius: 10
        color: dialog.appWindow.surfaceColor
        border.width: 1
        border.color: dialog.lineColor
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 12
            Layout.topMargin: 16
            Layout.bottomMargin: 12
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: "Download mod file"
                font.pixelSize: 17
                font.weight: Font.DemiBold
                color: dialog.appWindow.textColor
            }

            Rectangle {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                radius: 7
                color: closeMouse.containsMouse
                    ? Qt.rgba(dialog.appWindow.textColor.r, dialog.appWindow.textColor.g,
                              dialog.appWindow.textColor.b, 0.08)
                    : "transparent"

                Image {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                    sourceSize: Qt.size(14, 14)
                    smooth: true
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: dialog.appWindow.textColor
                    }
                }

                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: dialog.close()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: dialog.lineColor
            opacity: 0.6
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.topMargin: 16
            Layout.bottomMargin: 12
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: dialog.modName
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: dialog.appWindow.textColor
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                visible: dialog.fileName.length > 0 && dialog.fileName !== dialog.modName
                text: dialog.fileName
                font.pixelSize: 12
                color: Qt.rgba(dialog.appWindow.textColor.r, dialog.appWindow.textColor.g,
                               dialog.appWindow.textColor.b, 0.6)
                elide: Text.ElideRight
            }
        }

        Flickable {
            id: requirementsScroll
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 16
            Layout.preferredHeight: Math.min(requirementsCard.height, dialog.maxScrollHeight)
            contentWidth: width
            contentHeight: requirementsCard.height
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
                    const maxY = Math.max(0, requirementsScroll.contentHeight - requirementsScroll.height)
                    const targetY = Math.max(0, Math.min(maxY,
                        requirementsScroll.scrollTarget - event.angleDelta.y * 3.5))
                    if (targetY === requirementsScroll.scrollTarget)
                        return
                    requirementsScroll.scrollTarget = targetY
                    const distance = Math.abs(targetY - requirementsScroll.contentY)
                    requirementsScrollAnim.stop()
                    requirementsScrollAnim.to = targetY
                    requirementsScrollAnim.duration = Math.max(160, Math.min(500, distance * 0.35))
                    requirementsScrollAnim.start()
                }
            }

            NumberAnimation {
                id: requirementsScrollAnim
                target: requirementsScroll
                property: "contentY"
                easing.type: Easing.OutQuart
            }

            Rectangle {
                id: requirementsCard
                width: requirementsScroll.width
                height: requirementsContent.implicitHeight + 28
                radius: 8
                color: Qt.rgba(dialog.appWindow.textColor.r, dialog.appWindow.textColor.g,
                               dialog.appWindow.textColor.b, 0.035)
                border.width: 1
                border.color: Qt.rgba(dialog.appWindow.textColor.r, dialog.appWindow.textColor.g,
                                      dialog.appWindow.textColor.b, 0.09)

                Column {
                    id: requirementsContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 14
                    spacing: 8

                    Text {
                        width: parent.width
                        text: "Mod file requirements"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: dialog.appWindow.textColor
                    }

                    RowLayout {
                        width: parent.width
                        spacing: 10
                        visible: dialog.loading

                        BusyRing {
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            running: dialog.loading
                            ringColor: dialog.appWindow.primaryColor
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Loading requirements from Nexus..."
                            font.pixelSize: 12
                            color: Qt.rgba(dialog.appWindow.textColor.r, dialog.appWindow.textColor.g,
                                           dialog.appWindow.textColor.b, 0.6)
                        }
                    }

                    Text {
                        width: parent.width
                        visible: !dialog.loading && !dialog.hasRequirements
                        text: dialog.failed
                            ? "Could not load requirements from Nexus. You can still download the file."
                            : "No additional requirements for this file."
                        font.pixelSize: 12
                        color: Qt.rgba(dialog.appWindow.textColor.r, dialog.appWindow.textColor.g,
                                       dialog.appWindow.textColor.b, 0.6)
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width - 10
                        x: 10
                        visible: dialog.nexusRows.length > 0
                        text: "On Nexus"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: dialog.appWindow.textColor
                    }

                    NexusRequirementTable {
                        width: parent.width
                        visible: dialog.nexusRows.length > 0
                        appWindow: dialog.appWindow
                        rows: dialog.nexusRows
                    }

                    Text {
                        width: parent.width - 10
                        x: 10
                        visible: dialog.offsiteRows.length > 0
                        text: "Off-site requirements"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: dialog.appWindow.textColor
                    }

                    NexusRequirementTable {
                        width: parent.width
                        visible: dialog.offsiteRows.length > 0
                        appWindow: dialog.appWindow
                        rows: dialog.offsiteRows
                    }

                    Text {
                        width: parent.width - 10
                        x: 10
                        visible: dialog.dlcRows.length > 0
                        text: "DLC requirements"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: dialog.appWindow.textColor
                    }

                    NexusRequirementTable {
                        width: parent.width
                        visible: dialog.dlcRows.length > 0
                        appWindow: dialog.appWindow
                        rows: dialog.dlcRows
                        nameHeader: "DLC name"
                        notesHeader: ""
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: "transparent"

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: dialog.lineColor
                opacity: 0.6
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 10

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredWidth: 128
                    Layout.preferredHeight: 36
                    radius: 8
                    color: downloadMouse.containsMouse
                        ? Qt.lighter(dialog.appWindow.primaryColor, 1.08)
                        : dialog.appWindow.primaryColor

                    Text {
                        anchors.centerIn: parent
                        text: "Download"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: dialog.appWindow.relativeLuminance(dialog.appWindow.primaryColor) > 0.5
                            ? "#1A1A1A" : "#FFFFFF"
                    }

                    MouseArea {
                        id: downloadMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            dialog.close()
                            dialog.downloadConfirmed()
                        }
                    }
                }
            }
        }
    }
}
