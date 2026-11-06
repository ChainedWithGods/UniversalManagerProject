import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: filesTab
    property string filesSortField: "date"
    property bool filesSortDesc: true
    readonly property var entries: groupedFiles()
    readonly property int dateColumnWidth: 135
    readonly property int sizeColumnWidth: 70
    readonly property int versionColumnWidth: 55

    readonly property var pendingScanStatuses: ({
        "QUEUED": true,
        "WAITING_REPORT": true
    })
    readonly property var failedScanStatuses: ({
        "QUARANTINED": true,
        "REPORT_ERROR": true,
        "FILE_NOT_FOUND": true,
        "MOD_DOES_NOT_EXIST": true
    })

    function scanStatus(file) {
        return String((file && file.virusScanStatus) || "").toUpperCase()
    }

    function isScanPending(file) {
        return pendingScanStatuses[scanStatus(file)] === true
    }

    function isScanBlocked(file) {
        var status = scanStatus(file)
        return pendingScanStatuses[status] === true || failedScanStatuses[status] === true
    }

    function scanBlockMessage(file) {
        if (isScanPending(file))
            return "This file is not yet available for download. Virus scanning is in progress."
        if (failedScanStatuses[scanStatus(file)] === true)
            return "This file is not available for download."
        return ""
    }

    readonly property var hostWindow: Window.window

    property int pendingModId: 0
    property int pendingFileId: 0
    property string pendingFileName: ""
    property string pendingMode: "manager"

    function requestDownload(modId, fileId, fileName, mode) {
        var files = modManager.currentNexusModFiles || []
        for (var i = 0; i < files.length; ++i) {
            if (Number(files[i].fileId) === Number(fileId) && isScanBlocked(files[i]))
                return
        }
        if (mode === "manager" && String(modManager.downloadsPath || "").length === 0) {
            modManager.requestDownloadsPathSetup()
            return
        }
        pendingModId = modId
        pendingFileId = fileId || 0
        pendingFileName = fileName || ""
        pendingMode = mode
        downloadRequirementsDialog.openFor(Window.window.modData.name || "", pendingFileName)
    }

    function formatFileDate(timestamp) {
        if (!timestamp || timestamp <= 0)
            return ""
        var date = new Date(timestamp * 1000)
        var months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
        var day = date.getDate()
        var hours = date.getHours()
        var suffix = hours >= 12 ? "PM" : "AM"
        hours = hours % 12
        if (hours === 0)
            hours = 12
        var minutes = date.getMinutes()
        return (day < 10 ? "0" + day : "" + day) + " " + months[date.getMonth()] + " "
            + date.getFullYear() + ", " + hours + ":"
            + (minutes < 10 ? "0" + minutes : minutes) + suffix
    }

    function plainFileDescription(text) {
        var s = "" + (text || "")
        s = s.replace(/<br\s*\/?>/gi, "\n")
        s = s.replace(/<[^>]+>/g, "")
        s = s.replace(/\[\/?(?:b|i|u|s|url|img|quote|spoiler|list|color|size|font|center|left|right|code)[^\]]*\]/gi, "")
        s = s.replace(/&nbsp;/gi, " ").replace(/&amp;/gi, "&")
            .replace(/&lt;/gi, "<").replace(/&gt;/gi, ">").replace(/&quot;/gi, "\"")
        return s.trim()
    }

    function normalizedFileCategory(category) {
        return ("" + (category || "MISC")).toUpperCase().replace(/[\s-]+/g, "_")
    }

    function fileCategoryTitle(key) {
        switch (key) {
        case "MAIN": return "Main files"
        case "UPDATE": return "Update files"
        case "OPTIONAL": return "Optional files"
        case "OLD_VERSION": return "Old files"
        case "MISCELLANEOUS":
        case "MISC": return "Miscellaneous"
        case "ARCHIVED": return "Archived"
        }
        return key.replace(/_/g, " ").replace(/\b\w/g, function(c) { return c.toUpperCase() })
    }

    function groupedFiles() {
        var files = modManager.currentNexusModFiles || []
        var groups = ({})
        var order = []
        for (var i = 0; i < files.length; ++i) {
            var file = files[i]
            if (!file)
                continue
            var key = normalizedFileCategory(file.category)
            if (!groups[key]) {
                groups[key] = []
                order.push(key)
            }
            groups[key].push(file)
        }
        var rank = { "MAIN": 0, "UPDATE": 1, "OPTIONAL": 2,
            "MISC": 3, "MISCELLANEOUS": 3, "OLD_VERSION": 4, "ARCHIVED": 5 }
        order.sort(function(a, b) {
            var ra = rank[a] === undefined ? 9 : rank[a]
            var rb = rank[b] === undefined ? 9 : rank[b]
            return ra - rb
        })
        var field = filesSortField
        var descending = filesSortDesc
        var sortValue = function(file) {
            switch (field) {
            case "name": return (file.name || "").toLowerCase()
            case "size": return file.sizeKb || 0
            default: return file.uploadedTimestamp || 0
            }
        }
        for (var key in groups) {
            groups[key].sort(function(a, b) {
                var va = sortValue(a)
                var vb = sortValue(b)
                if (va === vb)
                    return 0
                var result = va < vb ? -1 : 1
                return descending ? -result : result
            })
        }

        var entries = []
        for (var g = 0; g < order.length; ++g) {
            var groupKey = order[g]
            entries.push({ kind: "header", title: fileCategoryTitle(groupKey) })
            for (var n = 0; n < groups[groupKey].length; ++n)
                entries.push({ kind: "file", file: groups[groupKey][n] })
        }
        return entries
    }

    Flickable {
        id: filesFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: filesCenteredWrapper.implicitHeight + 60
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        property real scrollTarget: 0

        onContentYChanged: {
            if (!filesScrollAnim.running)
                scrollTarget = contentY
        }

        onMovementStarted: filesScrollAnim.stop()

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: (event) => {
                event.accepted = true
                const maxY = Math.max(0, filesFlick.contentHeight - filesFlick.height)
                const targetY = Math.max(0, Math.min(maxY,
                    filesFlick.scrollTarget - event.angleDelta.y * 3.5))
                if (targetY === filesFlick.scrollTarget)
                    return
                filesFlick.scrollTarget = targetY
                const distance = Math.abs(targetY - filesFlick.contentY)
                filesScrollAnim.stop()
                filesScrollAnim.to = targetY
                filesScrollAnim.duration = Math.max(160, Math.min(500, distance * 0.35))
                filesScrollAnim.start()
            }
        }

        NumberAnimation {
            id: filesScrollAnim
            target: filesFlick
            property: "contentY"
            easing.type: Easing.OutQuart
        }

        Item {
            id: filesCenteredWrapper
            width: parent.width
            anchors.horizontalCenter: parent.horizontalCenter
            implicitHeight: filesCol.implicitHeight + 40

            ColumnLayout {
                id: filesCol
                width: Math.min(parent.width - 48, Window.window.contentMaxWidth + 36)
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 20
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: "Files available for download (" + Window.window.activeFileCount + ")"
                        font.pixelSize: 15
                        font.weight: Font.Bold
                        color: Window.window.textColor
                    }

                    Item { Layout.fillWidth: true }

                    BusyIndicator {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        visible: modManager.isLoadingNexusModFiles
                        running: visible
                    }

                    Text {
                        text: "Sort by"
                        font.pixelSize: 11
                        color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.75)
                    }

                    NexusSortPill {
                        options: [
                            { label: "Date uploaded", value: "date" },
                            { label: "File name", value: "name" },
                            { label: "File size", value: "size" }
                        ]
                        currentIndex: filesSortField === "name" ? 1
                            : (filesSortField === "size" ? 2 : 0)
                        onSelected: (index) => filesSortField = options[index].value
                    }

                    NexusSortPill {
                        options: [
                            { label: "Desc", value: true },
                            { label: "Asc", value: false }
                        ]
                        currentIndex: filesSortDesc ? 0 : 1
                        onSelected: (index) => filesSortDesc = options[index].value
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: filesCardColumn.implicitHeight + 16
                    radius: 8
                    color: Window.window.panelColor(Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.02))
                    border.width: 1
                    border.color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.14)

                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: (modManager.theme === "Crimson" || modManager.theme === "Wavey")
                            ? Qt.rgba(0, 0, 0, 0.28)
                            : "transparent"
                    }

                    Column {
                        id: filesCardColumn
                        anchors.top: parent.top
                        anchors.topMargin: 8
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.min(parent.width - 28, Window.window.contentMaxWidth)
                        spacing: 0

                        Text {
                            width: parent.width
                            visible: filesTab.entries.length === 0 && !modManager.isLoadingNexusModFiles
                            text: "No files found for this mod."
                            font.pixelSize: 12
                            color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.8)
                            topPadding: 8
                            bottomPadding: 12
                            leftPadding: 14
                            rightPadding: 14
                        }

                        Repeater {
                            model: filesTab.entries

                            delegate: Item {
                                id: fileEntry
                                required property var modelData
                                required property int index

                                readonly property bool isHeader: modelData.kind === "header"
                                readonly property var fileData: isHeader ? ({}) : (modelData.file || ({}))
                                readonly property string descriptionText: isHeader
                                    ? "" : plainFileDescription(fileData.description || "")
                                readonly property string fileScanStatus: isHeader
                                    ? "" : filesTab.scanStatus(fileData)
                                readonly property bool scanPending: isHeader
                                    ? false : filesTab.isScanPending(fileData)
                                readonly property bool scanBlocked: isHeader
                                    ? false : filesTab.isScanBlocked(fileData)
                                readonly property string scanMessage: isHeader
                                    ? "" : filesTab.scanBlockMessage(fileData)

                                width: filesCardColumn.width
                                height: isHeader
                                    ? fileHeaderContent.implicitHeight + (index === 0 ? 18 : 38)
                                    : fileEntryLayout.implicitHeight + 28

                                HoverHandler {
                                    id: fileRowHover
                                    enabled: !fileEntry.isHeader
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    visible: !fileEntry.isHeader
                                    color: fileRowHover.hovered
                                        ? Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.035)
                                        : "transparent"

                                    Behavior on color { ColorAnimation { duration: 90 } }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        height: 1
                                        color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.12)
                                    }
                                }

                                Column {
                                    id: fileHeaderContent
                                    visible: fileEntry.isHeader
                                    width: parent.width
                                    y: fileEntry.index === 0 ? 8 : 26
                                    spacing: 10

                                    Text {
                                        x: 14
                                        text: fileEntry.isHeader ? (fileEntry.modelData.title || "") : ""
                                        font.pixelSize: 17
                                        font.weight: Font.DemiBold
                                        color: Window.window.textColor
                                    }

                                    Item {
                                        x: 14
                                        width: parent.width - 28
                                        height: fileColumnLabels.implicitHeight + 10

                                        RowLayout {
                                            id: fileColumnLabels
                                            width: parent.width
                                            spacing: 0

                                            Text {
                                                Layout.fillWidth: true
                                                text: "File name"
                                                font.pixelSize: 10
                                                font.weight: Font.DemiBold
                                                color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.55)
                                            }
                                            Text {
                                                Layout.preferredWidth: filesTab.dateColumnWidth
                                                text: "Date uploaded"
                                                font.pixelSize: 10
                                                font.weight: Font.DemiBold
                                                color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.55)
                                            }
                                            Text {
                                                Layout.preferredWidth: filesTab.sizeColumnWidth
                                                text: "File size"
                                                font.pixelSize: 10
                                                font.weight: Font.DemiBold
                                                color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.55)
                                            }
                                            Text {
                                                Layout.preferredWidth: filesTab.versionColumnWidth
                                                text: "Version"
                                                font.pixelSize: 10
                                                font.weight: Font.DemiBold
                                                color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.55)
                                            }
                                        }

                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            height: 1
                                            color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.14)
                                        }
                                    }
                                }

                                Column {
                                    id: fileEntryLayout
                                    visible: !fileEntry.isHeader
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 14
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4

                                    RowLayout {
                                        width: parent.width
                                        spacing: 0

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8

                                            Rectangle {
                                                id: virusScanBadge
                                                z: 10
                                                Layout.alignment: Qt.AlignVCenter
                                                readonly property bool hasScanReport: !fileEntry.isHeader
                                                    && String(fileEntry.fileData.virusScanUrl || "").length > 0
                                                readonly property bool hasScanInfo: !fileEntry.isHeader
                                                    && (fileEntry.fileScanStatus.length > 0 || hasScanReport)
                                                visible: hasScanInfo
                                                width: 16
                                                height: 16
                                                radius: 8
                                                color: {
                                                    if (fileEntry.scanPending)
                                                        return virusScanMouse.containsMouse ? "#F7C34D" : "#F0B232"
                                                    if (fileEntry.scanBlocked)
                                                        return virusScanMouse.containsMouse ? "#E74C3C" : "#D64545"
                                                    return virusScanMouse.containsMouse ? "#3DDC84" : "#2ECC71"
                                                }

                                                Behavior on color { ColorAnimation { duration: 100 } }

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: fileEntry.scanPending ? "?" : (fileEntry.scanBlocked ? "✕" : "✔")
                                                    font.pixelSize: fileEntry.scanPending ? 10 : 9
                                                    font.weight: Font.Bold
                                                    color: "#FFFFFF"
                                                }

                                                MouseArea {
                                                    id: virusScanMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: (fileEntry.scanBlocked || !virusScanBadge.hasScanReport)
                                                        ? Qt.ArrowCursor : Qt.PointingHandCursor
                                                    onClicked: {
                                                        if (!fileEntry.scanBlocked && fileEntry.fileData.virusScanUrl)
                                                            Window.window.handleDescriptionLink(fileEntry.fileData.virusScanUrl)
                                                    }
                                                }

                                                Rectangle {
                                                    visible: virusScanMouse.containsMouse
                                                    z: 20
                                                    x: -2
                                                    y: -height - 6
                                                    width: virusScanTipText.implicitWidth + 16
                                                    height: virusScanTipText.implicitHeight + 10
                                                    radius: 4
                                                    color: "#F01B1D23"
                                                    border.width: 1
                                                    border.color: "#33FFFFFF"

                                                    Text {
                                                        id: virusScanTipText
                                                        anchors.centerIn: parent
                                                        text: fileEntry.scanPending
                                                            ? "This file has not been scanned for viruses"
                                                            : (fileEntry.scanBlocked
                                                                ? "This file is not available for download"
                                                                : (virusScanBadge.hasScanReport
                                                                    ? "Virus scan report (click to open)"
                                                                    : "Virus scan passed"))
                                                        font.pixelSize: 10
                                                        color: "#F2FFFFFF"
                                                    }
                                                }
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: fileEntry.fileData.name || "Unnamed file"
                                                font.pixelSize: 13
                                                font.weight: Font.DemiBold
                                                color: Window.window.textColor
                                                elide: Text.ElideRight
                                            }
                                        }

                                        Text {
                                            Layout.preferredWidth: filesTab.dateColumnWidth
                                            text: formatFileDate(fileEntry.fileData.uploadedTimestamp || 0)
                                            font.pixelSize: 11
                                            color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.78)
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.preferredWidth: filesTab.sizeColumnWidth
                                            text: {
                                                var kb = fileEntry.fileData.sizeKb || 0
                                                if (kb >= 1024)
                                                    return (kb / 1024.0).toFixed(1) + " MB"
                                                return Math.round(kb) + " KB"
                                            }
                                            font.pixelSize: 11
                                            color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.78)
                                        }

                                        Text {
                                            Layout.preferredWidth: filesTab.versionColumnWidth
                                            text: fileEntry.fileData.version || ""
                                            font.pixelSize: 11
                                            color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.78)
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Text {
                                        width: parent.width
                                        visible: fileEntry.descriptionText.length > 0
                                        text: fileEntry.descriptionText
                                        font.pixelSize: 12
                                        wrapMode: Text.Wrap
                                        color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.9)
                                    }

                                    Text {
                                        width: parent.width
                                        visible: fileEntry.scanMessage.length > 0
                                        text: fileEntry.scanMessage
                                        font.pixelSize: 12
                                        font.italic: true
                                        wrapMode: Text.Wrap
                                        topPadding: 2
                                        bottomPadding: 2
                                        color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.9)
                                    }

                                    RowLayout {
                                        width: parent.width
                                        spacing: 8
                                        visible: fileEntry.scanMessage.length === 0

                                        Rectangle {
                                            Layout.preferredHeight: 30
                                            Layout.preferredWidth: fileManagerLabel.implicitWidth + 26
                                            radius: 5
                                            color: fileManagerMouse.containsMouse
                                                ? Qt.lighter(Window.window.primaryColor, 1.08)
                                                : Window.window.primaryColor

                                            Behavior on color { ColorAnimation { duration: 100 } }

                                            Text {
                                                id: fileManagerLabel
                                                anchors.centerIn: parent
                                                text: "MOD MANAGER DOWNLOAD"
                                                font.pixelSize: 10
                                                font.weight: Font.Bold
                                                color: Window.window.relativeLuminance(Window.window.primaryColor) > 0.5
                                                    ? "#1A1A1A" : "#FFFFFF"
                                            }

                                            MouseArea {
                                                id: fileManagerMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    var modId = Window.window.modData.modId
                                                    var fileId = fileEntry.fileData.fileId || 0
                                                    var name = fileEntry.fileData.name || Window.window.modData.name
                                                    filesTab.requestDownload(modId, fileId, name, "manager")
                                                }
                                            }
                                        }

                                        Rectangle {
                                            Layout.preferredHeight: 30
                                            Layout.preferredWidth: fileManualLabel.implicitWidth + 26
                                            radius: 5
                                            color: fileManualMouse.containsMouse
                                                ? Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.12)
                                                : Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.05)
                                            border.width: 1
                                            border.color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.22)

                                            Behavior on color { ColorAnimation { duration: 100 } }

                                            Text {
                                                id: fileManualLabel
                                                anchors.centerIn: parent
                                                text: "MANUAL DOWNLOAD"
                                                font.pixelSize: 10
                                                font.weight: Font.Bold
                                                color: Window.window.textColor
                                            }

                                            MouseArea {
                                                id: fileManualMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: filesTab.requestDownload(
                                                    Window.window.modData.modId,
                                                    fileEntry.fileData.fileId || 0,
                                                    fileEntry.fileData.name || Window.window.modData.name,
                                                    "manual")
                                            }
                                        }

                                        Item { Layout.fillWidth: true }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    NexusDownloadRequirementsDialog {
        id: downloadRequirementsDialog
        appWindow: filesTab.hostWindow

        onDownloadConfirmed: {
            if (filesTab.pendingMode === "manager") {
                modManager.downloadNexusMod(filesTab.pendingModId, filesTab.pendingFileId,
                                            filesTab.pendingFileName)
            } else {
                Qt.openUrlExternally("https://www.nexusmods.com/"
                    + modManager.nexusGameDomain + "/mods/" + filesTab.pendingModId
                    + "?tab=files&file_id=" + filesTab.pendingFileId)
            }
        }
    }
}
