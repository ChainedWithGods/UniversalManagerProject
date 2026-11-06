import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Item {
    id: root

    property alias backupTypeDialog: typeDialog
    property alias selectionDialog: selectionDialog

    signal backupRequested(var names)

    readonly property bool glass: modManager.theme === "Glassmorphic"
    readonly property bool cozy: modManager.theme === "Cozy"
    readonly property color headingColor: glass ? "#FFFFFF" : window.textColor
    readonly property color bodyColor: glass
        ? Qt.rgba(1, 1, 1, 0.78)
        : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.64)
    readonly property color faintColor: glass
        ? Qt.rgba(1, 1, 1, 0.58)
        : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.46)
    readonly property color panelColor: glass
        ? Qt.rgba(0, 0, 0, 0.18)
        : (cozy ? Qt.rgba(164 / 255, 122 / 255, 81 / 255, 0.05) : Qt.rgba(0, 0, 0, 0.16))
    readonly property color fieldColor: glass
        ? Qt.rgba(0, 0, 0, 0.18)
        : (cozy ? Qt.rgba(0, 0, 0, 0.04) : Qt.rgba(0, 0, 0, 0.16))
    readonly property color panelBorderColor: glass
        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25)
        : window.borderColor
    readonly property color rowHoverColor: glass
        ? Qt.rgba(1, 1, 1, 0.05)
        : (cozy ? Qt.rgba(0, 0, 0, 0.035) : Qt.rgba(1, 1, 1, 0.03))
    readonly property color rowSelectedColor: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, glass ? 0.14 : 0.16)
    readonly property color dangerColor: cozy ? "#B4463D" : "#E05A5A"
    readonly property color dangerStrongColor: cozy ? "#8E2F28" : "#FFD7D7"
    readonly property color dangerMutedColor: cozy ? "#7C4A45" : "#C99A9C"
    readonly property color successColor: window.themePalette.enableButtonColor !== undefined
        ? window.themePalette.enableButtonColor : "#7FD1A8"

    property int folderCount: 0
    property int looseCount: 0
    property int visibleItemCount: 0
    property int selectedCount: 0
    property bool allVisibleSelected: false
    property bool partialVisibleSelection: false

    function refreshTypeCounts() {
        var folder = 0
        var loose = 0
        for (var i = 0; i < modManager.modList.length; i++) {
            var mod = modManager.modList[i]
            if (!mod)
                continue
            if (mod.type === "folder")
                folder++
            else if (mod.type === "loose")
                loose++
        }
        folderCount = folder
        looseCount = loose
    }

    function populateSelection() {
        backupListModel.clear()
        for (var i = 0; i < modManager.modList.length; i++) {
            var mod = modManager.modList[i]
            if (!mod || mod.type !== selectionDialog.modTypeToShow)
                continue
            var fileExt = ""
            if (selectionDialog.modTypeToShow === "loose" && mod.extensions)
                fileExt = mod.extensions
            backupListModel.append({
                name: mod.name,
                status: mod.status,
                fileCount: mod.fileCount,
                fileExtension: fileExt,
                selected: false,
                visible: true
            })
        }
        updateFilter()
    }

    function updateFilter() {
        var query = backupSearchField.text.toLowerCase()
        var visible = 0
        for (var i = 0; i < backupListModel.count; i++) {
            var mod = backupListModel.get(i)
            var show = mod.name.toLowerCase().includes(query)
            backupListModel.setProperty(i, "visible", show)
            if (show)
                visible++
        }
        visibleItemCount = visible
        updateSelectionState()
    }

    function updateSelectionState() {
        var count = 0
        var visible = 0
        var visibleSelected = 0
        for (var i = 0; i < backupListModel.count; i++) {
            var mod = backupListModel.get(i)
            if (mod.selected)
                count++
            if (mod.visible) {
                visible++
                if (mod.selected)
                    visibleSelected++
            }
        }
        selectedCount = count
        visibleItemCount = visible
        allVisibleSelected = visible > 0 && visibleSelected === visible
        partialVisibleSelection = visibleSelected > 0 && visibleSelected < visible
    }

    function toggleRow(index) {
        var mod = backupListModel.get(index)
        if (!mod)
            return
        backupListModel.setProperty(index, "selected", !mod.selected)
        updateSelectionState()
    }

    function toggleSelectAll() {
        if (visibleItemCount === 0)
            return
        var select = !allVisibleSelected
        for (var i = 0; i < backupListModel.count; i++) {
            if (backupListModel.get(i).visible)
                backupListModel.setProperty(i, "selected", select)
        }
        updateSelectionState()
    }

    function clearSelection() {
        for (var i = 0; i < backupListModel.count; i++)
            backupListModel.setProperty(i, "selected", false)
        updateSelectionState()
    }

    function collectSelectedNames() {
        var names = []
        for (var i = 0; i < backupListModel.count; i++) {
            var mod = backupListModel.get(i)
            if (mod.selected)
                names.push(mod.name)
        }
        return names
    }

    function backupButtonText() {
        var n = selectedCount
        if (n === 0)
            return "Backup selected"
        if (selectionDialog.modTypeToShow === "folder")
            return "Backup " + n + (n === 1 ? " mod" : " mods")
        return "Backup " + n + (n === 1 ? " file" : " files")
    }

    function typeCountText(count, singular, plural) {
        return count + (count === 1 ? " " + singular : " " + plural)
    }

    ListModel {
        id: backupListModel
    }

    Gradient {
        id: cozyDialogGradient
        GradientStop { position: 0.0; color: window.cozyDialogGradientTop }
        GradientStop { position: 1.0; color: window.cozyDialogGradientBottom }
    }

    Gradient {
        id: crimsonDialogGradient
        GradientStop { position: 0.0; color: window.crimsonDialogGradientTop }
        GradientStop { position: 1.0; color: window.crimsonDialogGradientBottom }
    }

    Gradient {
        id: waveyDialogGradient
        GradientStop { position: 0.0; color: window.waveyDialogGradientTop }
        GradientStop { position: 1.0; color: window.waveyDialogGradientBottom }
    }

    FolderDialog {
        id: backupPathPickerDialog
        title: "Select Backup Folder"
        onAccepted: modManager.setBackupPath(selectedFolder.toString().replace("file:///", ""))
    }

    Dialog {
        id: typeDialog
        width: 720
        height: implicitHeight
        modal: true
        anchors.centerIn: Overlay.overlay
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        palette {
            windowText: root.glass ? modManager.accentColor : window.textColor
            text: root.glass ? modManager.accentColor : window.textColor
        }

        background: Rectangle {
            color: root.glass ? Qt.rgba(0.15, 0.15, 0.15, 0.98) : window.surfaceColor
            radius: 22
            border.width: root.glass ? 1.5 : 1
            border.color: root.glass
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25)
                : window.borderColor

            gradient: root.cozy ? cozyDialogGradient
                : modManager.theme === "Crimson" ? crimsonDialogGradient
                : modManager.theme === "Wavey" ? waveyDialogGradient
                : null

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.6)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 16
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
                from: 0.92
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
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 90

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    anchors.topMargin: 22
                    anchors.bottomMargin: 12
                    spacing: 14

                    Image {
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        Layout.alignment: Qt.AlignTop
                        source: "qrc:/UniversalModManager/Lucide Icons/save.svg"
                        sourceSize.width: 26
                        sourceSize.height: 26
                        fillMode: Image.PreserveAspectFit
                        layer.enabled: true
                        layer.smooth: true
                        layer.textureSize: Qt.size(width * 2, height * 2)
                        layer.effect: ColorOverlay {
                            color: window.primaryColor
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        Text {
                            text: "Backup Mods"
                            font.pixelSize: 22
                            font.weight: Font.DemiBold
                            font.family: "Segoe UI Variable Display"
                            color: root.headingColor
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Choose what you want to back up. You'll pick individual items on the next step."
                            font.pixelSize: 13
                            color: root.bodyColor
                            wrapMode: Text.WordWrap
                        }
                    }

                    Button {
                        id: chooserCloseButton
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignTop
                        flat: true
                        hoverEnabled: true

                        contentItem: Text {
                            text: "✕"
                            font.pixelSize: 15
                            color: root.headingColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 9
                            color: chooserCloseButton.hovered
                                ? (root.glass ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06))
                                : "transparent"
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: typeDialog.close()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 4
                spacing: 14

                BackupTypeCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 190
                    title: "Folder Mods"
                    description: "Mods installed in their own subfolders."
                    iconName: "package.svg"
                    iconColor: window.primaryColor
                    countLabel: root.typeCountText(root.folderCount, "mod available", "mods available")
                    onClicked: {
                        typeDialog.close()
                        selectionDialog.modTypeToShow = "folder"
                        selectionDialog.open()
                    }
                }

                BackupTypeCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 190
                    title: "Loose Files"
                    description: "Single .pak, .ucas, or .utoc files in the mods root."
                    iconName: "file.svg"
                    iconColor: window.accentColor
                    countLabel: root.typeCountText(root.looseCount, "file available", "files available")
                    onClicked: {
                        typeDialog.close()
                        selectionDialog.modTypeToShow = "loose"
                        selectionDialog.open()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 26
                Layout.rightMargin: 26
                Layout.topMargin: 14
                spacing: 8

                Image {
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    source: "qrc:/UniversalModManager/Lucide Icons/folder.svg"
                    sourceSize.width: 14
                    sourceSize.height: 14
                    fillMode: Image.PreserveAspectFit
                    layer.enabled: true
                    layer.smooth: true
                    layer.textureSize: Qt.size(width * 2, height * 2)
                    layer.effect: ColorOverlay {
                        color: root.faintColor
                    }
                }

                Text {
                    text: "Backups are written to your configured backup folder."
                    font.pixelSize: 11
                    color: root.faintColor
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: window.separatorColor
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 78

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    anchors.topMargin: 18
                    anchors.bottomMargin: 18
                    spacing: 16

                    Text {
                        Layout.fillWidth: true
                        text: "You can change the destination before starting."
                        font.pixelSize: 11
                        color: root.faintColor
                    }

                    Button {
                        id: chooserCancelButton
                        Layout.preferredWidth: 96
                        Layout.preferredHeight: 40
                        hoverEnabled: true

                        contentItem: Text {
                            text: "Cancel"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: chooserCancelButton.hovered ? window.secondaryButtonTextHoverColor : window.secondaryButtonTextColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 10
                            color: chooserCancelButton.hovered ? window.secondaryButtonHoverColor : window.secondaryButtonColor
                            border.width: 1
                            border.color: window.secondaryButtonBorderColor
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: typeDialog.close()
                    }
                }
            }
        }

        onOpened: root.refreshTypeCounts()
    }

    Dialog {
        id: selectionDialog
        property string modTypeToShow: ""

        width: 720
        height: 640
        modal: true
        anchors.centerIn: Overlay.overlay
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        palette {
            windowText: root.glass ? modManager.accentColor : window.textColor
            text: root.glass ? modManager.accentColor : window.textColor
        }

        background: Rectangle {
            color: root.glass ? Qt.rgba(0.15, 0.15, 0.15, 0.98) : window.surfaceColor
            radius: 22
            border.width: root.glass ? 1.5 : 1
            border.color: root.glass
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25)
                : window.borderColor

            gradient: root.cozy ? cozyDialogGradient
                : modManager.theme === "Crimson" ? crimsonDialogGradient
                : modManager.theme === "Wavey" ? waveyDialogGradient
                : null

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.6)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 16
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
                from: 0.92
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
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 96

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 22
                    anchors.rightMargin: 24
                    anchors.topMargin: 22
                    anchors.bottomMargin: 12
                    spacing: 12

                    Button {
                        id: selectionBackButton
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        Layout.alignment: Qt.AlignTop
                        flat: true
                        hoverEnabled: true

                        contentItem: Image {
                            source: "qrc:/UniversalModManager/Lucide Icons/chevron-left.svg"
                            sourceSize.width: 18
                            sourceSize.height: 18
                            fillMode: Image.PreserveAspectFit
                            layer.enabled: true
                            layer.smooth: true
                            layer.textureSize: Qt.size(width * 2, height * 2)
                            layer.effect: ColorOverlay {
                                color: root.headingColor
                            }
                        }

                        background: Rectangle {
                            radius: 9
                            color: selectionBackButton.hovered
                                ? (root.glass ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06))
                                : "transparent"
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: {
                            selectionDialog.close()
                            typeDialog.open()
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        Text {
                            text: selectionDialog.modTypeToShow === "folder" ? "Backup Folder Mods" : "Backup Loose Files"
                            font.pixelSize: 21
                            font.weight: Font.DemiBold
                            font.family: "Segoe UI Variable Display"
                            color: root.headingColor
                        }

                        Text {
                            Layout.fillWidth: true
                            text: selectionDialog.modTypeToShow === "folder"
                                ? backupListModel.count + (backupListModel.count === 1 ? " mod" : " mods") + " in your library. Select the ones you want to include."
                                : backupListModel.count + (backupListModel.count === 1 ? " loose file" : " loose files") + " in the mods root. Select the ones you want to include."
                            font.pixelSize: 13
                            color: root.bodyColor
                            wrapMode: Text.WordWrap
                        }
                    }

                    Button {
                        id: selectionCloseButton
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignTop
                        flat: true
                        hoverEnabled: true

                        contentItem: Text {
                            text: "✕"
                            font.pixelSize: 15
                            color: root.headingColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 9
                            color: selectionCloseButton.hovered
                                ? (root.glass ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06))
                                : "transparent"
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: selectionDialog.close()
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    anchors.bottomMargin: 0
                    anchors.topMargin: 2
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        visible: !modManager.backupPath
                        radius: 12
                        color: Qt.rgba(root.dangerColor.r, root.dangerColor.g, root.dangerColor.b, 0.10)
                        border.width: 1
                        border.color: Qt.rgba(root.dangerColor.r, root.dangerColor.g, root.dangerColor.b, 0.30)

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 102
                            spacing: 12

                            Image {
                                Layout.preferredWidth: 17
                                Layout.preferredHeight: 17
                                source: "qrc:/UniversalModManager/Lucide Icons/triangle-alert.svg"
                                sourceSize.width: 17
                                sourceSize.height: 17
                                fillMode: Image.PreserveAspectFit
                                layer.enabled: true
                                layer.smooth: true
                                layer.textureSize: Qt.size(width * 2, height * 2)
                                layer.effect: ColorOverlay {
                                    color: root.dangerColor
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: "Backup path is not configured"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    color: root.dangerStrongColor
                                }

                                Text {
                                    text: "Choose a folder before starting the backup."
                                    font.pixelSize: 11
                                    color: root.dangerMutedColor
                                }
                            }

                        }

                        Button {
                            id: setPathButton
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            width: 78
                            height: 32
                            hoverEnabled: true

                            contentItem: Text {
                                text: "Set path"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: setPathButton.hovered ? window.secondaryButtonTextHoverColor : window.secondaryButtonTextColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                radius: 9
                                color: setPathButton.hovered ? window.secondaryButtonHoverColor : window.secondaryButtonColor
                                border.width: 1
                                border.color: window.secondaryButtonBorderColor
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }

                            onClicked: backupPathPickerDialog.open()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        spacing: 14

                        Item {
                            Layout.preferredWidth: selectAllRow.implicitWidth + 8
                            Layout.preferredHeight: 30
                            opacity: root.visibleItemCount === 0 ? 0.5 : 1.0

                            Row {
                                id: selectAllRow
                                anchors.left: parent.left
                                anchors.leftMargin: 4
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 9

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 18
                                    height: 18
                                    radius: 5
                                    color: root.allVisibleSelected
                                        ? window.primaryColor
                                        : (root.partialVisibleSelection
                                            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.2)
                                            : "transparent")
                                    border.width: 1.5
                                    border.color: root.allVisibleSelected || root.partialVisibleSelection
                                        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.7)
                                        : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.35)

                                    Image {
                                        anchors.centerIn: parent
                                        width: 11
                                        height: 11
                                        source: "qrc:/UniversalModManager/Lucide Icons/check.svg"
                                        sourceSize.width: 11
                                        sourceSize.height: 11
                                        fillMode: Image.PreserveAspectFit
                                        visible: root.allVisibleSelected
                                        layer.enabled: true
                                        layer.smooth: true
                                        layer.textureSize: Qt.size(width * 2, height * 2)
                                        layer.effect: ColorOverlay {
                                            color: "#FFFFFF"
                                        }
                                    }

                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 8
                                        height: 2
                                        radius: 1
                                        color: window.accentColor
                                        visible: root.partialVisibleSelection
                                    }
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "Select all"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: root.headingColor
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: root.visibleItemCount
                                    font.pixelSize: 11
                                    color: root.faintColor
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                enabled: root.visibleItemCount > 0
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.toggleSelectAll()
                            }
                        }

                        Text {
                            text: "Clear"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            color: window.accentColor
                            opacity: root.selectedCount > 0 ? 1.0 : 0.45

                            MouseArea {
                                anchors.fill: parent
                                anchors.margins: -4
                                enabled: root.selectedCount > 0
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.clearSelection()
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            Layout.preferredWidth: 250
                            Layout.preferredHeight: 36
                            radius: 10
                            color: root.fieldColor
                            border.width: 1
                            border.color: backupSearchField.activeFocus ? window.primaryColor : root.panelBorderColor
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            Image {
                                id: searchIcon
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                width: 15
                                height: 15
                                source: "qrc:/UniversalModManager/Lucide Icons/search.svg"
                                sourceSize.width: 15
                                sourceSize.height: 15
                                fillMode: Image.PreserveAspectFit
                                layer.enabled: true
                                layer.smooth: true
                                layer.textureSize: Qt.size(width * 2, height * 2)
                                layer.effect: ColorOverlay {
                                    color: root.faintColor
                                }
                            }

                            TextField {
                                id: backupSearchField
                                anchors.left: searchIcon.right
                                anchors.leftMargin: 8
                                anchors.right: searchClearButton.left
                                anchors.rightMargin: 4
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                placeholderText: "Search..."
                                placeholderTextColor: Qt.rgba(root.bodyColor.r, root.bodyColor.g, root.bodyColor.b, 0.55)
                                color: root.glass ? "#FFFFFF" : window.textColor
                                font.pixelSize: 12
                                verticalAlignment: TextInput.AlignVCenter
                                background: null
                                selectByMouse: true
                                persistentSelection: true
                                onTextChanged: root.updateFilter()
                                ContextMenu.menu: AppTextContextMenu {
                                    editor: backupSearchField
                                }
                            }

                            Button {
                                id: searchClearButton
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                width: 24
                                height: 24
                                visible: backupSearchField.text.length > 0
                                flat: true
                                hoverEnabled: true

                                contentItem: Text {
                                    text: "✕"
                                    font.pixelSize: 11
                                    color: root.bodyColor
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                background: Rectangle {
                                    radius: 7
                                    color: searchClearButton.hovered
                                        ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08)
                                        : "transparent"
                                }

                                onClicked: {
                                    backupSearchField.text = ""
                                    backupSearchField.forceActiveFocus()
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 8
                        color: root.panelColor
                        border.width: 1
                        border.color: root.panelBorderColor
                        clip: true

                        ListView {
                            id: selectionList
                            anchors.fill: parent
                            anchors.margins: 1
                            clip: true
                            model: backupListModel
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                            }

                            delegate: Item {
                                id: rowItem
                                width: selectionList.width
                                height: model.visible ? 56 : 0
                                visible: model.visible

                                Rectangle {
                                    anchors.fill: parent
                                    color: root.rowSelectedColor
                                    visible: model.selected
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    color: root.rowHoverColor
                                    visible: rowMouse.containsMouse && !model.selected
                                }

                                Rectangle {
                                    width: 2
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    color: window.accentColor
                                    visible: model.selected
                                }

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 1
                                    color: window.separatorColor
                                    visible: index < selectionList.count - 1
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 16
                                    spacing: 12

                                    Rectangle {
                                        Layout.preferredWidth: 18
                                        Layout.preferredHeight: 18
                                        radius: 5
                                        color: model.selected ? window.primaryColor : "transparent"
                                        border.width: 1.5
                                        border.color: model.selected
                                            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.9)
                                            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.35)

                                        Image {
                                            anchors.centerIn: parent
                                            width: 11
                                            height: 11
                                            source: "qrc:/UniversalModManager/Lucide Icons/check.svg"
                                            sourceSize.width: 11
                                            sourceSize.height: 11
                                            fillMode: Image.PreserveAspectFit
                                            visible: model.selected
                                            layer.enabled: true
                                            layer.smooth: true
                                            layer.textureSize: Qt.size(width * 2, height * 2)
                                            layer.effect: ColorOverlay {
                                                color: "#FFFFFF"
                                            }
                                        }
                                    }

                                    Image {
                                        Layout.preferredWidth: 20
                                        Layout.preferredHeight: 20
                                        source: "qrc:/UniversalModManager/Lucide Icons/"
                                            + (selectionDialog.modTypeToShow === "loose" ? "file.svg" : "package.svg")
                                        sourceSize.width: 20
                                        sourceSize.height: 20
                                        fillMode: Image.PreserveAspectFit
                                        opacity: 0.95
                                        layer.enabled: true
                                        layer.smooth: true
                                        layer.textureSize: Qt.size(width * 2, height * 2)
                                        layer.effect: ColorOverlay {
                                            color: selectionDialog.modTypeToShow === "loose" ? window.accentColor : window.primaryColor
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 3

                                        Text {
                                            Layout.fillWidth: true
                                            text: model.name
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                            color: root.headingColor
                                            elide: Text.ElideRight
                                        }

                                        RowLayout {
                                            spacing: 7

                                            Text {
                                                text: selectionDialog.modTypeToShow === "loose"
                                                    ? "Loose file"
                                                    : model.fileCount + (model.fileCount === 1 ? " file" : " files")
                                                font.pixelSize: 11
                                                color: root.bodyColor
                                            }

                                            Rectangle {
                                                Layout.preferredHeight: 16
                                                Layout.preferredWidth: extensionLabel.implicitWidth + 12
                                                radius: 5
                                                color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.16)
                                                visible: selectionDialog.modTypeToShow === "loose" && model.fileExtension !== ""

                                                Text {
                                                    id: extensionLabel
                                                    anchors.centerIn: parent
                                                    text: model.fileExtension ? model.fileExtension.toUpperCase() : ""
                                                    font.pixelSize: 9
                                                    font.weight: Font.DemiBold
                                                    font.letterSpacing: 0.5
                                                    color: window.primaryColor
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        text: model.status
                                        font.pixelSize: 11
                                        color: model.status === "Active" ? root.successColor : root.faintColor
                                    }
                                }

                                MouseArea {
                                    id: rowMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.toggleRow(index)
                                }
                            }
                        }

                        Column {
                            anchors.centerIn: parent
                            spacing: 6
                            visible: root.visibleItemCount === 0

                            Image {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 22
                                height: 22
                                source: "qrc:/UniversalModManager/Lucide Icons/search.svg"
                                sourceSize.width: 22
                                sourceSize.height: 22
                                fillMode: Image.PreserveAspectFit
                                opacity: 0.8
                                layer.enabled: true
                                layer.smooth: true
                                layer.textureSize: Qt.size(width * 2, height * 2)
                                layer.effect: ColorOverlay {
                                    color: window.accentColor
                                }
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "No matches"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: root.headingColor
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Try a different name, or clear the search field."
                                font.pixelSize: 11
                                color: root.faintColor
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: window.separatorColor
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 86

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    anchors.topMargin: 18
                    anchors.bottomMargin: 20
                    spacing: 14

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 5

                        RowLayout {
                            spacing: 4

                            Text {
                                text: root.selectedCount
                                font.pixelSize: 13
                                font.weight: Font.Bold
                                color: root.headingColor
                            }

                            Text {
                                text: "of"
                                font.pixelSize: 12
                                color: root.faintColor
                            }

                            Text {
                                text: backupListModel.count
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: root.headingColor
                            }

                            Text {
                                text: "selected"
                                font.pixelSize: 12
                                color: root.bodyColor
                            }
                        }

                        RowLayout {
                            spacing: 6
                            visible: modManager.backupPath !== ""

                            Image {
                                Layout.preferredWidth: 13
                                Layout.preferredHeight: 13
                                source: "qrc:/UniversalModManager/Lucide Icons/folder.svg"
                                sourceSize.width: 13
                                sourceSize.height: 13
                                fillMode: Image.PreserveAspectFit
                                layer.enabled: true
                                layer.smooth: true
                                layer.textureSize: Qt.size(width * 2, height * 2)
                                layer.effect: ColorOverlay {
                                    color: root.faintColor
                                }
                            }

                            Text {
                                Layout.maximumWidth: 320
                                text: modManager.backupPath
                                font.pixelSize: 11
                                color: root.faintColor
                                elide: Text.ElideMiddle
                            }

                            Text {
                                text: "Change"
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                color: window.accentColor

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: backupPathPickerDialog.open()
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        id: selectionCancelButton
                        Layout.preferredWidth: 96
                        Layout.preferredHeight: 40
                        hoverEnabled: true

                        contentItem: Text {
                            text: "Cancel"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: selectionCancelButton.hovered ? window.secondaryButtonTextHoverColor : window.secondaryButtonTextColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 10
                            color: selectionCancelButton.hovered ? window.secondaryButtonHoverColor : window.secondaryButtonColor
                            border.width: 1
                            border.color: window.secondaryButtonBorderColor
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: selectionDialog.close()
                    }

                    Button {
                        id: backupSelectedButton
                        Layout.preferredHeight: 40
                        Layout.preferredWidth: Math.max(150, backupButtonLabel.implicitWidth + 36)
                        enabled: root.selectedCount > 0 && modManager.backupPath !== ""
                        hoverEnabled: true

                        contentItem: Text {
                            id: backupButtonLabel
                            text: root.backupButtonText()
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: backupSelectedButton.enabled
                                ? "#FFFFFF"
                                : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.35)
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 10
                            color: !backupSelectedButton.enabled
                                ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06)
                                : (backupSelectedButton.hovered ? Qt.lighter(window.primaryColor, 1.12) : window.primaryColor)
                            border.width: !backupSelectedButton.enabled ? 1 : 0
                            border.color: window.separatorColor
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: {
                            var names = root.collectSelectedNames()
                            selectionDialog.close()
                            root.backupRequested(names)
                        }
                    }
                }
            }
        }

        onOpened: {
            backupSearchField.text = ""
            root.populateSelection()
        }
    }
}
