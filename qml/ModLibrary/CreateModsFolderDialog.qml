import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Dialog {
    id: createModsFolderDialog
    modal: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string profileName: ""
    property bool uppercase: true
    property string selectedName: "Mods"
    property string basePath: ""
    property string targetPath: ""
    property string status: "error"
    property string existingName: ""
    property string statusMessage: ""
    property bool useForProfile: false

    readonly property var nameOptions: uppercase
        ? ["~Mods", "Mods", "LogicMods"]
        : ["~mods", "mods", "logicmods"]
    readonly property bool folderExists: status === "exists"
    readonly property bool caseConflict: status === "caseConflict"
    readonly property bool canCreate: status === "missing"

    signal folderCreated(string path, string name, bool useForProfile)
    signal existingFolderUsed(string path, string name, bool useForProfile)
    signal createFailed(string message)

    function refresh(resetProfileChoice) {
        var res = modManager.checkModsFolderForCurrentPreset(selectedName)
        status = res.status || "error"
        basePath = res.basePath || ""
        targetPath = res.path || ""
        existingName = res.existingName || ""
        statusMessage = res.message || ""
        if (resetProfileChoice) {
            useForProfile = !res.currentExists
        }
    }

    function toggleUppercase() {
        var idx = nameOptions.indexOf(selectedName)
        uppercase = !uppercase
        selectedName = nameOptions[idx >= 0 ? idx : 1]
    }

    onSelectedNameChanged: refresh(false)
    onOpened: refresh(true)

    readonly property real idealWidth: 600
    readonly property real idealHeight: mainColumnLayout.implicitHeight + 40

    width: Math.min(idealWidth, parent ? parent.width * 0.92 : idealWidth)
    height: Math.min(idealHeight, parent ? parent.height * 0.92 : idealHeight)

    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    Behavior on height { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

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
                        text: "Create Mods Folder"
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        color: window.textColor
                    }

                    Text {
                        text: "Create a Mods, ~Mods or LogicMods folder for this profile."
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
                    onClicked: createModsFolderDialog.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: locationColumn.implicitHeight + 22
                radius: 12
                color: modManager.theme === "Glassmorphic"
                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08)
                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)
                border.width: 1
                border.color: window.borderColor

                ColumnLayout {
                    id: locationColumn
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    Text {
                        text: "Location"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: window.textSecondaryColor
                    }

                    Text {
                        visible: createModsFolderDialog.profileName !== ""
                        text: createModsFolderDialog.profileName
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        color: window.textColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        text: createModsFolderDialog.basePath !== ""
                            ? createModsFolderDialog.basePath
                            : "Game folder not found"
                        font.pixelSize: 12
                        color: window.textSecondaryColor
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    text: "Folder name"
                    font.pixelSize: 13
                    font.weight: Font.Bold
                    color: window.textColor
                    Layout.fillWidth: true
                }

                Rectangle {
                    width: 36
                    height: 36
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: 8
                    color: toggleCaseMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05) : "transparent"
                    border.width: 1
                    border.color: createModsFolderDialog.uppercase ? window.accentColor : window.borderColor

                    Text {
                        anchors.centerIn: parent
                        text: "Aa"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        color: createModsFolderDialog.uppercase ? window.accentColor : window.textSecondaryColor
                    }

                    MouseArea {
                        id: toggleCaseMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: createModsFolderDialog.toggleUppercase()
                    }

                    ToolTip {
                        visible: toggleCaseMouse.containsMouse
                        delay: 400
                        padding: 15

                        contentItem: Text {
                            text: "<b>Toggle Uppercase Folder Names</b><br>Creates the folder as '~Mods', 'Mods' and 'LogicMods', or as '~mods', 'mods' and 'logicmods'.<br><br><font color=\"" + window.textSecondaryColor + "\"><i>Note: Windows treats 'Mods/mods' and '~Mods/~mods' and 'LogicMods/logicmods' as the same folder due to case-insensitivity.</i></font>"
                            color: window.textColor
                            font.pixelSize: 14
                            wrapMode: Text.Wrap
                            textFormat: Text.StyledText
                            lineHeight: 1.2
                        }

                        background: Rectangle {
                            radius: 12
                            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor
                            border.width: 1
                            border.color: window.borderColor

                            layer.enabled: true
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Qt.rgba(0, 0, 0, 0.3)
                                shadowBlur: 0.5
                                shadowVerticalOffset: 4
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                implicitHeight: optionsColumn.implicitHeight

                Column {
                    id: optionsColumn
                    width: parent.width
                    spacing: 6

                    Repeater {
                        model: createModsFolderDialog.nameOptions

                        delegate: Rectangle {
                            id: optionRow
                            width: optionsColumn.width
                            height: 44
                            radius: 10

                            readonly property bool isSelected: modelData === createModsFolderDialog.selectedName

                            color: isSelected
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.16)
                                : (optionMouse.containsMouse
                                    ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05)
                                    : "transparent")
                            border.width: isSelected ? 1.5 : 1
                            border.color: isSelected ? window.accentColor : window.borderColor

                            Behavior on color { ColorAnimation { duration: 120 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 10

                                Text {
                                    text: modelData
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: window.textColor
                                    Layout.fillWidth: true
                                }
                            }

                            MouseArea {
                                id: optionMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: createModsFolderDialog.selectedName = modelData
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: statusCard
                Layout.fillWidth: true
                implicitHeight: statusColumn.implicitHeight + 20
                radius: 10

                color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, modManager.theme === "Glassmorphic" ? 0.12 : 0.10)
                border.width: 1
                border.color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.40)

                ColumnLayout {
                    id: statusColumn
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 3

                    Text {
                        Layout.fillWidth: true
                        text: {
                            if (createModsFolderDialog.status === "missing") return "New folder"
                            if (createModsFolderDialog.folderExists) return "Already exists"
                            if (createModsFolderDialog.caseConflict) return "Same folder, different case"
                            return "Cannot create the folder"
                        }
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: window.accentColor
                    }

                    Text {
                        Layout.fillWidth: true
                        text: {
                            if (createModsFolderDialog.status === "missing")
                                return "The folder will be created at " + createModsFolderDialog.targetPath
                            if (createModsFolderDialog.folderExists)
                                return "A folder named '" + createModsFolderDialog.existingName + "' already exists in this folder."
                            if (createModsFolderDialog.caseConflict)
                                return "A folder named '" + createModsFolderDialog.existingName + "' already exists. Windows treats '"
                                    + createModsFolderDialog.existingName + "' and '" + createModsFolderDialog.selectedName
                                    + "' as the same folder, so '" + createModsFolderDialog.selectedName + "' cannot be created next to it."
                            return createModsFolderDialog.statusMessage
                        }
                        font.pixelSize: 12
                        color: window.textSecondaryColor
                        wrapMode: Text.WordWrap
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                CheckBox {
                    id: useForProfileCheck
                    checked: createModsFolderDialog.useForProfile
                    onToggled: createModsFolderDialog.useForProfile = checked
                    enabled: createModsFolderDialog.canCreate || createModsFolderDialog.folderExists
                    implicitWidth: 18

                    indicator: Rectangle {
                        implicitWidth: 18
                        implicitHeight: 18
                        x: 0
                        y: parent.height / 2 - height / 2
                        radius: 4
                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.1) : "transparent"
                        border.color: useForProfileCheck.checked ? window.primaryColor : window.borderColor
                        border.width: 1.5

                        Rectangle {
                            width: 8
                            height: 8
                            anchors.centerIn: parent
                            radius: 2
                            color: window.primaryColor
                            visible: useForProfileCheck.checked
                        }
                    }
                }

                Text {
                    text: "Use this folder for the current profile"
                    font.pixelSize: 12
                    color: window.textSecondaryColor
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
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

                    onClicked: createModsFolderDialog.close()
                }

                Button {
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: 40
                    text: createModsFolderDialog.folderExists ? "Use Existing Folder" : "Create Folder"
                    enabled: createModsFolderDialog.canCreate || createModsFolderDialog.folderExists
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: parent.enabled ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.6)
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
                        if (createModsFolderDialog.folderExists) {
                            createModsFolderDialog.close()
                            createModsFolderDialog.existingFolderUsed(
                                createModsFolderDialog.targetPath,
                                createModsFolderDialog.existingName,
                                createModsFolderDialog.useForProfile)
                            return
                        }

                        var res = modManager.createModsFolderForCurrentPreset(createModsFolderDialog.selectedName)
                        if (res.status === "created") {
                            createModsFolderDialog.close()
                            createModsFolderDialog.folderCreated(
                                res.path,
                                createModsFolderDialog.selectedName,
                                createModsFolderDialog.useForProfile)
                        } else {
                            createModsFolderDialog.refresh(false)
                            createModsFolderDialog.createFailed(res.message || "Could not create the folder")
                        }
                    }
                }
            }
        }
    }
}
