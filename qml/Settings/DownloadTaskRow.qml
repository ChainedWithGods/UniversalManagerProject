import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Rectangle {
    id: taskRow

    required property int id
    required property string name
    required property string fileName
    required property string downloadState
    required property real progress
    required property double received
    required property double total
    required property double speed
    required property double eta
    required property string error
    required property string filePath
    required property string dateAdded

    property var metrics: null
    property string searchQuery: ""
    property string category: "all"
    property bool showActiveColumns: true

    readonly property int sizeColumnWidth: metrics ? metrics.size : 82
    readonly property int statusColumnWidth: metrics ? metrics.status : 154
    readonly property int speedColumnWidth: metrics ? metrics.speed : 88
    readonly property int etaColumnWidth: metrics ? metrics.eta : 94
    readonly property int dateColumnWidth: metrics ? metrics.date : 94
    readonly property int actionsColumnWidth: metrics ? metrics.actions : 104

    readonly property bool isActive: downloadState === "downloading"
        || downloadState === "paused" || downloadState === "queued"
    readonly property bool matchesFilter: downloadState !== "completed"
        && (category === "all" || category === "unfinished")
        && (searchQuery.length === 0 || name.toLowerCase().indexOf(searchQuery.toLowerCase()) !== -1)
    readonly property color statusColor: {
        if (downloadState === "downloading") return window.primaryColor
        if (downloadState === "paused") return "#F59E0B"
        if (downloadState === "queued") return "#94A3B8"
        if (downloadState === "failed") return "#EF4444"
        return "#22C55E"
    }

    visible: matchesFilter
    width: parent ? parent.width : 0
    height: 62
    color: hoverArea.containsMouse
        ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.035)
        : "transparent"

    Behavior on color { ColorAnimation { duration: 100 } }

    function formatBytes(value) {
        var bytes = Math.max(0, Number(value) || 0)
        if (bytes < 1024) return Math.round(bytes) + " B"
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1073741824) return (bytes / 1048576).toFixed(1) + " MB"
        return (bytes / 1073741824).toFixed(2) + " GB"
    }

    function formatSpeed(value) {
        var valueSpeed = Number(value) || 0
        return valueSpeed > 0 ? formatBytes(valueSpeed) + "/s" : "—"
    }

    function formatEta(seconds) {
        if (seconds < 0) return "—"
        var value = Math.max(0, Math.round(Number(seconds) || 0))
        if (value < 60) return value + "s"
        if (value < 3600) return Math.floor(value / 60) + "m " + (value % 60) + "s"
        return Math.floor(value / 3600) + "h " + Math.floor((value % 3600) / 60) + "m"
    }

    function stateLabel() {
        if (downloadState === "downloading") return "Downloading"
        if (downloadState === "paused") return "Paused"
        if (downloadState === "queued") return "Queued"
        if (downloadState === "failed") return "Failed"
        return "Completed"
    }

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
                width: 30
                height: 30
                radius: 8
                color: "transparent"

                Canvas {
                    id: downloadSpinner
                    anchors.centerIn: parent
                    width: 28
                    height: 28
                    visible: taskRow.downloadState === "downloading"

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle = taskRow.statusColor
                        ctx.lineWidth = 2
                        ctx.lineCap = "round"
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, width / 2 - 2,
                                -Math.PI / 2, Math.PI * 0.9)
                        ctx.stroke()
                    }

                    RotationAnimator on rotation {
                        running: downloadSpinner.visible && !window.focusAnimationsPaused
                        from: 0
                        to: 360
                        duration: 1200
                        loops: Animation.Infinite
                        onRunningChanged: if (!running) downloadSpinner.rotation = 0
                    }
                }

                Connections {
                    target: taskRow
                    function onStatusColorChanged() { downloadSpinner.requestPaint() }
                }

                Item {
                    anchors.centerIn: parent
                    width: 15
                    height: 15

                    Image {
                        id: stateIcon
                        anchors.fill: parent
                        source: taskRow.downloadState === "failed"
                            ? "qrc:/UniversalModManager/Lucide Icons/x.svg"
                            : (taskRow.downloadState === "paused"
                                ? "qrc:/UniversalModManager/Lucide Icons/pause.svg"
                                : "qrc:/UniversalModManager/Lucide Icons/arrow-down-to-line.svg")
                        sourceSize: Qt.size(16, 16)
                        visible: modManager.theme === "Cozy"
                    }

                    ColorOverlay {
                        anchors.fill: stateIcon
                        source: stateIcon
                        color: taskRow.statusColor
                        visible: modManager.theme !== "Cozy"
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Text {
                    Layout.fillWidth: true
                    text: taskRow.name
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: window.textColor
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    visible: taskRow.error.length > 0
                        || taskRow.fileName.length > 0 && taskRow.fileName !== taskRow.name
                    text: taskRow.error.length > 0 ? taskRow.error : taskRow.fileName
                    font.pixelSize: 10
                    color: taskRow.error.length > 0
                        ? (taskRow.downloadState === "failed" ? "#EF4444" : "#F59E0B")
                        : window.textSecondaryColor
                    elide: Text.ElideRight
                }
            }
        }

        Text {
            Layout.preferredWidth: taskRow.sizeColumnWidth
            Layout.minimumWidth: taskRow.sizeColumnWidth
            Layout.maximumWidth: taskRow.sizeColumnWidth
            text: taskRow.total > 0 ? taskRow.formatBytes(taskRow.total) : "—"
            font.pixelSize: 11
            color: window.textSecondaryColor
            elide: Text.ElideRight
        }

        ColumnLayout {
            Layout.preferredWidth: taskRow.statusColumnWidth
            Layout.minimumWidth: taskRow.statusColumnWidth
            Layout.maximumWidth: taskRow.statusColumnWidth
            spacing: 5

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: taskRow.stateLabel() + ((taskRow.downloadState === "failed"
                        || taskRow.downloadState === "queued")
                        ? ""
                        : " · " + Math.round(Math.max(0, Math.min(1, taskRow.progress)) * 100) + "%")
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: taskRow.statusColor
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 4
                radius: 2
                visible: taskRow.downloadState !== "failed"
                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08)

                Rectangle {
                    id: progressFill
                    width: parent.width * Math.max(0, Math.min(1, taskRow.progress))
                    height: parent.height
                    radius: parent.radius
                    color: taskRow.statusColor
                    clip: true

                    Behavior on width { NumberAnimation { duration: 150 } }

                    Rectangle {
                        width: Math.max(18, progressFill.width * 0.35)
                        height: parent.height
                        visible: taskRow.downloadState === "downloading"
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0) }
                            GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.8) }
                            GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0) }
                        }

                        NumberAnimation on x {
                            running: taskRow.downloadState === "downloading"
                                && !window.focusAnimationsPaused
                            from: -80
                            to: progressFill.width + 20
                            duration: 1500
                            loops: Animation.Infinite
                            easing.type: Easing.InOutSine
                        }
                    }
                }
            }
        }

        Text {
            visible: taskRow.showActiveColumns
            Layout.preferredWidth: taskRow.speedColumnWidth
            Layout.minimumWidth: taskRow.speedColumnWidth
            Layout.maximumWidth: taskRow.speedColumnWidth
            text: taskRow.downloadState === "downloading" ? taskRow.formatSpeed(taskRow.speed) : "—"
            font.pixelSize: 11
            color: window.textSecondaryColor
        }

        Text {
            visible: taskRow.showActiveColumns
            Layout.preferredWidth: taskRow.etaColumnWidth
            Layout.minimumWidth: taskRow.etaColumnWidth
            Layout.maximumWidth: taskRow.etaColumnWidth
            text: taskRow.downloadState === "downloading" ? taskRow.formatEta(taskRow.eta) : "—"
            font.pixelSize: 11
            color: window.textSecondaryColor
        }

        Text {
            Layout.preferredWidth: taskRow.dateColumnWidth
            Layout.minimumWidth: taskRow.dateColumnWidth
            Layout.maximumWidth: taskRow.dateColumnWidth
            text: taskRow.dateAdded.length > 0 ? taskRow.dateAdded : "—"
            font.pixelSize: 11
            color: window.textSecondaryColor
        }

        Item {
            Layout.preferredWidth: taskRow.actionsColumnWidth
            Layout.minimumWidth: taskRow.actionsColumnWidth
            Layout.maximumWidth: taskRow.actionsColumnWidth
            Layout.fillHeight: true

            RowLayout {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

            DownloadsActionButton {
                visible: taskRow.downloadState === "downloading"
                iconSource: "qrc:/UniversalModManager/Lucide Icons/pause.svg"
                tooltip: "Pause"
                tint: window.primaryColor
                onClicked: downloadManager.pause(taskRow.id)
            }

            DownloadsActionButton {
                visible: taskRow.downloadState === "paused"
                iconSource: "qrc:/UniversalModManager/Lucide Icons/play.svg"
                tooltip: "Resume"
                tint: window.primaryColor
                onClicked: downloadManager.resume(taskRow.id)
            }

            DownloadsActionButton {
                visible: taskRow.isActive
                iconSource: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                tooltip: "Cancel"
                danger: true
                onClicked: downloadManager.cancel(taskRow.id)
            }

            DownloadsActionButton {
                visible: taskRow.downloadState === "failed"
                iconSource: "qrc:/UniversalModManager/Lucide Icons/folder-open.svg"
                tooltip: "Open folder"
                onClicked: downloadManager.openFolder(taskRow.id)
            }

            DownloadsActionButton {
                visible: taskRow.downloadState === "failed"
                iconSource: "qrc:/UniversalModManager/Lucide Icons/trash-2.svg"
                tooltip: "Remove"
                danger: true
                onClicked: downloadManager.removeEntry(taskRow.id)
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
