import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Popup {
    id: quickScanDialog
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    width: Math.min(680, parent.width * 0.85)
    height: Math.min(560, parent.height * 0.85)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2

    padding: 0

    property bool isScanning: modManager.isConflictScanning
    property var conflictGroups: modManager.conflictGroups
    property string scanProgress: modManager.conflictScanProgress

    property bool includeDisabledMods: false
    property bool lastScanIncludeDisabled: false
    property bool rescanNeeded: false
    property var locallyDisabledMods: ({})
    property var locallyEnabledMods: ({})
    property bool footerInfoDismissed: false

    Gradient {
        id: quickScanDialogCozyGradient
        GradientStop { position: 0.0; color: window.cozyDialogGradientTop }
        GradientStop { position: 1.0; color: window.cozyDialogGradientBottom }
        orientation: Gradient.Vertical
    }
    Gradient {
        id: quickScanDialogCrimsonGradient
        GradientStop { position: 0.0; color: window.crimsonDialogGradientTop }
        GradientStop { position: 1.0; color: window.crimsonDialogGradientBottom }
        orientation: Gradient.Vertical
    }
    Gradient {
        id: quickScanDialogWaveyGradient
        GradientStop { position: 0.0; color: window.waveyDialogGradientTop }
        GradientStop { position: 1.0; color: window.waveyDialogGradientBottom }
        orientation: Gradient.Vertical
    }
    Gradient {
        id: quickScanDialogGlassmorphicGradient
        GradientStop { position: 0.0; color: Qt.rgba(0.16, 0.17, 0.22, 0.95) }
        GradientStop { position: 1.0; color: Qt.rgba(0.08, 0.09, 0.13, 0.96) }
        orientation: Gradient.Vertical
    }

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

    function getModName(modEntry) {
        return typeof modEntry === "string" ? modEntry : modEntry.name
    }

    function getRelevantMods(modList) {
        var result = []
        for (var i = 0; i < modList.length; i++) {
            var modName = getModName(modList[i])
            if (lastScanIncludeDisabled || !isDisabledInManager(modName)) {
                result.push(modName)
            }
        }
        return result
    }

    function isDisabledForDisplay(name) {
        var key = modKey(name)
        if (isLocallyEnabled(name)) return false
        return isLocallyDisabled(name) || (lastScanIncludeDisabled && (isDisabledInManager(name) || scannedModStatus[key]))
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

    readonly property color themeAccent: {
        if (modManager.theme === "Wavey") return "#3b82f6"
        if (modManager.theme === "Crimson") return themeConfig.crimson.accentColor
        if (modManager.theme === "Glassmorphic") return modManager.accentColor || "#8B5CF6"
        return window.primaryColor
    }

    ThemeConfig { id: themeConfig }

    readonly property var conflictColors: {
        if (modManager.theme === "Crimson") return themeConfig.crimson
        if (modManager.theme === "Wavey") return themeConfig.wavey
        return themeConfig.cozy
    }

    property var conflictingModsWithRelations: {
        var modData = {}

        if (conflictGroups) {
            for (var i = 0; i < conflictGroups.length; i++) {
                var group = conflictGroups[i]
                var conflicts = group.conflicts || []
                for (var j = 0; j < conflicts.length; j++) {
                    var conflict = conflicts[j]
                    var mods = getRelevantMods(conflict.conflictingMods || [])
                    if (mods.length >= 2) {

                        for (var k = 0; k < mods.length; k++) {
                            var modName = mods[k]

                            if (!modData[modName]) {
                                modData[modName] = {
                                    name: modName,
                                    conflictCount: 0,
                                    conflictsWith: {}
                                }
                            }

                            modData[modName].conflictCount++

                            for (var m = 0; m < mods.length; m++) {
                                if (m !== k) {
                                    var otherMod = mods[m]
                                    modData[modName].conflictsWith[otherMod] = true
                                }
                            }
                        }
                    }
                }
            }
        }

        var result = []
        for (var mod in modData) {
            var conflictsWithArray = []
            for (var other in modData[mod].conflictsWith) {
                conflictsWithArray.push(other)
            }
            result.push({
                name: modData[mod].name,
                conflictCount: modData[mod].conflictCount,
                conflictsWith: conflictsWithArray
            })
        }

        result.sort(function(a, b) {
            return b.conflictCount - a.conflictCount
        })

        return result
    }

    property var visualGroups: {
        var groups = []
        var processed = {}

        for (var i = 0; i < conflictingModsWithRelations.length; i++) {
            var mod = conflictingModsWithRelations[i]
            if (processed[mod.name]) continue

            var group = [mod.name]
            processed[mod.name] = true

            for (var j = 0; j < mod.conflictsWith.length; j++) {
                var otherModName = mod.conflictsWith[j]
                if (!processed[otherModName]) {
                    group.push(otherModName)
                    processed[otherModName] = true
                }
            }

            if (group.length > 1) {
                groups.push(group)
            } else {
                groups.push([mod.name])
            }
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
                for (var k = 0; k < conflictingModsWithRelations.length; k++) {
                    if (conflictingModsWithRelations[k].name === modName) {
                        modInfo = conflictingModsWithRelations[k]
                        break
                    }
                }

                if (modInfo) {
                    result.push({
                        name: modInfo.name,
                        conflictCount: modInfo.conflictCount,
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

    property int totalConflicts: {
        var total = 0
        if (conflictGroups) {
            for (var i = 0; i < conflictGroups.length; i++) {
                var group = conflictGroups[i]
                var conflicts = group.conflicts || []
                for (var j = 0; j < conflicts.length; j++) {
                    var conflict = conflicts[j]
                    var mods = getRelevantMods(conflict.conflictingMods || [])
                    if (mods.length >= 2) {
                        total++
                    }
                }
            }
        }
        return total
    }

    property var suggestions: {
        var list = []
        if (conflictGroups) {
            var seen = {}
            for (var i = 0; i < conflictGroups.length; i++) {
                var g = conflictGroups[i]
                var s = g.suggestion
                if (s && s.modToDisable) {
                    var key = (s.modToDisable + "").toLowerCase()
                    if (!seen[key]) {
                        list.push({ mod: s.modToDisable, reason: s.reason || "", score: s.score || 0 })
                        seen[key] = true
                    }
                }
            }
        }
        return list
    }

    onOpened: {
        rescanNeeded = false
        locallyDisabledMods = ({})
        locallyEnabledMods = ({})
        footerInfoDismissed = false
    }

    background: Rectangle {
        radius: 16
        border.width: 1
        border.color: modManager.theme === "Glassmorphic"
            ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.20)
            : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
        gradient: modManager.theme === "Cozy" ? quickScanDialogCozyGradient
                : modManager.theme === "Crimson" ? quickScanDialogCrimsonGradient
                : modManager.theme === "Wavey" ? quickScanDialogWaveyGradient
                : quickScanDialogGlassmorphicGradient

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowOpacity: 0.25
            shadowBlur: 0.4
            shadowVerticalOffset: 8
            shadowHorizontalOffset: 0
            shadowColor: "#000000"
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 16
                spacing: 12

                Rectangle {
                    width: 36
                    height: 36
                    radius: 10
                    color: Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.15)

                    Image {
                        id: headerIcon
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        source: "qrc:/UniversalModManager/Lucide Icons/scan-search.svg"
                        sourceSize: Qt.size(18, 18)
                        visible: false
                    }

                    ColorOverlay {
                        anchors.fill: headerIcon
                        source: headerIcon
                        color: themeAccent
                    }
                }

                Text {
                    text: "Quick Conflict Scan"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: window.textColor
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    spacing: 8
                    visible: !isScanning

                    Rectangle {
                        width: 18
                        height: 18
                        radius: 4
                        color: includeDisabledMods ? themeAccent : "transparent"
                        border.width: includeDisabledMods ? 0 : 1.5
                        border.color: window.borderColor

                        Image {
                            anchors.centerIn: parent
                            width: 10
                            height: 10
                            source: "qrc:/UniversalModManager/Lucide Icons/check.svg"
                            sourceSize: Qt.size(10, 10)
                            visible: includeDisabledMods
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                includeDisabledMods = !includeDisabledMods
                                if (!isScanning && flattenedModList.length > 0) rescanNeeded = true
                            }
                        }
                    }

                    Text {
                        text: "Include disabled"
                        font.pixelSize: 12
                        color: window.textSecondaryColor
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                includeDisabledMods = !includeDisabledMods
                                if (!isScanning && flattenedModList.length > 0) rescanNeeded = true
                            }
                        }
                    }
                }

                Rectangle {
                    id: rescanButton
                    visible: !isScanning
                    width: 80
                    height: 32
                    radius: 8
                    color: scanBtnMouse.containsMouse ? Qt.darker(themeAccent, 1.1) : themeAccent
                    transformOrigin: Item.Center

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6

                        Image {
                            id: rescanIcon
                            width: 12
                            height: 12
                            source: "qrc:/UniversalModManager/Lucide Icons/refresh-cw.svg"
                            sourceSize: Qt.size(12, 12)
                            visible: false
                        }

                        ColorOverlay {
                            width: 12
                            height: 12
                            source: rescanIcon
                            color: "#FFFFFF"
                        }

                        Text {
                            text: "Rescan"
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            color: "#FFFFFF"
                        }
                    }

                    MouseArea {
                        id: scanBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            rescanNeeded = false
                            locallyDisabledMods = ({})
                            lastScanIncludeDisabled = includeDisabledMods
                            modManager.scanConflicts(includeDisabledMods)
                        }
                    }
                }

                SequentialAnimation {
                    running: rescanNeeded && !isScanning && !window.focusAnimationsPaused
                    loops: Animation.Infinite
                    NumberAnimation { target: rescanButton; property: "scale"; from: 1.0; to: 1.08; duration: 280; easing.type: Easing.OutQuad }
                    NumberAnimation { target: rescanButton; property: "scale"; from: 1.08; to: 1.0; duration: 280; easing.type: Easing.InQuad }
                    PauseAnimation { duration: 700 }
                }

                Rectangle {
                    width: 32
                    height: 32
                    radius: 8
                    color: closeBtnMouse.containsMouse ? Qt.rgba(0, 0, 0, 0.1) : "transparent"

                    Image {
                        id: closeIcon
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                        sourceSize: Qt.size(16, 16)
                        visible: false
                    }

                    ColorOverlay {
                        anchors.fill: closeIcon
                        source: closeIcon
                        color: window.textSecondaryColor
                    }

                    MouseArea {
                        id: closeBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: quickScanDialog.close()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: window.borderColor
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 16
                visible: isScanning

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    width: 48
                    height: 48

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width
                        height: parent.height
                        radius: width / 2
                        color: "transparent"
                        border.width: 3
                        border.color: Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.2)
                    }

                    Rectangle {
                        id: spinner
                        anchors.centerIn: parent
                        width: parent.width
                        height: parent.height
                        radius: width / 2
                        color: "transparent"

                        Rectangle {
                            width: 3
                            height: parent.height / 2
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            color: themeAccent
                            radius: 1.5
                        }

                        RotationAnimation on rotation {
                            from: 0
                            to: 360
                            duration: 1000
                            loops: Animation.Infinite
                            running: isScanning
                        }
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Scanning mods..."
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    color: window.textColor
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: scanProgress || "Please wait"
                    font.pixelSize: 12
                    color: window.textSecondaryColor
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                visible: !isScanning && totalConflicts === 0

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    width: 64
                    height: 64

                    Image {
                        id: successIcon
                        anchors.centerIn: parent
                        width: 32
                        height: 32
                        source: modManager.modList.length === 0
                            ? "qrc:/UniversalModManager/Lucide Icons/triangle-alert.svg"
                            : "qrc:/UniversalModManager/Lucide Icons/circle-check.svg"
                        sourceSize: Qt.size(32, 32)
                        visible: false
                    }

                    ColorOverlay {
                        anchors.fill: successIcon
                        source: successIcon
                        color: modManager.modList.length === 0
                            ? (modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey" ? themeAccent : "#F59E0B")
                            : "#22C55E"
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: modManager.modList.length === 0 ? "No mods found to scan" : "No conflicts found!"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: modManager.modList.length === 0
                        ? (modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey" ? themeAccent : "#F59E0B")
                        : "#22C55E"
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: modManager.modList.length === 0 ? "Please install Mods." : "All your enabled mods are compatible"
                    font.pixelSize: 13
                    color: window.textSecondaryColor
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12
                visible: !isScanning && totalConflicts > 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    radius: 10
                    color: Qt.rgba(239/255, 68/255, 68/255, 0.1)
                    border.width: 1
                    border.color: Qt.rgba(239/255, 68/255, 68/255, 0.3)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 10

                        Image {
                            id: warningIcon
                            width: 20
                            height: 20
                            source: "qrc:/UniversalModManager/Lucide Icons/triangle-alert.svg"
                            sourceSize: Qt.size(20, 20)
                            visible: false
                        }

                        ColorOverlay {
                            width: 20
                            height: 20
                            source: warningIcon
                            color: "#EF4444"
                        }

                        Text {
                            text: totalConflicts + " asset" + (totalConflicts !== 1 ? "s" : "") + " with conflicts detected"
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            color: "#EF4444"
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: "Multiple mods are modifying the same assets"
                            font.pixelSize: 11
                            color: window.textSecondaryColor
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 0
                    visible: false
                    radius: 10
                    color: Qt.rgba(253/255, 230/255, 138/255, 0.2)
                    border.width: 1
                    border.color: Qt.rgba(234/255, 179/255, 8/255, 0.35)

                    RowLayout {
                        id: suggestionRow
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 10

                        Text {
                            text: "Suggested to disable:"
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            color: window.textColor
                        }

                        Rectangle {
                            width: modChipText.implicitWidth + 16
                            height: 24
                            radius: 6
                            color: Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.12)
                            border.width: 1
                            border.color: Qt.rgba(themeAccent.r, themeAccent.g, themeAccent.b, 0.25)

                            Text {
                                id: modChipText
                                anchors.centerIn: parent
                                text: suggestions.length > 0 ? suggestions[0].mod : ""
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                color: themeAccent
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            readonly property bool isDisabled: suggestions.length > 0 ? isDisabledForDisplay(suggestions[0].mod) : false
                            width: disableBtnRow.implicitWidth + 16
                            height: 26
                            radius: 6
                            color: isDisabled
                                ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, modManager.theme === "Glassmorphic" ? 0.18 : 0.12)
                                : themeAccent
                            border.width: isDisabled ? 1 : 0
                            border.color: isDisabled
                                ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.18)
                                : "transparent"

                            RowLayout {
                                id: disableBtnRow
                                anchors.centerIn: parent
                                spacing: 6

                                Image {
                                    id: banIcon
                                    width: 12
                                    height: 12
                                    source: "qrc:/UniversalModManager/Lucide Icons/ban.svg"
                                    sourceSize: Qt.size(12, 12)
                                    visible: false
                                }

                                ColorOverlay {
                                    width: 12
                                    height: 12
                                    source: banIcon
                                    color: parent.parent.isDisabled ? window.textSecondaryColor : "#FFFFFF"
                                }

                                Text {
                                    text: parent.parent.isDisabled ? "Disabled" : "Disable"
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    color: parent.parent.isDisabled ? window.textSecondaryColor : "#FFFFFF"
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (suggestions.length > 0) {
                                        var modName = suggestions[0].mod
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
                        }
                    }
                }

                ListView {
                    id: conflictingModsList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: flattenedModList
                    spacing: 8

                    ScrollBar.vertical: ScrollBar { }

                    delegate: Item {
                        width: conflictingModsList.width - 8
                        height: 56

                        Canvas {
                            id: groupIndicator
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 40
                            visible: modelData.groupSize > 1

                            property color lineColor: themeAccent

                            onLineColorChanged: requestPaint()

                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)

                                ctx.strokeStyle = lineColor
                                ctx.lineWidth = 3
                                ctx.lineCap = "round"

                                var centerX = 20
                                var midY = height / 2

                                ctx.beginPath()

                                if (modelData.isFirstInGroup && modelData.isLastInGroup) {

                                    ctx.moveTo(centerX, midY)
                                    ctx.lineTo(centerX + 15, midY)
                                } else if (modelData.isFirstInGroup) {

                                    ctx.moveTo(centerX, midY)
                                    ctx.lineTo(centerX, height)
                                    ctx.moveTo(centerX, midY)
                                    ctx.lineTo(centerX + 15, midY)
                                } else if (modelData.isLastInGroup) {

                                    ctx.moveTo(centerX, 0)
                                    ctx.lineTo(centerX, midY)
                                    ctx.lineTo(centerX + 15, midY)
                                } else {

                                    ctx.moveTo(centerX, 0)
                                    ctx.lineTo(centerX, height)
                                    ctx.moveTo(centerX, midY)
                                    ctx.lineTo(centerX + 15, midY)
                                }

                                ctx.stroke()
                            }

                            Component.onCompleted: requestPaint()
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.leftMargin: modelData.groupSize > 1 ? 48 : 0
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            radius: 10
                            color: modManager.theme === "Glassmorphic"
                                ? window.glassSurfaceSubtle
                                : window.surfaceColor
                            border.width: 1
                            border.color: window.borderColor

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: window.textColor
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    width: conflictCountText.implicitWidth + 16
                                    height: 26
                                    radius: 6
                                    color: Qt.rgba(239/255, 68/255, 68/255, 0.15)
                                    border.width: 1
                                    border.color: Qt.rgba(239/255, 68/255, 68/255, 0.3)

                                    Text {
                                        id: conflictCountText
                                        anchors.centerIn: parent
                                        text: modelData.conflictCount + " conflict" + (modelData.conflictCount !== 1 ? "s" : "")
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                        color: "#EF4444"
                                    }
                                }

                                Rectangle {
                                    readonly property bool isDisabled: isDisabledForDisplay(modelData.name)
                                    width: disableModBtnRow.implicitWidth + 16
                                    height: 32
                                    radius: 8
                                    color: isDisabled
                                        ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, modManager.theme === "Glassmorphic" ? 0.18 : 0.12)
                                        : (disableModBtnMouse.containsMouse ? Qt.darker(themeAccent, 1.1) : themeAccent)
                                    border.width: isDisabled ? 1 : 0
                                    border.color: isDisabled
                                        ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.18)
                                        : "transparent"

                                    RowLayout {
                                        id: disableModBtnRow
                                        anchors.centerIn: parent
                                        spacing: 6

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
                                            color: parent.parent.isDisabled ? window.textSecondaryColor : "#FFFFFF"
                                        }

                                        Text {
                                            text: parent.parent.isDisabled ? "Disabled" : "Disable"
                                            font.pixelSize: 11
                                            font.weight: Font.Medium
                                            color: parent.parent.isDisabled ? window.textSecondaryColor : "#FFFFFF"
                                        }
                                    }

                                    MouseArea {
                                        id: disableModBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
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
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: footerInfoDismissed ? 0 : 56
            visible: !footerInfoDismissed
            color: Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.3)

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 12

                Image {
                    id: infoIcon
                    width: 14
                    height: 14
                    source: "qrc:/UniversalModManager/Lucide Icons/info.svg"
                    sourceSize: Qt.size(14, 14)
                    visible: false
                }

                ColorOverlay {
                    width: 14
                    height: 14
                    source: infoIcon
                    color: window.textSecondaryColor
                }

                Text {
                    Layout.fillWidth: true
                    text: "For detailed analysis, use the Advanced Conflicts Detector"
                    font.pixelSize: 12
                    color: window.textSecondaryColor
                }

                Rectangle {
                    width: 24
                    height: 24
                    radius: 6
                    color: footerCloseMouse.containsMouse ? Qt.rgba(0, 0, 0, 0.1) : "transparent"

                    Image {
                        id: footerCloseIcon
                        anchors.centerIn: parent
                        width: 14
                        height: 14
                        source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                        sourceSize: Qt.size(14, 14)
                        visible: false
                    }

                    ColorOverlay {
                        anchors.fill: footerCloseIcon
                        source: footerCloseIcon
                        color: window.textSecondaryColor
                    }

                    MouseArea {
                        id: footerCloseMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: footerInfoDismissed = true
                    }
                }
            }
        }
    }

    function getFileTypeIcon(fileType) {
        switch ((fileType || "").toLowerCase()) {
            case "texture": return "qrc:/UniversalModManager/Lucide Icons/image.svg"
            case "material": return "qrc:/UniversalModManager/Lucide Icons/palette.svg"
            case "skeletalmesh": return "qrc:/UniversalModManager/Lucide Icons/box.svg"
            case "staticmesh": return "qrc:/UniversalModManager/Lucide Icons/box.svg"
            case "animation": return "qrc:/UniversalModManager/Lucide Icons/play.svg"
            case "blueprint": return "qrc:/UniversalModManager/Lucide Icons/code.svg"
            default: return "qrc:/UniversalModManager/Lucide Icons/file.svg"
        }
    }

    function getFileTypeIconColor(fileType) {
        var colors = conflictColors
        switch ((fileType || "").toLowerCase()) {
            case "texture": return colors.conflictTextureColor || "#3B82F6"
            case "material": return colors.conflictMaterialColor || "#A855F7"
            case "skeletalmesh": return colors.conflictSkeletalMeshColor || "#22C55E"
            case "staticmesh": return colors.conflictStaticMeshColor || "#22C55E"
            case "animation": return colors.conflictAnimationColor || "#F97316"
            case "blueprint": return colors.conflictBlueprintColor || "#EF4444"
            default: return colors.conflictDefaultColor || "#6B7280"
        }
    }

    function getFileTypeColor(fileType) {
        var baseColor = getFileTypeIconColor(fileType)
        var c = Qt.color ? Qt.color(baseColor) : baseColor
        return Qt.rgba(c.r, c.g, c.b, 0.15)
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

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: 200; easing.type: Easing.OutCubic }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 150; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; from: 1; to: 0.95; duration: 150; easing.type: Easing.InCubic }
    }
}
