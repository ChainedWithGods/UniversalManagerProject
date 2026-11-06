import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Pane {
    id: modLibraryRoot
    padding: 40
    bottomPadding: 0
    background: Rectangle { color: "transparent" }

    property string selectedModName: ""
    property string selectedDisplayName: ""
    property string hoveredModName: ""
    property var selectedModDetails: ({})
    property string selectedModDescription: ""
    property int imageRefreshTrigger: 0
    property string currentModImage: ""
    property int tagRefreshTrigger: 0
    property int favoriteRefreshTrigger: 0
    property int collectionStatusRefreshTrigger: 0
    property int installingModCount: 0
    property var currentTags: []
    property string viewMode: "cards"
    property var selectedMods: ([])

    readonly property bool selectionHasEnabled:
        modManager.modsActive >= 0
        && selectedMods.some(function(name) { return modManager.getModDetails(name).enabled })
    readonly property bool selectionHasDisabled:
        modManager.modsActive >= 0
        && selectedMods.some(function(name) { return !modManager.getModDetails(name).enabled })
    readonly property color toolbarContentColor: modManager.theme === "Cozy" ? window.textColor : "#FFFFFF"
    readonly property color toolbarOutlineColor:
        Qt.rgba(toolbarContentColor.r, toolbarContentColor.g, toolbarContentColor.b, 0.4)
    property int lastSelectedIndex: -1
    property string sortBy: "name"
    property bool sortAscending: true
    property var activeFilters: []
    property bool showAdvancedMenu: false
    property var quickModsFolderChoices: []
    property int quickModsFolderSelectedIndex: -1
    property string quickCurrentModsFolderName: ""
    property string quickCurrentModsFolderPath: ""
    property int quickCurrentModsFolderCount: 0
    property string quickTargetModsFolderName: ""
    property string quickTargetModsFolderPath: ""
    property int quickTargetModsFolderCount: 0
    property bool quickSwitchDisableAllMods: false
    property bool quickSwitchDisableAllCollections: false
    property bool quickSwitchShowAdvanced: false
    property var quickSwitchModsToDisable: []
    property var quickSwitchCollectionsToDisable: []
    property string quickSwitchAdvancedSearch: ""
    property string openedBundleId: ""
    property string bundleOriginViewMode: ""
    property int bundleStatusRevision: 0
    property string librarySearchText: ""
    property var expandedBundles: ({})

    property bool nexusUpdateCheckPending: false

    function runNexusUpdateCheck() {
        nexusUpdateResultDialog.close()
        nexusUpdateCheckPending = true
        modManager.checkNexusUpdates()
        if (!modManager.isCheckingNexusUpdates) {

            nexusUpdateCheckPending = false
            nexusUpdateResultDialog.show(0)
        }
    }

    onViewModeChanged: {
        if (viewMode !== "cards" && openedBundleId !== "")
            openedBundleId = ""
        filterModListInternal()
        refreshVisibleBundles()
    }

    readonly property var openedBundle: {
        var id = openedBundleId
        if (id === "")
            return ({})
        var bundles = modManager.modBundles || []
        for (var i = 0; i < bundles.length; ++i) {
            if (bundles[i].id === id)
                return bundles[i]
        }
        return ({})
    }

    property var visibleBundles: []

    onActiveFiltersChanged: refreshVisibleBundles()
    onSortByChanged: refreshVisibleBundles()
    onSortAscendingChanged: refreshVisibleBundles()
    onOpenedBundleIdChanged: refreshVisibleBundles()

    function refreshVisibleBundles() {
        var result = buildVisibleBundles()
        visibleBundles = Array.isArray(result) ? result : []
    }

    function bundleById(bundleId) {
        var bundles = modManager.modBundles || []
        for (var i = 0; i < bundles.length; ++i) {
            if (bundles[i].id === bundleId)
                return bundles[i]
        }
        return ({})
    }

    function modRowMatchesSearch(row, search) {
        if (!row || search === "")
            return true
        if ((row.name || "").toLowerCase().indexOf(search) !== -1)
            return true
        var details = modManager.getModDetails(row.name)
        if ((details.modId || "").toLowerCase().indexOf(search) !== -1)
            return true
        var tags = modManager.getModTags(row.name)
        for (var i = 0; i < tags.length; ++i) {
            if (tags[i].toLowerCase().indexOf(search) !== -1)
                return true
        }
        return false
    }

    function modRowPassesFilters(row) {
        if (!row)
            return false
        if (activeFilters.length === 0)
            return true

        var isEnabled = row.status === "Active"
        var modType = row.type || "folder"
        var imageKey = (row.type === "loose" && row.uniqueId) ? row.uniqueId : row.name
        var hasStatusFilter = activeFilters.indexOf("active") !== -1 || activeFilters.indexOf("disabled") !== -1
        var hasTypeFilter = activeFilters.indexOf("folder") !== -1 || activeFilters.indexOf("loose") !== -1
        var hasFavoriteFilter = activeFilters.indexOf("favorite") !== -1

        if (hasStatusFilter) {
            var matchesStatus = (activeFilters.indexOf("active") !== -1 && isEnabled)
                || (activeFilters.indexOf("disabled") !== -1 && !isEnabled)
            if (!matchesStatus)
                return false
        }
        if (hasTypeFilter) {
            var matchesType = (activeFilters.indexOf("folder") !== -1 && modType === "folder")
                || (activeFilters.indexOf("loose") !== -1 && modType === "loose")
            if (!matchesType)
                return false
        }
        if (hasFavoriteFilter && !modManager.isModFavorite(imageKey))
            return false
        return true
    }

    function buildVisibleBundles() {
        var tagRevision = tagRefreshTrigger
        var favoriteRevision = favoriteRefreshTrigger
        var statusRevision = bundleStatusRevision
        var sortField = sortBy
        var sortAsc = sortAscending
        var bundles = modManager.modBundles || []
        if (openedBundleId !== "" || bundles.length === 0)
            return []

        var search = String(librarySearchText || "").toLowerCase().trim()
        var filtersActive = activeFilters.length > 0

        var rows = {}
        var mods = modManager.modList || []
        for (var i = 0; i < mods.length; ++i) {
            if (mods[i] && mods[i].name)
                rows[mods[i].name] = mods[i]
        }

        var result = []
        for (var b = 0; b < bundles.length; ++b) {
            var bundle = bundles[b]
            var bundleTags = ""
            var tagList = bundle.tags || []
            for (var t = 0; t < tagList.length; ++t)
                bundleTags += " " + tagList[t]
            var bundleText = ((bundle.name || "") + " " + (bundle.folder || "") + " "
                              + (bundle.id || "") + bundleTags).toLowerCase()
            var bundleNameMatch = search !== "" && bundleText.indexOf(search) !== -1
            var searchOk = search === "" || bundleNameMatch
            var filterOk = !filtersActive
            var memberSearchMatch = false
            if (!searchOk || !filterOk) {
                var members = bundle.members || []
                for (var m = 0; m < members.length; ++m) {
                    var row = rows[members[m]]
                    if (!row)
                        continue
                    if (search !== "" && modRowMatchesSearch(row, search)) {
                        memberSearchMatch = true
                        searchOk = true
                    }
                    if (!filterOk && filtersActive && modRowPassesFilters(row))
                        filterOk = true
                    if (searchOk && filterOk)
                        break
                }
            }
            if (searchOk && filterOk) {
                bundle.expandOnSearch = search !== "" && !bundleNameMatch && memberSearchMatch
                result.push(bundle)
            }
        }

        result.sort(function(a, b) {
            if (sortField === "date") {
                var ad = a.date || ""
                var bd = b.date || ""
                return sortAsc ? (ad < bd ? -1 : (ad > bd ? 1 : 0))
                               : (ad > bd ? -1 : (ad < bd ? 1 : 0))
            }
            if (sortField === "size") {
                var as = a.sizeBytes || 0
                var bs = b.sizeBytes || 0
                return sortAsc ? as - bs : bs - as
            }
            if (sortField === "status") {
                var ar = a.memberCount > 0 ? a.activeCount / a.memberCount : 0
                var br = b.memberCount > 0 ? b.activeCount / b.memberCount : 0
                return sortAsc ? br - ar : ar - br
            }
            var an = (a.name || "").toLowerCase()
            var bn = (b.name || "").toLowerCase()
            return sortAsc ? an.localeCompare(bn) : bn.localeCompare(an)
        })
        return result
    }

    function openBundle(bundleId) {
        if (bundleId === "" || !bundleById(bundleId).id)
            return
        bundleOriginViewMode = viewMode
        openedBundleId = bundleId
        selectedMods = []
        lastSelectedIndex = -1
        viewMode = "cards"
        filterModListInternal()
        if (modScrollView)
            modScrollView.contentY = 0
    }

    function closeBundle() {
        if (openedBundleId === "")
            return
        var origin = bundleOriginViewMode
        bundleOriginViewMode = ""
        openedBundleId = ""
        selectedMods = []
        lastSelectedIndex = -1
        viewMode = origin !== "" ? origin : "cards"
        filterModListInternal()
        if (modScrollView)
            modScrollView.contentY = 0
    }

    function toggleBundle(bundleId) {
        var bundle = bundleById(bundleId)
        if (!bundle || !bundle.members)
            return
        var enable = (bundle.activeCount || 0) < (bundle.memberCount || 0)
        modManager.toggleModStatusBatch(bundle.members, enable)
        activeFilters = activeFilters.filter(function(value) {
            return value !== "active" && value !== "disabled"
        })
        filterModList()
        toastNotification.show((enable ? "Enabled " : "Disabled ") + bundle.memberCount
                               + (bundle.memberCount === 1 ? " mod" : " mods"),
                               enable ? "play.svg" : "pause.svg")
    }

    function openBundleTagDialog(bundleId) {
        if (!bundleById(bundleId).id)
            return
        modDialogs.openBundleTag(bundleId)
    }

    function requestBundleDelete(bundleId) {
        var bundle = bundleById(bundleId)
        if (!bundle || !bundle.members || bundle.members.length === 0)
            return
        if (deleteProgressDialog.visible)
            return

        var modsToDelete = []
        for (var i = 0; i < bundle.members.length; ++i)
            modsToDelete.push(bundle.members[i])

        selectedMods = modsToDelete
        lastSelectedIndex = -1
        deleteConfirmDialog.bundleName = bundle.name || ""
        deleteConfirmDialog.isBatchDelete = true
        deleteConfirmDialog.open()
    }

    function setBundleExpanded(bundleId, expanded) {
        if (!bundleId || !!expandedBundles[bundleId] === expanded)
            return
        var next = {}
        for (var key in expandedBundles)
            next[key] = expandedBundles[key]
        if (expanded)
            next[bundleId] = true
        else
            delete next[bundleId]
        expandedBundles = next
    }

    function setSelectedModContext(displayName, imageKey) {
        selectedDisplayName = displayName || ""
        selectedModName = imageKey || displayName || ""
    }

    function clearSelectedModContext() {
        selectedDisplayName = ""
        selectedModName = ""
    }

    function setQuickModsFolderTarget(index) {
        if (index < 0 || index >= quickModsFolderChoices.length)
            return

        var choice = quickModsFolderChoices[index]
        if (!choice || choice.current)
            return

        quickModsFolderSelectedIndex = index
        quickTargetModsFolderName = choice.name || ""
        quickTargetModsFolderPath = choice.path || ""
        quickTargetModsFolderCount = choice.count || 0
    }

    function currentProfileName() {
        var presets = modManager.gamePresets || []
        var activeId = modManager.activePresetId || ""
        for (var i = 0; i < presets.length; ++i) {
            if (presets[i] && presets[i].id === activeId) {
                return presets[i].name || ""
            }
        }
        return ""
    }

    function openQuickModsFolderSwitchDialog() {
        var choices = modManager.getAvailableModsFoldersForCurrentPreset()
        quickModsFolderChoices = choices || []
        quickModsFolderSelectedIndex = -1
        quickCurrentModsFolderName = ""
        quickCurrentModsFolderPath = ""
        quickCurrentModsFolderCount = 0
        quickTargetModsFolderName = ""
        quickTargetModsFolderPath = ""
        quickTargetModsFolderCount = 0
        quickSwitchDisableAllMods = false
        quickSwitchDisableAllCollections = false
        quickSwitchShowAdvanced = false
        quickSwitchModsToDisable = []
        quickSwitchCollectionsToDisable = []
        quickSwitchAdvancedSearch = ""

        if (!quickModsFolderChoices || quickModsFolderChoices.length === 0) {
            toastNotification.show("No mods folders were found for this profile", "folder-x.svg")
            return
        }

        var firstTargetIndex = -1
        for (var i = 0; i < quickModsFolderChoices.length; ++i) {
            var choice = quickModsFolderChoices[i]
            if (choice.current) {
                quickCurrentModsFolderName = choice.name || ""
                quickCurrentModsFolderPath = choice.path || ""
                quickCurrentModsFolderCount = choice.count || 0
            } else if (firstTargetIndex === -1) {
                firstTargetIndex = i
            }
        }

        if (firstTargetIndex === -1) {
            toastNotification.show("No other mods folder is available to switch to", "folder-x.svg")
            return
        }

        setQuickModsFolderTarget(firstTargetIndex)
        quickModsFolderSwitchDialog.open()
        focus = false
    }

    function quickSwitchEnabledMods() {
        var list = modManager.modList || []
        var result = []
        for (var i = 0; i < list.length; ++i) {
            var mod = list[i]
            if (mod && mod.status !== "Disabled") {
                var key = (mod.uniqueId && mod.uniqueId !== "") ? mod.uniqueId : (mod.name || "")
                if (key !== "") {
                    result.push({ key: key, name: mod.name || key, status: mod.status || "" })
                }
            }
        }
        return result
    }

    function quickSwitchCurrentPresetCollections() {
        var allCollections = modManager.collections || []
        var result = []
        var activePresetId = modManager.activePresetId || ""
        for (var i = 0; i < allCollections.length; ++i) {
            var collection = allCollections[i]
            if (!collection) continue
            if (activePresetId !== "" && collection.presetId !== activePresetId) continue
            result.push({
                id: collection.id || "",
                name: collection.name || "",
                modCount: collection.modCount || (collection.mods ? collection.mods.length : 0)
            })
        }
        return result
    }

    function quickSwitchToggleMod(key) {
        if (!key) return
        var list = quickSwitchModsToDisable.slice()
        var idx = list.indexOf(key)
        if (idx >= 0) {
            list.splice(idx, 1)
        } else {
            list.push(key)
        }
        quickSwitchModsToDisable = list
    }

    function quickSwitchToggleCollection(id) {
        if (!id) return
        var list = quickSwitchCollectionsToDisable.slice()
        var idx = list.indexOf(id)
        if (idx >= 0) {
            list.splice(idx, 1)
        } else {
            list.push(id)
        }
        quickSwitchCollectionsToDisable = list
    }

    function quickSwitchSelectAllMods() {
        var enabled = quickSwitchEnabledMods()
        var keys = []
        for (var i = 0; i < enabled.length; ++i) {
            keys.push(enabled[i].key)
        }
        quickSwitchModsToDisable = keys
    }

    function quickSwitchDeselectAllMods() {
        quickSwitchModsToDisable = []
    }

    function quickSwitchSelectAllCollections() {
        var collections = quickSwitchCurrentPresetCollections()
        var ids = []
        for (var i = 0; i < collections.length; ++i) {
            if (collections[i].id !== "") {
                ids.push(collections[i].id)
            }
        }
        quickSwitchCollectionsToDisable = ids
    }

    function quickSwitchDeselectAllCollections() {
        quickSwitchCollectionsToDisable = []
    }

    function quickSwitchFilteredMods() {
        var enabled = quickSwitchEnabledMods()
        var query = (quickSwitchAdvancedSearch || "").toLowerCase()
        if (query === "") return enabled
        var result = []
        for (var i = 0; i < enabled.length; ++i) {
            if ((enabled[i].name || "").toLowerCase().indexOf(query) >= 0) {
                result.push(enabled[i])
            }
        }
        return result
    }

    function loadSelectedModPreview() {
        var savedImagePath = modManager.getModImage(selectedModName)
        currentModImage = savedImagePath
        currentTags = modManager.getModTags(selectedModName)

        if (savedImagePath !== "") {
            if (savedImagePath.toLowerCase().endsWith(".gif")) {
                panelModGif.source = savedImagePath
                panelModImage.source = ""
            } else {
                panelModImage.source = savedImagePath
                panelModGif.source = ""
            }
        } else {
            panelModImage.source = ""
            panelModGif.source = ""
        }
    }

    function openSelectedModDetails(displayName, imageKey) {
        setSelectedModContext(displayName, imageKey)
        selectedModDetails = modManager.getModDetails(selectedDisplayName)
        selectedModDescription = modManager.getModDescription(selectedDisplayName)
        loadSelectedModPreview()
        modDetailsPanel.open()
    }

    function collectionHasEnabledMods(collection) {
        var _ = collectionStatusRefreshTrigger
        if (!collection || !collection.mods || collection.mods.length === 0)
            return false

        for (var i = 0; i < collection.mods.length; ++i) {
            var details = modManager.getModDetails(collection.mods[i])
            if (details && details.enabled)
                return true
        }

        return false
    }

    Item {
        id: focusItem
        width: 0
        height: 0
        focus: true
    }

    function clearFocus() {
        focusItem.forceActiveFocus()
    }

    Timer {
        id: filterDebounceTimer
        interval: 100
        repeat: false
        onTriggered: filterModListInternal()
    }

    function openInstallDialog() {
        installModDialog.open()
    }

    function openBackupDialog() {
        backupTypeDialog.open()
    }

    Component.onCompleted: {
        filterModList()
        refreshVisibleBundles()
    }

    MouseArea {
        anchors.fill: parent
        z: -1
        onClicked: {
            clearFocus()
            selectedMods = []
            lastSelectedIndex = -1
        }
    }

    Connections {
        target: modManager
        function onModImageChanged(modName) {

            imageRefreshTrigger++

            if (modName === selectedModName) {
                currentModImage = modManager.getModImage(selectedModName)
            }
        }
        function onModDescriptionChanged(modName) {
            if (modName === selectedDisplayName) {
                selectedModDescription = modManager.getModDescription(selectedDisplayName)
            }
        }
    }

    ModLibraryPageShortcuts {
        id: libraryShortcuts
    }

    function handleEscape() {
        if (openedBundleId !== "") {
            closeBundle()
            return
        }
        clearFocus()
        selectedMods = []
        lastSelectedIndex = -1
    }

    Keys.onEscapePressed: function(event) {
        handleEscape()
        event.accepted = true
    }

    Gradient {
        id: sharedCozyDialogGradient
        GradientStop { position: 0.0; color: window.cozyDialogGradientTop }
        GradientStop { position: 1.0; color: window.cozyDialogGradientBottom }
    }

    Gradient {
        id: sharedCrimsonDialogGradient
        GradientStop { position: 0.0; color: window.crimsonDialogGradientTop }
        GradientStop { position: 1.0; color: window.crimsonDialogGradientBottom }
    }

    Gradient {
        id: sharedWaveyDialogGradient
        GradientStop { position: 0.0; color: window.waveyDialogGradientTop }
        GradientStop { position: 1.0; color: window.waveyDialogGradientBottom }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        Column {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                RowLayout {
                    spacing: 12

                    Text {
                        text: "Mods Library"
                        font.pixelSize: 28
                        font.weight: 500
                        color: window.textColor
                    }

                    Rectangle {
                        Layout.preferredWidth: countText.contentWidth + 20
                        Layout.preferredHeight: 28
                        radius: 14
                        color: modManager.theme === "Glassmorphic" ?
                            Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.3) :
                            Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                        border.width: 1
                        border.color: window.primaryColor

                        Text {
                            id: countText
                            anchors.centerIn: parent
                            text: {
                                var total = modManager.modList ? modManager.modList.length : 0
                                var bundleCount = (modManager.modBundles || []).length
                                var summary = total + " mod" + (total !== 1 ? "s" : "")
                                if (bundleCount > 0)
                                    summary += " · " + bundleCount
                                        + (bundleCount === 1 ? " bundle" : " bundles")
                                return summary
                            }
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    id: advancedMenuButton
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 36
                    flat: true
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    scale: hovered ? 1.05 : 1.0

                    Behavior on scale {
                        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                    }

                    contentItem: Text {
                        text: "Advanced"
                        font.pixelSize: 13
                        font.weight: Font.Black
                        font.letterSpacing: 0.5
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: modManager.theme === "Glassmorphic"
                            ? Qt.rgba(1, 1, 1, advancedMenuButton.hovered ? 0.35 : 0.25)
                            : window.primaryColor
                        radius: 99
                        border.width: 2
                        border.color: modManager.theme === "Glassmorphic"
                            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, advancedMenuButton.hovered ? 0.8 : 0.6)
                            : window.primaryColor

                        Behavior on color { ColorAnimation { duration: 200 } }
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                    }

                    onClicked: showAdvancedMenu = !showAdvancedMenu
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    id: modLibrarySearchContainer
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 36
                    color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25) : window.surfaceColor
                    radius: window.cardRadius / 2
                    border.color: window.borderColor
                    border.width: window.borderWidth

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        TextField {
                            id: modSearchField
                            ContextMenu.menu: null
                            Layout.fillWidth: true
                            placeholderText: "Search Mods..."
                            font.pixelSize: 12
                            color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                            placeholderTextColor: window.textSecondaryColor
                            selectByMouse: true
                            persistentSelection: true
                            background: Rectangle { color: "transparent" }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                cursorShape: Qt.IBeamCursor
                                onPressed: function(mouse) {
                                    modSearchField.forceActiveFocus()
                                    modSearchContextMenu.popup(modLibrarySearchContainer, mouse.x, modLibrarySearchContainer.height + 4)
                                    mouse.accepted = true
                                }
                                onPressAndHold: function(mouse) {
                                    modSearchField.forceActiveFocus()
                                    modSearchContextMenu.popup(modLibrarySearchContainer, mouse.x, modLibrarySearchContainer.height + 4)
                                    mouse.accepted = true
                                }
                            }

                            onTextChanged: {
                                modLibraryRoot.librarySearchText = text
                                modLibraryRoot.refreshVisibleBundles()
                                filterModList()
                            }

                            Menu {
                                id: modSearchContextMenu
                                width: 280
                                padding: 8

                                background: Rectangle {
                                    color: modManager.theme === "Wavey" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(0.05, 0.05, 0.07, Math.max(0.88, Math.min(0.97, modManager.glassOpacity + 0.84))) : (modManager.theme === "Crimson" ? "transparent" : "transparent"))
                                    radius: 16
                                    border.width: 1
                                    border.color: modManager.theme === "Wavey" ? Qt.rgba(0.91, 0.12, 0.55, 0.4) : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.2) : window.borderColor)

                                    gradient: {
                                        if (modManager.theme === "Wavey") return waveySearchMenuGradient2
                                        if (modManager.theme === "Crimson") return crimsonSearchMenuGradient2
                                        if (modManager.theme === "Cozy") return cozySearchMenuGradient2
                                        return null
                                    }

                                    Gradient {
                                        id: waveySearchMenuGradient2
                                        GradientStop { position: 0.0; color: "#1b3185" }
                                        GradientStop { position: 1.0; color: "#0a2742" }
                                    }

                                    Gradient {
                                        id: crimsonSearchMenuGradient2
                                        GradientStop { position: 0.0; color: window.crimsonContextMenuGradientTop }
                                        GradientStop { position: 1.0; color: window.crimsonContextMenuGradientBottom }
                                    }

                                    Gradient {
                                        id: cozySearchMenuGradient2
                                        GradientStop { position: 0.0; color: window.cozyContextMenuGradientTop }
                                        GradientStop { position: 1.0; color: window.cozyContextMenuGradientBottom }
                                    }

                                    layer.enabled: true
                                    layer.effect: MultiEffect {
                                        shadowEnabled: true
                                        shadowColor: Qt.rgba(0, 0, 0, 0.4)
                                        shadowVerticalOffset: 8
                                        shadowBlur: 24
                                        blur: modManager.theme === "Glassmorphic" ? 0.5 : 0.0
                                        blurEnabled: modManager.theme === "Glassmorphic"
                                        blurMax: 32
                                    }
                                }

                                ContextMenuItem {
                                    text: "Undo"
                                    iconName: "undo-2.svg"
                                    shortcutText: "Ctrl+Z"
                                    enabled: modSearchField.canUndo
                                    onTriggered: modSearchField.undo()
                                }

                                ContextMenuItem {
                                    text: "Redo"
                                    iconName: "redo-2.svg"
                                    shortcutText: "Ctrl+Y"
                                    enabled: modSearchField.canRedo
                                    onTriggered: modSearchField.redo()
                                }

                                MenuSeparator {
                                    padding: 0
                                    topPadding: 4
                                    bottomPadding: 4
                                    contentItem: Rectangle {
                                        implicitHeight: 1
                                        implicitWidth: parent.width
                                        color: window.separatorColor
                                    }
                                }

                                ContextMenuItem {
                                    text: "Cut"
                                    iconName: "scissors.svg"
                                    shortcutText: "Ctrl+X"
                                    enabled: modSearchField.selectedText.length > 0
                                    onTriggered: modSearchField.cut()
                                }

                                ContextMenuItem {
                                    text: "Copy"
                                    iconName: "copy.svg"
                                    shortcutText: "Ctrl+C"
                                    enabled: modSearchField.selectedText.length > 0
                                    onTriggered: modSearchField.copy()
                                }

                                ContextMenuItem {
                                    text: "Paste"
                                    iconName: "clipboard.svg"
                                    shortcutText: "Ctrl+V"
                                    enabled: modSearchField.canPaste
                                    onTriggered: modSearchField.paste()
                                }

                                ContextMenuItem {
                                    text: "Delete"
                                    iconName: "trash.svg"
                                    shortcutText: "Del"
                                    enabled: modSearchField.selectedText.length > 0
                                    onTriggered: modSearchField.remove(modSearchField.selectionStart, modSearchField.selectionEnd)
                                }

                                MenuSeparator {
                                    padding: 0
                                    topPadding: 4
                                    bottomPadding: 4
                                    contentItem: Rectangle {
                                        implicitHeight: 1
                                        implicitWidth: parent.width
                                        color: window.separatorColor
                                    }
                                }

                                ContextMenuItem {
                                    text: "Select All"
                                    iconName: "text-select.svg"
                                    shortcutText: "Ctrl+A"
                                    enabled: modSearchField.text.length > 0
                                    onTriggered: modSearchField.selectAll()
                                }
                            }
                        }
                    }
                }

                ModLibraryFilters {
                    mode: "views"
                    Layout.preferredWidth: 300
                    Layout.preferredHeight: 44
                    viewMode: modLibraryRoot.viewMode
                    onViewModeRequested: function(mode) {
                        modLibraryRoot.clearFocus()
                        modLibraryRoot.viewMode = mode
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12
                visible: showAdvancedMenu

                Behavior on opacity {
                    NumberAnimation { duration: 200 }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Button {
                        id: installButton
                        text: "INSTALL MODS"
                        Layout.preferredWidth: 160
                        Layout.preferredHeight: 44
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: hovered ? 1.05 : 1.0

                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }

                        contentItem: Text {
                            text: "Install Mods"
                            font.pixelSize: 13
                            font.weight: Font.Black
                            font.letterSpacing: 0.5
                            color: modManager.theme === "Glassmorphic" ? window.accentColor : "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, installButton.hovered ? 0.20 : 0.12)
                                : window.primaryColor
                            radius: 99
                            border.width: 2
                            border.color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, installButton.hovered ? 0.8 : 0.5)
                                : window.primaryColor

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            installModDialog.open()
                            focus = false
                        }
                    }

                    Button {
                        id: backupButton
                        text: "BACKUP MODS"
                        Layout.preferredWidth: 155
                        Layout.preferredHeight: 44
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: hovered ? 1.05 : 1.0

                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }

                        contentItem: Text {
                            text: "Backup Mods"
                            font.pixelSize: 13
                            font.weight: Font.Black
                            font.letterSpacing: 0.5
                            color: modManager.theme === "Glassmorphic" ? window.accentColor : "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, backupButton.hovered ? 0.20 : 0.12)
                                : window.primaryColor
                            radius: 99
                            border.width: 2
                            border.color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, backupButton.hovered ? 0.8 : 0.5)
                                : window.primaryColor

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            backupTypeDialog.open()
                            focus = false
                        }
                    }

                    Button {
                        id: openFolderButton
                        text: "OPEN MODS FOLDER"
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 44
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: hovered ? 1.05 : 1.0

                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }

                        contentItem: Text {
                            text: "Open Mods Folder"
                            font.pixelSize: 13
                            font.weight: Font.Black
                            font.letterSpacing: 0.5
                            color: modManager.theme === "Glassmorphic" ? window.accentColor : "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, openFolderButton.hovered ? 0.20 : 0.12)
                                : window.primaryColor
                            radius: 99
                            border.width: 2
                            border.color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, openFolderButton.hovered ? 0.8 : 0.5)
                                : window.primaryColor

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: modManager.openModsDirectory()
                    }

                    Button {
                        id: switchModsFolderButton
                        text: "SWITCH MODS FOLDER"
                        Layout.preferredWidth: 190
                        Layout.preferredHeight: 44
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: hovered ? 1.05 : 1.0

                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }

                        contentItem: Text {
                            text: "Switch Mods Folder"
                            font.pixelSize: 13
                            font.weight: Font.Black
                            font.letterSpacing: 0.5
                            color: modManager.theme === "Glassmorphic" ? window.accentColor : "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, switchModsFolderButton.hovered ? 0.20 : 0.12)
                                : window.primaryColor
                            radius: 99
                            border.width: 2
                            border.color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, switchModsFolderButton.hovered ? 0.8 : 0.5)
                                : window.primaryColor

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: openQuickModsFolderSwitchDialog()
                    }

                    Button {
                        id: createModsFolderButton
                        text: "CREATE MODS FOLDER"
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 44
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: hovered ? 1.05 : 1.0

                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }

                        contentItem: Text {
                            text: "Create Mods Folder"
                            font.pixelSize: 13
                            font.weight: Font.Black
                            font.letterSpacing: 0.5
                            color: modManager.theme === "Glassmorphic" ? window.accentColor : "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, createModsFolderButton.hovered ? 0.20 : 0.12)
                                : window.primaryColor
                            radius: 99
                            border.width: 2
                            border.color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, createModsFolderButton.hovered ? 0.8 : 0.5)
                                : window.primaryColor

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            if (!modManager.activePresetId || modManager.activePresetId === "") {
                                toastNotification.show("No profile is selected", "folder-x.svg")
                                return
                            }
                            createModsFolderDialog.profileName = modLibraryRoot.currentProfileName()
                            createModsFolderDialog.open()
                            focus = false
                        }
                    }

                    Button {
                        id: collectionsButton
                        text: "COLLECTIONS"
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 44
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: hovered ? 1.05 : 1.0

                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }

                        contentItem: Text {
                            text: "Collections"
                            font.pixelSize: 13
                            font.weight: Font.Black
                            font.letterSpacing: 0.5
                            color: modManager.theme === "Glassmorphic" ? window.accentColor : "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, collectionsButton.hovered ? 0.20 : 0.12)
                                : window.primaryColor
                            radius: 99
                            border.width: 2
                            border.color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, collectionsButton.hovered ? 0.8 : 0.5)
                                : window.primaryColor

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            collectionsDialog.open()
                            focus = false
                        }
                    }

                    Button {
                        id: createBundleButton
                        text: "CREATE BUNDLE"
                        Layout.preferredWidth: 170
                        Layout.preferredHeight: 44
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: hovered ? 1.05 : 1.0

                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }

                        contentItem: Text {
                            text: "Create Bundle"
                            font.pixelSize: 13
                            font.weight: Font.Black
                            font.letterSpacing: 0.5
                            color: modManager.theme === "Glassmorphic" ? window.accentColor : "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, createBundleButton.hovered ? 0.20 : 0.12)
                                : window.primaryColor
                            radius: 99
                            border.width: 2
                            border.color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, createBundleButton.hovered ? 0.8 : 0.5)
                                : window.primaryColor

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            collectionDialogs.openCreateBundleDialog()
                            focus = false
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                ModLibraryFilters {
                    mode: "filters"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    viewMode: modLibraryRoot.viewMode
                    activeFilters: modLibraryRoot.activeFilters
                    sortBy: modLibraryRoot.sortBy
                    onFiltersChanged: function(filters) {
                        modLibraryRoot.activeFilters = filters
                        modLibraryRoot.filterModList()
                        modLibraryRoot.refreshVisibleBundles()
                    }
                    onSortRequested: function(field) {
                        modLibraryRoot.sortBy = field
                        modLibraryRoot.filterModList()
                        modLibraryRoot.refreshVisibleBundles()
                    }
                    onCheckUpdatesRequested: modLibraryRoot.runNexusUpdateCheck()
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 8
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: viewMode === "collections" ? 2
                : ((filteredModList.count > 0 || visibleBundles.length > 0 || openedBundleId !== "") ? 1 : 0)

            Rectangle {
                color: "transparent"

                Column {
                    anchors.centerIn: parent
                    spacing: 14

                    Item {
                        width: 64
                        height: 64
                        anchors.horizontalCenter: parent.horizontalCenter
                        opacity: modManager.theme === "Glassmorphic" ? 1.0 : 0.5

                        Image {
                            id: packageIcon
                            anchors.fill: parent
                            source: "qrc:/UniversalModManager/Lucide Icons/package.svg"
                            sourceSize.width: 128
                            sourceSize.height: 128
                            fillMode: Image.PreserveAspectFit
                            visible: modManager.theme !== "Glassmorphic"
                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: window.textSecondaryColor
                            }
                        }

                        Image {
                            anchors.fill: parent
                            source: "qrc:/UniversalModManager/Lucide Icons/package.svg"
                            sourceSize.width: 128
                            sourceSize.height: 128
                            fillMode: Image.PreserveAspectFit
                            visible: modManager.theme === "Glassmorphic"
                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: window.accentColor
                            }
                        }
                    }

                    Text {
                        text: "No Mods Found"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: window.textColor
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text {
                        text: "Install mods to see them here."
                        font.pixelSize: 12
                        color: window.textSecondaryColor
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                StackLayout {
                    anchors.fill: parent
                    currentIndex: viewMode === "details" ? 1 : 0

                    Item {
                        Flickable {
                            id: modScrollView
                            anchors.fill: parent
                            clip: true

                            contentWidth: width

                            readonly property real bundleHeaderHeight: bundleSection.visible
                                ? bundleSection.height + 10 : 0
                            contentHeight: Math.max(height, bundleHeaderHeight + modGridView.contentHeight)

                            function syncGridScroll() {
                                if (!modGridView || !bundleSection)
                                    return
                                const headerHeight = bundleHeaderHeight
                                modGridView.y = Math.max(headerHeight, contentY)
                                modGridView.contentY = Math.max(0, contentY - headerHeight)
                            }

                            onContentYChanged: syncGridScroll()
                            onHeightChanged: syncGridScroll()
                            onBundleHeaderHeightChanged: syncGridScroll()
                            Component.onCompleted: syncGridScroll()

                            boundsBehavior: Flickable.StopAtBounds
                            flickableDirection: Flickable.VerticalFlick

                            WheelHandler {
                                id: modScrollWheelHandler
                                target: modScrollView
                                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                onWheel: (event) => {

                                    var dy = 0
                                    if (event.pixelDelta && event.pixelDelta.y !== 0) {
                                        dy = event.pixelDelta.y
                                    } else {
                                        dy = event.angleDelta.y / 120 * 140
                                    }

                                    var nextY = modScrollView.contentY - dy
                                    nextY = Math.max(0, Math.min(nextY, modScrollView.contentHeight - modScrollView.height))

                                    modScrollView.cancelFlick()
                                    modScrollViewScrollAnim.to = nextY
                                    modScrollViewScrollAnim.restart()
                                    event.accepted = true
                                }
                            }

                            NumberAnimation {
                                id: modScrollViewScrollAnim
                                target: modScrollView
                                property: "contentY"
                                duration: 90
                                easing.type: Easing.OutCubic
                            }

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AlwaysOn
                                parent: modScrollView.parent
                                anchors.top: modScrollView.top
                                anchors.right: modScrollView.right
                                anchors.bottom: modScrollView.bottom
                            }

                            MouseArea {
                                anchors.fill: parent
                                z: -1
                                onClicked: {
                                    clearFocus()
                                    selectedMods = []
                                    lastSelectedIndex = -1
                                }
                                propagateComposedEvents: true
                            }

                            Item {
                                id: bundleSection
                                x: 10
                                width: modScrollView.width - 20
                                height: bundleSectionColumn.height
                                visible: bundleSectionColumn.height > 0
                                onHeightChanged: modScrollView.syncGridScroll()

                                Column {
                                    id: bundleSectionColumn
                                    width: parent.width
                                    spacing: 10

                                    ModBundleBreadcrumb {
                                        width: parent.width
                                        visible: modLibraryRoot.openedBundleId !== ""
                                        bundleData: modLibraryRoot.openedBundle
                                        onBackRequested: modLibraryRoot.closeBundle()
                                        onToggleRequested: function(enable) {
                                            modLibraryRoot.toggleBundle(modLibraryRoot.openedBundleId)
                                        }
                                        onFolderRequested: modManager.openBundleFolder(
                                            modLibraryRoot.openedBundleId)
                                        onDeleteRequested: modLibraryRoot.requestBundleDelete(
                                            modLibraryRoot.openedBundleId)
                                        onAddModsRequested: collectionDialogs.openAddModsForBundle(
                                            modLibraryRoot.openedBundleId,
                                            modLibraryRoot.openedBundle.name
                                                || modLibraryRoot.openedBundle.folder || "")
                                    }

                                    Repeater {
                                        model: modLibraryRoot.openedBundleId === ""
                                               && modLibraryRoot.viewMode === "bundles"
                                            ? modLibraryRoot.visibleBundles.length : 0

                                        delegate: Item {
                                            id: bundleCardHost
                                            property var bundle: modLibraryRoot.visibleBundles[index] || ({})
                                            width: parent ? parent.width : 0
                                            height: bundleCardItem.height

                                            ModBundleCard {
                                                id: bundleCardItem
                                                width: parent.width
                                                bundleId: bundleCardHost.bundle.id || ""
                                                bundleName: bundleCardHost.bundle.name || ""
                                                memberCount: bundleCardHost.bundle.memberCount || 0
                                                activeCount: bundleCardHost.bundle.activeCount || 0
                                                sizeText: bundleCardHost.bundle.size || ""
                                                dateText: bundleCardHost.bundle.date || ""
                                                folderPath: bundleCardHost.bundle.folderPath || ""
                                                tags: bundleCardHost.bundle.tags || []
                                                onOpenRequested: modLibraryRoot.openBundle(bundleId)
                                                onToggleRequested: function(enable) {
                                                    modLibraryRoot.toggleBundle(bundleId)
                                                }
                                                onFolderRequested: modManager.openBundleFolder(bundleId)
                                                onAddTagRequested: modLibraryRoot.openBundleTagDialog(bundleId)
                                            }
                                        }
                                    }

                                    Item {
                                        width: 1
                                        height: 10
                                        visible: modLibraryRoot.openedBundleId !== ""
                                            || (modLibraryRoot.viewMode === "bundles"
                                                && modLibraryRoot.visibleBundles.length > 0)
                                    }
                                }
                            }

                            GridView {
                                id: modGridView
                                anchors.horizontalCenter: parent.horizontalCenter

                                width: parent.width

                                height: modScrollView.height
                                y: 0
                                clip: true
                                model: filteredModList
                                cellWidth: width
                                cellHeight: 276
                                interactive: false

                                cacheBuffer: 600

                                onCountChanged: {
                                    forceLayout()
                                    modScrollView.syncGridScroll()
                                }

                                delegate: ModListItem {
                                    modName: model.name
                                    uniqueId: model.uniqueId || ""
                                    fileCount: model.fileCount
                                    modType: model.type || "folder"
                                    modExtensions: model.extensions || ""
                                    modEnabled: model.enabled
                                }
                            }
                        }
                    }

                    Item {
                        id: detailsViewContainer

                        Flickable {
                            id: modDetailsScrollView
                            anchors.fill: parent
                            clip: true
                            contentWidth: width
                            contentHeight: Math.max(height, detailsContentColumn.implicitHeight)
                            boundsBehavior: Flickable.StopAtBounds
                            flickableDirection: Flickable.VerticalFlick

                            MouseArea {
                                anchors.fill: parent
                                z: -1
                                onClicked: {
                                    clearFocus()
                                    selectedMods = []
                                    lastSelectedIndex = -1
                                }
                                propagateComposedEvents: true
                            }

                            readonly property real headerOffset: detailsHeaderLayout.implicitHeight
                                + detailsContentColumn.spacing
                                + (bundleListSection.visible
                                   ? bundleListSection.height + detailsContentColumn.spacing : 0)

                            function syncDetailsScroll() {
                                const offset = Math.max(0, contentY - headerOffset)
                                modDetailsListView.y = offset
                                modDetailsListView.contentY = offset
                            }

                            property real scrollTarget: 0
                            readonly property real wheelStepFactor: 3.5

                            function scrollByWheel(event) {
                                var dy = 0
                                if (event.pixelDelta && event.pixelDelta.y !== 0)
                                    dy = event.pixelDelta.y
                                else
                                    dy = event.angleDelta.y / 120 * 140
                                var maxY = Math.max(0, contentHeight - height)
                                var target = Math.max(0, Math.min(maxY, scrollTarget - dy * wheelStepFactor))
                                scrollTarget = target
                                var distance = Math.abs(target - contentY)
                                detailsScrollAnim.stop()
                                detailsScrollAnim.to = target
                                detailsScrollAnim.duration = Math.max(160, Math.min(500, distance * 0.3))
                                detailsScrollAnim.start()
                                event.accepted = true
                            }

                            onContentYChanged: {
                                if (!detailsScrollAnim.running)
                                    scrollTarget = contentY
                                syncDetailsScroll()
                            }
                            onMovementStarted: detailsScrollAnim.stop()
                            onHeightChanged: syncDetailsScroll()
                            onHeaderOffsetChanged: syncDetailsScroll()
                            Component.onCompleted: syncDetailsScroll()

                            NumberAnimation {
                                id: detailsScrollAnim
                                target: modDetailsScrollView
                                property: "contentY"
                                easing.type: Easing.OutQuart
                            }

                            WheelHandler {
                                id: detailsScrollWheelHandler
                                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                onWheel: (event) => modDetailsScrollView.scrollByWheel(event)
                            }

                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            Column {
                                id: detailsContentColumn
                                width: detailsViewContainer.width
                                spacing: 8

                                ColumnLayout {
                                    id: detailsHeaderLayout
                                    width: parent.width
                                    spacing: 8

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 52

                                        color: {

                                            if (modManager.theme === "Crimson")
                                                return window.themePalette.listHeaderColor
                                            if (selectedMods.length > 0)
                                                return window.primaryColor
                                            if (modManager.theme === "Glassmorphic")
                                                return Qt.rgba(
                                                    window.surfaceColor.r,
                                                    window.surfaceColor.g,
                                                    window.surfaceColor.b,
                                                    Math.max(0.14, Math.min(0.60, modManager.glassOpacity + 0.10))
                                                )
                                            return window.cardColor1
                                        }
                                        radius: 12
                                        border.width: 1
                                        border.color: selectedMods.length > 0 ? Qt.darker(window.primaryColor, 1.2) : (modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(1, 1, 1, Math.max(0.10, Math.min(0.28, modManager.glassOpacity + 0.06)))
                                            : Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.5))

                                        layer.enabled: true
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: Qt.rgba(0,0,0,0.05)
                                            shadowVerticalOffset: 2
                                            shadowBlur: 0.5
                                            blurEnabled: false
                                            blurMax: 16
                                            blur: 0.0
                                        }

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 24
                                            anchors.rightMargin: 24
                                            spacing: 16

                                            Text {
                                                Layout.preferredWidth: 40
                                                text: ""
                                                font.pixelSize: 13
                                                font.weight: Font.DemiBold
                                                color: selectedMods.length > 0 ? "#FFFFFF" : window.textSecondaryColor
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                Layout.preferredWidth: 300
                                                text: "NAME"
                                                font.pixelSize: 12
                                                font.weight: Font.ExtraBold
                                                font.letterSpacing: 0.5
                                                color: selectedMods.length > 0 ? "#FFFFFF" : window.textSecondaryColor
                                            }

                                            Text {
                                                Layout.preferredWidth: 105
                                                text: "STATUS"
                                                font.pixelSize: 12
                                                font.weight: Font.ExtraBold
                                                font.letterSpacing: 0.5
                                                color: selectedMods.length > 0 ? "#FFFFFF" : window.textSecondaryColor
                                                horizontalAlignment: Text.AlignHCenter
                                            }

                                            Text {
                                                Layout.preferredWidth: 100
                                                text: "TYPE"
                                                font.pixelSize: 12
                                                font.weight: Font.ExtraBold
                                                font.letterSpacing: 0.5
                                                color: selectedMods.length > 0 ? "#FFFFFF" : window.textSecondaryColor
                                                horizontalAlignment: Text.AlignHCenter
                                            }

                                            Text {
                                                Layout.preferredWidth: 100
                                                text: "FILES"
                                                font.pixelSize: 12
                                                font.weight: Font.ExtraBold
                                                font.letterSpacing: 0.5
                                                color: selectedMods.length > 0 ? "#FFFFFF" : window.textSecondaryColor
                                                horizontalAlignment: Text.AlignHCenter
                                            }

                                            Text {
                                                Layout.preferredWidth: 120
                                                text: "SIZE"
                                                font.pixelSize: 12
                                                font.weight: Font.ExtraBold
                                                font.letterSpacing: 0.5
                                                color: selectedMods.length > 0 ? "#FFFFFF" : window.textSecondaryColor
                                                horizontalAlignment: Text.AlignRight
                                            }

                                            Text {
                                                Layout.preferredWidth: 110
                                                text: "DATE"
                                                font.pixelSize: 12
                                                font.weight: Font.ExtraBold
                                                font.letterSpacing: 0.5
                                                color: selectedMods.length > 0 ? "#FFFFFF" : window.textSecondaryColor
                                                horizontalAlignment: Text.AlignRight
                                            }
                                        }
                                    }
                                }

                                Item {
                                    id: bundleListSection
                                    width: parent.width
                                    height: bundleListColumn.height
                                    visible: bundleListColumn.height > 0
                                    onHeightChanged: modDetailsScrollView.syncDetailsScroll()

                                    Column {
                                        id: bundleListColumn
                                        width: parent.width
                                        spacing: 8

                                        Repeater {
                                            model: modLibraryRoot.visibleBundles.length

                                            delegate: Item {
                                                id: bundleRowHost
                                                property var bundle: modLibraryRoot.visibleBundles[index] || ({})
                                                width: parent ? parent.width : 0
                                                height: bundleRowItem.height

                                                ModBundleListRow {
                                                    id: bundleRowItem
                                                    width: parent.width
                                                    bundleData: bundleRowHost.bundle
                                                    selectedMods: modLibraryRoot.selectedMods
                                                    statusRevision: modLibraryRoot.bundleStatusRevision
                                                    expanded: !!modLibraryRoot.expandedBundles[bundleRowHost.bundle.id]
                                                        || bundleRowHost.bundle.expandOnSearch === true
                                                    onToggleRequested: function(enable) {
                                                        modLibraryRoot.toggleBundle(bundleRowHost.bundle.id)
                                                    }
                                                    onFolderRequested: modManager.openBundleFolder(bundleRowHost.bundle.id)
                                                    onModActivated: function(modName) {
                                                        modLibraryRoot.openSelectedModDetails(modName, modName)
                                                    }
                                                    onBundleSelectionRequested: function(memberNames, additive) {
                                                        var names = memberNames || []
                                                        if (names.length === 0)
                                                            return
                                                        if (!additive) {
                                                            modLibraryRoot.selectedMods = names.slice()
                                                            modLibraryRoot.lastSelectedIndex = -1
                                                            return
                                                        }
                                                        var current = modLibraryRoot.selectedMods.slice()
                                                        var allSelected = true
                                                        for (var i = 0; i < names.length; ++i) {
                                                            if (current.indexOf(names[i]) === -1) {
                                                                allSelected = false
                                                                break
                                                            }
                                                        }
                                                        for (var j = 0; j < names.length; ++j) {
                                                            var at = current.indexOf(names[j])
                                                            if (allSelected) {
                                                                if (at !== -1)
                                                                    current.splice(at, 1)
                                                            } else if (at === -1) {
                                                                current.push(names[j])
                                                            }
                                                        }
                                                        modLibraryRoot.selectedMods = current
                                                        modLibraryRoot.lastSelectedIndex = -1
                                                    }
                                                    onModSelectionRequested: function(modName, additive) {
                                                        if (modName === "")
                                                            return
                                                        if (!additive) {
                                                            modLibraryRoot.selectedMods = [modName]
                                                            modLibraryRoot.lastSelectedIndex = -1
                                                            return
                                                        }
                                                        var current = modLibraryRoot.selectedMods.slice()
                                                        var at = current.indexOf(modName)
                                                        if (at === -1)
                                                            current.push(modName)
                                                        else
                                                            current.splice(at, 1)
                                                        modLibraryRoot.selectedMods = current
                                                        modLibraryRoot.lastSelectedIndex = -1
                                                    }
                                                    onExpansionRequested: function(wantExpanded) {
                                                        modLibraryRoot.setBundleExpanded(bundleRowHost.bundle.id, wantExpanded)
                                                        modDetailsScrollView.syncDetailsScroll()
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                Item {

                                    id: detailsListHost
                                    width: parent.width
                                    height: modDetailsListView.contentHeight

                                ListView {
                                    id: modDetailsListView
                                    width: parent.width
                                    height: modDetailsScrollView.height
                                    interactive: false
                                    clip: true

                                    model: filteredModList
                                    spacing: 8

                                    WheelHandler {
                                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                        onWheel: (event) => modDetailsScrollView.scrollByWheel(event)
                                    }

                                    cacheBuffer: 400
                                    onCountChanged: {
                                        forceLayout()
                                        modDetailsScrollView.syncDetailsScroll()
                                    }

                                    MouseArea {
                                        id: detailsDragArea
                                        anchors.fill: parent
                                        z: -1
                                        acceptedButtons: Qt.LeftButton
                                        preventStealing: true
                                        cursorShape: Qt.ArrowCursor

                                        property real dragStartX: 0
                                        property real dragStartY: 0
                                        property real dragCurrentX: 0
                                        property real dragCurrentY: 0
                                        property bool dragSelecting: false
                                        property bool dragMoved: false
                                        property var dragBaseSelection: []

                                        function applyDragSelection() {
                                            var left = Math.min(dragStartX, dragCurrentX)
                                            var right = Math.max(dragStartX, dragCurrentX)
                                            var top = Math.min(dragStartY, dragCurrentY) + modDetailsListView.contentY
                                            var bottom = Math.max(dragStartY, dragCurrentY) + modDetailsListView.contentY
                                            var names = []
                                            var content = modDetailsListView.contentItem
                                            if (content) {
                                                for (var i = 0; i < content.children.length; ++i) {
                                                    var child = content.children[i]
                                                    if (!child || child.modRowDelegate !== true)
                                                        continue
                                                    if (child.x < right && child.x + child.width > left
                                                            && child.y < bottom && child.y + child.height > top) {
                                                        names.push(child.modName)
                                                    }
                                                }
                                            }
                                            if (dragBaseSelection.length > 0) {
                                                var merged = dragBaseSelection.slice()
                                                for (var j = 0; j < names.length; ++j) {
                                                    if (merged.indexOf(names[j]) === -1)
                                                        merged.push(names[j])
                                                }
                                                selectedMods = merged
                                            } else {
                                                selectedMods = names
                                            }
                                            lastSelectedIndex = -1
                                        }

                                        onPressed: (mouse) => {
                                            dragStartX = mouse.x
                                            dragStartY = mouse.y
                                            dragCurrentX = mouse.x
                                            dragCurrentY = mouse.y
                                            dragMoved = false
                                            dragSelecting = false
                                            dragBaseSelection = (mouse.modifiers & Qt.ControlModifier)
                                                ? selectedMods.slice() : []
                                        }

                                        onPositionChanged: (mouse) => {
                                            if (!pressed)
                                                return
                                            dragCurrentX = mouse.x
                                            dragCurrentY = mouse.y
                                            if (!dragMoved
                                                    && Math.abs(mouse.x - dragStartX) < 4
                                                    && Math.abs(mouse.y - dragStartY) < 4)
                                                return
                                            dragMoved = true
                                            dragSelecting = true
                                            applyDragSelection()
                                        }

                                        onReleased: {
                                            if (dragSelecting) {
                                                applyDragSelection()
                                                dragSelecting = false
                                            } else {
                                                clearFocus()
                                                selectedMods = []
                                                lastSelectedIndex = -1
                                            }
                                            dragMoved = false
                                        }

                                        onCanceled: {
                                            dragSelecting = false
                                            dragMoved = false
                                        }
                                    }

                                    Rectangle {
                                        visible: detailsDragArea.dragSelecting
                                        x: Math.min(detailsDragArea.dragStartX, detailsDragArea.dragCurrentX)
                                        y: Math.min(detailsDragArea.dragStartY, detailsDragArea.dragCurrentY)
                                        width: Math.abs(detailsDragArea.dragCurrentX - detailsDragArea.dragStartX)
                                        height: Math.abs(detailsDragArea.dragCurrentY - detailsDragArea.dragStartY)
                                        color: Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                                                       window.primaryColor.b, 0.12)
                                        border.width: 1
                                        border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                                                              window.primaryColor.b, 0.8)
                                        radius: 2
                                        z: 50
                                    }

                                delegate: Rectangle {
                                    id: delegateRoot
                                    width: modDetailsListView.width
                                    height: 72
                                    color: "transparent"
                                    radius: 12

                                    property bool modRowDelegate: true
                                    property string modName: model.name
                                    property bool isSelected: selectedMods.indexOf(model.name) !== -1

                                    Rectangle {
                                        id: rowHighlight
                                        anchors.fill: parent
                                        radius: 12
                                        color: {
                                            if (delegateRoot.isSelected) {
                                                return Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                                            }
                                            if (modManager.theme === "Glassmorphic") {
                                                return Qt.rgba(1, 1, 1, 0.15)
                                            } else if (modManager.theme === "Wavey") {
                                                return Qt.rgba(window.cardColor1.r, window.cardColor1.g, window.cardColor1.b, 0.4)
                                            } else if (modManager.theme === "Cozy") {
                                                return Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08)
                                            } else {
                                                return Qt.rgba(window.cardColor1.r, window.cardColor1.g, window.cardColor1.b, 0.3)
                                            }
                                        }

                                        border.width: delegateRoot.isSelected ? 2 : 1
                                        border.color: delegateRoot.isSelected ? window.primaryColor :
                                                      (modManager.theme === "Glassmorphic" ?
                                                      Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4) :
                                                      Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.3))

                                        opacity: (delegateRoot.isSelected || detailMouseArea.containsMouse) ? 1.0 : 0.0
                                        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
                                    }

                                    Menu {
                                        id: contextMenu

                                        width: 280
                                        padding: 8

                                        background: Rectangle {
                                            color: modManager.theme === "Wavey" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(0.12, 0.12, 0.15, Math.max(0.68, Math.min(0.82, modManager.glassOpacity + 0.32))) : (modManager.theme === "Crimson" ? "transparent" : "transparent"))
                                            radius: 16
                                            border.width: 1
                                            border.color: modManager.theme === "Wavey" ? Qt.rgba(0.91, 0.12, 0.55, 0.4) : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.2) : window.borderColor)

                                            gradient: {
                                                if (modManager.theme === "Wavey") return waveyMenuGradient
                                                if (modManager.theme === "Crimson") return crimsonMenuGradient
                                                if (modManager.theme === "Cozy") return cozyMenuGradient
                                                return null
                                            }

                                            Gradient {
                                                id: waveyMenuGradient
                                            GradientStop { position: 0.0; color: '#1b3185' }
                                            GradientStop { position: 1.0; color: '#0a2742' }
                                            }

                                            Gradient {
                                                id: crimsonMenuGradient
                                                GradientStop { position: 0.0; color: window.crimsonContextMenuGradientTop }
                                                GradientStop { position: 1.0; color: window.crimsonContextMenuGradientBottom }
                                            }

                                            Gradient {
                                                id: cozyMenuGradient
                                                GradientStop { position: 0.0; color: window.cozyContextMenuGradientTop }
                                                GradientStop { position: 1.0; color: window.cozyContextMenuGradientBottom }
                                            }

                                            layer.enabled: true
                                            layer.effect: MultiEffect {
                                                shadowEnabled: true
                                                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                                                shadowVerticalOffset: 8
                                                shadowBlur: 24
                                                blur: modManager.theme === "Glassmorphic" ? 0.5 : 0.0
                                                blurEnabled: modManager.theme === "Glassmorphic"
                                                blurMax: 32
                                            }
                                        }

                                        ContextMenuItem {
                                            text: "Enable Selected"
                                            iconName: "play.svg"
                                            shortcutText: window.shortcutEnableAll
                                            visible: selectedMods.length > 0
                                            onTriggered: {

                                                var modsToEnable = []
                                                for (var i = 0; i < selectedMods.length; i++) {
                                                    var modName = selectedMods[i]
                                                    if (!modManager.getModDetails(modName).enabled) {
                                                        modsToEnable.push(modName)
                                                    }
                                                }
                                                if (modsToEnable.length > 0) {
                                                    modManager.toggleModStatusBatch(modsToEnable, true)

                                                    var newFilters = activeFilters.filter(f => f !== "active" && f !== "disabled")
                                                    activeFilters = newFilters
                                                    toastNotification.show("Enabled " + modsToEnable.length + (modsToEnable.length === 1 ? " mod" : " mods"), "play.svg")
                                                }
                                            }
                                        }

                                        ContextMenuItem {
                                            text: "Disable Selected"
                                            iconName: "pause.svg"
                                            shortcutText: window.shortcutDisableAll
                                            visible: selectedMods.length > 0
                                            onTriggered: {

                                                var modsToDisable = []
                                                for (var i = 0; i < selectedMods.length; i++) {
                                                    var modName = selectedMods[i]
                                                    if (modManager.getModDetails(modName).enabled) {
                                                        modsToDisable.push(modName)
                                                    }
                                                }
                                                if (modsToDisable.length > 0) {
                                                    modManager.toggleModStatusBatch(modsToDisable, false)

                                                    var newFilters = activeFilters.filter(f => f !== "active" && f !== "disabled")
                                                    activeFilters = newFilters
                                                    toastNotification.show("Disabled " + modsToDisable.length + (modsToDisable.length === 1 ? " mod" : " mods"), "pause.svg")
                                                }
                                            }
                                        }

                                        MenuSeparator {
                                            visible: selectedMods.length > 0
                                            padding: 0
                                            topPadding: 4
                                            bottomPadding: 4
                                            contentItem: Rectangle {
                                                implicitHeight: 1
                                                implicitWidth: parent.width
                                                color: window.separatorColor
                                            }
                                        }

                                        ContextMenuItem {
                                            text: "View Details"
                                            iconName: "info.svg"
                                            visible: selectedMods.length <= 1
                                            onTriggered: {
                                                var imageKey = (model.type === "loose" && model.uniqueId) ? model.uniqueId : model.name
                                                openSelectedModDetails(model.name, imageKey)
                                            }
                                        }

                                        ContextMenuItem {
                                            text: "View in Assets Viewer"
                                            iconName: "package.svg"
                                            shortcutText: window.shortcutShowInPakViewer
                                            visible: selectedMods.length <= 1
                                            onTriggered: {
                                                var pakPath = modManager.getModPakPath(model.name)
                                                if (pakPath && pakPath !== "") {
                                                    modManager.launchPakViewer(pakPath)
                                                    toastNotification.show("Launching Assets Viewer...", "scan.svg")
                                                } else {
                                                    toastNotification.show("No .pak file found for this mod", "file-x.svg")
                                                }
                                            }
                                        }

                                        ContextMenuItem {
                                            text: "Export Mod Structure"
                                            iconName: "share.svg"
                                            shortcutText: window.shortcutExportModStructure
                                            visible: selectedMods.length <= 1
                                            onTriggered: {
                                                var missing = []
                                                if (!modManager.gamePaksPath || modManager.gamePaksPath === "") {
                                                    missing.push("Game Paks folder")
                                                }
                                                if (!modManager.usmapFilePath || modManager.usmapFilePath === "") {
                                                    missing.push("USMAP file")
                                                }

                                                if (missing.length > 0) {
                                                    var msg = "Set " + missing.join(" and ") + " in Settings before exporting"
                                                    toastNotification.show(msg, "circle-x.svg")
                                                    return
                                                }

                                                modManager.exportModStructure(model.name)
                                                toastNotification.show("Exporting mod structure...", "share.svg")
                                            }
                                        }

                                        ContextMenuItem {
                                            text: "Enable/Disable"
                                            iconName: model.enabled ? "pause.svg" : "play.svg"
                                            shortcutText: window.shortcutToggleMod
                                            visible: selectedMods.length <= 1
                                            onTriggered: modManager.toggleModStatus(model.name)
                                        }

                                        MenuSeparator {
                                            visible: selectedMods.length <= 1
                                            padding: 0
                                            topPadding: 4
                                            bottomPadding: 4
                                            contentItem: Rectangle {
                                                implicitHeight: 1
                                                implicitWidth: parent.width
                                                color: window.separatorColor
                                            }
                                        }

                                        ContextMenuItem {
                                            text: "Add Preview"
                                            iconName: "image.svg"
                                            visible: selectedMods.length <= 1
                                            onTriggered: {
                                                var imageKey = (model.type === "loose" && model.uniqueId) ? model.uniqueId : model.name
                                                setSelectedModContext(model.name, imageKey)
                                                imageFileDialog.open()
                                            }
                                        }

                                        ContextMenuItem {
                                            text: "Manage Tags"
                                            iconName: "hash.svg"
                                            visible: selectedMods.length <= 1
                                            onTriggered: {
                                                var imageKey = (model.type === "loose" && model.uniqueId) ? model.uniqueId : model.name
                                                openSelectedModDetails(model.name, imageKey)
                                            }
                                        }
                                    }

                                    MouseArea {
                                        id: detailMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        cursorShape: Qt.PointingHandCursor
                                        propagateComposedEvents: false

                                        onEntered: {

                                            var uniqueId = model.uniqueId || ""
                                            var modName = model.name || ""
                                            setSelectedModContext(modName, (model.type === "loose" && uniqueId !== "") ? uniqueId : modName)
                                            hoveredModName = modName
                                        }

                                        onExited: {

                                            hoveredModName = ""

                                            if (selectedModDetails.name === "") {
                                                clearSelectedModContext()
                                            }
                                        }

                                        onPressed: (mouse) => {
                                            mouse.accepted = true
                                        }

                                        onClicked: (mouse) => {
                                            mouse.accepted = true

                                            if (mouse.button === Qt.RightButton) {

                                                if (selectedMods.indexOf(model.name) === -1) {
                                                    selectedMods = [model.name]
                                                    lastSelectedIndex = index
                                                }
                                                contextMenu.popup()
                                            } else if (mouse.button === Qt.LeftButton) {
                                                if (mouse.modifiers & Qt.ControlModifier) {

                                                    var modsCopy = selectedMods.slice()
                                                    var modIndex = modsCopy.indexOf(model.name)
                                                    if (modIndex === -1) {
                                                        modsCopy.push(model.name)
                                                    } else {
                                                        modsCopy.splice(modIndex, 1)
                                                    }
                                                    selectedMods = modsCopy
                                                    lastSelectedIndex = index
                                                } else if (mouse.modifiers & Qt.ShiftModifier) {

                                                    if (lastSelectedIndex !== -1) {
                                                        var startIdx = Math.min(lastSelectedIndex, index)
                                                        var endIdx = Math.max(lastSelectedIndex, index)

                                                        var modsCopy = []
                                                        for (var i = startIdx; i <= endIdx && i < filteredModList.count; i++) {
                                                            modsCopy.push(filteredModList.get(i).name)
                                                        }
                                                        selectedMods = modsCopy
                                                    } else {
                                                        selectedMods = [model.name]
                                                        lastSelectedIndex = index
                                                    }
                                                } else {

                                                    selectedMods = [model.name]
                                                    lastSelectedIndex = index
                                                }
                                            }
                                        }

                                        onDoubleClicked: (mouse) => {
                                            mouse.accepted = true
                                            if (mouse.button === Qt.LeftButton) {

                                                var imageKey = (model.type === "loose" && model.uniqueId) ? model.uniqueId : model.name
                                                openSelectedModDetails(model.name, imageKey)
                                            }
                                        }
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 20
                                        anchors.rightMargin: 20
                                        spacing: 15

                                        Rectangle {
                                            Layout.preferredWidth: 40
                                            Layout.preferredHeight: 40
                                            color: modManager.theme === "Cozy" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.2) : window.cardColor1)
                                            radius: 8

                                            Image {
                                                anchors.centerIn: parent
                                                source: "qrc:/UniversalModManager/Lucide Icons/" + (model.type === "loose" ? "file.svg" : "package.svg")
                                                sourceSize.width: 36
                                                sourceSize.height: 36
                                                fillMode: Image.PreserveAspectFit
                                                layer.enabled: true
                                                layer.effect: ColorOverlay {
                                                    color: model.type === "loose" ? window.accentColor : window.primaryColor
                                                }
                                            }
                                        }

                                        RowLayout {
                                            id: listNameRow
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 300
                                            spacing: 8

                                            readonly property var nexusUpdate: {
                                                var revision = modManager.nexusUpdatesRevision
                                                return modManager.nexusUpdateForMod(model.name)
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                text: model.name
                                                font.pixelSize: 14
                                                font.weight: Font.Medium
                                                color: window.textColor
                                                elide: Text.ElideRight
                                            }

                                            UpdateAvailableBadge {
                                                id: nameUpdateBadge
                                                compact: true
                                                Layout.preferredHeight: 22
                                                visible: listNameRow.nexusUpdate.available === true
                                                tooltipText: "Click to download version " + (listNameRow.nexusUpdate.latestVersion || "update")
                                                onClicked: modManager.downloadNexusMod(listNameRow.nexusUpdate.modId,
                                                                                        listNameRow.nexusUpdate.latestFileId,
                                                                                        listNameRow.nexusUpdate.name)
                                            }
                                        }

                                        Rectangle {
                                            Layout.preferredWidth: 105
                                            Layout.preferredHeight: 36
                                            radius: 18
                                            color: model.enabled ? "#4CAF50" : "#9E9E9E"
                                            border.width: 0

                                            layer.enabled: true
                                            layer.effect: MultiEffect {
                                                shadowEnabled: true
                                                shadowColor: Qt.rgba(0, 0, 0, 0.15)
                                                shadowHorizontalOffset: 0
                                                shadowVerticalOffset: 2
                                                shadowBlur: 0.3
                                            }

                                            Text {
                                                anchors.centerIn: parent
                                                text: model.enabled ? "● Active" : "● Disabled"
                                                font.pixelSize: 12
                                                font.weight: Font.Medium
                                                color: "#FFFFFF"
                                            }
                                        }

                                        Text {
                                            Layout.preferredWidth: 100
                                            text: model.type === "loose" ? "Loose" : "Folder"
                                            font.pixelSize: 13
                                            font.weight: Font.Bold
                                            color: model.type === "loose" ? "#9C27B0" : "#2196F3"
                                            horizontalAlignment: Text.AlignHCenter
                                        }

                                        Text {
                                            Layout.preferredWidth: 100
                                            text: model.fileCount + (model.fileCount === 1 ? " file" : " files")
                                            font.pixelSize: 13
                                            color: window.textSecondaryColor
                                            horizontalAlignment: Text.AlignHCenter
                                        }

                                        Text {
                                            Layout.preferredWidth: 120
                                            text: modManager.getModDetails(model.name).size || "N/A"
                                            font.pixelSize: 13
                                            color: window.textSecondaryColor
                                            horizontalAlignment: Text.AlignRight
                                        }

                                        Text {
                                            Layout.preferredWidth: 110
                                            text: modManager.getModDetails(model.name).installDate || "N/A"
                                            font.pixelSize: 13
                                            color: window.textSecondaryColor
                                            horizontalAlignment: Text.AlignRight
                                        }
                                    }
                                }
                            }
                            }
                        }
                        }

                        Rectangle {
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 64
                            visible: selectedMods.length > 0
                            z: 100

                            color: modManager.theme === "Crimson"
                                ? window.themePalette.listHeaderColor
                                : (modManager.theme === "Cozy"
                                    ? Qt.rgba(window.surfaceColor.r * 0.74 + window.primaryColor.r * 0.26,
                                              window.surfaceColor.g * 0.74 + window.primaryColor.g * 0.26,
                                              window.surfaceColor.b * 0.74 + window.primaryColor.b * 0.26,
                                              1)
                                    : Qt.darker(window.primaryColor, 1.5))
                            radius: 12
                            border.width: 1
                            border.color: modManager.theme === "Crimson"
                                ? window.borderColor
                                : (modManager.theme === "Cozy"
                                    ? Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                                              window.primaryColor.b, 0.55)
                                    : Qt.darker(window.primaryColor, 1.75))

                            layer.enabled: true
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Qt.rgba(0,0,0,0.2)
                                shadowVerticalOffset: 4
                                shadowBlur: 1.0
                                blurEnabled: false
                                blurMax: 24
                                blur: 0.0
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 24
                                anchors.rightMargin: 24
                                spacing: 12

                                Text {
                                    text: selectedMods.length + " selected"
                                    Layout.alignment: Qt.AlignVCenter
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    color: modLibraryRoot.toolbarContentColor
                                }

                                Item { Layout.fillWidth: true }

                                ExpandingActionButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.preferredWidth: implicitWidth
                                    Layout.preferredHeight: implicitHeight
                                    visible: modLibraryRoot.selectionHasDisabled
                                    label: "Enable"
                                    iconSource: "qrc:/UniversalModManager/Lucide Icons/play.svg"
                                    accentColor: window.accentColor
                                    idleColor: "transparent"
                                    idleBorderColor: modLibraryRoot.toolbarOutlineColor
                                    idleContentColor: modLibraryRoot.toolbarContentColor
                                    onClicked: {
                                        var modsToEnable = []
                                        for (var i = 0; i < selectedMods.length; i++) {
                                            var modName = selectedMods[i]
                                            if (!modManager.getModDetails(modName).enabled) {
                                                modsToEnable.push(modName)
                                            }
                                        }
                                        if (modsToEnable.length > 0) {
                                            modManager.toggleModStatusBatch(modsToEnable, true)
                                            var newFilters = activeFilters.filter(f => f !== "active" && f !== "disabled")
                                            activeFilters = newFilters
                                            toastNotification.show("Enabled " + modsToEnable.length + (modsToEnable.length === 1 ? " mod" : " mods"), "play.svg")
                                        }
                                    }
                                }

                                ExpandingActionButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.preferredWidth: implicitWidth
                                    Layout.preferredHeight: implicitHeight
                                    visible: modLibraryRoot.selectionHasEnabled
                                    label: "Disable"
                                    iconSource: "qrc:/UniversalModManager/Lucide Icons/pause.svg"
                                    accentColor: window.accentColor
                                    idleColor: "transparent"
                                    idleBorderColor: modLibraryRoot.toolbarOutlineColor
                                    idleContentColor: modLibraryRoot.toolbarContentColor
                                    onClicked: {
                                        var modsToDisable = []
                                        for (var i = 0; i < selectedMods.length; i++) {
                                            var modName = selectedMods[i]
                                            if (modManager.getModDetails(modName).enabled) {
                                                modsToDisable.push(modName)
                                            }
                                        }
                                        if (modsToDisable.length > 0) {
                                            modManager.toggleModStatusBatch(modsToDisable, false)
                                            var newFilters = activeFilters.filter(f => f !== "active" && f !== "disabled")
                                            activeFilters = newFilters
                                            toastNotification.show("Disabled " + modsToDisable.length + (modsToDisable.length === 1 ? " mod" : " mods"), "pause.svg")
                                        }
                                    }
                                }

                                ExpandingActionButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.preferredWidth: implicitWidth
                                    Layout.preferredHeight: implicitHeight
                                    visible: modLibraryRoot.openedBundleId !== ""
                                    label: "Remove"
                                    iconSource: "qrc:/UniversalModManager/Lucide Icons/folder-minus.svg"
                                    accentColor: window.accentColor
                                    idleColor: "transparent"
                                    idleBorderColor: modLibraryRoot.toolbarOutlineColor
                                    idleContentColor: modLibraryRoot.toolbarContentColor
                                    onClicked: {
                                        var members = []
                                        for (var i = 0; i < selectedMods.length; i++) {
                                            if ((modManager.getModDetails(selectedMods[i]).bundleId || "")
                                                    === modLibraryRoot.openedBundleId)
                                                members.push(selectedMods[i])
                                        }
                                        if (members.length === 0) {
                                            toastNotification.show("Select bundle members to remove", "folder-minus.svg")
                                            return
                                        }
                                        var error = modManager.removeModsFromBundle(
                                            modLibraryRoot.openedBundleId, members)
                                        if (error && error.length > 0)
                                            toastNotification.show(error, 6000)
                                        else
                                            toastNotification.show(
                                                members.length === 1
                                                    ? "Removed from bundle"
                                                    : "Removed " + members.length + " mods from bundle",
                                                "check.svg")
                                    }
                                }

                                ExpandingActionButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.preferredWidth: implicitWidth
                                    Layout.preferredHeight: implicitHeight
                                    label: "Clear"
                                    iconSource: "qrc:/UniversalModManager/Lucide Icons/circle-x.svg"
                                    accentColor: window.accentColor
                                    idleColor: "transparent"
                                    idleBorderColor: modLibraryRoot.toolbarOutlineColor
                                    idleContentColor: modLibraryRoot.toolbarContentColor
                                    onClicked: selectedMods = []
                                }

                                ExpandingActionButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.preferredWidth: implicitWidth
                                    Layout.preferredHeight: implicitHeight
                                    label: "Delete"
                                    iconSource: "qrc:/UniversalModManager/Lucide Icons/trash-2.svg"
                                    accentColor: window.accentColor
                                    idleColor: "transparent"
                                    idleBorderColor: modLibraryRoot.toolbarOutlineColor
                                    idleContentColor: modLibraryRoot.toolbarContentColor
                                    onClicked: {
                                        if (deleteProgressDialog.visible)
                                            return
                                        deleteConfirmDialog.isBatchDelete = true
                                        deleteConfirmDialog.open()
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item {
                StackLayout {
                    anchors.fill: parent
                    currentIndex: modManager.collections && modManager.collections.length > 0 ? 1 : 0

                    Rectangle {
                        color: "transparent"

                        Column {
                            anchors.centerIn: parent
                            spacing: 20

                            Item {
                                width: 64
                                height: 64
                                anchors.horizontalCenter: parent.horizontalCenter
                                opacity: 0.5

                                Image {
                                    anchors.fill: parent
                                    source: "qrc:/UniversalModManager/Lucide Icons/folder.svg"
                                    sourceSize.width: 128
                                    sourceSize.height: 128
                                    fillMode: Image.PreserveAspectFit
                                    layer.enabled: true
                                    layer.effect: ColorOverlay {
                                        color: window.textSecondaryColor
                                    }
                                }
                            }

                            Text {
                                text: "No Collections Yet"
                                font.pixelSize: 16
                                font.weight: Font.Medium
                                color: window.textColor
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            Text {
                                text: "Create a collection to organize your mods"
                                font.pixelSize: 13
                                color: window.textSecondaryColor
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            Button {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "+ New Collection"
                                flat: true
                                hoverEnabled: true

                                contentItem: Text {
                                    text: parent.text
                                    font.pixelSize: 13
                                    font.weight: Font.Bold
                                    color: "#FFFFFF"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                background: Rectangle {
                                    color: window.accentColor
                                    radius: 12
                                    implicitWidth: 160
                                    implicitHeight: 40
                                }

                                onClicked: {
                                    collectionsDialog.open()
                                    focus = false
                                }
                            }
                        }
                    }

                    Item {
                        Flickable {
                            id: collectionsScrollView
                            anchors.fill: parent
                            clip: true

                            contentWidth: width
                            contentHeight: Math.max(height, collectionsGridView.contentHeight)
                            onContentYChanged: collectionsGridView.contentY = contentY
                            onHeightChanged: collectionsGridView.contentY = contentY

                            boundsBehavior: Flickable.StopAtBounds
                            flickableDirection: Flickable.VerticalFlick

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AlwaysOn
                                parent: collectionsScrollView.parent
                                anchors.top: collectionsScrollView.top
                                anchors.right: collectionsScrollView.right
                                anchors.bottom: collectionsScrollView.bottom
                            }

                            MouseArea {
                                anchors.fill: parent
                                z: -1
                                onClicked: clearFocus()
                                propagateComposedEvents: true
                            }

                            GridView {
                                id: collectionsGridView
                                anchors.horizontalCenter: parent.horizontalCenter
                                model: modManager.collections
                                cellWidth: 412
                                cellHeight: 245
                                interactive: false
                                property int availableColumns: Math.max(1, Math.floor(parent.width / cellWidth))
                                property int gridColumns: Math.max(1, Math.min(4, availableColumns))
                                width: gridColumns * cellWidth
                                height: collectionsScrollView.height
                                y: collectionsScrollView.contentY

                                cacheBuffer: 600

                                onCountChanged: {
                                    forceLayout()
                                    collectionsGridView.contentY = collectionsScrollView.contentY
                                }

                                delegate: Rectangle {
                                    id: collectionCard
                                    width: 382
                                    height: 215
                                    property bool isCozyTheme: modManager.theme === "Cozy"
                                    property bool isCrimsonTheme: modManager.theme === "Crimson"
                                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : (collectionCard.hasPreview ? window.surfaceColor : window.cardColor1))
                                    radius: 0

                                    clip: false
                                    border.width: collectionCard.hasPreview ? 0 : 1
                                    border.color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.35)

                                    gradient: modManager.theme === "Crimson" ? collectionCardGradient : null
                                    Gradient {
                                        id: collectionCardGradient
                                        GradientStop { position: 0.0; color: window.crimsonGradientTop }
                                        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                                    }

                                    property bool hasPreview: (modelData.preview || "") !== ""
                                    property var collectionMods: modelData.mods || []
                                    property int modCount: collectionMods.length
                                    property int enabledCount: {
                                        var _rev = modManager.modsActive
                                        var _trigger = modLibraryRoot.collectionStatusRefreshTrigger
                                        var count = 0
                                        for (var i = 0; i < collectionMods.length; ++i) {
                                            var details = modManager.getModDetails(collectionMods[i])
                                            if (details && details.enabled)
                                                count++
                                        }
                                        return count
                                    }

                                    MouseArea {
                                        id: collectionCardClickArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            clearFocus()
                                            collectionDetailDialog.currentCollectionId = modelData.id
                                            collectionDetailDialog.currentCollectionName = modelData.name
                                            collectionDetailDialog.loadCollectionMods()
                                            collectionDetailDialog.open()
                                        }
                                    }

                                    Item {
                                        id: collectionPreviewContainer
                                        anchors.fill: parent
                                        anchors.margins: collectionCard.border.width
                                        visible: collectionCard.hasPreview
                                        clip: true

                                        Item {
                                            anchors.fill: parent
                                            visible: true

                                            Image {
                                                anchors.fill: parent
                                                anchors.margins: -24
                                                source: collectionCard.hasPreview ? "file:///" + modelData.preview : ""
                                                fillMode: Image.PreserveAspectCrop
                                                cache: false
                                                asynchronous: true
                                                smooth: true
                                                sourceSize: Qt.size(Math.ceil(parent.width * 2), Math.ceil(parent.height * 2))

                                                layer.enabled: true
                                                layer.effect: MultiEffect {
                                                    blurEnabled: true
                                                    blur: 1.0
                                                    blurMax: 64
                                                    blurMultiplier: 1.0
                                                    saturation: 0.35
                                                    brightness: -0.45
                                                }
                                            }

                                        }

                                        AnimatedImage {
                                            id: collectionCardPreview
                                            anchors.fill: parent
                                            source: collectionCard.hasPreview ? "file:///" + modelData.preview : ""

                                            fillMode: Image.PreserveAspectFit
                                            playing: true
                                            cache: false
                                            asynchronous: true
                                            smooth: true

                                            mipmap: false
                                        }

                                        Rectangle {
                                            anchors.fill: parent
                                            gradient: Gradient {
                                                GradientStop { position: 0.0; color: "transparent" }
                                                GradientStop { position: 0.58; color: "transparent" }
                                                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.55) }
                                            }
                                        }
                                    }

                                    Item {
                                        anchors.centerIn: parent
                                        anchors.verticalCenterOffset: -20
                                        width: 64
                                        height: 64
                                        visible: !collectionCard.hasPreview

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: 0
                                            color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.15)

                                            Image {
                                                anchors.centerIn: parent
                                                width: 36
                                                height: 36
                                                source: "qrc:/UniversalModManager/Lucide Icons/folder.svg"
                                                sourceSize.width: 72
                                                sourceSize.height: 72
                                                fillMode: Image.PreserveAspectFit
                                                layer.enabled: true
                                                layer.smooth: true
                                                layer.textureSize: Qt.size(width * 2, height * 2)
                                                layer.effect: ColorOverlay {
                                                    color: window.accentColor
                                                }
                                            }
                                        }
                                    }

                                    Item {
                                        anchors.bottom: parent.bottom
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        height: 72

                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 18
                                            anchors.rightMargin: 18
                                            anchors.topMargin: 10
                                            anchors.bottomMargin: 14
                                            spacing: 3

                                            Item { Layout.fillHeight: true }

                                            Text {
                                                text: modelData.name
                                                font.pixelSize: 16
                                                font.weight: Font.Bold
                                                color: collectionCard.hasPreview ? "#FFFFFF" : window.textColor
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                            }

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 6

                                                Text {
                                                    text: collectionCard.modCount === 0
                                                        ? "Empty"
                                                        : collectionCard.modCount + (collectionCard.modCount === 1 ? " mod" : " mods")
                                                            + " · " + collectionCard.enabledCount + " enabled"
                                                    font.pixelSize: 12
                                                    font.weight: Font.Medium
                                                    color: collectionCard.hasPreview ? Qt.rgba(255, 255, 255, 0.68) : window.textSecondaryColor
                                                    Layout.fillWidth: true
                                                    elide: Text.ElideRight
                                                }
                                            }
                                        }
                                    }

                                    Item {
                                        anchors.centerIn: parent
                                        width: collectionCardClickArea.containsMouse ? collectionCard.width : 0
                                        height: collectionCard.height
                                        clip: true
                                        z: 8

                                        Rectangle {
                                            width: collectionCard.width
                                            height: collectionCard.height
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            color: "transparent"
                                            border.width: 2
                                            border.color: window.accentColor
                                        }

                                        Behavior on width {
                                            NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                                        }
                                    }

                                    Button {
                                        id: mainCollectionMenuBtn
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        anchors.topMargin: 12
                                        anchors.rightMargin: 12
                                        width: 32
                                        height: 32
                                        flat: true
                                        hoverEnabled: true
                                        z: 10

                                        property string collectionId: modelData.id
                                        property string collectionName: modelData.name
                                        property string collectionPreview: modelData.preview || ""
                                        contentItem: Image {
                                            source: "qrc:/UniversalModManager/Lucide Icons/ellipsis-vertical.svg"
                                            sourceSize.width: 20
                                            sourceSize.height: 20
                                            fillMode: Image.PreserveAspectFit
                                            layer.enabled: true
                                            layer.smooth: true
                                            layer.textureSize: Qt.size(width * 2, height * 2)
                                            layer.effect: ColorOverlay {
                                                color: collectionCard.hasPreview ? "#FFFFFF" : window.textColor
                                            }
                                        }

                                        background: Rectangle {
                                            color: parent.hovered ? (collectionCard.hasPreview ? Qt.rgba(0, 0, 0, 0.4) : Qt.rgba(0, 0, 0, 0.06)) : (collectionCard.hasPreview ? Qt.rgba(0, 0, 0, 0.25) : "transparent")
                                            radius: 8
                                            border.width: collectionCard.hasPreview ? 1 : 0
                                            border.color: Qt.rgba(1, 1, 1, 0.2)
                                            Behavior on color { ColorAnimation { duration: 150 } }
                                        }

                                        onClicked: {
                                            mainCollectionContextMenu.collectionId = mainCollectionMenuBtn.collectionId
                                            mainCollectionContextMenu.collectionName = mainCollectionMenuBtn.collectionName
                                            mainCollectionContextMenu.collectionPreview = mainCollectionMenuBtn.collectionPreview
                                            mainCollectionContextMenu.collectionEnabled = modLibraryRoot.collectionHasEnabledMods(modelData)
                                            mainCollectionContextMenu.collectionModCount = collectionCard.modCount
                                            mainCollectionContextMenu.popup(mainCollectionMenuBtn, 0, mainCollectionMenuBtn.height + 4)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Menu {
        id: mainCollectionContextMenu
        property string collectionId: ""
        property string collectionName: ""
        property string collectionPreview: ""
        property bool collectionEnabled: false
        property int collectionModCount: 0

        background: Rectangle {
            implicitWidth: 200
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.1, 0.1, 0.1, 0.95) : window.surfaceColor
            radius: 12
            border.width: 1
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.15) : window.borderColor

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.15)
                shadowBlur: 0.5
                shadowVerticalOffset: 4
                shadowHorizontalOffset: 0
            }
        }

        MenuItem {
            id: mainAddPreviewMenuItem
            height: 40
            hoverEnabled: true

            contentItem: RowLayout {
                spacing: 12
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/image.svg"
                    sourceSize.width: 16
                    sourceSize.height: 16
                    fillMode: Image.PreserveAspectFit
                    Layout.leftMargin: 4
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    }
                }

                Text {
                    text: mainCollectionContextMenu.collectionPreview !== "" ? "Change Preview" : "Add Preview"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    Layout.fillWidth: true
                }
            }

            background: Rectangle {
                color: mainAddPreviewMenuItem.hovered ? (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.1) : Qt.rgba(0, 0, 0, 0.04)) : "transparent"
                radius: 8
                anchors.margins: 4
                anchors.fill: parent
                Behavior on color { ColorAnimation { duration: 150 } }
            }

            onTriggered: {
                collectionPreviewFileDialog.collectionId = mainCollectionContextMenu.collectionId
                collectionPreviewFileDialog.open()
            }
        }

        MenuItem {
            id: mainRemovePreviewMenuItem
            height: mainCollectionContextMenu.collectionPreview !== "" ? 40 : 0
            hoverEnabled: true
            visible: mainCollectionContextMenu.collectionPreview !== ""

            contentItem: RowLayout {
                spacing: 12
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                    sourceSize.width: 16
                    sourceSize.height: 16
                    fillMode: Image.PreserveAspectFit
                    Layout.leftMargin: 4
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    }
                }

                Text {
                    text: "Remove Preview"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    Layout.fillWidth: true
                }
            }

            background: Rectangle {
                color: mainRemovePreviewMenuItem.hovered ? (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.1) : Qt.rgba(0, 0, 0, 0.04)) : "transparent"
                radius: 8
                anchors.margins: 4
                anchors.fill: parent
                Behavior on color { ColorAnimation { duration: 150 } }
            }

            onTriggered: {
                modManager.setCollectionPreview(mainCollectionContextMenu.collectionId, "")
                toastNotification.show("Preview removed", "x.svg")
            }
        }

        MenuItem {
            id: mainEnableMenuItem
            height: visible ? 40 : 0
            hoverEnabled: true
            visible: !mainCollectionContextMenu.collectionEnabled && mainCollectionContextMenu.collectionModCount > 0

            contentItem: RowLayout {
                spacing: 12
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/play.svg"
                    sourceSize.width: 16
                    sourceSize.height: 16
                    fillMode: Image.PreserveAspectFit
                    Layout.leftMargin: 4
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    }
                }

                Text {
                    text: "Enable Collection"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    Layout.fillWidth: true
                }
            }

            background: Rectangle {
                color: mainEnableMenuItem.hovered ? (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.1) : Qt.rgba(0, 0, 0, 0.04)) : "transparent"
                radius: 8
                anchors.margins: 4
                anchors.fill: parent
                Behavior on color { ColorAnimation { duration: 150 } }
            }

            onTriggered: {
                modManager.toggleCollectionStatus(mainCollectionContextMenu.collectionId, true)
                modLibraryRoot.collectionStatusRefreshTrigger++
                toastNotification.show("Collection enabled", "play.svg")
            }
        }

        MenuItem {
            id: mainDisableMenuItem
            height: visible ? 40 : 0
            hoverEnabled: true
            visible: mainCollectionContextMenu.collectionEnabled && mainCollectionContextMenu.collectionModCount > 0

            contentItem: RowLayout {
                spacing: 12
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/pause.svg"
                    sourceSize.width: 16
                    sourceSize.height: 16
                    fillMode: Image.PreserveAspectFit
                    Layout.leftMargin: 4
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    }
                }

                Text {
                    text: "Disable Collection"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    Layout.fillWidth: true
                }
            }

            background: Rectangle {
                color: mainDisableMenuItem.hovered ? (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.1) : Qt.rgba(0, 0, 0, 0.04)) : "transparent"
                radius: 8
                anchors.margins: 4
                anchors.fill: parent
                Behavior on color { ColorAnimation { duration: 150 } }
            }

            onTriggered: {
                modManager.toggleCollectionStatus(mainCollectionContextMenu.collectionId, false)
                modLibraryRoot.collectionStatusRefreshTrigger++
                toastNotification.show("Collection disabled", "pause.svg")
            }
        }

        MenuItem {
            id: mainRenameMenuItem
            height: 40
            hoverEnabled: true

            contentItem: RowLayout {
                spacing: 12
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/pencil.svg"
                    sourceSize.width: 16
                    sourceSize.height: 16
                    fillMode: Image.PreserveAspectFit
                    Layout.leftMargin: 4
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    }
                }

                Text {
                    text: "Rename"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    Layout.fillWidth: true
                }
            }

            background: Rectangle {
                color: mainRenameMenuItem.hovered ? (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.1) : Qt.rgba(0, 0, 0, 0.04)) : "transparent"
                radius: 8
                anchors.margins: 4
                anchors.fill: parent
                Behavior on color { ColorAnimation { duration: 150 } }
            }

            onTriggered: {
                renameCollectionDialog.collectionId = mainCollectionContextMenu.collectionId
                renameCollectionDialog.currentName = mainCollectionContextMenu.collectionName
                renameCollectionDialog.open()
            }
        }

        MenuItem {
            id: mainDeleteMenuItem
            height: 40
            hoverEnabled: true

            contentItem: RowLayout {
                spacing: 12
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/trash-2.svg"
                    sourceSize.width: 16
                    sourceSize.height: 16
                    fillMode: Image.PreserveAspectFit
                    Layout.leftMargin: 4
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: "#EF4444"
                    }
                }

                Text {
                    text: "Delete"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: "#EF4444"
                    Layout.fillWidth: true
                }
            }

            background: Rectangle {
                color: mainDeleteMenuItem.hovered ? Qt.rgba(0.93, 0.26, 0.26, 0.1) : "transparent"
                radius: 8
                anchors.margins: 4
                anchors.fill: parent
                Behavior on color { ColorAnimation { duration: 150 } }
            }

            onTriggered: {
                modManager.deleteCollection(mainCollectionContextMenu.collectionId)
                toastNotification.show("Collection deleted", "trash-2.svg")
            }
        }
    }

    ModLibraryPageBackupDialogs {
        id: backupDialogs
    }

    readonly property alias backupProgressBar: backupDialogs.backupProgressBar
    readonly property alias backupProgressDialog: backupDialogs.backupProgressDialog
    readonly property alias backupProgressText: backupDialogs.backupProgressText
    readonly property alias backupTypeDialog: backupDialogs.backupTypeDialog
    readonly property alias deleteFinishTimer: backupDialogs.deleteFinishTimer
    readonly property alias deleteProgressDialog: backupDialogs.deleteProgressDialog
    readonly property alias installModDialog: backupDialogs.installModDialog
    readonly property alias resultDialog: backupDialogs.resultDialog

    ListModel {
        id: filteredModList
    }

    function filterModList() {

        filterDebounceTimer.restart()
    }

    function filterModListInternal() {
        filteredModList.clear()

        if (!modManager.modList || modManager.modList.length === 0) {
            return
        }

        var searchText = modSearchField.text.toLowerCase()
        var tempList = []
        var bundlesOnly = viewMode === "bundles"

        for (var i = 0; i < modManager.modList.length; i++) {
            var mod = modManager.modList[i]
            if (!mod || !mod.name) continue

            var modName = mod.name.toLowerCase()
            var isEnabled = mod.status === "Active"
            var modType = mod.type || "folder"
            var imageKey = (mod.type === "loose" && mod.uniqueId) ? mod.uniqueId : mod.name
            var bundleId = mod.bundleId || ""

            if (openedBundleId !== "") {
                if (bundleId !== openedBundleId)
                    continue
            } else if (bundlesOnly || (viewMode !== "cards" && bundleId !== "")) {

                continue
            }

            var matchesName = modName.includes(searchText)
            var matchesTags = false
            var matchesId = false

            if (searchText !== "") {

                var modDetails = modManager.getModDetails(mod.name)
                var modId = modDetails.modId || ""
                if (modId.toLowerCase().includes(searchText)) {
                    matchesId = true
                }

                var tags = modManager.getModTags(mod.name)
                for (var j = 0; j < tags.length; j++) {
                    if (tags[j].toLowerCase().includes(searchText)) {
                        matchesTags = true
                        break
                    }
                }
            }

            if (searchText !== "" && !matchesName && !matchesTags && !matchesId) {
                continue
            }

            var passesFilters = true
            if (activeFilters.length > 0) {
                var hasStatusFilter = activeFilters.indexOf("active") !== -1 || activeFilters.indexOf("disabled") !== -1
                var hasTypeFilter = activeFilters.indexOf("folder") !== -1 || activeFilters.indexOf("loose") !== -1
                var hasFavoriteFilter = activeFilters.indexOf("favorite") !== -1

                if (hasStatusFilter) {
                    var matchesStatus = (activeFilters.indexOf("active") !== -1 && isEnabled) ||
                                       (activeFilters.indexOf("disabled") !== -1 && !isEnabled)
                    if (!matchesStatus) passesFilters = false
                }

                if (hasTypeFilter) {
                    var matchesType = (activeFilters.indexOf("folder") !== -1 && modType === "folder") ||
                                     (activeFilters.indexOf("loose") !== -1 && modType === "loose")
                    if (!matchesType) passesFilters = false
                }

                if (hasFavoriteFilter) {
                    if (!modManager.isModFavorite(imageKey)) {
                        passesFilters = false
                    }
                }
            }

            if (passesFilters) {

                var modDetails = modManager.getModDetails(mod.name)
                tempList.push({
                    name: mod.name,
                    status: mod.status,
                    fileCount: mod.fileCount,
                    type: modType,
                    extensions: mod.extensions || "",
                    enabled: isEnabled,
                    size: modDetails.size || "",
                    date: modDetails.installDate || "",
                    uniqueId: mod.uniqueId || "",
                    bundleId: bundleId
                })
            }
        }

        tempList.sort(function(a, b) {
            var aVal, bVal

            if (sortBy === "name") {
                aVal = a.name.toLowerCase()
                bVal = b.name.toLowerCase()
                return sortAscending ? aVal.localeCompare(bVal) : bVal.localeCompare(aVal)
            } else if (sortBy === "date") {
                aVal = a.date
                bVal = b.date
                if (sortAscending) return aVal < bVal ? -1 : (aVal > bVal ? 1 : 0)
                else return aVal > bVal ? -1 : (aVal < bVal ? 1 : 0)
            } else if (sortBy === "size") {

                aVal = parseSizeString(a.size)
                bVal = parseSizeString(b.size)
                return sortAscending ? bVal - aVal : aVal - bVal
            } else if (sortBy === "status") {
                aVal = a.enabled ? 1 : 0
                bVal = b.enabled ? 1 : 0
                return sortAscending ? bVal - aVal : aVal - bVal
            }
            return 0
        })

        for (var k = 0; k < tempList.length; k++) {
            filteredModList.append(tempList[k])
        }
    }

    function parseSizeString(sizeStr) {
        if (!sizeStr || sizeStr === "N/A") return 0
        var parts = sizeStr.split(" ")
        if (parts.length !== 2) return 0
        var num = parseFloat(parts[0])
        var unit = parts[1].toUpperCase()
        if (unit === "B") return num
        if (unit === "KB") return num * 1024
        if (unit === "MB") return num * 1024 * 1024
        if (unit === "GB") return num * 1024 * 1024 * 1024
        return num
    }

    Connections {
        target: modManager

        function onModListChanged() {
            filterModList()
            refreshVisibleBundles()
        }

        function onModBundlesChanged() {
            bundleStatusRevision++
            if (openedBundleId !== "" && !bundleById(openedBundleId).id)
                closeBundle()
            refreshVisibleBundles()
        }

        function onModStatusChanged(modName, enabled) {

            bundleStatusRevision++
            var statusText = enabled ? "Active" : "Disabled"
            for (var i = 0; i < filteredModList.count; ++i) {
                var item = filteredModList.get(i)
                if (item.name === modName || item.uniqueId === modName) {
                    filteredModList.setProperty(i, "status", statusText)
                    filteredModList.setProperty(i, "enabled", enabled)
                }
            }

            if (activeFilters.indexOf("active") !== -1
                    || activeFilters.indexOf("disabled") !== -1
                    || sortBy === "status")
                filterModList()

            if (selectedDisplayName === modName)
                selectedModDetails = modManager.getModDetails(modName)
        }

        function onModRemoved(modName) {

            bundleStatusRevision++
            for (var i = filteredModList.count - 1; i >= 0; --i) {
                var item = filteredModList.get(i)
                if (item.name === modName || item.uniqueId === modName)
                    filteredModList.remove(i)
            }

            if (openedBundleId !== "" && !bundleById(openedBundleId).id)
                closeBundle()

            var selectedIndex = selectedMods.indexOf(modName)
            if (selectedIndex !== -1) {
                var remaining = selectedMods.slice()
                remaining.splice(selectedIndex, 1)
                selectedMods = remaining
            }

            if (selectedDisplayName === modName) {
                selectedDisplayName = ""
                selectedModName = ""
                selectedModDetails = ({})
                selectedModDescription = ""
                modDetailsPanel.close()
            }
        }

        function onModTagsChanged(modName) {

            tagRefreshTrigger++
            refreshVisibleBundles()
            currentTags = modManager.getModTags(selectedModName)

            filterModList()
        }

        function onModFavoriteChanged(modKey, favorite) {
            favoriteRefreshTrigger++
            refreshVisibleBundles()
            filterModList()
        }

        function onModRenamed(oldName, newName) {
            filterModList()
            refreshVisibleBundles()

            if (selectedDisplayName === oldName) {
                selectedDisplayName = newName
                selectedModName = newName
                selectedModDetails = modManager.getModDetails(newName)
                selectedModDescription = modManager.getModDescription(newName)
                loadSelectedModPreview()
            }
        }
    }

    Connections {
        target: modManager

        function onInstallProgress(message) {
            installProgressText.text = message
        }

        function onInstallComplete(success, message) {
            installProgressDialog.close()

            if (success) {
                var installedCount = modManager.lastInstalledModCount
                var toastMessage = installedCount === 1
                    ? "Mod installed successfully"
                    : (installedCount > 1
                        ? installedCount + " mods installed successfully"
                        : "No changes were installed")
                toastNotification.show(toastMessage, "package.svg")
            } else {
                toastNotification.show("Installation failed", "circle-x.svg")
            }

            resultDialog.isSuccess = success
            resultDialog.resultMessage = message
            resultDialog.open()
        }

        function onBackupProgress(current, total, modName) {
            backupProgressText.text = "Backing up: " + modName
            backupProgressBar.value = (current / total) * 100
        }

        function onBackupComplete(success, successCount, total) {
            backupProgressDialog.close()

            if (success) {
                var message = successCount === 1 ? "Backed up 1 mod" : "Backed up " + successCount + " of " + total + " mods"
                toastNotification.show(message, "save.svg")
            } else {
                toastNotification.show("Backup failed", "circle-x.svg")
            }

            resultDialog.isSuccess = success
            resultDialog.resultMessage = success ?
                (successCount === 1 ? "Successfully backed up 1 mod" : "Successfully backed up " + successCount + " of " + total + " mods") :
                "Backup failed"
            resultDialog.open()
        }

        function onDeleteProgress(current, total, modName) {
            deleteProgressDialog.isFinishing = false
            deleteProgressDialog.currentCount = current
            deleteProgressDialog.totalCount = total
            deleteProgressDialog.currentModName = modName
        }

        function onDeleteComplete(success, deletedCount, total) {
            deleteProgressDialog.currentCount = deletedCount
            deleteProgressDialog.totalCount = total
            deleteProgressDialog.currentModName = ""
            deleteProgressDialog.isFinishing = true
            deleteProgressDialog.finishMessage = success ? "Finalizing deletion..." : "Wrapping up partial deletion..."
            deleteProgressDialog.pendingSuccess = success
            deleteProgressDialog.pendingDeletedCount = deletedCount
            deleteProgressDialog.pendingTotalCount = total

            var elapsed = Date.now() - deleteProgressDialog.openedAtMs
            deleteFinishTimer.interval = Math.max(
                deleteProgressDialog.finishHoldMs,
                deleteProgressDialog.minimumVisibleMs - elapsed
            )
            deleteFinishTimer.restart()
        }

        function onModStructureExported(success, filePath) {
            if (success) {
                toastNotification.show("Mod structure exported", "check.svg")

                Qt.openUrlExternally("file:///" + filePath)
            } else {

                var missing = []
                if (!modManager.gamePaksPath || modManager.gamePaksPath === "") {
                    missing.push("Game Paks folder")
                }
                if (!modManager.usmapFilePath || modManager.usmapFilePath === "") {
                    missing.push("USMAP file")
                }

                if (missing.length > 0) {
                    toastNotification.show("Export failed – set " + missing.join(" and ") + " in Settings > Model Viewer.", "circle-x.svg")
                } else {
                    toastNotification.show("Export failed – check PakViewer path and game configuration in Settings.", "circle-x.svg")
                }
            }
        }
    }

    ModLibraryPageModDialogs {
        id: modDialogs
    }

    readonly property alias deleteConfirmDialog: modDialogs.deleteConfirmDialog
    readonly property alias imageFileDialog: modDialogs.imageFileDialog
    readonly property alias modDetailsPanel: modDialogs.modDetailsPanel
    readonly property alias panelModGif: modDialogs.panelModGif
    readonly property alias panelModImage: modDialogs.panelModImage

    ModLibraryPageNotifications {
        id: libraryNotifications
    }

    readonly property alias toastNotification: libraryNotifications.toastNotification

    ModLibraryUpdateResultDialog {
        id: nexusUpdateResultDialog
    }

    Connections {
        target: modManager
        function onIsCheckingNexusUpdatesChanged() {
            if (modManager.isCheckingNexusUpdates || !modLibraryRoot.nexusUpdateCheckPending)
                return
            modLibraryRoot.nexusUpdateCheckPending = false
            nexusUpdateResultDialog.show(modManager.nexusUpdatesAvailable)
        }
    }

    SwitchModsFolder {
        id: quickModsFolderSwitchDialog

        quickModsFolderChoices:          modLibraryRoot.quickModsFolderChoices
        quickModsFolderSelectedIndex:    modLibraryRoot.quickModsFolderSelectedIndex
        quickCurrentModsFolderName:      modLibraryRoot.quickCurrentModsFolderName
        quickCurrentModsFolderPath:      modLibraryRoot.quickCurrentModsFolderPath
        quickCurrentModsFolderCount:     modLibraryRoot.quickCurrentModsFolderCount
        quickTargetModsFolderName:       modLibraryRoot.quickTargetModsFolderName
        quickTargetModsFolderPath:       modLibraryRoot.quickTargetModsFolderPath
        quickTargetModsFolderCount:      modLibraryRoot.quickTargetModsFolderCount
        quickSwitchDisableAllMods:       modLibraryRoot.quickSwitchDisableAllMods
        quickSwitchDisableAllCollections: modLibraryRoot.quickSwitchDisableAllCollections
        quickSwitchShowAdvanced:         modLibraryRoot.quickSwitchShowAdvanced
        quickSwitchModsToDisable:        modLibraryRoot.quickSwitchModsToDisable
        quickSwitchCollectionsToDisable: modLibraryRoot.quickSwitchCollectionsToDisable
        quickSwitchAdvancedSearch:       modLibraryRoot.quickSwitchAdvancedSearch

        function quickSwitchFilteredMods()          { return modLibraryRoot.quickSwitchFilteredMods() }
        function quickSwitchCurrentPresetCollections() { return modLibraryRoot.quickSwitchCurrentPresetCollections() }

        onFolderTargetRequested:           function(index)   { modLibraryRoot.setQuickModsFolderTarget(index) }
        onDisableAllModsToggled:           function(value)   { modLibraryRoot.quickSwitchDisableAllMods = value }
        onDisableAllCollectionsToggled:    function(value)   { modLibraryRoot.quickSwitchDisableAllCollections = value }
        onAdvancedToggled:                 function(value)   { modLibraryRoot.quickSwitchShowAdvanced = value }
        onModToggled:                      function(key)     { modLibraryRoot.quickSwitchToggleMod(key) }
        onCollectionToggled:               function(id)      { modLibraryRoot.quickSwitchToggleCollection(id) }
        onSelectAllModsRequested:                            { modLibraryRoot.quickSwitchSelectAllMods() }
        onDeselectAllModsRequested:                          { modLibraryRoot.quickSwitchDeselectAllMods() }
        onSelectAllCollectionsRequested:                     { modLibraryRoot.quickSwitchSelectAllCollections() }
        onDeselectAllCollectionsRequested:                   { modLibraryRoot.quickSwitchDeselectAllCollections() }
        onAdvancedSearchChanged:           function(query)   { modLibraryRoot.quickSwitchAdvancedSearch = query }
        onSwitchRequested: function(targetPath, disableAllMods, disableAllCollections, modsToDisable, collectionsToDisable) {
            var ok = modManager.switchCurrentPresetModsFolder(
                targetPath,
                disableAllMods,
                disableAllCollections,
                modsToDisable,
                collectionsToDisable
            )
            if (ok) {
                quickModsFolderSwitchDialog.close()
                toastNotification.show("Switched to " + modLibraryRoot.quickTargetModsFolderName, "folder-cog.svg")
            } else {
                toastNotification.show("Failed to switch mods folder", "circle-alert.svg")
            }
        }
    }

    CreateModsFolderDialog {
        id: createModsFolderDialog

        onFolderCreated: function(path, name, useForProfile) {
            if (useForProfile) {
                var ok = modManager.switchCurrentPresetModsFolder(path, false, false, [], [])
                toastNotification.show(ok ? "Created " + name + " and switched to it" : "Created " + name,
                                       ok ? "folder-check.svg" : "circle-alert.svg")
            } else {
                toastNotification.show("Created " + name, "folder-check.svg")
            }
        }

        onExistingFolderUsed: function(path, name, useForProfile) {
            if (!useForProfile) {
                toastNotification.show(name + " already exists", "folder-check.svg")
                return
            }
            var ok = modManager.switchCurrentPresetModsFolder(path, false, false, [], [])
            toastNotification.show(ok ? "Switched to " + name : "Failed to switch to " + name,
                                   ok ? "folder-cog.svg" : "circle-alert.svg")
        }

        onCreateFailed: function(message) {
            toastNotification.show(message, "circle-alert.svg")
        }
    }

    ModLibraryPageCollectionDialogs {
        id: collectionDialogs
    }

    readonly property alias collectionDetailDialog: collectionDialogs.collectionDetailDialog
    readonly property alias collectionPreviewFileDialog: collectionDialogs.collectionPreviewFileDialog
    readonly property alias collectionsDialog: collectionDialogs.collectionsDialog
    readonly property alias renameCollectionDialog: collectionDialogs.renameCollectionDialog

}
