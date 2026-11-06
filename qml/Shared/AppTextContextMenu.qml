import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Menu {
    id: menu
    required property var editor
    width: 250
    padding: 8

    background: Rectangle {
        color: modManager.theme === "Wavey"
            ? "transparent"
            : (modManager.theme === "Glassmorphic"
                ? Qt.rgba(0.05, 0.05, 0.07, Math.max(0.88, Math.min(0.97, modManager.glassOpacity + 0.84)))
                : "transparent")
        radius: 14
        border.width: 1
        border.color: modManager.theme === "Wavey"
            ? Qt.rgba(0.91, 0.12, 0.55, 0.4)
            : (modManager.theme === "Glassmorphic"
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.2)
                : window.borderColor)

        gradient: {
            if (modManager.theme === "Wavey") return waveyGradient
            if (modManager.theme === "Crimson") return crimsonGradient
            if (modManager.theme === "Cozy") return cozyGradient
            return null
        }

        Gradient {
            id: waveyGradient
            GradientStop { position: 0.0; color: "#1b3185" }
            GradientStop { position: 1.0; color: "#0a2742" }
        }

        Gradient {
            id: crimsonGradient
            GradientStop { position: 0.0; color: window.crimsonContextMenuGradientTop }
            GradientStop { position: 1.0; color: window.crimsonContextMenuGradientBottom }
        }

        Gradient {
            id: cozyGradient
            GradientStop { position: 0.0; color: window.cozyContextMenuGradientTop }
            GradientStop { position: 1.0; color: window.cozyContextMenuGradientBottom }
        }

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.35)
            shadowVerticalOffset: 6
            shadowBlur: 0.6
        }
    }

    SettingsContextMenuItem {
        text: "Undo"
        iconName: "undo-2.svg"
        shortcutText: "Ctrl+Z"
        enabled: menu.editor ? menu.editor.canUndo : false
        onTriggered: if (menu.editor) menu.editor.undo()
    }

    SettingsContextMenuItem {
        text: "Redo"
        iconName: "redo-2.svg"
        shortcutText: "Ctrl+Alt+X"
        enabled: menu.editor ? menu.editor.canRedo : false
        onTriggered: if (menu.editor) menu.editor.redo()
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

    SettingsContextMenuItem {
        text: "Cut"
        iconName: "scissors.svg"
        shortcutText: "Ctrl+X"
        enabled: menu.editor && !menu.editor.readOnly && menu.editor.selectedText.length > 0
        onTriggered: if (menu.editor) menu.editor.cut()
    }

    SettingsContextMenuItem {
        text: "Copy"
        iconName: "copy.svg"
        shortcutText: "Ctrl+C"
        enabled: menu.editor ? menu.editor.selectedText.length > 0 : false
        onTriggered: if (menu.editor) menu.editor.copy()
    }

    SettingsContextMenuItem {
        text: "Paste"
        iconName: "clipboard.svg"
        shortcutText: "Ctrl+V"
        enabled: menu.editor ? !menu.editor.readOnly : false
        onTriggered: if (menu.editor) menu.editor.paste()
    }

    SettingsContextMenuItem {
        text: "Delete"
        iconName: "trash.svg"
        enabled: menu.editor && !menu.editor.readOnly && menu.editor.selectedText.length > 0
        onTriggered: if (menu.editor) menu.editor.remove(menu.editor.selectionStart, menu.editor.selectionEnd)
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

    SettingsContextMenuItem {
        text: "Select All"
        iconName: "text-select.svg"
        shortcutText: "Ctrl+A"
        enabled: menu.editor ? menu.editor.text.length > 0 : false
        onTriggered: if (menu.editor) menu.editor.selectAll()
    }
}
