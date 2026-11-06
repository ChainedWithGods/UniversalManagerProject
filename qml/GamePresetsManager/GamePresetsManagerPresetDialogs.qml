import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects
import QtQuick.Dialogs

Item {
    id: presetDialogsPart
    anchors.fill: parent

    property alias createPresetDialog: createPresetDialog
    property alias deleteConfirmDialog: deleteConfirmDialog
    property alias renameDialog: renameDialog

    Dialog {
        id: createPresetDialog
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 420
        padding: 0

        property string dialogStep: "name"
        property string pendingPresetName: ""

        onOpened: {
            dialogStep = "name"
            pendingPresetName = ""
            newPresetNameField.text = ""
            newPresetNameField.forceActiveFocus()
        }

        onClosed: {
            dialogStep = "name"
            pendingPresetName = ""
            newPresetNameField.text = ""
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.9; to: 1; duration: 200; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 150; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; from: 1; to: 0.9; duration: 150; easing.type: Easing.InCubic }
        }

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            radius: 16
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(255, 255, 255, 0.1)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? presetsDialogCozyGradient
                    : modManager.theme === "Crimson" ? presetsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? presetsDialogWaveyGradient
                    : null
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowVerticalOffset: 12
                shadowBlur: 2.5
            }
        }

        contentItem: Item {
            implicitHeight: createPresetDialog.dialogStep === "name" ? nameStepContent.implicitHeight : choiceStepContent.implicitHeight

            Behavior on implicitHeight { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

            ColumnLayout {
                id: nameStepContent
                anchors.fill: parent
                spacing: 0
                opacity: createPresetDialog.dialogStep === "name" ? 1 : 0
                visible: opacity > 0

                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    color: "transparent"

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 8

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: 48
                            height: 48
                            radius: 14
                            color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.15)

                            Text {
                                anchors.centerIn: parent
                                text: "+"
                                font.pixelSize: 24
                                font.weight: Font.Medium
                                color: window.accentColor
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 28
                    Layout.rightMargin: 28
                    spacing: 8

                    Text {
                        text: "Create New Profile"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: window.textColor
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "Enter a name for your new game profile"
                        font.pixelSize: 13
                        color: window.textSecondaryColor
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Item { Layout.preferredHeight: 12 }

                    Rectangle {
                        id: newPresetNameFieldContainer
                        Layout.fillWidth: true
                        height: 48
                        radius: 12
                        color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05)
                        border.width: newPresetNameField.activeFocus ? 2 : 1
                        border.color: newPresetNameField.activeFocus ? window.accentColor : window.borderColor

                        Behavior on border.color { ColorAnimation { duration: 150 } }
                        Behavior on border.width { NumberAnimation { duration: 150 } }

                        TextField {
                            id: newPresetNameField
                            ContextMenu.menu: null
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            verticalAlignment: Text.AlignVCenter
                            placeholderText: "Profile name..."
                            placeholderTextColor: window.textSecondaryColor
                            font.pixelSize: 14
                            color: window.textColor
                            background: null
                            selectByMouse: true
                            persistentSelection: true

                            Keys.onReturnPressed: {
                                if (text.trim() !== "") {
                                    createPresetDialog.pendingPresetName = text.trim()
                                    createPresetDialog.dialogStep = "choice"
                                }
                            }
                        }

                        MouseArea {
                            z: 1
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            cursorShape: Qt.IBeamCursor
                            onPressed: function(mouse) {
                                newPresetNameField.forceActiveFocus()
                                newPresetContextMenu.popup(newPresetNameFieldContainer, mouse.x, newPresetNameFieldContainer.height + 4)
                                mouse.accepted = true
                            }
                            onPressAndHold: function(mouse) {
                                newPresetNameField.forceActiveFocus()
                                newPresetContextMenu.popup(newPresetNameFieldContainer, mouse.x, newPresetNameFieldContainer.height + 4)
                                mouse.accepted = true
                            }
                        }

                        Menu {
                            id: newPresetContextMenu
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
                                enabled: newPresetNameField.canUndo
                                onTriggered: newPresetNameField.undo()
                            }
                            RuntimeContextMenuItem {
                                text: "Redo"
                                iconName: "redo-2.svg"
                                enabled: newPresetNameField.canRedo
                                onTriggered: newPresetNameField.redo()
                            }
                            MenuSeparator {}
                            RuntimeContextMenuItem {
                                text: "Cut"
                                iconName: "scissors.svg"
                                enabled: newPresetNameField.selectedText.length > 0
                                onTriggered: newPresetNameField.cut()
                            }
                            RuntimeContextMenuItem {
                                text: "Copy"
                                iconName: "copy.svg"
                                enabled: newPresetNameField.selectedText.length > 0
                                onTriggered: newPresetNameField.copy()
                            }
                            RuntimeContextMenuItem {
                                text: "Paste"
                                iconName: "clipboard.svg"
                                enabled: true
                                onTriggered: newPresetNameField.paste()
                            }
                            RuntimeContextMenuItem {
                                text: "Delete"
                                iconName: "trash.svg"
                                enabled: newPresetNameField.selectedText.length > 0
                                onTriggered: newPresetNameField.remove(newPresetNameField.selectionStart, newPresetNameField.selectionEnd)
                            }
                            MenuSeparator {}
                            RuntimeContextMenuItem {
                                text: "Select All"
                                iconName: "text-select.svg"
                                enabled: newPresetNameField.text.length > 0
                                onTriggered: newPresetNameField.selectAll()
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 24 }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)

                    Rectangle {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: window.borderColor
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 12

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            Layout.preferredWidth: cancelCreateText.width + 32
                            Layout.preferredHeight: 40
                            radius: 10
                            color: cancelCreateMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"

                            Behavior on color { ColorAnimation { duration: 150 } }

                            Text {
                                id: cancelCreateText
                                anchors.centerIn: parent
                                text: "Cancel"
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                color: window.textSecondaryColor
                            }

                            MouseArea {
                                id: cancelCreateMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: createPresetDialog.close()
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: continueBtnText.width + 40
                            Layout.preferredHeight: 40
                            radius: 10
                            color: continueBtnMouse.containsMouse ? Qt.darker(window.accentColor, 1.1) : window.accentColor
                            opacity: newPresetNameField.text.trim() !== "" ? 1.0 : 0.5

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on opacity { NumberAnimation { duration: 150 } }

                            Text {
                                id: continueBtnText
                                anchors.centerIn: parent
                                text: "Continue"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: "#FFFFFF"
                            }

                            MouseArea {
                                id: continueBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: newPresetNameField.text.trim() !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: {
                                    if (newPresetNameField.text.trim() !== "") {
                                        createPresetDialog.pendingPresetName = newPresetNameField.text.trim()
                                        createPresetDialog.dialogStep = "choice"
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                id: choiceStepContent
                anchors.fill: parent
                spacing: 0
                opacity: createPresetDialog.dialogStep === "choice" ? 1 : 0
                visible: opacity > 0

                Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    color: "transparent"

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 8

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: 48
                            height: 48
                            radius: 14
                            color: Qt.rgba(46/255, 204/255, 113/255, 0.15)

                            Canvas {
                                anchors.centerIn: parent
                                width: 22
                                height: 22
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.strokeStyle = "#2ECC71"
                                    ctx.lineWidth = 2.5
                                    ctx.lineCap = "round"
                                    ctx.lineJoin = "round"

                                    ctx.beginPath()
                                    ctx.moveTo(4, 11)
                                    ctx.lineTo(9, 16)
                                    ctx.lineTo(18, 6)
                                    ctx.stroke()
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 28
                    Layout.rightMargin: 28
                    spacing: 8

                    Text {
                        text: "Profile Created!"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: window.textColor
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "\"" + createPresetDialog.pendingPresetName + "\""
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: window.accentColor
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Item { Layout.preferredHeight: 4 }

                    Text {
                        text: "Would you like to configure the settings now?"
                        font.pixelSize: 13
                        color: window.textSecondaryColor
                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Item { Layout.preferredHeight: 16 }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            id: configureNowCard
                            Layout.fillWidth: true
                            height: 64
                            radius: 12
                            color: configureNowMouse.containsMouse ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.1) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)
                            border.width: configureNowMouse.containsMouse ? 2 : 1
                            border.color: configureNowMouse.containsMouse ? window.accentColor : window.borderColor

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                Rectangle {
                                    width: 36
                                    height: 36
                                    radius: 10
                                    color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.15)

                                    Canvas {
                                        anchors.centerIn: parent
                                        width: 18
                                        height: 18
                                        onPaint: {
                                            var ctx = getContext("2d")
                                            ctx.clearRect(0, 0, width, height)
                                            ctx.strokeStyle = window.accentColor
                                            ctx.lineWidth = 1.5
                                            ctx.lineCap = "round"
                                            ctx.lineJoin = "round"

                                            var s = 18/24

                                            ctx.beginPath()
                                            ctx.arc(12*s, 12*s, 3*s, 0, Math.PI * 2)
                                            ctx.stroke()

                                            var teeth = 8
                                            for (var i = 0; i < teeth; i++) {
                                                var angle = (i / teeth) * Math.PI * 2
                                                var x1 = 12*s + Math.cos(angle) * 5*s
                                                var y1 = 12*s + Math.sin(angle) * 5*s
                                                var x2 = 12*s + Math.cos(angle) * 8*s
                                                var y2 = 12*s + Math.sin(angle) * 8*s
                                                ctx.beginPath()
                                                ctx.moveTo(x1, y1)
                                                ctx.lineTo(x2, y2)
                                                ctx.stroke()
                                            }
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        text: "Configure Now"
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: window.textColor
                                    }

                                    Text {
                                        text: "Configure Universal Manager Now"
                                        font.pixelSize: 12
                                        color: window.textSecondaryColor
                                    }
                                }

                                Text {
                                    text: "→"
                                    font.pixelSize: 16
                                    color: configureNowMouse.containsMouse ? window.accentColor : window.textSecondaryColor

                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }
                            }

                            MouseArea {
                                id: configureNowMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    modManager.createPresetAndReset(createPresetDialog.pendingPresetName)
                                    createPresetDialog.close()
                                }
                            }
                        }

                        Rectangle {
                            id: configureLaterCard
                            Layout.fillWidth: true
                            height: 64
                            radius: 12
                            color: configureLaterMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)
                            border.width: 1
                            border.color: configureLaterMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.2) : window.borderColor

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                Rectangle {
                                    width: 36
                                    height: 36
                                    radius: 10
                                    color: Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g, window.textSecondaryColor.b, 0.15)

                                    Canvas {
                                        anchors.centerIn: parent
                                        width: 18
                                        height: 18
                                        onPaint: {
                                            var ctx = getContext("2d")
                                            ctx.clearRect(0, 0, width, height)
                                            ctx.strokeStyle = window.textSecondaryColor
                                            ctx.lineWidth = 1.5
                                            ctx.lineCap = "round"
                                            ctx.lineJoin = "round"

                                            var s = 18/24

                                            ctx.beginPath()
                                            ctx.arc(12*s, 12*s, 9*s, 0, Math.PI * 2)
                                            ctx.stroke()

                                            ctx.beginPath()
                                            ctx.moveTo(12*s, 12*s)
                                            ctx.lineTo(12*s, 7*s)
                                            ctx.stroke()

                                            ctx.beginPath()
                                            ctx.moveTo(12*s, 12*s)
                                            ctx.lineTo(16*s, 12*s)
                                            ctx.stroke()
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        text: "Configure Later"
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: window.textColor
                                    }

                                    Text {
                                        text: "Nah Keep current settings for now"
                                        font.pixelSize: 12
                                        color: window.textSecondaryColor
                                    }
                                }

                                Text {
                                    text: "→"
                                    font.pixelSize: 16
                                    color: configureLaterMouse.containsMouse ? window.textColor : window.textSecondaryColor

                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }
                            }

                            MouseArea {
                                id: configureLaterMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    modManager.createPreset(createPresetDialog.pendingPresetName)
                                    createPresetDialog.close()
                                }
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 24 }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)

                    Rectangle {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: window.borderColor
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 12

                        Rectangle {
                            Layout.preferredWidth: backBtnText.width + 32
                            Layout.preferredHeight: 36
                            radius: 8
                            color: backBtnMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"

                            Behavior on color { ColorAnimation { duration: 150 } }

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 6

                                Text {
                                    text: "←"
                                    font.pixelSize: 14
                                    color: window.textSecondaryColor
                                }

                                Text {
                                    id: backBtnText
                                    text: "Back"
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: window.textSecondaryColor
                                }
                            }

                            MouseArea {
                                id: backBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    createPresetDialog.dialogStep = "name"
                                    newPresetNameField.text = createPresetDialog.pendingPresetName
                                    newPresetNameField.forceActiveFocus()
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }

    Dialog {
        id: renameDialog
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 420
        padding: 0

        property string targetId: ""
        property string currentName: ""

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.9; to: 1; duration: 200; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 150; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; from: 1; to: 0.9; duration: 150; easing.type: Easing.InCubic }
        }

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            radius: 16
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(255, 255, 255, 0.1)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? presetsDialogCozyGradient
                    : modManager.theme === "Crimson" ? presetsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? presetsDialogWaveyGradient
                    : null
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowVerticalOffset: 12
                shadowBlur: 2.5
            }
        }

        onOpened: {
            renameField.text = currentName
            renameField.selectAll()
            renameField.forceActiveFocus()
        }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                color: "transparent"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 48
                        height: 48
                        radius: 14
                        color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.15)

                        Canvas {
                            anchors.centerIn: parent
                            width: 22
                            height: 22
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.strokeStyle = window.accentColor
                                ctx.lineWidth = 1.8
                                ctx.lineCap = "round"
                                ctx.lineJoin = "round"

                                var s = 22/24
                                ctx.beginPath()
                                ctx.moveTo(17*s, 3*s)
                                ctx.lineTo(21*s, 7*s)
                                ctx.lineTo(8*s, 20*s)
                                ctx.lineTo(3*s, 21*s)
                                ctx.lineTo(4*s, 16*s)
                                ctx.closePath()
                                ctx.stroke()

                                ctx.beginPath()
                                ctx.moveTo(15*s, 5*s)
                                ctx.lineTo(19*s, 9*s)
                                ctx.stroke()
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 8

                Text {
                    text: "Rename Profile"
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    color: window.textColor
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: "Enter a new name for this profile"
                    font.pixelSize: 13
                    color: window.textSecondaryColor
                    Layout.alignment: Qt.AlignHCenter
                }

                Item { Layout.preferredHeight: 12 }

                Rectangle {
                    id: renameFieldContainer
                    Layout.fillWidth: true
                    height: 48
                    radius: 12
                    color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.05)
                    border.width: renameField.activeFocus ? 2 : 1
                    border.color: renameField.activeFocus ? window.accentColor : window.borderColor

                    Behavior on border.color { ColorAnimation { duration: 150 } }
                    Behavior on border.width { NumberAnimation { duration: 150 } }

                    TextField {
                        id: renameField
                        ContextMenu.menu: null
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        verticalAlignment: Text.AlignVCenter
                        placeholderText: "Profile name..."
                        placeholderTextColor: window.textSecondaryColor
                        font.pixelSize: 14
                        color: window.textColor
                        background: null
                        selectByMouse: true
                        persistentSelection: true
                    }

                    MouseArea {
                        z: 1
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        cursorShape: Qt.IBeamCursor
                        onPressed: function(mouse) {
                            renameField.forceActiveFocus()
                            renameContextMenu.popup(renameFieldContainer, mouse.x, renameFieldContainer.height + 4)
                            mouse.accepted = true
                        }
                        onPressAndHold: function(mouse) {
                            renameField.forceActiveFocus()
                            renameContextMenu.popup(renameFieldContainer, mouse.x, renameFieldContainer.height + 4)
                            mouse.accepted = true
                        }
                    }

                    Menu {
                        id: renameContextMenu
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
                            enabled: renameField.canUndo
                            onTriggered: renameField.undo()
                        }
                        RuntimeContextMenuItem {
                            text: "Redo"
                            iconName: "redo-2.svg"
                            enabled: renameField.canRedo
                            onTriggered: renameField.redo()
                        }
                        MenuSeparator {}
                        RuntimeContextMenuItem {
                            text: "Cut"
                            iconName: "scissors.svg"
                            enabled: renameField.selectedText.length > 0
                            onTriggered: renameField.cut()
                        }
                        RuntimeContextMenuItem {
                            text: "Copy"
                            iconName: "copy.svg"
                            enabled: renameField.selectedText.length > 0
                            onTriggered: renameField.copy()
                        }
                        RuntimeContextMenuItem {
                            text: "Paste"
                            iconName: "clipboard.svg"
                            enabled: true
                            onTriggered: renameField.paste()
                        }
                        RuntimeContextMenuItem {
                            text: "Delete"
                            iconName: "trash.svg"
                            enabled: renameField.selectedText.length > 0
                            onTriggered: renameField.remove(renameField.selectionStart, renameField.selectionEnd)
                        }
                        MenuSeparator {}
                        RuntimeContextMenuItem {
                            text: "Select All"
                            iconName: "text-select.svg"
                            enabled: renameField.text.length > 0
                            onTriggered: renameField.selectAll()
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 24 }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: window.borderColor
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 12

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        Layout.preferredWidth: cancelRenameText.width + 32
                        Layout.preferredHeight: 40
                        radius: 10
                        color: cancelRenameMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: cancelRenameText
                            anchors.centerIn: parent
                            text: "Cancel"
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            color: window.textSecondaryColor
                        }

                        MouseArea {
                            id: cancelRenameMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: renameDialog.close()
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: renameBtnText.width + 40
                        Layout.preferredHeight: 40
                        radius: 10
                        color: renameBtnMouse.containsMouse ? Qt.darker(window.accentColor, 1.1) : window.accentColor

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: renameBtnText
                            anchors.centerIn: parent
                            text: "Save Changes"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            id: renameBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (renameField.text.trim() !== "") {
                                    modManager.renamePreset(renameDialog.targetId, renameField.text.trim())
                                    renameDialog.close()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: deleteConfirmDialog
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 420
        padding: 0

        property string targetPresetId: ""
        property string targetPresetName: ""
        property bool isLastProfile: modManager.gamePresets.length <= 1

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.9; to: 1; duration: 200; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 150; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; from: 1; to: 0.9; duration: 150; easing.type: Easing.InCubic }
        }

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            radius: 16
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(255, 255, 255, 0.1)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? presetsDialogCozyGradient
                    : modManager.theme === "Crimson" ? presetsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? presetsDialogWaveyGradient
                    : null
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowVerticalOffset: 12
                shadowBlur: 2.5
            }
        }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                color: "transparent"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 48
                        height: 48
                        radius: 14
                        color: Qt.rgba(231/255, 76/255, 60/255, 0.15)

                        Canvas {
                            anchors.centerIn: parent
                            width: 22
                            height: 22
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.strokeStyle = "#E74C3C"
                                ctx.lineWidth = 1.8
                                ctx.lineCap = "round"
                                ctx.lineJoin = "round"

                                var s = 22/24

                                ctx.beginPath()
                                ctx.moveTo(10*s, 11*s)
                                ctx.lineTo(10*s, 17*s)
                                ctx.stroke()

                                ctx.beginPath()
                                ctx.moveTo(14*s, 11*s)
                                ctx.lineTo(14*s, 17*s)
                                ctx.stroke()

                                ctx.beginPath()
                                ctx.moveTo(19*s, 6*s)
                                ctx.lineTo(19*s, 20*s)
                                ctx.quadraticCurveTo(19*s, 22*s, 17*s, 22*s)
                                ctx.lineTo(7*s, 22*s)
                                ctx.quadraticCurveTo(5*s, 22*s, 5*s, 20*s)
                                ctx.lineTo(5*s, 6*s)
                                ctx.stroke()

                                ctx.beginPath()
                                ctx.moveTo(3*s, 6*s)
                                ctx.lineTo(21*s, 6*s)
                                ctx.stroke()

                                ctx.beginPath()
                                ctx.moveTo(8*s, 6*s)
                                ctx.lineTo(8*s, 4*s)
                                ctx.quadraticCurveTo(8*s, 2*s, 10*s, 2*s)
                                ctx.lineTo(14*s, 2*s)
                                ctx.quadraticCurveTo(16*s, 2*s, 16*s, 4*s)
                                ctx.lineTo(16*s, 6*s)
                                ctx.stroke()
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 12

                Text {
                    text: deleteConfirmDialog.isLastProfile ? "Reset All Settings?" : "Delete Profile?"
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    color: window.textColor
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    text: deleteConfirmDialog.isLastProfile
                        ? "This is your only profile. Deleting it will reset all settings, paths, and mod data back to defaults.\n\nYou can create a new profile anytime from the Games Manager."
                        : "Are you sure you want to delete \"" + deleteConfirmDialog.targetPresetName + "\"?\nThis action cannot be undone."
                    font.pixelSize: 13
                    color: window.textSecondaryColor
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    lineHeight: 1.4
                }
            }

            Item { Layout.preferredHeight: 24 }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: window.borderColor
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 12

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        Layout.preferredWidth: cancelDeleteText.width + 32
                        Layout.preferredHeight: 40
                        radius: 10
                        color: cancelDeleteMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: cancelDeleteText
                            anchors.centerIn: parent
                            text: "Cancel"
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            color: window.textSecondaryColor
                        }

                        MouseArea {
                            id: cancelDeleteMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: deleteConfirmDialog.close()
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: deleteBtnText.width + 40
                        Layout.preferredHeight: 40
                        radius: 10
                        color: deleteBtnMouse.containsMouse ? Qt.darker("#E74C3C", 1.1) : "#E74C3C"

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: deleteBtnText
                            anchors.centerIn: parent
                            text: deleteConfirmDialog.isLastProfile ? "Reset Everything" : "Delete Profile"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            id: deleteBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                modManager.deletePreset(deleteConfirmDialog.targetPresetId)
                                deleteConfirmDialog.close()
                            }
                        }
                    }
                }
            }
        }
    }
}
