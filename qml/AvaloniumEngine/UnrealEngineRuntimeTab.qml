import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Dialogs
import Qt.labs.folderlistmodel
import Qt5Compat.GraphicalEffects
import QtMultimedia

Item {
    id: runtimeTab

    property string gameExePath: modManager.gamePath ? modManager.gamePath : ""
    property string outputPath: modManager.runtimeOutputPath ? modManager.runtimeOutputPath : ""

    property real processPopupClosedAt: 0
    property string globalsStatus: modManager.avaloniumStatus
    property string runtimeLog: modManager.universalRuntimeLog
    property var runtimeHelpEntries: [
        {
            key: "InGameGUI",
            title: "In-Game GUI",
            group: "runtime",
            type: "Toggle",
            overview: "Renders the Universal Manager overlay window inside the game after DLL injection. The overlay provides a live control panel for live memory inspection.",
            summary: "Enables or disables the in-game Universal Manager overlay.",
            details: "Keep this enabled if you want the overlay to be available during gameplay. Disable it if a protected game terminates immediately after DLL injection."
        },
        {
            key: "UnrealEngineConsole",
            title: "Unreal Engine Console",
            group: "runtime",
            type: "Toggle",
            overview: "Hooks into the game's built-in Unreal Engine developer console and makes it accessible at runtime. This allows you to type and execute any console command the game's executable exposes.",
            summary: "Enables the Unreal Engine developer console inside the running game.",
            details: "If enabling the console causes instability, disable it for that specific game."
        },
        {
            key: "SkipPatternScan",
            title: "Skip Pattern Scan",
            group: "runtime",
            type: "Toggle",
            overview: "Controls which GUObjectArray discovery strategy the runtime uses first. By default, the runtime runs an AOB (Array of Bytes) pattern scan. Enabling this option skips that scan entirely and jumps straight to the obfuscation-aware fallback path.",
            summary: "Skips the default GUObjectArray AOB scan and goes directly to the obfuscation scan.",
            details: "Enable this for heavily protected or obfuscated games where the normal pattern scan consistently times out or fails."
        },
        {
            key: "EnableDiscoveryCache",
            title: "Enable Encrypted Cache",
            group: "runtime",
            type: "Toggle",
            overview: "Enables an encrypted cache system that saves the results of the engine discovery process to disk. On subsequent launches, the runtime can restore the engine state from the cache in milliseconds instead of running the full discovery scan again.",
            summary: "Accelerates startup by caching engine discovery results.",
            details: "The cache is encrypted using a rolling XOR cipher and includes PE validation (TimeDateStamp, CheckSum, SizeOfImage). If the game executable is updated or modified, the cache is automatically invalidated. Keep this enabled for faster initialization on repeated launches."
        },
        {
            key: "VerboseLog",
            title: "Verbose Log",
            group: "runtime",
            type: "Toggle",
            overview: "Enables verbose internal-trace logging in the runtime log file. When enabled, the logger emits Discovery [Debug] probes, USMap layout dumps, candidate scans (TUObjectArray / FField candidates), UStruct layout traces, and retry / fallback messages. When disabled, only high-level lifecycle and discovery result messages are written.",
            summary: "Filters the runtime log to high-level events or expands it with internal debug traces.",
            details: "Leave this disabled for normal use to keep the log concise. Enable it when you are troubleshooting discovery issues, named-pattern mismatches, or obfuscation heuristic failures, then share the resulting Avalonium.log when reporting a bug."
        },
        {
            key: "DumpAllDeveloperCommands",
            title: "Dump All Developer Commands",
            group: "sdk",
            type: "Action",
            overview: "Dumps all developer commands exposed by the game, then writes them to a file in the output directory for inspection or use in runtime to execute commands.",
            summary: "Collects and saves all developer commands.",
            details: "You typically only need to run this once per game. After the command list has been saved, you can disable this option to avoid the extra scanning overhead on subsequent runs."
        },
        {
            key: "AutoDiscoverAESKey",
            title: "Auto-discover AES Key",
            group: "crypto",
            type: "Toggle",
            overview: "Enables dynamic in-process AES-256 key discovery. Scans the running game's memory (mov imm32/imm64 opcodes, high-entropy .rdata/.data buffers, and heap regions) for AES key candidates, then validates each one against FPakInfo::IndexHash using hardware SHA-1. Successfully verified keys are saved to <Executable>_AESKey.json with complete decrypted asset inventory.",
            summary: "Automatically discovers and verifies the AES-256 encryption key from live memory at startup.",
            details: "When enabled, Avalonium performs sub-second memory scanning across the main executable and all loaded DLLs to find and verify the game's AES key. The scan uses hardware-accelerated SHA-1 validation against pak index hashes for mathematical certainty. If no encryption is detected, the scan aborts in 0ms. Verified keys are exported with Hex, Base64, C-Array formats and full pak/utoc asset breakdown. Benchmark results show discovery times ranging from 23ms to 2s depending on the number of candidates and archive sizes."
        },
        {
            key: "EnableLuaMods",
            title: "Enable Lua Mods",
            group: "lua",
            type: "Toggle",
            overview: "Activates the Avalonium Lua scripting bridge and the mod loader runtime. When enabled, Avalonium scans the configured mods folder, loads any .lua scripts it finds, and registers their exposed functions, hooks, and event handlers with the injected runtime.",
            summary: "Enables the Lua scripting and mod loader runtime inside the game.",
            details: "Disable this if you dont want use Avalonium with lua mods."
        },
        {
            key: "LuaModsPath",
            title: "Lua Mods Path",
            group: "lua",
            type: "Path",
            overview: "Custom directory where Avalonium looks for .lua mods at runtime. Avalonium recursively scans this folder for .lua files, loads every script it finds, and exposes their events to the game.",
            summary: "Custom directory for Lua mods (leave empty for default 'Mods' folder next to Avalonium.dll).",
            details: "Leave this empty to use the default 'Mods' folder that lives next to Avalonium.dll or point Avalonium to use a custom directory."
        },
        {
            key: "EnableCppMods",
            title: "Enable C++ Mods",
            group: "cpp",
            type: "Toggle",
            overview: "Activates the Avalonium native C++ mod loader. When enabled, Avalonium scans the configured mods folder and loads every mod folder that contains dlls/main.dll or dlls/<ModName>.dll exporting start_mod and uninstall_mod.",
            summary: "Enables the native C++ mod loader runtime inside the game.",
            details: "Disable this if you only use Lua mods or want to inject Avalonium without loading native mods."
        },
        {
            key: "CppModsPath",
            title: "C++ Mods Path",
            group: "cpp",
            type: "Path",
            overview: "Custom directory where Avalonium looks for native C++ mods at runtime. A mod is a folder that contains dlls/main.dll or dlls/<ModName>.dll exporting start_mod and uninstall_mod.",
            summary: "Custom directory for C++ mods (leave empty for default 'Mods' folder next to Avalonium.dll).",
            details: "Leave this empty to use the default 'Mods/Avalonium' folder that lives next to Avalonium.dll or point Avalonium to use a custom directory."
        },
        {
            key: "Enabled",
            title: "Enable Crash Watchdog",
            group: "watchdog",
            type: "Toggle",
            overview: "Starts the external AggressiveDog crash watchdog beside Avalonium.dll when the runtime loads. It records crash details for diagnosis; it does not prevent or fix a crash, and a normal exit with code 0 does not create a report.",
            summary: "Launches the AggressiveDog crash watchdog with the runtime.",
            details: "Leave this disabled unless you are diagnosing a crash. Some protected games react badly to external monitoring, so test with the watchdog off if the game closes unexpectedly."
        },
        {
            key: "LogDirectory",
            title: "Watchdog Reports Folder",
            group: "watchdog",
            type: "Path",
            overview: "Folder where the watchdog writes AggressiveDog_*.txt crash reports, exit reports, AggressiveDog-status.log and optional minidumps. Relative paths resolve beside Avalonium.dll.",
            summary: "Folder for watchdog reports, status log and minidumps.",
            details: "Leave this empty for the default CrashHandler/Crashes folder beside Avalonium.dll, or point it at a writable folder."
        },
        {
            key: "DumpRegisters",
            title: "Dump Registers",
            group: "watchdog",
            type: "Toggle",
            overview: "Includes the faulting thread's CPU register state in the watchdog text report.",
            summary: "Adds CPU registers to watchdog crash reports.",
            details: "Keep this enabled for useful crash reports. It only affects the text report format."
        },
        {
            key: "WriteMiniDump",
            title: "Write Mini-Dump",
            group: "watchdog",
            type: "Toggle",
            overview: "Writes a .dmp minidump beside each watchdog crash report so the crash can be inspected in a debugger.",
            summary: "Writes a minidump beside each crash report.",
            details: "Minidumps can use substantial disk space. Disable this if you only need the text report."
        },
        {
            key: "CatchFastFail",
            title: "Catch Fast-Fail",
            group: "watchdog",
            type: "Toggle",
            overview: "Captures Windows fast-fail events in addition to standard fatal exceptions.",
            summary: "Captures fast-fail terminations as well as fatal exceptions.",
            details: "Leave this enabled for the broadest crash coverage unless a report is too noisy."
        },
        {
            key: "TerminationBreakpoint",
            title: "Termination Breakpoint",
            group: "watchdog",
            type: "Toggle",
            overview: "Arms a hardware breakpoint on NtTerminateProcess so abnormal terminations are attributed to an in-process call. It only applies when the watchdog attaches as a debugger.",
            summary: "Attributes abnormal exits to the terminating thread.",
            details: "Disable this when a protected game treats debug registers as tampering. A protected game may terminate when this is enabled."
        },
        {
            key: "AttachDebugger",
            title: "Attach Debugger",
            group: "watchdog",
            type: "Toggle",
            overview: "Attaches the watchdog as a debugger so it can capture exception-level crash reports, call stacks and minidumps. With this off, the watchdog stays a passive monitor and still writes exit reports with thread snapshots.",
            summary: "Attaches as a debugger for exception-level crash reports.",
            details: "Off by default because protected games and Denovo can treat an attached debugger as tampering and terminate. Leave it off unless you are diagnosing a crash on an unprotected title."
        },
        {
            key: "MaxStackDepth",
            title: "Max Stack Depth",
            group: "watchdog",
            type: "Value",
            overview: "Maximum number of call-stack frames captured for the faulting thread in watchdog reports. Values are clamped to 1-256.",
            summary: "Frame cap for the faulting thread's call stack.",
            details: "Higher values produce deeper stacks but larger reports. Lower it when reports are too large."
        },
        {
            key: "OtherThreadStackDepth",
            title: "Other Thread Stack Depth",
            group: "watchdog",
            type: "Value",
            overview: "Maximum number of stack frames captured per other thread. Values are clamped to 0-64; 0 disables other-thread stacks.",
            summary: "Frames per other thread in watchdog reports.",
            details: "Set to 0 to skip other-thread stacks, or lower it when reports are too large."
        },
        {
            key: "SDKValidation",
            title: "SDK Validation",
            group: "sdk",
            type: "Action",
            overview: "After an SDK dump, the runtime re-reads SDKMetadata.json and cross-checks every recorded offset, class, and function signature against live memory to verify the dump is accurate.",
            summary: "Validates the generated SDK by comparing SDKMetadata.json against live memory.",
            details: "Enable this when you need high confidence in a freshly dumped SDK. The validation pass is more thorough than a standard run, so it may add noticeable overhead. Disable it once the SDK has been confirmed accurate."
        },
        {
            key: "SDKPath",
            title: "SDK Path",
            group: "paths",
            type: "Path",
            overview: "Tells the runtime where to find SDKMetadata.json on disk. The runtime reads this file during SDK Validation to know which offsets and signatures to verify against live memory.",
            summary: "Path to the folder containing SDKMetadata.json used for SDK Validation.",
            details: "This setting is only required when SDK Validation is enabled."
        },
        {
            key: "GenerateBuildSystem",
            title: "Generate Build System",
            group: "sdk",
            type: "Action",
            overview: "Automatically invokes GenerateBuild.py immediately after the SDK dump completes. This script sets up the build environment and project files needed to compile code against the dumped SDK.",
            summary: "Automatically runs GenerateBuild.py after the SDK dump finishes.",
            details: "Enable this if you want the post-dump build setup to run without any manual steps. Disable it if you prefer to run GenerateBuild.py yourself, or if you only need the raw SDK output."
        },
        {
            key: "SDKLibclangGeneration",
            title: "SDK Libclang Generation",
            group: "sdk",
            type: "Action",
            overview: "After the SDK dump, the runtime feeds the generated headers through a libclang-powered AST (Abstract Syntax Tree) pipeline. This produces a more structured, compiler-verified representation of the SDK and can catch issues that a plain text dump would miss.",
            summary: "Runs the libclang AST generation and verification pipeline after the SDK dump.",
            details: "Enable this when you want a deeper, compiler-validated SDK analysis pass."
        },
        {
            key: "LibclangPath",
            title: "Libclang Path",
            group: "paths",
            type: "Path",
            overview: "Specifies where libclang.dll is located on disk. The runtime loads this DLL when SDK Libclang Generation is enabled to drive the AST parsing pipeline.",
            summary: "Optional full path to libclang.dll.",
            details: "If left empty, Universal Manager will try to discover libclang automatically from an installed Visual Studio LLVM toolchain or a standalone libclang package. Only set this if auto-discovery fails or you want to use a specific version."
        },
        {
            key: "InitDelayMinutes",
            title: "Init Delay (Minutes)",
            group: "startup",
            type: "Value",
            overview: "Instructs the runtime to wait the specified number of minutes after DLL injection before beginning memory scanning and initialization. This gives the game engine time to fully load before the runtime starts operating on its memory.",
            summary: "Adds a minute-based startup delay before runtime initialization begins.",
            details: "Use this for games that load slowly or that are not ready for memory operations immediately after launch. This is typically needed when using dwmapi.dll for injection."
        },
        {
            key: "InitDelaySeconds",
            title: "Init Delay (Seconds)",
            group: "startup",
            type: "Value",
            overview: "Instructs the runtime to wait the specified number of seconds after DLL injection before beginning initialization. This complements Init Delay Minutes and allows precise sub-minute tuning.",
            summary: "Adds a second-based startup delay before runtime initialization begins.",
            details: "Use this for fine-grained delay tuning when a full minute-based delay is too long. For example, setting Minutes to 0 and Seconds to 10 will delay initialization by exactly 10 seconds. This is typically needed when using dwmapi.dll for injection."
        }
    ]
    property var runtimeHelpFilterOptions: [
        { key: "All", title: "All" },
        { key: "Toggle", title: "Toggles" },
        { key: "Action", title: "Actions" },
        { key: "Path", title: "Paths" },
        { key: "Value", title: "Values" }
    ]
    property var runtimeHelpGroups: [
        {
            key: "runtime",
            title: "Runtime & Overlay",
            description: "Injection-time UI, console, and compatibility settings."
        },
        {
            key: "crypto",
            title: "Crypto & Asset Discovery",
            description: "Dynamic AES key discovery and pak archive decryption."
        },
        {
            key: "sdk",
            title: "SDK Generation",
            description: "SDK dumping, validation, and post-processing options."
        },
        {
            key: "paths",
            title: "Paths",
            description: "File locations used by advanced SDK features."
        },
        {
            key: "lua",
            title: "Lua Mods",
            description: "Lua scripting and mod loader runtime."
        },
        {
            key: "cpp",
            title: "C++ Mods",
            description: "Native C++ mod loader runtime."
        },
        {
            key: "watchdog",
            title: "Crash Watchdog",
            description: "AggressiveDog crash reports and diagnostics."
        },
        {
            key: "startup",
            title: "Startup Delay",
            description: "Delay initialization until the game is ready."
        }
    ]
    property string runtimeHelpSource: "Guidance based on Avalonium.ini"
    property string runtimeHelpFilterType: "All"
    property int runtimeHelpIndex: 0
    readonly property bool runtimeHelpHasVisibleEntries: runtimeHelpFirstVisibleEntryIndex() >= 0
    readonly property var currentRuntimeHelpEntry: runtimeHelpEntries.length > 0
                                               && runtimeHelpIndex >= 0
                                               && runtimeHelpIndex < runtimeHelpEntries.length
                                               ? runtimeHelpEntries[runtimeHelpIndex]
                                               : null

    UnrealEngineRuntimeTabNotifications {
        id: runtimeNotifications
        anchors.fill: parent
    }

    UnrealEngineRuntimeTabDialogs {
        id: runtimeDialogs
        anchors.fill: parent
        toastNotification: runtimeNotifications.toastNotification
    }

    readonly property alias generatingDialog: runtimeDialogs.generatingDialog
    readonly property alias initializationDialog: runtimeDialogs.initializationDialog
    readonly property alias runtimeConfigDialog: runtimeDialogs.runtimeConfigDialog
    readonly property alias runtimeOutputPathDialog: runtimeDialogs.runtimeOutputPathDialog
    readonly property alias avaloniumConflictDialog: runtimeDialogs.avaloniumConflictDialog
    readonly property alias cDriveAdminDialog: runtimeDialogs.cDriveAdminDialog
    readonly property alias unsupportedVersionGameRunningDialog: runtimeDialogs.unsupportedVersionGameRunningDialog
    readonly property alias runtimeAlertDialog: runtimeDialogs.runtimeAlertDialog
    readonly property alias toastNotification: runtimeNotifications.toastNotification

    function isAvaloniumInjected() {
        if (typeof modManager.isAvaloniumInjected === "function") {
            return modManager.isAvaloniumInjected()
        }
        return false
    }

    property var runningProcessesList: []
    property string processSearchText: ""

    readonly property var filteredProcesses: {
        var list = runningProcessesList || []
        var query = (processSearchText || "").trim().toLowerCase()
        if (query.length === 0) {
            return list
        }
        return list.filter(function(item) {
            var name = (item && item.name ? item.name : "").toLowerCase()
            var pid = (item && item.pid ? item.pid : "").toString()
            var title = (item && item.title ? item.title : "").toLowerCase()
            var path = (item && item.path ? item.path : "").toLowerCase()
            return name.indexOf(query) !== -1 || pid.indexOf(query) !== -1 || title.indexOf(query) !== -1 || path.indexOf(query) !== -1
        })
    }

    function refreshRunningProcesses() {
        if (typeof modManager.getRunningProcesses === "function") {
            var res = modManager.getRunningProcesses()
            runningProcessesList = res || []
        }
    }

    component UiverseToggle : Item {
        id: uiverseToggle
        property bool checked: false
        signal toggled(bool checked)

        width: 46
        height: 24

        readonly property real _circleDiameter: 18
        readonly property real _offset: (height - _circleDiameter) / 2
        readonly property color _accent: window.primaryColor
        readonly property color _offBg: Qt.rgba(_accent.r, _accent.g, _accent.b,
                                               modManager.theme === "Cozy" ? 0.35 : 0.25)

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
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: uiverseToggle.toggled(!uiverseToggle.checked)
        }
    }

    SoundEffect {
        id: initializationCompleteSound
        source: "qrc:/UniversalModManager/assets/Notifications/initialized.wav"
        volume: 0.5
    }

    function copyLogToClipboard() {
        var text = runtimeLog || ""
        if (text.length === 0) return
        modManager.copyTextToClipboard(text)
        toastNotification.show("Copied to clipboard")
    }

    onRuntimeHelpFilterTypeChanged: ensureValidRuntimeHelpSelection()

    function openRuntimeConfigHelp() {
        ensureValidRuntimeHelpSelection()
        runtimeConfigHelpPopup.open()
    }

    function runtimeHelpEntryIndexByKey(key) {
        for (var i = 0; i < runtimeHelpEntries.length; ++i) {
            if ((runtimeHelpEntries[i].key || "") === key)
                return i
        }
        return -1
    }

    function selectRuntimeHelpEntryByKey(key) {
        var idx = runtimeHelpEntryIndexByKey(key)
        if (idx >= 0)
            runtimeHelpIndex = idx
    }

    function runtimeHelpEntryMatchesFilter(entry) {
        if (!entry)
            return false
        return runtimeHelpFilterType === "All" || runtimeHelpType(entry) === runtimeHelpFilterType
    }

    function runtimeHelpEntriesForGroup(groupKey) {
        var matches = []
        for (var i = 0; i < runtimeHelpEntries.length; ++i) {
            var entry = runtimeHelpEntries[i]
            if ((entry.group || "") === groupKey && runtimeHelpEntryMatchesFilter(entry))
                matches.push(entry)
        }
        return matches
    }

    function runtimeHelpFirstVisibleEntryIndex() {
        for (var i = 0; i < runtimeHelpGroups.length; ++i) {
            var groupEntries = runtimeHelpEntriesForGroup(runtimeHelpGroups[i].key || "")
            if (groupEntries.length > 0)
                return runtimeHelpEntryIndexByKey(groupEntries[0].key || "")
        }
        return -1
    }

    function ensureValidRuntimeHelpSelection() {
        var firstIndex = runtimeHelpFirstVisibleEntryIndex()
        if (firstIndex < 0) {
            runtimeHelpIndex = -1
            return
        }

        if (runtimeHelpIndex >= 0
                && runtimeHelpIndex < runtimeHelpEntries.length
                && runtimeHelpEntryMatchesFilter(runtimeHelpEntries[runtimeHelpIndex])) {
            return
        }

        runtimeHelpIndex = firstIndex
    }

    function runtimeHelpSummary(entry) {
        if (!entry) return ""
        return entry.summary || ""
    }

    function runtimeHelpOverview(entry) {
        if (!entry) return ""
        return entry.overview || ""
    }

    function runtimeHelpDetails(entry) {
        if (!entry) return ""
        return entry.details || ""
    }

    function runtimeHelpType(entry) {
        if (!entry) return ""
        return entry.type || ""
    }

    Gradient {
        id: cozyHelpDialogGradient
        GradientStop { position: 0.0; color: window.cozyDialogGradientTop }
        GradientStop { position: 1.0; color: window.cozyDialogGradientBottom }
    }
    Gradient {
        id: crimsonHelpDialogGradient
        GradientStop { position: 0.0; color: window.crimsonDialogGradientTop }
        GradientStop { position: 1.0; color: window.crimsonDialogGradientBottom }
    }
    Gradient {
        id: waveyHelpDialogGradient
        GradientStop { position: 0.0; color: "#121820" }
        GradientStop { position: 1.0; color: "#1E2631" }
    }

    Popup {
        id: runtimeConfigHelpPopup
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        x: Math.round(((parent ? parent.width : runtimeTab.width) - width) / 2)
        y: Math.round(((parent ? parent.height : runtimeTab.height) - height) / 2)
        width: Math.min(940, Math.max(640, (parent ? parent.width : runtimeTab.width) - 48))
        height: Math.min(680, Math.max(480, (parent ? parent.height : runtimeTab.height) - 64))
        padding: 0

        background: Rectangle {
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy") return cozyHelpDialogGradient
                if (modManager.theme === "Crimson") return crimsonHelpDialogGradient
                if (modManager.theme === "Wavey") return waveyHelpDialogGradient
                return null
            }

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.55)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 18
                shadowBlur: 0.9
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.65)
            Behavior on opacity { NumberAnimation { duration: 200 } }
        }

        enter: Transition {
            NumberAnimation { property: "scale"; from: 0.94; to: 1.0; duration: 180; easing.type: Easing.OutCubic }
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160 }
        }

        exit: Transition {
            NumberAnimation { property: "scale"; from: 1.0; to: 0.97; duration: 140; easing.type: Easing.InCubic }
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 140 }
        }

        contentItem: Item {
            implicitWidth: runtimeConfigHelpPopup.width
            implicitHeight: runtimeConfigHelpPopup.height

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: "Runtime Configuration Help"
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        color: window.textColor
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06)
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 14

                    Rectangle {
                        Layout.preferredWidth: 350
                        Layout.fillHeight: true
                        radius: 16
                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                        border.width: 1.5
                        border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    text: "Features"
                                    font.pixelSize: 15
                                    font.weight: Font.Bold
                                    color: window.textColor
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "Browse by group or filter"
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                    elide: Text.ElideRight
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Text {
                                    text: "TYPE FILTERS"
                                    font.pixelSize: 11
                                    font.weight: Font.Bold
                                    color: window.textColor
                                    opacity: 0.75
                                }

                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Repeater {
                                        model: runtimeTab.runtimeHelpFilterOptions

                                        delegate: Rectangle {
                                            required property var modelData
                                            property bool active: runtimeTab.runtimeHelpFilterType === (modelData.key || "")
                                            radius: 999
                                            color: active
                                                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                                                : (filterChipMouse.containsMouse
                                                    ? (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.05))
                                                    : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.025)))
                                            border.width: 1
                                            border.color: active
                                                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.50)
                                                : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.6))
                                            implicitWidth: filterChipText.implicitWidth + 22
                                            implicitHeight: 28

                                            Behavior on color { ColorAnimation { duration: 150 } }

                                            Text {
                                                id: filterChipText
                                                anchors.centerIn: parent
                                                text: modelData.title || ""
                                                font.pixelSize: 12
                                                font.weight: Font.Bold
                                                color: active ? window.primaryColor : window.textColor
                                            }

                                            MouseArea {
                                                id: filterChipMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: runtimeTab.runtimeHelpFilterType = modelData.key || "All"
                                            }
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.04)
                            }

                            ScrollView {
                                id: runtimeHelpSidebarScroll
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                rightPadding: 16
                                contentWidth: availableWidth

                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                ScrollBar.vertical: ScrollBar {
                                    id: sidebarVScrollBar
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
                                        color: sidebarVScrollBar.pressed
                                            ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor)
                                            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.25)
                                    }
                                }

                                Column {
                                    id: runtimeHelpSidebarContent
                                    width: runtimeHelpSidebarScroll.availableWidth
                                    spacing: 14

                                    Repeater {
                                        model: runtimeTab.runtimeHelpGroups

                                        delegate: Column {
                                            id: runtimeHelpGroupSection
                                            required property var modelData
                                            property var groupInfo: modelData
                                            property var groupEntries: runtimeTab.runtimeHelpEntriesForGroup(groupInfo.key || "")
                                            width: runtimeHelpSidebarContent.width
                                            spacing: 6
                                            visible: groupEntries.length > 0

                                            RowLayout {
                                                width: runtimeHelpGroupSection.width
                                                spacing: 8

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: groupInfo.title || ""
                                                    font.pixelSize: 13
                                                    font.weight: Font.Bold
                                                    color: window.textColor
                                                    elide: Text.ElideRight
                                                }

                                                Rectangle {
                                                    radius: 8
                                                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)
                                                    border.width: 1
                                                    border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.25)
                                                    implicitWidth: groupCountText.implicitWidth + 14
                                                    implicitHeight: 20

                                                    Text {
                                                        id: groupCountText
                                                        anchors.centerIn: parent
                                                        text: groupEntries.length
                                                        font.pixelSize: 11
                                                        font.weight: Font.Bold
                                                        color: window.primaryColor
                                                    }
                                                }
                                            }

                                            Text {
                                                width: runtimeHelpGroupSection.width
                                                visible: (groupInfo.description || "").length > 0
                                                text: groupInfo.description || ""
                                                font.pixelSize: 12
                                                color: window.textColor
                                                opacity: 0.75
                                                wrapMode: Text.WordWrap
                                                lineHeight: 1.35
                                            }

                                            Repeater {
                                                model: groupEntries

                                                delegate: Rectangle {
                                                    id: helpItemRow
                                                    required property var modelData
                                                    property bool selected: runtimeTab.currentRuntimeHelpEntry !== null
                                                                            && (runtimeTab.currentRuntimeHelpEntry.key || "") === (modelData.key || "")
                                                    width: runtimeHelpGroupSection.width
                                                    height: 46
                                                    radius: 10
                                                    color: selected
                                                        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                                                        : (runtimeHelpEntryMouse.containsMouse
                                                            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08)
                                                            : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0, 0, 0, 0.02)))
                                                    border.width: selected ? 1.5 : 1
                                                    border.color: selected
                                                        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.65)
                                                        : (runtimeHelpEntryMouse.containsMouse
                                                            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.25)
                                                            : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)))

                                                    Behavior on color { ColorAnimation { duration: 140 } }

                                                    Rectangle {
                                                        anchors.left: parent.left
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        anchors.leftMargin: 5
                                                        width: 4
                                                        height: 20
                                                        radius: 2
                                                        color: window.primaryColor
                                                        visible: helpItemRow.selected
                                                    }

                                                    RowLayout {
                                                        anchors.fill: parent
                                                        anchors.leftMargin: helpItemRow.selected ? 16 : 12
                                                        anchors.rightMargin: 12
                                                        spacing: 8

                                                        Text {
                                                            Layout.fillWidth: true
                                                            text: modelData.title || modelData.key || ""
                                                            font.pixelSize: 13
                                                            font.weight: Font.Bold
                                                            color: helpItemRow.selected ? window.primaryColor : window.textColor
                                                            elide: Text.ElideRight
                                                        }

                                                        Rectangle {
                                                            visible: (modelData.type || "").length > 0
                                                            radius: 6
                                                            color: helpItemRow.selected
                                                                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                                                                : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.05))
                                                            border.width: 1
                                                            border.color: helpItemRow.selected
                                                                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.40)
                                                                : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.08))
                                                            implicitWidth: typeTagText.implicitWidth + 12
                                                            implicitHeight: 22

                                                            Text {
                                                                id: typeTagText
                                                                anchors.centerIn: parent
                                                                text: modelData.type || ""
                                                                font.pixelSize: 11
                                                                font.weight: Font.Bold
                                                                color: helpItemRow.selected ? window.primaryColor : window.textColor
                                                            }
                                                        }
                                                    }

                                                    MouseArea {
                                                        id: runtimeHelpEntryMouse
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: runtimeTab.selectRuntimeHelpEntryByKey(modelData.key || "")
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        width: runtimeHelpSidebarContent.width
                                        visible: !runtimeTab.runtimeHelpHasVisibleEntries
                                        text: "No settings match the current filter."
                                        font.pixelSize: 13
                                        color: window.textSecondaryColor
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 16
                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                        border.width: 1.5
                        border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 14

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    text: "Setting Details"
                                    font.pixelSize: 15
                                    font.weight: Font.Bold
                                    color: window.textColor
                                }

                                Item { Layout.fillWidth: true }

                                RowLayout {
                                    spacing: 6
                                    visible: runtimeTab.currentRuntimeHelpEntry !== null

                                    Rectangle {
                                        radius: 8
                                        color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)
                                        border.width: 1
                                        border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.30)
                                        implicitWidth: keyBadgeText.implicitWidth + 16
                                        implicitHeight: 24

                                        Text {
                                            id: keyBadgeText
                                            anchors.centerIn: parent
                                            text: runtimeTab.currentRuntimeHelpEntry ? (runtimeTab.currentRuntimeHelpEntry.key || "") : ""
                                            font.pixelSize: 11
                                            font.weight: Font.Bold
                                            color: window.primaryColor
                                        }
                                    }

                                    Rectangle {
                                        visible: runtimeTab.currentRuntimeHelpEntry !== null && runtimeTab.runtimeHelpType(runtimeTab.currentRuntimeHelpEntry).length > 0
                                        radius: 8
                                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.05)
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.6)
                                        implicitWidth: typeBadgeText.implicitWidth + 16
                                        implicitHeight: 24

                                        Text {
                                            id: typeBadgeText
                                            anchors.centerIn: parent
                                            text: runtimeTab.currentRuntimeHelpEntry ? runtimeTab.runtimeHelpType(runtimeTab.currentRuntimeHelpEntry) : ""
                                            font.pixelSize: 11
                                            font.weight: Font.Bold
                                            color: window.textColor
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.04)
                            }

                            ScrollView {
                                id: runtimeHelpScroll
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                rightPadding: 14
                                contentWidth: availableWidth

                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                ScrollBar.vertical: ScrollBar {
                                    id: detailVScrollBar
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
                                        color: detailVScrollBar.pressed
                                            ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor)
                                            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.25)
                                    }
                                }

                                ColumnLayout {
                                    width: runtimeHelpScroll.availableWidth
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 14
                                        visible: runtimeTab.currentRuntimeHelpEntry !== null

                                        Text {
                                            Layout.fillWidth: true
                                            text: runtimeTab.currentRuntimeHelpEntry ? (runtimeTab.currentRuntimeHelpEntry.title || runtimeTab.currentRuntimeHelpEntry.key || "") : ""
                                            font.pixelSize: 24
                                            font.weight: Font.Bold
                                            color: window.textColor
                                            wrapMode: Text.WordWrap
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: runtimeTab.runtimeHelpSummary(runtimeTab.currentRuntimeHelpEntry)
                                            font.pixelSize: 14
                                            font.weight: Font.Medium
                                            color: window.textColor
                                            wrapMode: Text.WordWrap
                                            lineHeight: 1.45
                                        }

                                        Rectangle {
                                            visible: runtimeTab.runtimeHelpOverview(runtimeTab.currentRuntimeHelpEntry).length > 0
                                            Layout.fillWidth: true
                                            radius: 12
                                            color: modManager.theme === "Glassmorphic"
                                                ? Qt.rgba(1, 1, 1, 0.04)
                                                : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.03)
                                            border.width: 1
                                            border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.22)
                                            implicitHeight: overviewColumn.implicitHeight + 28

                                            ColumnLayout {
                                                id: overviewColumn
                                                anchors.fill: parent
                                                anchors.margins: 16
                                                spacing: 10

                                                RowLayout {
                                                    spacing: 8

                                                    Rectangle {
                                                        width: 24
                                                        height: 24
                                                        radius: 6
                                                        color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)

                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: "?"
                                                            font.pixelSize: 13
                                                            font.weight: Font.Bold
                                                            color: window.primaryColor
                                                        }
                                                    }

                                                    Text {
                                                        text: "What It Does"
                                                        font.pixelSize: 14
                                                        font.weight: Font.Bold
                                                        color: window.textColor
                                                    }
                                                }

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: runtimeTab.runtimeHelpOverview(runtimeTab.currentRuntimeHelpEntry)
                                                    font.pixelSize: 13
                                                    color: window.textColor
                                                    wrapMode: Text.WordWrap
                                                    lineHeight: 1.5
                                                }
                                            }
                                        }

                                        Rectangle {
                                            visible: runtimeTab.runtimeHelpDetails(runtimeTab.currentRuntimeHelpEntry).length > 0
                                            Layout.fillWidth: true
                                            radius: 12
                                            color: modManager.theme === "Glassmorphic"
                                                ? Qt.rgba(46 / 255, 158 / 255, 96 / 255, 0.06)
                                                : Qt.rgba(46 / 255, 158 / 255, 96 / 255, 0.04)
                                            border.width: 1
                                            border.color: Qt.rgba(46 / 255, 158 / 255, 96 / 255, 0.30)
                                            implicitHeight: detailColumn.implicitHeight + 28

                                            ColumnLayout {
                                                id: detailColumn
                                                anchors.fill: parent
                                                anchors.margins: 16
                                                spacing: 10

                                                RowLayout {
                                                    spacing: 8

                                                    Rectangle {
                                                        width: 24
                                                        height: 24
                                                        radius: 6
                                                        color: Qt.rgba(46 / 255, 158 / 255, 96 / 255, 0.15)

                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: "✓"
                                                            font.pixelSize: 13
                                                            font.weight: Font.Bold
                                                            color: "#2E9E60"
                                                        }
                                                    }

                                                    Text {
                                                        text: "Tip"
                                                        font.pixelSize: 14
                                                        font.weight: Font.Bold
                                                        color: window.textColor
                                                    }
                                                }

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: runtimeTab.runtimeHelpDetails(runtimeTab.currentRuntimeHelpEntry)
                                                    font.pixelSize: 13
                                                    color: window.textColor
                                                    wrapMode: Text.WordWrap
                                                    lineHeight: 1.5
                                                }
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.alignment: Qt.AlignCenter
                                        spacing: 8
                                        visible: runtimeTab.currentRuntimeHelpEntry === null
                                                 || !runtimeTab.runtimeHelpHasVisibleEntries

                                        Item { Layout.preferredHeight: 40 }

                                        Text {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: "No Setting Selected"
                                            font.pixelSize: 16
                                            font.weight: Font.Bold
                                            color: window.textColor
                                        }

                                        Text {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: runtimeTab.runtimeHelpHasVisibleEntries
                                                ? "Select a setting from the features list on the left to view details."
                                                : "No runtime configuration help entries match your filter."
                                            font.pixelSize: 13
                                            color: window.textColor
                                            opacity: 0.8
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: runtimeTab.runtimeHelpSource.length > 0 ? "Source: " + runtimeTab.runtimeHelpSource : ""
                        font.pixelSize: 12
                        color: window.textColor
                        opacity: 0.8
                        elide: Text.ElideMiddle
                    }

                    Rectangle {
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 38
                        radius: 19
                        color: helpFooterCloseMouse.containsMouse
                            ? (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.08))
                            : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.04))
                        border.width: 1
                        border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.15) : window.borderColor

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            anchors.centerIn: parent
                            text: "Close"
                            font.pixelSize: 13
                            font.weight: Font.Bold
                            color: window.textColor
                        }

                        MouseArea {
                            id: helpFooterCloseMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: runtimeConfigHelpPopup.close()
                        }
                    }
                }
            }
        }
    }

    property int studioViewMode: 0

    function parseGlobalsData(s) {
        if (!s || s.trim().length === 0) return null
        var lines = s.split(/\r?\n/)
        var coreKeys = ["Game", "Version", "Engine", "Initialized"]
        var pointerKeys = ["GUObjectArray", "GNames", "GWorld", "GMalloc", "GNatives", "GUObjectHashTables", "GEngine", "ProcessEvent", "ProcessEventIdx"]
        var coreClassKeys = ["Class", "Object", "Actor", "Pawn", "Enum", "Function", "Package", "Struct", "World"]
        var sdkKeys = ["Functions", "CoreClasses", "Offsets", "Objects", "Globals"]
        var layoutKeys = ["SuperOff", "ChildrenOff", "EFieldNextOff", "ChildPropsOff", "FFieldNameOff", "FFieldNextOff", "UFieldNextOff"]
        var res = { core: [], pointers: [], classes: [], sdk: [], layout: [], other: [] }
        for (var i = 0; i < lines.length; i++) {
            var line = lines[i].trim()
            if (!line) continue
            var idx = line.indexOf(":")
            if (idx === -1) idx = line.indexOf("=")
            var key = idx > 0 ? line.slice(0, idx).trim() : ""
            var val = idx > 0 ? line.slice(idx + 1).trim() : line
            if (!key && !val) continue
            if (coreKeys.indexOf(key) !== -1) res.core.push({ key: key, val: val })
            else if (pointerKeys.indexOf(key) !== -1) res.pointers.push({ key: key, val: val })
            else if (coreClassKeys.indexOf(key) !== -1) res.classes.push({ key: key, val: val })
            else if (sdkKeys.indexOf(key) !== -1) res.sdk.push({ key: key, val: val })
            else if (layoutKeys.indexOf(key) !== -1) res.layout.push({ key: key, val: val })
            else res.other.push({ key: key, val: val })
        }
        return res
    }

    readonly property var currentGlobalsData: parseGlobalsData(globalsStatus)

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 18

        ColumnLayout {
            Layout.preferredWidth: 680
            Layout.minimumWidth: 580
            Layout.maximumWidth: 760
            Layout.fillHeight: true
            spacing: 14

            Rectangle {
                id: controlDeckCard
                Layout.fillWidth: true
                Layout.preferredHeight: controlDeckCol.implicitHeight + 28
                radius: 20
                color: modManager.theme === "Wavey" ? Qt.rgba(0.08, 0.16, 0.28, 0.94) : window.surfaceColor
                border.width: window.borderWidth
                border.color: modManager.theme === "Glassmorphic"
                    ? Qt.rgba(1, 1, 1, 0.18)
                    : window.borderColor

                layer.enabled: modManager.theme !== "Glassmorphic"
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(0, 0, 0, 0.18)
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 3
                    shadowBlur: 0.35
                }

                ColumnLayout {
                    id: controlDeckCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: modManager.isAdvancedProcessMode ? "Target Running Process" : "Game Executable Path"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: window.textColor
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: "Advanced Injection"
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            color: modManager.isAdvancedProcessMode
                                ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor)
                                : window.textSecondaryColor
                        }

                        UiverseToggle {
                            checked: modManager.isAdvancedProcessMode
                            onToggled: function(val) {
                                modManager.setAdvancedProcessMode(val)
                                if (val) {
                                    runtimeTab.refreshRunningProcesses()
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            visible: !modManager.isAdvancedProcessMode
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            radius: 18
                            color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(1, 1, 1, 0.08)
                                : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05)
                            border.width: 1
                            border.color: window.borderColor

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: gameExePath || "Auto-detect from active game"
                                    font.pixelSize: 13
                                    color: gameExePath ? window.textColor : window.textSecondaryColor
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideMiddle
                                }
                            }
                        }

                        Button {
                            id: autoDetectExeButton
                            visible: !modManager.isAdvancedProcessMode
                            enabled: !modManager.generating
                            text: "Auto Detected"
                            Layout.preferredWidth: 110
                            Layout.preferredHeight: 36
                            flat: true

                            contentItem: Text {
                                text: parent.text
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: "#FFFFFF"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                color: parent.hovered ? Qt.lighter(window.primaryColor, 1.15) : window.primaryColor
                                radius: 18
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                        }

                        Rectangle {
                            id: processSelectorBtn
                            visible: modManager.isAdvancedProcessMode
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            radius: 18
                            color: processSelectorMouse.containsMouse
                                ? (modManager.theme === "Glassmorphic" ? window.glassSurfaceHover : Qt.lighter(window.surfaceColor, 1.06))
                                : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
                            border.width: 1
                            border.color: processSelectorMouse.containsMouse ? window.primaryColor : window.borderColor
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: modManager.selectedProcessPid > 0
                                        ? (modManager.selectedProcessName + "   [PID: " + modManager.selectedProcessPid + "]")
                                        : "Select a running process..."
                                    font.pixelSize: 13
                                    font.weight: modManager.selectedProcessPid > 0 ? Font.DemiBold : Font.Normal
                                    color: modManager.selectedProcessPid > 0 ? window.textColor : window.textSecondaryColor
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }

                                Text {
                                    text: processDropdownPopup.visible ? "▲" : "▼"
                                    font.pixelSize: 10
                                    color: window.textSecondaryColor
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            MouseArea {
                                id: processSelectorMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (processDropdownPopup.visible) {
                                        processDropdownPopup.close()
                                    } else if (Date.now() - runtimeTab.processPopupClosedAt > 250) {
                                        runtimeTab.refreshRunningProcesses()
                                        processDropdownPopup.open()
                                    }
                                }
                            }

                            Popup {
                                id: processDropdownPopup
                                x: 0
                                y: processSelectorBtn.height + 6
                                width: processSelectorBtn.width + refreshProcessesBtn.width + 10
                                height: 340
                                padding: 0
                                focus: true
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                                onClosed: {
                                    runtimeTab.processPopupClosedAt = Date.now()
                                }

                                background: Rectangle {
                                    radius: 16
                                    color: modManager.theme === "Glassmorphic"
                                        ? Qt.rgba(0.10, 0.10, 0.15, 0.98)
                                        : (modManager.theme === "Wavey" ? Qt.rgba(0.08, 0.16, 0.28, 0.97)
                                        : window.surfaceColor)
                                    border.width: 1.5
                                    border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.25) : window.borderColor

                                    layer.enabled: true
                                    layer.effect: MultiEffect {
                                        shadowEnabled: true
                                        shadowColor: Qt.rgba(0, 0, 0, 0.45)
                                        shadowHorizontalOffset: 0
                                        shadowVerticalOffset: 10
                                        shadowBlur: 0.8
                                    }
                                }

                                contentItem: ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 8

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 36
                                        radius: 12
                                        color: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(1, 1, 1, 0.08)
                                            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06)
                                        border.width: 1
                                        border.color: processSearchInput.activeFocus ? window.primaryColor : window.borderColor

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 12
                                            anchors.rightMargin: 12
                                            spacing: 8

                                            TextInput {
                                                id: processSearchInput
                                                Layout.fillWidth: true
                                                text: runtimeTab.processSearchText
                                                font.pixelSize: 13
                                                color: window.textColor
                                                verticalAlignment: TextInput.AlignVCenter
                                                clip: true
                                                selectByMouse: true

                                                Text {
                                                    anchors.fill: parent
                                                    text: "Search by process name or PID..."
                                                    font.pixelSize: 13
                                                    color: window.textSecondaryColor
                                                    verticalAlignment: Text.AlignVCenter
                                                    visible: !processSearchInput.text && !processSearchInput.activeFocus
                                                }

                                                onTextChanged: {
                                                    runtimeTab.processSearchText = text
                                                }
                                            }

                                            Text {
                                                text: "Clear"
                                                font.pixelSize: 11
                                                font.weight: Font.DemiBold
                                                color: clearSearchMouse.containsMouse ? window.primaryColor : window.textSecondaryColor
                                                visible: processSearchInput.text.length > 0
                                                verticalAlignment: Text.AlignVCenter

                                                MouseArea {
                                                    id: clearSearchMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        processSearchInput.text = ""
                                                        runtimeTab.processSearchText = ""
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 1
                                        color: Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.4)
                                    }

                                    ListView {
                                        id: processListView
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        clip: true
                                        model: runtimeTab.filteredProcesses
                                        spacing: 4

                                        ScrollBar.vertical: ScrollBar {
                                            policy: ScrollBar.AsNeeded
                                        }

                                        delegate: Rectangle {
                                            id: processDelegateItem
                                            width: processListView.width - (processListView.contentHeight > processListView.height ? 12 : 0)
                                            height: (modelData.title && modelData.title.length > 0) ? 52 : 40
                                            radius: 10
                                            color: delegateMouse.containsMouse
                                                ? (modManager.theme === "Glassmorphic"
                                                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.18)
                                                    : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12))
                                                : (modManager.selectedProcessPid === modelData.pid
                                                    ? (modManager.theme === "Glassmorphic"
                                                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10)
                                                        : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08))
                                                    : "transparent")
                                            border.width: modManager.selectedProcessPid === modelData.pid ? 1 : 0
                                            border.color: window.primaryColor
                                            Behavior on color { ColorAnimation { duration: 120 } }

                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 10
                                                anchors.rightMargin: 10
                                                anchors.topMargin: 4
                                                anchors.bottomMargin: 4
                                                spacing: 2

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 8

                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: modelData.name || "Unknown"
                                                        font.pixelSize: 13
                                                        font.weight: modManager.selectedProcessPid === modelData.pid ? Font.Bold : Font.DemiBold
                                                        color: modManager.selectedProcessPid === modelData.pid
                                                            ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor)
                                                            : window.textColor
                                                        elide: Text.ElideRight
                                                    }

                                                    Rectangle {
                                                        Layout.preferredHeight: 20
                                                        Layout.preferredWidth: pidText.implicitWidth + 12
                                                        radius: 10
                                                        color: modManager.theme === "Glassmorphic"
                                                            ? Qt.rgba(1, 1, 1, 0.12)
                                                            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08)

                                                        Text {
                                                            id: pidText
                                                            anchors.centerIn: parent
                                                            text: "PID: " + modelData.pid
                                                            font.pixelSize: 11
                                                            font.weight: Font.Medium
                                                            color: window.textSecondaryColor
                                                        }
                                                    }
                                                }

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: modelData.title || modelData.path || ""
                                                    font.pixelSize: 11
                                                    color: window.textSecondaryColor
                                                    opacity: 0.8
                                                    elide: Text.ElideMiddle
                                                    visible: text.length > 0
                                                }
                                            }

                                            MouseArea {
                                                id: delegateMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    modManager.setSelectedProcess(modelData.pid, modelData.name, modelData.path || "")
                                                    processDropdownPopup.close()
                                                    toastNotification.show("Selected: " + modelData.name + " (" + modelData.pid + ")")
                                                }
                                            }
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: runtimeTab.runningProcessesList.length === 0 ? "No running processes found" : "No matching processes"
                                            font.pixelSize: 13
                                            color: window.textSecondaryColor
                                            visible: processListView.count === 0
                                        }
                                    }
                                }
                            }
                        }

                        Button {
                            id: refreshProcessesBtn
                            visible: modManager.isAdvancedProcessMode
                            text: "Refresh"
                            Layout.preferredWidth: 80
                            Layout.preferredHeight: 36
                            flat: true

                            contentItem: Text {
                                text: parent.text
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: "#FFFFFF"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                color: parent.hovered ? Qt.lighter(window.primaryColor, 1.15) : window.primaryColor
                                radius: 18
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }

                            onClicked: {
                                runtimeTab.refreshRunningProcesses()
                                toastNotification.show("Process list refreshed")
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            radius: 17
                            color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(1, 1, 1, 0.08)
                                : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05)
                            border.width: 1
                            border.color: window.borderColor

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 8

                                Text {
                                    text: "Output:"
                                    font.pixelSize: 11
                                    font.weight: Font.Bold
                                    color: window.primaryColor
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: outputPath || "Select runtime output folder..."
                                    font.pixelSize: 12
                                    color: outputPath ? window.textColor : window.textSecondaryColor
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideMiddle
                                }
                            }
                        }

                        Button {
                            id: browseOutputButton
                            enabled: !modManager.initializing && !modManager.generating
                            text: "Browse"
                            Layout.preferredWidth: 74
                            Layout.preferredHeight: 34
                            flat: true
                            focusPolicy: Qt.NoFocus

                            contentItem: Text {
                                text: parent.text
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                color: "#FFFFFF"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                color: parent.hovered ? Qt.lighter(window.primaryColor, 1.15) : window.primaryColor
                                radius: 17
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }

                            onClicked: runtimeOutputPathDialog.open()
                        }

                        Button {
                            id: runtimeConfigureButton
                            enabled: !modManager.generating
                            text: "Configure"
                            Layout.preferredWidth: 84
                            Layout.preferredHeight: 34
                            flat: true
                            focusPolicy: Qt.NoFocus

                            contentItem: Text {
                                text: parent.text
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                color: modManager.theme === "Cozy" ? "#FFFFFF" : ((modManager.theme === "Crimson" || modManager.theme === "Wavey") ? "#FFFFFF" : (parent.hovered ? window.primaryColor : window.textColor))
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                color: {
                                    if (modManager.theme === "Cozy")
                                        return parent.hovered ? "#453400" : "#301b00"
                                    if (modManager.theme === "Glassmorphic")
                                        return parent.hovered ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.20) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)
                                    if (modManager.theme === "Crimson")
                                        return parent.hovered ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.30) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                                    if (modManager.theme === "Wavey")
                                        return parent.hovered ? "#3860A5FA" : "#2460A5FA"
                                    return parent.hovered ? Qt.rgba(0, 0, 0, 0.04) : "transparent"
                                }
                                border.width: modManager.theme === "Cozy" ? 0 : 1
                                border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.40) : (modManager.theme === "Crimson" ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.55) : (modManager.theme === "Wavey" ? "#7360A5FA" : (parent.hovered ? window.primaryColor : Qt.rgba(0, 0, 0, 0.18))))
                                radius: 17
                                Behavior on color { ColorAnimation { duration: 150 } }
                                Behavior on border.color { ColorAnimation { duration: 150 } }
                            }

                            onClicked: runtimeConfigDialog.openWithConfig()
                        }

                        Rectangle {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            radius: 17
                            color: runtimeConfigHelpMouse.containsMouse
                                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                                : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08)
                            border.width: 1
                            border.color: runtimeConfigHelpMouse.containsMouse ? window.primaryColor : window.borderColor

                            Image {
                                id: runtimeConfigHelpIcon
                                anchors.centerIn: parent
                                width: 15
                                height: 15
                                source: "qrc:/UniversalModManager/Lucide Icons/circle-question-mark.svg"
                                sourceSize: Qt.size(32, 32)
                                visible: modManager.theme === "Cozy"
                            }

                            ColorOverlay {
                                anchors.fill: runtimeConfigHelpIcon
                                source: runtimeConfigHelpIcon
                                color: window.primaryColor
                                visible: modManager.theme !== "Cozy"
                            }

                            MouseArea {
                                id: runtimeConfigHelpMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: runtimeTab.openRuntimeConfigHelp()
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: commandCenterCard
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                radius: 18
                color: modManager.theme === "Wavey" ? Qt.rgba(0.08, 0.16, 0.28, 0.94) : window.surfaceColor
                border.width: window.borderWidth
                border.color: window.borderColor

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Button {
                        id: launchGameButton
                        enabled: (!modManager.initializing || initializationDialog.gameStillRunningAfterError) && !modManager.generating
                        text: "Launch Game"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        flat: true

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            color: "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: parent.hovered ? Qt.lighter(window.primaryColor, 1.15) : window.primaryColor
                            radius: 14
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: {
                            if (modManager.isAdvancedProcessMode) {
                                runtimeAlertDialog.showMessage(
                                    "Advanced Injection is Active",
                                    "Disable Advanced Injection to launch the game. This mode targets already-running processes — start your game first, then inject."
                                )
                                return
                            }
                            if (initializationDialog.gameStillRunningAfterError) {
                                if (modManager.isGameProcessRunning()) {
                                    unsupportedVersionGameRunningDialog.open()
                                    return
                                }
                                initializationDialog.gameStillRunningAfterError = false
                            } else if (modManager.isGameProcessRunning()) {
                                runtimeAlertDialog.showMessage("Game Still Running", "Game is already running")
                                return
                            }
                            var msg = modManager.launchGame()
                            if (msg && msg !== "") {
                                toastNotification.show(msg)
                            }
                        }
                    }

                    Button {
                        id: injectDllButton
                        enabled: (!modManager.initializing || initializationDialog.gameStillRunningAfterError) && !modManager.generating
                        text: "Inject Avalonium"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        flat: true

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            color: "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: parent.hovered ? Qt.lighter(window.primaryColor, 1.18) : window.primaryColor
                            radius: 14
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: {
                            if (initializationDialog.gameStillRunningAfterError) {
                                if (modManager.isGameProcessRunning()) {
                                    unsupportedVersionGameRunningDialog.open()
                                    return
                                }
                                initializationDialog.gameStillRunningAfterError = false
                            } else if (isAvaloniumInjected()) {
                                runtimeAlertDialog.showMessage("Already Injected", "Avalonium is already injected")
                                return
                            }
                            initializationDialog.animationRestartTrigger++
                            var msg = modManager.injectAvaloniumDll()
                            if (msg && msg !== "") {
                                toastNotification.show(msg)
                            }
                        }
                    }

                    Button {
                        id: generateUsmapButton
                        enabled: (!modManager.initializing || initializationDialog.gameStillRunningAfterError) && !modManager.generating
                        text: "Generate USMAP"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        flat: true
                        focusPolicy: Qt.NoFocus

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: modManager.theme === "Cozy" ? "#FFFFFF" : ((modManager.theme === "Crimson" || modManager.theme === "Wavey") ? "#FFFFFF" : (parent.hovered ? window.primaryColor : window.textColor))
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: {
                                if (modManager.theme === "Cozy")
                                    return parent.hovered ? "#453400" : "#301b00"
                                if (modManager.theme === "Glassmorphic")
                                    return parent.hovered ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.22) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.14)
                                if (modManager.theme === "Crimson")
                                    return parent.hovered ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.30) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                                if (modManager.theme === "Wavey")
                                    return parent.hovered ? "#3860A5FA" : "#2460A5FA"
                                return parent.hovered ? Qt.rgba(0, 0, 0, 0.05) : "transparent"
                            }
                            border.width: modManager.theme === "Cozy" ? 0 : 1
                            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.40) : (modManager.theme === "Crimson" ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.55) : (modManager.theme === "Wavey" ? "#7360A5FA" : (parent.hovered ? window.primaryColor : Qt.rgba(0, 0, 0, 0.18))))
                            radius: 14
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: {
                            if (initializationDialog.gameStillRunningAfterError) {
                                runtimeAlertDialog.showMessage(
                                    "Unsupported Engine Version",
                                    "This action is not supported for the detected engine version. Avalonium requires Unreal Engine 4.23 or newer."
                                )
                                return
                            }
                            modManager.generateUsmap()
                        }
                    }

                    Button {
                        id: generateSdkButton
                        enabled: (!modManager.initializing || initializationDialog.gameStillRunningAfterError) && !modManager.generating
                        text: "Generate SDK"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        flat: true
                        focusPolicy: Qt.NoFocus

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: modManager.theme === "Cozy" ? "#FFFFFF" : ((modManager.theme === "Crimson" || modManager.theme === "Wavey") ? "#FFFFFF" : (parent.hovered ? window.primaryColor : window.textColor))
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: {
                                if (modManager.theme === "Cozy")
                                    return parent.hovered ? "#453400" : "#301b00"
                                if (modManager.theme === "Glassmorphic")
                                    return parent.hovered ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.22) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.14)
                                if (modManager.theme === "Crimson")
                                    return parent.hovered ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.30) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                                if (modManager.theme === "Wavey")
                                    return parent.hovered ? "#3860A5FA" : "#2460A5FA"
                                return parent.hovered ? Qt.rgba(0, 0, 0, 0.05) : "transparent"
                            }
                            border.width: modManager.theme === "Cozy" ? 0 : 1
                            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.40) : (modManager.theme === "Crimson" ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.55) : (modManager.theme === "Wavey" ? "#7360A5FA" : (parent.hovered ? window.primaryColor : Qt.rgba(0, 0, 0, 0.18))))
                            radius: 14
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: {
                            if (initializationDialog.gameStillRunningAfterError) {
                                runtimeAlertDialog.showMessage(
                                    "Unsupported Engine Version",
                                    "This action is not supported for the detected engine version. Avalonium requires Unreal Engine 4.23 or newer."
                                )
                                return
                            }
                            modManager.generateSdk()
                        }
                    }
                }
            }

            Rectangle {
                id: studioDeckCard
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 20
                color: modManager.theme === "Wavey" ? Qt.rgba(0.08, 0.16, 0.28, 0.94) : window.surfaceColor
                border.width: window.borderWidth
                border.color: window.borderColor

                layer.enabled: modManager.theme !== "Glassmorphic"
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(0, 0, 0, 0.2)
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 3
                    shadowBlur: 0.3
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 280
                            Layout.preferredHeight: 34
                            radius: 17
                            color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(1, 1, 1, 0.08)
                                : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06)
                            border.width: 1
                            border.color: window.borderColor

                            RowLayout {
                                anchors.fill: parent
                                spacing: 0

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: 17
                                    color: studioViewMode === 0
                                        ? (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.30) : window.primaryColor)
                                        : "transparent"
                                    Behavior on color { ColorAnimation { duration: 150 } }

                                    Text {
                                        anchors.centerIn: parent
                                        text: "Engine Globals"
                                        font.pixelSize: 12
                                        font.weight: studioViewMode === 0 ? Font.Bold : Font.Medium
                                        color: studioViewMode === 0 ? "#FFFFFF" : window.textSecondaryColor
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: studioViewMode = 0
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: 17
                                    color: studioViewMode === 1
                                        ? (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.30) : window.primaryColor)
                                        : "transparent"
                                    Behavior on color { ColorAnimation { duration: 150 } }

                                    Text {
                                        anchors.centerIn: parent
                                        text: "Output Explorer"
                                        font.pixelSize: 12
                                        font.weight: studioViewMode === 1 ? Font.Bold : Font.Medium
                                        color: studioViewMode === 1 ? "#FFFFFF" : window.textSecondaryColor
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: studioViewMode = 1
                                    }
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        RowLayout {
                            visible: studioViewMode === 1
                            spacing: 4

                            Button {
                                id: backButton
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                flat: true
                                enabled: explorerPanel.historyIndex > 0 && !modManager.generating

                                contentItem: Canvas {
                                    width: 14
                                    height: 14
                                    readonly property color strokeColor: backButton.enabled
                                        ? (modManager.theme === "Glassmorphic" ? window.accentColor : window.textColor)
                                        : window.textSecondaryColor
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.clearRect(0, 0, 14, 14)
                                        ctx.strokeStyle = strokeColor
                                        ctx.lineWidth = 2
                                        ctx.lineCap = "round"
                                        ctx.lineJoin = "round"
                                        ctx.beginPath()
                                        ctx.moveTo(9, 2)
                                        ctx.lineTo(4, 7)
                                        ctx.lineTo(9, 12)
                                        ctx.stroke()
                                    }
                                    onEnabledChanged: requestPaint()
                                    onStrokeColorChanged: requestPaint()
                                    Component.onCompleted: requestPaint()
                                }
                                background: Rectangle { color: "transparent"; radius: 6 }
                                onClicked: explorerPanel.goBack()
                            }

                            Button {
                                id: forwardButton
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                flat: true
                                enabled: explorerPanel.historyIndex < explorerPanel.folderHistory.length - 1 && !modManager.generating

                                contentItem: Canvas {
                                    width: 14
                                    height: 14
                                    readonly property color strokeColor: forwardButton.enabled
                                        ? (modManager.theme === "Glassmorphic" ? window.accentColor : window.textColor)
                                        : window.textSecondaryColor
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.clearRect(0, 0, 14, 14)
                                        ctx.strokeStyle = strokeColor
                                        ctx.lineWidth = 2
                                        ctx.lineCap = "round"
                                        ctx.lineJoin = "round"
                                        ctx.beginPath()
                                        ctx.moveTo(5, 2)
                                        ctx.lineTo(10, 7)
                                        ctx.lineTo(5, 12)
                                        ctx.stroke()
                                    }
                                    onEnabledChanged: requestPaint()
                                    onStrokeColorChanged: requestPaint()
                                    Component.onCompleted: requestPaint()
                                }
                                background: Rectangle { color: "transparent"; radius: 6 }
                                onClicked: explorerPanel.goForward()
                            }

                            Button {
                                id: upButton
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                flat: true
                                enabled: explorerPanel.currentFolder.length > 0 && !modManager.generating

                                contentItem: Canvas {
                                    width: 14
                                    height: 14
                                    readonly property color strokeColor: upButton.enabled
                                        ? (modManager.theme === "Glassmorphic" ? window.accentColor : window.textColor)
                                        : window.textSecondaryColor
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.clearRect(0, 0, 14, 14)
                                        ctx.strokeStyle = strokeColor
                                        ctx.lineWidth = 2
                                        ctx.lineCap = "round"
                                        ctx.lineJoin = "round"
                                        ctx.beginPath()
                                        ctx.moveTo(2, 9)
                                        ctx.lineTo(7, 4)
                                        ctx.lineTo(12, 9)
                                        ctx.stroke()
                                    }
                                    onEnabledChanged: requestPaint()
                                    onStrokeColorChanged: requestPaint()
                                    Component.onCompleted: requestPaint()
                                }
                                background: Rectangle { color: "transparent"; radius: 6 }
                                onClicked: explorerPanel.goUp()
                            }
                        }

                        Button {
                            visible: studioViewMode === 0 && globalsStatus && globalsStatus.length > 0
                            text: "Copy Globals"
                            Layout.preferredHeight: 28
                            Layout.preferredWidth: 92
                            flat: true
                            contentItem: Text {
                                text: parent.text
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                color: window.textSecondaryColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 8
                                color: parent.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                                border.width: 1
                                border.color: window.borderColor
                            }
                            onClicked: {
                                modManager.copyTextToClipboard(globalsStatus)
                                toastNotification.show("Globals copied to clipboard")
                            }
                        }
                    }

                    Item {
                        id: globalsView
                        visible: studioViewMode === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 8
                            visible: !currentGlobalsData || globalsStatus.length === 0

                            Text {
                                text: "No Active Engine Telemetry"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: window.textColor
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Text {
                                text: "Inject Avalonium into the game to discover engine globals, memory pointers, and layout offsets."
                                font.pixelSize: 12
                                color: window.textSecondaryColor
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                Layout.preferredWidth: 360
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }

                        ScrollView {
                            id: globalsScrollView
                            anchors.fill: parent
                            clip: true
                            visible: currentGlobalsData && globalsStatus.length > 0
                            contentWidth: availableWidth

                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                            ScrollBar.vertical: ScrollBar {
                                id: globalsVScrollBar
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.rightMargin: 2
                                width: 6
                                policy: ScrollBar.AsNeeded

                                contentItem: Rectangle {
                                    implicitWidth: 6
                                    implicitHeight: 60
                                    radius: 3
                                    color: globalsVScrollBar.pressed
                                        ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor)
                                        : Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g, window.textSecondaryColor.b,
                                                  globalsVScrollBar.hovered ? 0.6 : 0.35)
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }

                                background: Rectangle {
                                    implicitWidth: 6
                                    radius: 3
                                    color: Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g, window.textSecondaryColor.b, 0.08)
                                }
                            }

                            ColumnLayout {
                                width: globalsScrollView.availableWidth - 12
                                spacing: 14

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    visible: currentGlobalsData && currentGlobalsData.core.length > 0

                                    Text {
                                        text: "CORE ENGINE"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor
                                        opacity: 0.9
                                    }

                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Repeater {
                                            model: currentGlobalsData ? currentGlobalsData.core : []
                                            delegate: Rectangle {
                                                height: 30
                                                width: chipRow.implicitWidth + 20
                                                radius: 8
                                                color: modManager.theme === "Glassmorphic"
                                                    ? Qt.rgba(1, 1, 1, 0.08)
                                                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05)
                                                border.width: 1
                                                border.color: modelData.key === "Initialized" && modelData.val === "YES"
                                                    ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor)
                                                    : window.borderColor

                                                RowLayout {
                                                    id: chipRow
                                                    anchors.centerIn: parent
                                                    spacing: 6

                                                    Text {
                                                        text: modelData.key + ":"
                                                        font.pixelSize: 11
                                                        font.weight: Font.DemiBold
                                                        color: window.textSecondaryColor
                                                    }

                                                    Text {
                                                        text: modelData.val
                                                        font.pixelSize: 12
                                                        font.weight: Font.Bold
                                                        color: modelData.key === "Initialized" && modelData.val === "YES"
                                                            ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor)
                                                            : (modelData.key === "Version" ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor) : window.textColor)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    visible: currentGlobalsData && currentGlobalsData.pointers.length > 0

                                    Text {
                                        text: "ENGINE MEMORY POINTERS"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor
                                        opacity: 0.9
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        Repeater {
                                            model: currentGlobalsData ? currentGlobalsData.pointers : []
                                            delegate: Rectangle {
                                                Layout.fillWidth: true
                                                height: 30
                                                radius: 8
                                                color: ptrMouse.containsMouse
                                                    ? (modManager.theme === "Glassmorphic"
                                                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.18)
                                                        : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12))
                                                    : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.04))
                                                border.width: 1
                                                border.color: ptrMouse.containsMouse
                                                    ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor)
                                                    : window.borderColor
                                                Behavior on color { ColorAnimation { duration: 120 } }
                                                Behavior on border.color { ColorAnimation { duration: 120 } }

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 10
                                                    anchors.rightMargin: 10
                                                    spacing: 8

                                                    Text {
                                                        text: modelData.key
                                                        font.pixelSize: 11
                                                        font.weight: Font.DemiBold
                                                        color: window.textColor
                                                    }

                                                    Item { Layout.fillWidth: true }

                                                    Text {
                                                        text: modelData.val
                                                        font.pixelSize: 11
                                                        font.family: "Consolas"
                                                        font.weight: Font.Medium
                                                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor
                                                    }
                                                }

                                                MouseArea {
                                                    id: ptrMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        modManager.copyTextToClipboard(modelData.val)
                                                        toastNotification.show("Copied " + modelData.key + ": " + modelData.val)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    visible: currentGlobalsData && currentGlobalsData.classes.length > 0

                                    Text {
                                        text: "CORE ENGINE CLASSES"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor
                                        opacity: 0.9
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        Repeater {
                                            model: currentGlobalsData ? currentGlobalsData.classes : []
                                            delegate: Rectangle {
                                                Layout.fillWidth: true
                                                height: 30
                                                radius: 8
                                                color: classMouse.containsMouse
                                                    ? (modManager.theme === "Glassmorphic"
                                                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.18)
                                                        : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12))
                                                    : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.04))
                                                border.width: 1
                                                border.color: classMouse.containsMouse
                                                    ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor)
                                                    : window.borderColor
                                                Behavior on color { ColorAnimation { duration: 120 } }
                                                Behavior on border.color { ColorAnimation { duration: 120 } }

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 10
                                                    anchors.rightMargin: 10
                                                    spacing: 8

                                                    Text {
                                                        text: modelData.key
                                                        font.pixelSize: 11
                                                        font.weight: Font.DemiBold
                                                        color: window.textColor
                                                    }

                                                    Item { Layout.fillWidth: true }

                                                    Text {
                                                        text: modelData.val
                                                        font.pixelSize: 11
                                                        font.family: "Consolas"
                                                        font.weight: Font.Medium
                                                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor
                                                    }
                                                }

                                                MouseArea {
                                                    id: classMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        modManager.copyTextToClipboard(modelData.val)
                                                        toastNotification.show("Copied " + modelData.key + ": " + modelData.val)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    visible: currentGlobalsData && (currentGlobalsData.sdk.length > 0 || currentGlobalsData.layout.length > 0)

                                    Text {
                                        text: "SDK & STRUCT OFFSETS"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor
                                        opacity: 0.9
                                    }

                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Repeater {
                                            model: (currentGlobalsData ? currentGlobalsData.sdk : []).concat(currentGlobalsData ? currentGlobalsData.layout : [])
                                            delegate: Rectangle {
                                                height: 28
                                                width: sdkRow.implicitWidth + 16
                                                radius: 8
                                                color: modManager.theme === "Glassmorphic"
                                                    ? Qt.rgba(1, 1, 1, 0.06)
                                                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.04)
                                                border.width: 1
                                                border.color: modelData.val === "YES"
                                                    ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.35)
                                                    : window.borderColor

                                                RowLayout {
                                                    id: sdkRow
                                                    anchors.centerIn: parent
                                                    spacing: 6

                                                    Text {
                                                        text: modelData.key + ":"
                                                        font.pixelSize: 11
                                                        color: window.textSecondaryColor
                                                    }

                                                    Text {
                                                        text: modelData.val
                                                        font.pixelSize: 11
                                                        font.family: "Consolas"
                                                        font.weight: Font.DemiBold
                                                        color: modelData.val === "YES"
                                                            ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor)
                                                            : window.textColor
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    visible: currentGlobalsData && currentGlobalsData.other.length > 0

                                    Text {
                                        text: "OTHER INFORMATION"
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor
                                        opacity: 0.9
                                    }

                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Repeater {
                                            model: currentGlobalsData ? currentGlobalsData.other : []
                                            delegate: Rectangle {
                                                height: 28
                                                width: otherRow.implicitWidth + 16
                                                radius: 8
                                                color: modManager.theme === "Glassmorphic"
                                                    ? Qt.rgba(1, 1, 1, 0.06)
                                                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.04)
                                                border.width: 1
                                                border.color: modelData.val === "YES"
                                                    ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.35)
                                                    : window.borderColor

                                                RowLayout {
                                                    id: otherRow
                                                    anchors.centerIn: parent
                                                    spacing: 6

                                                    Text {
                                                        text: modelData.key ? (modelData.key + ":") : ""
                                                        font.pixelSize: 11
                                                        color: window.textSecondaryColor
                                                        visible: modelData.key.length > 0
                                                    }

                                                    Text {
                                                        text: modelData.val
                                                        font.pixelSize: 11
                                                        font.weight: Font.Medium
                                                        color: modelData.val === "YES"
                                                            ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor)
                                                            : window.textColor
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        id: explorerPanel
                        visible: studioViewMode === 1
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        property string currentFolder: runtimeTab.outputPath || ""
                        property var folderHistory: []
                        property int historyIndex: -1

                        onCurrentFolderChanged: {
                            if (explorerPanel.currentFolder.length > 0 && explorerPanel.folderHistory.length === 0) {
                                explorerPanel.folderHistory.push(explorerPanel.currentFolder)
                                explorerPanel.historyIndex = 0
                            }
                        }

                        Connections {
                            target: runtimeTab
                            function onOutputPathChanged() {
                                explorerPanel.folderHistory = []
                                explorerPanel.historyIndex = -1
                                explorerPanel.currentFolder = runtimeTab.outputPath || ""
                            }
                        }

                        function navigateToFolder(path) {
                            if (path === currentFolder) return
                            if (historyIndex < folderHistory.length - 1) {
                                folderHistory = folderHistory.slice(0, historyIndex + 1)
                            }
                            folderHistory.push(path)
                            historyIndex = folderHistory.length - 1
                            currentFolder = path
                        }

                        function goBack() {
                            if (historyIndex > 0) {
                                historyIndex--
                                currentFolder = folderHistory[historyIndex]
                            }
                        }

                        function goForward() {
                            if (historyIndex < folderHistory.length - 1) {
                                historyIndex++
                                currentFolder = folderHistory[historyIndex]
                            }
                        }

                        function goUp() {
                            if (currentFolder.length === 0) return
                            var parts = currentFolder.replace(/\\/g, "/").split("/").filter(function(s) { return s.length > 0 })
                            if (parts.length <= 1) return
                            parts.pop()
                            var parentPath = parts.join("/")
                            if (currentFolder.startsWith("file:///")) {
                                parentPath = "file:///" + parentPath
                            }
                            navigateToFolder(parentPath)
                        }

                        FolderListModel {
                            id: runtimeOutputFolderModel
                            folder: explorerPanel.currentFolder.length > 0
                                    ? ("file:///" + explorerPanel.currentFolder.replace(/\\/g, "/"))
                                    : ""
                            showDirs: true
                            showDotAndDotDot: false
                            sortField: FolderListModel.Name
                        }

                        Text {
                            anchors.centerIn: parent
                            width: parent.width - 32
                            visible: !(runtimeTab.outputPath && runtimeTab.outputPath.length > 0) || runtimeOutputFolderModel.count === 0
                            text: !(runtimeTab.outputPath && runtimeTab.outputPath.length > 0)
                                  ? "Set an output path above to browse generated files."
                                  : "No files found in the current directory."
                            font.pixelSize: 12
                            color: window.textSecondaryColor
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                        }

                        ListView {
                            id: runtimeOutputExplorerList
                            anchors.fill: parent
                            model: runtimeOutputFolderModel
                            clip: true
                            spacing: 4
                            visible: (runtimeTab.outputPath && runtimeTab.outputPath.length > 0) && runtimeOutputFolderModel.count > 0

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                            }

                            WheelHandler {
                                target: null
                                onWheel: function(event) {
                                    var delta = -event.angleDelta.y
                                    var multiplier = 1.5
                                    if (event.modifiers & Qt.ShiftModifier) multiplier = 5.0
                                    else if (event.modifiers & Qt.ControlModifier) multiplier = 15.0
                                    var scrollAmount = (runtimeOutputExplorerList.height * 0.35) * multiplier * (delta / 120.0)
                                    var maxContentY = runtimeOutputExplorerList.contentHeight - runtimeOutputExplorerList.height
                                    var targetY = Math.max(0, Math.min(runtimeOutputExplorerList.contentY + scrollAmount, maxContentY))
                                    explorerScrollAnimation.stop()
                                    explorerScrollAnimation.from = runtimeOutputExplorerList.contentY
                                    explorerScrollAnimation.to = targetY
                                    explorerScrollAnimation.duration = 80
                                    explorerScrollAnimation.start()
                                    event.accepted = true
                                }
                            }

                            NumberAnimation {
                                id: explorerScrollAnimation
                                target: runtimeOutputExplorerList
                                property: "contentY"
                                easing.type: Easing.OutCubic
                            }

                            delegate: Rectangle {
                                id: explorerItem
                                width: ListView.view ? ListView.view.width : 0
                                height: 32
                                radius: 8
                                color: explorerItem.ListView.isCurrentItem
                                       ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.16)
                                       : (itemMouse.containsMouse ? (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.04)) : "transparent")
                                border.width: explorerItem.ListView.isCurrentItem ? 1 : 0
                                border.color: window.primaryColor

                                property bool isDirectory: model.fileIsDir || false

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8

                                    Item {
                                        Layout.preferredWidth: 20
                                        Layout.preferredHeight: 20

                                        Image {
                                            id: fileIconImage
                                            anchors.centerIn: parent
                                            width: 18
                                            height: 18
                                            sourceSize: Qt.size(18, 18)
                                            smooth: true
                                            antialiasing: true
                                            fillMode: Image.PreserveAspectFit
                                            source: isDirectory
                                                ? "qrc:/UniversalModManager/Lucide Icons/folder.svg"
                                                : (fileName === "Avalonium.log"
                                                    ? "qrc:/UniversalModManager/Lucide Icons/code.svg"
                                                    : (fileName && fileName.toLowerCase().endsWith(".usmap")
                                                        ? "qrc:/UniversalModManager/Lucide Icons/map.svg"
                                                        : "qrc:/UniversalModManager/Lucide Icons/file.svg"))
                                            visible: false
                                        }

                                        ColorOverlay {
                                            anchors.fill: fileIconImage
                                            source: fileIconImage
                                            color: isDirectory ? window.accentColor : window.textSecondaryColor
                                            smooth: true
                                            antialiasing: true
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: fileName
                                        font.pixelSize: 12
                                        color: explorerItem.ListView.isCurrentItem ? window.textColor : window.textSecondaryColor
                                        font.weight: isDirectory ? Font.DemiBold : Font.Normal
                                        elide: Text.ElideRight
                                    }
                                }

                                MouseArea {
                                    id: itemMouse
                                    enabled: !modManager.generating
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: isDirectory ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: {
                                        runtimeOutputExplorerList.currentIndex = index
                                    }
                                    onDoubleClicked: {
                                        if (isDirectory) {
                                            var currentPath = explorerPanel.currentFolder
                                            var newPath = currentPath + "/" + fileName
                                            explorerPanel.navigateToFolder(newPath)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Rectangle {
                id: logCard
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 20
                color: modManager.theme === "Wavey" ? Qt.rgba(0.08, 0.16, 0.28, 0.94) : window.surfaceColor
                border.width: window.borderWidth
                border.color: modManager.theme === "Glassmorphic"
                    ? Qt.rgba(1, 1, 1, 0.18)
                    : window.borderColor

                layer.enabled: modManager.theme !== "Glassmorphic"
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(0, 0, 0, 0.2)
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 3
                    shadowBlur: 0.3
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: "Live Runtime Console"
                            font.pixelSize: 13
                            font.weight: Font.Bold
                            color: window.textColor
                        }

                        Rectangle {
                            id: backgroundProgressBar
                            visible: modManager.initializing && initializationDialog.waitingInBackground
                            Layout.preferredWidth: 160
                            Layout.preferredHeight: 6
                            color: Qt.rgba(44/255, 44/255, 44/255, 1.0)
                            radius: 3
                            clip: true

                            Rectangle {
                                id: progressOverlay
                                anchors.fill: parent
                                radius: parent.radius
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 1.0) }
                                    GradientStop { position: 0.3; color: Qt.rgba(0, 0, 0, 0.14) }
                                    GradientStop { position: 0.7; color: Qt.rgba(0, 0, 0, 0.14) }
                                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 1.0) }
                                }
                            }

                            Rectangle {
                                id: progressLight
                                width: 60
                                height: parent.height
                                x: -60
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0) }
                                    GradientStop { position: 0.4; color: window.primaryColor }
                                    GradientStop { position: 0.6; color: window.primaryColor }
                                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0) }
                                }

                                SequentialAnimation on x {
                                    loops: Animation.Infinite
                                    running: backgroundProgressBar.visible
                                    NumberAnimation {
                                        from: -60
                                        to: backgroundProgressBar.width
                                        duration: 1800
                                        easing.type: Easing.InOutQuad
                                    }
                                    PauseAnimation { duration: 200 }
                                }
                            }
                        }

                        Button {
                            id: killProcessBarButton
                            visible: modManager.initializing && initializationDialog.waitingInBackground
                            enabled: !modManager.generating
                            text: "Kill process"
                            Layout.preferredWidth: 84
                            Layout.preferredHeight: 26
                            flat: true
                            focusPolicy: Qt.NoFocus

                            contentItem: Text {
                                text: parent.text
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                color: modManager.theme === "Cozy" ? "#FFFFFF" : ((modManager.theme === "Crimson" || modManager.theme === "Wavey") ? "#FFFFFF" : (parent.hovered ? window.primaryColor : window.textColor))
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                color: {
                                    if (modManager.theme === "Cozy")
                                        return parent.hovered ? "#453400" : "#301b00"
                                    if (modManager.theme === "Glassmorphic")
                                        return parent.hovered ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.20) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)
                                    if (modManager.theme === "Crimson")
                                        return parent.hovered ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.30) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                                    if (modManager.theme === "Wavey")
                                        return parent.hovered ? "#3860A5FA" : "#2460A5FA"
                                    return parent.hovered ? Qt.rgba(0, 0, 0, 0.04) : "transparent"
                                }
                                border.width: modManager.theme === "Cozy" ? 0 : 1
                                border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.40) : (modManager.theme === "Crimson" ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.55) : (modManager.theme === "Wavey" ? "#7360A5FA" : (parent.hovered ? window.primaryColor : Qt.rgba(0, 0, 0, 0.18))))
                                radius: 13
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }

                            onClicked: {
                                modManager.killGameProcess()
                                initializationDialog.waitingInBackground = false
                                toastNotification.show("Game process terminated")
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            id: logCopyBtn
                            enabled: !modManager.generating
                            flat: true
                            Layout.preferredHeight: 26
                            Layout.preferredWidth: 54
                            background: Rectangle {
                                radius: 8
                                color: parent.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                                border.width: 1
                                border.color: window.borderColor
                            }
                            contentItem: Text {
                                text: "Copy"
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                color: window.textSecondaryColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                runtimeTab.copyLogToClipboard()
                            }
                        }

                        Button {
                            id: logClearBtn
                            enabled: !modManager.generating
                            flat: true
                            Layout.preferredHeight: 26
                            Layout.preferredWidth: 54
                            background: Rectangle {
                                radius: 8
                                color: parent.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                                border.width: 1
                                border.color: window.borderColor
                            }
                            contentItem: Text {
                                text: "Clear"
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                color: window.textSecondaryColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                modManager.clearUniversalRuntimeLog()
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 14
                        color: modManager.theme === "Glassmorphic"
                            ? Qt.rgba(0.04, 0.04, 0.06, 0.40)
                            : (modManager.theme === "Crimson" ? window.bgColor : Qt.rgba(0, 0, 0, 0.04))
                        border.width: 1
                        border.color: modManager.theme === "Glassmorphic"
                            ? Qt.rgba(1, 1, 1, 0.10)
                            : Qt.rgba(0, 0, 0, 0.06)

                        Flickable {
                            id: logFlickable
                            anchors.fill: parent
                            anchors.margins: 10

                            anchors.bottomMargin: logHScan.visible ? 24 : 10
                            contentWidth: Math.max(width, logTextArea.paintedWidth)
                            contentHeight: logTextArea.implicitHeight
                            clip: true
                            flickableDirection: Flickable.HorizontalAndVerticalFlick
                            boundsBehavior: Flickable.StopAtBounds

                            property bool userIsScrolling: false
                            property real lastContentHeight: 0
                            property bool autoScrollEnabled: true

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                            }

                            ScrollBar.horizontal: ScrollBar {
                                id: logHScan
                                policy: ScrollBar.AsNeeded
                            }

                            WheelHandler {
                                id: wheelHandler
                                target: null
                                onWheel: function(event) {
                                    logFlickable.userIsScrolling = true
                                    logFlickable.autoScrollEnabled = false
                                    userScrollTimer.restart()

                                    var delta = -event.angleDelta.y
                                    var multiplier = 1.5
                                    if (event.modifiers & Qt.ShiftModifier) multiplier = 5.0
                                    else if (event.modifiers & Qt.ControlModifier) multiplier = 15.0

                                    var scrollAmount = (logFlickable.height * 0.35) * multiplier * (delta / 120.0)
                                    var targetY = logFlickable.contentY + scrollAmount
                                    targetY = Math.max(0, Math.min(targetY, logFlickable.contentHeight - logFlickable.height))

                                    scrollAnimation.stop()
                                    scrollAnimation.from = logFlickable.contentY
                                    scrollAnimation.to = targetY
                                    scrollAnimation.duration = 80
                                    scrollAnimation.start()

                                    event.accepted = true
                                }
                            }

                            onMovingChanged: {
                                if (moving) {
                                    userIsScrolling = true
                                    autoScrollEnabled = false
                                    userScrollTimer.restart()
                                }
                            }

                            NumberAnimation {
                                id: scrollAnimation
                                target: logFlickable
                                property: "contentY"
                                easing.type: Easing.OutCubic
                            }

                            Timer {
                                id: userScrollTimer
                                interval: 500
                                onTriggered: {
                                    logFlickable.userIsScrolling = false
                                    if (logFlickable.contentHeight > logFlickable.height) {
                                        var distanceFromBottom = logFlickable.contentHeight - logFlickable.height - logFlickable.contentY
                                        if (distanceFromBottom <= 100) {
                                            logFlickable.autoScrollEnabled = true
                                        }
                                    }
                                }
                            }

                            Text {
                                id: logTextArea

                                width: Math.max(logFlickable.width, paintedWidth)
                                wrapMode: Text.NoWrap
                                text: runtimeLog || "[Avalonium] Waiting for Injection...\n"
                                font.pixelSize: 14

                                font.family: "Consolas"
                                color: window.textColor
                                textFormat: Text.PlainText
                                lineHeight: 1.3

                                onTextChanged: {
                                    if (logFlickable.autoScrollEnabled && !logFlickable.userIsScrolling) {
                                        Qt.callLater(function() {
                                            if (logFlickable.contentHeight > logFlickable.height) {
                                                var targetPos = logFlickable.contentHeight - logFlickable.height
                                                autoScrollAnimation.stop()
                                                autoScrollAnimation.from = logFlickable.contentY
                                                autoScrollAnimation.to = Math.max(0, targetPos)
                                                autoScrollAnimation.duration = 50
                                                autoScrollAnimation.start()
                                            }
                                        })
                                    }
                                }
                            }

                            onContentHeightChanged: {
                                if (autoScrollEnabled && !userIsScrolling) {
                                    Qt.callLater(function() {
                                        if (contentHeight > height) {
                                            var targetPos = contentHeight - height
                                            autoScrollAnimation.stop()
                                            autoScrollAnimation.from = contentY
                                            autoScrollAnimation.to = Math.max(0, targetPos)
                                            autoScrollAnimation.duration = 50
                                            autoScrollAnimation.start()
                                        }
                                        lastContentHeight = contentHeight
                                    })
                                } else {
                                    lastContentHeight = contentHeight
                                }
                            }

                            NumberAnimation {
                                id: autoScrollAnimation
                                target: logFlickable
                                property: "contentY"
                                easing.type: Easing.OutQuart
                            }
                        }
                    }
                }
            }
        }
    }
}
