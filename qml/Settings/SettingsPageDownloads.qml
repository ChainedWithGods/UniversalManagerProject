import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

ColumnLayout {
    id: downloadsPage
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.rightMargin: 10
    spacing: 16
    visible: generalSectionIndex === 5

    property string searchText: ""
    property string category: "all"

    property var filteredFiles: []
    property int unfinishedCount: 0
    property int visibleTaskCount: 0
    property int archiveCount: 0
    property int modFileCount: 0
    property int modUpdatesCount: 0
    property int downloadedModsCount: 0
    property real totalSpeed: 0

    readonly property bool pathConfigured: (modManager.downloadsPath || "").length > 0
    readonly property int finishedCount: modManager.downloadedModsCount
    readonly property int allCount: unfinishedCount + finishedCount
    readonly property int visibleRows: visibleTaskCount + filteredFiles.length
    readonly property bool hasRows: visibleRows > 0
    readonly property bool showActiveColumns: (category === "all" || category === "unfinished") && unfinishedCount > 0

    readonly property color dividerColor: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08)

    readonly property string categoryTitle: {
        switch (category) {
        case "unfinished": return "Unfinished downloads"
        case "finished": return "Finished downloads"
        case "modupdates": return "Mod updates"
        case "moddownloads": return "Downloaded mods"
        case "archives": return "Archives"
        case "modfiles": return "Mod files"
        }
        return "All downloads"
    }

    readonly property int categoryTotal: {
        switch (category) {
        case "unfinished": return unfinishedCount
        case "finished": return finishedCount
        case "modupdates": return modUpdatesCount
        case "moddownloads": return downloadedModsCount
        case "archives": return archiveCount
        case "modfiles": return modFileCount
        }
        return allCount
    }

    readonly property string categorySubtitle: {
        var totalText = categoryTotal === 1 ? "1 download" : categoryTotal + " downloads"
        if (searchText.length > 0)
            return visibleRows + " of " + totalText
        return totalText
    }

    function formatSpeed(value) {
        var speedValue = Number(value) || 0
        if (speedValue <= 0) return ""
        if (speedValue < 1024) return Math.round(speedValue) + " B/s"
        if (speedValue < 1048576) return (speedValue / 1024).toFixed(1) + " KB/s"
        if (speedValue < 1073741824) return (speedValue / 1048576).toFixed(1) + " MB/s"
        return (speedValue / 1073741824).toFixed(2) + " GB/s"
    }

    function refreshTasks() {
        var tasks = downloadManager.snapshot()
        var query = searchText.toLowerCase()
        var showTasks = category === "all" || category === "unfinished"
        var unfinished = 0
        var matching = 0
        var speed = 0

        for (var i = 0; i < tasks.length; i++) {
            var task = tasks[i]
            if (task.state === "completed")
                continue

            unfinished++
            if (task.state === "downloading")
                speed += Number(task.speed) || 0
            if (showTasks && (query.length === 0 || String(task.name).toLowerCase().indexOf(query) !== -1))
                matching++
        }

        unfinishedCount = unfinished
        visibleTaskCount = matching
        totalSpeed = speed
    }

    function refreshFiles() {
        var source = modManager.downloadedModsList
        var query = searchText.toLowerCase()
        var files = []
        var archives = 0
        var modFiles = 0
        var modUpdates = 0
        var downloadedMods = 0

        for (var i = 0; i < source.length; i++) {
            var file = source[i]
            var isArchive = file.fileType === "archive"
            var isModFile = file.fileType === "ue_mod" || file.fileType === "ue_mod_encrypted"
            var isUpdate = file.library === "updates"

            if (isArchive) archives++
            if (isModFile) modFiles++
            if (isUpdate) modUpdates++
            else downloadedMods++

            if (category === "unfinished") continue
            if (category === "modupdates" && !isUpdate) continue
            if (category === "moddownloads" && isUpdate) continue
            if (category === "archives" && !isArchive) continue
            if (category === "modfiles" && !isModFile) continue
            if (query.length > 0 && String(file.name).toLowerCase().indexOf(query) === -1) continue
            files.push(file)
        }

        archiveCount = archives
        modFileCount = modFiles
        modUpdatesCount = modUpdates
        downloadedModsCount = downloadedMods
        filteredFiles = files
    }

    function refreshAll() {
        refreshTasks()
        refreshFiles()
    }

    function selectCategory(key) {
        if (category === key)
            return
        category = key
        refreshAll()
    }

    function emptyIcon() {
        if (!pathConfigured) return "folder-cog.svg"
        if (searchText.length > 0) return "search-x.svg"
        if (finishedCount === 0 && unfinishedCount === 0) return "inbox.svg"
        if (category === "unfinished") return "circle-check.svg"
        if (category === "modupdates") return "cloud-download.svg"
        if (category === "moddownloads") return "package.svg"
        if (category === "archives") return "archive.svg"
        if (category === "modfiles") return "file-box.svg"
        return "layers.svg"
    }

    function emptyTitle() {
        if (!pathConfigured) return "Downloads folder not configured"
        if (searchText.length > 0) return "No matching downloads"
        if (finishedCount === 0 && unfinishedCount === 0) return "No downloads yet"
        if (category === "unfinished") return "Nothing in progress"
        if (category === "modupdates") return "No mod updates yet"
        if (category === "moddownloads") return "No downloaded mods yet"
        if (category === "archives") return "No archives yet"
        if (category === "modfiles") return "No mod files yet"
        return "Nothing here yet"
    }

    function emptyHint() {
        if (!pathConfigured) return "Choose a downloads folder in Paths & Tools to start collecting mod files."
        if (searchText.length > 0) return "Try a different search term or category."
        if (finishedCount === 0 && unfinishedCount === 0) return "Downloads from Nexus Mods and files you add will appear here."
        if (category === "unfinished") return "Active and paused downloads show up here."
        if (category === "modupdates") return "Updates downloaded through Universal Manager will appear here."
        if (category === "moddownloads") return "Mods downloaded through Universal Manager will appear here."
        if (category === "archives") return "Archive downloads you save will appear here."
        if (category === "modfiles") return "Unreal Engine Loose files will appear here."
        return "Try another category."
    }

    Component.onCompleted: refreshAll()

    onSearchTextChanged: refreshAll()

    onVisibleChanged: {
        if (visible && pathConfigured)
            modManager.scanDownloadedMods()
    }

    Connections {
        target: modManager
        function onDownloadedModsListChanged() { downloadsPage.refreshFiles() }
    }

    Connections {
        target: downloadManager
        function onRevisionChanged() { downloadsPage.refreshTasks() }
    }

    Shortcut {
        sequence: window.shortcutSearch
        enabled: downloadsPage.visible && settingsPage.visible
        onActivated: searchField.focusInput()
    }

    QtObject {
        id: columns
        readonly property int size: 82
        readonly property int status: 154
        readonly property int speed: 88
        readonly property int eta: 94
        readonly property int date: 94
        readonly property int actions: 104
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        ColumnLayout {
            spacing: 2

            Text {
                text: "Downloads"
                font.pixelSize: 20
                font.weight: 600
                color: window.textColor
            }

            Text {
                text: "Manage queued, active and finished downloads"
                font.pixelSize: 12
                color: window.textSecondaryColor
            }
        }

        Item { Layout.fillWidth: true }
    }

    Rectangle {
        id: card
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 420
        radius: window.cardRadius
        color: modManager.theme === "Crimson"
            ? "transparent"
            : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
        gradient: modManager.theme === "Crimson" ? crimsonCardGradient : null
        border.width: window.borderWidth
        border.color: window.borderColor

        Gradient {
            id: crimsonCardGradient
            GradientStop { position: 0.0; color: window.crimsonGradientTop }
            GradientStop { position: 1.0; color: window.crimsonGradientBottom }
        }

        layer.enabled: modManager.theme !== "Glassmorphic"
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.22)
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 3
            shadowBlur: 0.35
        }

        RowLayout {
            anchors.fill: parent
            spacing: 0

            DownloadsSidebar {
                Layout.preferredWidth: 210
                Layout.fillHeight: true
                allCount: downloadsPage.allCount
                unfinishedCount: downloadsPage.unfinishedCount
                finishedCount: downloadsPage.finishedCount
                archiveCount: downloadsPage.archiveCount
                modFileCount: downloadsPage.modFileCount
                modUpdatesCount: downloadsPage.modUpdatesCount
                downloadedModsCount: downloadsPage.downloadedModsCount
                category: downloadsPage.category
                onCategorySelected: function(key) { downloadsPage.selectCategory(key) }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 14
                    Layout.leftMargin: 18
                    Layout.rightMargin: 18
                    spacing: 10

                    ColumnLayout {
                        spacing: 1

                        Text {
                            text: downloadsPage.categoryTitle
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            color: window.textColor
                        }

                        Text {
                            text: downloadsPage.categorySubtitle
                            font.pixelSize: 11
                            color: window.textSecondaryColor
                        }
                    }

                    Item { Layout.fillWidth: true }

                    DownloadsSearchField {
                        id: searchField
                        Layout.preferredWidth: 230
                        onTextChanged: downloadsPage.searchText = text
                    }

                    DownloadsToolbarButton {
                        visible: downloadsPage.pathConfigured
                        iconSource: "qrc:/UniversalModManager/Lucide Icons/refresh-cw.svg"
                        tooltipText: "Refresh"
                        busy: modManager.isScanningDownloads
                        onClicked: modManager.scanDownloadedMods()
                    }

                    DownloadsToolbarButton {
                        visible: downloadsPage.pathConfigured
                        iconSource: "qrc:/UniversalModManager/Lucide Icons/folder-open.svg"
                        label: "Open folder"
                        onClicked: modManager.openDownloadsFolder()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.topMargin: 14
                    Layout.bottomMargin: 8
                    spacing: 12

                    ColumnHeader {
                        Layout.fillWidth: true
                        text: "NAME"
                    }

                    ColumnHeader {
                        columnWidth: columns.size
                        text: "SIZE"
                    }

                    ColumnHeader {
                        columnWidth: columns.status
                        text: "STATUS"
                    }

                    ColumnHeader {
                        columnWidth: columns.speed
                        text: "SPEED"
                        visible: downloadsPage.showActiveColumns
                    }

                    ColumnHeader {
                        columnWidth: columns.eta
                        text: "TIME LEFT"
                        visible: downloadsPage.showActiveColumns
                    }

                    ColumnHeader {
                        columnWidth: columns.date
                        text: "DATE ADDED"
                    }

                    ColumnHeader {
                        columnWidth: columns.actions
                        text: "ACTIONS"
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: downloadsPage.dividerColor
                }

                Item {
                    id: tableArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ScrollView {
                        id: tableScroll
                        anchors.fill: parent
                        visible: downloadsPage.hasRows
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        contentWidth: availableWidth
                        contentHeight: rowsColumn.implicitHeight

                        Component.onCompleted: {
                            if (contentItem) {
                                contentItem.boundsBehavior = Flickable.DragAndOvershootBounds
                                contentItem.flickDeceleration = 1500
                                contentItem.maximumFlickVelocity = 2500
                            }
                        }

                        Column {
                            id: rowsColumn
                            width: tableScroll.availableWidth
                            spacing: 0

                            Repeater {
                                model: downloadManager

                                delegate: DownloadTaskRow {
                                    metrics: columns
                                    searchQuery: downloadsPage.searchText
                                    category: downloadsPage.category
                                    showActiveColumns: downloadsPage.showActiveColumns
                                }
                            }

                            Repeater {
                                model: downloadsPage.filteredFiles

                                delegate: Item {
                                    id: fileDelegate
                                    property var fileData: modelData

                                    width: parent ? parent.width : 0
                                    height: fileRow.height

                                    DownloadedFileRow {
                                        id: fileRow
                                        width: fileDelegate.width
                                        metrics: columns
                                        showActiveColumns: downloadsPage.showActiveColumns
                                        name: fileDelegate.fileData.name
                                        fileSize: fileDelegate.fileData.sizeFormatted
                                        fileExtension: fileDelegate.fileData.extension
                                        fileType: fileDelegate.fileData.fileType
                                        modifiedDate: fileDelegate.fileData.modifiedDate
                                        filePath: fileDelegate.fileData.path
                                    }
                                }
                            }

                            Item { width: 1; height: 6 }
                        }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 12
                        visible: modManager.isScanningDownloads && !downloadsPage.hasRows

                        BusyIndicator {
                            Layout.alignment: Qt.AlignHCenter
                            running: true
                            palette.dark: window.primaryColor
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "Scanning downloads folder..."
                            font.pixelSize: 12
                            color: window.textSecondaryColor
                        }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.max(120, Math.min(tableArea.width - 80, 420))
                        spacing: 10
                        visible: !modManager.isScanningDownloads && !downloadsPage.hasRows

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: 54
                            height: 54
                            radius: 27
                            color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.10)

                            Item {
                                anchors.centerIn: parent
                                width: 24
                                height: 24

                                Image {
                                    id: emptyIconImage
                                    anchors.fill: parent
                                    source: "qrc:/UniversalModManager/Lucide Icons/" + downloadsPage.emptyIcon()
                                    sourceSize: Qt.size(24, 24)
                                    visible: modManager.theme === "Cozy"
                                }

                                ColorOverlay {
                                    anchors.fill: emptyIconImage
                                    source: emptyIconImage
                                    color: window.primaryColor
                                    visible: modManager.theme !== "Cozy"
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: downloadsPage.emptyTitle()
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            color: window.textColor
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            Layout.fillWidth: true
                            text: downloadsPage.emptyHint()
                            font.pixelSize: 12
                            color: window.textSecondaryColor
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                        }

                        Rectangle {
                            visible: !downloadsPage.pathConfigured
                            Layout.alignment: Qt.AlignHCenter
                            width: configureRow.implicitWidth + 26
                            height: 32
                            radius: 8
                            color: configureArea.containsMouse ? Qt.lighter(window.primaryColor, 1.08) : window.primaryColor

                            RowLayout {
                                id: configureRow
                                anchors.centerIn: parent
                                spacing: 6

                                Item {
                                    width: 14
                                    height: 14

                                    Image {
                                        id: configureIcon
                                        anchors.fill: parent
                                        source: "qrc:/UniversalModManager/Lucide Icons/settings.svg"
                                        sourceSize: Qt.size(14, 14)

                                        layer.enabled: true
                                        layer.effect: ColorOverlay { color: "#FFFFFF" }
                                    }
                                }

                                Text {
                                    text: "Configure path"
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    color: "#FFFFFF"
                                }
                            }

                            MouseArea {
                                id: configureArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: generalSectionIndex = 1
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: "transparent"

                    Rectangle {
                        anchors.top: parent.top
                        width: parent.width
                        height: 1
                        color: downloadsPage.dividerColor
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        spacing: 10

                        Text {
                            text: downloadsPage.visibleRows === 1 ? "1 download" : downloadsPage.visibleRows + " downloads"
                            font.pixelSize: 11
                            color: window.textSecondaryColor
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            visible: downloadsPage.unfinishedCount > 0
                            text: downloadsPage.unfinishedCount + " in progress"
                            font.pixelSize: 11
                            color: window.textSecondaryColor
                        }

                        Text {
                            visible: downloadsPage.totalSpeed > 0
                            text: downloadsPage.formatSpeed(downloadsPage.totalSpeed)
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: window.textColor
                        }
                    }
                }
            }
        }
    }

    component ColumnHeader: Text {
        property int columnWidth: 0

        Layout.preferredWidth: columnWidth
        Layout.minimumWidth: columnWidth
        Layout.maximumWidth: columnWidth > 0 ? columnWidth : Number.POSITIVE_INFINITY
        font.pixelSize: 10
        font.weight: Font.DemiBold
        font.letterSpacing: 0.5
        color: window.textSecondaryColor
        opacity: modManager.theme === "Glassmorphic" ? 0.95 : 0.75
        elide: Text.ElideRight
    }
}
