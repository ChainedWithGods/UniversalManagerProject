import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Rectangle {
    id: root
    property alias text: field.text
    implicitWidth: 230
    implicitHeight: 34
    radius: 8
    color: modManager.theme === "Glassmorphic"
        ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.10)
        : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.035)
    border.width: 1
    border.color: field.activeFocus
        ? window.primaryColor
        : (modManager.theme === "Glassmorphic"
            ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.30)
            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.12))

    function focusInput() {
        field.forceActiveFocus()
        if (field.text.length > 0)
            field.selectAll()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 6
        spacing: 7

        Item {
            width: 14
            height: 14

            Image {
                id: searchIcon
                anchors.fill: parent
                source: "qrc:/UniversalModManager/Lucide Icons/search.svg"
                sourceSize: Qt.size(14, 14)
                opacity: 0.6
                visible: modManager.theme === "Cozy"
            }

            ColorOverlay {
                anchors.fill: searchIcon
                source: searchIcon
                color: window.iconColor
                opacity: 0.6
                visible: modManager.theme !== "Cozy"
            }
        }

        TextField {
            id: field
            ContextMenu.menu: null
            Layout.fillWidth: true
            placeholderText: "Search downloads..."
            placeholderTextColor: window.textSecondaryColor
            color: window.textColor
            font.pixelSize: 12
            selectByMouse: true
            persistentSelection: true
            background: Rectangle { color: "transparent" }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                cursorShape: Qt.IBeamCursor
                onPressed: function(mouse) {
                    field.forceActiveFocus()
                    searchMenu.popup(root, mouse.x, root.height + 4)
                    mouse.accepted = true
                }
                onPressAndHold: function(mouse) {
                    field.forceActiveFocus()
                    searchMenu.popup(root, mouse.x, root.height + 4)
                    mouse.accepted = true
                }
            }

            Menu {
                id: searchMenu
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
                    enabled: field.canUndo
                    onTriggered: field.undo()
                }

                SettingsContextMenuItem {
                    text: "Redo"
                    iconName: "redo-2.svg"
                    shortcutText: "Ctrl+Alt+X"
                    enabled: field.canRedo
                    onTriggered: field.redo()
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
                    enabled: field.selectedText.length > 0
                    onTriggered: field.cut()
                }

                SettingsContextMenuItem {
                    text: "Copy"
                    iconName: "copy.svg"
                    shortcutText: "Ctrl+C"
                    enabled: field.selectedText.length > 0
                    onTriggered: field.copy()
                }

                SettingsContextMenuItem {
                    text: "Paste"
                    iconName: "clipboard.svg"
                    shortcutText: "Ctrl+V"
                    onTriggered: field.paste()
                }

                SettingsContextMenuItem {
                    text: "Delete"
                    iconName: "trash.svg"
                    enabled: field.selectedText.length > 0
                    onTriggered: field.remove(field.selectionStart, field.selectionEnd)
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
                    enabled: field.text.length > 0
                    onTriggered: field.selectAll()
                }
            }
        }

        Rectangle {
            width: 22
            height: 22
            radius: 11
            visible: field.text.length > 0
            color: clearArea.containsMouse
                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                : "transparent"

            Item {
                anchors.centerIn: parent
                width: 11
                height: 11

                Image {
                    id: clearIcon
                    anchors.fill: parent
                    source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                    sourceSize: Qt.size(12, 12)
                    opacity: 0.7
                    visible: modManager.theme === "Cozy"
                }

                ColorOverlay {
                    anchors.fill: clearIcon
                    source: clearIcon
                    color: window.iconColor
                    opacity: 0.7
                    visible: modManager.theme !== "Cozy"
                }
            }

            MouseArea {
                id: clearArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: field.text = ""
            }
        }
    }
}
