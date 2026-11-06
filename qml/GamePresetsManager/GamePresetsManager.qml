import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects
import QtQuick.Dialogs

Item {
    id: gamePresetsManagerPage
    anchors.fill: parent

    property bool ueDetectAutoTriggered: false

    property int totalProfiles: modManager && modManager.gamePresets ? modManager.gamePresets.length : 0
    property int totalMods: modManager ? modManager.totalModsAllPresets : 0
    property int totalActiveMods: modManager ? modManager.totalActiveModsAllPresets : 0
    property string selectedPresetIdForPreview: ""
    property var selectedGames: ({})
    property string selectedModsFolderType: modsFolderTypeCombo.currentText
    property string gameScanActionMessage: ""
    property bool gameScanActionSuccess: true
    property bool isDeletingModsFolders: false
    property bool useUppercaseMods: false
    property bool useUppercaseModsName: false
    property int modsFolderRefreshToken: 0

    onUseUppercaseModsChanged: {
        var savedIdx = modsFolderTypeCombo.currentIndex
        Qt.callLater(function() {
            if (modsFolderTypeCombo.model && savedIdx >= 0 && savedIdx < modsFolderTypeCombo.model.length) {
                modsFolderTypeCombo.currentIndex = savedIdx
            }
        })
    }

    property var pendingAddGames: []
    property var pendingAddQueue: []
    property var currentAddGame: ({})
    property var currentAddChoices: []
    property int currentAddChoiceIndex: 0

    property var pendingDeleteGames: []
    property var pendingDeleteQueue: []
    property var currentDeleteGame: ({})
    property var currentDeleteChoices: []
    property int currentDeleteChoiceIndex: 0

    property var pendingCreateModsFolderGames: ([])
    property string pendingCreateModsFolderType: ""
    property var pendingAddSelectedQueue: ([])

    Gradient {
        id: presetsDialogCozyGradient
        GradientStop { position: 0.0; color: window.cozyDialogGradientTop }
        GradientStop { position: 1.0; color: window.cozyDialogGradientBottom }
        orientation: Gradient.Vertical
    }
    Gradient {
        id: presetsDialogCrimsonGradient
        GradientStop { position: 0.0; color: window.crimsonDialogGradientTop }
        GradientStop { position: 1.0; color: window.crimsonDialogGradientBottom }
        orientation: Gradient.Vertical
    }
    Gradient {
        id: presetsDialogWaveyGradient
        GradientStop { position: 0.0; color: window.waveyDialogGradientTop }
        GradientStop { position: 1.0; color: window.waveyDialogGradientBottom }
        orientation: Gradient.Vertical
    }

    property bool hasAnySelectedGame: {
        if (!modManager) return false
        var list = modManager.installedGamesList || []
        for (var i = 0; i < list.length; i++) {
            if (selectedGames[list[i].path] === true) {
                return true
            }
        }
        return false
    }

    property bool canCreateSelectedModsFolder: hasAnySelectedGame

    property bool canOpenSelectedModsFolder: {
        if (!modManager) return false
        var list = modManager.installedGamesList || []
        var selected = []
        for (var i = 0; i < list.length; i++) {
            var g = list[i]
            if (selectedGames[g.path] === true) {
                selected.push(g)
            }
        }
        if (selected.length === 0) return false
        for (var j = 0; j < selected.length; j++) {
            if (!modManager.modsFolderExistsForGame(selected[j], selectedModsFolderType)) {
                return false
            }
        }
        return true
    }

    property bool canDeleteSelectedModsFolder: {
        if (!modManager) return false
        var list = modManager.installedGamesList || []
        var selected = []
        for (var i = 0; i < list.length; i++) {
            var g = list[i]
            if (selectedGames[g.path] === true) {
                selected.push(g)
            }
        }
        if (selected.length === 0) return false

        for (var j = 0; j < selected.length; j++) {
            if (modManager.modsFolderExistsForGame(selected[j], selectedModsFolderType)) {
                return true
            }
        }
        return false
    }

    function getSelectedGamesList() {
        var selected = []
        if (!modManager) return selected
        var list = modManager.installedGamesList || []
        for (var i = 0; i < list.length; i++) {
            var g = list[i]
            if (selectedGames[g.path] === true) {
                selected.push(g)
            }
        }
        return selected
    }

    function startAddSelectedGames() {
        var selected = []
        var list = modManager.installedGamesList || []
        for (var i = 0; i < list.length; i++) {
            var g = list[i]
            if (selectedGames[g.path] === true) {
                selected.push(g)
            }
        }
        if (selected.length === 0) return

        var missing = []
        for (var j = 0; j < selected.length; j++) {
            if (!modManager.modsFolderExistsForGame(selected[j], selectedModsFolderType)) {
                missing.push(selected[j])
            }
        }

        pendingAddSelectedQueue = selected.slice(0)
        pendingCreateModsFolderGames = missing
        pendingCreateModsFolderType = selectedModsFolderType

        if (missing.length > 0) {
            createModsFolderWarningDialog.open()
            return
        }

        pendingAddQueue = pendingAddSelectedQueue.slice(0)
        pendingAddGames = []
        selectedGames = ({})
        processNextAddGame()
    }

    function processNextAddGame() {
        if (pendingAddQueue.length === 0) {
            if (pendingAddGames.length > 0) {
                modManager.addGamesToPresets(pendingAddGames, selectedModsFolderType)
            }
            return
        }
        var game = pendingAddQueue.shift()
        var choices = modManager.getModsFoldersForGame(game)
        if (choices.length > 1) {
            currentAddGame = game
            currentAddChoices = choices
            currentAddChoiceIndex = 0
            modsFolderChoiceModel.clear()
            for (var i = 0; i < choices.length; i++) {
                modsFolderChoiceModel.append(choices[i])
            }
            modsFolderChoiceDialog.open()
            return
        }
        if (choices.length === 1) {
            game.modsFolderPath = choices[0].path
        }
        pendingAddGames.push(game)
        processNextAddGame()
    }

    function startDeleteSelectedGames() {
        var selected = []
        var list = modManager.installedGamesList || []
        for (var i = 0; i < list.length; i++) {
            var g = list[i]
            if (selectedGames[g.path] === true) {
                selected.push(g)
            }
        }
        if (selected.length === 0) return

        pendingDeleteQueue = selected.slice(0)
        pendingDeleteGames = []
        selectedGames = ({})

        deleteDelayTimer.start()
    }

    Timer {
        id: deleteDelayTimer
        interval: 100
        repeat: false
        onTriggered: processNextDeleteGame()
    }

    Timer {
        id: deleteExecuteTimer
        interval: 100
        repeat: false
        onTriggered: {
            if (pendingDeleteGames.length > 0) {
                modManager.deleteModsFoldersForGames(pendingDeleteGames, selectedModsFolderType)
                gameScanDialog.rebuildDisplayGames()
                selectedGames = Object.assign({}, selectedGames)
                modsFolderRefreshToken = modsFolderRefreshToken + 1
                pendingDeleteGames = []
            }
            isDeletingModsFolders = false
        }
    }

    function executePendingDeleteGames() {
        if (pendingDeleteGames.length === 0) {
            isDeletingModsFolders = false
            return
        }

        isDeletingModsFolders = true
        deleteExecuteTimer.start()
    }

    function processNextDeleteGame() {
        if (pendingDeleteQueue.length === 0) {
            if (pendingDeleteGames.length > 0) {
                executePendingDeleteGames()
                return
            }
            isDeletingModsFolders = false
            return
        }

        var game = pendingDeleteQueue.shift()
        var choices = modManager.getModsFoldersForGame(game)
        if (choices.length > 1) {
            currentDeleteGame = game
            currentDeleteChoices = choices
            currentDeleteChoiceIndex = 0
            deleteModsFolderChoiceModel.clear()
            for (var i = 0; i < choices.length; i++) {
                deleteModsFolderChoiceModel.append(choices[i])
            }
            var totalMods = 0
            var choiceNames = []
            for (var j = 0; j < choices.length; j++) {
                totalMods += Number(choices[j].count || 0)
                choiceNames.push(choices[j].name)
            }
            deleteModsFolderChoiceModel.append({
                "name": "Delete all mods folders",
                "path": choiceNames.join(" / "),
                "count": totalMods,
                "deleteAll": true
            })
            deleteModsFolderChoiceDialog.open()
            return
        }

        if (choices.length === 1) {
            game.modsFolderPath = choices[0].path
        }
        pendingDeleteGames.push(game)
        processNextDeleteGame()
    }

    function setGameSelected(gamePath, checked) {
        var next = Object.assign({}, selectedGames)
        next[gamePath] = checked
        selectedGames = next
    }

    function _continueAddSelectedGames() {
        pendingAddQueue = pendingAddSelectedQueue.slice(0)
        pendingAddGames = []
        selectedGames = ({})
        processNextAddGame()
    }

    property bool isGlassmorphic: modManager && modManager.theme === "Glassmorphic"
    property real glassOpacity: modManager ? modManager.glassOpacity : 0.25

    FileDialog {
        id: previewImageDialog
        title: "Select Preview Image"
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.gif *.webp *.bmp)", "All files (*)"]
        onAccepted: {
            if (selectedPresetIdForPreview !== "") {
                modManager.setPresetPreview(selectedPresetIdForPreview, selectedFile)
                selectedPresetIdForPreview = ""
            }
        }
        onRejected: {
            selectedPresetIdForPreview = ""
        }
    }

    Dialog {
        id: createModsFolderWarningDialog
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 520
        padding: 0
        closePolicy: Popup.NoAutoClose

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            radius: 20
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.20)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? presetsDialogCozyGradient
                    : modManager.theme === "Crimson" ? presetsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? presetsDialogWaveyGradient
                    : null
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"
        }

        contentItem: ColumnLayout {
            spacing: 0
            anchors.fill: parent

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: window.borderColor
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20

                    Text {
                        text: "Missing Mods Folder"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: window.textColor
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 16
                Layout.bottomMargin: 16
                spacing: 12

                Text {
                    text: pendingCreateModsFolderGames.length === 1
                        ? (pendingCreateModsFolderGames[0].name ? pendingCreateModsFolderGames[0].name : "Selected Game")
                        : (pendingCreateModsFolderGames.length + " selected games")
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    color: window.textColor
                    Layout.fillWidth: true
                }

                Text {
                    text: pendingCreateModsFolderGames.length === 1
                        ? "This game does not have a mods folder. Would you like to create one?"
                        : "Some selected games do not have a mods folder. Would you like to create one for them?"
                    font.pixelSize: 13
                    color: window.textSecondaryColor
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "Folder name:"
                        font.pixelSize: 13
                        color: window.textSecondaryColor
                    }

                    ComboBox {
                        id: createModsFolderTypeCombo
                        model: gamePresetsManagerPage.useUppercaseMods
                            ? ["~Mods", "Mods", "LogicMods"]
                            : ["~mods", "mods", "logicmods"]
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 36

                        Component.onCompleted: {
                            var wanted = (gamePresetsManagerPage.pendingCreateModsFolderType || "").toLowerCase()
                            var idx = -1
                            for (var i = 0; i < model.length; ++i) {
                                if (model[i].toLowerCase() === wanted) {
                                    idx = i
                                    break
                                }
                            }
                            currentIndex = idx >= 0 ? idx : 0
                        }
                        onCurrentTextChanged: {
                            if (currentText) gamePresetsManagerPage.pendingCreateModsFolderType = currentText
                        }

                        delegate: ItemDelegate {
                            width: createModsFolderTypeCombo.width
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
                            x: createModsFolderTypeCombo.width - width - 10
                            y: createModsFolderTypeCombo.topPadding + (createModsFolderTypeCombo.availableHeight - height) / 2
                            width: 10
                            height: 6
                            contextType: "2d"
                            Connections {
                                target: createModsFolderTypeCombo
                                function onPressedChanged() { createModsFolderTypeCombo.indicator.requestPaint() }
                            }
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()
                                ctx.moveTo(0, 0)
                                ctx.lineTo(width, 0)
                                ctx.lineTo(width / 2, height)
                                ctx.closePath()
                                ctx.fillStyle = createModsFolderTypeCombo.pressed ? window.accentColor : window.textSecondaryColor
                                ctx.fill()
                            }
                        }

                        contentItem: Text {
                            leftPadding: 10
                            rightPadding: createModsFolderTypeCombo.indicator.width + 10
                            text: createModsFolderTypeCombo.displayText
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: window.textColor
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        background: Rectangle {
                            color: createModsFolderTypeCombo.pressed
                                ? (isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05))
                                : (isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 0.95) : window.surfaceColor)
                            border.color: createModsFolderTypeCombo.activeFocus ? window.accentColor : window.borderColor
                            border.width: 1
                            radius: 8
                            Behavior on border.color { ColorAnimation { duration: 150 } }
                        }

                        popup: Popup {
                            y: createModsFolderTypeCombo.height + 4
                            width: createModsFolderTypeCombo.width
                            implicitHeight: contentItem.implicitHeight + 8
                            padding: 4
                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: createModsFolderTypeCombo.popup.visible ? createModsFolderTypeCombo.delegateModel : null
                                currentIndex: createModsFolderTypeCombo.highlightedIndex
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

                    Item { Layout.fillWidth: true }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: window.borderColor
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 12

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        Layout.preferredWidth: cancelCreateModsFolderText.width + 32
                        Layout.preferredHeight: 40
                        radius: 10
                        color: cancelCreateModsFolderMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: cancelCreateModsFolderText
                            anchors.centerIn: parent
                            text: "Not now"
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            color: window.textSecondaryColor
                        }

                        MouseArea {
                            id: cancelCreateModsFolderMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                createModsFolderWarningDialog.close()
                                gamePresetsManagerPage._continueAddSelectedGames()
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: confirmCreateModsFolderText.width + 40
                        Layout.preferredHeight: 40
                        radius: 10
                        color: confirmCreateModsFolderMouse.containsMouse ? Qt.darker(window.accentColor, 1.1) : window.accentColor

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: confirmCreateModsFolderText
                            anchors.centerIn: parent
                            text: "Create"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            id: confirmCreateModsFolderMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (modManager && pendingCreateModsFolderGames.length > 0) {
                                    modManager.createModsFoldersForGames(pendingCreateModsFolderGames, pendingCreateModsFolderType)
                                    gameScanDialog.rebuildDisplayGames()
                                    selectedGames = Object.assign({}, selectedGames)
                                    modsFolderRefreshToken = modsFolderRefreshToken + 1
                                }
                                createModsFolderWarningDialog.close()
                                gamePresetsManagerPage._continueAddSelectedGames()
                            }
                        }
                    }
                }
            }
        }
    }

    GamePresetsManagerGameScanDialog {
        id: gameScanPart
    }

    readonly property alias gameScanDialog: gameScanPart.gameScanDialog
    readonly property alias modsFolderTypeCombo: gameScanPart.modsFolderTypeCombo

    Connections {
        target: modManager
        function onGameScanActionMessage(message, success) {
            gameScanActionMessage = message
            gameScanActionSuccess = success
        }
        function onLuaProfileFolderResult(presetName, error) {

            if (error && error.length > 0) {
                gameScanActionMessage = "Lua folder for \"" + presetName + "\" failed: " + error
                gameScanActionSuccess = false
            } else {
                gameScanActionMessage = "Lua mods folder created for \"" + presetName + "\""
                gameScanActionSuccess = true
            }
        }
        function onGameUEVersionDetected(gamePath, version) {
            if (gameScanDialog) {
                gameScanDialog.ueVersionRefreshToken = gameScanDialog.ueVersionRefreshToken + 1
                gameScanDialog.rebuildDisplayGames()
            }
        }
        function onInstalledGamesListChanged() {
            if (gameScanDialog) {
                gameScanDialog.rebuildDisplayGames()
                gameScanDialog.ueVersionRefreshToken = gameScanDialog.ueVersionRefreshToken + 1
            }
        }
    }

    GamePresetsManagerContent {
        id: contentPart
    }

    GamePresetsManagerPresetDialogs {
        id: presetDialogsPart
    }

    readonly property alias createPresetDialog: presetDialogsPart.createPresetDialog
    readonly property alias deleteConfirmDialog: presetDialogsPart.deleteConfirmDialog
    readonly property alias renameDialog: presetDialogsPart.renameDialog

    GamePresetsManagerModsFolderDialogs {
        id: modsFolderDialogsPart
    }

    readonly property alias deleteModsFolderChoiceDialog: modsFolderDialogsPart.deleteModsFolderChoiceDialog
    readonly property alias deleteModsFolderChoiceModel: modsFolderDialogsPart.deleteModsFolderChoiceModel
    readonly property alias deleteModsFolderConfirmDialog: modsFolderDialogsPart.deleteModsFolderConfirmDialog
    readonly property alias modsFolderChoiceDialog: modsFolderDialogsPart.modsFolderChoiceDialog
    readonly property alias modsFolderChoiceModel: modsFolderDialogsPart.modsFolderChoiceModel
}
