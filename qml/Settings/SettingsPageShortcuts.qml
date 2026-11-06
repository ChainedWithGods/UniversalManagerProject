import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

ColumnLayout {
    Layout.fillWidth: true
    Layout.rightMargin: 10
    spacing: 15
    visible: generalSectionIndex === 3

    Text {
        text: "Keyboard Shortcuts"
        font.pixelSize: 20
        font.weight: 600
        color: window.textColor
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.rightMargin: 10
        spacing: 16

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 450
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
            radius: window.cardRadius
            border.width: window.borderWidth
            border.color: window.borderColor

            gradient: modManager.theme === "Crimson" ? crimsonShortcutCard1Gradient : null

            Gradient {
                id: crimsonShortcutCard1Gradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }

            layer.enabled: modManager.theme !== "Glassmorphic"
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.3)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 4
                shadowBlur: 0.4
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 25
                spacing: 15

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 15

                    Rectangle {
                        width: modManager.theme === "Wavey" ? 60 : 52
                        height: modManager.theme === "Wavey" ? 60 : 52
                        color: "transparent"
                        radius: 14
                        border.color: "transparent"
                        border.width: 0

                        Image {
                            id: shortcutsIcon
                            anchors.centerIn: parent
                            width: modManager.theme === "Wavey" ? 40 : 32
                            height: modManager.theme === "Wavey" ? 40 : 32
                            source: modManager.theme === "Glassmorphic"
                                    ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/keyboard.png"
                                    : (modManager.theme === "Crimson"
                                       ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/keyboard.png"
                                       : (modManager.theme === "Wavey"
                                          ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/shortcuts.png"
                                          : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/shortcuts.png"))
                            sourceSize: Qt.size(modManager.theme === "Wavey" ? 80 : 32, modManager.theme === "Wavey" ? 80 : 32)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            antialiasing: true
                        }

                        ColorOverlay {
                            anchors.fill: shortcutsIcon
                            source: shortcutsIcon
                            color: window.primaryColor
                            visible: modManager.theme === "Glassmorphic"
                        }
                    }

                    Column {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: "General Shortcuts"
                            font.pixelSize: 18
                            font.weight: 600
                            color: window.textColor
                        }

                        Text {
                            text: "Application-wide keyboard shortcuts."
                            font.pixelSize: 13
                            color: window.textSecondaryColor
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 15

                    ShortcutRow {
                        label: "Install Mods"
                        shortcut: window.shortcutInstall
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("install", newShortcut)
                    }

                    ShortcutRow {
                        label: "Backup Mods"
                        shortcut: window.shortcutBackup
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("backup", newShortcut)
                    }

                    ShortcutRow {
                        label: "Refresh List"
                        shortcut: window.shortcutRefresh
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("refresh", newShortcut)
                    }

                    ShortcutRow {
                        label: "Settings"
                        shortcut: window.shortcutSettings
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("settings", newShortcut)
                    }

                    ShortcutRow {
                        label: "Search"
                        shortcut: window.shortcutSearch
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("search", newShortcut)
                    }

                    ShortcutRow {
                        label: "Select All"
                        shortcut: window.shortcutSelectAll
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("selectAll", newShortcut)
                    }

                    ShortcutRow {
                        label: "Home"
                        shortcut: window.shortcutHome
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("home", newShortcut)
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 450
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
            radius: window.cardRadius
            border.width: window.borderWidth
            border.color: window.borderColor

            gradient: modManager.theme === "Crimson" ? crimsonModActionsCardGradient : null

            Gradient {
                id: crimsonModActionsCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }

            layer.enabled: modManager.theme !== "Glassmorphic"
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.3)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 4
                shadowBlur: 0.4
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 25
                spacing: 15

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 15

                    Rectangle {
                        width: modManager.theme === "Wavey" ? 60 : 52
                        height: modManager.theme === "Wavey" ? 60 : 52
                        color: "transparent"
                        radius: 14
                        border.color: "transparent"
                        border.width: 0

                        Image {
                            id: modActionsIcon
                            anchors.centerIn: parent
                            width: modManager.theme === "Wavey" ? 40 : 32
                            height: modManager.theme === "Wavey" ? 40 : 32
                            source: modManager.theme === "Glassmorphic"
                                    ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/keyboard.png"
                                    : (modManager.theme === "Crimson"
                                       ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/keyboard.png"
                                       : (modManager.theme === "Wavey"
                                          ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/shortcuts.png"
                                          : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/shortcuts.png"))
                            sourceSize: Qt.size(modManager.theme === "Wavey" ? 80 : 32, modManager.theme === "Wavey" ? 80 : 32)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            antialiasing: true
                        }

                        ColorOverlay {
                            anchors.fill: modActionsIcon
                            source: modActionsIcon
                            color: window.primaryColor
                            visible: modManager.theme === "Glassmorphic"
                        }
                    }

                    Column {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: "Mods Actions"
                            font.pixelSize: 18
                            font.weight: 600
                            color: window.textColor
                        }

                        Text {
                            text: "Quick actions for mods management."
                            font.pixelSize: 13
                            color: window.textSecondaryColor
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 15

                    ShortcutRow {
                        label: "Delete Mod"
                        shortcut: window.shortcutDelete
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("delete", newShortcut)
                    }

                    ShortcutRow {
                        label: "Toggle Mod (mouse hover)"
                        shortcut: window.shortcutToggleMod
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("toggleMod", newShortcut)
                    }

                    ShortcutRow {
                        label: "Open Folder (mouse Hover)"
                        shortcut: window.shortcutOpenFolder
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("openFolder", newShortcut)
                    }

                    ShortcutRow {
                        label: "View in Assets Viewer (mouse Hover)"
                        shortcut: window.shortcutShowInPakViewer
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("showInPakViewer", newShortcut)
                    }

                    ShortcutRow {
                        label: "Export Structure (mouse Hover)"
                        shortcut: window.shortcutExportModStructure
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("exportModStructure", newShortcut)
                    }

                    ShortcutRow {
                        label: "Enable All Mods"
                        shortcut: window.shortcutEnableAll
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("enableAll", newShortcut)
                    }

                    ShortcutRow {
                        label: "Disable All Mods"
                        shortcut: window.shortcutDisableAll
                        onShortcutUpdated: (newShortcut) => window.updateShortcut("disableAll", newShortcut)
                    }
                }
            }
        }
    }

    component ShortcutRow: RowLayout {
        property string label: "Action"
        property string shortcut: "Ctrl+X"
        signal shortcutUpdated(string newShortcut)

        Layout.fillWidth: true
        spacing: 15

        Text {
            text: label
            font.pixelSize: 14
            font.weight: 500
            color: window.textColor
            Layout.preferredWidth: 260
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            horizontalAlignment: Text.AlignLeft
            elide: Text.ElideRight
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: {
                if (shortcutField.activeFocus) {
                    return modManager.theme === "Glassmorphic" ?
                           Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25) :
                           Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                } else {

                    if (modManager.theme === "Glassmorphic") {
                        return Qt.rgba(1, 1, 1, 0.15)
                    } else if (modManager.theme === "Crimson" || modManager.theme === "Wavey") {
                        return Qt.rgba(0, 0, 0, 0.3)
                    } else {
                        return window.cardColor3
                    }
                }
            }
            radius: 8
            border.width: shortcutField.activeFocus ? 2 : 1
            border.color: shortcutField.activeFocus ? window.primaryColor : window.borderColor

            Behavior on color { ColorAnimation { duration: 150 } }
            Behavior on border.color { ColorAnimation { duration: 150 } }
            Behavior on border.width { NumberAnimation { duration: 150 } }

            TextInput {
                id: shortcutField
                anchors.fill: parent
                anchors.margins: 10
                text: shortcut
                font.pixelSize: 13
                font.family: "Consolas"
                color: window.textColor
                verticalAlignment: Text.AlignVCenter
                selectByMouse: true
                readOnly: true

                property string pendingModifiers: ""

                Keys.onPressed: (event) => {
                    var key = event.key
                    var modifiers = event.modifiers
                    var modifierStr = ""
                    var keyStr = ""

                    if (modifiers & Qt.ControlModifier) modifierStr += "Ctrl+"
                    if (modifiers & Qt.AltModifier) modifierStr += "Alt+"
                    if (modifiers & Qt.ShiftModifier) modifierStr += "Shift+"

                    var isModifierOnly = (key === Qt.Key_Control || key === Qt.Key_Shift ||
                                          key === Qt.Key_Alt || key === Qt.Key_Meta)

                    if (isModifierOnly) {

                        pendingModifiers = modifierStr
                        text = modifierStr
                    } else {

                        if (key >= Qt.Key_A && key <= Qt.Key_Z) {
                            keyStr = String.fromCharCode(key)
                        } else if (key >= Qt.Key_0 && key <= Qt.Key_9) {
                            keyStr = String.fromCharCode(key)
                        } else if (key >= Qt.Key_F1 && key <= Qt.Key_F12) {
                            keyStr = "F" + (key - Qt.Key_F1 + 1)
                        } else if (key === Qt.Key_Delete) {
                            keyStr = "Delete"
                        } else if (key === Qt.Key_Backspace) {
                            keyStr = "Backspace"
                        } else if (key === Qt.Key_Return || key === Qt.Key_Enter) {
                            keyStr = "Enter"
                        } else if (key === Qt.Key_Space) {
                            keyStr = "Space"
                        } else if (key === Qt.Key_Tab) {
                            keyStr = "Tab"
                        } else if (key === Qt.Key_Escape) {
                            keyStr = "Escape"
                        } else if (key === Qt.Key_Home) {
                            keyStr = "Home"
                        } else if (key === Qt.Key_End) {
                            keyStr = "End"
                        } else if (key === Qt.Key_PageUp) {
                            keyStr = "PgUp"
                        } else if (key === Qt.Key_PageDown) {
                            keyStr = "PgDown"
                        } else if (key === Qt.Key_Left) {
                            keyStr = "Left"
                        } else if (key === Qt.Key_Right) {
                            keyStr = "Right"
                        } else if (key === Qt.Key_Up) {
                            keyStr = "Up"
                        } else if (key === Qt.Key_Down) {
                            keyStr = "Down"
                        } else if (key === Qt.Key_Insert) {
                            keyStr = "Insert"
                        } else if (key === Qt.Key_Comma) {
                            keyStr = ","
                        } else if (key === Qt.Key_Period) {
                            keyStr = "."
                        } else if (key === Qt.Key_Slash) {
                            keyStr = "/"
                        } else if (key === Qt.Key_Semicolon) {
                            keyStr = ";"
                        } else if (key === Qt.Key_BracketLeft) {
                            keyStr = "["
                        } else if (key === Qt.Key_BracketRight) {
                            keyStr = "]"
                        } else if (key === Qt.Key_Minus) {
                            keyStr = "-"
                        } else if (key === Qt.Key_Equal) {
                            keyStr = "="
                        }

                        if (keyStr !== "") {
                            var fullShortcut = modifierStr + keyStr
                            text = fullShortcut
                            shortcutUpdated(fullShortcut)
                            pendingModifiers = ""
                            focus = false
                        }
                    }

                    event.accepted = true
                }

                Keys.onReleased: (event) => {

                    var key = event.key
                    var isModifierOnly = (key === Qt.Key_Control || key === Qt.Key_Shift ||
                                          key === Qt.Key_Alt || key === Qt.Key_Meta)

                    if (isModifierOnly && pendingModifiers !== "") {

                        var modifiers = event.modifiers
                        if (modifiers === Qt.NoModifier) {
                            text = shortcut
                            pendingModifiers = ""
                        } else {

                            var modifierStr = ""
                            if (modifiers & Qt.ControlModifier) modifierStr += "Ctrl+"
                            if (modifiers & Qt.AltModifier) modifierStr += "Alt+"
                            if (modifiers & Qt.ShiftModifier) modifierStr += "Shift+"
                            text = modifierStr
                            pendingModifiers = modifierStr
                        }
                    }
                    event.accepted = true
                }

                onActiveFocusChanged: {
                    if (!activeFocus) {

                        text = shortcut
                        pendingModifiers = ""
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: parent.forceActiveFocus()
                }
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: shortcutField.activeFocus ? "Press key..." : "Click to edit"
                font.pixelSize: 11
                color: window.textSecondaryColor
                opacity: 0.6
            }
        }
    }
}
