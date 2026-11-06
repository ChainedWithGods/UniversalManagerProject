import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Rectangle {
    id: fileRow

    required property string name
    required property string fileSize
    required property string fileExtension
    required property string fileType
    required property string modifiedDate
    required property string filePath

    property var metrics: null
    property bool showActiveColumns: true

    readonly property int sizeColumnWidth: metrics ? metrics.size : 82
    readonly property int statusColumnWidth: metrics ? metrics.status : 154
    readonly property int speedColumnWidth: metrics ? metrics.speed : 88
    readonly property int etaColumnWidth: metrics ? metrics.eta : 94
    readonly property int dateColumnWidth: metrics ? metrics.date : 94
    readonly property int actionsColumnWidth: metrics ? metrics.actions : 104

    readonly property bool isModFile: fileType === "ue_mod" || fileType === "ue_mod_encrypted"
    readonly property var nexusUpdate: {
        var revision = modManager.nexusUpdatesRevision
        return modManager.nexusUpdateForArchive(filePath)
    }
    readonly property var nexusUpdateTarget: {
        var revision = modManager.nexusUpdatesRevision
        return modManager.nexusUpdateTargetForArchive(filePath)
    }
    readonly property color typeColor: {
        if (fileType === "ue_mod_encrypted") return "#F97316"
        if (fileType === "ue_mod") return "#8B5CF6"
        return window.primaryColor
    }

    width: parent ? parent.width : 0
    height: 58
    color: hoverArea.containsMouse
        ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.035)
        : "transparent"

    Behavior on color { ColorAnimation { duration: 100 } }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        z: -1
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 60
            spacing: 10

            Rectangle {
                width: 28
                height: 28
                radius: 8
                color: "transparent"

                Item {
                    anchors.centerIn: parent
                    width: 14
                    height: 14

                    Image {
                        id: fileIcon
                        anchors.fill: parent
                        source: fileRow.fileType === "archive"
                            ? "qrc:/UniversalModManager/Lucide Icons/archive.svg"
                            : (fileRow.fileType === "ue_mod_encrypted"
                                ? "qrc:/UniversalModManager/Lucide Icons/lock.svg"
                                : (fileRow.fileType === "ue_mod"
                                    ? "qrc:/UniversalModManager/Lucide Icons/file-box.svg"
                                    : "qrc:/UniversalModManager/Lucide Icons/file.svg"))
                        sourceSize: Qt.size(15, 15)
                        visible: modManager.theme === "Cozy"
                    }

                    ColorOverlay {
                        anchors.fill: fileIcon
                        source: fileIcon
                        color: fileRow.typeColor
                        visible: modManager.theme !== "Cozy"
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: fileRow.name
                font.pixelSize: 12
                font.weight: Font.DemiBold
                color: window.textColor
                elide: Text.ElideRight
            }

            Rectangle {
                visible: fileRow.fileExtension.length > 0
                width: extensionLabel.implicitWidth + 10
                height: 16
                radius: 4
                color: Qt.rgba(fileRow.typeColor.r, fileRow.typeColor.g, fileRow.typeColor.b, 0.13)

                Text {
                    id: extensionLabel
                    anchors.centerIn: parent
                    text: fileRow.fileExtension.toUpperCase()
                    font.pixelSize: 8
                    font.weight: Font.Bold
                    color: fileRow.typeColor
                }
            }

            UpdateAvailableBadge {
                visible: fileRow.nexusUpdate.available === true
                compact: true
                tooltipText: "Click to download version " + (fileRow.nexusUpdate.latestVersion || "update")
                onClicked: modManager.downloadNexusMod(fileRow.nexusUpdate.modId,
                                                        fileRow.nexusUpdate.latestFileId,
                                                        fileRow.nexusUpdate.name)
            }
        }

        Text {
            Layout.preferredWidth: fileRow.sizeColumnWidth
            Layout.minimumWidth: fileRow.sizeColumnWidth
            Layout.maximumWidth: fileRow.sizeColumnWidth
            text: fileRow.fileSize
            font.pixelSize: 11
            color: window.textSecondaryColor
            elide: Text.ElideRight
        }

        Text {
            Layout.preferredWidth: fileRow.statusColumnWidth
            Layout.minimumWidth: fileRow.statusColumnWidth
            Layout.maximumWidth: fileRow.statusColumnWidth
            text: "Completed"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: "#22C55E"
        }

        Text {
            visible: fileRow.showActiveColumns
            Layout.preferredWidth: fileRow.speedColumnWidth
            Layout.minimumWidth: fileRow.speedColumnWidth
            Layout.maximumWidth: fileRow.speedColumnWidth
            text: ""
            font.pixelSize: 11
            color: window.textSecondaryColor
        }

        Text {
            visible: fileRow.showActiveColumns
            Layout.preferredWidth: fileRow.etaColumnWidth
            Layout.minimumWidth: fileRow.etaColumnWidth
            Layout.maximumWidth: fileRow.etaColumnWidth
            text: ""
            font.pixelSize: 11
            color: window.textSecondaryColor
        }

        Text {
            Layout.preferredWidth: fileRow.dateColumnWidth
            Layout.minimumWidth: fileRow.dateColumnWidth
            Layout.maximumWidth: fileRow.dateColumnWidth
            text: fileRow.modifiedDate
            font.pixelSize: 11
            color: window.textSecondaryColor
            elide: Text.ElideRight
        }

        Item {
            Layout.preferredWidth: fileRow.actionsColumnWidth
            Layout.minimumWidth: fileRow.actionsColumnWidth
            Layout.maximumWidth: fileRow.actionsColumnWidth
            Layout.fillHeight: true

            RowLayout {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

            Rectangle {
                width: installRow.implicitWidth + 18
                height: 26
                radius: 6
                color: installArea.containsMouse
                    ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.22)
                    : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)
                border.width: 1
                border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.22)

                RowLayout {
                    id: installRow
                    anchors.centerIn: parent
                    spacing: 5

                    Item {
                        width: 12
                        height: 12

                        Image {
                            id: installIcon
                            anchors.fill: parent
                            source: "qrc:/UniversalModManager/Lucide Icons/download.svg"
                            sourceSize: Qt.size(12, 12)
                            visible: modManager.theme === "Cozy"
                        }

                        ColorOverlay {
                            anchors.fill: installIcon
                            source: installIcon
                            color: window.primaryColor
                            visible: modManager.theme !== "Cozy"
                        }
                    }

                    Text {
                        text: fileRow.nexusUpdateTarget.available === true ? "Update" : "Install"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: window.primaryColor
                    }
                }

                MouseArea {
                    id: installArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: modManager.installModsAsync([fileRow.filePath])
                }
            }

            DownloadsActionButton {
                iconSource: "qrc:/UniversalModManager/Lucide Icons/folder-open.svg"
                tooltip: "Open downloads folder"
                onClicked: modManager.openDownloadsFolder()
            }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06)
    }
}
