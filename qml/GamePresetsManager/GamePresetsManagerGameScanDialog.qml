import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects
import QtQuick.Dialogs

Item {
    id: gameScanPart
    anchors.fill: parent

    property alias gameScanDialog: gameScanDialog
    property alias modsFolderTypeCombo: modsFolderTypeCombo

    Dialog {
        id: gameScanDialog
        title: "Installed Unreal Engine Games"
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(1180, Math.max(980, gamePresetsManagerPage.width - 80))
        height: Math.min(820, Math.max(700, gamePresetsManagerPage.height - 60))
        padding: 0
        closePolicy: Popup.NoAutoClose

        property bool busy: modManager && modManager.isGameScanRunning
        property string gameSearchFilter: ""
        property string engineFilterMode: "All Engines"
        property string sortGroupMode: "A-Z"
        property var displayGames: []
        property string scanDriveMessage: "all drives"
        property var selectedScanDrives: ({})
        property int ueVersionRefreshToken: 0

property var engineVersionOptions: (ueVersionRefreshToken, buildEngineVersionOptions())

        function clearSearchFocus() {
            if (searchField && searchField.activeFocus) {
                searchField.focus = false
                Qt.inputMethod.hide()
                gameScanDialog.forceActiveFocus()
            }
        }

        function gameMatchesFilter(game) {
            var f = gameSearchFilter.trim().toLowerCase()
            if (f === "")
                return true
            var name = (game && game.name ? ("" + game.name) : "").toLowerCase()
            var path = (game && game.path ? ("" + game.path) : "").toLowerCase()
            return name.indexOf(f) >= 0 || path.indexOf(f) >= 0
        }

        function gameMatchesEngineFilterForMode(game, mode) {
            if (mode === "All Engines")
                return true
            if (!game || !modManager)
                return false
            var gamePath = game.path || ""
            if (gamePath === "")
                return false

            var normalizedVersion = normalizeEngineVersionLabel(modManager.getUEVersionForGame(gamePath))

            if (mode === "Custom Games") {
                return modManager.isCustomGameDetectedForGame(gamePath)
            }

            if (mode === "UE4 Only") {
                return normalizedVersion.startsWith("UE 4.")
            }
            if (mode === "UE5 Only") {
                return normalizedVersion.startsWith("UE 5.")
            }

            return normalizedVersion === mode
        }

        function gameMatchesEngineFilter(game) {
            return gameMatchesEngineFilterForMode(game, engineFilterMode)
        }

        function buildEngineVersionOptions() {
            var games = (modManager && modManager.installedGamesList)
                ? modManager.installedGamesList
                : []

            function entryFor(value) {
                var count = 0
                for (var i = 0; i < games.length; i++) {
                    if (gameMatchesEngineFilterForMode(games[i], value)) {
                        count++
                    }
                }
                return { value: value, label: value + " (" + count + ")" }
            }

            var options = [
                entryFor("Custom Games"),
                entryFor("All Engines"),
                entryFor("UE5 Only"),
                entryFor("UE4 Only")
            ]
            for (var ue5Minor = 8; ue5Minor >= 0; ue5Minor--) {
                options.push(entryFor("UE 5." + ue5Minor))
            }
            for (var ue4Minor = 28; ue4Minor >= 0; ue4Minor--) {
                options.push(entryFor("UE 4." + ue4Minor))
            }
            return options
        }

        function normalizeEngineVersionLabel(version) {
            var value = version === undefined || version === null ? "" : ("" + version).trim()
            var match = value.match(/([45])\.(\d+)/)
            if (!match)
                return ""
            return "UE " + match[1] + "." + parseInt(match[2], 10)
        }

        function compareEngineVersions(a, b) {
            var normalizedA = normalizeEngineVersionLabel(a)
            var normalizedB = normalizeEngineVersionLabel(b)

            if (normalizedA === "" && normalizedB === "")
                return 0
            if (normalizedA === "")
                return 1
            if (normalizedB === "")
                return -1

            var matchA = normalizedA.match(/([45])\.(\d+)/)
            var matchB = normalizedB.match(/([45])\.(\d+)/)
            var majorA = parseInt(matchA[1], 10)
            var majorB = parseInt(matchB[1], 10)
            if (majorA !== majorB)
                return majorA - majorB

            var minorA = parseInt(matchA[2], 10)
            var minorB = parseInt(matchB[2], 10)
            return minorA - minorB
        }

        function filteredGamesCount() {
            return getFilteredGames().length
        }

        function driveFromPath(path) {
            if (!path)
                return ""
            var normalized = ("" + path).replace("\\", "/")
            if (normalized.length >= 2 && normalized.charAt(1) === ":")
                return normalized.substring(0, 2).toUpperCase()
            return ""
        }

        function driveLabel(drivePath) {
            return driveFromPath(drivePath)
        }

        function driveRoot(drivePath) {
            if (!drivePath)
                return ""
            var normalized = ("" + drivePath).replace("\\", "/")
            if (normalized.charAt(normalized.length - 1) !== "/")
                normalized += "/"
            return normalized
        }

        function detectStore(path) {
            var p = (path || "").toLowerCase()
            if (p.indexOf("steamapps") >= 0 || p.indexOf("steam") >= 0)
                return "Steam"
            if (p.indexOf("epic games") >= 0 || p.indexOf("epic") >= 0)
                return "Epic"
            if (p.indexOf("gog") >= 0)
                return "GOG"
            if (p.indexOf("origin") >= 0)
                return "Origin"
            if (p.indexOf("ubisoft") >= 0)
                return "Ubisoft"
            if (p.indexOf("ea games") >= 0 || p.indexOf("electronic arts") >= 0)
                return "EA"
            if (p.indexOf("battle.net") >= 0 || p.indexOf("blizzard") >= 0)
                return "Battle.net"
            if (p.indexOf("windowsapps") >= 0 || p.indexOf("xbox") >= 0)
                return "Xbox"
            return "Other"
        }

        function groupKeyForGame(game) {
            if (!game)
                return ""
            if (sortGroupMode === "Engine") {
                var detectedVersion = normalizeEngineVersionLabel(modManager ? modManager.getUEVersionForGame(game.path || "") : "")
                return detectedVersion === "" ? "Unknown" : detectedVersion
            }
            if (sortGroupMode === "Drive") {
                var drive = driveFromPath(game.path)
                return drive === "" ? "#" : drive
            }
            if (sortGroupMode === "Store") {
                return detectStore(game.path)
            }
            var name = (game.name || "")
            if (name.length === 0)
                return "#"
            var first = name.substring(0, 1).toUpperCase()
            if (first < "A" || first > "Z")
                return "#"
            return first
        }

        function compareGroupKeys(a, b) {
            if (sortGroupMode === "Engine")
                return compareEngineVersions(a, b)
            if (a < b)
                return -1
            if (a > b)
                return 1
            return 0
        }

        function getFilteredGames() {
            var list = modManager ? modManager.installedGamesList : []
            var filtered = []
            for (var i = 0; i < list.length; i++) {
                if (gameMatchesFilter(list[i]) && gameMatchesEngineFilter(list[i])) {
                    filtered.push(list[i])
                }
            }
            return filtered
        }

        function syncGamesListViewport(resetToTop) {
            if (!gamesListView)
                return

            gamesListView.cancelFlick()
            gamesListWheelAnim.stop()

            Qt.callLater(function() {
                if (!gamesListView)
                    return

                var maxY = Math.max(0, gamesListView.contentHeight - gamesListView.height)
                var nextY = resetToTop ? 0 : Math.max(0, Math.min(gamesListView.contentY, maxY))
                gamesListView.wheelTargetY = nextY
                gamesListView.contentY = nextY
            })
        }

        function rebuildDisplayGames(resetToTop) {
            var filtered = getFilteredGames()
            var groups = {}
            for (var i = 0; i < filtered.length; i++) {
                var g = filtered[i]
                var key = groupKeyForGame(g)
                if (!groups[key])
                    groups[key] = []
                groups[key].push(g)
            }
            var keys = Object.keys(groups)
            keys.sort(compareGroupKeys)
            var list = []
            for (var k = 0; k < keys.length; k++) {
                var groupKey = keys[k]
                var items = groups[groupKey]
                items.sort(function(a, b) {
                    var an = (a.name || "").toLowerCase()
                    var bn = (b.name || "").toLowerCase()
                    if (an < bn) return -1
                    if (an > bn) return 1
                    return 0
                })
                for (var j = 0; j < items.length; j++) {
                    list.push(items[j])
                }
            }
            displayGames = list
            syncGamesListViewport(resetToTop === true)
        }

        function selectAllGames() {
            var list = getFilteredGames()
            var next = {}
            for (var i = 0; i < list.length; i++) {
                next[list[i].path] = true
            }
            selectedGames = next
        }

        function selectNoneGames() {
            selectedGames = ({})
        }

        function pruneSelectedGamesToVisible() {
            var list = getFilteredGames()
            if (!selectedGames || Object.keys(selectedGames).length === 0)
                return
            var visiblePaths = ({})
            for (var i = 0; i < list.length; i++) {
                visiblePaths[list[i].path] = true
            }
            var next = ({})
            var changed = false
            for (var path in selectedGames) {
                if (selectedGames[path] === true && visiblePaths[path] === true) {
                    next[path] = true
                } else if (selectedGames[path] === true) {
                    changed = true
                }
            }
            if (changed)
                selectedGames = next
        }

        function selectedDriveList() {
            var list = []
            for (var key in selectedScanDrives) {
                if (selectedScanDrives[key] === true) {
                    list.push(driveRoot(key))
                }
            }
            return list
        }

        onOpened: {
            selectedGames = ({})
            gameScanActionMessage = ""
            gameSearchFilter = ""
            engineFilterMode = "All Engines"
            gameScanDialog.scanDriveMessage = "all drives"
            if (modManager && modManager.installedGamesList.length === 0 && !modManager.isGameScanRunning) {
                if (!modManager.loadInstalledGamesCache()) {
                    modManager.scanInstalledGames()
                }
            }
            rebuildDisplayGames()
            if (modManager && modManager.installedGamesList.length > 0) {

                if (!ueDetectAutoTriggered && !modManager.isUeDetectRunning) {
                    ueDetectAutoTriggered = true
                    modManager.detectUEVersionsForAllGames()
                }
            }
        }

        onGameSearchFilterChanged: { rebuildDisplayGames(true); pruneSelectedGamesToVisible() }
        onEngineFilterModeChanged: { rebuildDisplayGames(true); pruneSelectedGamesToVisible() }
        onSortGroupModeChanged: { rebuildDisplayGames(true); pruneSelectedGamesToVisible() }

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            radius: 16
            border.width: 1
            border.color: isGlassmorphic ? Qt.rgba(255, 255, 255, 0.1) : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)

            gradient: modManager.theme === "Cozy" ? presetsDialogCozyGradient
                    : modManager.theme === "Crimson" ? presetsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? presetsDialogWaveyGradient
                    : null
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowVerticalOffset: 12
                shadowBlur: 2.5
            }
        }

        header: Rectangle {
            height: 60
            color: "transparent"

            Text {
                text: "𝙳𝚎𝚎𝚙 𝚂𝚌𝚊𝚗𝚗𝚎𝚛"
                anchors.centerIn: parent
                font.pixelSize: 18
                font.weight: Font.Bold
                color: window.textColor
                font.letterSpacing: 0.5
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: isGlassmorphic ? Qt.rgba(255, 255, 255, 0.1) : window.borderColor
            }

            MouseArea {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 60
                cursorShape: Qt.PointingHandCursor
                onClicked: gameScanDialog.close()

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: window.textSecondaryColor
                    font.pixelSize: 14
                }
            }
        }

        contentItem: ColumnLayout {
            width: gameScanDialog.availableWidth
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 100
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Rectangle {
                            id: searchFieldContainer
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            radius: 10
                            color: isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.6) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)
                            border.width: 1
                            border.color: searchMouse.containsMouse || searchField.activeFocus ? window.accentColor : window.borderColor
                            opacity: gameScanDialog.busy ? 0.7 : 1.0

                            Behavior on border.color { ColorAnimation { duration: 150 } }
                            Behavior on opacity { NumberAnimation { duration: 150 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 10
                                spacing: 8

                                Text {
                                    text: "⌕"
                                    font.pixelSize: 14
                                    color: window.textSecondaryColor
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                TextField {
                                    id: searchField
                                    ContextMenu.menu: null
                                    Layout.fillWidth: true
                                    text: gameScanDialog.gameSearchFilter
                                    placeholderText: "Search games..."
                                    selectByMouse: true
                                    persistentSelection: true
                                    enabled: !gameScanDialog.busy
                                    inputMethodHints: Qt.ImhNoAutoUppercase

                                    background: null
                                    color: window.textColor
                                    placeholderTextColor: window.textSecondaryColor

                                    onTextChanged: gameScanDialog.gameSearchFilter = text
                                }

                                Rectangle {
                                    width: 24
                                    height: 24
                                    radius: 8
                                    color: clearMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"
                                    visible: searchField.text.length > 0 && !gameScanDialog.busy
                                    Behavior on color { ColorAnimation { duration: 120 } }

                                    Text {
                                        anchors.centerIn: parent
                                        text: "✕"
                                        font.pixelSize: 12
                                        color: window.textSecondaryColor
                                    }

                                    MouseArea {
                                        id: clearMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            searchField.text = ""
                                            searchField.forceActiveFocus()
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: searchMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }

                            MouseArea {
                                z: 1
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                cursorShape: Qt.IBeamCursor
                                onPressed: function(mouse) {
                                    searchField.forceActiveFocus()
                                    searchContextMenu.popup(searchFieldContainer, mouse.x, searchFieldContainer.height + 4)
                                    mouse.accepted = true
                                }
                                onPressAndHold: function(mouse) {
                                    searchField.forceActiveFocus()
                                    searchContextMenu.popup(searchFieldContainer, mouse.x, searchFieldContainer.height + 4)
                                    mouse.accepted = true
                                }
                            }

                            Menu {
                                id: searchContextMenu
                                width: 180
                                padding: 6

                                background: Rectangle {
                                    color: {
                                        if (modManager.theme === "Crimson") return Qt.rgba(0.05, 0.06, 0.10, 0.98)
                                        if (modManager.theme === "Glassmorphic") return Qt.rgba(0.05, 0.05, 0.07, Math.max(0.88, Math.min(0.97, modManager.glassOpacity + 0.84)))
                                        if (modManager.theme === "Wavey") return Qt.rgba(0.08, 0.16, 0.28, 0.97)
                                        return window.surfaceColor
                                    }
                                    radius: 12
                                    border.width: 1
                                    border.color: modManager.theme === "Glassmorphic"
                                        ? Qt.rgba(1, 1, 1, 0.10)
                                        : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.08) : window.borderColor)

                                    layer.enabled: true
                                    layer.effect: MultiEffect {
                                        shadowEnabled: true
                                        shadowColor: Qt.rgba(0, 0, 0, 0.40)
                                        shadowHorizontalOffset: 0
                                        shadowVerticalOffset: 8
                                        shadowBlur: 0.75
                                    }
                                }

                                RuntimeContextMenuItem {
                                    text: "Undo"
                                    iconName: "undo-2.svg"
                                    enabled: searchField.canUndo
                                    onTriggered: searchField.undo()
                                }
                                RuntimeContextMenuItem {
                                    text: "Redo"
                                    iconName: "redo-2.svg"
                                    enabled: searchField.canRedo
                                    onTriggered: searchField.redo()
                                }
                                MenuSeparator {}
                                RuntimeContextMenuItem {
                                    text: "Cut"
                                    iconName: "scissors.svg"
                                    enabled: searchField.selectedText.length > 0
                                    onTriggered: searchField.cut()
                                }
                                RuntimeContextMenuItem {
                                    text: "Copy"
                                    iconName: "copy.svg"
                                    enabled: searchField.selectedText.length > 0
                                    onTriggered: searchField.copy()
                                }
                                RuntimeContextMenuItem {
                                    text: "Paste"
                                    iconName: "clipboard.svg"
                                    enabled: true
                                    onTriggered: searchField.paste()
                                }
                                RuntimeContextMenuItem {
                                    text: "Delete"
                                    iconName: "trash.svg"
                                    enabled: searchField.selectedText.length > 0
                                    onTriggered: searchField.remove(searchField.selectionStart, searchField.selectionEnd)
                                }
                                MenuSeparator {}
                                RuntimeContextMenuItem {
                                    text: "Select All"
                                    iconName: "text-select.svg"
                                    enabled: searchField.text.length > 0
                                    onTriggered: searchField.selectAll()
                                }
                            }
                        }

                        RowLayout {
                            spacing: 8
                            Text {
                                text: "Folder:"
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: window.textSecondaryColor
                            }

                            ComboBox {
                                id: modsFolderTypeCombo
                                model: useUppercaseMods ? ["~Mods", "Mods", "LogicMods"] : ["~mods", "mods", "logicmods"]
                                currentIndex: 1
                                Layout.preferredWidth: 110
                                Layout.preferredHeight: 36

                                onCurrentTextChanged: {
                                    if (currentText === "~Mods" || currentText === "Mods" || currentText === "LogicMods") {
                                        useUppercaseMods = true
                                    } else if (currentText === "~mods" || currentText === "mods" || currentText === "logicmods") {
                                        useUppercaseMods = false
                                    }
                                }

                                delegate: ItemDelegate {
                                    width: modsFolderTypeCombo.width
                                    height: 36
                                    contentItem: Text {
                                        text: modelData
                                        color: hovered ? "#FFFFFF" : window.textColor
                                        font.pixelSize: 13
                                        font.weight: Font.Medium
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 10
                                    }
                                    background: Rectangle {
                                        color: hovered ? window.accentColor : "transparent"
                                        radius: 6
                                        anchors.margins: 4
                                        anchors.fill: parent
                                    }
                                }

                                indicator: Canvas {
                                    x: modsFolderTypeCombo.width - width - 10
                                    y: modsFolderTypeCombo.topPadding + (modsFolderTypeCombo.availableHeight - height) / 2
                                    width: 10
                                    height: 6
                                    contextType: "2d"
                                    Connections {
                                        target: modsFolderTypeCombo
                                        function onPressedChanged() { modsFolderTypeCombo.indicator.requestPaint() }
                                    }
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.reset()
                                        ctx.moveTo(0, 0)
                                        ctx.lineTo(width, 0)
                                        ctx.lineTo(width / 2, height)
                                        ctx.closePath()
                                        ctx.fillStyle = modsFolderTypeCombo.pressed ? window.accentColor : window.textSecondaryColor
                                        ctx.fill()
                                    }
                                }

                                contentItem: Text {
                                    leftPadding: 10
                                    rightPadding: modsFolderTypeCombo.indicator.width + 10
                                    text: modsFolderTypeCombo.displayText
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: window.textColor
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }

                                background: Rectangle {
                                    color: modsFolderTypeCombo.pressed
                                        ? (isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05))
                                        : (isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.95) : window.surfaceColor)
                                    border.color: modsFolderTypeCombo.activeFocus ? window.accentColor : window.borderColor
                                    border.width: 1
                                    radius: 8
                                    Behavior on border.color { ColorAnimation { duration: 150 } }
                                }

                                popup: Popup {
                                    y: modsFolderTypeCombo.height + 4
                                    width: modsFolderTypeCombo.width
                                    implicitHeight: contentItem.implicitHeight + 8
                                    padding: 4
                                    contentItem: ListView {
                                        clip: true
                                        implicitHeight: contentHeight
                                        model: modsFolderTypeCombo.popup.visible ? modsFolderTypeCombo.delegateModel : null
                                        currentIndex: modsFolderTypeCombo.highlightedIndex
                                        ScrollIndicator.vertical: ScrollIndicator { }
                                    }
                                    background: Rectangle {
                                        color: isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor
                                        border.color: window.borderColor
                                        border.width: 1
                                        radius: 10
                                        layer.enabled: true
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: Qt.rgba(0, 0, 0, 0.15)
                                            shadowVerticalOffset: 4
                                            shadowBlur: 0.8
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                width: 36
                                height: 36
                                radius: 8
                                color: toggleUppercaseMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05) : "transparent"
                                border.width: 1
                                border.color: useUppercaseMods ? window.accentColor : window.borderColor

                                Text {
                                    anchors.centerIn: parent
                                    text: "Aa"
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    color: useUppercaseMods ? window.accentColor : window.textSecondaryColor
                                }

                                MouseArea {
                                    id: toggleUppercaseMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        useUppercaseMods = !useUppercaseMods
                                    }
                                }

                                ToolTip {
                                    id: uppercaseModsToolTip
                                    visible: toggleUppercaseMouse.containsMouse
                                    delay: 400
                                    padding: 15

                                    contentItem: Text {
                                        text: "<b>Toggle Uppercase Folder Names</b><br>If a game is missing the Mods folder, you can create any type (uppercase or lowercase) using the selection below.<br><br><font color=\"" + window.textSecondaryColor + "\"><i>Note: Windows treats 'Mods/mods' and '~Mods/~mods' and 'LogicMods/logicmods' as the same folder due to case-insensitivity.</i></font>"
                                        color: window.textColor
                                        font.pixelSize: 14
                                        wrapMode: Text.Wrap
                                        textFormat: Text.StyledText
                                        lineHeight: 1.2
                                    }

                                    background: Rectangle {
                                        radius: 12
                                        color: isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor
                                        border.width: 1
                                        border.color: window.borderColor

                                        layer.enabled: true
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: Qt.rgba(0, 0, 0, 0.3)
                                            shadowBlur: 0.5
                                            shadowVerticalOffset: 4
                                        }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            spacing: 8
                            Text {
                                text: "Engine:"
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: window.textSecondaryColor
                            }

ComboBox {
    id: engineFilterCombo
    model: gameScanDialog.engineVersionOptions
    textRole: "label"
    valueRole: "value"
    currentIndex: {
        var targetValue = gameScanDialog.engineFilterMode
        for (var i = 0; i < model.length; i++) {
            if (model[i] && model[i].value === targetValue)
                return i
        }
        return 0
    }
    onActivated: gameScanDialog.engineFilterMode = currentValue
    Layout.preferredWidth: {

        var text = engineFilterCombo.displayText || ""
        return Math.max(100, text.length * 9 + 50)
    }
    Layout.preferredHeight: 36

delegate: ItemDelegate {
        id: engineFilterDelegate
        required property var modelData
        required property int index
        width: engineFilterCombo.width
        height: 36
        contentItem: Text {
            text: engineFilterDelegate.modelData ? (engineFilterDelegate.modelData.label || "") : ""
            color: hovered ? "#FFFFFF" : window.textColor
                                        font.pixelSize: 13
                                        font.weight: Font.Medium
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 10
                                    }
                                    background: Rectangle {
                                        color: hovered ? window.accentColor : "transparent"
                                        radius: 6
                                        anchors.margins: 4
                                        anchors.fill: parent
                                    }
                                }

                                indicator: Canvas {
                                    x: engineFilterCombo.width - width - 10
                                    y: engineFilterCombo.topPadding + (engineFilterCombo.availableHeight - height) / 2
                                    width: 10
                                    height: 6
                                    contextType: "2d"
                                    Connections {
                                        target: engineFilterCombo
                                        function onPressedChanged() { engineFilterCombo.indicator.requestPaint() }
                                    }
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.reset()
                                        ctx.moveTo(0, 0)
                                        ctx.lineTo(width, 0)
                                        ctx.lineTo(width / 2, height)
                                        ctx.closePath()
                                        ctx.fillStyle = engineFilterCombo.pressed ? window.accentColor : window.textSecondaryColor
                                        ctx.fill()
                                    }
                                }

                                contentItem: Text {
                                    leftPadding: 10
                                    rightPadding: engineFilterCombo.indicator.width + 10
                                    text: engineFilterCombo.displayText
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: window.textColor
                                    verticalAlignment: Text.AlignVCenter
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }

                                background: Rectangle {
                                    color: engineFilterCombo.pressed
                                        ? (isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05))
                                        : (isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.95) : window.surfaceColor)
                                    border.color: engineFilterCombo.activeFocus ? window.accentColor : window.borderColor
                                    border.width: 1
                                    radius: 8
                                    Behavior on border.color { ColorAnimation { duration: 150 } }
                                }

                                popup: Popup {
                                    y: engineFilterCombo.height + 4
                                    width: {

                                        var maxLen = 0
                                        var entries = gameScanDialog.engineVersionOptions || []
                                        for (var i = 0; i < entries.length; i++) {
                                            var entry = entries[i]
                                            if (!entry || !entry.label) continue
                                            if (entry.label.length > maxLen) maxLen = entry.label.length
                                        }
                                        return Math.max(engineFilterCombo.width,
                                                        Math.max(130, maxLen * 9 + 50))
                                    }
                                    implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)
                                    padding: 4

                                    onOpened: {
                                        engineFilterPopupList.positionViewAtBeginning()
                                        Qt.callLater(function() {
                                            engineFilterPopupList.contentY = 0
                                        })
                                    }
                                    contentItem: ListView {
                                                                                id: engineFilterPopupList
                                                                                clip: true
                                                                                implicitHeight: contentHeight
                                                                                model: engineFilterCombo.popup.visible ? engineFilterCombo.delegateModel : null

                                                                                currentIndex: -1
                                                                                highlightFollowsCurrentItem: false
                                                                                boundsBehavior: Flickable.StopAtBounds
                                                                                flickDeceleration: 2200
                                                                                maximumFlickVelocity: 12000
                                                                                cacheBuffer: 480

                                        WheelHandler {
                                            target: null
                                            onWheel: function(event) {
                                                var maxScroll = Math.max(0, engineFilterPopupList.contentHeight - engineFilterPopupList.height)
                                                var step = Math.max(42, engineFilterPopupList.height * 0.2)
                                                var delta = (event.angleDelta.y / 120) * step
                                                engineFilterWheelAnimation.to = Math.max(0, Math.min(maxScroll, engineFilterPopupList.contentY - delta))
                                                engineFilterWheelAnimation.restart()
                                                event.accepted = true
                                            }
                                        }

                                        NumberAnimation {
                                            id: engineFilterWheelAnimation
                                            target: engineFilterPopupList
                                            property: "contentY"
                                            duration: 110
                                            easing.type: Easing.OutCubic
                                        }

                                        ScrollBar.vertical: ScrollBar {
                                            policy: ScrollBar.AsNeeded
                                            interactive: true
                                        }
                                    }
                                    background: Rectangle {
                                        color: isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor
                                        border.color: window.borderColor
                                        border.width: 1
                                        radius: 10
                                        layer.enabled: true
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: Qt.rgba(0, 0, 0, 0.15)
                                            shadowVerticalOffset: 4
                                            shadowBlur: 0.8
                                        }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            spacing: 8
                            Text {
                                text: "Group:"
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: window.textSecondaryColor
                            }

                            ComboBox {
                                id: groupByCombo
                                model: ["A-Z", "Drive", "Store", "Engine"]
                                currentIndex: model.indexOf(gameScanDialog.sortGroupMode)
                                onActivated: gameScanDialog.sortGroupMode = currentText
                                Layout.preferredWidth: 100
                                Layout.preferredHeight: 36

                                delegate: ItemDelegate {
                                    width: groupByCombo.width
                                    height: 36
                                    contentItem: Text {
                                        text: modelData
                                        color: hovered ? "#FFFFFF" : window.textColor
                                        font.pixelSize: 13
                                        font.weight: Font.Medium
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 10
                                    }
                                    background: Rectangle {
                                        color: hovered ? window.accentColor : "transparent"
                                        radius: 6
                                        anchors.margins: 4
                                        anchors.fill: parent
                                    }
                                }

                                indicator: Canvas {
                                    x: groupByCombo.width - width - 10
                                    y: groupByCombo.topPadding + (groupByCombo.availableHeight - height) / 2
                                    width: 10
                                    height: 6
                                    contextType: "2d"
                                    Connections {
                                        target: groupByCombo
                                        function onPressedChanged() { groupByCombo.indicator.requestPaint() }
                                    }
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.reset()
                                        ctx.moveTo(0, 0)
                                        ctx.lineTo(width, 0)
                                        ctx.lineTo(width / 2, height)
                                        ctx.closePath()
                                        ctx.fillStyle = groupByCombo.pressed ? window.accentColor : window.textSecondaryColor
                                        ctx.fill()
                                    }
                                }

                                contentItem: Text {
                                    leftPadding: 10
                                    rightPadding: groupByCombo.indicator.width + 10
                                    text: groupByCombo.displayText
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: window.textColor
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }

                                background: Rectangle {
                                    color: groupByCombo.pressed
                                        ? (isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05))
                                        : (isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.95) : window.surfaceColor)
                                    border.color: groupByCombo.activeFocus ? window.accentColor : window.borderColor
                                    border.width: 1
                                    radius: 8
                                    Behavior on border.color { ColorAnimation { duration: 150 } }
                                }

                                popup: Popup {
                                    y: groupByCombo.height + 4
                                    width: groupByCombo.width
                                    implicitHeight: contentItem.implicitHeight + 8
                                    padding: 4
                                    contentItem: ListView {
                                        clip: true
                                        implicitHeight: contentHeight
                                        model: groupByCombo.popup.visible ? groupByCombo.delegateModel : null
                                        currentIndex: groupByCombo.highlightedIndex
                                        ScrollIndicator.vertical: ScrollIndicator { }
                                    }
                                    background: Rectangle {
                                        color: isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor
                                        border.color: window.borderColor
                                        border.width: 1
                                        radius: 10
                                        layer.enabled: true
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: Qt.rgba(0, 0, 0, 0.15)
                                            shadowVerticalOffset: 4
                                            shadowBlur: 0.8
                                        }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            Layout.preferredWidth: selectAllText.width + 24
                            Layout.preferredHeight: 32
                            radius: 8
                            color: selectAllMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06) : "transparent"
                            border.width: 1
                            border.color: selectAllMouse.containsMouse ? window.accentColor : window.borderColor
                            opacity: !gameScanDialog.busy ? 1.0 : 0.6

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            Text {
                                id: selectAllText
                                anchors.centerIn: parent
                                text: "Select All"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: selectAllMouse.containsMouse ? window.textColor : window.textSecondaryColor
                            }

                            MouseArea {
                                id: selectAllMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: !gameScanDialog.busy ? Qt.PointingHandCursor : Qt.ArrowCursor
                                enabled: !gameScanDialog.busy
                                onClicked: gameScanDialog.selectAllGames()
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: selectNoneText.width + 24
                            Layout.preferredHeight: 32
                            radius: 8
                            color: selectNoneMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06) : "transparent"
                            border.width: 1
                            border.color: selectNoneMouse.containsMouse ? window.accentColor : window.borderColor
                            opacity: !gameScanDialog.busy ? 1.0 : 0.6
                            visible: hasAnySelectedGame

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            Text {
                                id: selectNoneText
                                anchors.centerIn: parent
                                text: "Select None"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: selectNoneMouse.containsMouse ? window.textColor : window.textSecondaryColor
                            }

                            MouseArea {
                                id: selectNoneMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: !gameScanDialog.busy ? Qt.PointingHandCursor : Qt.ArrowCursor
                                enabled: !gameScanDialog.busy
                                onClicked: gameScanDialog.selectNoneGames()
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: rescanText.width + 24
                            Layout.preferredHeight: 32
                            radius: 8
                            color: rescanMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06) : "transparent"
                            border.width: 1
                            border.color: rescanMouse.containsMouse ? window.accentColor : window.borderColor
                            opacity: !gameScanDialog.busy ? 1.0 : 0.6
                            visible: true

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            Text {
                                id: rescanText
                                anchors.centerIn: parent
                                text: "Rescan"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: rescanMouse.containsMouse ? window.textColor : window.textSecondaryColor
                            }

                            MouseArea {
                                id: rescanMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: !gameScanDialog.busy ? Qt.PointingHandCursor : Qt.ArrowCursor
                                enabled: !gameScanDialog.busy
                                onClicked: {
                                    gameScanActionMessage = ""
                                    selectedGames = ({})
                                    gameScanDialog.scanDriveMessage = "all drives"
                                    if (modManager) {
                                        modManager.scanInstalledGames()
                                    }
                                }
                            }
                        }

                        RowLayout {
                            visible: modManager && modManager.isUeDetectRunning
                            spacing: 8
                            Layout.alignment: Qt.AlignVCenter

                            Item {
                                width: 18
                                height: 18

                                Canvas {
                                    id: ueDetectSpinner
                                    anchors.fill: parent
                                    antialiasing: true
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.reset()
                                        var w = width
                                        var h = height
                                        var r = Math.min(w, h) / 2 - 1
                                        var cx = w / 2
                                        var cy = h / 2

                                        ctx.lineWidth = 2
                                        ctx.lineCap = "round"
                                        ctx.strokeStyle = window.accentColor
                                        ctx.beginPath()
                                        ctx.arc(cx, cy, r, 0.2 * Math.PI, 1.7 * Math.PI, false)
                                        ctx.stroke()
                                    }
                                }

                                RotationAnimator on rotation {
                                    from: 0
                                    to: 360
                                    duration: 900
                                    loops: Animation.Infinite
                                    running: gameScanDialog.visible && modManager && modManager.isUeDetectRunning && !window.focusAnimationsPaused
                                }
                            }

                            Text {
                                text: "Detecting Unreal Engine versions.Hang on tight!"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: window.textColor
                            }
                        }

                        Item { Layout.fillWidth: true }

                        RowLayout {
                            visible: gameScanDialog.busy
                            spacing: 8

                            Row {
                                spacing: 4
                                Layout.alignment: Qt.AlignVCenter

                                Repeater {
                                    model: 3
                                    Rectangle {
                                        width: 6
                                        height: 6
                                        radius: 3
                                        color: window.accentColor
                                        opacity: 0.5

                                        SequentialAnimation on opacity {
                                            loops: Animation.Infinite
                                            running: gameScanDialog.busy && !window.focusAnimationsPaused
                                            PauseAnimation { duration: index * 150 }
                                            NumberAnimation { from: 0.5; to: 1.0; duration: 300; easing.type: Easing.InOutQuad }
                                            NumberAnimation { from: 1.0; to: 0.5; duration: 300; easing.type: Easing.InOutQuad }
                                            PauseAnimation { duration: (2 - index) * 150 }
                                        }

                                        SequentialAnimation on scale {
                                            loops: Animation.Infinite
                                            running: gameScanDialog.busy && !window.focusAnimationsPaused
                                            PauseAnimation { duration: index * 150 }
                                            NumberAnimation { from: 1.0; to: 1.2; duration: 300; easing.type: Easing.InOutQuad }
                                            NumberAnimation { from: 1.2; to: 1.0; duration: 300; easing.type: Easing.InOutQuad }
                                            PauseAnimation { duration: (2 - index) * 150 }
                                        }
                                    }
                                }
                            }

                            Text {
                                text: "Scanning " + gameScanDialog.scanDriveMessage + "..."
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: window.textColor
                            }
                        }

                        Text {
                            text: {
                                var total = modManager ? modManager.installedGamesList.length : 0
                                if (gameScanDialog.gameSearchFilter.trim() === "")
                                    return total + " games"
                                return gameScanDialog.filteredGamesCount() + " / " + total
                            }
                            font.pixelSize: 12
                            color: window.textSecondaryColor
                            visible: !gameScanDialog.busy
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 56
                color: "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    Text {
                        text: "Drives:"
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: window.textSecondaryColor
                    }

                    Repeater {
                        model: modManager ? modManager.availableDrives : []

                        Rectangle {
                            property string drivePath: modelData
                            property bool isSelected: gameScanDialog.selectedScanDrives[drivePath] === true
                            Layout.preferredHeight: 30
                            Layout.preferredWidth: driveText.width + 22
                            radius: 8
                            color: isSelected ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.15) : "transparent"
                            border.width: 1
                            border.color: isSelected ? window.accentColor : window.borderColor

                            Behavior on color { ColorAnimation { duration: 120 } }
                            Behavior on border.color { ColorAnimation { duration: 120 } }

                            Text {
                                id: driveText
                                anchors.centerIn: parent
                                text: gameScanDialog.driveLabel(drivePath)
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: isSelected ? window.accentColor : window.textSecondaryColor
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var next = Object.assign({}, gameScanDialog.selectedScanDrives)
                                    next[drivePath] = !(next[drivePath] === true)
                                    gameScanDialog.selectedScanDrives = next
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        Layout.preferredWidth: scanSelectedText.width + 36
                        Layout.preferredHeight: 34
                        radius: 10
                        color: scanSelectedMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06) : "transparent"
                        border.width: 1
                        border.color: scanSelectedMouse.containsMouse ? window.accentColor : window.borderColor
                        opacity: (!gameScanDialog.busy && gameScanDialog.selectedDriveList().length > 0) ? 1.0 : 0.5

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 6

                            Text {
                                id: scanSelectedText
                                text: "Scan Selected Drive(s)"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: scanSelectedMouse.containsMouse ? window.textColor : window.textSecondaryColor
                            }
                        }

                        MouseArea {
                            id: scanSelectedMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: !gameScanDialog.busy && gameScanDialog.selectedDriveList().length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                            enabled: !gameScanDialog.busy && gameScanDialog.selectedDriveList().length > 0
                            onClicked: {
                                var drives = gameScanDialog.selectedDriveList()
                                if (drives.length > 0) {
                                    gameScanActionMessage = ""
                                    selectedGames = ({})
                                    gameScanDialog.scanDriveMessage = drives.length === 1 ? "drive " + gameScanDialog.driveLabel(drives[0]) : "selected drives"
                                    if (modManager) {
                                        modManager.scanInstalledGamesForDrives(drives)
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: cancelScanText.width + 32
                        Layout.preferredHeight: 34
                        radius: 10
                        color: cancelScanMouse.containsMouse ? Qt.rgba(220/255, 38/255, 38/255, 0.12) : "transparent"
                        border.width: 1
                        border.color: cancelScanMouse.containsMouse ? "#DC2626" : window.borderColor
                        visible: gameScanDialog.busy

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 6

                            Text {
                                id: cancelScanText
                                text: "Cancel Scan"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: cancelScanMouse.containsMouse ? "#DC2626" : window.textSecondaryColor
                            }
                        }

                        MouseArea {
                            id: cancelScanMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (modManager) {
                                    modManager.cancelGameScan()
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"

                ListView {
                    id: gamesListView
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 8
                    clip: true
                    model: gameScanDialog.displayGames
                    boundsBehavior: Flickable.StopAtBounds
                    flickDeceleration: 1300
                    maximumFlickVelocity: 9000
                    property real wheelTargetY: contentY
                    onContentHeightChanged: gameScanDialog.syncGamesListViewport(false)
                    onHeightChanged: gameScanDialog.syncGamesListViewport(false)
                    onDragStarted: gameScanDialog.clearSearchFocus()
                    onFlickStarted: gameScanDialog.clearSearchFocus()

                    WheelHandler {
                        id: gamesListWheelHandler
                        target: gamesListView
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                        onWheel: (event) => {
                            var dy = 0
                            if (event.pixelDelta && event.pixelDelta.y !== 0) {
                                dy = event.pixelDelta.y * 1.15
                            } else {
                                dy = event.angleDelta.y / 120 * 180
                            }

                            var maxY = Math.max(0, gamesListView.contentHeight - gamesListView.height)
                            var baseY = gamesListWheelAnim.running ? gamesListView.wheelTargetY : gamesListView.contentY
                            var nextY = baseY - dy
                            nextY = Math.max(0, Math.min(nextY, maxY))

                            gameScanDialog.clearSearchFocus()
                            gamesListView.cancelFlick()
                            gamesListView.wheelTargetY = nextY
                            gamesListWheelAnim.to = gamesListView.wheelTargetY
                            gamesListWheelAnim.restart()
                            event.accepted = true
                        }
                    }

                    NumberAnimation {
                        id: gamesListWheelAnim
                        target: gamesListView
                        property: "contentY"
                        duration: 110
                        easing.type: Easing.OutQuart
                    }

                    ScrollBar.vertical: ScrollBar {
                        width: 12
                        policy: ScrollBar.AsNeeded
                        interactive: true
                    }

                    delegate: Item {
                        id: gameListItem
                        width: gamesListView.width - 10
                        property bool isGroupHeader: modelData && modelData.isGroupHeader === true
                        property var gameData: modelData
                        property bool hovered: false
                        property string gamePath: !isGroupHeader && gameData ? gameData.path : ""
                        property bool isSelected: !isGroupHeader && gamePresetsManagerPage.selectedGames[gamePath] === true
                        property bool modsFolderExists: !isGroupHeader && modManager
                            ? (modsFolderRefreshToken >= 0 && modManager.modsFolderExistsForGame(gameData, selectedModsFolderType))
                            : false
                        property string ueVersion: !isGroupHeader && modManager ?
                            gameScanDialog.ueVersionRefreshToken >= 0 ? modManager.getUEVersionForGame(gamePath) : "" : ""
                        property bool ueDetectRunning: modManager ?
                            gameScanDialog.ueVersionRefreshToken >= 0 ? modManager.isUeDetectRunning : false : false
                        property bool ueDetectFailed: !isGroupHeader && !isCustomGame && !ueDetectRunning && ueVersion === ""
                        property bool ueDetectPending: !isGroupHeader && !isCustomGame && ueDetectRunning && ueVersion === ""

                        property int customGameListVersion: modManager ? modManager.customGameVersions.length : 0
                        property string customGameDisplayName: !isGroupHeader && modManager && gamePath !== "" && customGameListVersion >= 0
                            ? modManager.getCustomGameDisplayNameForGame(gamePath)
                            : ""
                        property string customGameBaseVersion: !isGroupHeader && modManager && gamePath !== "" && customGameListVersion >= 0
                            ? modManager.getCustomGameBaseVersionForGame(gamePath)
                            : ""
                        property bool isCustomGame: !isGroupHeader && customGameDisplayName !== ""

                        height: isGroupHeader ? 0 : 96
                        visible: !isGroupHeader

                        Rectangle {
                            id: gameListDelegate
                            anchors.fill: parent
                            radius: 12
                            visible: !isGroupHeader
                            color: modManager.theme === "Crimson" ? "transparent" :
                                   (isSelected ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.08) :
                                   (hovered ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.01)))

                            gradient: modManager.theme === "Crimson" ? gameListItemGradient : null

                            Gradient {
                                id: gameListItemGradient
                                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                            }

                            border.width: isSelected ? 1 : (hovered ? 1 : 0)
                            border.color: isSelected ? window.accentColor : window.borderColor
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.width { NumberAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                enabled: !isGroupHeader
                                cursorShape: Qt.PointingHandCursor
                                onEntered: gameListItem.hovered = true
                                onExited: gameListItem.hovered = false
                                onPressed: gameScanDialog.clearSearchFocus()
                                onClicked: {
                                    var current = gamePresetsManagerPage.selectedGames[gamePath] === true
                                    gamePresetsManagerPage.setGameSelected(gamePath, !current)
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                anchors.topMargin: 8
                                anchors.bottomMargin: 8
                                spacing: 16

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Text {
                                        text: (!isGroupHeader && gameData && gameData.name) ? gameData.name : ""
                                        font.pixelSize: 15
                                        font.weight: Font.Bold
                                        color: window.textColor
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: (!isGroupHeader && gameData && gameData.path) ? gameData.path : ""
                                        font.pixelSize: 12
                                        color: window.textSecondaryColor
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                        opacity: 0.8
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.topMargin: 4
                                        spacing: 10

                                        Rectangle {
                                            Layout.preferredHeight: 22
                                            Layout.preferredWidth: modsBadgeText.width + 18
                                            radius: 10
                                            color: modsFolderExists ? "#ffffff" : "#ffffff"
                                            border.width: 2
                                            border.color: modsFolderExists ? "#128C12" : "#ff0548"

                                            Text {
                                                id: modsBadgeText
                                                anchors.centerIn: parent
                                                text: modsFolderExists ? "Mods folder exists" : "missing Mods folder"
                                                font.pixelSize: 13
                                                font.weight: Font.Medium
                                                color: modsFolderExists ? "#128C12" : "#d6141a"
                                            }
                                        }

                                        Rectangle {
                                            id: ueVersionBadge
                                            Layout.preferredHeight: 22
                                            Layout.preferredWidth: ueVersionText.width + 16
                                            radius: 10
                                            color: (isCustomGame || ueVersion !== "") ? window.accentColor : "transparent"
                                            border.width: (isCustomGame || ueVersion !== "") ? 0 : 0
                                            visible: ueVersion !== "" || isCustomGame

                                            Text {
                                                id: ueVersionText
                                                anchors.centerIn: parent
                                                text: isCustomGame
                                                      ? (customGameBaseVersion !== ""
                                                         ? "Custom Game · " + customGameBaseVersion
                                                         : "Custom Game")
                                                      : ueVersion
                                                font.pixelSize: 12
                                                font.weight: Font.Bold
                                                color: "#FFFFFF"
                                            }

                                            HoverHandler {
                                                id: ueVersionBadgeHover
                                                cursorShape: Qt.PointingHandCursor
                                            }
                                            ToolTip {
                                                id: ueVersionBadgeToolTip
                                                visible: isCustomGame && ueVersionBadgeHover.hovered
                                                text: isCustomGame && modManager
                                                      ? modManager.getCustomGameSummaryForGame(gamePath)
                                                      : ""
                                                delay: 200
                                                padding: 12

                                                contentItem: Text {
                                                    text: ueVersionBadgeToolTip.text
                                                    color: window.textColor
                                                    font.pixelSize: 12
                                                    wrapMode: Text.Wrap
                                                }

                                                background: Rectangle {
                                                    radius: 10
                                                    color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor
                                                    border.width: 1
                                                    border.color: window.borderColor

                                                    layer.enabled: true
                                                    layer.effect: MultiEffect {
                                                        shadowEnabled: true
                                                        shadowColor: Qt.rgba(0, 0, 0, 0.3)
                                                        shadowBlur: 0.5
                                                        shadowVerticalOffset: 4
                                                    }
                                                }
                                            }
                                        }

                                        Rectangle {
                                            id: ueWarningBadge
                                            Layout.preferredHeight: 22
                                            Layout.preferredWidth: ueWarningText.width + 16
                                            radius: 10
                                            color: "#ffffff"
                                            border.width: 2
                                            border.color: "#ff0548"
                                            visible: ueDetectFailed

                                            Text {
                                                id: ueWarningText
                                                anchors.centerIn: parent
                                                text: "Warning"
                                                font.pixelSize: 12
                                                font.weight: Font.Bold
                                                color: "#d6141a"
                                            }
                                        }

                                        Rectangle {
                                            id: uePendingBadge
                                            Layout.preferredHeight: 22
                                            Layout.preferredWidth: uePendingText.width + 16
                                            radius: 10
                                            color: isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.6) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)
                                            border.width: 1
                                            border.color: window.borderColor
                                            visible: ueDetectPending

                                            Text {
                                                id: uePendingText
                                                anchors.centerIn: parent
                                                text: "Detecting..."
                                                font.pixelSize: 12
                                                font.weight: Font.Medium
                                                color: window.textSecondaryColor
                                            }
                                        }

                                        Item {
                                            Layout.fillWidth: true
                                        }
                                    }
                                }

                                Rectangle {
                                    width: 28
                                    height: 28
                                    radius: 8
                                    color: actionsMenuMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"

                                    Row {
                                        anchors.centerIn: parent
                                        spacing: 3

                                        Repeater {
                                            model: 3
                                            Rectangle {
                                                width: 3
                                                height: 3
                                                radius: 1.5
                                                color: window.textSecondaryColor
                                            }
                                        }
                                    }

                                    MouseArea {
                                        id: actionsMenuMouse
                                        anchors.fill: parent
                                        anchors.margins: -2
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: gameActionsMenu.open()
                                    }

                                    Menu {
                                        id: gameActionsMenu
                                        x: parent.width - width
                                        y: parent.height
                                        width: 180
                                        padding: 6

                                        background: Rectangle {
                                            color: {
                                                if (modManager.theme === "Crimson") return Qt.rgba(0.05, 0.06, 0.10, 0.98)
                                                if (modManager.theme === "Glassmorphic") return Qt.rgba(0.05, 0.05, 0.07, Math.max(0.88, Math.min(0.97, modManager.glassOpacity + 0.84)))
                                                if (modManager.theme === "Wavey") return Qt.rgba(0.08, 0.16, 0.28, 0.97)
                                                return window.surfaceColor
                                            }
                                            radius: 12
                                            border.width: 1
                                            border.color: modManager.theme === "Glassmorphic"
                                                ? Qt.rgba(1, 1, 1, 0.10)
                                                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.08) : window.borderColor)

                                            layer.enabled: true
                                            layer.effect: MultiEffect {
                                                shadowEnabled: true
                                                shadowColor: Qt.rgba(0, 0, 0, 0.40)
                                                shadowHorizontalOffset: 0
                                                shadowVerticalOffset: 8
                                                shadowBlur: 0.75
                                            }
                                        }

                                        RuntimeContextMenuItem {
                                            text: "Open Game Folder"
                                            iconName: "folder-open.svg"
                                            onTriggered: modManager.openGameFolder(gameData.path)
                                        }

                                        RuntimeContextMenuItem {
                                            text: "Copy Path"
                                            iconName: "clipboard.svg"
                                            onTriggered: modManager.copyTextToClipboard(gameData.path)
                                        }

                                        RuntimeContextMenuItem {
                                            text: "Copy Name"
                                            iconName: "copy.svg"
                                            onTriggered: modManager.copyTextToClipboard(gameData.name)
                                        }

                                        RuntimeContextMenuItem {
                                            text: "Detect UE Version"
                                            iconName: "cpu.svg"
                                            onTriggered: {
                                                var version = modManager.detectUEVersionForGame(gameData.path)
                                                if (version !== "") {
                                                    gameScanActionMessage = "Detected: " + gameData.name + " uses " + version
                                                    gameScanActionSuccess = true
                                                } else {
                                                    gameScanActionMessage = "Could not detect UE version for " + gameData.name + ". Please check if the detector path is set in the tools."
                                                    gameScanActionSuccess = false
                                                }
                                            }
                                        }

                                        RuntimeContextMenuItem {
                                            text: "Open Mods Folder"
                                            iconName: "folder.svg"
                                            enabled: modsFolderExists
                                            onTriggered: modManager.openModsFoldersForGames([gameData], selectedModsFolderType)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: gameScanActionMessage !== ""
                    ? Math.min(messageColumn.implicitHeight + 24, 190) : 0
                Layout.topMargin: gameScanActionMessage !== "" ? 8 : 0
                Layout.bottomMargin: gameScanActionMessage !== "" ? 8 : 0
                visible: gameScanActionMessage !== ""
                radius: 12
                color: gameScanActionSuccess
                    ? Qt.rgba(22/255, 163/255, 74/255, 0.08)
                    : Qt.rgba(220/255, 38/255, 38/255, 0.08)
                border.width: 1
                border.color: gameScanActionSuccess
                    ? Qt.rgba(22/255, 163/255, 74/255, 0.2)
                    : Qt.rgba(220/255, 38/255, 38/255, 0.2)

                Behavior on Layout.preferredHeight {
                    NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                }

                Flickable {
                    id: messageFlick
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 36
                    anchors.topMargin: 12
                    anchors.bottomMargin: 12
                    contentWidth: width
                    contentHeight: messageColumn.implicitHeight
                    clip: true
                    flickableDirection: Flickable.VerticalFlick
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    ColumnLayout {
                        id: messageColumn
                        width: messageFlick.width - 10
                        spacing: 8

                        Repeater {
                            model: {
                                if (gameScanActionMessage === "") return []
                                var sections = []
                                var parts = gameScanActionMessage.split("\n")
                                var notePrefix = "[note] "
                                for (var i = 0; i < parts.length; i++) {
                                    var part = parts[i].trim()
                                    if (part === "") continue

                                    var isNote = part.indexOf(notePrefix) === 0
                                    if (isNote) {
                                        part = part.substring(notePrefix.length)
                                        sections.push({ label: part, games: [], isNote: true })
                                        continue
                                    }

                                    var colonIndex = part.indexOf(":")
                                    if (colonIndex > 0) {
                                        var label = part.substring(0, colonIndex).trim()
                                        var gamesStr = part.substring(colonIndex + 1).trim()
                                        var games = gamesStr.split(",").map(function(g) { return g.trim() }).filter(function(g) { return g !== "" })
                                        sections.push({ label: label, games: games, isNote: false })
                                    } else {
                                        sections.push({ label: part, games: [], isNote: false })
                                    }
                                }
                                return sections
                            }

                            delegate: ColumnLayout {
                                Layout.fillWidth: true
                                spacing: modelData.isNote ? 2 : 6

                                Text {
                                    Layout.fillWidth: true
                                    visible: modelData.isNote === true
                                    text: modelData.label
                                    font.pixelSize: 13
                                    font.italic: true
                                    color: gameScanActionSuccess ? window.textSecondaryColor : "#DC2626"
                                    wrapMode: Text.Wrap
                                    textFormat: Text.StyledText
                                    lineHeight: 1.3
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    color: gameScanActionSuccess ? "#16A34A" : "#DC2626"
                                    visible: !modelData.isNote && text !== ""
                                    wrapMode: Text.Wrap
                                }

                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    visible: !modelData.isNote && modelData.games.length > 0

                                    Repeater {
                                        model: modelData.games

                                        delegate: Rectangle {
                                            width: chipText.width + 20
                                            height: 30
                                            radius: 7
                                            color: gameScanActionSuccess
                                                ? Qt.rgba(22/255, 163/255, 74/255, 0.12)
                                                : Qt.rgba(220/255, 38/255, 38/255, 0.12)

                                            Text {
                                                id: chipText
                                                anchors.centerIn: parent
                                                text: modelData
                                                font.pixelSize: 13
                                                font.weight: Font.Medium
                                                color: gameScanActionSuccess ? "#16A34A" : "#DC2626"
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 8
                    width: 20
                    height: 20
                    radius: 10
                    color: closeMessageMouse.containsMouse
                        ? (gameScanActionSuccess ? Qt.rgba(22/255, 163/255, 74/255, 0.15) : Qt.rgba(220/255, 38/255, 38/255, 0.15))
                        : "transparent"

                    Behavior on color { ColorAnimation { duration: 150 } }

                    Canvas {
                        anchors.centerIn: parent
                        width: 10
                        height: 10
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = gameScanActionSuccess ? "#16A34A" : "#DC2626"
                            ctx.lineWidth = 1.5
                            ctx.lineCap = "round"

                            ctx.beginPath()
                            ctx.moveTo(2, 2)
                            ctx.lineTo(8, 8)
                            ctx.moveTo(8, 2)
                            ctx.lineTo(2, 8)
                            ctx.stroke()
                        }
                    }

                    MouseArea {
                        id: closeMessageMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: gameScanActionMessage = ""
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 80
                color: "transparent"

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: isGlassmorphic ? Qt.rgba(255, 255, 255, 0.1) : window.borderColor
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: createModFolderText.width + 44
                        Layout.preferredHeight: 44
                        radius: 12
                        color: createModFolderMouse.containsMouse && createModFolderMouse.enabled ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05) : "transparent"
                        border.width: 1
                        border.color: createModFolderMouse.containsMouse && createModFolderMouse.enabled ? window.accentColor : window.borderColor
                        opacity: createModFolderMouse.enabled ? 1.0 : 0.5

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        scale: createModFolderMouse.pressed ? 0.96 : 1.0
                        Behavior on scale { NumberAnimation { duration: 100 } }

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 8

                            Canvas {
                                width: 18
                                height: 18
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.fillStyle = createModFolderMouse.containsMouse && createModFolderMouse.enabled ? window.accentColor : window.textSecondaryColor

                                    var s = 18/24
                                    ctx.beginPath()
                                    ctx.moveTo(2*s, 4*s)
                                    ctx.lineTo(9*s, 4*s)
                                    ctx.lineTo(11*s, 7*s)
                                    ctx.lineTo(22*s, 7*s)
                                    ctx.lineTo(22*s, 20*s)
                                    ctx.lineTo(2*s, 20*s)
                                    ctx.closePath()
                                    ctx.fill()
                                }

                                property color iconColor: createModFolderMouse.containsMouse && createModFolderMouse.enabled ? window.accentColor : window.textSecondaryColor
                                onIconColorChanged: requestPaint()
                            }

                            Text {
                                id: createModFolderText
                                text: "Create Mods Folder"
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: createModFolderMouse.containsMouse && createModFolderMouse.enabled ? window.textColor : window.textSecondaryColor
                            }
                        }

                        MouseArea {
                            id: createModFolderMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: createModFolderMouse.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            enabled: !gameScanDialog.busy && canCreateSelectedModsFolder
                            onClicked: {
                                var selected = []
                                var list = modManager.installedGamesList
                                for (var i = 0; i < list.length; i++) {
                                    var g = list[i]
                                    if (selectedGames[g.path] === true) {
                                        selected.push(g)
                                    }
                                }
                                if (selected.length > 0) {
                                    modManager.createModsFoldersForGames(selected, selectedModsFolderType)
                                    gameScanDialog.rebuildDisplayGames()
                                    selectedGames = Object.assign({}, selectedGames)
                                    modsFolderRefreshToken = modsFolderRefreshToken + 1
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: openModsFolderText.width + 44
                        Layout.preferredHeight: 44
                        radius: 12
                        color: openModsFolderMouse.containsMouse && openModsFolderMouse.enabled ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05) : "transparent"
                        border.width: 1
                        border.color: openModsFolderMouse.containsMouse && openModsFolderMouse.enabled ? window.accentColor : window.borderColor
                        opacity: openModsFolderMouse.enabled ? 1.0 : 0.5

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        scale: openModsFolderMouse.pressed ? 0.96 : 1.0
                        Behavior on scale { NumberAnimation { duration: 100 } }

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 8

                            Canvas {
                                width: 18
                                height: 18
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.strokeStyle = openModsFolderMouse.containsMouse && openModsFolderMouse.enabled ? window.accentColor : window.textSecondaryColor
                                    ctx.lineWidth = 1.5
                                    ctx.lineCap = "round"
                                    ctx.lineJoin = "round"

                                    var s = 18/24
                                    ctx.beginPath()
                                    ctx.moveTo(2*s, 5*s)
                                    ctx.lineTo(9*s, 5*s)
                                    ctx.lineTo(11*s, 8*s)
                                    ctx.lineTo(22*s, 8*s)
                                    ctx.lineTo(22*s, 20*s)
                                    ctx.lineTo(2*s, 20*s)
                                    ctx.closePath()
                                    ctx.stroke()

                                    ctx.beginPath()
                                    ctx.moveTo(12*s, 15*s)
                                    ctx.lineTo(12*s, 9*s)
                                    ctx.lineTo(9*s, 12*s)
                                    ctx.moveTo(12*s, 9*s)
                                    ctx.lineTo(15*s, 12*s)
                                    ctx.stroke()
                                }

                                property color iconColor: openModsFolderMouse.containsMouse && openModsFolderMouse.enabled ? window.accentColor : window.textSecondaryColor
                                onIconColorChanged: requestPaint()
                            }

                            Text {
                                id: openModsFolderText
                                text: "Open Mods Folder"
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: openModsFolderMouse.containsMouse && openModsFolderMouse.enabled ? window.textColor : window.textSecondaryColor
                            }
                        }

                        MouseArea {
                            id: openModsFolderMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: openModsFolderMouse.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            enabled: !gameScanDialog.busy && canOpenSelectedModsFolder
                            onClicked: {
                                var selected = getSelectedGamesList()
                                if (selected.length > 0) {
                                    modManager.openModsFoldersForGames(selected, selectedModsFolderType)
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: deleteModsFolderText.width + 44
                        Layout.preferredHeight: 44
                        radius: 12
                        color: deleteModsFolderMouse.containsMouse && deleteModsFolderMouse.enabled ? Qt.rgba(231/255, 76/255, 60/255, 0.1) : "transparent"
                        border.width: 1
                        border.color: deleteModsFolderMouse.containsMouse && deleteModsFolderMouse.enabled ? "#E74C3C" : window.borderColor
                        opacity: deleteModsFolderMouse.enabled ? 1.0 : 0.5

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        scale: deleteModsFolderMouse.pressed ? 0.96 : 1.0
                        Behavior on scale { NumberAnimation { duration: 100 } }

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 8

                            Canvas {
                                width: 18
                                height: 18
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.strokeStyle = deleteModsFolderMouse.containsMouse && deleteModsFolderMouse.enabled ? "#E74C3C" : window.textSecondaryColor
                                    ctx.lineWidth = 1.5
                                    ctx.lineCap = "round"
                                    ctx.lineJoin = "round"

                                    var s = 18/24

                                    ctx.beginPath()
                                    ctx.moveTo(19*s, 7*s)
                                    ctx.lineTo(19*s, 20*s)
                                    ctx.quadraticCurveTo(19*s, 22*s, 17*s, 22*s)
                                    ctx.lineTo(7*s, 22*s)
                                    ctx.quadraticCurveTo(5*s, 22*s, 5*s, 20*s)
                                    ctx.lineTo(5*s, 7*s)
                                    ctx.stroke()

                                    ctx.beginPath()
                                    ctx.moveTo(3*s, 7*s)
                                    ctx.lineTo(21*s, 7*s)
                                    ctx.stroke()

                                    ctx.beginPath()
                                    ctx.moveTo(9*s, 7*s)
                                    ctx.lineTo(9*s, 4*s)
                                    ctx.quadraticCurveTo(9*s, 2*s, 10.5*s, 2*s)
                                    ctx.lineTo(13.5*s, 2*s)
                                    ctx.quadraticCurveTo(15*s, 2*s, 15*s, 4*s)
                                    ctx.lineTo(15*s, 7*s)
                                    ctx.stroke()

                                    ctx.beginPath()
                                    ctx.moveTo(10*s, 11*s)
                                    ctx.lineTo(10*s, 18*s)
                                    ctx.stroke()

                                    ctx.beginPath()
                                    ctx.moveTo(14*s, 11*s)
                                    ctx.lineTo(14*s, 18*s)
                                    ctx.stroke()
                                }

                                property color iconColor: deleteModsFolderMouse.containsMouse && deleteModsFolderMouse.enabled ? "#E74C3C" : window.textSecondaryColor
                                onIconColorChanged: requestPaint()
                            }

                            Text {
                                id: deleteModsFolderText
                                text: "Delete Mods Folder"
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: deleteModsFolderMouse.containsMouse && deleteModsFolderMouse.enabled ? "#E74C3C" : window.textSecondaryColor
                            }
                        }

                        MouseArea {
                            id: deleteModsFolderMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            enabled: !gameScanDialog.busy && canDeleteSelectedModsFolder
                            onClicked: {
                                deleteModsFolderConfirmDialog.open()
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        id: addProfilesBtn
                        Layout.preferredWidth: addProfilesText.width + 48
                        Layout.preferredHeight: 44
                        radius: 12
                        color: addProfilesMouse.containsMouse ? Qt.darker(window.accentColor, 1.1) : window.accentColor
                        opacity: !gameScanDialog.busy ? 1.0 : 0.6

                        Behavior on color { ColorAnimation { duration: 150 } }

                        layer.enabled: false

                        scale: addProfilesMouse.pressed ? 0.96 : 1.0
                        Behavior on scale { NumberAnimation { duration: 100 } }

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 8

                            Text {
                                text: "+"
                                font.pixelSize: 18
                                font.weight: Font.Bold
                                color: "#FFFFFF"
                            }

                            Text {
                                id: addProfilesText
                                text: "Add selected to profiles"
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                color: "#FFFFFF"
                            }
                        }

                        MouseArea {
                            id: addProfilesMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: !gameScanDialog.busy ? Qt.PointingHandCursor : Qt.ArrowCursor
                            enabled: !gameScanDialog.busy
                            onClicked: {
                                startAddSelectedGames()
                            }
                        }
                    }
                }
            }
        }
    }
}
