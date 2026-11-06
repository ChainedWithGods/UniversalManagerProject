import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects
import QtQuick.Dialogs

Item {
    id: modsFolderDialogsPart
    anchors.fill: parent

    property alias deleteModsFolderChoiceDialog: deleteModsFolderChoiceDialog
    property alias deleteModsFolderChoiceModel: deleteModsFolderChoiceModel
    property alias deleteModsFolderConfirmDialog: deleteModsFolderConfirmDialog
    property alias modsFolderChoiceDialog: modsFolderChoiceDialog
    property alias modsFolderChoiceModel: modsFolderChoiceModel

    Dialog {
        id: modsFolderChoiceDialog
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 720
        padding: 0
        closePolicy: Popup.NoAutoClose

        ListModel {
            id: modsFolderChoiceModel
        }

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            radius: 20
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.20)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? presetsDialogCozyGradient
                    : modManager.theme === "Crimson" ? presetsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? presetsDialogWaveyGradient
                    : null
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"
        }

        contentItem: ColumnLayout {
            spacing: 0
            anchors.fill: parent

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: window.borderColor
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20

                    Text {
                        text: "Choose Mods Folder"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: window.textColor
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 16
                Layout.bottomMargin: 16
                spacing: 12

                Text {
                    text: currentAddGame.name ? currentAddGame.name : "Selected Game"
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    color: window.textColor
                    Layout.fillWidth: true
                }

                Text {
                    text: "Multiple mods folders were found. Pick the one to use for this profile."
                    font.pixelSize: 13
                    color: window.textSecondaryColor
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                ListView {
                    id: modsFolderChoiceList
                    model: modsFolderChoiceModel
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(360, modsFolderChoiceModel.count * 76)
                    clip: true
                    spacing: 8

                    delegate: Rectangle {
                        id: choiceDelegate
                        width: modsFolderChoiceList.width

                        height: Math.max(60, choiceDelegateContent.implicitHeight + 16)
                        radius: 10
                        color: index === currentAddChoiceIndex
                            ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.18)
                            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)
                        border.width: index === currentAddChoiceIndex ? 1 : 0
                        border.color: window.accentColor

                        ColumnLayout {
                            id: choiceDelegateContent
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            anchors.topMargin: 8
                            anchors.bottomMargin: 8
                            spacing: 4

                            Text {
                                text: model.name + " • " + model.count + " mods"
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                color: window.textColor
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Text {
                                text: model.path
                                font.pixelSize: 12
                                color: window.textSecondaryColor
                                Layout.fillWidth: true
                                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                elide: Text.ElideNone
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: currentAddChoiceIndex = index
                        }
                    }
                }
            }

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
                        Layout.preferredWidth: skipFolderText.width + 32
                        Layout.preferredHeight: 40
                        radius: 10
                        color: skipFolderMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: skipFolderText
                            anchors.centerIn: parent
                            text: "Skip"
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            color: window.textSecondaryColor
                        }

                        MouseArea {
                            id: skipFolderMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                modsFolderChoiceDialog.close()
                                processNextAddGame()
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: chooseFolderText.width + 40
                        Layout.preferredHeight: 40
                        radius: 10
                        color: chooseFolderMouse.containsMouse ? Qt.darker(window.accentColor, 1.1) : window.accentColor
                        opacity: modsFolderChoiceModel.count > 0 ? 1.0 : 0.6

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: chooseFolderText
                            anchors.centerIn: parent
                            text: "Use Folder"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            id: chooseFolderMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: modsFolderChoiceModel.count > 0
                            cursorShape: modsFolderChoiceModel.count > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                if (modsFolderChoiceModel.count > 0) {
                                    var choice = modsFolderChoiceModel.get(currentAddChoiceIndex)
                                    currentAddGame.modsFolderPath = choice.path
                                    pendingAddGames.push(currentAddGame)
                                }
                                modsFolderChoiceDialog.close()
                                processNextAddGame()
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: deleteModsFolderChoiceDialog
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 720
        padding: 0
        closePolicy: Popup.NoAutoClose

        ListModel {
            id: deleteModsFolderChoiceModel
        }

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            radius: 20
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.20)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? presetsDialogCozyGradient
                    : modManager.theme === "Crimson" ? presetsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? presetsDialogWaveyGradient
                    : null
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"
        }

        contentItem: ColumnLayout {
            spacing: 0
            anchors.fill: parent

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: window.borderColor
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20

                    Text {
                        text: "Choose Mods Folder"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: window.textColor
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 16
                Layout.bottomMargin: 16
                spacing: 12

                Text {
                    text: currentDeleteGame.name ? currentDeleteGame.name : "Selected Game"
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    color: window.textColor
                    Layout.fillWidth: true
                }

                Text {
                    text: "Multiple mods folders were found. Pick the one to delete."
                    font.pixelSize: 13
                    color: window.textSecondaryColor
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                ListView {
                    id: deleteModsFolderChoiceList
                    model: deleteModsFolderChoiceModel
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(360, deleteModsFolderChoiceModel.count * 76)
                    clip: true
                    spacing: 8

                    delegate: Rectangle {
                        id: deleteChoiceDelegate
                        width: deleteModsFolderChoiceList.width

                        height: Math.max(60, deleteChoiceDelegateContent.implicitHeight + 16)
                        radius: 10
                        color: index === currentDeleteChoiceIndex
                            ? Qt.rgba(231/255, 76/255, 60/255, 0.12)
                            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.03)
                        border.width: index === currentDeleteChoiceIndex ? 1 : 0
                        border.color: "#E74C3C"

                        ColumnLayout {
                            id: deleteChoiceDelegateContent
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            anchors.topMargin: 8
                            anchors.bottomMargin: 8
                            spacing: 4

                            Text {
                                text: model.name + " • " + model.count + " mods"
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                color: window.textColor
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Text {
                                text: model.path
                                font.pixelSize: 12
                                color: window.textSecondaryColor
                                Layout.fillWidth: true
                                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                elide: Text.ElideNone
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: currentDeleteChoiceIndex = index
                        }
                    }
                }
            }

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
                        Layout.preferredWidth: skipDeleteFolderText.width + 32
                        Layout.preferredHeight: 40
                        radius: 10
                        color: skipDeleteFolderMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: skipDeleteFolderText
                            anchors.centerIn: parent
                            text: "Skip"
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            color: window.textSecondaryColor
                        }

                        MouseArea {
                            id: skipDeleteFolderMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                deleteModsFolderChoiceDialog.close()
                                processNextDeleteGame()
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: deleteFolderText.width + 40
                        Layout.preferredHeight: 40
                        radius: 10
                        color: deleteFolderMouse.containsMouse ? Qt.darker("#E74C3C", 1.1) : "#E74C3C"
                        opacity: deleteModsFolderChoiceModel.count > 0 ? 1.0 : 0.6

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: deleteFolderText
                            anchors.centerIn: parent
                            text: "Delete Folder"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            id: deleteFolderMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: deleteModsFolderChoiceModel.count > 0
                            cursorShape: deleteModsFolderChoiceModel.count > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                if (deleteModsFolderChoiceModel.count > 0) {
                                    var choice = deleteModsFolderChoiceModel.get(currentDeleteChoiceIndex)
                                    if (choice.deleteAll === true) {
                                        for (var i = 0; i < currentDeleteChoices.length; i++) {
                                            var deleteGame = Object.assign({}, currentDeleteGame)
                                            deleteGame.modsFolderPath = currentDeleteChoices[i].path
                                            pendingDeleteGames.push(deleteGame)
                                        }
                                    } else {
                                        currentDeleteGame.modsFolderPath = choice.path
                                        pendingDeleteGames.push(currentDeleteGame)
                                    }
                                }
                                deleteModsFolderChoiceDialog.close()
                                processNextDeleteGame()
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: deleteModsFolderConfirmDialog
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 420
        padding: 0
        closePolicy: Popup.CloseOnEscape

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
            radius: 20
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.15)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? presetsDialogCozyGradient
                    : modManager.theme === "Crimson" ? presetsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? presetsDialogWaveyGradient
                    : null
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.15)
                shadowVerticalOffset: 20
                shadowBlur: 1.0
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
                                ctx.lineWidth = 2
                                ctx.lineCap = "round"
                                ctx.lineJoin = "round"

                                var s = 22/24

                                ctx.beginPath()
                                ctx.moveTo(12*s, 3*s)
                                ctx.lineTo(22*s, 20*s)
                                ctx.lineTo(2*s, 20*s)
                                ctx.closePath()
                                ctx.stroke()

                                ctx.beginPath()
                                ctx.moveTo(12*s, 9*s)
                                ctx.lineTo(12*s, 14*s)
                                ctx.stroke()

                                ctx.beginPath()
                                ctx.arc(12*s, 17*s, 0.8*s, 0, Math.PI * 2)
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
                    text: "Delete Mods Folder?"
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    color: window.textColor
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    property int selectedCount: {
                        var count = 0
                        var list = modManager ? modManager.installedGamesList : []
                        for (var i = 0; i < list.length; i++) {
                            if (selectedGames[list[i].path] === true) count++
                        }
                        return count
                    }
                    text: selectedCount === 1
                        ? "This will permanently delete the Mods folder for the selected game. All mods inside will be lost."
                        : "This will permanently delete the Mods folder for " + selectedCount + " selected games. All mods inside will be lost."
                    font.pixelSize: 13
                    color: window.textSecondaryColor
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    lineHeight: 1.4
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    radius: 8
                    color: Qt.rgba(231/255, 76/255, 60/255, 0.08)
                    border.width: 1
                    border.color: Qt.rgba(231/255, 76/255, 60/255, 0.2)

                    Text {
                        anchors.centerIn: parent
                        text: "This action cannot be undone"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: "#E74C3C"
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
                        Layout.preferredWidth: cancelDeleteModsText.width + 32
                        Layout.preferredHeight: 40
                        radius: 10
                        color: cancelDeleteModsMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : "transparent"

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: cancelDeleteModsText
                            anchors.centerIn: parent
                            text: "Cancel"
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            color: window.textSecondaryColor
                        }

                        MouseArea {
                            id: cancelDeleteModsMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: deleteModsFolderConfirmDialog.close()
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: confirmDeleteModsText.width + 40
                        Layout.preferredHeight: 40
                        radius: 10
                        color: confirmDeleteModsMouse.containsMouse ? Qt.darker("#E74C3C", 1.15) : "#E74C3C"

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: confirmDeleteModsText
                            anchors.centerIn: parent
                            text: "Delete"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            id: confirmDeleteModsMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                startDeleteSelectedGames()
                                deleteModsFolderConfirmDialog.close()
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: deletingLoadingOverlay
        parent: Overlay.overlay
        x: 0
        y: 0
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        color: Qt.rgba(0, 0, 0, 0.5)
        z: 10000
        visible: isDeletingModsFolders

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true
        }

        Rectangle {
            anchors.centerIn: parent
            width: 380
            height: 280
            radius: 24
            color: isGlassmorphic ? Qt.rgba(0.12, 0.12, 0.12, 1.0) : window.surfaceColor
            border.width: 1
            border.color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.3)

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 28

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    width: 80
                    height: 80

                    Rectangle {
                        anchors.centerIn: parent
                        width: 80
                        height: 80
                        radius: 40
                        color: "transparent"
                        border.width: 3
                        border.color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.15)
                    }

                    Canvas {
                        id: middleRingCanvas
                        anchors.centerIn: parent
                        width: 70
                        height: 70
                        rotation: 0

                        property color currentAccentColor: window.accentColor
                        onCurrentAccentColorChanged: requestPaint()

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)

                            var gradient = ctx.createLinearGradient(0, 0, width, height)
                            gradient.addColorStop(0, Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 1))
                            gradient.addColorStop(0.5, Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.6))
                            gradient.addColorStop(1, Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.2))

                            ctx.strokeStyle = gradient
                            ctx.lineWidth = 3
                            ctx.lineCap = "round"

                            ctx.beginPath()
                            ctx.arc(width/2, height/2, (width-3)/2, 0, Math.PI * 1.5)
                            ctx.stroke()
                        }

                        RotationAnimator on rotation {
                            running: deletingLoadingOverlay.visible
                            from: 0
                            to: 360
                            duration: 1200
                            loops: Animation.Infinite
                        }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: 50
                        height: 50
                        radius: 25
                        color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.2)
                        border.width: 2
                        border.color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.4)

                        SequentialAnimation on scale {
                            running: deletingLoadingOverlay.visible
                            loops: Animation.Infinite
                            NumberAnimation { from: 1.0; to: 1.15; duration: 700; easing.type: Easing.InOutQuad }
                            NumberAnimation { from: 1.15; to: 1.0; duration: 700; easing.type: Easing.InOutQuad }
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 12
                            height: 12
                            radius: 6
                            color: window.accentColor
                        }
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.leftMargin: 40
                    Layout.rightMargin: 40
                    spacing: 10

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Deleting Mods Folders"
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        color: window.textColor
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Removing the selected folders..."
                        font.pixelSize: 13
                        color: window.textSecondaryColor
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true

                        SequentialAnimation on opacity {
                            running: deletingLoadingOverlay.visible
                            loops: Animation.Infinite
                            NumberAnimation { from: 1.0; to: 0.5; duration: 900; easing.type: Easing.InOutQuad }
                            NumberAnimation { from: 0.5; to: 1.0; duration: 900; easing.type: Easing.InOutQuad }
                        }
                    }
                }
            }

            scale: deletingLoadingOverlay.visible ? 1.0 : 0.9

            Behavior on scale {
                NumberAnimation { duration: 300; easing.type: Easing.OutBack }
            }
        }
    }
}
