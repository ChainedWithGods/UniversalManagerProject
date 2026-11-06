import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Item {
    id: homePageTips

    height: hintText.height
    width: parent ? parent.width : 0

    property var hintList: []
    property int hintIndex: 0
    property string currentHint: ""
    function formatHint(t, theme) {
        var s = t
        s = s.replace("{search}", window.shortcutSearch)
        s = s.replace("{install}", window.shortcutInstall)
        s = s.replace("{openFolder}", window.shortcutOpenFolder)
        s = s.replace("{backup}", window.shortcutBackup)
        s = s.replace("{refresh}", window.shortcutRefresh)
        s = s.replace("{settings}", window.shortcutSettings)
        s = s.replace("{home}", window.shortcutHome)
        s = s.replace("{selectAll}", window.shortcutSelectAll)
        s = s.replace("{delete}", window.shortcutDelete)
        s = s.replace("{toggleMod}", window.shortcutToggleMod)
        s = s.replace("{enableAll}", window.shortcutEnableAll)
        s = s.replace("{disableAll}", window.shortcutDisableAll)
        s = s.replace("{showPak}", window.shortcutShowInPakViewer)
        s = s.replace("{exportStructure}", window.shortcutExportModStructure)
        if (theme !== null && theme !== undefined) s = s.replace("{theme}", theme)
        return s
    }
    function shuffle(a) {
        for (var i = a.length - 1; i > 0; i--) {
            var r = Math.floor(Math.random() * (i + 1))
            var tmp = a[i]
            a[i] = a[r]
            a[r] = tmp
        }
        return a
    }
    function buildHints() {

        var proTips = [
            "Set a path for the output before injecting Avalonium",
            "Adding a delay is useful if the game has slow start up",
            "The best time to inject Avalonium is when the game is in the main menu (in the gameplay is even better)"
        ]

        var fyiTips = [
            "Avalonium currently only supports UE4.23 - UE5.8",
            "Avalonium is meant for lawful uses, it was not developed or designed for illegal uses",
            "Universal Manager project under development since 2024",
            "Universal Manager project developed by a solo developer",
            "Universal Manager Project is free and open source. If you paid for this, you got scammed!",
        ]

        var generalTips = [
            "Ensure you have a valid USMAP file before viewing models",
            "Drag & drop files to install mods",
            "Use search filters to find mods faster",
            "Back up before big changes",
            "Install only trusted mods",
            "Read Mods descriptions before installing",
            "Refresh after changing files",
            "Check mod versions before updating",
            "Back up before uninstalling packs",
            "Keep a clean backup before updates",
            "Try one mod at a time when testing",
            "Read changelogs before updating",
            "Remove unused mods to keep things tidy",
            "Check storage usage to stay within limits",
            "Try a quick refresh if a mod seems missing",
            "Restart the app after big installs",
            "Keep your library clean for best performance"
        ]

        var hints = [
            "Right-click a mod for more actions",
            "Did you know you can find mods by their IDs",
            "Use thumbnails for quick recognition",
            "Use Home for quick stats",
            "Use the context menu to manage mods",
            "Use the toolbar to batch actions faster",
            "Rename mods for clarity",
            "Use filters to hide inactive mods",
            "Keep mod names short and clear",
            "Select multiple mods for bulk actions",
            "Use Home to check active mod count"
        ]

        var quickTips = [
            "Press {search} to search mods",
            "Press {install} to install a local mod",
            "Press {openFolder} to open mods folder",
            "Press {backup} to back up your mods",
            "Press {refresh} to refresh the library",
            "Press {settings} to open Settings",
            "Press {home} to return Home",
            "Press {selectAll} to select all mods",
            "Press {delete} to remove selected mods",
            "Press {toggleMod} on hover to enable/disable a mod",
            "Press {enableAll} to enable all mods",
            "Press {disableAll} to disable all mods",
            "Press {showPak} to view Mod assets in Assets Viewer",
            "Press {exportStructure} to export mod structure",
            "Use keyboard shortcuts to speed up tasks"
        ]

        var trySuggestions = [
            "Try the {theme} theme for a new look",
            "Change accent color in Glassmorphic theme configration",
            "Sort by Active to see enabled mods",
            "Organize mods into folders",
            "Sort by Name to find mods fast",
            "Sort by Size to spot big mods",
            "Sort by Date to see recent installs",
            "Use bulk enable to test playlists",
            "Pin your favorites for quick access",
            "Use tags to group similar mods"
        ]

        var headsUp = [
            "Cozy favors warm, soft tones",
            "Glassmorphic lets wallpaper shine through",
            "Glassmorphic looks best with subtle wallpapers",
            "Crimson highlights contrast and glow"
        ]

        var themes = ["Cozy", "Crimson", "Wavey", "Glassmorphic"]
        var list = []

        for (var pt = 0; pt < proTips.length; pt++) {
            list.push("Pro tip: " + formatHint(proTips[pt], null))
        }

        for (var ft = 0; ft < fyiTips.length; ft++) {
            list.push("FYI: " + formatHint(fyiTips[ft], null))
        }

        for (var gt = 0; gt < generalTips.length; gt++) {
            list.push("Tip: " + formatHint(generalTips[gt], null))
        }

        for (var ht = 0; ht < hints.length; ht++) {
            list.push("Hint: " + formatHint(hints[ht], null))
        }

        for (var qt = 0; qt < quickTips.length; qt++) {
            list.push("Quick tip: " + formatHint(quickTips[qt], null))
        }

        for (var ts = 0; ts < trySuggestions.length; ts++) {
            var tryTpl = trySuggestions[ts]
            if (tryTpl.indexOf("{theme}") >= 0) {
                for (var th = 0; th < themes.length; th++) {
                    list.push("Try: " + formatHint(tryTpl, themes[th]))
                }
            } else {
                list.push("Try: " + formatHint(tryTpl, null))
            }
        }

        for (var hu = 0; hu < headsUp.length; hu++) {
            list.push("Heads up: " + formatHint(headsUp[hu], null))
        }

        list = shuffle(list)
        var maxHints = 220
        hintList = list.slice(0, maxHints)
    }
    function selectRandomHint() {
        if (hintList.length === 0) return
        var next = Math.floor(Math.random() * hintList.length)
        if (next === hintIndex) next = (next + 1) % hintList.length
        hintIndex = next
        currentHint = hintList[hintIndex]
    }
    Component.onCompleted: {
        buildHints()
        selectRandomHint()
    }

Text {
    id: hintText
    text: currentHint
    font.pixelSize: 14
    color: window.textSecondaryColor
    font.weight: 400
    opacity: 1
    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
}
Timer {
    id: hintRotateTimer
    interval: 15000
    repeat: true
    running: !window.focusAnimationsPaused
    onTriggered: {
        hintText.opacity = 0
        selectRandomHint()
        hintText.opacity = 1
    }
}
}
