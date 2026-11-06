import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects
import Qt.labs.platform as Platform

Item {
    id: conflictDetectorTab

    component RuntimeContextMenuItem : MenuItem {
        id: menuItem
        property string iconName: ""
        property bool isDanger: false
        property string shortcutText: ""

        implicitWidth: 180
        implicitHeight: 32
        width: parent ? parent.width : implicitWidth
        height: 32

        contentItem: RowLayout {
            spacing: 12

            Image {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                source: menuItem.iconName !== "" ? "qrc:/UniversalModManager/Lucide Icons/" + menuItem.iconName : ""
                sourceSize: Qt.size(16, 16)
                visible: menuItem.iconName !== ""
                opacity: menuItem.enabled ? 1.0 : 0.5

                layer.enabled: true
                layer.effect: ColorOverlay {
                    color: menuItem.isDanger ? "#EF4444" : ((modManager.theme === "Glassmorphic" || modManager.theme === "Wavey") ? "#FFFFFF" : window.textColor)
                }
            }

            Text {
                Layout.fillWidth: true
                text: menuItem.text
                font.pixelSize: 13
                font.weight: Font.Medium
                color: menuItem.isDanger ? "#EF4444" : ((modManager.theme === "Glassmorphic" || modManager.theme === "Wavey") ? "#FFFFFF" : window.textColor)
                opacity: menuItem.enabled ? 1.0 : 0.5
            }

            Text {
                text: menuItem.shortcutText
                font.pixelSize: 11
                color: (modManager.theme === "Glassmorphic" || modManager.theme === "Wavey") ? Qt.rgba(1, 1, 1, 0.5) : window.textSecondaryColor
                visible: menuItem.shortcutText !== ""
                opacity: menuItem.enabled ? 1.0 : 0.5
            }
        }

        background: Rectangle {
            color: menuItem.highlighted
                ? (modManager.theme === "Glassmorphic"
                    ? Qt.rgba(1, 1, 1, 0.10)
                    : (modManager.theme === "Crimson"
                        ? Qt.rgba(0.65, 0.40, 0.95, 0.20)
                        : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)))
                : "transparent"
            radius: 8

            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    property bool isScanning: modManager.isConflictScanning
    property var conflictGroups: modManager.conflictGroups
    property string lastScanTime: modManager.lastConflictScanTime
    property string scanProgress: modManager.conflictScanProgress
    property var scanErrors: modManager.conflictScanErrors

    property bool includeDisabledMods: false
    property int selectedModIndex: -1
    property string selectedFileType: "all"
    property string fileSearchText: ""
    property string modSearchText: ""
    property bool rescanNeeded: false

    property var locallyDisabledMods: ({})
    property var locallyEnabledMods: ({})

    function modKey(name) {
        return ((name || "") + "").toLowerCase()
    }

    function isLocallyDisabled(name) {
        return !!locallyDisabledMods[modKey(name)]
    }

    function isLocallyEnabled(name) {
        return !!locallyEnabledMods[modKey(name)]
    }

    property var modStatusMap: {
        var map = {}
        var list = modManager.modList || []
        for (var i = 0; i < list.length; i++) {
            var mod = list[i]
            var isDisabled = (mod.status || "").toLowerCase() === "disabled"
            if (mod.name) {
                map[modKey(mod.name)] = isDisabled
            }
            if (mod.uniqueId) {
                map[modKey(mod.uniqueId)] = isDisabled
            }
        }
        return map
    }

    property var scannedModStatus: {
        var map = {}
        if (conflictGroups) {
            for (var i = 0; i < conflictGroups.length; i++) {
                var groupMods = conflictGroups[i].mods || []
                for (var m = 0; m < groupMods.length; m++) {
                    map[modKey(groupMods[m].name)] = groupMods[m].isDisabled
                }
            }
        }
        return map
    }

    function isDisabledInManager(name) {
        var key = modKey(name)
        return modStatusMap[key] === true
    }

    function isDisabledForDisplay(name) {
        var key = modKey(name)
        if (isLocallyEnabled(name)) return false
        return isLocallyDisabled(name) || (includeDisabledMods && (isDisabledInManager(name) || scannedModStatus[key])) || (!includeDisabledMods && isDisabledInManager(name))
    }

    function markLocallyDisabled(name) {
        var key = modKey(name)
        var copy = {}
        for (var k in locallyDisabledMods) {
            copy[k] = locallyDisabledMods[k]
        }
        copy[key] = true
        locallyDisabledMods = copy
        clearLocallyEnabled(name)
    }

    function markLocallyEnabled(name) {
        var key = modKey(name)
        var copy = {}
        for (var k in locallyEnabledMods) {
            copy[k] = locallyEnabledMods[k]
        }
        copy[key] = true
        locallyEnabledMods = copy
        clearLocallyDisabled(name)
    }

    function clearLocallyDisabled(name) {
        var key = modKey(name)
        if (!locallyDisabledMods[key]) return
        var copy = {}
        for (var k in locallyDisabledMods) {
            if (k !== key) copy[k] = locallyDisabledMods[k]
        }
        locallyDisabledMods = copy
    }

    function clearLocallyEnabled(name) {
        var key = modKey(name)
        if (!locallyEnabledMods[key]) return
        var copy = {}
        for (var k in locallyEnabledMods) {
            if (k !== key) copy[k] = locallyEnabledMods[k]
        }
        locallyEnabledMods = copy
    }

    function formatScanTime(isoTime) {
        if (!isoTime || isoTime === "") return ""
        var date = new Date(isoTime)
        if (isNaN(date.getTime())) return isoTime
        var now = new Date()
        var diffMs = now - date
        var diffMins = Math.floor(diffMs / 60000)
        if (diffMins < 1) return "just now"
        if (diffMins < 60) return diffMins + " min ago"
        return Math.floor(diffMins / 60) + " hr ago"
    }

    function mapFileTypeDisplay(fileType, filePath) {
        var rawType = (fileType || "").toString().trim()
        var path = (filePath || "").toString().toLowerCase()

        var norm = rawType.toLowerCase().replace(/ /g, "").replace(/_/g, "")
        if (norm === "skeletalmesh") return "Skeletal Mesh"
        if (norm === "staticmesh") return "Static Mesh"
        if (norm === "material") return "Material"
        if (norm === "materialinstanceconstant" || norm === "materialinstance") return "Material Instance"
        if (norm === "texture2d" || norm === "texture") return "Texture 2D"
        if (norm === "physicsasset") return "Physics Asset"
        if (norm === "animsequence" || norm === "animblueprint" || norm === "animation") return "Animation"
        if (norm === "blueprintgeneratedclass" || norm === "blueprint") return "Blueprint"
        if (norm === "dataasset") return "Data Asset"
        if (norm === "soundwave" || norm === "soundcue" || norm === "audio") return "Audio"
        if (norm === "datatable") return "Data Table"
        if (norm === "niagarasystem" || norm === "niagaravfx") return "Niagara VFX"

        if (path) {
            var fileName = getFileName(path).toLowerCase()

            if (fileName.indexOf("_ao.") !== -1 || fileName.indexOf("_basecolor.") !== -1 ||
                fileName.indexOf("_metallic.") !== -1 || fileName.indexOf("_normal.") !== -1 ||
                fileName.indexOf("_roughness.") !== -1 || fileName.indexOf("_id.") !== -1 ||
                fileName.indexOf("_specular.") !== -1 || fileName.indexOf("_mask.") !== -1 ||
                fileName.indexOf("_d.") !== -1 || fileName.indexOf("_n.") !== -1 ||
                fileName.indexOf("_m.") !== -1 || fileName.indexOf("_r.") !== -1 ||
                fileName.indexOf("_t.") !== -1 || path.indexOf("/textures/") !== -1 ||
                fileName.indexOf("_texture") !== -1 || fileName.indexOf("t_") === 0) {
                return "Texture 2D"
            }

            if (fileName.indexOf("mi_") === 0 || fileName.indexOf("_inst.") !== -1 ||
                fileName.indexOf("m_") === 0 || path.indexOf("/body_mat/") !== -1 ||
                path.indexOf("/materials/") !== -1 || path.indexOf("material") !== -1) {
                return "Material Instance"
            }

            if (fileName.indexOf("sk_") === 0 || fileName.indexOf("skm_") === 0 ||
                path.indexOf("skeletal") !== -1) {
                return "Skeletal Mesh"
            }

            if (fileName.indexOf("sm_") === 0 || path.indexOf("static") !== -1 ||
                path.indexOf("/mesh/") !== -1) {
                return "Static Mesh"
            }

            if (fileName.indexOf("phys_") === 0 || fileName.indexOf("_physicsasset") !== -1 ||
                path.indexOf("physics") !== -1) {
                return "Physics Asset"
            }

            if (fileName.indexOf("bp_") === 0 || fileName.indexOf("wbp_") === 0 ||
                fileName.indexOf("abp_") === 0 || fileName.indexOf("ibc_bp_") === 0 ||
                fileName.indexOf("wawa_bp_") === 0 || path.indexOf("/abp/") !== -1) {
                return "Blueprint"
            }

            if (fileName.indexOf("da_") === 0 || fileName.indexOf("dt_") === 0 ||
                path.indexOf("data") !== -1) {
                return "Data Asset"
            }

            if (fileName.indexOf("ns_") === 0 || fileName.indexOf("ps_") === 0 ||
                path.indexOf("niagara") !== -1 || path.indexOf("particle") !== -1) {
                return "Niagara VFX"
            }
        }

        if (rawType && rawType !== "Asset" && rawType !== "Other" && rawType !== "Unknown") {
            return rawType
        }

        return "Unreal Asset"
    }

    property var scannedMods: {
        var mods = {}
        if (conflictGroups) {
            for (var i = 0; i < conflictGroups.length; i++) {
                var group = conflictGroups[i]
                var conflicts = group.conflicts || []
                for (var j = 0; j < conflicts.length; j++) {
                    var conflict = conflicts[j]
                    var conflictingMods = conflict.conflictingMods || []
                    var category = mapFileTypeDisplay(conflict.fileType, conflict.filePath)

                    var uniqueMods = []
                    var uniqueDisabled = {}
                    for (var u = 0; u < conflictingMods.length; u++) {
                        var candidateName = typeof conflictingMods[u] === "string"
                            ? conflictingMods[u]
                            : (conflictingMods[u].name || "Unknown")
                        if (uniqueMods.indexOf(candidateName) === -1) {
                            uniqueMods.push(candidateName)
                            uniqueDisabled[candidateName] = typeof conflictingMods[u] === "object"
                                ? conflictingMods[u].isDisabled
                                : false
                        }
                    }
                    if (uniqueMods.length < 2)
                        continue

                    for (var k = 0; k < uniqueMods.length; k++) {
                        var modName = uniqueMods[k]
                        if (!mods[modName]) {
                            mods[modName] = {
                                name: modName,
                                conflictCount: 0,
                                files: [],
                                conflictsWith: {},
                                isDisabled: uniqueDisabled[modName]
                            }
                        }
                        mods[modName].conflictCount++
                        mods[modName].files.push({
                            path: conflict.filePath,
                            type: category,
                            rawType: conflict.fileType,
                            severity: conflict.severity
                        })

                        for (var m = 0; m < uniqueMods.length; m++) {
                            if (m !== k) {
                                mods[modName].conflictsWith[uniqueMods[m]] = true
                            }
                        }
                    }
                }
            }
        }
        return Object.values(mods)
    }

    property var visualGroups: {
        var groups = []
        var processed = {}

        for (var i = 0; i < scannedMods.length; i++) {
            var mod = scannedMods[i]
            if (processed[mod.name]) continue

            var group = [mod.name]
            processed[mod.name] = true

            var conflictsWithArray = Object.keys(mod.conflictsWith || {})
            for (var j = 0; j < conflictsWithArray.length; j++) {
                var otherModName = conflictsWithArray[j]
                if (!processed[otherModName]) {
                    group.push(otherModName)
                    processed[otherModName] = true
                }
            }

            groups.push(group)
        }

        return groups
    }

    property var flattenedModList: {
        var result = []

        for (var i = 0; i < visualGroups.length; i++) {
            var group = visualGroups[i]
            for (var j = 0; j < group.length; j++) {
                var modName = group[j]

                var modInfo = null
                for (var k = 0; k < scannedMods.length; k++) {
                    if (scannedMods[k].name === modName) {
                        modInfo = scannedMods[k]
                        break
                    }
                }

                if (modInfo) {
                    result.push({
                        name: modInfo.name,
                        conflictCount: modInfo.conflictCount,
                        files: modInfo.files,
                        isDisabled: modInfo.isDisabled,
                        groupIndex: i,
                        groupSize: group.length,
                        positionInGroup: j,
                        isFirstInGroup: j === 0,
                        isLastInGroup: j === group.length - 1
                    })
                }
            }
        }

        return result
    }

    property var filteredModList: {
        var list = flattenedModList || []
        var search = modSearchText.trim().toLowerCase()
        if (!search) return list
        return list.filter(function(m) {
            return m.name.toLowerCase().indexOf(search) !== -1
        })
    }

    property var selectedModFiles: {
        if (selectedModIndex < 0 || selectedModIndex >= flattenedModList.length) return []
        var files = flattenedModList[selectedModIndex].files || []
        var search = fileSearchText.trim().toLowerCase()
        return files.filter(function(f) {
            if (selectedFileType !== "all" && f.type !== selectedFileType) {
                return false
            }
            if (search) {
                var name = getFileName(f.path).toLowerCase()
                var path = (f.path || "").toLowerCase()
                if (name.indexOf(search) === -1 && path.indexOf(search) === -1) return false
            }
            return true
        })
    }

    property var fileTypes: {
        var types = {"all": 0}
        if (selectedModIndex >= 0 && selectedModIndex < flattenedModList.length) {
            var files = flattenedModList[selectedModIndex].files || []
            for (var i = 0; i < files.length; i++) {
                var t = files[i].type || "Unreal Asset"
                types[t] = (types[t] || 0) + 1
            }
        }
        return types
    }

    property var sortedFileTypeKeys: {
        var keys = Object.keys(fileTypes)
        var result = ["all"]
        var otherKeys = keys.filter(function(k) { return k !== "all" })
        otherKeys.sort(function(a, b) { return fileTypes[b] - fileTypes[a] })
        return result.concat(otherKeys)
    }

    readonly property color themeAccent: {
        if (modManager.theme === "Wavey") return themeConfig.wavey.accentColor
        if (modManager.theme === "Crimson") return themeConfig.crimson.accentColor
        if (modManager.theme === "Glassmorphic") return modManager.accentColor
        return themeConfig.cozy.accentColor
    }

    ThemeConfig { id: themeConfig }

    readonly property var conflictColors: {
        if (modManager.theme === "Crimson") return themeConfig.crimson
        if (modManager.theme === "Wavey") return themeConfig.wavey
        return themeConfig.cozy
    }

    readonly property color tooltipBackgroundColor: modManager.theme === "Glassmorphic"
        ? Qt.rgba(0.12, 0.12, 0.12, 0.98)
        : window.surfaceColor

    onScannedModsChanged: {
        if (scannedMods.length > 0 && (selectedModIndex < 0 || selectedModIndex >= flattenedModList.length)) {
            selectedModIndex = 0
            selectedFileType = "all"
        }
    }

    Gradient {
        id: conflictCozyGradient
        GradientStop { position: 0.0; color: window.cozyDialogGradientTop }
        GradientStop { position: 1.0; color: window.cozyDialogGradientBottom }
    }

    Gradient {
        id: conflictCrimsonGradient
        GradientStop { position: 0.0; color: window.crimsonGradientTop }
        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
    }

    Gradient {
        id: conflictWaveyGradient
        GradientStop { position: 0.0; color: window.waveyDialogGradientTop }
        GradientStop { position: 1.0; color: window.waveyDialogGradientBottom }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            radius: 12
            color: (modManager.theme === "Crimson" || modManager.theme === "Cozy" || modManager.theme === "Wavey")
                ? "transparent"
                : (modManager.theme === "Glassmorphic"
                    ? window.surfaceColor
                    : window.surfaceColor)
            border.width: 1
            border.color: window.borderColor

            gradient: modManager.theme === "Cozy" ? conflictCozyGradient
                : modManager.theme === "Crimson" ? conflictCrimsonGradient
                : modManager.theme === "Wavey" ? conflictWaveyGradient
                : null

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 14

                Rectangle {
                    width: 36
                    height: 36
                    radius: 8
                    color: Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.12)
                    Layout.alignment: Qt.AlignVCenter

                    Image {
                        id: headerIcon
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        source: "qrc:/UniversalModManager/Lucide Icons/scan-search.svg"
                        sourceSize: Qt.size(20, 20)
                        visible: false
                    }

                    ColorOverlay {
                        anchors.fill: headerIcon
                        source: headerIcon
                        color: themeAccent
                    }
                }

                ColumnLayout {
                    spacing: 2
                    Layout.alignment: Qt.AlignVCenter

                    RowLayout {
                        spacing: 8
                        Text {
                            text: "Mod Conflict Detector"
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            color: window.textColor
                        }

                        Rectangle {
                            visible: lastScanTime !== ""
                            width: scanBadgeText.implicitWidth + 10
                            height: 18
                            radius: 9
                            color: Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.12)
                            Text {
                                id: scanBadgeText
                                anchors.centerIn: parent
                                text: formatScanTime(lastScanTime)
                                font.pixelSize: 10
                                font.weight: Font.Medium
                                color: themeAccent
                            }
                        }
                    }

                    Text {
                        text: scannedMods.length > 0
                            ? scannedMods.length + " mods with collisions detected"
                            : "Scan installed mods to detect asset file overrides"
                        font.pixelSize: 11
                        color: window.textSecondaryColor
                    }
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    spacing: 10
                    Layout.alignment: Qt.AlignVCenter

                    Rectangle {
                        height: 32
                        width: incDisabledRow.implicitWidth + 16
                        radius: 8
                        color: Qt.rgba(0, 0, 0, 0.04)
                        border.width: 1
                        border.color: window.borderColor

                        RowLayout {
                            id: incDisabledRow
                            anchors.centerIn: parent
                            spacing: 8

                            Rectangle {
                                width: 16
                                height: 16
                                radius: 4
                                color: includeDisabledMods ? themeAccent : "transparent"
                                border.width: includeDisabledMods ? 0 : 1.5
                                border.color: window.borderColor

                                Image {
                                    anchors.centerIn: parent
                                    width: 10; height: 10
                                    source: "qrc:/UniversalModManager/Lucide Icons/check.svg"
                                    sourceSize: Qt.size(10, 10)
                                    visible: includeDisabledMods
                                }
                            }

                            Text {
                                text: "Include disabled"
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                color: window.textColor
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                includeDisabledMods = !includeDisabledMods
                                if (!isScanning && scannedMods.length > 0) rescanNeeded = true
                            }
                        }
                    }

                    Rectangle {
                        visible: scannedMods.length > 0
                        width: 32; height: 32
                        radius: 8
                        color: copyBtnMouse.containsMouse ? Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.15) : Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.05)
                        border.width: 1
                        border.color: window.borderColor

                        Image {
                            id: copyIcon
                            anchors.centerIn: parent
                            width: 15; height: 15
                            source: "qrc:/UniversalModManager/Lucide Icons/copy.svg"
                            sourceSize: Qt.size(15, 15)
                            visible: false
                        }
                        ColorOverlay {
                            anchors.fill: copyIcon
                            source: copyIcon
                            color: themeAccent
                        }
                        MouseArea {
                            id: copyBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: modManager.copyConflictSummary()
                        }
                    }

                    Rectangle {
                        visible: scannedMods.length > 0
                        width: 32; height: 32
                        radius: 8
                        color: exportBtnMouse.containsMouse ? Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.15) : Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.05)
                        border.width: 1
                        border.color: window.borderColor

                        Image {
                            id: downloadIcon
                            anchors.centerIn: parent
                            width: 15; height: 15
                            source: "qrc:/UniversalModManager/Lucide Icons/download.svg"
                            sourceSize: Qt.size(15, 15)
                            visible: false
                        }
                        ColorOverlay {
                            anchors.fill: downloadIcon
                            source: downloadIcon
                            color: themeAccent
                        }
                        MouseArea {
                            id: exportBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: exportDialog.open()
                        }
                    }

                    Rectangle {
                        visible: selectedModIndex >= 0 && scannedMods.length > 0
                        width: viewAssetsBtnText.implicitWidth + 34
                        height: 32
                        radius: 8
                        color: viewAssetsBtnMouse.containsMouse ? Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.15) : Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.05)
                        border.width: 1
                        border.color: window.borderColor

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 6

                            Item {
                                Layout.preferredWidth: 15
                                Layout.preferredHeight: 15

                                Image {
                                    id: viewAssetsIcon
                                    anchors.centerIn: parent
                                    width: 15
                                    height: 15
                                    source: "qrc:/UniversalModManager/Lucide Icons/eye.svg"
                                    sourceSize: Qt.size(15, 15)
                                    visible: false
                                }
                                ColorOverlay {
                                    anchors.fill: viewAssetsIcon
                                    source: viewAssetsIcon
                                    color: themeAccent
                                }
                            }

                            Text {
                                id: viewAssetsBtnText
                                text: "View in Assets Viewer"
                                font.pixelSize: 12
                                font.weight: 600
                                color: themeAccent
                            }
                        }

                        MouseArea {
                            id: viewAssetsBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            enabled: selectedModIndex >= 0 && selectedModIndex < flattenedModList.length
                            onClicked: {
                                if (selectedModIndex >= 0 && selectedModIndex < flattenedModList.length) {
                                    modManager.launchPakViewerForMod(flattenedModList[selectedModIndex].name)
                                }
                            }
                        }

                        ToolTip {
                            id: viewAssetsToolTip
                            visible: viewAssetsBtnMouse.containsMouse
                            text: "Open the selected mod in the Assets Viewer"
                            delay: 400
                            padding: 8
                            contentItem: Text { text: viewAssetsToolTip.text; color: window.textColor; font.pixelSize: 11 }
                            background: Rectangle { radius: 8; color: tooltipBackgroundColor; border.color: window.borderColor }
                        }
                    }

                    Rectangle {
                        id: scanButtonRect
                        width: 86; height: 32
                        radius: 8
                        color: isScanning ? Qt.darker(themeAccent, 1.1) : themeAccent
                        transformOrigin: Item.Center

                        SequentialAnimation {
                            running: rescanNeeded && !isScanning && !window.focusAnimationsPaused
                            loops: Animation.Infinite
                            NumberAnimation { target: scanButtonRect; property: "scale"; from: 1.0; to: 1.08; duration: 320; easing.type: Easing.OutQuad }
                            NumberAnimation { target: scanButtonRect; property: "scale"; from: 1.08; to: 1.0; duration: 320; easing.type: Easing.InQuad }
                            PauseAnimation { duration: 500 }
                        }

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 6

                            Image {
                                id: scanIcon
                                width: 13; height: 13
                                source: isScanning
                                    ? "qrc:/UniversalModManager/Lucide Icons/loader.svg"
                                    : "qrc:/UniversalModManager/Lucide Icons/scan.svg"
                                sourceSize: Qt.size(13, 13)
                                visible: false

                                rotation: isScanning ? rotationValue : 0
                                property real rotationValue: 0

                                RotationAnimation {
                                    target: scanIcon
                                    property: "rotationValue"
                                    running: isScanning && !window.focusAnimationsPaused
                                    from: 0
                                    to: 360
                                    duration: 1000
                                    loops: Animation.Infinite
                                }
                            }

                            ColorOverlay {
                                width: 13; height: 13
                                source: scanIcon
                                color: "#FFFFFF"
                                rotation: scanIcon.rotation
                            }

                            Text {
                                text: isScanning ? "Scanning" : "Scan"
                                font.pixelSize: 12
                                font.weight: 600
                                color: "#FFFFFF"
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            enabled: !isScanning
                            onClicked: {
                                rescanNeeded = false
                                locallyDisabledMods = ({})
                                locallyEnabledMods = ({})
                                modManager.scanConflicts(includeDisabledMods)
                            }
                        }
                    }
                }
            }
        }

        Item {
            id: splitContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !isScanning && scannedMods.length > 0

            property int leftPanelWidth: 380
            property int minLeftPanelWidth: 280
            property int maxLeftPanelWidth: Math.min(980, Math.max(380, splitContainer.width - 800))

            Rectangle {
                id: leftPanel
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.leftPanelWidth
                radius: 12
                color: (modManager.theme === "Crimson" || modManager.theme === "Cozy" || modManager.theme === "Wavey")
                    ? "transparent"
                    : (modManager.theme === "Glassmorphic"
                        ? window.surfaceColor
                        : window.surfaceColor)
                border.width: 1
                border.color: window.borderColor

                gradient: modManager.theme === "Cozy" ? conflictCozyGradient
                    : modManager.theme === "Crimson" ? conflictCrimsonGradient
                    : modManager.theme === "Wavey" ? conflictWaveyGradient
                    : null

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Image {
                            id: packageIcon
                            width: 16; height: 16
                            source: "qrc:/UniversalModManager/Lucide Icons/package.svg"
                            sourceSize: Qt.size(16, 16)
                            visible: false
                        }
                        ColorOverlay {
                            width: 16; height: 16
                            source: packageIcon
                            color: themeAccent
                        }

                        Text {
                            text: "Mods with Conflicts"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: window.textColor
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: countText.implicitWidth + 14
                            height: 24
                            radius: 10
                            color: "#EF4444"

                            Text {
                                id: countText
                                anchors.centerIn: parent
                                text: scannedMods.length
                                font.pixelSize: 13
                                font.weight: Font.Bold
                                color: "#FFFFFF"
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 30
                        radius: 6
                        color: Qt.rgba(0, 0, 0, 0.04)
                        border.width: modSearchInput.activeFocus ? 1 : 1
                        border.color: modSearchInput.activeFocus
                            ? Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.5)
                            : window.borderColor

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8; anchors.rightMargin: 8
                            spacing: 6

                            Image {
                                id: modSearchIconImg
                                width: 12; height: 12
                                source: "qrc:/UniversalModManager/Lucide Icons/search.svg"
                                sourceSize: Qt.size(12, 12)
                                visible: false
                            }
                            ColorOverlay {
                                width: 12; height: 12
                                source: modSearchIconImg
                                color: window.textSecondaryColor
                            }

                            TextInput {
                                id: modSearchInput
                                Layout.fillWidth: true
                                font.pixelSize: 11
                                color: window.textColor
                                clip: true
                                onTextChanged: modSearchText = text

                                Text {
                                    anchors.fill: parent
                                    text: "Search mods..."
                                    font.pixelSize: 11
                                    color: window.textSecondaryColor
                                    visible: parent.text.length === 0
                                }
                            }
                        }

                        MouseArea {
                            z: 5
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            propagateComposedEvents: true
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    modSearchContextMenu.popup()
                                }
                            }
                        }

                        Menu {
                            id: modSearchContextMenu
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
                                enabled: modSearchInput.canUndo
                                onTriggered: modSearchInput.undo()
                            }
                            RuntimeContextMenuItem {
                                text: "Redo"
                                iconName: "redo-2.svg"
                                enabled: modSearchInput.canRedo
                                onTriggered: modSearchInput.redo()
                            }
                            MenuSeparator {}
                            RuntimeContextMenuItem {
                                text: "Cut"
                                iconName: "scissors.svg"
                                enabled: modSearchInput.selectedText.length > 0
                                onTriggered: modSearchInput.cut()
                            }
                            RuntimeContextMenuItem {
                                text: "Copy"
                                iconName: "copy.svg"
                                enabled: modSearchInput.selectedText.length > 0
                                onTriggered: modSearchInput.copy()
                            }
                            RuntimeContextMenuItem {
                                text: "Paste"
                                iconName: "clipboard.svg"
                                enabled: true
                                onTriggered: modSearchInput.paste()
                            }
                            RuntimeContextMenuItem {
                                text: "Delete"
                                iconName: "trash.svg"
                                enabled: modSearchInput.selectedText.length > 0
                                onTriggered: modSearchInput.remove(modSearchInput.selectionStart, modSearchInput.selectionEnd)
                            }
                            MenuSeparator {}
                            RuntimeContextMenuItem {
                                text: "Select All"
                                iconName: "text-select.svg"
                                enabled: modSearchInput.text.length > 0
                                onTriggered: modSearchInput.selectAll()
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true; height: 1
                        color: window.borderColor; opacity: 0.4
                    }

                    ListView {
                        id: modsListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: filteredModList
                        spacing: 6
                        currentIndex: selectedModIndex

                        delegate: Item {
                            width: modsListView.width
                            height: 60

                            property bool isHovered: modItemMouse.containsMouse || pakViewerBtnMouse.containsMouse || disableModBtnMouse.containsMouse

                            Canvas {
                                id: groupIndicator
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 28
                                visible: modelData.groupSize > 1

                                property color lineColor: themeAccent

                                onLineColorChanged: requestPaint()

                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.strokeStyle = lineColor
                                    ctx.lineWidth = 2.5
                                    ctx.lineCap = "round"

                                    var centerX = 14
                                    var midY = height / 2
                                    ctx.beginPath()

                                    if (modelData.isFirstInGroup && modelData.isLastInGroup) {
                                        ctx.moveTo(centerX, midY); ctx.lineTo(centerX + 10, midY)
                                    } else if (modelData.isFirstInGroup) {
                                        ctx.moveTo(centerX, midY); ctx.lineTo(centerX, height)
                                        ctx.moveTo(centerX, midY); ctx.lineTo(centerX + 10, midY)
                                    } else if (modelData.isLastInGroup) {
                                        ctx.moveTo(centerX, 0); ctx.lineTo(centerX, midY)
                                        ctx.lineTo(centerX + 10, midY)
                                    } else {
                                        ctx.moveTo(centerX, 0); ctx.lineTo(centerX, height)
                                        ctx.moveTo(centerX, midY); ctx.lineTo(centerX + 10, midY)
                                    }
                                    ctx.stroke()
                                }
                                Component.onCompleted: requestPaint()
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.leftMargin: modelData.groupSize > 1 ? 32 : 0
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                radius: 8

                                color: index === selectedModIndex
                                    ? Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.14)
                                    : (isHovered ? Qt.rgba(0, 0, 0, 0.04) : "transparent")
                                border.width: index === selectedModIndex ? 1 : 0
                                border.color: Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.4)

                                MouseArea {
                                    id: modItemMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        selectedModIndex = index
                                        selectedFileType = "all"
                                    }
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 8

                                    Item {
                                        width: 28; height: 28
                                        Layout.alignment: Qt.AlignVCenter

                                        Image {
                                            id: folderIcon
                                            anchors.centerIn: parent
                                            width: 28; height: 28
                                            source: "qrc:/UniversalModManager/Lucide Icons/folder.svg"
                                            sourceSize: Qt.size(28, 28)
                                            visible: false
                                        }
                                        ColorOverlay {
                                            anchors.fill: folderIcon
                                            source: folderIcon
                                            color: themeAccent
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                        spacing: 3

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.name
                                            font.pixelSize: 14
                                            font.weight: 600
                                            color: isDisabledForDisplay(modelData.name) ? window.textSecondaryColor : window.textColor
                                            elide: Text.ElideRight
                                        }

RowLayout {
                                        Layout.alignment: Qt.AlignLeft
                                        spacing: 6

                                        Rectangle {
                                            id: conflictBadgePill
                                            Layout.preferredWidth: conflictBadgeNumber.implicitWidth + 18
                                            Layout.preferredHeight: 26
                                            radius: 10

                                            color: "#EF4444"

                                            border.width: 1
                                            border.color: Qt.rgba(239/255, 68/255, 68/255, 0.25)

                                            Text {
                                                id: conflictBadgeNumber
                                                anchors.centerIn: parent
                                                text: modelData.conflictCount
                                                font.pixelSize: 14
                                                font.weight: Font.Bold
                                                font.family: "Segoe UI Variable Display"
                                                color: "#FFFFFF"
                                            }
                                        }

                                        Text {
                                            text: "conflict" + (modelData.conflictCount !== 1 ? "s" : "")
                                            font.pixelSize: 12
                                            font.weight: Font.Medium
                                            color: "#EF4444"
                                            Layout.alignment: Qt.AlignVCenter
                                        }
                                    }
                                    }

                                    Rectangle {
                                        readonly property bool isModDisabled: isDisabledForDisplay(modelData.name)
                                        Layout.alignment: Qt.AlignVCenter
                                        width: 76
                                        height: 28
                                        radius: 7
                                        color: isModDisabled
                                            ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, modManager.theme === "Glassmorphic" ? 0.18 : 0.12)
                                            : (disableModBtnMouse.containsMouse ? Qt.darker(themeAccent, 1.1) : themeAccent)
                                        border.width: isModDisabled ? 1 : 0
                                        border.color: isModDisabled
                                            ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.18)
                                            : "transparent"

                                        RowLayout {
                                            anchors.centerIn: parent
                                            spacing: 5

                                            Image {
                                                id: disableIcon
                                                width: 12
                                                height: 12
                                                source: "qrc:/UniversalModManager/Lucide Icons/ban.svg"
                                                sourceSize: Qt.size(12, 12)
                                                visible: false
                                            }

                                            ColorOverlay {
                                                width: 12
                                                height: 12
                                                source: disableIcon
                                                color: parent.parent.isModDisabled ? window.textSecondaryColor : "#FFFFFF"
                                            }

                                            Text {
                                                text: parent.parent.isModDisabled ? "Disabled" : "Disable"
                                                font.pixelSize: 11
                                                font.weight: Font.Medium
                                                color: parent.parent.isModDisabled ? window.textSecondaryColor : "#FFFFFF"
                                            }
                                        }

                                        MouseArea {
                                            id: disableModBtnMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: function(event) {
                                                event.accepted = true
                                                var modName = modelData.name
                                                var isCurrentlyDisabled = isDisabledForDisplay(modName)

                                                if (isCurrentlyDisabled) {
                                                    if (isDisabledInManager(modName)) {
                                                        modManager.toggleModStatus(modName)
                                                    } else {
                                                        modManager.disableConflictingMod(modName)
                                                    }
                                                    markLocallyEnabled(modName)
                                                } else {
                                                    modManager.disableConflictingMod(modName)
                                                    markLocallyDisabled(modName)
                                                }
                                                rescanNeeded = true
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.alignment: Qt.AlignVCenter
                                        width: 26; height: 26; radius: 6
                                        opacity: (isHovered || index === selectedModIndex) ? 1.0 : 0.0
                                        Behavior on opacity { NumberAnimation { duration: 140 } }
                                        color: pakViewerBtnMouse.containsMouse ? Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.2) : "transparent"

                                        Image {
                                            id: eyeIcon
                                            anchors.centerIn: parent
                                            width: 14; height: 14
                                            source: "qrc:/UniversalModManager/Lucide Icons/eye.svg"
                                            sourceSize: Qt.size(14, 14)
                                            visible: false
                                        }
                                        ColorOverlay {
                                            anchors.fill: eyeIcon
                                            source: eyeIcon
                                            color: themeAccent
                                            opacity: pakViewerBtnMouse.containsMouse ? 1.0 : 0.7
                                        }

                                        MouseArea {
                                            id: pakViewerBtnMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            enabled: parent.opacity > 0.1
                                            onClicked: function(event) {
                                                event.accepted = true
                                                modManager.launchPakViewerForMod(modelData.name)
                                            }
                                        }

                                        ToolTip {
                                            id: pakViewerToolTip
                                            visible: pakViewerBtnMouse.containsMouse && parent.opacity > 0.5
                                            text: "View in Assets Viewer"
                                            delay: 400
                                            padding: 8
                                            contentItem: Text { text: pakViewerToolTip.text; color: window.textColor; font.pixelSize: 11 }
                                            background: Rectangle { radius: 8; color: tooltipBackgroundColor; border.color: window.borderColor }
                                        }
                                    }
                                }
                            }
                        }
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            width: 6
                        }
                    }
                }
            }

            Rectangle {
                id: divider
                anchors.left: leftPanel.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 6
                width: 6
                color: dividerMouse.containsMouse || dividerMouse.pressed ? Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.3) : "transparent"

                Rectangle {
                    anchors.centerIn: parent
                    width: 2; height: parent.height
                    color: window.borderColor
                    opacity: 0.4
                }

                MouseArea {
                    id: dividerMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.SplitHCursor
                    property real dragStartX: 0
                    property int startWidth: 0
                    onPressed: function(mouse) {
                        dragStartX = mapToItem(null, mouse.x, mouse.y).x
                        startWidth = parent.parent.leftPanelWidth
                    }
                    onPositionChanged: function(mouse) {
                        if (pressed) {
                            var currentX = mapToItem(null, mouse.x, mouse.y).x
                            var newWidth = startWidth + (currentX - dragStartX)
                            var container = parent.parent
                            var minW = container.minLeftPanelWidth
                            var maxW = container.maxLeftPanelWidth
                            if (newWidth >= minW && newWidth <= maxW) {
                                container.leftPanelWidth = newWidth
                            }
                        }
                    }
                }
            }

            Rectangle {
                anchors.left: divider.right
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 6
                radius: 12
                color: (modManager.theme === "Crimson" || modManager.theme === "Cozy" || modManager.theme === "Wavey")
                    ? "transparent"
                    : (modManager.theme === "Glassmorphic"
                        ? window.surfaceColor
                        : window.surfaceColor)
                border.width: 1
                border.color: window.borderColor

                gradient: modManager.theme === "Cozy" ? conflictCozyGradient
                    : modManager.theme === "Crimson" ? conflictCrimsonGradient
                    : modManager.theme === "Wavey" ? conflictWaveyGradient
                    : null

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Image {
                            id: alertHeaderIcon
                            width: 18; height: 18
                            source: "qrc:/UniversalModManager/Lucide Icons/triangle-alert.svg"
                            sourceSize: Qt.size(18, 18)
                            visible: false
                        }
                        ColorOverlay {
                            width: 18; height: 18
                            source: alertHeaderIcon
                            color: "#EF4444"
                        }

                        Text {
                            text: "Conflicting Assets"
                            font.pixelSize: 15
                            font.weight: Font.Bold
                            color: window.textColor
                        }

                        Rectangle {
                            visible: selectedModIndex >= 0
                            width: liveCountText.implicitWidth + 14
                            height: 20
                            radius: 10
                            color: Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.12)
                            Text {
                                id: liveCountText
                                anchors.centerIn: parent
                                text: selectedModFiles.length
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                color: themeAccent
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            visible: selectedModIndex >= 0
                            Layout.preferredWidth: 220
                            height: 30
                            radius: 6
                            color: Qt.rgba(0, 0, 0, 0.04)
                            border.width: fileSearch.activeFocus ? 1 : 1
                            border.color: fileSearch.activeFocus
                                ? Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.5)
                                : window.borderColor

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                spacing: 6

                                Image {
                                    id: searchIconImg
                                    width: 12; height: 12
                                    source: "qrc:/UniversalModManager/Lucide Icons/search.svg"
                                    sourceSize: Qt.size(12, 12)
                                    visible: false
                                }
                                ColorOverlay {
                                    width: 12; height: 12
                                    source: searchIconImg
                                    color: window.textSecondaryColor
                                }

                                TextInput {
                                    id: fileSearch
                                    Layout.fillWidth: true
                                    font.pixelSize: 11
                                    color: window.textColor
                                    clip: true
                                    onTextChanged: fileSearchText = text

                                    Text {
                                        anchors.fill: parent
                                        text: "Search assets or paths..."
                                        font.pixelSize: 11
                                        color: window.textSecondaryColor
                                        visible: parent.text.length === 0
                                    }
                                }
                            }

                            MouseArea {
                                z: 5
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                propagateComposedEvents: true
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        fileSearchContextMenu.popup()
                                    }
                                }
                            }

                            Menu {
                                id: fileSearchContextMenu
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
                                    enabled: fileSearch.canUndo
                                    onTriggered: fileSearch.undo()
                                }
                                RuntimeContextMenuItem {
                                    text: "Redo"
                                    iconName: "redo-2.svg"
                                    enabled: fileSearch.canRedo
                                    onTriggered: fileSearch.redo()
                                }
                                MenuSeparator {}
                                RuntimeContextMenuItem {
                                    text: "Cut"
                                    iconName: "scissors.svg"
                                    enabled: fileSearch.selectedText.length > 0
                                    onTriggered: fileSearch.cut()
                                }
                                RuntimeContextMenuItem {
                                    text: "Copy"
                                    iconName: "copy.svg"
                                    enabled: fileSearch.selectedText.length > 0
                                    onTriggered: fileSearch.copy()
                                }
                                RuntimeContextMenuItem {
                                    text: "Paste"
                                    iconName: "clipboard.svg"
                                    enabled: true
                                    onTriggered: fileSearch.paste()
                                }
                                RuntimeContextMenuItem {
                                    text: "Delete"
                                    iconName: "trash.svg"
                                    enabled: fileSearch.selectedText.length > 0
                                    onTriggered: fileSearch.remove(fileSearch.selectionStart, fileSearch.selectionEnd)
                                }
                                MenuSeparator {}
                                RuntimeContextMenuItem {
                                    text: "Select All"
                                    iconName: "text-select.svg"
                                    enabled: fileSearch.text.length > 0
                                    onTriggered: fileSearch.selectAll()
                                }
                            }
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        visible: selectedModIndex >= 0
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ScrollBar.vertical.policy: ScrollBar.AlwaysOff

                        Row {
                            spacing: 8

                            Repeater {
                                model: sortedFileTypeKeys

                                Rectangle {
                                    width: chipLabel.implicitWidth + 20
                                    height: 28
                                    radius: 14
                                    color: selectedFileType === modelData
                                        ? themeAccent
                                        : Qt.rgba(0, 0, 0, 0.04)
                                    border.width: selectedFileType === modelData ? 0 : 1
                                    border.color: window.borderColor

                                    Text {
                                        id: chipLabel
                                        anchors.centerIn: parent
                                        text: modelData === "all"
                                            ? "All (" + (flattenedModList.length > 0 && selectedModIndex >= 0
                                                ? (flattenedModList[selectedModIndex].files || []).length
                                                : 0) + ")"
                                            : modelData + " (" + fileTypes[modelData] + ")"
                                        font.pixelSize: 11
                                        font.weight: selectedFileType === modelData ? 600 : 400
                                        color: selectedFileType === modelData ? "#FFFFFF" : window.textColor
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: selectedFileType = modelData
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true; height: 1
                        color: window.borderColor; opacity: 0.4
                    }

                    ListView {
                        id: filesListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: selectedModFiles
                        spacing: 6
                        visible: selectedModIndex >= 0

                        delegate: Rectangle {
                            width: filesListView.width
                            height: 80
                            radius: 8
                            color: fileItemMouse.containsMouse ? Qt.rgba(0, 0, 0, 0.03) : Qt.rgba(0, 0, 0, 0.015)
                            border.width: 1
                            border.color: window.borderColor

                            MouseArea {
                                id: fileItemMouse
                                anchors.fill: parent
                                hoverEnabled: true
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16; anchors.rightMargin: 16
                                spacing: 0

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Layout.alignment: Qt.AlignVCenter

                                    Text {
                                        Layout.fillWidth: true
                                        text: getFileName(modelData.path)
                                        font.pixelSize: 14
                                        font.weight: Font.Bold
                                        color: window.textColor
                                        elide: Text.ElideMiddle
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: getFilePath(modelData.path)
                                        font.pixelSize: 11
                                        color: window.textSecondaryColor
                                        elide: Text.ElideMiddle
                                    }

                                    RowLayout {
                                        spacing: 4
                                        visible: getConflictingModsForFile(modelData.path).length > 0

                                        Image {
                                            id: colAlertIcon
                                            width: 10; height: 10
                                            source: "qrc:/UniversalModManager/Lucide Icons/triangle-alert.svg"
                                            sourceSize: Qt.size(10, 10)
                                            visible: false
                                        }
                                        ColorOverlay {
                                            width: 10; height: 10
                                            source: colAlertIcon
                                            color: "#EF4444"
                                        }

                                        Text {
                                            text: "Collides with: "
                                            font.pixelSize: 11
                                            color: window.textSecondaryColor
                                        }

                                        Text {
                                            text: getConflictingModsForFile(modelData.path).join(", ")
                                            font.pixelSize: 11
                                            font.weight: 600
                                            color: window.textColor
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            width: 6
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: selectedModIndex < 0

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 12

                            Image {
                                id: emptyStateIcon
                                Layout.alignment: Qt.AlignHCenter
                                width: 48; height: 48
                                source: "qrc:/UniversalModManager/Lucide Icons/mouse-pointer-click.svg"
                                sourceSize: Qt.size(48, 48)
                                visible: false
                            }
                            ColorOverlay {
                                Layout.alignment: Qt.AlignHCenter
                                width: 48; height: 48
                                source: emptyStateIcon
                                color: themeAccent
                                opacity: 0.4
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Select a mod to view conflicts"
                                font.pixelSize: 13
                                color: window.textSecondaryColor
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !isScanning && scannedMods.length === 0 && lastScanTime === ""
            radius: 12
            color: Qt.rgba(window.surfaceColor.r, window.surfaceColor.g, window.surfaceColor.b, 0.5)
            border.width: 1
            border.color: window.borderColor

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    width: 52; height: 52

                    Image {
                        id: emptySearchIcon
                        anchors.fill: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/scan-search.svg"
                        sourceSize: Qt.size(52, 52)
                        visible: false
                    }
                    ColorOverlay {
                        anchors.fill: parent
                        source: emptySearchIcon
                        color: themeAccent
                        opacity: 0.5
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "No Conflict Scan Results"
                    font.pixelSize: 15
                    font.weight: Font.Bold
                    color: window.textColor
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Click \"Scan\" above to check all installed mods for asset file conflicts."
                    font.pixelSize: 12
                    color: window.textSecondaryColor
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !isScanning && scannedMods.length === 0 && lastScanTime !== ""
            radius: 12
            color: Qt.rgba(34/255, 197/255, 94/255, 0.08)
            border.width: 1
            border.color: Qt.rgba(34/255, 197/255, 94/255, 0.3)

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    width: 52; height: 52

                    Image {
                        id: noConflictCheckIcon
                        anchors.centerIn: parent
                        width: 52; height: 52
                        source: "qrc:/UniversalModManager/Lucide Icons/circle-check.svg"
                        sourceSize: Qt.size(52, 52)
                        visible: false
                    }

                    ColorOverlay {
                        anchors.fill: noConflictCheckIcon
                        source: noConflictCheckIcon
                        color: "#22C55E"
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "No Conflicts Detected!"
                    font.pixelSize: 16
                    font.weight: Font.Bold
                    color: "#22C55E"
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "All your installed mods are fully compatible and modify separate files."
                    font.pixelSize: 12
                    color: window.textSecondaryColor
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: isScanning
            radius: 12
            color: window.surfaceColor
            border.width: 1
            border.color: Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.3)

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 14

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    width: 48
                    height: 48

                    Canvas {
                        id: loadingCircle
                        anchors.fill: parent
                        antialiasing: true

                        property real rotationAngle: 0

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)

                            ctx.save()
                            ctx.translate(width / 2, height / 2)
                            ctx.rotate(rotationAngle * Math.PI / 180)

                            ctx.lineWidth = 4
                            ctx.lineCap = "round"
                            ctx.strokeStyle = Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 1.0)

                            ctx.beginPath()
                            ctx.arc(0, 0, 18, 0, 1.5 * Math.PI)
                            ctx.stroke()

                            ctx.restore()
                        }

                        RotationAnimation on rotationAngle {
                            running: isScanning && !window.focusAnimationsPaused
                            from: 0
                            to: 360
                            duration: 1000
                            loops: Animation.Infinite
                        }

                        onRotationAngleChanged: requestPaint()
                        Component.onCompleted: requestPaint()
                    }
                }

                Text {
                    text: "Scanning Mod Packages..."
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    color: window.textColor
                }

                Text {
                    text: scanProgress || "Reading .pak and .utoc assets..."
                    font.pixelSize: 12
                    color: window.textSecondaryColor
                }
            }
        }
    }

    function getFileTypeIcon(fileType) {
        switch (fileType) {
            case "Texture 2D":         return "qrc:/UniversalModManager/Lucide Icons/image.svg"
            case "Material":
            case "Material Instance":  return "qrc:/UniversalModManager/Lucide Icons/palette.svg"
            case "Skeletal Mesh":
            case "Static Mesh":        return "qrc:/UniversalModManager/Lucide Icons/box.svg"
            case "Physics Asset":      return "qrc:/UniversalModManager/Lucide Icons/triangle-alert.svg"
            case "Animation":          return "qrc:/UniversalModManager/Lucide Icons/play.svg"
            case "Blueprint":          return "qrc:/UniversalModManager/Lucide Icons/code.svg"
            case "Data Asset":
            case "Data Table":         return "qrc:/UniversalModManager/Lucide Icons/database.svg"
            case "Audio":              return "qrc:/UniversalModManager/Lucide Icons/music.svg"
            case "Niagara VFX":        return "qrc:/UniversalModManager/Lucide Icons/sparkles.svg"
            default:                   return "qrc:/UniversalModManager/Lucide Icons/file.svg"
        }
    }

    function getFileTypeIconColor(fileType) {
        var colors = conflictColors
        switch (fileType) {
            case "Texture 2D":         return colors.conflictTextureColor || "#0284C7"
            case "Material":
            case "Material Instance":  return colors.conflictMaterialColor || "#A855F7"
            case "Skeletal Mesh":
            case "Static Mesh":        return colors.conflictSkeletalMeshColor || "#10B981"
            case "Physics Asset":      return "#EF4444"
            case "Animation":          return colors.conflictAnimationColor || "#F97316"
            case "Blueprint":          return colors.conflictBlueprintColor || "#EC4899"
            case "Data Asset":
            case "Data Table":         return "#14B8A6"
            case "Audio":              return "#8B5CF6"
            case "Niagara VFX":        return "#06B6D4"
            default:                   return colors.conflictDefaultColor || "#6B7280"
        }
    }

    function getSeverityOverrideText(fileType) {
        switch (fileType) {
            case "Physics Asset":
            case "Skeletal Mesh":
            case "Static Mesh":
                return "PHYSICS OVERRIDE"
            case "Material":
            case "Material Instance":
                return "MATERIAL OVERRIDE"
            case "Texture 2D":
                return "TEXTURE OVERRIDE"
            default:
                return "ASSET OVERRIDE"
        }
    }

    function getSeverityOverrideColor(fileType) {
        switch (fileType) {
            case "Physics Asset":
            case "Skeletal Mesh":
            case "Static Mesh":
                return "#EF4444"
            case "Material":
            case "Material Instance":
                return "#A855F7"
            case "Texture 2D":
                return "#0284C7"
            default:
                return "#475569"
        }
    }

    function getFileName(path) {
        if (!path) return "Unknown"
        var parts = path.split("/")
        return parts[parts.length - 1] || path
    }

    function getFilePath(path) {
        if (!path) return ""
        var parts = path.split("/")
        parts.pop()
        return parts.join("/")
    }

    function getConflictingModsForFile(filePath) {
        if (selectedModIndex < 0 || selectedModIndex >= flattenedModList.length) return []

        var selectedModName = flattenedModList[selectedModIndex].name
        var conflictingMods = []

        if (conflictGroups) {
            for (var i = 0; i < conflictGroups.length; i++) {
                var group = conflictGroups[i]
                var conflicts = group.conflicts || []
                for (var j = 0; j < conflicts.length; j++) {
                    var conflict = conflicts[j]
                    if (conflict.filePath === filePath) {
                        var mods = conflict.conflictingMods || []
                        for (var k = 0; k < mods.length; k++) {
                            var modName = typeof mods[k] === "string" ? mods[k] : (mods[k].name || "Unknown")
                            if (modName !== selectedModName && conflictingMods.indexOf(modName) === -1) {
                                conflictingMods.push(modName)
                            }
                        }
                    }
                }
            }
        }

        return conflictingMods
    }

    Platform.FileDialog {
        id: exportDialog
        title: "Export Conflict Report"
        fileMode: Platform.FileDialog.SaveFile
        nameFilters: ["JSON files (*.json)"]
        defaultSuffix: "json"
        currentFile: "file:///conflict_report.json"
        onAccepted: {
            var path = file.toString().replace("file:///", "")
            modManager.exportConflictReport(path)
        }
    }
}
