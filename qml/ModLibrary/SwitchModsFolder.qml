import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Dialog {
    id: quickModsFolderSwitchDialog
    modal: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    component SwitchContextMenuItem : MenuItem {
        id: menuItem
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

    property var    quickModsFolderChoices:          []
    property int    quickModsFolderSelectedIndex:    -1
    property string quickCurrentModsFolderName:      ""
    property string quickCurrentModsFolderPath:      ""
    property int    quickCurrentModsFolderCount:     0
    property string quickTargetModsFolderName:       ""
    property string quickTargetModsFolderPath:       ""
    property int    quickTargetModsFolderCount:      0
    property bool   quickSwitchDisableAllMods:       false
    property bool   quickSwitchDisableAllCollections: false
    property bool   quickSwitchShowAdvanced:         false
    property var    quickSwitchModsToDisable:        []
    property var    quickSwitchCollectionsToDisable: []
    property string quickSwitchAdvancedSearch:       ""
    property string quickSwitchCollectionSearch:     ""

    function quickSwitchFilteredCollections() {
        var list = typeof quickSwitchCurrentPresetCollections === "function" ? quickSwitchCurrentPresetCollections() : [];
        if (!quickSwitchCollectionSearch || quickSwitchCollectionSearch.trim() === "")
            return list;
        var q = quickSwitchCollectionSearch.toLowerCase().trim();
        return list.filter(function(col) {
            return col && col.name && col.name.toLowerCase().indexOf(q) !== -1;
        });
    }

    function quickSwitchFilteredMods() {
        var list = typeof quickSwitchCurrentPresetMods === "function" ? quickSwitchCurrentPresetMods() : [];
        if (!quickSwitchAdvancedSearch || quickSwitchAdvancedSearch.trim() === "")
            return list;
        var q = quickSwitchAdvancedSearch.toLowerCase().trim();
        return list.filter(function(m) {
            return m && m.name && m.name.toLowerCase().indexOf(q) !== -1;
        });
    }

    readonly property real idealWidth: quickSwitchShowAdvanced ? 940 : 780
    readonly property real idealHeight: mainColumnLayout.implicitHeight + 40

    width: Math.min(idealWidth, parent ? parent.width * 0.95 : idealWidth)
    height: Math.min(idealHeight, parent ? parent.height * 0.95 : idealHeight)

    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    Behavior on height { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

    signal folderTargetRequested(int index)
    signal disableAllModsToggled(bool value)
    signal disableAllCollectionsToggled(bool value)
    signal advancedToggled(bool value)
    signal modToggled(string key)
    signal collectionToggled(string id)
    signal selectAllModsRequested()
    signal deselectAllModsRequested()
    signal selectAllCollectionsRequested()
    signal deselectAllCollectionsRequested()
    signal advancedSearchChanged(string query)
    signal switchRequested(string targetPath, bool disableAllMods, bool disableAllCollections,
                           var modsToDisable, var collectionsToDisable)

    palette {
        windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
    }

    background: Rectangle {
        radius: 18
        border.width: 1
        border.color: modManager.theme === "Glassmorphic"
            ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.15)
            : window.borderColor

        color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

        gradient: {
            if (modManager.theme === "Cozy") return dialogCozyGradient
            if (modManager.theme === "Crimson") return dialogCrimsonGradient
            if (modManager.theme === "Wavey") return dialogWaveyGradient
            return null
        }
        Gradient {
            id: dialogCozyGradient
            GradientStop { position: 0.0; color: window.cozyDialogGradientTop }
            GradientStop { position: 1.0; color: window.cozyDialogGradientBottom }
        }
        Gradient {
            id: dialogCrimsonGradient
            GradientStop { position: 0.0; color: window.crimsonDialogGradientTop }
            GradientStop { position: 1.0; color: window.crimsonDialogGradientBottom }
        }
        Gradient {
            id: dialogWaveyGradient
            GradientStop { position: 0.0; color: window.waveyDialogGradientTop }
            GradientStop { position: 1.0; color: window.waveyDialogGradientBottom }
        }
    }

    Overlay.modal: Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, modManager.theme === "Glassmorphic" ? 0.65 : 0.5)
    }

    contentItem: Item {
        anchors.fill: parent

        ColumnLayout {
            id: mainColumnLayout
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Text {
                        text: "Switch Mods Folder"
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        color: window.textColor
                    }

                    Text {
                        text: "Switch the current profile to another detected mods folder."
                        font.pixelSize: 13
                        color: window.textSecondaryColor
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                Button {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    text: "✕"
                    flat: true
                    hoverEnabled: true
                    contentItem: Text {
                        text: parent.text
                        color: window.textSecondaryColor
                        font.pixelSize: 16
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                        radius: 16
                        border.width: 1
                        border.color: parent.hovered ? window.borderColor : "transparent"
                    }
                    onClicked: quickModsFolderSwitchDialog.close()
                }
            }

            Flickable {
                id: bodyScrollArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: mainDialogBodyColumn.implicitHeight
                clip: true
                contentWidth: width
                contentHeight: mainDialogBodyColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                interactive: contentHeight > height

                ScrollBar.vertical: ScrollBar {
                    policy: bodyScrollArea.interactive ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                }

                ColumnLayout {
                    id: mainDialogBodyColumn
                    width: bodyScrollArea.width
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: quickCurrentFolderLayout.implicitHeight + 22
                        radius: 12
                        color: modManager.theme === "Glassmorphic"
                            ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08)
                            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)
                        border.width: 1
                        border.color: window.borderColor

                        ColumnLayout {
                            id: quickCurrentFolderLayout
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 4

                            Text {
                                text: "Current folder"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: window.textSecondaryColor
                            }

                            Text {
                                text: quickCurrentModsFolderName + "  •  " + quickCurrentModsFolderCount + " mods"
                                font.pixelSize: 15
                                font.weight: Font.Bold
                                color: window.textColor
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            Text {
                                text: quickCurrentModsFolderPath
                                font.pixelSize: 12
                                color: window.textSecondaryColor
                                wrapMode: Text.WrapAnywhere
                                Layout.fillWidth: true
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "Switch to"
                            font.pixelSize: 13
                            font.weight: Font.Bold
                            color: window.textColor
                        }

                        ListView {
                            id: quickModsFolderOptionsList
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(180, Math.max(58, (quickModsFolderChoices.length - 1) * 62))
                            clip: true
                            spacing: 6
                            model: quickModsFolderChoices.length

                            delegate: Rectangle {
                                property var option: quickModsFolderChoices[index]
                                width: quickModsFolderOptionsList.width
                                height: option && !option.current ? 58 : 0
                                visible: option && !option.current
                                radius: 10
                                color: index === quickModsFolderSelectedIndex
                                    ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.16)
                                    : (modManager.theme === "Glassmorphic"
                                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.06)
                                        : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03))
                                border.width: index === quickModsFolderSelectedIndex ? 1.5 : 1
                                border.color: index === quickModsFolderSelectedIndex ? window.accentColor : window.borderColor

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 3

                                    Text {
                                        text: (option ? option.name : "") + "  •  " + (option ? option.count : 0) + " mods"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        color: window.textColor
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: option ? option.path : ""
                                        font.pixelSize: 11
                                        color: window.textSecondaryColor
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: quickModsFolderSwitchDialog.folderTargetRequested(index)
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: quickSwitchOptionsLayout.implicitHeight + 24
                        radius: 12
                        color: modManager.theme === "Glassmorphic"
                            ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08)
                            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)
                        border.width: 1
                        border.color: window.borderColor

                        ColumnLayout {
                            id: quickSwitchOptionsLayout
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    text: "Before switching"
                                    font.pixelSize: 13
                                    font.weight: Font.Bold
                                    color: window.textColor
                                    Layout.fillWidth: true
                                }

                                Button {
                                    id: quickSwitchAdvancedToggle
                                    Layout.preferredHeight: 28
                                    Layout.preferredWidth: quickSwitchAdvancedRow.implicitWidth + 20
                                    flat: true
                                    hoverEnabled: true
                                    padding: 2

                                    RowLayout {
                                        id: quickSwitchAdvancedRow
                                        anchors.centerIn: parent
                                        spacing: 6

                                        Text {
                                            text: quickSwitchShowAdvanced ? "Hide advanced" : "Advanced"
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                            color: quickSwitchAdvancedToggle.hovered
                                                ? window.primaryColor
                                                : window.textColor
                                        }

                                        Text {
                                            text: quickSwitchShowAdvanced ? "▾" : "▸"
                                            font.pixelSize: 11
                                            color: quickSwitchAdvancedToggle.hovered
                                                ? window.primaryColor
                                                : window.textSecondaryColor
                                        }
                                    }

                                    background: Rectangle {
                                        radius: 6
                                        color: quickSwitchAdvancedToggle.hovered
                                            ? (modManager.theme === "Glassmorphic"
                                                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.14)
                                                : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06))
                                            : "transparent"
                                        border.width: 1
                                        border.color: quickSwitchAdvancedToggle.hovered ? window.primaryColor : window.borderColor
                                    }

                                    onClicked: quickModsFolderSwitchDialog.advancedToggled(!quickSwitchShowAdvanced)
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 24

                                CheckBox {
                                    id: disableAllModsBeforeSwitch
                                    text: "Disable all Mods"
                                    checked: quickSwitchDisableAllMods
                                    onToggled: quickModsFolderSwitchDialog.disableAllModsToggled(checked)

                                    indicator: Rectangle {
                                        implicitWidth: 18
                                        implicitHeight: 18
                                        x: disableAllModsBeforeSwitch.leftPadding
                                        y: parent.height / 2 - height / 2
                                        radius: 4
                                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.1) : "transparent"
                                        border.color: disableAllModsBeforeSwitch.checked ? window.primaryColor : window.borderColor
                                        border.width: 1.5

                                        Rectangle {
                                            width: 8
                                            height: 8
                                            anchors.centerIn: parent
                                            radius: 2
                                            color: window.primaryColor
                                            visible: disableAllModsBeforeSwitch.checked
                                        }
                                    }

                                    contentItem: Text {
                                        text: disableAllModsBeforeSwitch.text
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: window.textColor
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: disableAllModsBeforeSwitch.indicator.width + 8
                                    }
                                }

                                CheckBox {
                                    id: disableAllCollectionsBeforeSwitch
                                    text: "Disable all Collections"
                                    checked: quickSwitchDisableAllCollections
                                    onToggled: quickModsFolderSwitchDialog.disableAllCollectionsToggled(checked)

                                    indicator: Rectangle {
                                        implicitWidth: 18
                                        implicitHeight: 18
                                        x: disableAllCollectionsBeforeSwitch.leftPadding
                                        y: parent.height / 2 - height / 2
                                        radius: 4
                                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.1) : "transparent"
                                        border.color: disableAllCollectionsBeforeSwitch.checked ? window.primaryColor : window.borderColor
                                        border.width: 1.5

                                        Rectangle {
                                            width: 8
                                            height: 8
                                            anchors.centerIn: parent
                                            radius: 2
                                            color: window.primaryColor
                                            visible: disableAllCollectionsBeforeSwitch.checked
                                        }
                                    }

                                    contentItem: Text {
                                        text: disableAllCollectionsBeforeSwitch.text
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: window.textColor
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: disableAllCollectionsBeforeSwitch.indicator.width + 8
                                    }
                                }
                            }

                            Text {
                                text: "Switching changes the active mods directory for this profile. Your mod files stay where they are."
                                font.pixelSize: 12
                                color: window.textSecondaryColor
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                visible: quickSwitchShowAdvanced
                                radius: 10
                                color: modManager.theme === "Glassmorphic"
                                    ? Qt.rgba(0, 0, 0, 0.18)
                                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.02)
                                border.width: 1
                                border.color: window.borderColor
                                implicitHeight: quickSwitchAdvancedContent.implicitHeight + 20

                                ColumnLayout {
                                    id: quickSwitchAdvancedContent
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 10

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 14

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 1
                                            spacing: 6

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 6

                                                Text {
                                                    text: "Mods to disable"
                                                    font.pixelSize: 12
                                                    font.weight: Font.Bold
                                                    color: quickSwitchDisableAllMods ? window.textSecondaryColor : window.textColor
                                                    Layout.fillWidth: true
                                                    opacity: quickSwitchDisableAllMods ? 0.55 : 1.0
                                                }

                                                Button {
                                                    text: "All"
                                                    Layout.preferredHeight: 22
                                                    Layout.preferredWidth: 36
                                                    enabled: !quickSwitchDisableAllMods
                                                    flat: true
                                                    hoverEnabled: true
                                                    padding: 0
                                                    contentItem: Text {
                                                        text: parent.text
                                                        font.pixelSize: 11
                                                        font.weight: Font.Medium
                                                        color: parent.enabled ? (parent.hovered ? window.primaryColor : window.textColor) : window.textSecondaryColor
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    background: Rectangle {
                                                        radius: 4
                                                        color: parent.hovered && parent.enabled ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"
                                                        border.width: 1
                                                        border.color: parent.hovered && parent.enabled ? window.primaryColor : window.borderColor
                                                    }
                                                    onClicked: quickModsFolderSwitchDialog.selectAllModsRequested()
                                                }

                                                Button {
                                                    text: "None"
                                                    Layout.preferredHeight: 22
                                                    Layout.preferredWidth: 44
                                                    enabled: !quickSwitchDisableAllMods
                                                    flat: true
                                                    hoverEnabled: true
                                                    padding: 0
                                                    contentItem: Text {
                                                        text: parent.text
                                                        font.pixelSize: 11
                                                        font.weight: Font.Medium
                                                        color: parent.enabled ? (parent.hovered ? window.primaryColor : window.textColor) : window.textSecondaryColor
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    background: Rectangle {
                                                        radius: 4
                                                        color: parent.hovered && parent.enabled ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"
                                                        border.width: 1
                                                        border.color: parent.hovered && parent.enabled ? window.primaryColor : window.borderColor
                                                    }
                                                    onClicked: quickModsFolderSwitchDialog.deselectAllModsRequested()
                                                }
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 32
                                                radius: 6
                                                color: modManager.theme === "Glassmorphic"
                                                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08)
                                                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.04)
                                                border.width: 1
                                                border.color: quickSwitchDisableAllMods ? Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.3) : window.borderColor
                                                opacity: quickSwitchDisableAllMods ? 0.55 : 1.0

                                                TextInput {
                                                    id: quickSwitchAdvancedSearchField
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 8
                                                    anchors.rightMargin: 26
                                                    color: window.textColor
                                                    enabled: !quickSwitchDisableAllMods
                                                    font.pixelSize: 12
                                                    selectByMouse: true
                                                    verticalAlignment: TextInput.AlignVCenter
                                                    text: quickSwitchAdvancedSearch
                                                    onTextChanged: quickModsFolderSwitchDialog.advancedSearchChanged(text)

                                                    Text {
                                                        anchors.fill: parent
                                                        text: "Search mods…"
                                                        font.pixelSize: 12
                                                        color: window.textSecondaryColor
                                                        visible: parent.text.length === 0
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                }

                                                Text {
                                                    anchors.right: parent.right
                                                    anchors.rightMargin: 8
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: "⌕"
                                                    font.pixelSize: 13
                                                    color: window.textSecondaryColor
                                                }

                                                MouseArea {
                                                    z: 5
                                                    anchors.fill: parent
                                                    acceptedButtons: Qt.RightButton
                                                    propagateComposedEvents: true
                                                    onClicked: function(mouse) {
                                                        if (mouse.button === Qt.RightButton) {
                                                            quickSwitchModSearchContextMenu.popup()
                                                        }
                                                    }
                                                }

                                                Menu {
                                                    id: quickSwitchModSearchContextMenu
                                                    width: 180
                                                    padding: 6

                                                    background: Rectangle {
                                                        color: modManager.theme === "Glassmorphic"
                                                            ? Qt.rgba(0.08, 0.08, 0.12, 0.95)
                                                            : (modManager.theme === "Crimson" ? Qt.rgba(0.10, 0.04, 0.14, 0.95) : window.surfaceColor)
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
                                                            blurEnabled: modManager.theme === "Glassmorphic"
                                                            blur: 0.5
                                                            blurMax: 32
                                                        }
                                                    }

                                                    SwitchContextMenuItem {
                                                        text: "Undo"
                                                        iconName: "undo-2.svg"
                                                        enabled: quickSwitchAdvancedSearchField.canUndo
                                                        onTriggered: quickSwitchAdvancedSearchField.undo()
                                                    }
                                                    SwitchContextMenuItem {
                                                        text: "Redo"
                                                        iconName: "redo-2.svg"
                                                        enabled: quickSwitchAdvancedSearchField.canRedo
                                                        onTriggered: quickSwitchAdvancedSearchField.redo()
                                                    }
                                                    MenuSeparator {}
                                                    SwitchContextMenuItem {
                                                        text: "Cut"
                                                        iconName: "scissors.svg"
                                                        enabled: quickSwitchAdvancedSearchField.selectedText.length > 0
                                                        onTriggered: quickSwitchAdvancedSearchField.cut()
                                                    }
                                                    SwitchContextMenuItem {
                                                        text: "Copy"
                                                        iconName: "copy.svg"
                                                        enabled: quickSwitchAdvancedSearchField.selectedText.length > 0
                                                        onTriggered: quickSwitchAdvancedSearchField.copy()
                                                    }
                                                    SwitchContextMenuItem {
                                                        text: "Paste"
                                                        iconName: "clipboard.svg"
                                                        enabled: true
                                                        onTriggered: quickSwitchAdvancedSearchField.paste()
                                                    }
                                                    SwitchContextMenuItem {
                                                        text: "Delete"
                                                        iconName: "trash.svg"
                                                        enabled: quickSwitchAdvancedSearchField.selectedText.length > 0
                                                        onTriggered: quickSwitchAdvancedSearchField.remove(quickSwitchAdvancedSearchField.selectionStart, quickSwitchAdvancedSearchField.selectionEnd)
                                                    }
                                                    MenuSeparator {}
                                                    SwitchContextMenuItem {
                                                        text: "Select All"
                                                        iconName: "text-select.svg"
                                                        enabled: quickSwitchAdvancedSearchField.text.length > 0
                                                        onTriggered: quickSwitchAdvancedSearchField.selectAll()
                                                    }
                                                }
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 170
                                                radius: 6
                                                color: Qt.rgba(0, 0, 0, 0.12)
                                                border.width: 1
                                                border.color: window.borderColor
                                                opacity: quickSwitchDisableAllMods ? 0.55 : 1.0
                                                clip: true

                                                ListView {
                                                    id: quickSwitchAdvancedModsList
                                                    anchors.fill: parent
                                                    anchors.margins: 4
                                                    clip: true
                                                    spacing: 3
                                                    interactive: !quickSwitchDisableAllMods
                                                    model: quickSwitchFilteredMods()
                                                    delegate: Rectangle {
                                                        width: quickSwitchAdvancedModsList.width
                                                        height: 30
                                                        radius: 5
                                                        property bool isSelected: quickSwitchModsToDisable.indexOf(modelData.key) >= 0
                                                        color: isSelected
                                                            ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.18)
                                                            : (quickSwitchAdvancedModsMouse.containsMouse
                                                                ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05)
                                                                : "transparent")

                                                        Row {
                                                            anchors.fill: parent
                                                            anchors.leftMargin: 8
                                                            anchors.rightMargin: 8
                                                            spacing: 8

                                                            Rectangle {
                                                                width: 15
                                                                height: 15
                                                                radius: 3
                                                                anchors.verticalCenter: parent.verticalCenter
                                                                color: isSelected
                                                                    ? (modManager.theme === "Glassmorphic"
                                                                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.2)
                                                                        : Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.15))
                                                                    : "transparent"
                                                                border.width: 1.2
                                                                border.color: isSelected ? window.primaryColor : window.borderColor

                                                                Rectangle {
                                                                    width: 6
                                                                    height: 6
                                                                    radius: 1
                                                                    anchors.centerIn: parent
                                                                    color: window.primaryColor
                                                                    visible: isSelected
                                                                }
                                                            }

                                                            Text {
                                                                anchors.verticalCenter: parent.verticalCenter
                                                                text: modelData.name
                                                                font.pixelSize: 12
                                                                font.weight: Font.Medium
                                                                color: window.textColor
                                                                elide: Text.ElideMiddle
                                                                width: parent.width - 24
                                                            }
                                                        }

                                                        MouseArea {
                                                            id: quickSwitchAdvancedModsMouse
                                                            anchors.fill: parent
                                                            hoverEnabled: true
                                                            enabled: !quickSwitchDisableAllMods
                                                            cursorShape: quickSwitchDisableAllMods ? Qt.ArrowCursor : Qt.PointingHandCursor
                                                            onClicked: quickModsFolderSwitchDialog.modToggled(modelData.key)
                                                        }
                                                    }

                                                    Text {
                                                        anchors.centerIn: parent
                                                        visible: quickSwitchAdvancedModsList.count === 0
                                                        text: {
                                                            if (quickSwitchDisableAllMods) return "Disabled by master switch"
                                                            if (quickSwitchAdvancedSearch !== "") return "No mods match search"
                                                            return "No enabled mods"
                                                        }
                                                        font.pixelSize: 12
                                                        color: window.textSecondaryColor
                                                    }
                                                }
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 1
                                            spacing: 6

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 6

                                                Text {
                                                    text: "Collections to disable"
                                                    font.pixelSize: 12
                                                    font.weight: Font.Bold
                                                    color: quickSwitchDisableAllCollections ? window.textSecondaryColor : window.textColor
                                                    Layout.fillWidth: true
                                                    opacity: quickSwitchDisableAllCollections ? 0.55 : 1.0
                                                }

                                                Button {
                                                    text: "All"
                                                    Layout.preferredHeight: 22
                                                    Layout.preferredWidth: 36
                                                    enabled: !quickSwitchDisableAllCollections
                                                    flat: true
                                                    hoverEnabled: true
                                                    padding: 0
                                                    contentItem: Text {
                                                        text: parent.text
                                                        font.pixelSize: 11
                                                        font.weight: Font.Medium
                                                        color: parent.enabled ? (parent.hovered ? window.primaryColor : window.textColor) : window.textSecondaryColor
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    background: Rectangle {
                                                        radius: 4
                                                        color: parent.hovered && parent.enabled ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"
                                                        border.width: 1
                                                        border.color: parent.hovered && parent.enabled ? window.primaryColor : window.borderColor
                                                    }
                                                    onClicked: quickModsFolderSwitchDialog.selectAllCollectionsRequested()
                                                }

                                                Button {
                                                    text: "None"
                                                    Layout.preferredHeight: 22
                                                    Layout.preferredWidth: 44
                                                    enabled: !quickSwitchDisableAllCollections
                                                    flat: true
                                                    hoverEnabled: true
                                                    padding: 0
                                                    contentItem: Text {
                                                        text: parent.text
                                                        font.pixelSize: 11
                                                        font.weight: Font.Medium
                                                        color: parent.enabled ? (parent.hovered ? window.primaryColor : window.textColor) : window.textSecondaryColor
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    background: Rectangle {
                                                        radius: 4
                                                        color: parent.hovered && parent.enabled ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"
                                                        border.width: 1
                                                        border.color: parent.hovered && parent.enabled ? window.primaryColor : window.borderColor
                                                    }
                                                    onClicked: quickModsFolderSwitchDialog.deselectAllCollectionsRequested()
                                                }
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 32
                                                radius: 6
                                                color: modManager.theme === "Glassmorphic"
                                                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08)
                                                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.04)
                                                border.width: 1
                                                border.color: quickSwitchDisableAllCollections ? Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.3) : window.borderColor
                                                opacity: quickSwitchDisableAllCollections ? 0.55 : 1.0

                                                TextInput {
                                                    id: quickSwitchCollectionSearchField
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 8
                                                    anchors.rightMargin: 26
                                                    color: window.textColor
                                                    enabled: !quickSwitchDisableAllCollections
                                                    font.pixelSize: 12
                                                    selectByMouse: true
                                                    verticalAlignment: TextInput.AlignVCenter
                                                    text: quickSwitchCollectionSearch
                                                    onTextChanged: quickSwitchCollectionSearch = text

                                                    Text {
                                                        anchors.fill: parent
                                                        text: "Search collections…"
                                                        font.pixelSize: 12
                                                        color: window.textSecondaryColor
                                                        visible: parent.text.length === 0
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                }

                                                Text {
                                                    anchors.right: parent.right
                                                    anchors.rightMargin: 8
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: "⌕"
                                                    font.pixelSize: 13
                                                    color: window.textSecondaryColor
                                                }

                                                MouseArea {
                                                    z: 5
                                                    anchors.fill: parent
                                                    acceptedButtons: Qt.RightButton
                                                    propagateComposedEvents: true
                                                    onClicked: function(mouse) {
                                                        if (mouse.button === Qt.RightButton) {
                                                            quickSwitchCollectionSearchContextMenu.popup()
                                                        }
                                                    }
                                                }

                                                Menu {
                                                    id: quickSwitchCollectionSearchContextMenu
                                                    width: 180
                                                    padding: 6

                                                    background: Rectangle {
                                                        color: modManager.theme === "Glassmorphic"
                                                            ? Qt.rgba(0.08, 0.08, 0.12, 0.95)
                                                            : (modManager.theme === "Crimson" ? Qt.rgba(0.10, 0.04, 0.14, 0.95) : window.surfaceColor)
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
                                                            blurEnabled: modManager.theme === "Glassmorphic"
                                                            blur: 0.5
                                                            blurMax: 32
                                                        }
                                                    }

                                                    SwitchContextMenuItem {
                                                        text: "Undo"
                                                        iconName: "undo-2.svg"
                                                        enabled: quickSwitchCollectionSearchField.canUndo
                                                        onTriggered: quickSwitchCollectionSearchField.undo()
                                                    }
                                                    SwitchContextMenuItem {
                                                        text: "Redo"
                                                        iconName: "redo-2.svg"
                                                        enabled: quickSwitchCollectionSearchField.canRedo
                                                        onTriggered: quickSwitchCollectionSearchField.redo()
                                                    }
                                                    MenuSeparator {}
                                                    SwitchContextMenuItem {
                                                        text: "Cut"
                                                        iconName: "scissors.svg"
                                                        enabled: quickSwitchCollectionSearchField.selectedText.length > 0
                                                        onTriggered: quickSwitchCollectionSearchField.cut()
                                                    }
                                                    SwitchContextMenuItem {
                                                        text: "Copy"
                                                        iconName: "copy.svg"
                                                        enabled: quickSwitchCollectionSearchField.selectedText.length > 0
                                                        onTriggered: quickSwitchCollectionSearchField.copy()
                                                    }
                                                    SwitchContextMenuItem {
                                                        text: "Paste"
                                                        iconName: "clipboard.svg"
                                                        enabled: true
                                                        onTriggered: quickSwitchCollectionSearchField.paste()
                                                    }
                                                    SwitchContextMenuItem {
                                                        text: "Delete"
                                                        iconName: "trash.svg"
                                                        enabled: quickSwitchCollectionSearchField.selectedText.length > 0
                                                        onTriggered: quickSwitchCollectionSearchField.remove(quickSwitchCollectionSearchField.selectionStart, quickSwitchCollectionSearchField.selectionEnd)
                                                    }
                                                    MenuSeparator {}
                                                    SwitchContextMenuItem {
                                                        text: "Select All"
                                                        iconName: "text-select.svg"
                                                        enabled: quickSwitchCollectionSearchField.text.length > 0
                                                        onTriggered: quickSwitchCollectionSearchField.selectAll()
                                                    }
                                                }
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 170
                                                radius: 6
                                                color: Qt.rgba(0, 0, 0, 0.12)
                                                border.width: 1
                                                border.color: window.borderColor
                                                opacity: quickSwitchDisableAllCollections ? 0.55 : 1.0
                                                clip: true

                                                ListView {
                                                    id: quickSwitchAdvancedCollectionsList
                                                    anchors.fill: parent
                                                    anchors.margins: 4
                                                    clip: true
                                                    spacing: 3
                                                    interactive: !quickSwitchDisableAllCollections
                                                    model: quickSwitchFilteredCollections()
                                                    delegate: Rectangle {
                                                        width: quickSwitchAdvancedCollectionsList.width
                                                        height: 30
                                                        radius: 5
                                                        property bool isSelected: quickSwitchCollectionsToDisable.indexOf(modelData.id) >= 0
                                                        color: isSelected
                                                            ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.18)
                                                            : (quickSwitchAdvancedCollectionsMouse.containsMouse
                                                                ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05)
                                                                : "transparent")

                                                        Row {
                                                            anchors.fill: parent
                                                            anchors.leftMargin: 8
                                                            anchors.rightMargin: 8
                                                            spacing: 8

                                                            Rectangle {
                                                                width: 15
                                                                height: 15
                                                                radius: 3
                                                                anchors.verticalCenter: parent.verticalCenter
                                                                color: isSelected
                                                                    ? (modManager.theme === "Glassmorphic"
                                                                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.2)
                                                                        : Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.15))
                                                                    : "transparent"
                                                                border.width: 1.2
                                                                border.color: isSelected ? window.primaryColor : window.borderColor

                                                                Rectangle {
                                                                    width: 6
                                                                    height: 6
                                                                    radius: 1
                                                                    anchors.centerIn: parent
                                                                    color: window.primaryColor
                                                                    visible: isSelected
                                                                }
                                                            }

                                                            Text {
                                                                anchors.verticalCenter: parent.verticalCenter
                                                                text: modelData.name + "  •  " + modelData.modCount + " mod" + (modelData.modCount !== 1 ? "s" : "")
                                                                font.pixelSize: 12
                                                                font.weight: Font.Medium
                                                                color: window.textColor
                                                                elide: Text.ElideRight
                                                                width: parent.width - 24
                                                            }
                                                        }

                                                        MouseArea {
                                                            id: quickSwitchAdvancedCollectionsMouse
                                                            anchors.fill: parent
                                                            hoverEnabled: true
                                                            enabled: !quickSwitchDisableAllCollections
                                                            cursorShape: quickSwitchDisableAllCollections ? Qt.ArrowCursor : Qt.PointingHandCursor
                                                            onClicked: quickModsFolderSwitchDialog.collectionToggled(modelData.id)
                                                        }
                                                    }

                                                    Text {
                                                        anchors.centerIn: parent
                                                        visible: quickSwitchAdvancedCollectionsList.count === 0
                                                        text: {
                                                            if (quickSwitchDisableAllCollections) return "Disabled by master switch"
                                                            if (quickSwitchCollectionSearch !== "") return "No collections match search"
                                                            return "No collections in profile"
                                                        }
                                                        font.pixelSize: 12
                                                        color: window.textSecondaryColor
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        text: {
                                            if (quickSwitchDisableAllMods && quickSwitchDisableAllCollections)
                                                return "Master toggles override per-item selection above."
                                            if (quickSwitchDisableAllMods)
                                                return "\"Disable all Mods\" overrides per-item selection."
                                            if (quickSwitchDisableAllCollections)
                                                return "\"Disable all Collections\" overrides per-item selection."
                                            return "Choose specific mods and/or collections to disable when entering this folder."
                                        }
                                        font.pixelSize: 11
                                        color: window.textSecondaryColor
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: quickTargetFolderLayout.implicitHeight + 20
                        radius: 12
                        color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.08)
                        border.width: 1
                        border.color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.28)

                        ColumnLayout {
                            id: quickTargetFolderLayout
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 4

                            Text {
                                text: "Switching to"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: window.textSecondaryColor
                            }

                            Text {
                                text: quickTargetModsFolderName + "  •  " + quickTargetModsFolderCount + " mods"
                                font.pixelSize: 15
                                font.weight: Font.Bold
                                color: window.textColor
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            Text {
                                text: quickTargetModsFolderPath
                                font.pixelSize: 12
                                color: window.textSecondaryColor
                                wrapMode: Text.WrapAnywhere
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Item { Layout.fillWidth: true }

                Button {
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 40
                    text: "Cancel"
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: modManager.theme === "Glassmorphic"
                            ? (parent.hovered ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.75))
                            : window.textColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: "transparent"
                        radius: 8
                        border.width: 1
                        border.color: window.borderColor
                    }

                    onClicked: quickModsFolderSwitchDialog.close()
                }

                Button {
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 40
                    text: "Switch Folder"
                    enabled: quickTargetModsFolderPath !== ""
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
                        color: parent.enabled
                            ? (parent.hovered ? Qt.lighter(window.primaryColor, 1.08) : window.primaryColor)
                            : Qt.rgba(0.3, 0.3, 0.3, 0.35)
                        radius: 8
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    onClicked: {
                        quickModsFolderSwitchDialog.switchRequested(
                            quickTargetModsFolderPath,
                            quickSwitchDisableAllMods,
                            quickSwitchDisableAllCollections,
                            quickSwitchModsToDisable,
                            quickSwitchCollectionsToDisable
                        )
                    }
                }
            }
        }
    }
}
