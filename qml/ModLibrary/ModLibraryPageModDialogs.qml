import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Item {
    id: modDialogs
    anchors.fill: parent

    property alias deleteConfirmDialog: deleteConfirmDialog
    property alias imageFileDialog: imageFileDialog
    property alias modDetailsPanel: modDetailsPanel
    property alias panelModGif: panelModGif
    property alias panelModImage: panelModImage

    function openBundleTag(bundleId) {
        addTagDialog.bundleTargetId = bundleId
        addTagDialog.open()
    }

    Dialog {
        id: modDetailsPanel
        modal: true
        anchors.centerIn: Overlay.overlay

        width: 1050

        height: 580
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: {
            var savedImagePath = modManager.getModImage(selectedModName)
            if (savedImagePath !== "") {
                if (savedImagePath.toLowerCase().endsWith(".gif")) {
                    panelModGif.source = savedImagePath
                    panelModImage.source = ""
                } else {
                    panelModImage.source = savedImagePath
                    panelModGif.source = ""
                }
            }
            selectedModDescription = modManager.getModDescription(selectedDisplayName)
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.75)
            Behavior on opacity { NumberAnimation { duration: 300 } }
        }

        enter: Transition {
            NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 250; easing.type: Easing.OutCubic }
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 250 }
        }

        exit: Transition {
            NumberAnimation { property: "scale"; from: 1.0; to: 0.95; duration: 200; easing.type: Easing.InCubic }
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 200 }
        }

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            id: dialogBg
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.1, 0.1, 0.12, 0.95) : window.surfaceColor
            radius: 16
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.1)
            gradient: {
                if (modManager.theme === "Cozy") return sharedCozyDialogGradient
                if (modManager.theme === "Crimson") return sharedCrimsonDialogGradient
                if (modManager.theme === "Wavey") return sharedWaveyDialogGradient
                return null
            }
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.5)
                shadowBlur: 1.0
                shadowVerticalOffset: 10
            }
        }

        contentItem: Item {

            Button {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 16
                width: 36
                height: 36
                flat: true
                text: "✕"
                z: 100

                contentItem: Text {
                    text: parent.text
                    color: "#FFFFFF"
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    style: Text.Outline
                    styleColor: "#000000"
                }
                background: Rectangle {
                    color: parent.hovered ? Qt.rgba(0,0,0,0.7) : Qt.rgba(0,0,0,0.4)
                    radius: 18
                    border.width: 1
                    border.color: Qt.rgba(1,1,1,0.2)
                }
                onClicked: modDetailsPanel.close()
            }

            RowLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    id: modImageContainer
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width * 0.50
                    color: "#1E1E1E"

                    radius: 16

                    layer.enabled: true
                    layer.effect: MultiEffect {
                        maskEnabled: true
                        maskThresholdMin: 0.5
                        maskSpreadAtMin: 1.0
                        maskSource: ShaderEffectSource {
                            sourceItem: Rectangle {
                                width: modImageContainer.width
                                height: modImageContainer.height
                                radius: 16
                            }
                        }
                    }

                    Image {
                        anchors.fill: parent
                        source: panelModImage.source != "" ? panelModImage.source : (panelModGif.source != "" ? panelModGif.source : "")
                        fillMode: Image.PreserveAspectCrop

                        visible: (panelModImage.visible || panelModGif.visible)
                        opacity: 0.4
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            blurEnabled: true
                            blurMax: 64
                            blur: 1.0
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.3) }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                        visible: !panelModImage.visible && !panelModGif.visible
                    }

                    Image {
                        id: panelModImage
                        anchors.fill: parent
                        anchors.margins: 0
                        fillMode: Image.PreserveAspectFit
                        visible: source != "" && !source.toString().toLowerCase().endsWith(".gif")
                        mipmap: true
                        cache: false
                        asynchronous: true
                    }

                    AnimatedImage {
                        id: panelModGif
                        anchors.fill: parent
                        anchors.margins: 0
                        fillMode: Image.PreserveAspectFit
                        visible: source != "" && source.toString().toLowerCase().endsWith(".gif")
                        playing: visible
                    }

                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Qt.rgba(0,0,0,0.3) }
                            GradientStop { position: 0.6; color: "transparent" }
                            GradientStop { position: 1.0; color: Qt.rgba(0,0,0,0.7) }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            imageFileDialog.open()
                            focus = false
                        }
                        visible: !panelModImage.visible && !panelModGif.visible
                        z: 5
                        Text {
                            anchors.centerIn: parent
                            text: "Click to upload Preview"
                            color: "#FFFFFF"
                            font.pixelSize: 16
                            opacity: 0.9
                            font.weight: Font.DemiBold
                            style: Text.Outline
                            styleColor: Qt.rgba(0,0,0,0.5)
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.margins: 12
                        width: 32
                        height: 32
                        radius: 16
                        color: Qt.rgba(0,0,0,0.5)
                        visible: panelModImage.visible || panelModGif.visible

                        Image {
                            id: changeImageIcon
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            source: "qrc:/UniversalModManager/Lucide Icons/camera.svg"
                            sourceSize: Qt.size(16, 16)
                            fillMode: Image.PreserveAspectFit
                            visible: false
                        }

                        ColorOverlay {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            source: changeImageIcon
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                imageFileDialog.open()
                                focus = false
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    color: modManager.theme === "Glassmorphic"
                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10)
                        : Qt.rgba(0, 0, 0, 0.06)
                }

                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    color: modManager.theme === "Glassmorphic"
                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.02)
                        : Qt.rgba(0, 0, 0, 0.01)

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 32
                        spacing: 16

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: selectedModName
                                font.pixelSize: 28
                                font.weight: Font.ExtraBold
                                color: window.textColor
                                wrapMode: Text.Wrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                spacing: 12

                                Rectangle {
                                    width: statusText.contentWidth + 24
                                    height: 24
                                    radius: 12
                                    color: selectedModDetails.enabled ? "#4CAF50" : "#9E9E9E"

                                    Text {
                                        id: statusText
                                        anchors.centerIn: parent
                                        text: selectedModDetails.enabled ? "ACTIVE" : "DISABLED"
                                        font.pixelSize: 11
                                        font.weight: Font.Bold
                                        color: "#FFFFFF"
                                        font.letterSpacing: 1.0
                                    }
                                }

                                Rectangle {
                                    id: renameModButton
                                    Layout.preferredHeight: 24
                                    width: renameLabel.contentWidth + 30
                                    radius: 12
                                    color: renameModHoverHandler.hovered
                                        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                                        : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.10)
                                    border.width: 1
                                    border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                                    HoverHandler {
                                        id: renameModHoverHandler
                                        cursorShape: Qt.PointingHandCursor
                                    }

                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Image {
                                            source: "qrc:/UniversalModManager/Lucide Icons/pencil.svg"
                                            sourceSize.width: 12
                                            sourceSize.height: 12
                                            fillMode: Image.PreserveAspectFit
                                            mipmap: true
                                            layer.enabled: true
                                            layer.effect: ColorOverlay {
                                                color: window.primaryColor
                                            }
                                        }
                                        Text {
                                            id: renameLabel
                                            text: "RENAME"
                                            font.pixelSize: 11
                                            font.weight: Font.Bold
                                            color: window.primaryColor
                                            font.letterSpacing: 1.0
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {

                                            var nameToRename = selectedDisplayName
                                            if (!nameToRename && selectedModDetails && selectedModDetails.uniqueId) {
                                                nameToRename = selectedModDetails.uniqueId
                                            }
                                            if (nameToRename) {
                                                renameModDialog.targetModName = nameToRename
                                                renameModDialog.open()
                                            }
                                        }
                                    }

                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }

                                Text {
                                    text: "•"
                                    color: window.textSecondaryColor
                                    font.pixelSize: 14
                                }

                                Text {
                                    text: selectedModDetails.size || "N/A"
                                    color: window.textSecondaryColor
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                }

                                Text {
                                    text: "•"
                                    color: window.textSecondaryColor
                                    font.pixelSize: 14
                                }

                                Text {
                                    text: selectedModDetails.installDate || "N/A"
                                    color: window.textSecondaryColor
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 10 }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 48
                            color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08) : Qt.rgba(0,0,0,0.05)
                            radius: 12
                            border.width: 1
                            border.color: Qt.rgba(1,1,1,0.1)

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 8
                                spacing: 12

                                Text {
                                    Layout.fillWidth: true
                                    text: selectedModDetails.path || ""
                                    elide: Text.ElideMiddle
                                    color: window.textSecondaryColor
                                    font.pixelSize: 13
                                    font.family: "Consolas, monospace"
                                }

                                Button {
                                    id: copyBtn
                                    Layout.preferredWidth: 36
                                    Layout.preferredHeight: 36
                                    flat: true
                                    hoverEnabled: true

                                    background: Rectangle {
                                        color: copyBtn.hovered ? Qt.rgba(1,1,1,0.1) : "transparent"
                                        radius: 8
                                    }

                                    contentItem: Item {
                                        Image {
                                            id: copyIcon
                                            anchors.centerIn: parent
                                            width: 16
                                            height: 16
                                            source: "qrc:/UniversalModManager/Lucide Icons/copy.svg"
                                            sourceSize: Qt.size(16, 16)
                                            fillMode: Image.PreserveAspectFit
                                            visible: false
                                        }
                                        ColorOverlay {
                                            anchors.centerIn: parent
                                            width: 16
                                            height: 16
                                            source: copyIcon
                                            color: window.textSecondaryColor
                                        }
                                    }

                                    onClicked: {
                                        var path = selectedModDetails.path || ""
                                        if (path) {
                                            var tempInput = Qt.createQmlObject('import QtQuick; TextInput { visible: false }', modLibraryRoot)
                                            tempInput.text = path
                                            tempInput.selectAll()
                                            tempInput.copy()
                                            tempInput.destroy()
                                            toastNotification.show("Path copied", "copy.svg")
                                        }
                                    }
                                }
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 10

                            Repeater {
                                model: currentTags
                                delegate: Rectangle {
                                    implicitWidth: Math.min(tagContent.implicitWidth + 30, 400)
                                    width: implicitWidth
                                    height: 30
                                    radius: 15
                                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                                    border.width: 1
                                    border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.3)

                                    Row {
                                        id: tagContent
                                        anchors.centerIn: parent
                                        spacing: 8

                                        Text {
                                            text: modelData
                                            color: window.textColor
                                            font.pixelSize: 13
                                            font.weight: Font.Medium
                                            anchors.verticalCenter: parent.verticalCenter
                                            elide: Text.ElideRight
                                            maximumLineCount: 1
                                            width: Math.min(implicitWidth, 350)
                                        }

                                        Rectangle {
                                            width: 18
                                            height: 18
                                            radius: 9
                                            color: closeMouseArea.containsMouse ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.2) : "transparent"
                                            anchors.verticalCenter: parent.verticalCenter

                                            Text {
                                                anchors.centerIn: parent
                                                text: "✕"
                                                color: closeMouseArea.containsMouse ? window.primaryColor : window.textSecondaryColor
                                                font.pixelSize: 10
                                                font.weight: Font.Bold
                                            }

                                            MouseArea {
                                                id: closeMouseArea
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                hoverEnabled: true
                                                onClicked: modManager.removeModTag(selectedModName, modelData)
                                            }
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                id: addTagBtn
                                width: 74
                                height: 28
                                radius: 14
                                border.width: 1
                                border.color: addTagMouse.containsMouse ? window.primaryColor : window.textSecondaryColor
                                color: addTagMouse.containsMouse ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1) : "transparent"

                                Behavior on color { ColorAnimation { duration: 200 } }

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Text {
                                        text: "+"
                                        color: addTagMouse.containsMouse ? window.primaryColor : window.textSecondaryColor
                                        font.pixelSize: 13
                                        font.weight: Font.Bold
                                    }
                                    Text {
                                        text: "Tag"
                                        color: addTagMouse.containsMouse ? window.primaryColor : window.textSecondaryColor
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                    }
                                }
                                MouseArea {
                                    id: addTagMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        addTagDialog.open()
                                        focus = false
                                    }
                                }
                            }
                        }

                        Item { Layout.preferredHeight: 10 }

                        ColumnLayout {
                            id: mainActions
                            Layout.fillWidth: true
                            spacing: 10
                            property int compactButtonHeight: 38
                            property int compactRadius: 11

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 10
                                rowSpacing: 10

                                Button {
                                    id: openPakViewerButton
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: mainActions.compactButtonHeight
                                    hoverEnabled: true

                                    background: Rectangle {
                                        id: openPakViewerBg
                                        property color baseColor: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10)
                                            : Qt.rgba(0, 0, 0, 0.03)

                                        color: openPakViewerButton.hovered
                                               ? Qt.lighter(baseColor, 1.10)
                                               : baseColor
                                        radius: mainActions.compactRadius
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(1, 1, 1, openPakViewerButton.hovered ? 0.34 : 0.22)
                                            : Qt.rgba(0, 0, 0, openPakViewerButton.hovered ? 0.18 : 0.10)
                                        Behavior on color { ColorAnimation { duration: 160 } }
                                        Behavior on border.color { ColorAnimation { duration: 160 } }
                                    }

                                    HoverHandler { cursorShape: Qt.PointingHandCursor }

                                    contentItem: Text {
                                        text: "Assets Viewer"
                                        color: window.textColor
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: {
                                        var pakPath = modManager.getModPakPath(selectedDisplayName)
                                        if (pakPath && pakPath !== "") {
                                            modManager.launchPakViewer(pakPath)
                                            toastNotification.show("Launching PakViewer...", "scan.svg")
                                        } else {
                                            toastNotification.show("No .pak file found for this mod", "file-x.svg")
                                        }
                                    }
                                }

                                Button {
                                    id: exportStructureButton
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: mainActions.compactButtonHeight
                                    hoverEnabled: true

                                    background: Rectangle {
                                        id: exportStructureBg
                                        property color baseColor: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10)
                                            : Qt.rgba(0, 0, 0, 0.03)

                                        color: exportStructureButton.hovered
                                               ? Qt.lighter(baseColor, 1.10)
                                               : baseColor
                                        radius: mainActions.compactRadius
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(1, 1, 1, exportStructureButton.hovered ? 0.34 : 0.22)
                                            : Qt.rgba(0, 0, 0, exportStructureButton.hovered ? 0.18 : 0.10)
                                        Behavior on color { ColorAnimation { duration: 160 } }
                                        Behavior on border.color { ColorAnimation { duration: 160 } }
                                    }

                                    HoverHandler { cursorShape: Qt.PointingHandCursor }

                                    contentItem: Text {
                                        text: "Export Structure"
                                        color: window.textColor
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: {
                                        var missing = []
                                        if (!modManager.gamePaksPath || modManager.gamePaksPath === "") {
                                            missing.push("Game Paks folder")
                                        }
                                        if (!modManager.usmapFilePath || modManager.usmapFilePath === "") {
                                            missing.push("USMAP file")
                                        }

                                        if (missing.length > 0) {
                                            var msg = "Structure exporting needs usmap file"
                                            toastNotification.show(msg, "circle-x.svg")
                                            return
                                        }

                                        modManager.exportModStructure(selectedDisplayName)
                                        toastNotification.show("Exporting mod structure...", "share.svg")
                                    }
                                }

                                Button {
                                    id: describeModButton
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: mainActions.compactButtonHeight
                                    hoverEnabled: true

                                    background: Rectangle {
                                        id: describeModBg
                                        property color baseColor: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10)
                                            : Qt.rgba(0, 0, 0, 0.03)

                                        color: describeModButton.hovered
                                               ? Qt.lighter(baseColor, 1.10)
                                               : baseColor
                                        radius: mainActions.compactRadius
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(1, 1, 1, describeModButton.hovered ? 0.34 : 0.22)
                                            : Qt.rgba(0, 0, 0, describeModButton.hovered ? 0.18 : 0.10)
                                        Behavior on color { ColorAnimation { duration: 160 } }
                                        Behavior on border.color { ColorAnimation { duration: 160 } }
                                    }

                                    HoverHandler { cursorShape: Qt.PointingHandCursor }

                                    contentItem: Text {
                                        text: "Describe Mod"
                                        color: window.textColor
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: {
                                        descriptionField.text = selectedModDescription
                                        modDescriptionDialog.open()
                                    }
                                }

                                Button {
                                    id: openFolderBtn
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: mainActions.compactButtonHeight
                                    hoverEnabled: true

                                    background: Rectangle {
                                        id: openFolderBg
                                        property color baseColor: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10)
                                            : Qt.rgba(0, 0, 0, 0.03)

                                        color: openFolderBtn.hovered
                                               ? Qt.lighter(baseColor, 1.10)
                                               : baseColor
                                        radius: mainActions.compactRadius
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(1, 1, 1, openFolderBtn.hovered ? 0.34 : 0.22)
                                            : Qt.rgba(0, 0, 0, openFolderBtn.hovered ? 0.18 : 0.10)
                                        Behavior on color { ColorAnimation { duration: 160 } }
                                        Behavior on border.color { ColorAnimation { duration: 160 } }
                                    }

                                    HoverHandler { cursorShape: Qt.PointingHandCursor }

                                    contentItem: Text {
                                        text: "Open Folder"
                                        color: window.textColor
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: modManager.openModFolder(selectedModName)
                                }
                            }

                            Button {
                                id: mainActionButton
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredHeight: 48
                                leftPadding: 40
                                rightPadding: 40
                                hoverEnabled: true

                                background: Rectangle {
                                    id: mainActionBg

                                    property color enableColor: modManager.enableButtonColor
                                    property color disableColor: modManager.disableButtonColor
                                    property color baseColor: selectedModDetails.enabled ? disableColor : enableColor

                                    color: mainActionButton.hovered
                                           ? Qt.lighter(baseColor, 1.08)
                                           : baseColor
                                    radius: 13
                                    border.width: 1.2
                                    border.color: Qt.rgba(1, 1, 1, 0.28)

                                    layer.enabled: true
                                    layer.effect: MultiEffect {
                                        shadowEnabled: true
                                        shadowColor: Qt.rgba(0, 0, 0, 0.30)
                                        shadowHorizontalOffset: 0
                                        shadowVerticalOffset: 6
                                        shadowBlur: 0.8
                                    }

                                    Behavior on color { ColorAnimation { duration: 180 } }
                                    Behavior on border.color { ColorAnimation { duration: 180 } }
                                }

                                HoverHandler { cursorShape: Qt.PointingHandCursor }

                                contentItem: Text {
                                    text: selectedModDetails.enabled ? "DISABLE MOD" : "ENABLE MOD"
                                    color: "#FFFFFF"
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    font.letterSpacing: 1.2
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: {
                                    var wasEnabled = selectedModDetails.enabled
                                    modManager.toggleModStatus(selectedDisplayName)
                                    selectedModDetails = modManager.getModDetails(selectedDisplayName)
                                    if (wasEnabled) toastNotification.show("Mod disabled", "pause.svg")
                                    else toastNotification.show("Mod enabled", "play.svg")
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                Layout.topMargin: 2
                                color: window.borderColor
                                opacity: 0.55
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Button {
                                    id: removeImgBtn
                                    Layout.preferredHeight: mainActions.compactButtonHeight
                                    leftPadding: 24
                                    rightPadding: 24
                                    visible: currentModImage !== ""
                                    hoverEnabled: true

                                    background: Rectangle {
                                        id: removeImgBg
                                        property color dangerColor: "#ff5252"

                                        color: removeImgBtn.hovered
                                               ? Qt.rgba(1.0, 0.32, 0.36, 0.24)
                                               : Qt.rgba(1.0, 0.32, 0.36, 0.16)
                                        radius: mainActions.compactRadius
                                        border.width: 1.2
                                        border.color: removeImgBtn.hovered
                                            ? Qt.rgba(1.0, 0.32, 0.36, 0.70)
                                            : Qt.rgba(1.0, 0.32, 0.36, 0.55)
                                        Behavior on color { ColorAnimation { duration: 180 } }
                                        Behavior on border.color { ColorAnimation { duration: 180 } }
                                    }
                                    contentItem: Text {
                                        text: "Remove Preview"
                                        color: removeImgBtn.hovered ? "#FFFFFF" : removeImgBg.dangerColor
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    HoverHandler { cursorShape: Qt.PointingHandCursor }

                                    onClicked: {
                                        modManager.removeModImage(selectedModName)
                                        panelModImage.source = ""
                                        panelModGif.source = ""
                                        toastNotification.show("Preview removed", "trash-2.svg")
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                Button {
                                    id: deleteBtn
                                    Layout.preferredHeight: mainActions.compactButtonHeight
                                    leftPadding: 24
                                    rightPadding: 24
                                    hoverEnabled: true

                                    background: Rectangle {
                                        id: deleteBg
                                        property color dangerColor: "#ff5252"

                                        color: deleteBtn.hovered
                                               ? Qt.rgba(1.0, 0.32, 0.36, 0.24)
                                               : Qt.rgba(1.0, 0.32, 0.36, 0.16)
                                        radius: mainActions.compactRadius
                                        border.width: 1.2
                                        border.color: deleteBtn.hovered
                                            ? Qt.rgba(1.0, 0.32, 0.36, 0.70)
                                            : Qt.rgba(1.0, 0.32, 0.36, 0.55)
                                        Behavior on color { ColorAnimation { duration: 180 } }
                                        Behavior on border.color { ColorAnimation { duration: 180 } }
                                    }
                                    contentItem: Text {
                                        text: "Delete"
                                        color: deleteBtn.hovered ? "#FFFFFF" : deleteBg.dangerColor
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: {
                                        if (deleteProgressDialog.visible)
                                            return
                                        deleteConfirmDialog.isBatchDelete = false
                                        deleteConfirmDialog.open()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: modDescriptionDialog
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 620
        height: 380
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(0.15, 0.15, 0.15, 0.92)
                : window.surfaceColor
            radius: 16
            border.width: modManager.theme === "Glassmorphic" ? 1 : window.borderWidth
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.1) : window.borderColor
            gradient: {
                if (modManager.theme === "Cozy") return sharedCozyDialogGradient
                if (modManager.theme === "Crimson") return sharedCrimsonDialogGradient
                if (modManager.theme === "Wavey") return sharedWaveyDialogGradient
                return null
            }
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, modManager.theme === "Glassmorphic" ? 0.5 : 0.35)
                shadowVerticalOffset: 10
                shadowBlur: modManager.theme === "Glassmorphic" ? 1.2 : 0.9
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, modManager.theme === "Glassmorphic" ? 0.65 : 0.55)
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 18

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: "Describe this Mod"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        color: window.textColor
                    }

                    Text {
                        text: selectedDisplayName
                        font.pixelSize: 13
                        color: window.textSecondaryColor
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
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
                    onClicked: modDescriptionDialog.close()
                }
            }

            Rectangle {
                id: descriptionFieldContainer
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: modManager.theme === "Glassmorphic"
                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.14)
                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.04)
                radius: 12
                border.width: 1
                border.color: window.borderColor

                TextArea {
                    id: descriptionField
                    ContextMenu.menu: null
                    anchors.fill: parent
                    anchors.margins: 8
                    wrapMode: TextArea.Wrap
                    color: window.textColor
                    background: Rectangle { color: "transparent" }
                    selectByMouse: true
                    persistentSelection: true
                }

                Text {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 14
                    text: "The description will appear in the Home page on the Mod preview"
                    visible: descriptionField.text.length === 0
                    color: Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g, window.textSecondaryColor.b, 0.68)
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    z: 1
                }

                MouseArea {
                    z: 1
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    cursorShape: Qt.IBeamCursor
                    onPressed: function(mouse) {
                        descriptionField.forceActiveFocus()
                        descriptionContextMenu.popup()
                        mouse.accepted = true
                    }
                    onPressAndHold: function(mouse) {
                        descriptionField.forceActiveFocus()
                        descriptionContextMenu.popup()
                        mouse.accepted = true
                    }
                }

                Menu {
                    id: descriptionContextMenu
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

                    ContextMenuItem {
                        text: "Undo"
                        iconName: "undo-2.svg"
                        enabled: descriptionField.canUndo
                        onTriggered: descriptionField.undo()
                    }
                    ContextMenuItem {
                        text: "Redo"
                        iconName: "redo-2.svg"
                        enabled: descriptionField.canRedo
                        onTriggered: descriptionField.redo()
                    }
                    MenuSeparator {}
                    ContextMenuItem {
                        text: "Cut"
                        iconName: "scissors.svg"
                        enabled: descriptionField.selectedText.length > 0
                        onTriggered: descriptionField.cut()
                    }
                    ContextMenuItem {
                        text: "Copy"
                        iconName: "copy.svg"
                        enabled: descriptionField.selectedText.length > 0
                        onTriggered: descriptionField.copy()
                    }
                    ContextMenuItem {
                        text: "Paste"
                        iconName: "clipboard.svg"
                        enabled: descriptionField.canPaste
                        onTriggered: descriptionField.paste()
                    }
                    ContextMenuItem {
                        text: "Delete"
                        iconName: "trash.svg"
                        enabled: descriptionField.selectedText.length > 0
                        onTriggered: descriptionField.remove(descriptionField.selectionStart, descriptionField.selectionEnd)
                    }
                    MenuSeparator {}
                    ContextMenuItem {
                        text: "Select All"
                        iconName: "text-select.svg"
                        enabled: descriptionField.text.length > 0
                        onTriggered: descriptionField.selectAll()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    text: descriptionField.text.length + " chars"
                    color: window.textSecondaryColor
                    font.pixelSize: 12
                }

                Item { Layout.fillWidth: true }

                Button {
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 40
                    text: "Cancel"
                    hoverEnabled: true
                    contentItem: Text {
                        text: parent.text
                        color: window.textColor
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered
                               ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08)
                               : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.04)
                        radius: 10
                        border.width: 1
                        border.color: window.borderColor
                    }
                    onClicked: modDescriptionDialog.close()
                }

                Button {
                    Layout.preferredWidth: 140
                    Layout.preferredHeight: 40
                    text: "Save"
                    hoverEnabled: true
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? Qt.lighter(window.primaryColor, 1.1) : window.primaryColor
                        radius: 10
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.2)
                    }
                    onClicked: {
                        modManager.setModDescription(selectedDisplayName, descriptionField.text)
                        selectedModDescription = modManager.getModDescription(selectedDisplayName)
                        toastNotification.show("Description saved", "save.svg")
                        modDescriptionDialog.close()
                    }
                }
            }
        }
    }

    FileDialog {
        id: imageFileDialog
        title: "Select preivew for this mod"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Preview files (*.png *.jpg *.jpeg *.gif *.bmp)", "All files (*.*)"]
        onAccepted: {
            var filePath = selectedFile.toString()
            if (filePath.toLowerCase().endsWith(".gif")) {
                panelModGif.source = selectedFile
                panelModImage.source = ""
            } else {
                panelModImage.source = selectedFile
                panelModGif.source = ""
            }

            modManager.setModImage(selectedModName, filePath)
            toastNotification.show("Preview uploaded successfully", "image.svg")
        }
    }

    Dialog {
        id: deleteConfirmDialog
        property bool isBatchDelete: false
        property string bundleName: ""
        property bool isPermanent: !modManager.deleteToRecycleBin
        property color iconColor: isPermanent ? "#F44336" : window.primaryColor
        property color iconBgColor: modManager.theme === "Glassmorphic"
                                  ? Qt.rgba(iconColor.r, iconColor.g, iconColor.b, 0.2)
                                  : Qt.rgba(iconColor.r, iconColor.g, iconColor.b, 0.1)
        modal: true
        anchors.centerIn: parent
        width: 420
        height: 356
        padding: 0
        closePolicy: Popup.CloseOnEscape

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.12, 0.12, 0.15, 0.98) : window.surfaceColor
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy") return sharedCozyDialogGradient
                if (modManager.theme === "Crimson") return sharedCrimsonDialogGradient
                if (modManager.theme === "Wavey") return sharedWaveyDialogGradient
                return null
            }

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.7)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.85)

            Behavior on opacity {
                NumberAnimation { duration: 250 }
            }
        }

        enter: Transition {
            NumberAnimation {
                property: "scale"
                from: 0.9
                to: 1.0
                duration: 250
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "scale"
                from: 1.0
                to: 0.95
                duration: 200
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }

        contentItem: Item {
            implicitWidth: 420
            implicitHeight: 356

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 32
                spacing: 16

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 80
                    height: 80
                    radius: 40
                    color: "transparent"

                    Image {
                        anchors.centerIn: parent
                        width: 46
                        height: 46
                        source: "qrc:/UniversalModManager/Lucide Icons/trash.svg"
                        fillMode: Image.PreserveAspectFit

                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: deleteConfirmDialog.iconColor
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: deleteConfirmDialog.bundleName !== ""
                              ? "Delete Bundle"
                              : (deleteConfirmDialog.isBatchDelete ? "Delete Mods" : "Delete Mod")
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: {
                            if (deleteConfirmDialog.bundleName !== "") {
                                var count = selectedMods.length
                                return "The " + count + (count === 1 ? " mod" : " mods")
                                    + " in \"" + deleteConfirmDialog.bundleName + "\" and the bundle folder will be "
                                    + (deleteConfirmDialog.isPermanent ? "permanently deleted."
                                                                      : "moved to the Recycle Bin.")
                            }
                            return deleteConfirmDialog.isBatchDelete ?
                                (deleteConfirmDialog.isPermanent ?
                                    (selectedMods.length === 1 ? "Your 1 selected mod will be permanently deleted." : "Your " + selectedMods.length + " selected mods will be permanently deleted.") :
                                    (selectedMods.length === 1 ? "Your 1 selected mod will be moved to the Recycle Bin." : "Your " + selectedMods.length + " selected mods will be moved to the Recycle Bin.")) :
                                (deleteConfirmDialog.isPermanent ? "Your mod will be permanently deleted." : "Your mod will be moved to the Recycle Bin.")
                        }
                        font.pixelSize: 15
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        lineHeight: 1.4
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    spacing: 12

                    Rectangle {
                        id: cancelDeleteButton
                        Layout.fillWidth: true
                        Layout.preferredWidth: 160
                        Layout.preferredHeight: 48
                        radius: 24
                        color: cancelDeleteMouse.containsMouse ? window.secondaryButtonHoverColor : window.secondaryButtonColor
                        border.width: 1
                        border.color: window.secondaryButtonBorderColor

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        Text {
                            anchors.centerIn: parent
                            text: "Cancel"
                            font.pixelSize: 15
                            font.weight: Font.Medium
                            color: cancelDeleteMouse.containsMouse ? window.secondaryButtonTextHoverColor : window.secondaryButtonTextColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 150 } }
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
                        id: confirmDeleteButton
                        Layout.fillWidth: true
                        Layout.preferredWidth: 160
                        Layout.preferredHeight: 48
                        radius: 24
                        color: confirmDeleteMouse.containsMouse ? deleteConfirmDialog.iconColor : Qt.rgba(deleteConfirmDialog.iconColor.r, deleteConfirmDialog.iconColor.g, deleteConfirmDialog.iconColor.b, 0.15)
                        border.width: 1.5
                        border.color: confirmDeleteMouse.containsMouse ? deleteConfirmDialog.iconColor : Qt.rgba(deleteConfirmDialog.iconColor.r, deleteConfirmDialog.iconColor.g, deleteConfirmDialog.iconColor.b, 0.4)

                        Behavior on color { ColorAnimation { duration: 200 } }
                        Behavior on border.color { ColorAnimation { duration: 200 } }

                        Text {
                            anchors.centerIn: parent
                            text: "Confirm"
                            font.pixelSize: 15
                            font.weight: Font.Bold
                            color: confirmDeleteMouse.containsMouse ? "#FFFFFF" : window.textColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        MouseArea {
                            id: confirmDeleteMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (deleteProgressDialog.visible) {
                                    deleteConfirmDialog.close()
                                    return
                                }

                                var modsToDelete = deleteConfirmDialog.isBatchDelete
                                    ? selectedMods.slice() : []

                                deleteConfirmDialog.close()

                                panelModImage.source = ""
                                panelModGif.source = ""

                                if (deleteConfirmDialog.isBatchDelete) {

                                    var count = modsToDelete.length
                                    if (count === 0)
                                        return

                                    selectedMods = []
                                    deleteProgressDialog.open()

                                    deleteProgressDialog.totalCount = count
                                    deleteProgressDialog.currentCount = 0
                                    deleteProgressDialog.currentModName = ""
                                    modManager.deleteModsAsync(modsToDelete)
                                } else {

                                    var modName = selectedModName
                                    modDetailsPanel.close()
                                    modManager.deleteMod(modName)
                                    toastNotification.show(deleteConfirmDialog.isPermanent ? "Mod deleted permanently" : "Mod moved to Recycle Bin", "trash.svg")
                                }
                            }
                        }
                    }
                }
            }
        }

        onClosed: {
            if (bundleName !== "")
                selectedMods = []
            bundleName = ""
        }
    }

    Dialog {
        id: addTagDialog
        property string bundleTargetId: ""
        modal: true
        anchors.centerIn: parent
        width: 420
        height: 280
        padding: 0
        closePolicy: Popup.CloseOnEscape

        function commitTag() {
            var tag = tagInput.text.trim()
            if (tag === "")
                return
            if (bundleTargetId !== "")
                modManager.addModBundleTag(bundleTargetId, tag)
            else
                modManager.addModTag(selectedModName, tag)
            tagInput.text = ""
            close()
        }

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.12, 0.12, 0.15, 0.98) : window.surfaceColor
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy") return sharedCozyDialogGradient
                if (modManager.theme === "Crimson") return sharedCrimsonDialogGradient
                if (modManager.theme === "Wavey") return sharedWaveyDialogGradient
                return null
            }

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.6)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.75)

            Behavior on opacity {
                NumberAnimation { duration: 250 }
            }
        }

        enter: Transition {
            NumberAnimation {
                property: "scale"
                from: 0.9
                to: 1.0
                duration: 250
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "scale"
                from: 1.0
                to: 0.95
                duration: 200
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 32
            spacing: 20

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 24
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)

                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/tags.svg"
                        sourceSize.width: 24
                        sourceSize.height: 24
                        fillMode: Image.PreserveAspectFit
                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: window.primaryColor
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: "Add Tag"
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    }

                    Text {
                        text: addTagDialog.bundleTargetId !== ""
                              ? "Enter a tag name for this bundle"
                              : "Enter a tag name for this mod"
                        font.pixelSize: 12
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    }
                }
            }

            Rectangle {
                id: tagInputContainer
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08) : window.cardColor1
                radius: 12
                border.width: tagInput.activeFocus ? 2 : 1
                border.color: tagInput.activeFocus ? window.primaryColor : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.2) : window.borderColor)

                Behavior on border.color { ColorAnimation { duration: 150 } }
                Behavior on border.width { NumberAnimation { duration: 150 } }

                TextField {
                    id: tagInput
                    ContextMenu.menu: null
                    anchors.fill: parent
                    anchors.margins: 12
                    placeholderTextColor: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textSecondaryColor
                    color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    font.pixelSize: 13
                    background: Rectangle { color: "transparent" }
                    selectByMouse: true
                    persistentSelection: true

                    Keys.onReturnPressed: addTagDialog.commitTag()
                }

                MouseArea {
                    z: 1
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    cursorShape: Qt.IBeamCursor
                    onPressed: function(mouse) {
                        tagInput.forceActiveFocus()
                        tagInputContextMenu.popup()
                        mouse.accepted = true
                    }
                    onPressAndHold: function(mouse) {
                        tagInput.forceActiveFocus()
                        tagInputContextMenu.popup()
                        mouse.accepted = true
                    }
                }

                Menu {
                    id: tagInputContextMenu
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

                    ContextMenuItem {
                        text: "Undo"
                        iconName: "undo-2.svg"
                        enabled: tagInput.canUndo
                        onTriggered: tagInput.undo()
                    }
                    ContextMenuItem {
                        text: "Redo"
                        iconName: "redo-2.svg"
                        enabled: tagInput.canRedo
                        onTriggered: tagInput.redo()
                    }
                    MenuSeparator {}
                    ContextMenuItem {
                        text: "Cut"
                        iconName: "scissors.svg"
                        enabled: tagInput.selectedText.length > 0
                        onTriggered: tagInput.cut()
                    }
                    ContextMenuItem {
                        text: "Copy"
                        iconName: "copy.svg"
                        enabled: tagInput.selectedText.length > 0
                        onTriggered: tagInput.copy()
                    }
                    ContextMenuItem {
                        text: "Paste"
                        iconName: "clipboard.svg"
                        enabled: tagInput.canPaste
                        onTriggered: tagInput.paste()
                    }
                    ContextMenuItem {
                        text: "Delete"
                        iconName: "trash.svg"
                        enabled: tagInput.selectedText.length > 0
                        onTriggered: tagInput.remove(tagInput.selectionStart, tagInput.selectionEnd)
                    }
                    MenuSeparator {}
                    ContextMenuItem {
                        text: "Select All"
                        iconName: "text-select.svg"
                        enabled: tagInput.text.length > 0
                        onTriggered: tagInput.selectAll()
                    }
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    id: cancelTagButton
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: "Cancel"
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: modManager.theme === "Glassmorphic" ?
                            (cancelTagButton.hovered ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.7)) :
                            (cancelTagButton.hovered ? window.textColor : window.textSecondaryColor)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter

                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    background: Rectangle {
                        color: "transparent"
                        radius: 12
                    }

                    onClicked: {
                        tagInput.text = ""
                        addTagDialog.close()
                    }
                }

                Button {
                    id: addTagConfirmButton
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    enabled: tagInput.text.trim() !== ""
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: "Add Tag"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: addTagConfirmButton.enabled ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.4)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: {
                            if (!addTagConfirmButton.enabled) {
                                return Qt.rgba(0.3, 0.3, 0.3, 0.3)
                            }
                            if (addTagConfirmButton.hovered) {
                                return Qt.lighter(window.primaryColor, 1.1)
                            }
                            return window.primaryColor
                        }
                        radius: 12

                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    scale: addTagConfirmButton.enabled && addTagConfirmButton.hovered ? 1.05 : 1.0
                    Behavior on scale {
                        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                    }

                    onClicked: addTagDialog.commitTag()
                }
            }
        }

        onOpened: {
            tagInput.text = ""
            tagInput.forceActiveFocus()
        }

        onClosed: bundleTargetId = ""
    }

    Dialog {
        id: renameModDialog
        modal: true
        anchors.centerIn: parent
        width: 420
        height: 280
        padding: 0
        closePolicy: Popup.CloseOnEscape

        property string targetModName: ""

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.12, 0.12, 0.15, 0.98) : window.surfaceColor
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy") return sharedCozyDialogGradient
                if (modManager.theme === "Crimson") return sharedCrimsonDialogGradient
                if (modManager.theme === "Wavey") return sharedWaveyDialogGradient
                return null
            }

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.6)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.75)

            Behavior on opacity {
                NumberAnimation { duration: 250 }
            }
        }

        enter: Transition {
            NumberAnimation {
                property: "scale"
                from: 0.9
                to: 1.0
                duration: 250
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "scale"
                from: 1.0
                to: 0.95
                duration: 200
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 32
            spacing: 20

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 24
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)

                    Image {
                        anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/pencil.svg"
                        sourceSize.width: 24
                        sourceSize.height: 24
                        fillMode: Image.PreserveAspectFit
                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: window.primaryColor
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: "Rename Mod"
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    }

                    Text {
                        text: "Enter a new name for this mod"
                        font.pixelSize: 12
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    }
                }
            }

            Rectangle {
                id: renameModContainer
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08) : window.cardColor1
                radius: 12
                border.width: renameModInput.activeFocus ? 2 : 1
                border.color: renameModInput.activeFocus ? window.primaryColor : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.2) : window.borderColor)

                Behavior on border.color { ColorAnimation { duration: 150 } }
                Behavior on border.width { NumberAnimation { duration: 150 } }

                TextField {
                    id: renameModInput
                    ContextMenu.menu: null
                    anchors.fill: parent
                    anchors.margins: 12
                    placeholderText: "New mod name..."
                    placeholderTextColor: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textSecondaryColor
                    color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    font.pixelSize: 13
                    background: Rectangle { color: "transparent" }
                    selectByMouse: true
                    persistentSelection: true
                    text: renameModDialog.targetModName
                    validator: RegularExpressionValidator { regularExpression: /^[^\\/<>|*?:"]+$/ }

                    Keys.onReturnPressed: {
                        var trimmed = renameModInput.text.trim()
                        if (trimmed !== "" && trimmed !== renameModDialog.targetModName) {
                            modManager.renameMod(renameModDialog.targetModName, trimmed)
                            renameModDialog.close()
                            toastNotification.show("Mod renamed", "check.svg")
                        }
                    }
                }

                MouseArea {
                    z: 1
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    cursorShape: Qt.IBeamCursor
                    onPressed: function(mouse) {
                        renameModInput.forceActiveFocus()
                        renameModContextMenu.popup()
                        mouse.accepted = true
                    }
                    onPressAndHold: function(mouse) {
                        renameModInput.forceActiveFocus()
                        renameModContextMenu.popup()
                        mouse.accepted = true
                    }
                }

                Menu {
                    id: renameModContextMenu
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

                    ContextMenuItem {
                        text: "Undo"
                        iconName: "undo-2.svg"
                        enabled: renameModInput.canUndo
                        onTriggered: renameModInput.undo()
                    }
                    ContextMenuItem {
                        text: "Redo"
                        iconName: "redo-2.svg"
                        enabled: renameModInput.canRedo
                        onTriggered: renameModInput.redo()
                    }
                    MenuSeparator {}
                    ContextMenuItem {
                        text: "Cut"
                        iconName: "scissors.svg"
                        enabled: renameModInput.selectedText.length > 0
                        onTriggered: renameModInput.cut()
                    }
                    ContextMenuItem {
                        text: "Copy"
                        iconName: "copy.svg"
                        enabled: renameModInput.selectedText.length > 0
                        onTriggered: renameModInput.copy()
                    }
                    ContextMenuItem {
                        text: "Paste"
                        iconName: "clipboard.svg"
                        enabled: renameModInput.canPaste
                        onTriggered: renameModInput.paste()
                    }
                    ContextMenuItem {
                        text: "Delete"
                        iconName: "trash.svg"
                        enabled: renameModInput.selectedText.length > 0
                        onTriggered: renameModInput.remove(renameModInput.selectionStart, renameModInput.selectionEnd)
                    }
                    MenuSeparator {}
                    ContextMenuItem {
                        text: "Select All"
                        iconName: "text-select.svg"
                        enabled: renameModInput.text.length > 0
                        onTriggered: renameModInput.selectAll()
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.preferredHeight: 14
                visible: renameModInput.text.length > 0 && (renameModInput.text.trim() === "" || renameModInput.text.trim() === renameModDialog.targetModName)
                text: renameModInput.text.trim() === ""
                    ? "Name cannot be empty."
                    : "New name must differ from the current name."
                color: "#E57373"
                font.pixelSize: 11
                font.weight: Font.Medium
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    id: cancelRenameModButton
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: "Cancel"
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: modManager.theme === "Glassmorphic" ?
                            (cancelRenameModButton.hovered ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.7)) :
                            (cancelRenameModButton.hovered ? window.textColor : window.textSecondaryColor)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter

                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    background: Rectangle {
                        color: "transparent"
                        radius: 12
                    }

                    onClicked: renameModDialog.close()
                }

                Button {
                    id: confirmRenameModButton
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    text: "Rename"
                    enabled: renameModInput.text.trim() !== "" && renameModInput.text.trim() !== renameModDialog.targetModName
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: confirmRenameModButton.enabled
                            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, confirmRenameModButton.hovered ? 0.9 : 1.0)
                            : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.3)
                        radius: 12

                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    onClicked: {
                        var trimmed = renameModInput.text.trim()
                        if (trimmed !== "" && trimmed !== renameModDialog.targetModName) {
                            modManager.renameMod(renameModDialog.targetModName, trimmed)
                            renameModDialog.close()
                            toastNotification.show("Mod renamed", "check.svg")
                        }
                    }
                }
            }
        }

        onOpened: {
            renameModInput.text = renameModDialog.targetModName
            renameModInput.forceActiveFocus()
            renameModInput.selectAll()
        }
    }

}
