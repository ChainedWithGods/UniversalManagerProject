import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Dialog {
    id: renameDialog
    modal: true
    closePolicy: Popup.NoAutoClose
    anchors.centerIn: Overlay.overlay
    width: Math.min(1100, parent.width * 0.96)
    height: Math.min(820, parent.height * 0.94)
    padding: 24

    property var modGroups: []
    property string sessionId: ""
    property string bundleFolderName: ""
    property var archiveBundleNames: ({})
    property var archiveBundleEnabled: ({})
    property bool singleBundleEnabled: true
    property var collapsedArchives: ({})
    property int bundleNamesRevision: 0
    property int selectedIndex: -1
    property bool selectedGroupEnabled: false
    property string searchText: ""
    property int listRevision: 0
    property bool syncingNameField: false
    readonly property var rows: buildRows()
    readonly property var currentGroup: (selectedIndex >= 0 && selectedIndex < modGroups.length) ? modGroups[selectedIndex] : ({})

    readonly property color installAccent: modManager.theme === "Glassmorphic"
        ? modManager.accentColor
        : (modManager.theme === "Wavey" ? "#60a5fa" : window.primaryColor)

    readonly property bool hasBlockingErrors: {
        for (var i = 0; i < modGroups.length; ++i) {
            if (modGroups[i].installEnabled !== false && modGroups[i].installValid === false)
                return true
        }
        return false
    }

    function planRootCount() {
        var paths = {}
        var count = 0
        for (var i = 0; i < modGroups.length; ++i) {
            var roots = modGroups[i].installRoots || []
            for (var j = 0; j < roots.length; ++j) {
                var path = roots[j].rootPath || ""
                if (path !== "" && !paths[path]) {
                    paths[path] = true
                    count++
                }
            }
        }
        return count
    }

    function planFileCount() {
        var count = 0
        for (var i = 0; i < modGroups.length; ++i) {
            var roots = modGroups[i].installRoots || []
            for (var j = 0; j < roots.length; ++j) {
                count += (roots[j].coreFiles ? roots[j].coreFiles.length : 0)
                count += (roots[j].companionFiles ? roots[j].companionFiles.length : 0)
            }
        }
        return count
    }

    function pathLeaf(path) {
        if (!path)
            return ""
        var parts = String(path).replace(/\\/g, "/").split("/")
        while (parts.length > 0 && parts[parts.length - 1] === "")
            parts.pop()
        return parts.length > 0 ? parts[parts.length - 1] : ""
    }

    function cleanedSourceName(path) {
        if (!path)
            return ""
        var base = String(path).replace(/\\/g, "/").split("/").pop() || ""
        base = base.replace(/\.(zip|rar|7z)$/i, "")
        base = base.replace(/-\d+-\d+-\d+-\d+.*$/i, "")
        base = base.replace(/\s+\d+\s+\d+\s+\d{4}-\d{2}-\d{2}.*$/i, "")
        base = base.replace(/\s*\(\d+\)\s*$/, "")
        base = base.replace(/[\\/:*?"<>|]/g, "").trim()
        return base
    }

    function distinctInstallSources() {
        var sources = []
        for (var i = 0; i < modGroups.length; ++i) {
            var raw = modGroups[i].archivePath || modGroups[i].folderPath || ""
            if (raw === "")
                continue
            var key = raw.toLowerCase()
            var found = false
            for (var j = 0; j < sources.length; ++j) {
                if (sources[j].toLowerCase() === key) {
                    found = true
                    break
                }
            }
            if (!found)
                sources.push(raw)
        }
        return sources
    }

    function bundleForGroup(group) {
        var sources = distinctInstallSources()
        if (sources.length <= 1)
            return singleBundleEnabled ? safeFolderName(bundleFolderName) : ""

        var raw = group ? (group.archivePath || group.folderPath || "") : ""
        if (raw === "" || !isArchiveBundleEnabled(raw))
            return ""
        return safeFolderName(archiveBundleNames[raw] || "")
    }

    function cloneMap(source) {
        var target = {}
        for (var key in source)
            target[key] = source[key]
        return target
    }

    function setArchiveBundleName(source, value) {
        var updated = cloneMap(archiveBundleNames || ({}))
        updated[source] = value
        archiveBundleNames = updated
    }

    function isArchiveBundleEnabled(source) {
        return archiveBundleEnabled[source] !== false
    }

    function toggleArchiveBundle(source) {
        var enabled = !isArchiveBundleEnabled(source)
        var updated = cloneMap(archiveBundleEnabled || ({}))
        updated[source] = enabled
        archiveBundleEnabled = updated

        if (enabled && safeFolderName(archiveBundleNames[source] || "") === "") {
            var names = cloneMap(archiveBundleNames || ({}))
            names[source] = cleanedSourceName(source)
            archiveBundleNames = names
            bundleNamesRevision++
        }
    }

    function toggleSingleBundle() {
        if (singleBundleEnabled) {
            singleBundleEnabled = false
            return
        }

        singleBundleEnabled = true
        if (safeFolderName(bundleFolderName) === "") {
            var sources = distinctInstallSources()
            var fallback = sources.length > 0 ? cleanedSourceName(sources[0]) : ""
            bundleFolderName = fallback
            if (bundleField)
                bundleField.text = fallback
        }
    }

    function mappingBundleForGroup(group) {
        if (!group || (group.updateOf || "") === "")
            return bundleForGroup(group)

        const current = existingFolderFor(group.updateOf).replace(/\\/g, "/")
        const slash = current.lastIndexOf("/")
        if (slash <= 0)
            return ""
        return current.slice(0, slash).split("/")[0]
    }

    function suggestedBundleName() {
        if (!modGroups || modGroups.length < 2)
            return ""
        var sources = distinctInstallSources()
        if (sources.length !== 1)
            return ""
        var base = cleanedSourceName(sources[0])
        if (base.length < 2 || base.toLowerCase() === "download")
            return ""
        return base
    }

    function targetFolderPreview(group) {
        if (group && (group.updateOf || "") !== "") {
            var current = existingFolderFor(group.updateOf)
            if (current !== "")
                return current
        }
        var bundle = effectiveBundleForGroup(group)
        if (bundle === "")
            return ""
        var leaf = safeFolderName(group && (group.newName || group.originalName) || "")
        return bundle + "/" + (leaf !== "" ? leaf : "<mod>")
    }

    function existingFolderFor(modName) {
        if (!modName)
            return ""
        var mods = modManager.modList || []
        for (var i = 0; i < mods.length; ++i) {
            if (mods[i] && mods[i].name === modName
                    && (mods[i].type || "folder") === "folder")
                return mods[i].folderPath || ""
        }
        return ""
    }

    function effectiveBundleForGroup(group) {
        if (group && (group.updateOf || "") !== "") {
            var current = existingFolderFor(group.updateOf).replace(/\\/g, "/")
            if (current === "")
                return ""
            var slash = current.lastIndexOf("/")
            return slash < 0 ? "" : current.slice(0, slash)
        }
        return bundleForGroup(group)
    }

    function hasAnyUpdate() {
        for (var i = 0; i < modGroups.length; ++i) {
            if ((modGroups[i].updateOf || "") !== "")
                return true
        }
        return false
    }

    function allGroupsUpdate() {
        if (modGroups.length === 0)
            return false
        for (var i = 0; i < modGroups.length; ++i) {
            if ((modGroups[i].updateOf || "") === "")
                return false
        }
        return true
    }

    function existingBundleForSource(source) {
        for (var i = 0; i < modGroups.length; ++i) {
            if ((modGroups[i].archivePath || modGroups[i].folderPath || "") !== source)
                continue
            if ((modGroups[i].updateOf || "") === "")
                continue
            const current = existingFolderFor(modGroups[i].updateOf).replace(/\\/g, "/")
            const slash = current.lastIndexOf("/")
            if (slash > 0)
                return current.slice(0, slash).split("/")[0]
        }
        return ""
    }

    function groupRootNames(group) {
        var names = []
        var roots = group && group.installRoots ? group.installRoots : []
        for (var i = 0; i < roots.length; ++i) {
            var name = roots[i].rootName || ""
            if (name !== "" && names.indexOf(name) === -1)
                names.push(name)
        }
        return names
    }

    function extraGroupRootNames(group) {
        var activeName = pathLeaf(modManager.modsPath)
        var names = groupRootNames(group)
        if (activeName === "")
            return names
        var extra = []
        for (var i = 0; i < names.length; ++i) {
            if (names[i].toLowerCase() !== activeName.toLowerCase())
                extra.push(names[i])
        }
        return extra
    }

    function groupHasLuaRoot(group) {
        var roots = group && group.installRoots ? group.installRoots : []
        for (var i = 0; i < roots.length; ++i) {
            if (roots[i].rootKind === "lua")
                return true
        }
        return false
    }

    function setGroupLuaEnabled(index, enabled) {
        if (index < 0 || index >= modGroups.length)
            return
        modGroups[index].luaEnabled = enabled === true
    }

    function pathSummary(group) {
        var activePath = modManager.modsPath || ""
        var names = groupRootNames(group)
        var extra = extraGroupRootNames(group)
        var sharesActive = extra.length < names.length
        var targets = sharesActive ? extra : names
        var parts = []
        if (activePath !== "")
            parts.push("Your Mods folder: " + shortPath(activePath))
        if (targets.length > 0) {
            parts.push(sharesActive ? "This archive also installs to: " + targets.join(" + ")
                                    : "Will install to: " + targets.join(" + "))
        }
        return parts.join("  ·  ")
    }

    function shortPath(path) {
        var normalized = String(path).replace(/\\/g, "/")
        var lower = normalized.toLowerCase()
        var marker = lower.indexOf("/content/paks/")
        if (marker < 0 && lower.indexOf("content/paks/") === 0)
            marker = 0
        if (marker < 0 && lower.endsWith("/content/paks"))
            marker = lower.length - "/content/paks".length

        if (marker >= 0) {
            var prior = normalized.lastIndexOf("/", marker - 1)
            return normalized.slice(prior + 1)
        }

        var parts = normalized.split("/")
        var segments = []
        for (var i = 0; i < parts.length; ++i) {
            if (parts[i] !== "")
                segments.push(parts[i])
        }
        if (segments.length <= 2)
            return normalized
        return segments.slice(segments.length - 2).join("/")
    }

    function safeFolderName(name, fallback) {
        var invalid = "\\/:*?\"<>|"
        var cleaned = String(name || "")
        for (var i = 0; i < invalid.length; ++i)
            cleaned = cleaned.split(invalid.charAt(i)).join("")
        cleaned = cleaned.replace(/[. ]+$/g, "").trim()
        if (cleaned !== "")
            return cleaned

        var fallbackName = String(fallback || "")
        for (var j = 0; j < invalid.length; ++j)
            fallbackName = fallbackName.split(invalid.charAt(j)).join("")
        return fallbackName.replace(/[. ]+$/g, "").trim()
    }

    component StyledToolTip : ToolTip {
        id: tip
        delay: 400
        padding: 8

        contentItem: Text {
            text: tip.text
            font.pixelSize: 11
            font.weight: Font.Medium
            color: modManager.theme === "Cozy" ? "#2C2C2C" : "#F1F5F9"
        }

        background: Rectangle {
            radius: 8
            color: {
                if (modManager.theme === "Cozy") return "#FFF9F2"
                if (modManager.theme === "Glassmorphic") return Qt.rgba(0.12, 0.12, 0.16, 0.96)
                if (modManager.theme === "Wavey") return Qt.rgba(0.08, 0.16, 0.28, 0.96)
                return Qt.rgba(0.10, 0.10, 0.15, 0.96)
            }
            border.width: 1
            border.color: {
                if (modManager.theme === "Cozy") return "#D8C5B2"
                if (modManager.theme === "Glassmorphic") return Qt.rgba(1, 1, 1, 0.18)
                return Qt.rgba(1, 1, 1, 0.12)
            }

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.35)
                shadowBlur: 0.5
                shadowVerticalOffset: 3
            }
        }
    }

    component AppCheckBox : CheckBox {
        id: appCheck
        spacing: 6
        font.pixelSize: 11

        readonly property bool lightTheme: modManager.theme === "Cozy"

        contentItem: Text {
            text: appCheck.text
            font: appCheck.font
            color: appCheck.lightTheme ? "#2C2C2C" : "#E2E8F0"
            verticalAlignment: Text.AlignVCenter
            leftPadding: appCheck.indicator.width + appCheck.spacing
            elide: Text.ElideRight
        }

        indicator: Rectangle {
            implicitWidth: 20
            implicitHeight: 20
            x: appCheck.leftPadding
            y: appCheck.topPadding + (appCheck.availableHeight - height) / 2
            radius: 6
            color: appCheck.checked ? window.primaryColor : "transparent"
            border.width: 1
            border.color: appCheck.checked
                ? window.primaryColor
                : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.35)

            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }

            Canvas {
                anchors.centerIn: parent
                width: 14
                height: 14
                visible: appCheck.checked
                onVisibleChanged: if (visible) requestPaint()
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = "#FFFFFF"
                    ctx.lineWidth = 2.6
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(3.2, 7.2)
                    ctx.lineTo(6.0, 10.0)
                    ctx.lineTo(11.0, 4.0)
                    ctx.stroke()
                }
            }
        }
    }

    component LuaEnableSwitch : Item {
        id: uiverseToggle
        property bool checked: false
        signal toggled(bool checked)

        implicitWidth: 46
        implicitHeight: 24

        readonly property real _circleDiameter: 18
        readonly property real _offset: (height - _circleDiameter) / 2
        readonly property color _accent: window.primaryColor
        readonly property color _offBg: Qt.rgba(_accent.r, _accent.g, _accent.b, modManager.theme === "Cozy" ? 0.35 : 0.25)

        Rectangle {
            id: slider
            anchors.fill: parent
            radius: height / 2
            color: uiverseToggle.checked ? uiverseToggle._accent : uiverseToggle._offBg

            Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutBack } }

            Rectangle {
                width: uiverseToggle._circleDiameter / 2
                height: width / 2 - 1
                radius: 1
                color: "#ffffff"
                anchors.verticalCenter: parent.verticalCenter
                x: uiverseToggle.checked
                   ? (parent.width - uiverseToggle._offset - width - (width / 2))
                   : (uiverseToggle._offset + (width / 2))

                Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }
            }

            Rectangle {
                id: circle
                width: uiverseToggle._circleDiameter
                height: uiverseToggle._circleDiameter
                radius: width / 2
                color: "#ffffff"
                anchors.verticalCenter: parent.verticalCenter
                x: uiverseToggle.checked
                   ? (parent.width - uiverseToggle._offset - width)
                   : uiverseToggle._offset

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(0, 0, 0, 0.32)
                    shadowHorizontalOffset: 1
                    shadowVerticalOffset: 1
                    shadowBlur: 0.45
                }

                Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }

                Text {
                    anchors.centerIn: parent
                    text: "×"
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    color: Qt.rgba(uiverseToggle._accent.r, uiverseToggle._accent.g, uiverseToggle._accent.b, 1.0)
                    scale: uiverseToggle.checked ? 0 : 1
                    opacity: uiverseToggle.checked ? 0 : 1
                    Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: uiverseToggle.toggled(!uiverseToggle.checked)
            }
        }
    }

    component EditMenuItem : MenuItem {
        id: editItem
        property string iconName: ""
        property bool isDanger: false

        implicitWidth: 180
        implicitHeight: 32
        width: parent ? parent.width : implicitWidth
        height: 32

        contentItem: RowLayout {
            spacing: 12

            Image {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                source: editItem.iconName !== ""
                    ? "qrc:/UniversalModManager/Lucide Icons/" + editItem.iconName : ""
                sourceSize: Qt.size(16, 16)
                visible: editItem.iconName !== ""
                opacity: editItem.enabled ? 1.0 : 0.5

                layer.enabled: true
                layer.effect: ColorOverlay {
                    color: editItem.isDanger ? "#EF4444"
                        : ((modManager.theme === "Glassmorphic"
                            || modManager.theme === "Wavey") ? "#FFFFFF" : window.textColor)
                }
            }

            Text {
                Layout.fillWidth: true
                text: editItem.text
                font.pixelSize: 13
                font.weight: Font.Medium
                color: editItem.isDanger ? "#EF4444"
                    : ((modManager.theme === "Glassmorphic"
                        || modManager.theme === "Wavey") ? "#FFFFFF" : window.textColor)
                opacity: editItem.enabled ? 1.0 : 0.5
            }
        }

        background: Rectangle {
            color: editItem.highlighted
                ? (modManager.theme === "Glassmorphic"
                    ? Qt.rgba(1, 1, 1, 0.10)
                    : (modManager.theme === "Crimson"
                        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                                  window.primaryColor.b, 0.20)
                        : Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                                  window.primaryColor.b, 0.15)))
                : "transparent"
            radius: 8

            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    component EditContextMenu : Menu {
        id: editMenu
        required property var editor
        width: 180
        padding: 6

        background: Rectangle {
            color: {
                if (modManager.theme === "Crimson")
                    return Qt.rgba(0.05, 0.06, 0.10, 0.98)
                if (modManager.theme === "Glassmorphic")
                    return Qt.rgba(0.05, 0.05, 0.07,
                                   Math.max(0.88, Math.min(0.97, modManager.glassOpacity + 0.84)))
                if (modManager.theme === "Wavey")
                    return Qt.rgba(0.08, 0.16, 0.28, 0.97)
                return window.surfaceColor
            }
            radius: 12
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g,
                          modManager.accentColor.b, 0.10)
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

        EditMenuItem {
            text: "Undo"
            iconName: "undo-2.svg"
            enabled: editMenu.editor ? editMenu.editor.canUndo : false
            onTriggered: if (editMenu.editor) editMenu.editor.undo()
        }
        EditMenuItem {
            text: "Redo"
            iconName: "redo-2.svg"
            enabled: editMenu.editor ? editMenu.editor.canRedo : false
            onTriggered: if (editMenu.editor) editMenu.editor.redo()
        }
        MenuSeparator {}
        EditMenuItem {
            text: "Cut"
            iconName: "scissors.svg"
            enabled: editMenu.editor && !editMenu.editor.readOnly
                     && editMenu.editor.selectedText.length > 0
            onTriggered: if (editMenu.editor) editMenu.editor.cut()
        }
        EditMenuItem {
            text: "Copy"
            iconName: "copy.svg"
            enabled: editMenu.editor ? editMenu.editor.selectedText.length > 0 : false
            onTriggered: if (editMenu.editor) editMenu.editor.copy()
        }
        EditMenuItem {
            text: "Paste"
            iconName: "clipboard.svg"
            enabled: editMenu.editor ? !editMenu.editor.readOnly : false
            onTriggered: if (editMenu.editor) editMenu.editor.paste()
        }
        EditMenuItem {
            text: "Delete"
            iconName: "trash.svg"
            enabled: editMenu.editor && !editMenu.editor.readOnly
                     && editMenu.editor.selectedText.length > 0
            onTriggered: if (editMenu.editor) editMenu.editor.remove(editMenu.editor.selectionStart,
                                                                    editMenu.editor.selectionEnd)
        }
        MenuSeparator {}
        EditMenuItem {
            text: "Select All"
            iconName: "text-select.svg"
            enabled: editMenu.editor ? editMenu.editor.text.length > 0 : false
            onTriggered: if (editMenu.editor) editMenu.editor.selectAll()
        }
    }

    component AppFlatButton : Rectangle {
        id: appButton
        property string text: ""
        property bool primary: false
        signal clicked()

        implicitWidth: appButtonLabel.implicitWidth + 24
        implicitHeight: 28
        radius: 8
        color: appButtonMouse.containsMouse
            ? (modManager.theme === "Cozy"
                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.16)
                : Qt.rgba(1, 1, 1, 0.10))
            : "transparent"
        border.width: 1
        border.color: appButtonMouse.containsMouse
            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.45)
            : (modManager.theme === "Cozy" ? "#D8C5B2" : Qt.rgba(1, 1, 1, 0.14))

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }

        Text {
            id: appButtonLabel
            anchors.centerIn: parent
            text: appButton.text
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: appButton.primary
                ? window.primaryColor
                : (modManager.theme === "Cozy" ? "#5A5A5A" : "#CBD5E1")
        }

        MouseArea {
            id: appButtonMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: appButton.clicked()
        }
    }

    function generateSessionId() {
        var chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        var id = ""
        for (var i = 0; i < 8; i++) {
            id += chars.charAt(Math.floor(Math.random() * chars.length))
        }
        return id
    }

    property int groupSelectionRevision: 0
    signal groupSelectionReset(bool enabled)
    function toggleArchiveCollapsed(sourceKey) {
        var updated = cloneMap(collapsedArchives || ({}))
        updated[sourceKey] = !(updated[sourceKey] === true)
        collapsedArchives = updated
    }

    function archiveSourceKeys() {
        var keys = []
        var seen = ({})
        for (var i = 0; i < modGroups.length; ++i) {
            var source = modGroups[i].archivePath || modGroups[i].folderPath || ""
            var key = source !== "" ? source : "__other__"
            if (!seen[key]) {
                seen[key] = true
                keys.push(key)
            }
        }
        return keys
    }

    function allArchivesCollapsed() {
        var keys = archiveSourceKeys()
        if (keys.length === 0)
            return false
        for (var i = 0; i < keys.length; ++i) {
            if (collapsedArchives[keys[i]] !== true)
                return false
        }
        return true
    }

    function setAllArchivesCollapsed(collapsed) {
        var updated = ({})
        if (collapsed) {
            var keys = archiveSourceKeys()
            for (var i = 0; i < keys.length; ++i)
                updated[keys[i]] = true
        }
        collapsedArchives = updated
    }

    function setGroupNewName(index, name) {
        if (index < 0 || index >= modGroups.length)
            return
        var group = modGroups[index]
        if ((group.newName || "") === name)
            return

        var copy = {}
        for (var key in group)
            copy[key] = group[key]
        copy.newName = name
        var updated = modGroups.slice()
        updated[index] = copy
        modGroups = updated
    }

    readonly property bool canInstall: {
        var revision = groupSelectionRevision
        return !hasBlockingErrors && anyGroupEnabled()
    }

    function anyGroupEnabled() {
        for (var i = 0; i < modGroups.length; ++i) {
            if (modGroups[i].installEnabled !== false)
                return true
        }
        return false
    }

    function setAllGroupsEnabled(enabled) {
        for (var i = 0; i < modGroups.length; ++i)
            modGroups[i].installEnabled = enabled
        selectedGroupEnabled = enabled
        groupSelectionRevision++
        groupSelectionReset(enabled)
    }

    function buildRows() {
        var revision = listRevision
        if (!modGroups || modGroups.length === 0)
            return []
        var query = String(searchText || "").trim().toLowerCase()

        var result = []
        for (var i = 0; i < modGroups.length; ++i) {
            var group = modGroups[i]
            var original = group.originalName || ""
            var renamed = group.newName || ""
            if (query !== ""
                    && original.toLowerCase().indexOf(query) === -1
                    && renamed.toLowerCase().indexOf(query) === -1)
                continue
            result.push({
                isHeader: false,
                index: i,
                name: original,
                fileCount: group.files ? group.files.length : 0,
                updateOf: group.updateOf || ""
            })
        }
        if (query !== "")
            return result

        var sourceOrder = []
        var groupsBySource = {}
        for (var g = 0; g < modGroups.length; ++g) {
            var source = modGroups[g].archivePath || modGroups[g].folderPath || ""
            var key = source !== "" ? source : "__other__"
            if (!groupsBySource[key]) {
                groupsBySource[key] = []
                sourceOrder.push(key)
            }
            groupsBySource[key].push(g)
        }
        if (modGroups.length <= 1)
            return result

        var grouped = []
        for (var s = 0; s < sourceOrder.length; ++s) {
            var sourceKey = sourceOrder[s]
            var indices = groupsBySource[sourceKey]
            var collapsed = collapsedArchives[sourceKey] === true
            grouped.push({
                isHeader: true,
                sourceKey: sourceKey,
                name: sourceKey === "__other__" ? "Other mods"
                                                 : cleanedSourceName(sourceKey),
                count: indices.length,
                collapsed: collapsed,
                index: 0,
                fileCount: 0,
                updateOf: ""
            })
            if (collapsed)
                continue
            for (var n = 0; n < indices.length; ++n) {
                var rowGroup = modGroups[indices[n]]
                grouped.push({
                    isHeader: false,
                    sourceKey: sourceKey,
                    index: indices[n],
                    name: rowGroup.originalName || "",
                    fileCount: rowGroup.files ? rowGroup.files.length : 0,
                    updateOf: rowGroup.updateOf || ""
                })
            }
        }
        return grouped
    }

    function syncSelectedGroupState() {
        if (nameField) {
            syncingNameField = true
            var group = (selectedIndex >= 0 && selectedIndex < modGroups.length)
                ? modGroups[selectedIndex] : null
            nameField.text = group ? (group.newName || group.originalName || "") : ""
            syncingNameField = false
        }
        selectedGroupEnabled = selectedIndex >= 0
            && modGroups[selectedIndex].installEnabled !== false
        if (luaEnableSwitch) {
            luaEnableSwitch.checked = selectedIndex >= 0 && selectedIndex < modGroups.length
                && modGroups[selectedIndex].luaEnabled === true
        }
        if (modGroupColumn)
            modGroupColumn.installEnabled = selectedGroupEnabled
        if (modGroupColumn)
            modGroupColumn.extraFilesExpanded = false
        if (groupEditorFlick)
            groupEditorFlick.contentY = 0
    }

    onSelectedIndexChanged: syncSelectedGroupState()

    onOpened: {
        if (sessionId === "") {
            sessionId = generateSessionId()
        }

        for (var i = 0; i < modGroups.length; i++) {
            if (!modGroups[i].modId) {
                modGroups[i].modId = generateSessionId()
            }
            if (modGroups[i].installEnabled === undefined)
                modGroups[i].installEnabled = true
            if (modGroups[i].newName === undefined || modGroups[i].newName === "")
                modGroups[i].newName = modGroups[i].originalName || ""
        }
        searchText = ""
        collapsedArchives = ({})
        if (searchField)
            searchField.text = ""
        var sources = distinctInstallSources()
        if (sources.length > 1) {
            var names = {}
            var enabled = {}
            for (var i = 0; i < sources.length; ++i) {
                var sourceName = existingBundleForSource(sources[i])
                    || cleanedSourceName(sources[i])
                names[sources[i]] = sourceName
                enabled[sources[i]] = sourceName !== ""
            }
            archiveBundleNames = names
            archiveBundleEnabled = enabled
            singleBundleEnabled = false
            bundleFolderName = ""
            if (bundleField)
                bundleField.text = ""
        } else {
            archiveBundleNames = ({})
            archiveBundleEnabled = ({})
            bundleFolderName = suggestedBundleName()
            singleBundleEnabled = safeFolderName(bundleFolderName) !== ""
            if (bundleField)
                bundleField.text = bundleFolderName
        }
        bundleNamesRevision++
        selectedIndex = modGroups.length > 0 ? 0 : -1
        listRevision++
        syncSelectedGroupState()
    }

    function collectMappings() {
        var mappings = []
        for (var i = 0; i < modGroups.length; i++) {
            var fileSelections = []

            if (modGroups[i].files) {
                for (var j = 0; j < modGroups[i].files.length; j++) {
                    fileSelections.push({
                        "name": modGroups[i].files[j].name,
                        "selected": modGroups[i].files[j].selected !== false
                    })
                }
            }
            mappings.push({
                "originalName": modGroups[i].originalName,
                "newName": modGroups[i].newName,
                "modId": modGroups[i].modId,
                "archivePath": modGroups[i].archivePath || "",
                "sourceFolder": modGroups[i].folderPath || "",
                "updateOf": modGroups[i].updateOf || "",
                "install": modGroups[i].installEnabled !== false,
                "luaEnabled": modGroups[i].luaEnabled === true,
                "bundleFolder": mappingBundleForGroup(modGroups[i]),
                "files": fileSelections
            })
        }
        return mappings
    }

    function getModName(index) {
        if (index >= 0 && index < modGroups.length) {
            return modGroups[index].originalName
        }
        return ""
    }

    function getExtraFiles(index) {
        if (index >= 0 && index < modGroups.length && modGroups[index].extraFiles) {
            return modGroups[index].extraFiles
        }
        return []
    }

    Shortcut {
        sequence: "Return"
        enabled: renameDialog.visible
        onActivated: {
            if (renameDialog.canInstall)
                renameDialog.accept()
        }
    }
    Shortcut {
        sequence: "Enter"
        enabled: renameDialog.visible
        onActivated: {
            if (renameDialog.canInstall)
                renameDialog.accept()
        }
    }
    Shortcut {
        sequence: "Esc"
        enabled: renameDialog.visible
        onActivated: renameDialog.reject()
    }

    background: Rectangle {
        color: {
            if (modManager.theme === "Wavey") return Qt.rgba(0.08, 0.12, 0.20, 0.94)
            if (modManager.theme === "Glassmorphic") return Qt.rgba(0.08, 0.08, 0.12, 0.55)
            return "transparent"
        }
        radius: 20
        border.width: 1
        border.color: {
            if (modManager.theme === "Glassmorphic") return Qt.rgba(1, 1, 1, 0.18)
            if (modManager.theme === "Wavey") return Qt.rgba(0.38, 0.60, 0.95, 0.35)
            if (modManager.theme === "Crimson") return window.borderColor
            return window.borderColor || "#D8C3AE"
        }

        gradient: {
            if (modManager.theme === "Cozy") return cozyDialogGradient
            if (modManager.theme === "Crimson") return crimsonDialogGradient
            if (modManager.theme === "Wavey") return waveyDialogGradient
            return null
        }

        Gradient {
            id: cozyDialogGradient
            GradientStop { position: 0.0; color: window.cozyDialogGradientTop }
            GradientStop { position: 1.0; color: window.cozyDialogGradientBottom }
        }

        Gradient {
            id: crimsonDialogGradient
            GradientStop { position: 0.0; color: window.crimsonDialogGradientTop }
            GradientStop { position: 1.0; color: window.crimsonDialogGradientBottom }
        }

        Gradient {
            id: waveyDialogGradient
            GradientStop { position: 0.0; color: window.waveyDialogGradientTop }
            GradientStop { position: 1.0; color: window.waveyDialogGradientBottom }
        }

        Rectangle {
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 48
            height: 2
            radius: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.2; color: renameDialog.installAccent }
                GradientStop { position: 0.8; color: renameDialog.installAccent }
                GradientStop { position: 1.0; color: "transparent" }
            }
            opacity: 0.85
        }

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.55)
            shadowVerticalOffset: 16
            shadowBlur: modManager.theme === "Glassmorphic" ? 36 : 40
            blur: modManager.theme === "Glassmorphic" ? 0.25 : 0.0
            blurEnabled: modManager.theme === "Glassmorphic"
            blurMax: 32
        }
    }

    Overlay.modal: Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.75)
        Behavior on opacity { NumberAnimation { duration: 250 } }
    }

    enter: Transition {
        NumberAnimation { property: "scale"; from: 0.94; to: 1.0; duration: 280; easing.type: Easing.OutCubic }
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 240 }
    }

    exit: Transition {
        NumberAnimation { property: "scale"; from: 1.0; to: 0.95; duration: 180; easing.type: Easing.InCubic }
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 180 }
    }

    contentItem: ColumnLayout {
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Item {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44

                Image {
                    anchors.centerIn: parent
                    source: "qrc:/UniversalModManager/Lucide Icons/download.svg"
                    sourceSize: Qt.size(26, 26)
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: renameDialog.installAccent
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                RowLayout {
                    spacing: 8

                    Text {
                        text: "Mod Installation"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        color: modManager.theme === "Cozy" ? "#2C2C2C" : "#FFFFFF"
                        font.letterSpacing: -0.2
                    }

                    Rectangle {
                        visible: renameDialog.modGroups.length > 1
                        Layout.preferredHeight: 20
                        Layout.preferredWidth: modCountText.implicitWidth + 14
                        radius: 10
                        color: modManager.theme === "Cozy" ? Qt.rgba(0, 0, 0, 0.06) : Qt.rgba(1, 1, 1, 0.10)

                        Text {
                            id: modCountText
                            anchors.centerIn: parent
                            text: renameDialog.modGroups.length + " Mods"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: modManager.theme === "Cozy" ? "#6B6B6B" : "#94a3b8"
                        }
                    }
                }

                Text {
                    text: "Review destinations and display names before installing"
                    font.pixelSize: 12
                    color: modManager.theme === "Cozy" ? "#7A7A7A" : "#94a3b8"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: modManager.theme === "Cozy" ? "#EAE0D5" : Qt.rgba(1, 1, 1, 0.08)
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 16
                color: "transparent"

                Rectangle {
                    anchors.centerIn: parent
                    width: 2
                    height: parent.height - 24
                    radius: 1
                    color: SplitHandle.pressed || SplitHandle.hovered
                        ? window.primaryColor
                        : (modManager.theme === "Cozy" ? "#E6D8CA" : Qt.rgba(1, 1, 1, 0.10))
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
            }

            Rectangle {
                SplitView.preferredWidth: Math.max(270, Math.min(420, renameDialog.width * 0.37))
                SplitView.minimumWidth: 270
                radius: 12
                color: {
                    if (modManager.theme === "Cozy") return Qt.rgba(1, 1, 1, 0.65)
                    if (modManager.theme === "Glassmorphic") return Qt.rgba(1, 1, 1, 0.04)
                    return Qt.rgba(1, 1, 1, 0.03)
                }
                border.width: 1
                border.color: {
                    if (modManager.theme === "Cozy") return "#E6D8CA"
                    if (modManager.theme === "Glassmorphic") return Qt.rgba(1, 1, 1, 0.08)
                    return Qt.rgba(1, 1, 1, 0.06)
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Rectangle {
                        Layout.fillWidth: true

                        Layout.preferredHeight: Math.min(bundleContent.implicitHeight + 20, 300)
                        radius: 10
                        visible: renameDialog.modGroups.length > 1
                            && !renameDialog.allGroupsUpdate()
                        color: modManager.theme === "Cozy"
                            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                                      window.primaryColor.b, 0.06)
                            : Qt.rgba(1, 1, 1, 0.04)
                        border.width: 1
                        border.color: modManager.theme === "Cozy"
                            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                                      window.primaryColor.b, 0.22)
                            : Qt.rgba(1, 1, 1, 0.08)

                        ScrollView {
                            id: bundleScroll
                            anchors.fill: parent
                            anchors.margins: 10
                            clip: true
                            contentWidth: availableWidth
                            rightPadding: 10

                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical: ScrollBar {
                                id: bundleScrollBar
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.rightMargin: 2
                                width: 6
                                policy: ScrollBar.AsNeeded

                                contentItem: Rectangle {
                                    implicitWidth: 6
                                    implicitHeight: 40
                                    radius: 3
                                    color: bundleScrollBar.pressed
                                        ? window.primaryColor
                                        : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.25)
                                }
                            }

                            WheelHandler {
                                target: bundleScroll.contentItem
                                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                onWheel: (event) => {
                                    var flick = bundleScroll.contentItem
                                    if (!flick)
                                        return
                                    event.accepted = true
                                    var dy = 0
                                    if (event.pixelDelta && event.pixelDelta.y !== 0)
                                        dy = event.pixelDelta.y
                                    else
                                        dy = event.angleDelta.y / 120 * 140
                                    var maxY = Math.max(0, flick.contentHeight - flick.height)
                                    var targetY = Math.max(0, Math.min(maxY, flick.contentY - dy))
                                    if (flick.cancelFlick)
                                        flick.cancelFlick()
                                    bundleScrollAnim.stop()
                                    bundleScrollAnim.to = targetY
                                    bundleScrollAnim.duration = Math.max(120, Math.min(400, Math.abs(targetY - flick.contentY) * 0.35))
                                    bundleScrollAnim.start()
                                }
                            }

                            NumberAnimation {
                                id: bundleScrollAnim
                                target: bundleScroll.contentItem
                                property: "contentY"
                                easing.type: Easing.OutCubic
                            }

                        ColumnLayout {
                            id: bundleContent
                            width: bundleScroll.availableWidth
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Image {
                                    Layout.preferredWidth: 14
                                    Layout.preferredHeight: 14
                                    source: "qrc:/UniversalModManager/Lucide Icons/folder-tree.svg"
                                    sourceSize: Qt.size(14, 14)
                                    layer.enabled: true
                                    layer.effect: ColorOverlay {
                                        color: modManager.theme === "Cozy" ? "#8A7A6A" : "#8291A8"
                                    }
                                }

                                Text {
                                    text: "Install as bundle"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: window.textColor
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pixelSize: 10
                                lineHeight: 1.15
                                color: modManager.theme === "Cozy" ? "#8A7A6A" : "#94a3b8"
                                text: renameDialog.distinctInstallSources().length > 1
                                    ? "Each archive gets its own bundle folder. Uncheck an archive to install its mods directly in the mods folder."
                                    : "Installs these mods together under one folder. Uncheck to install each mod directly in the mods folder."
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                visible: renameDialog.distinctInstallSources().length <= 1
                                spacing: 8

                                AppCheckBox {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: Math.min(implicitWidth, 160)
                                    Layout.alignment: Qt.AlignVCenter
                                    text: "Bundle these mods"
                                    checked: renameDialog.singleBundleEnabled
                                    onToggled: renameDialog.toggleSingleBundle()
                                }

                                TextField {
                                    id: bundleField
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 300
                                    Layout.minimumWidth: 100
                                    Layout.preferredHeight: 30
                                    enabled: renameDialog.singleBundleEnabled
                                    opacity: enabled ? 1 : 0.45
                                    placeholderText: "Bundle name (optional)"
                                    font.pixelSize: 12
                                    color: modManager.theme === "Cozy" ? "#1F1F1F" : "#FFFFFF"
                                    placeholderTextColor: modManager.theme === "Cozy" ? "#B0A090" : "#64748b"
                                    selectByMouse: true
                                    leftPadding: 8
                                    rightPadding: 8
                                    verticalAlignment: TextInput.AlignVCenter
                                    onTextChanged: {
                                        if (renameDialog.bundleFolderName !== text)
                                            renameDialog.bundleFolderName = text
                                    }
                                    ContextMenu.menu: EditContextMenu { editor: bundleField }

                                    background: Rectangle {
                                        radius: 8
                                        color: modManager.theme === "Cozy" ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.28)
                                        border.width: 1
                                        border.color: bundleField.activeFocus
                                            ? (modManager.theme === "Cozy" ? window.primaryColor : renameDialog.installAccent)
                                            : (modManager.theme === "Cozy" ? "#DECFC0" : Qt.rgba(1, 1, 1, 0.12))
                                    }
                                }
                            }

                            Repeater {
                                model: renameDialog.distinctInstallSources().length > 1
                                    ? renameDialog.distinctInstallSources() : []

                                delegate: RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    AppCheckBox {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: Math.min(implicitWidth, 130)
                                        Layout.alignment: Qt.AlignVCenter
                                        text: renameDialog.cleanedSourceName(modelData)
                                        checked: renameDialog.isArchiveBundleEnabled(modelData)
                                        onToggled: renameDialog.toggleArchiveBundle(modelData)
                                    }

                                    TextField {
                                        id: bundledArchiveField
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 300
                                        Layout.minimumWidth: 100
                                        Layout.preferredHeight: 30
                                        enabled: renameDialog.isArchiveBundleEnabled(modelData)
                                        opacity: enabled ? 1 : 0.45
                                        placeholderText: "Bundle name (optional)"
                                        font.pixelSize: 11
                                        color: modManager.theme === "Cozy" ? "#1F1F1F" : "#FFFFFF"
                                        placeholderTextColor: modManager.theme === "Cozy" ? "#B0A090" : "#64748b"
                                        selectByMouse: true
                                        leftPadding: 8
                                        rightPadding: 8
                                        verticalAlignment: TextInput.AlignVCenter
                                        onTextChanged: renameDialog.setArchiveBundleName(modelData, text)
                                        Component.onCompleted: text = renameDialog.archiveBundleNames[modelData] || ""
                                        ContextMenu.menu: EditContextMenu { editor: bundledArchiveField }

                                        Connections {
                                            target: renameDialog
                                            function onBundleNamesRevisionChanged() {
                                                bundledArchiveField.text =
                                                    renameDialog.archiveBundleNames[modelData] || ""
                                            }
                                        }

                                        background: Rectangle {
                                            radius: 8
                                            color: modManager.theme === "Cozy" ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.28)
                                            border.width: 1
                                            border.color: bundledArchiveField.activeFocus
                                                ? (modManager.theme === "Cozy" ? window.primaryColor : window.accentColor)
                                                : (modManager.theme === "Cozy" ? "#DECFC0" : Qt.rgba(1, 1, 1, 0.12))
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 5
                                visible: renameDialog.effectiveBundleForGroup(renameDialog.currentGroup) !== ""

                                Image {
                                    Layout.preferredWidth: 11
                                    Layout.preferredHeight: 11
                                    source: "qrc:/UniversalModManager/Lucide Icons/folder-open.svg"
                                    sourceSize: Qt.size(22, 22)
                                    layer.enabled: true
                                    layer.effect: ColorOverlay {
                                        color: modManager.theme === "Cozy" ? "#8A7A6A" : "#8291A8"
                                    }
                                }

                                Text {
                                    text: "Installs to"
                                    font.pixelSize: 9
                                    font.weight: Font.Bold
                                    color: modManager.theme === "Cozy" ? "#8A7A6A" : "#8291A8"
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: renameDialog.targetFolderPreview(renameDialog.currentGroup)
                                    font.family: "JetBrains Mono"
                                    font.pixelSize: 10
                                    color: modManager.theme === "Cozy" ? "#3E3E3E" : "#cbd5e1"
                                    elide: Text.ElideMiddle
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: renameDialog.effectiveBundleForGroup(renameDialog.currentGroup) !== ""
                                    && renameDialog.hasAnyUpdate()
                                text: "Existing mods update in their current folder; new mods go under the bundle name."
                                font.pixelSize: 9
                                wrapMode: Text.WordWrap
                                color: modManager.theme === "Cozy" ? "#A89886" : "#64748b"
                            }
                        }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            placeholderText: "Search mods..."
                            font.pixelSize: 12
                            color: modManager.theme === "Cozy" ? "#1F1F1F" : "#FFFFFF"
                            placeholderTextColor: modManager.theme === "Cozy" ? "#B0A090" : "#64748b"
                            selectByMouse: true
                            ContextMenu.menu: EditContextMenu { editor: searchField }

                            onTextChanged: {
                                renameDialog.searchText = text
                                renameDialog.listRevision++
                                var candidates = renameDialog.rows
                                if (candidates.length === 0)
                                    return
                                var found = false
                                var firstModIndex = -1
                                for (var i = 0; i < candidates.length; ++i) {
                                    if (candidates[i].isHeader)
                                        continue
                                    if (firstModIndex < 0)
                                        firstModIndex = candidates[i].index
                                    if (candidates[i].index === renameDialog.selectedIndex) {
                                        found = true
                                        break
                                    }
                                }
                                if (!found && firstModIndex >= 0)
                                    renameDialog.selectedIndex = firstModIndex
                            }

                            background: Rectangle {
                                radius: 8
                                color: modManager.theme === "Cozy" ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.28)
                                border.width: 1
                                border.color: searchField.activeFocus
                                    ? (modManager.theme === "Cozy" ? window.primaryColor : renameDialog.installAccent)
                                    : (modManager.theme === "Cozy" ? "#DECFC0" : Qt.rgba(1, 1, 1, 0.12))
                            }
                        }

                        Rectangle {
                            id: collapseAllButton
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            Layout.alignment: Qt.AlignVCenter
                            visible: renameDialog.modGroups.length > 1 && renameDialog.searchText === ""
                            radius: 8
                            color: collapseAllMouse.containsMouse
                                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.16)
                                : (modManager.theme === "Cozy" ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.28))
                            border.width: 1
                            border.color: collapseAllMouse.containsMouse
                                ? window.primaryColor
                                : (modManager.theme === "Cozy" ? "#DECFC0" : Qt.rgba(1, 1, 1, 0.12))

                            property bool allCollapsed: {
                                var c = renameDialog.collapsedArchives
                                return renameDialog.allArchivesCollapsed()
                            }

                            Image {
                                id: collapseAllIcon
                                anchors.centerIn: parent
                                width: 14
                                height: 14
                                source: collapseAllButton.allCollapsed
                                    ? "qrc:/UniversalModManager/Lucide Icons/chevrons-up.svg"
                                    : "qrc:/UniversalModManager/Lucide Icons/chevrons-down.svg"
                                sourceSize: Qt.size(28, 28)
                                visible: false
                            }

                            ColorOverlay {
                                anchors.fill: collapseAllIcon
                                source: collapseAllIcon
                                color: collapseAllMouse.containsMouse ? window.primaryColor : window.textSecondaryColor
                            }

                            MouseArea {
                                id: collapseAllMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: renameDialog.setAllArchivesCollapsed(!collapseAllButton.allCollapsed)
                            }

                            ToolTip {
                                id: collapseAllToolTip
                                visible: collapseAllMouse.containsMouse
                                text: collapseAllButton.allCollapsed ? "Expand all archives" : "Collapse all archives"
                                delay: 400
                                padding: 8
                                contentItem: Text {
                                    text: collapseAllToolTip.text
                                    color: window.textColor
                                    font.pixelSize: 11
                                }
                                background: Rectangle {
                                    radius: 8
                                    color: window.surfaceColor
                                    border.color: window.borderColor
                                }
                            }
                        }
                    }

                    ListView {
                        id: groupList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 4
                        model: renameDialog.rows
                        ScrollBar.vertical: ScrollBar {
                            id: groupListScrollBar
                            width: 6
                            policy: ScrollBar.AsNeeded

                            contentItem: Rectangle {
                                implicitWidth: 6
                                implicitHeight: 40
                                radius: 3
                                color: groupListScrollBar.pressed
                                    ? window.primaryColor
                                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.25)
                            }
                        }

                        property real pendingScrollY: -1
                        onContentHeightChanged: {
                            if (pendingScrollY >= 0) {
                                var restored = Math.min(pendingScrollY, Math.max(0, contentHeight - height))
                                contentY = restored
                                scrollTarget = restored
                                pendingScrollY = -1
                            }
                        }

                        property real scrollTarget: 0
                        onContentYChanged: {
                            if (!groupListScrollAnim.running)
                                scrollTarget = contentY
                        }
                        onMovementStarted: groupListScrollAnim.stop()

                        WheelHandler {
                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                            onWheel: (event) => {
                                event.accepted = true
                                var dy = 0
                                if (event.pixelDelta && event.pixelDelta.y !== 0)
                                    dy = event.pixelDelta.y
                                else
                                    dy = event.angleDelta.y / 120 * 140
                                var maxY = Math.max(0, groupList.contentHeight - groupList.height)
                                var targetY = Math.max(0, Math.min(maxY, groupList.scrollTarget - dy))
                                if (targetY === groupList.scrollTarget)
                                    return
                                groupList.scrollTarget = targetY
                                groupListScrollAnim.stop()
                                groupListScrollAnim.to = targetY
                                groupListScrollAnim.duration = Math.max(120, Math.min(400, Math.abs(targetY - groupList.contentY) * 0.35))
                                groupListScrollAnim.start()
                            }
                        }

                        NumberAnimation {
                            id: groupListScrollAnim
                            target: groupList
                            property: "contentY"
                            easing.type: Easing.OutCubic
                        }

                        delegate: Rectangle {
                            id: groupRow
                            width: ListView.view ? ListView.view.width : 0
                            height: modelData.isHeader ? 34 : 56
                            radius: 8
                            color: !modelData.isHeader
                                   && modelData.index === renameDialog.selectedIndex
                                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.16)
                                : (!modelData.isHeader && rowHover.hovered
                                   ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08)
                                   : "transparent")
                            border.width: !modelData.isHeader
                                          && modelData.index === renameDialog.selectedIndex ? 1 : 0
                            border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.45)

                            HoverHandler {
                                id: rowHover
                                cursorShape: modelData.isHeader
                                    ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (modelData.isHeader) {
                                        groupList.pendingScrollY = groupList.contentY
                                        renameDialog.toggleArchiveCollapsed(modelData.sourceKey)
                                    } else {
                                        renameDialog.selectedIndex = modelData.index
                                    }
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 6
                                visible: modelData.isHeader

                                Image {
                                    Layout.preferredWidth: 13
                                    Layout.preferredHeight: 13
                                    source: modelData.collapsed
                                        ? "qrc:/UniversalModManager/Lucide Icons/chevron-right.svg"
                                        : "qrc:/UniversalModManager/Lucide Icons/chevron-down.svg"
                                    sourceSize: Qt.size(26, 26)
                                    layer.enabled: true
                                    layer.effect: ColorOverlay {
                                        color: modManager.theme === "Cozy" ? "#8A7A6A" : "#94a3b8"
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: modelData.name
                                    font.pixelSize: 11
                                    font.weight: Font.Bold
                                    color: window.textColor
                                    elide: Text.ElideRight
                                }

                                Text {
                                    text: modelData.count + (modelData.count === 1 ? " mod" : " mods")
                                    font.pixelSize: 10
                                    color: modManager.theme === "Cozy" ? "#7A6A58" : "#94a3b8"
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8
                                z: 1
                                visible: !modelData.isHeader

                                AppCheckBox {
                                    id: rowInstallCheck
                                    Layout.alignment: Qt.AlignVCenter
                                    checked: renameDialog.modGroups[modelData.index].installEnabled !== false
                                    onToggled: {
                                        renameDialog.modGroups[modelData.index].installEnabled = checked
                                        if (modelData.index === renameDialog.selectedIndex)
                                            renameDialog.selectedGroupEnabled = checked
                                        renameDialog.groupSelectionRevision++
                                    }

                                    Connections {
                                        target: renameDialog
                                        function onGroupSelectionReset(enabled) {
                                            rowInstallCheck.checked = enabled
                                        }
                                        function onGroupSelectionRevisionChanged() {
                                            rowInstallCheck.checked =
                                                renameDialog.modGroups[modelData.index].installEnabled !== false
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 1

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        color: modManager.theme === "Cozy" ? "#2C2C2C" : "#F1F5F9"
                                        elide: Text.ElideRight
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        Text {
                                            text: modelData.fileCount + " core files"
                                            font.pixelSize: 11
                                            font.weight: Font.Medium
                                            color: modManager.theme === "Cozy" ? "#7A6A58" : "#B8C4D6"
                                            elide: Text.ElideRight
                                        }

                                        Rectangle {
                                            visible: modelData.updateOf !== ""
                                            Layout.alignment: Qt.AlignVCenter
                                            Layout.preferredHeight: 18
                                            Layout.preferredWidth: updateBadgeLabel.implicitWidth + 14
                                            radius: 9
                                            color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.16)
                                            border.width: 1
                                            border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.35)

                                            Text {
                                                id: updateBadgeLabel
                                                anchors.centerIn: parent
                                                text: "Update"
                                                font.pixelSize: 9
                                                font.weight: Font.Bold
                                                color: renameDialog.installAccent
                                            }
                                        }

                                        Item { Layout.fillWidth: true }
                                    }
                                }

                                Rectangle {
                                    Layout.alignment: Qt.AlignVCenter
                                    visible: renameDialog.modGroups[modelData.index].installValid === false
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: "#EF4444"
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: groupList.count === 0
                        text: "No mods match your search."
                        wrapMode: Text.Wrap
                        font.pixelSize: 11
                        color: modManager.theme === "Cozy" ? "#8A8A8A" : "#64748b"
                    }
                }
            }

            Rectangle {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 360
                radius: 14
                color: renameDialog.selectedGroupEnabled
                    ? (modManager.theme === "Cozy"
                        ? Qt.rgba(1, 1, 1, 0.82)
                        : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.14))
                    : (modManager.theme === "Cozy"
                        ? Qt.rgba(1, 1, 1, 0.55)
                        : (modManager.theme === "Glassmorphic"
                            ? Qt.rgba(1, 1, 1, 0.025)
                            : Qt.rgba(1, 1, 1, 0.03)))
                border.width: 1
                border.color: renameDialog.selectedGroupEnabled
                    ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.55)
                    : (modManager.theme === "Cozy"
                        ? "#E6D8CA"
                        : (modManager.theme === "Glassmorphic"
                            ? Qt.rgba(1, 1, 1, 0.08)
                            : Qt.rgba(1, 1, 1, 0.06)))

                opacity: renameDialog.selectedGroupEnabled ? 1.0 : 0.65

                Behavior on color { ColorAnimation { duration: 160 } }
                Behavior on border.color { ColorAnimation { duration: 160 } }
                Behavior on opacity { NumberAnimation { duration: 160 } }

                Flickable {
                    id: groupEditorFlick
                    anchors.fill: parent
                    anchors.margins: 14
                    clip: true
                    contentWidth: width
                    contentHeight: modGroupColumn.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    ColumnLayout {
                            id: modGroupColumn
                            width: groupEditorFlick.width
                            property int index: renameDialog.selectedIndex
                            spacing: 14

                            property var extraFiles: renameDialog.getExtraFiles(index)
                            property bool extraFilesExpanded: false
                            property var installRoots: renameDialog.currentGroup.installRoots || []
                            property bool hasRootPlan: installRoots.length > 0
                            property string installError: renameDialog.currentGroup.installError || ""
                            property var installWarnings: renameDialog.currentGroup.warnings || []
                            readonly property string groupOriginalName: renameDialog.currentGroup.originalName || ""
                            property bool installEnabled: renameDialog.currentGroup.installEnabled !== false
                            readonly property string installFolderName: renameDialog.safeFolderName(
                                nameField.text, renameDialog.currentGroup.originalName)

                            function warningsText() {
                                var parts = []
                                for (var i = 0; i < installWarnings.length; ++i)
                                    parts.push("• " + installWarnings[i])
                                return parts.join("\n")
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                visible: modGroupColumn.hasRootPlan
                                         && (modGroupColumn.installRoots.length > 1
                                             || renameDialog.extraGroupRootNames(renameDialog.currentGroup).length > 0)
                                implicitHeight: pathNoticeColumn.implicitHeight + 20
                                radius: 9
                                color: {
                                    if (modManager.theme === "Cozy") return Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.10)
                                    return Qt.rgba(renameDialog.installAccent.r, renameDialog.installAccent.g, renameDialog.installAccent.b, 0.14)
                                }
                                border.width: 1
                                border.color: {
                                    if (modManager.theme === "Cozy") return Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.30)
                                    return Qt.rgba(renameDialog.installAccent.r, renameDialog.installAccent.g, renameDialog.installAccent.b, 0.40)
                                }

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 8
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.topMargin: 10
                                    anchors.bottomMargin: 10
                                    width: 3
                                    radius: 1.5
                                    color: {
                                        if (modManager.theme === "Cozy") return window.primaryColor
                                        return renameDialog.installAccent
                                    }
                                }

                                ColumnLayout {
                                    id: pathNoticeColumn
                                    anchors.fill: parent
                                    anchors.leftMargin: 20
                                    anchors.rightMargin: 12
                                    anchors.topMargin: 10
                                    anchors.bottomMargin: 10
                                    spacing: 3

                                    Text {
                                        Layout.fillWidth: true
                                        visible: modGroupColumn.installRoots.length > 1
                                        text: "Universal Manager detected archive has multiple mods folders"
                                        wrapMode: Text.WordWrap
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: modManager.theme === "Cozy" ? "#3E3026" : "#E2E8F0"
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        visible: renameDialog.extraGroupRootNames(renameDialog.currentGroup).length > 0
                                        text: renameDialog.pathSummary(renameDialog.currentGroup)
                                        wrapMode: Text.WordWrap
                                        font.pixelSize: 11
                                        lineHeight: 1.2
                                        color: modManager.theme === "Cozy" ? "#6B5A48" : "#C7D2E4"
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 2

                                    Text {
                                        text: "ORIGINAL ARCHIVE NAME"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                        font.letterSpacing: 0.5
                                        color: modManager.theme === "Cozy" ? "#9E8B79" : "#64748b"
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: renameDialog.currentGroup.originalName || ""
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: modManager.theme === "Cozy" ? "#2C2C2C" : "#F1F5F9"
                                        wrapMode: Text.WrapAnywhere
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                    }
                                }

                                Rectangle {
                                    id: modIdBadge
                                    Layout.preferredHeight: 28
                                    Layout.preferredWidth: idRow.implicitWidth + 18
                                    radius: 7
                                    color: idMouseArea.containsMouse
                                        ? Qt.rgba(renameDialog.installAccent.r, renameDialog.installAccent.g, renameDialog.installAccent.b, 0.24)
                                        : Qt.rgba(renameDialog.installAccent.r, renameDialog.installAccent.g, renameDialog.installAccent.b, 0.12)
                                    border.width: 1
                                    border.color: idMouseArea.containsMouse
                                        ? renameDialog.installAccent
                                        : Qt.rgba(renameDialog.installAccent.r, renameDialog.installAccent.g, renameDialog.installAccent.b, 0.35)

                                    Behavior on color { ColorAnimation { duration: 150 } }
                                    Behavior on border.color { ColorAnimation { duration: 150 } }

                                    RowLayout {
                                        id: idRow
                                        anchors.centerIn: parent
                                        spacing: 6

                                        Image {
                                            Layout.preferredWidth: 12
                                            Layout.preferredHeight: 12
                                            source: "qrc:/UniversalModManager/Lucide Icons/copy.svg"
                                            sourceSize: Qt.size(12, 12)
                                            layer.enabled: true
                                            layer.effect: ColorOverlay {
                                                color: renameDialog.installAccent
                                            }
                                        }

                                        Text {
                                            text: "ID #" + (renameDialog.currentGroup.modId || "")
                                            font.family: "JetBrains Mono"
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                            color: renameDialog.installAccent
                                        }
                                    }

                                    MouseArea {
                                        id: idMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var idVal = renameDialog.currentGroup.modId
                                            if (idVal) {
                                                try {
                                                    var tempInput = Qt.createQmlObject('import QtQuick; TextInput { }', renameDialog)
                                                    tempInput.text = idVal
                                                    tempInput.selectAll()
                                                    tempInput.copy()
                                                    tempInput.destroy()
                                                    idCopiedTooltip.text = "Copied: #" + idVal
                                                    idCopiedTooltip.open()
                                                } catch (e) {}
                                            }
                                        }
                                    }

                                    StyledToolTip {
                                        id: idCopiedTooltip
                                        visible: idMouseArea.containsMouse
                                        text: "Click to copy ID"
                                        delay: 300
                                    }
                                }

                                RowLayout {
                                    visible: renameDialog.modGroups.length > 1
                                    spacing: 4

                                    AppFlatButton {
                                        text: "Select All"
                                        primary: true
                                        onClicked: renameDialog.setAllGroupsEnabled(true)
                                    }

                                    AppFlatButton {
                                        text: "Clear"
                                        onClicked: renameDialog.setAllGroupsEnabled(false)
                                    }
                                }
                            }

                            Rectangle {
                                id: nameFieldContainer
                                Layout.fillWidth: true
                                implicitHeight: 74
                                radius: 12
                                color: modManager.theme === "Cozy" ? "#FFFFFF" : Qt.rgba(0, 0, 0, 0.35)
                                border.width: nameField.activeFocus ? 1.5 : 1
                                border.color: {
                                    if (nameField.activeFocus)
                                        return renameDialog.installAccent
                                    if (modManager.theme === "Cozy") return "#DECFC0"
                                    if (modManager.theme === "Glassmorphic") return Qt.rgba(1, 1, 1, 0.12)
                                    return Qt.rgba(1, 1, 1, 0.08)
                                }

                                Behavior on border.color { ColorAnimation { duration: 200 } }

                                layer.enabled: nameField.activeFocus
                                layer.effect: MultiEffect {
                                    shadowEnabled: true
                                    shadowColor: {
                                        if (modManager.theme === "Cozy") return Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.25)
                                        return Qt.rgba(renameDialog.installAccent.r, renameDialog.installAccent.g, renameDialog.installAccent.b, 0.35)
                                    }
                                    shadowBlur: 14
                                }

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 14
                                    anchors.topMargin: 10
                                    anchors.bottomMargin: 8
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: renameDialog.currentGroup.updateOf
                                                  ? "DISPLAY NAME AFTER UPDATE"
                                                  : "NEW MOD NAME"
                                            font.pixelSize: 10
                                            font.weight: Font.Bold
                                            font.letterSpacing: 0.6
                                            color: nameField.activeFocus
                                                ? renameDialog.installAccent
                                                : (modManager.theme === "Cozy" ? "#8A8A8A" : "#64748b")
                                        }

                                        Text {
                                            visible: !!renameDialog.currentGroup.updateOf
                                            text: "UPDATES " + (renameDialog.currentGroup.updateOf || "")
                                            font.pixelSize: 10
                                            font.weight: Font.Bold
                                            font.letterSpacing: 0.6
                                            color: renameDialog.installAccent
                                        }

                                        Item { Layout.fillWidth: true }

                                        Rectangle {
                                            visible: !renameDialog.currentGroup.updateOf
                                                     && nameField.text !== renameDialog.currentGroup.originalName
                                            Layout.preferredHeight: 20
                                            Layout.preferredWidth: resetRow.implicitWidth + 12
                                            radius: 5
                                            color: resetMouseArea.containsMouse ? (modManager.theme === "Cozy" ? Qt.rgba(0, 0, 0, 0.08) : Qt.rgba(1, 1, 1, 0.12)) : "transparent"

                                            RowLayout {
                                                id: resetRow
                                                anchors.centerIn: parent
                                                spacing: 4

                                                Image {
                                                    Layout.preferredWidth: 10
                                                    Layout.preferredHeight: 10
                                                    source: "qrc:/UniversalModManager/Lucide Icons/undo-2.svg"
                                                    sourceSize: Qt.size(10, 10)
                                                    layer.enabled: true
                                                    layer.effect: ColorOverlay {
                                                        color: modManager.theme === "Cozy" ? "#7A7A7A" : "#94a3b8"
                                                    }
                                                }

                                                Text {
                                                    text: "Reset"
                                                    font.pixelSize: 10
                                                    font.weight: Font.Medium
                                                    color: modManager.theme === "Cozy" ? "#7A7A7A" : "#94a3b8"
                                                }
                                            }

                                            MouseArea {
                                                id: resetMouseArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    nameField.text = renameDialog.currentGroup.originalName || ""
                                                }
                                            }

                                            StyledToolTip {
                                                visible: resetMouseArea.containsMouse
                                                text: "Reset to original archive name"
                                                delay: 300
                                            }
                                        }

                                        Rectangle {
                                            visible: nameField.text.length > 0 && nameField.activeFocus
                                            Layout.preferredHeight: 20
                                            Layout.preferredWidth: 20
                                            radius: 10
                                            color: clearMouseArea.containsMouse ? (modManager.theme === "Cozy" ? Qt.rgba(0, 0, 0, 0.1) : Qt.rgba(1, 1, 1, 0.15)) : "transparent"

                                            Image {
                                                anchors.centerIn: parent
                                                source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                                                sourceSize: Qt.size(10, 10)
                                                layer.enabled: true
                                                layer.effect: ColorOverlay {
                                                    color: modManager.theme === "Cozy" ? "#7A7A7A" : "#94a3b8"
                                                }
                                            }

                                            MouseArea {
                                                id: clearMouseArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: nameField.text = ""
                                            }
                                        }
                                    }

                                    TextField {
                                        id: nameField
                                        ContextMenu.menu: EditContextMenu { editor: nameField }
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        text: (renameDialog.currentGroup.newName || renameDialog.currentGroup.originalName) || ""
                                        placeholderText: "Enter mod display name..."
                                        placeholderTextColor: modManager.theme === "Cozy" ? "#B0A090" : "#475569"
                                        font.pixelSize: 14
                                        font.weight: Font.Medium
                                        color: modManager.theme === "Cozy" ? "#1F1F1F" : "#FFFFFF"
                                        selectByMouse: true
                                        persistentSelection: true
                                        verticalAlignment: Text.AlignVCenter
                                        padding: 0
                                        leftPadding: 0
                                        rightPadding: 0

                                        background: Rectangle {
                                            color: "transparent"
                                        }

                                        onTextChanged: {
                                            if (renameDialog.syncingNameField)
                                                return
                                            renameDialog.setGroupNewName(
                                                renameDialog.selectedIndex, text)
                                        }
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: 2
                                spacing: 10
                                visible: modGroupColumn.hasRootPlan

                                Rectangle {
                                    Layout.fillWidth: true
                                    visible: modGroupColumn.installError !== ""
                                    implicitHeight: installErrorRow.implicitHeight + 20
                                    radius: 9
                                    color: Qt.rgba(0.94, 0.27, 0.27, 0.10)
                                    border.width: 1
                                    border.color: Qt.rgba(0.94, 0.27, 0.27, 0.35)

                                    RowLayout {
                                        id: installErrorRow
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 9

                                        Image {
                                            Layout.preferredWidth: 15
                                            Layout.preferredHeight: 15
                                            source: "qrc:/UniversalModManager/Lucide Icons/circle-alert.svg"
                                            sourceSize: Qt.size(15, 15)
                                            layer.enabled: true
                                            layer.effect: ColorOverlay {
                                                color: "#EF4444"
                                            }
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modGroupColumn.installError
                                            wrapMode: Text.WordWrap
                                            font.pixelSize: 11
                                            color: modManager.theme === "Cozy" ? "#7F1D1D" : "#FCA5A5"
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    visible: modGroupColumn.installWarnings.length > 0
                                             && modGroupColumn.installError === ""
                                    implicitHeight: installWarningRow.implicitHeight + 20
                                    radius: 9
                                    color: Qt.rgba(0.96, 0.62, 0.04, 0.10)
                                    border.width: 1
                                    border.color: Qt.rgba(0.96, 0.62, 0.04, 0.32)

                                    RowLayout {
                                        id: installWarningRow
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 9

                                        Image {
                                            Layout.preferredWidth: 15
                                            Layout.preferredHeight: 15
                                            Layout.alignment: Qt.AlignTop
                                            source: "qrc:/UniversalModManager/Lucide Icons/triangle-alert.svg"
                                            sourceSize: Qt.size(15, 15)
                                            layer.enabled: true
                                            layer.effect: ColorOverlay {
                                                color: "#F59E0B"
                                            }
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modGroupColumn.warningsText()
                                            wrapMode: Text.WordWrap
                                            font.pixelSize: 11
                                            color: modManager.theme === "Cozy" ? "#7C4A03" : "#FCD34D"
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    visible: renameDialog.groupHasLuaRoot(renameDialog.currentGroup)
                                    implicitHeight: luaOptionRow.implicitHeight + 20
                                    radius: 9
                                    color: modManager.theme === "Cozy"
                                        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.06)
                                        : Qt.rgba(1, 1, 1, 0.03)
                                    border.width: 1
                                    border.color: modManager.theme === "Cozy"
                                        ? "#EDE4D9"
                                        : Qt.rgba(1, 1, 1, 0.07)

                                    RowLayout {
                                        id: luaOptionRow
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        anchors.topMargin: 10
                                        anchors.bottomMargin: 10
                                        spacing: 10

                                        Image {
                                            Layout.preferredWidth: 16
                                            Layout.preferredHeight: 16
                                            Layout.alignment: Qt.AlignVCenter
                                            source: "qrc:/UniversalModManager/Lucide Icons/folder-code.svg"
                                            sourceSize: Qt.size(16, 16)
                                            layer.enabled: true
                                            layer.effect: ColorOverlay {
                                                color: window.primaryColor
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 1

                                            Text {
                                                text: "Enable the Lua mod after install"
                                                font.pixelSize: 12
                                                font.weight: Font.Medium
                                                color: modManager.theme === "Cozy" ? "#2C2C2C" : "#F1F5F9"
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: "Off keeps it disabled in the Lua Mods list until you turn it on."
                                                wrapMode: Text.WordWrap
                                                font.pixelSize: 10
                                                color: modManager.theme === "Cozy" ? "#8A7A6A" : "#8291A8"
                                            }
                                        }

                                        LuaEnableSwitch {
                                            id: luaEnableSwitch
                                            Layout.alignment: Qt.AlignVCenter
                                            checked: renameDialog.currentGroup.luaEnabled === true
                                            onToggled: {
                                                luaEnableSwitch.checked = checked
                                                renameDialog.setGroupLuaEnabled(
                                                    renameDialog.selectedIndex, checked)
                                            }
                                        }
                                    }
                                }

                                Repeater {
                                    model: modGroupColumn.installRoots

                                    delegate: Item {
                                        id: destinationDelegate
                                        Layout.fillWidth: true
                                        implicitHeight: destinationSection.implicitHeight
                                        property var rootItem: modelData

                                        InstallDestinationSection {
                                            id: destinationSection
                                            width: parent.width
                                            rootData: destinationDelegate.rootItem
                                            folderName: modGroupColumn.installFolderName
                                            bundleFolderName: renameDialog.effectiveBundleForGroup(
                                                renameDialog.currentGroup)
                                            originalName: modGroupColumn.groupOriginalName
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: 2
                                spacing: 10
                                visible: !modGroupColumn.hasRootPlan

                                Image {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    source: "qrc:/UniversalModManager/Lucide Icons/folder.svg"
                                    sourceSize: Qt.size(16, 16)
                                    layer.enabled: true
                                    layer.effect: ColorOverlay {
                                        color: window.primaryColor
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 1

                                    Text {
                                        text: renameDialog.pathLeaf(modManager.modsPath) || "Destination"
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        color: modManager.theme === "Cozy" ? "#2C2C2C" : "#F1F5F9"
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: modManager.modsPath
                                            ? renameDialog.shortPath(
                                                  modManager.modsPath
                                                  + (renameDialog.effectiveBundleForGroup(
                                                         renameDialog.currentGroup) !== ""
                                                      ? "/" + renameDialog.effectiveBundleForGroup(
                                                             renameDialog.currentGroup)
                                                      : "")
                                                  + "/" + modGroupColumn.installFolderName)
                                            : "Destination unavailable"
                                        font.family: "JetBrains Mono"
                                        font.pixelSize: 11
                                        color: modManager.theme === "Cozy" ? "#6F5F4E" : "#A3B0C4"
                                        elide: Text.ElideMiddle
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                spacing: 8
                                visible: !modGroupColumn.hasRootPlan

                                Text {
                                    text: "Target Files Preview"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: modManager.theme === "Cozy" ? "#5A5A5A" : "#cbd5e1"
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    text: (renameDialog.currentGroup.files ? renameDialog.currentGroup.files.length : 0) + " core files"
                                    font.pixelSize: 11
                                    color: modManager.theme === "Cozy" ? "#8A8A8A" : "#64748b"
                                }
                            }

                            Column {
                                Layout.fillWidth: true
                                spacing: 8
                                visible: !modGroupColumn.hasRootPlan

                                Repeater {
                                    model: renameDialog.currentGroup.files

                                    delegate: Rectangle {
                                        id: fileItem
                                        width: parent.width
                                        implicitHeight: 52
                                        radius: 10
                                        color: fileHoverArea.containsMouse
                                            ? (modManager.theme === "Cozy" ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.07))
                                            : (modManager.theme === "Cozy" ? "#F7F3EE" : Qt.rgba(1, 1, 1, 0.035))
                                        border.width: 1
                                        border.color: fileHoverArea.containsMouse
                                            ? (modManager.theme === "Cozy" ? "#D8C5B2" : Qt.rgba(1, 1, 1, 0.15))
                                            : (modManager.theme === "Cozy" ? "#EBE0D4" : Qt.rgba(1, 1, 1, 0.05))

                                        Behavior on color { ColorAnimation { duration: 150 } }
                                        Behavior on border.color { ColorAnimation { duration: 150 } }

                                        readonly property string extUpper: (modelData.extension || "").toUpperCase()
                                        readonly property color extColor: {
                                            if (extUpper.indexOf("PAK") !== -1) return modManager.pakColor || "#F97316"
                                            if (extUpper.indexOf("UCAS") !== -1) return modManager.ucasColor || "#D946EF"
                                            if (extUpper.indexOf("UTOC") !== -1) return modManager.utocColor || "#3B82F6"
                                            return modManager.theme === "Cozy" ? window.primaryColor : "#94a3b8"
                                        }

                                        readonly property string computedName: {
                                            var typedName = nameField.text.trim()
                                            if (typedName.toLowerCase().endsWith("_p"))
                                                typedName = typedName.slice(0, typedName.length - 2)

                                            var n = modelData.name || ""
                                            var dot = n.lastIndexOf(".")
                                            var base = dot > 0 ? n.slice(0, dot) : n
                                            if (base.toLowerCase().endsWith("_p"))
                                                base = base.slice(0, base.length - 2)

                                            if (modGroupColumn.groupOriginalName !== ""
                                                    && base.toLowerCase() !== modGroupColumn.groupOriginalName.toLowerCase())
                                                return base

                                            return typedName.length > 0 ? typedName : base
                                        }

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 12
                                            anchors.rightMargin: 12
                                            spacing: 12

                                            Image {
                                                Layout.preferredWidth: 16
                                                Layout.preferredHeight: 16
                                                source: "qrc:/UniversalModManager/Lucide Icons/file-box.svg"
                                                sourceSize: Qt.size(16, 16)
                                                layer.enabled: true
                                                layer.effect: ColorOverlay {
                                                    color: fileItem.extColor
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                spacing: 2

                                                Text {
                                                    id: fileNameText
                                                    Layout.fillWidth: true
                                                    Layout.minimumWidth: 0
                                                    text: fileItem.computedName
                                                    font.family: "JetBrains Mono"
                                                    font.pixelSize: 13
                                                    font.weight: Font.Medium
                                                    color: modManager.theme === "Cozy" ? "#2B2B2B" : "#F1F5F9"
                                                    elide: Text.ElideMiddle
                                                }

                                                Text {
                                                    visible: modelData.size !== undefined && modelData.size !== ""
                                                    text: modelData.size || ""
                                                    font.pixelSize: 10
                                                    color: modManager.theme === "Cozy" ? "#8A8A8A" : "#64748b"
                                                }
                                            }

                                            Rectangle {
                                                Layout.preferredWidth: 54
                                                Layout.minimumWidth: 54
                                                Layout.preferredHeight: 24
                                                radius: 6
                                                color: Qt.rgba(fileItem.extColor.r, fileItem.extColor.g, fileItem.extColor.b, 0.15)
                                                border.width: 1.5
                                                border.color: Qt.rgba(fileItem.extColor.r, fileItem.extColor.g, fileItem.extColor.b, 0.45)

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: fileItem.extUpper
                                                    font.family: "JetBrains Mono"
                                                    font.pixelSize: 10
                                                    font.weight: Font.Bold
                                                    color: fileItem.extColor
                                                }
                                            }
                                        }

                                        MouseArea {
                                            id: fileHoverArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                        }

                                        StyledToolTip {
                                            visible: fileHoverArea.containsMouse
                                            text: fileItem.computedName + "." + (modelData.extension ? modelData.extension.toLowerCase() : "")
                                            delay: 400
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                visible: !modGroupColumn.hasRootPlan && modGroupColumn.extraFiles.length > 0
                                radius: 10
                                color: modManager.theme === "Cozy" ? "#F7F3EE" : Qt.rgba(1, 1, 1, 0.02)
                                border.width: 1
                                border.color: modManager.theme === "Cozy" ? "#EBE0D4" : Qt.rgba(1, 1, 1, 0.05)
                                implicitHeight: extraFilesColumn.implicitHeight + 16

                                ColumnLayout {
                                    id: extraFilesColumn
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 8

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 38
                                        radius: 8
                                        color: extraFilesToggle.containsMouse
                                            ? (modManager.theme === "Cozy" ? Qt.rgba(0, 0, 0, 0.05) : Qt.rgba(1, 1, 1, 0.06))
                                            : "transparent"
                                        Behavior on color { ColorAnimation { duration: 150 } }

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 10
                                            anchors.rightMargin: 10
                                            spacing: 10

                                            Image {
                                                Layout.preferredWidth: 14
                                                Layout.preferredHeight: 14
                                                source: modGroupColumn.extraFilesExpanded
                                                    ? "qrc:/UniversalModManager/Lucide Icons/chevron-down.svg"
                                                    : "qrc:/UniversalModManager/Lucide Icons/chevron-right.svg"
                                                sourceSize: Qt.size(14, 14)
                                                layer.enabled: true
                                                layer.effect: ColorOverlay {
                                                    color: modManager.theme === "Cozy" ? "#6B6B6B" : "#94a3b8"
                                                }
                                            }

                                            Image {
                                                Layout.preferredWidth: 14
                                                Layout.preferredHeight: 14
                                                source: "qrc:/UniversalModManager/Lucide Icons/archive.svg"
                                                sourceSize: Qt.size(14, 14)
                                                layer.enabled: true
                                                layer.effect: ColorOverlay {
                                                    color: modManager.theme === "Cozy" ? window.primaryColor : "#94a3b8"
                                                }
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: "Additional archive files"
                                                font.pixelSize: 12
                                                font.weight: Font.DemiBold
                                                color: modManager.theme === "Cozy" ? "#4A4A4A" : "#cbd5e1"
                                            }

                                            Rectangle {
                                                Layout.preferredWidth: 28
                                                Layout.preferredHeight: 20
                                                radius: 10
                                                color: modManager.theme === "Cozy" ? Qt.rgba(0, 0, 0, 0.06) : Qt.rgba(1, 1, 1, 0.08)

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modGroupColumn.extraFiles.length
                                                    font.pixelSize: 10
                                                    font.weight: Font.Bold
                                                    color: modManager.theme === "Cozy" ? "#6B6B6B" : "#94a3b8"
                                                }
                                            }
                                        }

                                        MouseArea {
                                            id: extraFilesToggle
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: modGroupColumn.extraFilesExpanded = !modGroupColumn.extraFilesExpanded
                                        }
                                    }

                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        visible: modGroupColumn.extraFilesExpanded

                                        Repeater {
                                            model: modGroupColumn.extraFiles

                                            delegate: Rectangle {
                                                width: parent.width
                                                height: 40
                                                radius: 8
                                                color: modManager.theme === "Cozy" ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.03)
                                                border.width: 1
                                                border.color: modManager.theme === "Cozy" ? "#ECECEC" : Qt.rgba(1, 1, 1, 0.04)

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 10
                                                    anchors.rightMargin: 10
                                                    spacing: 10

                                                    Image {
                                                        Layout.preferredWidth: 14
                                                        Layout.preferredHeight: 14
                                                        source: "qrc:/UniversalModManager/Lucide Icons/file.svg"
                                                        sourceSize: Qt.size(14, 14)
                                                        layer.enabled: true
                                                        layer.effect: ColorOverlay {
                                                            color: modManager.theme === "Cozy" ? "#8A8A8A" : "#64748b"
                                                        }
                                                    }

                                                    Text {
                                                        text: modelData.relativePath || modelData.name
                                                        Layout.fillWidth: true
                                                        Layout.minimumWidth: 0
                                                        font.family: "JetBrains Mono"
                                                        font.pixelSize: 11
                                                        color: modManager.theme === "Cozy" ? "#4A4A4A" : "#cbd5e1"
                                                        elide: Text.ElideMiddle
                                                    }

                                                    Text {
                                                        text: modelData.size || ""
                                                        font.pixelSize: 10
                                                        color: modManager.theme === "Cozy" ? "#8B8B8B" : "#64748b"
                                                    }

                                                    Rectangle {
                                                        Layout.preferredWidth: 46
                                                        Layout.minimumWidth: 46
                                                        Layout.preferredHeight: 20
                                                        radius: 4
                                                        color: Qt.rgba(1, 1, 1, 0.04)
                                                        border.width: 1
                                                        border.color: modManager.theme === "Cozy" ? "#E0E0E0" : Qt.rgba(1, 1, 1, 0.08)

                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: (modelData.extension || "FILE").toUpperCase()
                                                            font.pixelSize: 9
                                                            font.weight: Font.Bold
                                                            color: modManager.theme === "Cozy" ? "#6B6B6B" : "#94a3b8"
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
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: modManager.theme === "Cozy" ? "#EAE0D5" : Qt.rgba(1, 1, 1, 0.08)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                visible: renameDialog.planRootCount() > 0
                text: {
                    var roots = renameDialog.planRootCount()
                    var files = renameDialog.planFileCount()
                    return roots + (roots === 1 ? " location" : " locations")
                        + " · " + files + (files === 1 ? " file" : " files")
                }
                font.pixelSize: 11
                font.weight: Font.Medium
                color: modManager.theme === "Cozy" ? "#8A7A6A" : "#8291A8"
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: 130
                Layout.preferredHeight: 44
                radius: 10
                color: cancelHover.containsMouse
                    ? (modManager.theme === "Cozy" ? Qt.rgba(0, 0, 0, 0.06) : Qt.rgba(1, 1, 1, 0.08))
                    : "transparent"
                border.width: 1
                border.color: modManager.theme === "Cozy" ? "#D8C5B2" : Qt.rgba(1, 1, 1, 0.14)

                Behavior on color { ColorAnimation { duration: 150 } }

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        text: "Cancel"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: cancelHover.containsMouse
                            ? (modManager.theme === "Cozy" ? "#1A1A1A" : "#FFFFFF")
                            : (modManager.theme === "Cozy" ? "#5A5A5A" : "#cbd5e1")
                    }
                }

                MouseArea {
                    id: cancelHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: renameDialog.reject()
                }
            }

            Rectangle {
                id: confirmBtn
                Layout.preferredWidth: 160
                Layout.preferredHeight: 44
                radius: 10
                opacity: renameDialog.canInstall ? 1.0 : 0.45
                color: {
                    if (modManager.theme === "Wavey") return confirmHover.containsMouse ? "#2563eb" : "#3b82f6"
                    if (modManager.theme === "Glassmorphic") return confirmHover.containsMouse ? Qt.lighter(modManager.accentColor, 1.1) : modManager.accentColor
                    return "transparent"
                }

                gradient: {
                    if (modManager.theme === "Cozy") return cozyButtonGradient
                    if (modManager.theme === "Crimson") return crimsonButtonGradient
                    return null
                }

                Gradient {
                    id: cozyButtonGradient
                    GradientStop { position: 0.0; color: confirmHover.containsMouse ? "#E09940" : "#D6A963" }
                    GradientStop { position: 1.0; color: confirmHover.containsMouse ? "#C88930" : "#BE9950" }
                }

                Gradient {
                    id: crimsonButtonGradient
                    GradientStop { position: 0.0; color: confirmHover.containsMouse ? Qt.lighter(window.primaryColor, 1.12) : window.primaryColor }
                    GradientStop { position: 1.0; color: confirmHover.containsMouse ? window.primaryColor : Qt.darker(window.primaryColor, 1.15) }
                }

                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on opacity { NumberAnimation { duration: 150 } }

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: {
                        if (modManager.theme === "Cozy") return Qt.rgba(0.84, 0.66, 0.39, confirmHover.containsMouse ? 0.55 : 0.35)
                        if (modManager.theme === "Crimson") return Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, confirmHover.containsMouse ? 0.65 : 0.45)
                        if (modManager.theme === "Wavey") return Qt.rgba(0.23, 0.51, 0.96, confirmHover.containsMouse ? 0.65 : 0.45)
                        return Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, confirmHover.containsMouse ? 0.65 : 0.45)
                    }
                    shadowVerticalOffset: confirmHover.containsMouse ? 5 : 2
                    shadowBlur: confirmHover.containsMouse ? 18 : 12
                }

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 8

                    Image {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        source: "qrc:/UniversalModManager/Lucide Icons/download.svg"
                        sourceSize: Qt.size(16, 16)
                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: "#FFFFFF"
                        }
                    }

                    Text {
                        text: "Confirm Install"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: "#FFFFFF"
                    }
                }

                MouseArea {
                    id: confirmHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: renameDialog.canInstall ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                    onClicked: {
                        if (renameDialog.canInstall)
                            renameDialog.accept()
                    }
                }
            }
        }
    }
}
