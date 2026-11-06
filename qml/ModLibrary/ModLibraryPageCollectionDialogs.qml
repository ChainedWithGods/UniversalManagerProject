import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Item {
    id: collectionDialogs
    anchors.fill: parent

    property alias collectionDetailDialog: collectionDetailDialog
    property alias collectionPreviewFileDialog: collectionPreviewFileDialog
    property alias collectionsDialog: collectionsDialog
    property alias renameCollectionDialog: renameCollectionDialog

    readonly property bool isGlass: modManager.theme === "Glassmorphic"
    readonly property color glassDialogColor: Qt.rgba(0.075, 0.07, 0.095, 0.99)
    readonly property color glassDialogBorder: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.28)
    readonly property color glassPanelColor: Qt.rgba(1, 1, 1, 0.04)
    readonly property color glassPanelBorder: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.16)
    readonly property color glassTextSecondary: Qt.rgba(1, 1, 1, 0.62)
    readonly property color glassGhostColor: Qt.rgba(1, 1, 1, 0.06)
    readonly property color glassGhostBorder: Qt.rgba(1, 1, 1, 0.14)
    readonly property color glassRowBorder: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.14)
    readonly property color glassMenuColor: Qt.rgba(0.06, 0.055, 0.075, 0.985)
    readonly property color glassMenuBorder: Qt.rgba(1, 1, 1, 0.1)

    readonly property color dialogText: isGlass ? "#FFFFFF" : window.textColor
    readonly property color dialogSubText: isGlass ? glassTextSecondary : window.textSecondaryColor

    property int collectionsRevision: 0

    function refreshCollections() {
        collectionDialogs.collectionsRevision++
        modLibraryRoot.collectionStatusRefreshTrigger++
    }

    function openAddModsForBundle(bundleId, bundleName) {
        addModsToCollectionDialog.addMode = "bundle"
        addModsToCollectionDialog.targetBundleId = bundleId
        addModsToCollectionDialog.targetBundleName = bundleName
        addModsToCollectionDialog.open()
    }

    function openCreateBundleDialog() {
        addModsToCollectionDialog.addMode = "createBundle"
        addModsToCollectionDialog.targetBundleId = ""
        addModsToCollectionDialog.targetBundleName = ""
        addModsToCollectionDialog.open()
    }

    function totalCollectionMods() {
        var _ = collectionDialogs.collectionsRevision + modLibraryRoot.collectionStatusRefreshTrigger
        var list = modManager.collections || []
        var total = 0
        for (var i = 0; i < list.length; ++i)
            total += (list[i].mods || []).length
        return total
    }

    function enabledCountFor(collection) {
        var _ = collectionDialogs.collectionsRevision + modLibraryRoot.collectionStatusRefreshTrigger
        if (!collection || !collection.mods)
            return 0
        var enabled = 0
        for (var i = 0; i < collection.mods.length; ++i) {
            var details = modManager.getModDetails(collection.mods[i])
            if (details && details.enabled)
                enabled++
        }
        return enabled
    }

    function collectionPillText(collection) {
        if (!collection || (collection.mods || []).length === 0)
            return "Empty"
        var enabled = enabledCountFor(collection)
        if (enabled === collection.mods.length)
            return "All enabled"
        if (enabled === 0)
            return "Disabled"
        return enabled + " enabled"
    }

    function collectionPillActive(collection) {
        return enabledCountFor(collection) > 0
    }

    function ownerCollectionName(modName) {
        var list = modManager.collections || []
        for (var i = 0; i < list.length; ++i) {
            if ((list[i].mods || []).indexOf(modName) !== -1)
                return list[i].name
        }
        return ""
    }

    function openCollection(id, name) {
        collectionDetailDialog.currentCollectionId = id
        collectionDetailDialog.currentCollectionName = name
        collectionDetailDialog.loadCollectionMods()
        collectionDetailDialog.open()
    }

    Dialog {
        id: collectionsDialog
        modal: true
        width: Math.min(1200, parent.width * 0.92)
        height: Math.min(800, parent.height * 0.92)
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        palette {
            windowText: collectionDialogs.isGlass ? modManager.accentColor : window.textColor
            text: collectionDialogs.isGlass ? modManager.accentColor : window.textColor
        }

        background: Rectangle {
            color: collectionDialogs.isGlass
                ? collectionDialogs.glassDialogColor
                : (modManager.theme === "Crimson" || modManager.theme === "Wavey" ? window.surfaceColor : window.bgColor)
            radius: 18
            border.width: collectionDialogs.isGlass ? 1 : (modManager.theme === "Wavey" ? 1 : 0)
            border.color: collectionDialogs.isGlass
                ? collectionDialogs.glassDialogBorder
                : (modManager.theme === "Wavey" ? Qt.rgba(1, 1, 1, 0.1) : "transparent")

            gradient: {
                if (modManager.theme === "Cozy") return sharedCozyDialogGradient
                if (modManager.theme === "Crimson") return sharedCrimsonDialogGradient
                if (modManager.theme === "Wavey") return sharedWaveyDialogGradient
                return null
            }

            layer.enabled: collectionDialogs.isGlass
            layer.effect: DropShadow {
                transparentBorder: true
                radius: 32
                samples: 65
                color: Qt.rgba(0, 0, 0, 0.5)
                horizontalOffset: 0
                verticalOffset: 10
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                Image {
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    Layout.alignment: Qt.AlignVCenter
                    source: "qrc:/UniversalModManager/Lucide Icons/library-big.svg"
                    sourceSize.width: 52
                    sourceSize.height: 52
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    layer.enabled: true
                    layer.smooth: true
                    layer.textureSize: Qt.size(width * 2, height * 2)
                    layer.effect: ColorOverlay {
                        color: collectionDialogs.isGlass ? window.accentColor : window.primaryColor
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: "Collections"
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                        color: collectionDialogs.isGlass ? window.accentColor : window.textColor
                    }

                    Text {
                        text: {
                            var _ = collectionDialogs.collectionsRevision + modLibraryRoot.collectionStatusRefreshTrigger
                            var count = (modManager.collections || []).length
                            return count + (count === 1 ? " collection" : " collections")
                                + " · " + collectionDialogs.totalCollectionMods() + " mods"
                        }
                        font.pixelSize: 13
                        color: collectionDialogs.dialogSubText
                    }
                }

                Button {
                    Layout.preferredWidth: 168
                    Layout.preferredHeight: 38

                    visible: {
                        var _ = collectionDialogs.collectionsRevision + modLibraryRoot.collectionStatusRefreshTrigger
                        return (modManager.collections || []).length > 0
                    }
                    text: "New collection"
                    flat: true
                    hoverEnabled: true

                    contentItem: RowLayout {
                        spacing: 8
                        Layout.alignment: Qt.AlignCenter

                        Image {
                            source: "qrc:/UniversalModManager/Lucide Icons/plus.svg"
                            sourceSize.width: 15
                            sourceSize.height: 15
                            fillMode: Image.PreserveAspectFit
                            layer.enabled: true
                            layer.smooth: true
                            layer.textureSize: Qt.size(width * 2, height * 2)
                            layer.effect: ColorOverlay { color: "#FFFFFF" }
                        }

                        Text {
                            text: "New collection"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                        }
                    }

                    background: Rectangle {
                        color: window.accentColor
                        radius: 10
                        opacity: parent.hovered ? 0.9 : 1.0
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }

                    onClicked: newCollectionDialog.open()
                }

                Button {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    flat: true
                    hoverEnabled: true

                    contentItem: Image {
                        source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                        sourceSize.width: 17
                        sourceSize.height: 17
                        fillMode: Image.PreserveAspectFit
                        layer.enabled: true
                        layer.smooth: true
                        layer.textureSize: Qt.size(width * 2, height * 2)
                        layer.effect: ColorOverlay {
                            color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.6) : window.textSecondaryColor
                        }
                    }

                    background: Rectangle {
                        color: parent.hovered
                            ? (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06))
                            : "transparent"
                        radius: 8
                    }

                    onClicked: collectionsDialog.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: collectionDialogs.isGlass ? collectionDialogs.glassPanelColor : window.surfaceColor
                radius: 14
                border.width: 1
                border.color: collectionDialogs.isGlass ? collectionDialogs.glassPanelBorder : window.borderColor

                Flickable {
                    id: collectionsFlickable
                    anchors.fill: parent
                    anchors.margins: 14
                    clip: true
                    contentWidth: width
                    contentHeight: collectionsFlow.height + 8
                    boundsBehavior: Flickable.StopAtBounds
                    flickableDirection: Flickable.VerticalFlick

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    Flow {
                        id: collectionsFlow
                        anchors.horizontalCenter: parent.horizontalCenter
                        readonly property int cardSpacing: 14
                        readonly property int minCardWidth: 320
                        readonly property int columns: Math.max(1, Math.floor((collectionsFlickable.width + cardSpacing) / (minCardWidth + cardSpacing)))
                        readonly property int cardWidth: columns > 0
                            ? Math.floor((collectionsFlickable.width - (columns - 1) * cardSpacing) / columns)
                            : minCardWidth
                        width: columns * cardWidth + (columns - 1) * cardSpacing
                        spacing: cardSpacing

                        Repeater {
                            model: modManager.collections

                            Rectangle {
                                id: dialogCollectionCard
                                width: collectionsFlow.cardWidth
                                height: Math.round(width * 9 / 16)
                                radius: 0
                                color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.05) : window.cardColor1
                                clip: false
                                border.width: 1
                                border.color: collectionDialogs.isGlass
                                    ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.22)
                                    : window.borderColor

                                readonly property bool hasPreview: (modelData.preview || "") !== ""
                                readonly property int modCount: (modelData.mods || []).length
                                readonly property int enabledCount: collectionDialogs.enabledCountFor(modelData)

                                HoverHandler {
                                    id: cardHover
                                }

                                Item {
                                    anchors.fill: parent
                                    anchors.margins: dialogCollectionCard.border.width
                                    visible: dialogCollectionCard.hasPreview
                                    clip: true

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: -24
                                        source: dialogCollectionCard.hasPreview ? "file:///" + modelData.preview : ""
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        cache: false
                                        sourceSize: Qt.size(Math.ceil(parent.width * 2), Math.ceil(parent.height * 2))

                                        layer.enabled: true
                                        layer.effect: MultiEffect {
                                            blurEnabled: true
                                            blur: 1.0
                                            blurMax: 64
                                            blurMultiplier: 1.0
                                            saturation: 0.35
                                            brightness: -0.45
                                        }
                                    }

                                    Image {
                                        anchors.fill: parent
                                        source: dialogCollectionCard.hasPreview ? "file:///" + modelData.preview : ""
                                        fillMode: Image.PreserveAspectFit
                                        asynchronous: true
                                        cache: false
                                        smooth: true
                                        sourceSize: Qt.size(Math.ceil(parent.width * 2), Math.ceil(parent.height * 2))
                                    }

                                    Rectangle {
                                        anchors.fill: parent
                                        gradient: Gradient {
                                            GradientStop { position: 0.0; color: "transparent" }
                                            GradientStop { position: 0.58; color: "transparent" }
                                            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.55) }
                                        }
                                    }
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    visible: !dialogCollectionCard.hasPreview
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.16) }
                                        GradientStop { position: 0.55; color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.04) }
                                        GradientStop { position: 1.0; color: collectionDialogs.isGlass ? Qt.rgba(0, 0, 0, 0.18) : window.cardColor1 }
                                    }
                                }

                                Rectangle {
                                    anchors.centerIn: parent
                                    anchors.verticalCenterOffset: -18
                                    width: 56
                                    height: 56
                                    radius: 14
                                    visible: !dialogCollectionCard.hasPreview
                                    color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.14)

                                    Image {
                                        anchors.centerIn: parent
                                        source: "qrc:/UniversalModManager/Lucide Icons/folder.svg"
                                        sourceSize.width: 26
                                        sourceSize.height: 26
                                        fillMode: Image.PreserveAspectFit
                                        layer.enabled: true
                                        layer.smooth: true
                                        layer.textureSize: Qt.size(width * 2, height * 2)
                                        layer.effect: ColorOverlay { color: window.accentColor }
                                    }
                                }

                                Item {
                                    anchors.centerIn: parent
                                    width: cardHover.hovered ? dialogCollectionCard.width : 0
                                    height: dialogCollectionCard.height
                                    clip: true
                                    z: 8

                                    Rectangle {
                                        width: dialogCollectionCard.width
                                        height: dialogCollectionCard.height
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        color: "transparent"
                                        border.width: 2
                                        border.color: window.accentColor
                                    }

                                    Behavior on width {
                                        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                                    }
                                }

                                Button {
                                    id: dialogCollectionMenuBtn
                                    anchors.top: parent.top
                                    anchors.right: parent.right
                                    anchors.margins: 9
                                    width: 30
                                    height: 30
                                    flat: true
                                    hoverEnabled: true
                                    z: 10

                                    contentItem: Image {
                                        source: "qrc:/UniversalModManager/Lucide Icons/ellipsis-vertical.svg"
                                        sourceSize.width: 17
                                        sourceSize.height: 17
                                        fillMode: Image.PreserveAspectFit
                                        layer.enabled: true
                                        layer.smooth: true
                                        layer.textureSize: Qt.size(width * 2, height * 2)
                                        layer.effect: ColorOverlay {
                                            color: dialogCollectionCard.hasPreview ? "#FFFFFF" : window.textColor
                                        }
                                    }

                                    background: Rectangle {
                                        color: parent.hovered
                                            ? (dialogCollectionCard.hasPreview ? Qt.rgba(0, 0, 0, 0.4) : Qt.rgba(0, 0, 0, 0.08))
                                            : (dialogCollectionCard.hasPreview ? Qt.rgba(0, 0, 0, 0.25) : "transparent")
                                        radius: 7
                                    }

                                    onClicked: dialogCollectionContextMenu.open()

                                    Menu {
                                        id: dialogCollectionContextMenu
                                        y: dialogCollectionMenuBtn.height + 4
                                        width: 200
                                        padding: 6

                                        background: Rectangle {
                                            color: collectionDialogs.isGlass
                                                ? collectionDialogs.glassMenuColor
                                                : window.surfaceColor
                                            radius: 12
                                            border.width: 1
                                            border.color: collectionDialogs.isGlass
                                                ? collectionDialogs.glassMenuBorder
                                                : window.borderColor

                                            layer.enabled: true
                                            layer.effect: MultiEffect {
                                                shadowEnabled: true
                                                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                                                shadowHorizontalOffset: 0
                                                shadowVerticalOffset: 8
                                                shadowBlur: 0.75
                                            }
                                        }

                                        ContextMenuItem {
                                            text: dialogCollectionCard.hasPreview ? "Change Preview" : "Add Preview"
                                            iconName: "image.svg"
                                            onTriggered: {
                                                collectionPreviewFileDialog.collectionId = modelData.id
                                                collectionPreviewFileDialog.open()
                                            }
                                        }

                                        ContextMenuItem {
                                            text: "Remove Preview"
                                            iconName: "x.svg"
                                            visible: dialogCollectionCard.hasPreview
                                            height: visible ? implicitHeight : 0
                                            onTriggered: {
                                                modManager.setCollectionPreview(modelData.id, "")
                                                collectionDialogs.refreshCollections()
                                                toastNotification.show("Preview removed", "x.svg")
                                            }
                                        }

                                        ContextMenuItem {
                                            text: "Enable Collection"
                                            iconName: "play.svg"
                                            visible: dialogCollectionCard.modCount > 0 && dialogCollectionCard.enabledCount === 0
                                            height: visible ? implicitHeight : 0
                                            onTriggered: {
                                                modManager.toggleCollectionStatus(modelData.id, true)
                                                collectionDialogs.refreshCollections()
                                                toastNotification.show("Collection enabled", "play.svg")
                                            }
                                        }

                                        ContextMenuItem {
                                            text: "Disable Collection"
                                            iconName: "pause.svg"
                                            visible: dialogCollectionCard.modCount > 0 && dialogCollectionCard.enabledCount > 0
                                            height: visible ? implicitHeight : 0
                                            onTriggered: {
                                                modManager.toggleCollectionStatus(modelData.id, false)
                                                collectionDialogs.refreshCollections()
                                                toastNotification.show("Collection disabled", "pause.svg")
                                            }
                                        }

                                        ContextMenuItem {
                                            text: "Rename"
                                            iconName: "pencil.svg"
                                            onTriggered: {
                                                renameCollectionDialog.collectionId = modelData.id
                                                renameCollectionDialog.currentName = modelData.name
                                                renameCollectionDialog.open()
                                            }
                                        }

                                        ContextMenuItem {
                                            text: "Delete"
                                            iconName: "trash-2.svg"
                                            isDanger: true
                                            onTriggered: {
                                                modManager.deleteCollection(modelData.id)
                                                collectionDialogs.refreshCollections()
                                                toastNotification.show("Collection deleted", "trash-2.svg")
                                            }
                                        }
                                    }
                                }

                                Item {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 72

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 18
                                        anchors.rightMargin: 18
                                        anchors.topMargin: 10
                                        anchors.bottomMargin: 14
                                        spacing: 3

                                        Item { Layout.fillHeight: true }

                                        Text {
                                            text: modelData.name
                                            font.pixelSize: 16
                                            font.weight: Font.Bold
                                            color: dialogCollectionCard.hasPreview || collectionDialogs.isGlass ? "#FFFFFF" : window.textColor
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 6

                                            Text {
                                                text: dialogCollectionCard.modCount === 0
                                                    ? "Empty"
                                                    : dialogCollectionCard.modCount + (dialogCollectionCard.modCount === 1 ? " mod" : " mods")
                                                        + " · " + dialogCollectionCard.enabledCount + " enabled"
                                                font.pixelSize: 12
                                                font.weight: Font.Medium
                                                color: dialogCollectionCard.hasPreview || collectionDialogs.isGlass
                                                    ? Qt.rgba(1, 1, 1, 0.68)
                                                    : window.textSecondaryColor
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    z: 1
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: collectionDialogs.openCollection(modelData.id, modelData.name)
                                }
                            }
                        }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 64, 460)
                    spacing: 12
                    visible: !modManager.collections || modManager.collections.length === 0

                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 30
                        height: 30
                        source: "qrc:/UniversalModManager/Lucide Icons/library-big.svg"
                        sourceSize.width: 60
                        sourceSize.height: 60
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        layer.enabled: true
                        layer.smooth: true
                        layer.textureSize: Qt.size(width * 2, height * 2)
                        layer.effect: ColorOverlay { color: window.accentColor }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "No collections yet"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: collectionDialogs.dialogText
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width
                        text: "Group mods into a collection to switch a whole setup on or off at once."
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        color: collectionDialogs.dialogSubText
                    }

                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "New collection"
                        flat: true
                        hoverEnabled: true

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            implicitWidth: 160
                            implicitHeight: 40
                            radius: 11
                            color: window.accentColor
                            opacity: parent.hovered ? 0.9 : 1.0
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                        }

                        onClicked: newCollectionDialog.open()
                    }
                }
            }
        }
    }

    Dialog {
        id: collectionDetailDialog
        modal: true
        width: Math.min(1200, parent.width * 0.92)
        height: Math.min(800, parent.height * 0.92)
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property string currentCollectionId: ""
        property string currentCollectionName: ""
        property string currentPreview: ""
        property var currentCollectionMods: []
        property int modStatusRefreshTrigger: 0
        property string modSearch: ""
        property string modFilter: "all"

        function loadCollectionMods() {
            var collections = modManager.collections || []
            for (var i = 0; i < collections.length; i++) {
                if (collections[i].id === currentCollectionId) {
                    currentCollectionMods = collections[i].mods || []
                    currentPreview = collections[i].preview || ""
                    currentCollectionName = collections[i].name || currentCollectionName
                    return
                }
            }
            currentCollectionMods = []
            currentPreview = ""
        }

        function enabledCount() {
            var _ = collectionDetailDialog.modStatusRefreshTrigger
            var enabled = 0
            for (var i = 0; i < currentCollectionMods.length; ++i) {
                var details = modManager.getModDetails(currentCollectionMods[i])
                if (details && details.enabled)
                    enabled++
            }
            return enabled
        }

        function visibleMods() {
            var _ = collectionDetailDialog.modStatusRefreshTrigger
            var search = collectionDetailDialog.modSearch.trim().toLowerCase()
            var result = []
            for (var i = 0; i < currentCollectionMods.length; ++i) {
                var name = currentCollectionMods[i]
                if (search !== "" && name.toLowerCase().indexOf(search) === -1)
                    continue
                var details = modManager.getModDetails(name)
                var enabled = details ? !!details.enabled : false
                if (collectionDetailDialog.modFilter === "enabled" && !enabled)
                    continue
                if (collectionDetailDialog.modFilter === "disabled" && enabled)
                    continue
                result.push(name)
            }
            return result
        }

        function toggleAll(enable) {
            if (currentCollectionMods.length === 0)
                return
            modManager.toggleCollectionStatus(currentCollectionId, enable)
            modStatusRefreshTrigger++
            collectionDialogs.refreshCollections()
            toastNotification.show(enable ? "Collection enabled" : "Collection disabled",
                                   enable ? "play.svg" : "pause.svg")
        }

        Connections {
            target: modManager
            function onModListChanged() {
                modLibraryRoot.collectionStatusRefreshTrigger++
                collectionDetailDialog.modStatusRefreshTrigger++
                if (collectionDetailDialog.visible)
                    collectionDetailDialog.loadCollectionMods()
            }
            function onCollectionsChanged() {
                if (collectionDetailDialog.visible)
                    collectionDetailDialog.loadCollectionMods()
            }
        }

        palette {
            windowText: collectionDialogs.isGlass ? modManager.accentColor : window.textColor
            text: collectionDialogs.isGlass ? modManager.accentColor : window.textColor
        }

        background: Rectangle {
            color: collectionDialogs.isGlass
                ? collectionDialogs.glassDialogColor
                : (modManager.theme === "Crimson" || modManager.theme === "Wavey" ? window.surfaceColor : window.bgColor)
            radius: 18
            border.width: collectionDialogs.isGlass ? 1 : (modManager.theme === "Wavey" ? 1 : 0)
            border.color: collectionDialogs.isGlass
                ? collectionDialogs.glassDialogBorder
                : (modManager.theme === "Wavey" ? Qt.rgba(1, 1, 1, 0.1) : "transparent")

            gradient: {
                if (modManager.theme === "Cozy") return sharedCozyDialogGradient
                if (modManager.theme === "Crimson") return sharedCrimsonDialogGradient
                if (modManager.theme === "Wavey") return sharedWaveyDialogGradient
                return null
            }

            layer.enabled: collectionDialogs.isGlass
            layer.effect: DropShadow {
                transparentBorder: true
                radius: 32
                samples: 65
                color: Qt.rgba(0, 0, 0, 0.5)
                horizontalOffset: 0
                verticalOffset: 10
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    flat: true
                    hoverEnabled: true

                    contentItem: Image {
                        source: "qrc:/UniversalModManager/Lucide Icons/arrow-left.svg"
                        sourceSize.width: 21
                        sourceSize.height: 21
                        fillMode: Image.PreserveAspectFit
                        layer.enabled: true
                        layer.smooth: true
                        layer.textureSize: Qt.size(width * 2, height * 2)
                        layer.effect: ColorOverlay {
                            color: collectionDialogs.isGlass ? "#FFFFFF" : window.textColor
                        }
                    }

                    background: Rectangle {
                        color: parent.hovered
                            ? (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06))
                            : "transparent"
                        radius: 9
                    }

                    onClicked: collectionDetailDialog.close()
                }

                Image {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    Layout.alignment: Qt.AlignVCenter
                    source: "qrc:/UniversalModManager/Lucide Icons/folder.svg"
                    sourceSize.width: 48
                    sourceSize.height: 48
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    layer.enabled: true
                    layer.smooth: true
                    layer.textureSize: Qt.size(width * 2, height * 2)
                    layer.effect: ColorOverlay {
                        color: collectionDialogs.isGlass ? window.accentColor : window.primaryColor
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: collectionDetailDialog.currentCollectionName
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                        color: collectionDialogs.isGlass ? window.accentColor : window.textColor
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        text: collectionDetailDialog.currentCollectionMods.length
                            + (collectionDetailDialog.currentCollectionMods.length === 1 ? " mod · " : " mods · ")
                            + collectionDetailDialog.enabledCount() + " enabled"
                        font.pixelSize: 13
                        color: collectionDialogs.dialogSubText
                    }
                }

                Button {
                    Layout.preferredWidth: 138
                    Layout.preferredHeight: 38
                    text: "Add mods"
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: "Add mods"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: window.accentColor
                        radius: 10
                        opacity: parent.hovered ? 0.9 : 1.0
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }

                    onClicked: {
                        addModsToCollectionDialog.addMode = "collection"
                        addModsToCollectionDialog.open()
                    }
                }

                Button {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    flat: true
                    hoverEnabled: true

                    contentItem: Image {
                        source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                        sourceSize.width: 17
                        sourceSize.height: 17
                        fillMode: Image.PreserveAspectFit
                        layer.enabled: true
                        layer.smooth: true
                        layer.textureSize: Qt.size(width * 2, height * 2)
                        layer.effect: ColorOverlay {
                            color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.6) : window.textSecondaryColor
                        }
                    }

                    background: Rectangle {
                        color: parent.hovered
                            ? (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06))
                            : "transparent"
                        radius: 8
                    }

                    onClicked: collectionDetailDialog.close()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 250
                            Layout.preferredHeight: 34
                            radius: 9
                            color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.05) : window.cardColor1
                            border.width: 1
                            border.color: detailSearchField.activeFocus
                                ? window.accentColor
                                : (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.1) : window.borderColor)

                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8

                                Image {
                                    source: "qrc:/UniversalModManager/Lucide Icons/search.svg"
                                    sourceSize.width: 14
                                    sourceSize.height: 14
                                    fillMode: Image.PreserveAspectFit
                                    layer.enabled: true
                                    layer.smooth: true
                                    layer.textureSize: Qt.size(width * 2, height * 2)
                                    layer.effect: ColorOverlay {
                                        color: collectionDialogs.isGlass ? window.accentColor : window.textSecondaryColor
                                    }
                                }

                                TextField {
                                    id: detailSearchField
                                    Layout.fillWidth: true
                                    text: collectionDetailDialog.modSearch
                                    placeholderText: "Find in this collection…"
                                    font.pixelSize: 12
                                    color: collectionDialogs.dialogText
                                    placeholderTextColor: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.4) : window.textSecondaryColor
                                    selectByMouse: true
                                    padding: 0
                                    background: Rectangle { color: "transparent" }

                                    ContextMenu.menu: null

                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        cursorShape: Qt.IBeamCursor
                                        onPressed: function(mouse) {
                                            detailSearchField.forceActiveFocus()
                                            detailSearchContextMenu.popup(detailSearchField, mouse.x, mouse.y)
                                            mouse.accepted = true
                                        }
                                        onPressAndHold: function(mouse) {
                                            detailSearchField.forceActiveFocus()
                                            detailSearchContextMenu.popup(detailSearchField, mouse.x, mouse.y)
                                            mouse.accepted = true
                                        }
                                    }

                                    Menu {
                                        id: detailSearchContextMenu
                                        width: 180
                                        padding: 6

                                        background: Rectangle {
                                            color: collectionDialogs.isGlass ? collectionDialogs.glassMenuColor : window.surfaceColor
                                            radius: 12
                                            border.width: 1
                                            border.color: collectionDialogs.isGlass ? collectionDialogs.glassMenuBorder : window.borderColor
                                        }

                                        ContextMenuItem { text: "Undo"; iconName: "undo-2.svg"; enabled: detailSearchField.canUndo; onTriggered: detailSearchField.undo() }
                                        ContextMenuItem { text: "Redo"; iconName: "redo-2.svg"; enabled: detailSearchField.canRedo; onTriggered: detailSearchField.redo() }
                                        MenuSeparator {}
                                        ContextMenuItem { text: "Cut"; iconName: "scissors.svg"; enabled: detailSearchField.selectedText.length > 0; onTriggered: detailSearchField.cut() }
                                        ContextMenuItem { text: "Copy"; iconName: "copy.svg"; enabled: detailSearchField.selectedText.length > 0; onTriggered: detailSearchField.copy() }
                                        ContextMenuItem { text: "Paste"; iconName: "clipboard.svg"; enabled: true; onTriggered: detailSearchField.paste() }
                                        ContextMenuItem { text: "Delete"; iconName: "trash.svg"; enabled: detailSearchField.selectedText.length > 0; onTriggered: detailSearchField.remove(detailSearchField.selectionStart, detailSearchField.selectionEnd) }
                                        MenuSeparator {}
                                        ContextMenuItem { text: "Select All"; iconName: "text-select.svg"; enabled: detailSearchField.text.length > 0; onTriggered: detailSearchField.selectAll() }
                                    }

                                    onTextChanged: {
                                        if (collectionDetailDialog.modSearch !== text)
                                            collectionDetailDialog.modSearch = text
                                    }
                                }
                            }
                        }

                        RowLayout {
                            spacing: 4

                            Repeater {
                                model: [
                                    { key: "all", label: "All" },
                                    { key: "enabled", label: "Enabled" },
                                    { key: "disabled", label: "Disabled" }
                                ]

                                delegate: Rectangle {
                                    height: 30
                                    width: filterLabel.implicitWidth + 20
                                    radius: 8
                                    color: collectionDetailDialog.modFilter === modelData.key
                                        ? (collectionDialogs.isGlass
                                            ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.16)
                                            : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.14))
                                        : "transparent"
                                    border.width: 1
                                    border.color: collectionDetailDialog.modFilter === modelData.key
                                        ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.4)
                                        : "transparent"

                                    Text {
                                        id: filterLabel
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        color: collectionDetailDialog.modFilter === modelData.key
                                            ? (collectionDialogs.isGlass ? window.accentColor : window.primaryColor)
                                            : collectionDialogs.dialogSubText
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: collectionDetailDialog.modFilter = modelData.key
                                    }
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            Layout.preferredHeight: 30
                            flat: true
                            hoverEnabled: true
                            enabled: collectionDetailDialog.currentCollectionMods.length > 0

                            contentItem: Text {
                                text: "Enable all"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: parent.enabled ? window.accentColor : collectionDialogs.dialogSubText
                            }

                            background: Rectangle {
                                color: parent.hovered ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.1) : "transparent"
                                radius: 8
                            }

                            onClicked: collectionDetailDialog.toggleAll(true)
                        }

                        Button {
                            Layout.preferredHeight: 30
                            flat: true
                            hoverEnabled: true
                            enabled: collectionDetailDialog.currentCollectionMods.length > 0

                            contentItem: Text {
                                text: "Disable all"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: parent.enabled ? collectionDialogs.dialogSubText : Qt.rgba(1, 1, 1, 0.25)
                            }

                            background: Rectangle {
                                color: parent.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                                radius: 8
                            }

                            onClicked: collectionDetailDialog.toggleAll(false)
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ListView {
                            id: collectionModsList
                            anchors.fill: parent
                            clip: true
                            spacing: 6
                            model: collectionDetailDialog.visibleMods()
                            visible: count > 0
                            boundsBehavior: Flickable.StopAtBounds
                            flickDeceleration: 1400
                            maximumFlickVelocity: 9000

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                                width: 10
                            }

                            delegate: Rectangle {
                                id: collectionModDelegate
                                width: ListView.view.width
                                height: 58
                                radius: 11
                                color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.05) : window.cardColor1
                                border.width: 1
                                border.color: modHover.hovered
                                    ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.5)
                                    : (collectionDialogs.isGlass
                                        ? collectionDialogs.glassRowBorder
                                        : window.borderColor)

                                Behavior on border.color { ColorAnimation { duration: 150 } }

                                property var _dlg: collectionDetailDialog
                                property bool modEnabled: {
                                    var _ = _dlg.modStatusRefreshTrigger
                                    var details = modManager.getModDetails(modelData)
                                    return details ? !!details.enabled : false
                                }

                                HoverHandler { id: modHover }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 10
                                    spacing: 11

                                    Image {
                                        Layout.preferredWidth: 18
                                        Layout.preferredHeight: 18
                                        Layout.alignment: Qt.AlignVCenter
                                        source: "qrc:/UniversalModManager/Lucide Icons/package.svg"
                                        sourceSize.width: 36
                                        sourceSize.height: 36
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        layer.enabled: true
                                        layer.smooth: true
                                        layer.textureSize: Qt.size(width * 2, height * 2)
                                        layer.effect: ColorOverlay { color: window.accentColor }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1

                                        Text {
                                            text: modelData
                                            font.pixelSize: 13
                                            font.weight: Font.Medium
                                            color: collectionDialogs.isGlass ? "#FFFFFF" : window.textColor
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: {
                                                var details = modManager.getModDetails(modelData)
                                                return details ? (details.size || "N/A") : "N/A"
                                            }
                                            font.pixelSize: 11
                                            color: collectionDialogs.dialogSubText
                                        }
                                    }

                                    Rectangle {
                                        Layout.preferredHeight: 20
                                        Layout.preferredWidth: modStateLabel.implicitWidth + 16
                                        radius: 10
                                        color: collectionModDelegate.modEnabled
                                            ? Qt.rgba(0.13, 0.77, 0.36, 0.18)
                                            : (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g, window.textSecondaryColor.b, 0.18))
                                        border.width: 1
                                        border.color: collectionModDelegate.modEnabled
                                            ? Qt.rgba(0.13, 0.77, 0.36, 0.45)
                                            : (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g, window.textSecondaryColor.b, 0.35))

                                        Text {
                                            id: modStateLabel
                                            anchors.centerIn: parent
                                            text: collectionModDelegate.modEnabled ? "Enabled" : "Disabled"
                                            font.pixelSize: 10
                                            font.weight: Font.DemiBold
                                            color: collectionModDelegate.modEnabled
                                                ? (collectionDialogs.isGlass ? Qt.rgba(0.55, 0.94, 0.66, 1.0) : Qt.rgba(0.10, 0.55, 0.28, 1.0))
                                                : (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.55) : window.textSecondaryColor)
                                        }
                                    }

                                    Button {
                                        Layout.preferredWidth: 28
                                        Layout.preferredHeight: 28
                                        flat: true
                                        hoverEnabled: true

                                        contentItem: Image {
                                            source: collectionModDelegate.modEnabled
                                                ? "qrc:/UniversalModManager/Lucide Icons/pause.svg"
                                                : "qrc:/UniversalModManager/Lucide Icons/play.svg"
                                            sourceSize.width: 15
                                            sourceSize.height: 15
                                            fillMode: Image.PreserveAspectFit
                                            layer.enabled: true
                                            layer.smooth: true
                                            layer.textureSize: Qt.size(width * 2, height * 2)
                                            layer.effect: ColorOverlay { color: window.accentColor }
                                        }

                                        background: Rectangle {
                                            color: parent.hovered ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.12) : "transparent"
                                            radius: 7
                                        }

                                        onClicked: {
                                            var enable = !collectionModDelegate.modEnabled
                                            modManager.toggleModStatusBatch([modelData], enable)
                                            collectionDetailDialog.modStatusRefreshTrigger++
                                            collectionDialogs.refreshCollections()
                                            toastNotification.show(enable ? "Mod enabled" : "Mod disabled",
                                                                   enable ? "play.svg" : "pause.svg")
                                        }
                                    }

                                    Button {
                                        Layout.preferredWidth: 28
                                        Layout.preferredHeight: 28
                                        flat: true
                                        hoverEnabled: true

                                        contentItem: Image {
                                            source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                                            sourceSize.width: 15
                                            sourceSize.height: 15
                                            fillMode: Image.PreserveAspectFit
                                            layer.enabled: true
                                            layer.smooth: true
                                            layer.textureSize: Qt.size(width * 2, height * 2)
                                            layer.effect: ColorOverlay { color: "#EF4444" }
                                        }

                                        background: Rectangle {
                                            color: parent.hovered ? Qt.rgba(0.93, 0.26, 0.26, 0.1) : "transparent"
                                            radius: 7
                                        }

                                        onClicked: {
                                            modManager.removeModFromCollection(collectionDetailDialog.currentCollectionId, modelData)
                                            collectionDetailDialog.loadCollectionMods()
                                            collectionDialogs.refreshCollections()
                                            toastNotification.show("Removed from collection", "x.svg")
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 60, 420)
                            spacing: 8
                            visible: collectionDetailDialog.currentCollectionMods.length === 0

                            Image {
                                Layout.alignment: Qt.AlignHCenter
                                width: 26
                                height: 26
                                source: "qrc:/UniversalModManager/Lucide Icons/folder.svg"
                                sourceSize.width: 52
                                sourceSize.height: 52
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                layer.enabled: true
                                layer.smooth: true
                                layer.textureSize: Qt.size(width * 2, height * 2)
                                layer.effect: ColorOverlay { color: window.accentColor }
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "No mods in this collection yet"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: collectionDialogs.dialogText
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Use Add mods to pick from your library."
                                font.pixelSize: 12
                                color: collectionDialogs.dialogSubText
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 60, 420)
                            spacing: 8
                            visible: collectionDetailDialog.currentCollectionMods.length > 0
                                && collectionDetailDialog.visibleMods().length === 0

                            Image {
                                Layout.alignment: Qt.AlignHCenter
                                width: 24
                                height: 24
                                source: "qrc:/UniversalModManager/Lucide Icons/search.svg"
                                sourceSize.width: 24
                                sourceSize.height: 24
                                fillMode: Image.PreserveAspectFit
                                layer.enabled: true
                                layer.smooth: true
                                layer.textureSize: Qt.size(width * 2, height * 2)
                                layer.effect: ColorOverlay {
                                    color: collectionDialogs.isGlass ? window.accentColor : window.textSecondaryColor
                                }
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "No matching mods"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: collectionDialogs.dialogText
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Try another search or filter."
                                font.pixelSize: 12
                                color: collectionDialogs.dialogSubText
                            }
                        }
                    }
                }

                ColumnLayout {
                    id: detailSidebar
                    Layout.preferredWidth: 296
                    Layout.fillHeight: true
                    spacing: 10

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 12
                        color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.04) : window.cardColor1
                        border.width: 1
                        border.color: collectionDialogs.isGlass ? collectionDialogs.glassRowBorder : window.borderColor
                        implicitHeight: previewColumn.implicitHeight + 22

                        ColumnLayout {
                            id: previewColumn
                            anchors.fill: parent
                            anchors.margins: 11
                            spacing: 8

                            Text {
                                text: "PREVIEW"
                                font.pixelSize: 10
                                font.letterSpacing: 1
                                font.weight: Font.Bold
                                color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.35) : window.textSecondaryColor
                            }

                            Rectangle {
                                id: previewBox
                                Layout.fillWidth: true

                                readonly property real previewAspect: previewImage.implicitWidth > 0
                                    && previewImage.implicitHeight > 0
                                    ? previewImage.implicitWidth / previewImage.implicitHeight
                                    : 0
                                readonly property real uncappedHeight: previewAspect > 0
                                    ? width / previewAspect
                                    : previewPlaceholder.implicitHeight + 28
                                readonly property real maxPreviewHeight: detailSidebar.height * 0.6
                                Layout.preferredHeight: Math.min(uncappedHeight, maxPreviewHeight)
                                radius: 9
                                clip: true
                                color: "transparent"

                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.18) }
                                    GradientStop { position: 1.0; color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.03) }
                                }

                                Image {
                                    id: previewImage
                                    anchors.fill: parent
                                    source: collectionDetailDialog.currentPreview !== "" ? "file:///" + collectionDetailDialog.currentPreview : ""
                                    visible: collectionDetailDialog.currentPreview !== ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: false
                                }

                                Image {
                                    id: previewPlaceholder
                                    anchors.centerIn: parent
                                    visible: collectionDetailDialog.currentPreview === ""
                                    source: "qrc:/UniversalModManager/Lucide Icons/folder.svg"
                                    sourceSize.width: 30
                                    sourceSize.height: 30
                                    fillMode: Image.PreserveAspectFit
                                    layer.enabled: true
                                    layer.smooth: true
                                    layer.textureSize: Qt.size(width * 2, height * 2)
                                    layer.effect: ColorOverlay { color: window.accentColor }
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                flat: true
                                hoverEnabled: true

                                contentItem: RowLayout {
                                    spacing: 6

                                    Text {
                                        Layout.fillWidth: true
                                        text: "Change preview"
                                        font.pixelSize: 12
                                        color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.75) : window.textColor
                                        elide: Text.ElideRight
                                    }

                                    Image {
                                        source: "qrc:/UniversalModManager/Lucide Icons/chevron-right.svg"
                                        sourceSize.width: 13
                                        sourceSize.height: 13
                                        fillMode: Image.PreserveAspectFit
                                        layer.enabled: true
                                        layer.smooth: true
                                        layer.textureSize: Qt.size(width * 2, height * 2)
                                        layer.effect: ColorOverlay { color: collectionDialogs.dialogSubText }
                                    }
                                }

                                background: Rectangle {
                                    color: parent.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent"
                                    radius: 7
                                }

                                onClicked: {
                                    collectionPreviewFileDialog.collectionId = collectionDetailDialog.currentCollectionId
                                    collectionPreviewFileDialog.open()
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 12
                        color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.04) : window.cardColor1
                        border.width: 1
                        border.color: collectionDialogs.isGlass ? collectionDialogs.glassRowBorder : window.borderColor
                        implicitHeight: collectionColumn.implicitHeight + 22

                        ColumnLayout {
                            id: collectionColumn
                            anchors.fill: parent
                            anchors.margins: 11
                            spacing: 6

                            Text {
                                text: "COLLECTION"
                                font.pixelSize: 10
                                font.letterSpacing: 1
                                font.weight: Font.Bold
                                color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.35) : window.textSecondaryColor
                            }

                            Button {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                flat: true
                                hoverEnabled: true

                                contentItem: RowLayout {
                                    spacing: 6

                                    Text {
                                        Layout.fillWidth: true
                                        text: "Rename"
                                        font.pixelSize: 12
                                        color: collectionDialogs.dialogText
                                    }

                                    Image {
                                        source: "qrc:/UniversalModManager/Lucide Icons/chevron-right.svg"
                                        sourceSize.width: 13
                                        sourceSize.height: 13
                                        fillMode: Image.PreserveAspectFit
                                        layer.enabled: true
                                        layer.smooth: true
                                        layer.textureSize: Qt.size(width * 2, height * 2)
                                        layer.effect: ColorOverlay { color: collectionDialogs.dialogSubText }
                                    }
                                }

                                background: Rectangle {
                                    color: parent.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent"
                                    radius: 7
                                }

                                onClicked: {
                                    renameCollectionDialog.collectionId = collectionDetailDialog.currentCollectionId
                                    renameCollectionDialog.currentName = collectionDetailDialog.currentCollectionName
                                    renameCollectionDialog.open()
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                flat: true
                                hoverEnabled: true

                                contentItem: RowLayout {
                                    spacing: 6

                                    Text {
                                        Layout.fillWidth: true
                                        text: "Delete collection"
                                        font.pixelSize: 12
                                        color: "#F27070"
                                    }

                                    Image {
                                        source: "qrc:/UniversalModManager/Lucide Icons/chevron-right.svg"
                                        sourceSize.width: 13
                                        sourceSize.height: 13
                                        fillMode: Image.PreserveAspectFit
                                        layer.enabled: true
                                        layer.smooth: true
                                        layer.textureSize: Qt.size(width * 2, height * 2)
                                        layer.effect: ColorOverlay { color: Qt.rgba(0.95, 0.44, 0.44, 0.6) }
                                    }
                                }

                                background: Rectangle {
                                    color: parent.hovered ? Qt.rgba(0.93, 0.26, 0.26, 0.08) : "transparent"
                                    radius: 7
                                }

                                onClicked: {
                                    modManager.deleteCollection(collectionDetailDialog.currentCollectionId)
                                    collectionDialogs.refreshCollections()
                                    toastNotification.show("Collection deleted", "trash-2.svg")
                                    collectionDetailDialog.close()
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }

        onOpened: {
            modSearch = ""
            modFilter = "all"
            detailSearchField.text = ""
            modStatusRefreshTrigger++
            loadCollectionMods()
        }
    }

    Dialog {
        id: addModsToCollectionDialog
        modal: true
        width: Math.min(900, parent.width * 0.85)
        height: Math.min(700, parent.height * 0.85)
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property var selectedModsToAdd: []
        property string addMode: "collection"
        property string targetBundleId: ""
        property string targetBundleName: ""

        function isModInAnyCollection(modName) {
            return collectionDialogs.ownerCollectionName(modName) !== ""
        }

        function bundleOwnerName(modName) {
            var bundleId = String(modManager.getModDetails(modName).bundleId || "")
            if (bundleId === "")
                return ""
            var bundles = modManager.modBundles || []
            for (var i = 0; i < bundles.length; ++i) {
                if (bundles[i].id === bundleId)
                    return bundles[i].name || bundles[i].folder || "a bundle"
            }
            return "a bundle"
        }

        function modRowType(modName) {
            var list = modManager.modList || []
            for (var i = 0; i < list.length; ++i) {
                if (list[i].name === modName)
                    return list[i].type || ""
            }
            return ""
        }

        function isModUnavailable(modName) {
            if (addMode === "collection")
                return isModInAnyCollection(modName)
            if (modRowType(modName) !== "folder")
                return true
            return bundleOwnerName(modName) !== ""
        }

        function filteredMods() {
            var list = modManager.modList || []
            var search = addModSearchField.text.toLowerCase()
            if (search === "")
                return list
            return list.filter(function(mod) {
                return mod.name.toLowerCase().indexOf(search) !== -1
            })
        }

        function toggleSelection(modName) {
            if (isModUnavailable(modName))
                return
            var index = selectedModsToAdd.indexOf(modName)
            var next = selectedModsToAdd.slice()
            if (index === -1)
                next.push(modName)
            else
                next.splice(index, 1)
            selectedModsToAdd = next
        }

        function selectAllAvailable() {
            var list = filteredMods()
            var next = selectedModsToAdd.slice()
            for (var i = 0; i < list.length; ++i) {
                var name = list[i].name
                if (!isModUnavailable(name) && next.indexOf(name) === -1)
                    next.push(name)
            }
            selectedModsToAdd = next
        }

        palette {
            windowText: collectionDialogs.isGlass ? modManager.accentColor : window.textColor
            text: collectionDialogs.isGlass ? modManager.accentColor : window.textColor
        }

        background: Rectangle {
            color: collectionDialogs.isGlass
                ? collectionDialogs.glassDialogColor
                : (modManager.theme === "Crimson" ? "transparent" : window.surfaceColor)
            radius: 18
            border.width: collectionDialogs.isGlass ? 1 : 0
            border.color: collectionDialogs.isGlass ? collectionDialogs.glassDialogBorder : "transparent"

            gradient: {
                if (modManager.theme === "Cozy") return sharedCozyDialogGradient
                if (modManager.theme === "Crimson") return sharedCrimsonDialogGradient
                if (modManager.theme === "Wavey") return sharedWaveyDialogGradient
                return null
            }

            layer.enabled: collectionDialogs.isGlass
            layer.effect: DropShadow {
                transparentBorder: true
                radius: 32
                samples: 65
                color: Qt.rgba(0, 0, 0, 0.5)
                horizontalOffset: 0
                verticalOffset: 10
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 34
                    flat: true
                    hoverEnabled: true

                    contentItem: Image {
                        source: "qrc:/UniversalModManager/Lucide Icons/arrow-left.svg"
                        sourceSize.width: 20
                        sourceSize.height: 20
                        fillMode: Image.PreserveAspectFit
                        layer.enabled: true
                        layer.smooth: true
                        layer.textureSize: Qt.size(width * 2, height * 2)
                        layer.effect: ColorOverlay {
                            color: collectionDialogs.isGlass ? "#FFFFFF" : window.textColor
                        }
                    }

                    background: Rectangle {
                        color: parent.hovered
                            ? (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06))
                            : "transparent"
                        radius: 9
                    }

                    onClicked: addModsToCollectionDialog.close()
                }

                Image {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.alignment: Qt.AlignVCenter
                    source: "qrc:/UniversalModManager/Lucide Icons/plus.svg"
                    sourceSize.width: 40
                    sourceSize.height: 40
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    layer.enabled: true
                    layer.smooth: true
                    layer.textureSize: Qt.size(width * 2, height * 2)
                    layer.effect: ColorOverlay { color: window.accentColor }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        text: addModsToCollectionDialog.addMode === "createBundle"
                            ? "Create bundle"
                            : "Add mods"
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        color: collectionDialogs.isGlass ? window.accentColor : window.textColor
                    }

                    Text {
                        text: addModsToCollectionDialog.addMode === "createBundle"
                            ? "Select one or more folder mods"
                            : "to " + (addModsToCollectionDialog.addMode === "bundle"
                                ? addModsToCollectionDialog.targetBundleName
                                : collectionDetailDialog.currentCollectionName)
                        font.pixelSize: 12
                        color: collectionDialogs.dialogSubText
                    }
                }

            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                visible: addModsToCollectionDialog.addMode === "createBundle"
                radius: 10
                color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.05) : window.cardColor1
                border.width: 1
                border.color: newBundleNameField.activeFocus
                    ? window.accentColor
                    : (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.1) : window.borderColor)

                Behavior on border.color { ColorAnimation { duration: 150 } }

                TextField {
                    id: newBundleNameField
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    placeholderText: "Bundle name"
                    placeholderTextColor: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.4) : window.textSecondaryColor
                    color: collectionDialogs.dialogText
                    font.pixelSize: 13
                    selectByMouse: true
                    verticalAlignment: TextInput.AlignVCenter
                    padding: 0
                    background: Rectangle { color: "transparent" }
                    ContextMenu.menu: null
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                radius: 10
                color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.05) : window.cardColor1
                border.width: 1
                border.color: addModSearchField.activeFocus
                    ? window.accentColor
                    : (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.1) : window.borderColor)

                Behavior on border.color { ColorAnimation { duration: 150 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 11
                    anchors.rightMargin: 11
                    spacing: 8

                    Image {
                        source: "qrc:/UniversalModManager/Lucide Icons/search.svg"
                        sourceSize.width: 15
                        sourceSize.height: 15
                        fillMode: Image.PreserveAspectFit
                        layer.enabled: true
                        layer.smooth: true
                        layer.textureSize: Qt.size(width * 2, height * 2)
                        layer.effect: ColorOverlay {
                            color: collectionDialogs.isGlass ? window.accentColor : window.textSecondaryColor
                        }
                    }

                    TextField {
                        id: addModSearchField
                        Layout.fillWidth: true
                        placeholderText: "Search your library…"
                        placeholderTextColor: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.4) : window.textSecondaryColor
                        color: collectionDialogs.dialogText
                        font.pixelSize: 13
                        selectByMouse: true
                        padding: 0
                        background: Rectangle { color: "transparent" }

                        ContextMenu.menu: null

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            cursorShape: Qt.IBeamCursor
                            onPressed: function(mouse) {
                                addModSearchField.forceActiveFocus()
                                addModSearchContextMenu.popup(addModSearchField, mouse.x, mouse.y)
                                mouse.accepted = true
                            }
                            onPressAndHold: function(mouse) {
                                addModSearchField.forceActiveFocus()
                                addModSearchContextMenu.popup(addModSearchField, mouse.x, mouse.y)
                                mouse.accepted = true
                            }
                        }

                        Menu {
                            id: addModSearchContextMenu
                            width: 190
                            padding: 6

                            background: Rectangle {
                                color: collectionDialogs.isGlass ? collectionDialogs.glassMenuColor : window.surfaceColor
                                radius: 12
                                border.width: 1
                                border.color: collectionDialogs.isGlass ? collectionDialogs.glassMenuBorder : window.borderColor

                                layer.enabled: true
                                layer.effect: MultiEffect {
                                    shadowEnabled: true
                                    shadowColor: Qt.rgba(0, 0, 0, 0.4)
                                    shadowHorizontalOffset: 0
                                    shadowVerticalOffset: 8
                                    shadowBlur: 0.75
                                }
                            }

                            ContextMenuItem {
                                text: "Undo"
                                iconName: "undo-2.svg"
                                enabled: addModSearchField.canUndo
                                onTriggered: addModSearchField.undo()
                            }
                            ContextMenuItem {
                                text: "Redo"
                                iconName: "redo-2.svg"
                                enabled: addModSearchField.canRedo
                                onTriggered: addModSearchField.redo()
                            }
                            MenuSeparator {}
                            ContextMenuItem {
                                text: "Cut"
                                iconName: "scissors.svg"
                                enabled: addModSearchField.selectedText.length > 0
                                onTriggered: addModSearchField.cut()
                            }
                            ContextMenuItem {
                                text: "Copy"
                                iconName: "copy.svg"
                                enabled: addModSearchField.selectedText.length > 0
                                onTriggered: addModSearchField.copy()
                            }
                            ContextMenuItem {
                                text: "Paste"
                                iconName: "clipboard.svg"
                                enabled: true
                                onTriggered: addModSearchField.paste()
                            }
                            ContextMenuItem {
                                text: "Delete"
                                iconName: "trash.svg"
                                enabled: addModSearchField.selectedText.length > 0
                                onTriggered: addModSearchField.remove(addModSearchField.selectionStart, addModSearchField.selectionEnd)
                            }
                            MenuSeparator {}
                            ContextMenuItem {
                                text: "Select All"
                                iconName: "text-select.svg"
                                enabled: addModSearchField.text.length > 0
                                onTriggered: addModSearchField.selectAll()
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: collectionDialogs.isGlass ? collectionDialogs.glassPanelColor : window.surfaceColor
                radius: 12
                border.width: 1
                border.color: collectionDialogs.isGlass ? collectionDialogs.glassPanelBorder : window.borderColor

                Item {
                    anchors.fill: parent

                    ListView {
                        id: availableModsList
                        anchors.fill: parent
                        anchors.margins: 8
                        clip: true
                        spacing: 6
                        model: addModsToCollectionDialog.filteredMods()
                        boundsBehavior: Flickable.StopAtBounds
                        flickDeceleration: 1400
                        maximumFlickVelocity: 9000

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            width: 10
                        }

                        delegate: Rectangle {
                            id: addModDelegate
                            width: ListView.view.width
                            height: 54
                            radius: 10
                            color: modHover2.hovered && !isInCollection
                                ? (collectionDialogs.isGlass ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.1) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.06))
                                : (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.045) : window.cardColor1)
                            border.width: 1
                            border.color: isSelected
                                ? window.accentColor
                                : (modHover2.hovered && !isInCollection
                                    ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.45)
                                    : (collectionDialogs.isGlass ? collectionDialogs.glassRowBorder : window.borderColor))
                            opacity: isInCollection ? 0.5 : 1.0
                            clip: true

                            property bool isSelected: addModsToCollectionDialog.selectedModsToAdd.indexOf(modelData.name) !== -1
                            property bool isInCollection: addModsToCollectionDialog.isModUnavailable(modelData.name)
                            property bool isLoose: addModsToCollectionDialog.addMode !== "collection"
                                && addModsToCollectionDialog.modRowType(modelData.name) === "loose"
                            property string ownerName: addModsToCollectionDialog.addMode === "collection"
                                ? collectionDialogs.ownerCollectionName(modelData.name)
                                : addModsToCollectionDialog.bundleOwnerName(modelData.name)

                            Behavior on border.color { ColorAnimation { duration: 150 } }
                            Behavior on opacity { NumberAnimation { duration: 150 } }

                            HoverHandler { id: modHover2 }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 11

                                Rectangle {
                                    Layout.preferredWidth: 19
                                    Layout.preferredHeight: 19
                                    radius: 5
                                    color: addModDelegate.isSelected ? window.accentColor : "transparent"
                                    border.width: 1
                                    border.color: addModDelegate.isSelected
                                        ? window.accentColor
                                        : (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.28) : window.borderColor)

                                    Image {
                                        anchors.centerIn: parent
                                        visible: addModDelegate.isSelected
                                        source: "qrc:/UniversalModManager/Lucide Icons/check.svg"
                                        sourceSize.width: 12
                                        sourceSize.height: 12
                                        fillMode: Image.PreserveAspectFit
                                        layer.enabled: true
                                        layer.smooth: true
                                        layer.textureSize: Qt.size(width * 2, height * 2)
                                        layer.effect: ColorOverlay { color: "#241633" }
                                    }
                                }

                                Image {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    Layout.alignment: Qt.AlignVCenter
                                    source: "qrc:/UniversalModManager/Lucide Icons/package.svg"
                                    sourceSize.width: 32
                                    sourceSize.height: 32
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    layer.enabled: true
                                    layer.smooth: true
                                    layer.textureSize: Qt.size(width * 2, height * 2)
                                    layer.effect: ColorOverlay { color: window.accentColor }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Text {
                                        text: modelData.name
                                        font.pixelSize: 13
                                        font.weight: Font.Medium
                                        color: collectionDialogs.isGlass ? "#FFFFFF" : window.textColor
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: modelData.fileCount + (modelData.fileCount === 1 ? " file · " : " files · ")
                                            + (modManager.getModDetails(modelData.name).size || "N/A")
                                        font.pixelSize: 11
                                        color: collectionDialogs.dialogSubText
                                    }
                                }

                                Text {
                                    text: addModDelegate.isLoose
                                        ? "Loose file"
                                        : (addModDelegate.isInCollection
                                            ? (addModDelegate.ownerName !== ""
                                                ? "In " + addModDelegate.ownerName
                                                : (addModsToCollectionDialog.addMode === "collection"
                                                    ? "In a collection" : "In a bundle"))
                                            : "Available")
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    color: addModDelegate.isInCollection ? collectionDialogs.dialogSubText : window.accentColor
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: addModDelegate.isInCollection ? Qt.ForbiddenCursor : Qt.PointingHandCursor
                                enabled: !addModDelegate.isInCollection
                                onClicked: addModsToCollectionDialog.toggleSelection(modelData.name)
                            }
                        }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 60, 420)
                        spacing: 8
                        visible: availableModsList.count === 0

                        Image {
                            Layout.alignment: Qt.AlignHCenter
                            width: 24
                            height: 24
                            source: "qrc:/UniversalModManager/Lucide Icons/package.svg"
                            sourceSize.width: 48
                            sourceSize.height: 48
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            layer.enabled: true
                            layer.smooth: true
                            layer.textureSize: Qt.size(width * 2, height * 2)
                            layer.effect: ColorOverlay { color: window.accentColor }
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: addModSearchField.text.trim() === "" ? "No mods available" : "No mods match your search"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: collectionDialogs.dialogText
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            width: parent.width
                            text: addModSearchField.text.trim() === ""
                                ? (addModsToCollectionDialog.addMode === "createBundle"
                                    ? "Install at least one folder mod first, then bundle it."
                                    : (addModsToCollectionDialog.addMode === "bundle"
                                        ? "Install mods first, then add them to this bundle."
                                        : "Install mods first, then add them to this collection."))
                                : "Try a different name."
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            font.pixelSize: 12
                            color: collectionDialogs.dialogSubText
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    text: addModsToCollectionDialog.selectedModsToAdd.length === 0
                        ? "0 selected"
                        : (addModsToCollectionDialog.selectedModsToAdd.length === 1
                            ? "1 selected"
                            : addModsToCollectionDialog.selectedModsToAdd.length + " selected")
                    font.pixelSize: 12
                    color: collectionDialogs.dialogSubText
                }

                Button {
                    Layout.preferredHeight: 28
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: "Select all"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: window.accentColor
                    }

                    background: Rectangle {
                        color: parent.hovered ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.1) : "transparent"
                        radius: 7
                    }

                    onClicked: addModsToCollectionDialog.selectAllAvailable()
                }

                Button {
                    Layout.preferredHeight: 28
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: "Clear"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: collectionDialogs.dialogSubText
                    }

                    background: Rectangle {
                        color: parent.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                        radius: 7
                    }

                    onClicked: addModsToCollectionDialog.selectedModsToAdd = []
                }

                Item { Layout.fillWidth: true }

                Button {
                    Layout.preferredWidth: 110
                    Layout.preferredHeight: 38
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: "Cancel"
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.82) : window.textSecondaryColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: collectionDialogs.isGlass ? collectionDialogs.glassGhostColor : "transparent"
                        radius: 10
                        border.width: collectionDialogs.isGlass ? 1 : 0
                        border.color: collectionDialogs.isGlass ? collectionDialogs.glassGhostBorder : "transparent"
                    }

                    onClicked: addModsToCollectionDialog.close()
                }

                Button {
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 38
                    flat: true
                    hoverEnabled: true
                    enabled: addModsToCollectionDialog.selectedModsToAdd.length > 0

                    contentItem: Text {
                        text: addModsToCollectionDialog.addMode === "createBundle"
                            ? (addModsToCollectionDialog.selectedModsToAdd.length > 0
                                ? "Create bundle (" + addModsToCollectionDialog.selectedModsToAdd.length + ")"
                                : "Create bundle")
                            : (addModsToCollectionDialog.selectedModsToAdd.length > 0
                                ? (addModsToCollectionDialog.selectedModsToAdd.length === 1
                                    ? "Add 1 mod"
                                    : "Add " + addModsToCollectionDialog.selectedModsToAdd.length + " mods")
                                : "Add selected")
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: parent.enabled ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.4)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: parent.enabled
                            ? window.accentColor
                            : Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.3)
                        radius: 10
                        opacity: parent.hovered && parent.enabled ? 0.9 : 1.0
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }

                    onClicked: {
                        var selected = addModsToCollectionDialog.selectedModsToAdd.slice()
                        var added = selected.length
                        if (addModsToCollectionDialog.addMode === "createBundle") {
                            var bundleName = newBundleNameField.text.trim()
                            if (bundleName === "") {
                                toastNotification.show("Enter a bundle name", 6000)
                                return
                            }
                            if (selected.length < 1) {
                                toastNotification.show("Select at least one mod", 6000)
                                return
                            }
                            var createError = modManager.groupModsIntoBundle(selected, bundleName)
                            if (createError && createError.length > 0) {
                                toastNotification.show(createError, 6000)
                                return
                            }
                            addModsToCollectionDialog.selectedModsToAdd = []
                            addModSearchField.text = ""
                            newBundleNameField.text = ""
                            addModsToCollectionDialog.close()
                            toastNotification.show("Bundle created", "check.svg")
                            return
                        } else if (addModsToCollectionDialog.addMode === "bundle") {
                            var error = modManager.addModsToBundle(
                                addModsToCollectionDialog.targetBundleId, selected)
                            if (error && error.length > 0) {
                                toastNotification.show(error, 6000)
                                return
                            }
                        } else {
                            for (var i = 0; i < selected.length; i++) {
                                modManager.addModToCollection(collectionDetailDialog.currentCollectionId,
                                                              selected[i])
                            }
                            collectionDetailDialog.loadCollectionMods()
                            collectionDialogs.refreshCollections()
                        }
                        addModsToCollectionDialog.selectedModsToAdd = []
                        addModSearchField.text = ""
                        addModsToCollectionDialog.close()
                        toastNotification.show(added + (added === 1 ? " mod added" : " mods added"), "check.svg")
                    }
                }
            }
        }

        onOpened: {
            selectedModsToAdd = []
            addModSearchField.text = ""
            newBundleNameField.text = ""
        }
    }

    Dialog {
        id: newCollectionDialog
        modal: true
        width: 420
        height: 220
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        padding: 22
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        palette {
            windowText: collectionDialogs.isGlass ? modManager.accentColor : window.textColor
            text: collectionDialogs.isGlass ? modManager.accentColor : window.textColor
        }

        background: Rectangle {
            color: collectionDialogs.isGlass
                ? collectionDialogs.glassDialogColor
                : (modManager.theme === "Crimson" ? "transparent" : window.surfaceColor)
            radius: 16
            border.width: collectionDialogs.isGlass ? 1 : 0
            border.color: collectionDialogs.isGlass ? collectionDialogs.glassDialogBorder : "transparent"

            gradient: {
                if (modManager.theme === "Cozy") return sharedCozyDialogGradient
                if (modManager.theme === "Crimson") return sharedCrimsonDialogGradient
                if (modManager.theme === "Wavey") return sharedWaveyDialogGradient
                return null
            }

            layer.enabled: collectionDialogs.isGlass
            layer.effect: DropShadow {
                transparentBorder: true
                radius: 32
                samples: 65
                color: Qt.rgba(0, 0, 0, 0.5)
                horizontalOffset: 0
                verticalOffset: 10
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Image {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    Layout.alignment: Qt.AlignVCenter
                    source: "qrc:/UniversalModManager/Lucide Icons/plus.svg"
                    sourceSize.width: 36
                    sourceSize.height: 36
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    layer.enabled: true
                    layer.smooth: true
                    layer.textureSize: Qt.size(width * 2, height * 2)
                    layer.effect: ColorOverlay { color: window.accentColor }
                }

                Text {
                    Layout.fillWidth: true
                    text: "New collection"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: collectionDialogs.isGlass ? window.accentColor : window.textColor
                }
            }

            Rectangle {
                id: collectionNameContainer
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.06) : window.cardColor1
                radius: 10
                border.width: collectionNameInput.activeFocus ? 2 : 1
                border.color: collectionNameInput.activeFocus
                    ? window.accentColor
                    : (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.14) : window.borderColor)

                Behavior on border.color { ColorAnimation { duration: 150 } }
                Behavior on border.width { NumberAnimation { duration: 150 } }

                TextField {
                    id: collectionNameInput
                    anchors.fill: parent
                    anchors.margins: 11
                    leftPadding: 6
                    rightPadding: 6
                    topPadding: 0
                    bottomPadding: 0
                    verticalAlignment: TextInput.AlignVCenter
                    placeholderText: "Collection name…"
                    placeholderTextColor: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.45) : window.textSecondaryColor
                    color: collectionDialogs.isGlass ? "#FFFFFF" : window.textColor
                    font.pixelSize: 13
                    selectByMouse: true
                    persistentSelection: true
                    background: Rectangle { color: "transparent" }

                    ContextMenu.menu: null

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        cursorShape: Qt.IBeamCursor
                        onPressed: function(mouse) {
                            collectionNameInput.forceActiveFocus()
                            collectionNameContextMenu.popup(collectionNameInput, mouse.x, mouse.y)
                            mouse.accepted = true
                        }
                        onPressAndHold: function(mouse) {
                            collectionNameInput.forceActiveFocus()
                            collectionNameContextMenu.popup(collectionNameInput, mouse.x, mouse.y)
                            mouse.accepted = true
                        }
                    }

                    Menu {
                        id: collectionNameContextMenu
                        width: 180
                        padding: 6

                        background: Rectangle {
                            color: collectionDialogs.isGlass ? collectionDialogs.glassMenuColor : window.surfaceColor
                            radius: 12
                            border.width: 1
                            border.color: collectionDialogs.isGlass ? collectionDialogs.glassMenuBorder : window.borderColor
                        }

                        ContextMenuItem { text: "Undo"; iconName: "undo-2.svg"; enabled: collectionNameInput.canUndo; onTriggered: collectionNameInput.undo() }
                        ContextMenuItem { text: "Redo"; iconName: "redo-2.svg"; enabled: collectionNameInput.canRedo; onTriggered: collectionNameInput.redo() }
                        MenuSeparator {}
                        ContextMenuItem { text: "Cut"; iconName: "scissors.svg"; enabled: collectionNameInput.selectedText.length > 0; onTriggered: collectionNameInput.cut() }
                        ContextMenuItem { text: "Copy"; iconName: "copy.svg"; enabled: collectionNameInput.selectedText.length > 0; onTriggered: collectionNameInput.copy() }
                        ContextMenuItem { text: "Paste"; iconName: "clipboard.svg"; enabled: true; onTriggered: collectionNameInput.paste() }
                        ContextMenuItem { text: "Delete"; iconName: "trash.svg"; enabled: collectionNameInput.selectedText.length > 0; onTriggered: collectionNameInput.remove(collectionNameInput.selectionStart, collectionNameInput.selectionEnd) }
                        MenuSeparator {}
                        ContextMenuItem { text: "Select All"; iconName: "text-select.svg"; enabled: collectionNameInput.text.length > 0; onTriggered: collectionNameInput.selectAll() }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: "Cancel"
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.82) : window.textSecondaryColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: collectionDialogs.isGlass ? collectionDialogs.glassGhostColor : "transparent"
                        radius: 10
                        border.width: collectionDialogs.isGlass ? 1 : 0
                        border.color: collectionDialogs.isGlass ? collectionDialogs.glassGhostBorder : "transparent"
                    }

                    onClicked: {
                        collectionNameInput.text = ""
                        newCollectionDialog.close()
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    flat: true
                    hoverEnabled: true
                    enabled: collectionNameInput.text.trim() !== ""

                    contentItem: Text {
                        text: "Create"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: parent.enabled ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.4)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: parent.enabled
                            ? window.accentColor
                            : Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.3)
                        radius: 10
                        opacity: parent.hovered && parent.enabled ? 0.9 : 1.0
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }

                    onClicked: {
                        if (collectionNameInput.text.trim() !== "") {
                            modManager.createCollection(collectionNameInput.text.trim(), "")
                            collectionNameInput.text = ""
                            collectionDialogs.refreshCollections()
                            newCollectionDialog.close()
                            toastNotification.show("Collection created", "check.svg")
                        }
                    }
                }
            }
        }

        onOpened: {
            collectionNameInput.text = ""
            collectionNameInput.forceActiveFocus()
        }
    }

    Dialog {
        id: renameCollectionDialog
        modal: true
        width: 420
        height: 220
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        padding: 22
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property string collectionId: ""
        property string currentName: ""

        palette {
            windowText: collectionDialogs.isGlass ? modManager.accentColor : window.textColor
            text: collectionDialogs.isGlass ? modManager.accentColor : window.textColor
        }

        background: Rectangle {
            color: collectionDialogs.isGlass
                ? collectionDialogs.glassDialogColor
                : (modManager.theme === "Crimson" ? "transparent" : window.surfaceColor)
            radius: 16
            border.width: collectionDialogs.isGlass ? 1 : 0
            border.color: collectionDialogs.isGlass ? collectionDialogs.glassDialogBorder : "transparent"

            gradient: {
                if (modManager.theme === "Cozy") return sharedCozyDialogGradient
                if (modManager.theme === "Crimson") return sharedCrimsonDialogGradient
                if (modManager.theme === "Wavey") return sharedWaveyDialogGradient
                return null
            }

            layer.enabled: collectionDialogs.isGlass
            layer.effect: DropShadow {
                transparentBorder: true
                radius: 32
                samples: 65
                color: Qt.rgba(0, 0, 0, 0.5)
                horizontalOffset: 0
                verticalOffset: 10
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Image {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    Layout.alignment: Qt.AlignVCenter
                    source: "qrc:/UniversalModManager/Lucide Icons/pencil.svg"
                    sourceSize.width: 36
                    sourceSize.height: 36
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    layer.enabled: true
                    layer.smooth: true
                    layer.textureSize: Qt.size(width * 2, height * 2)
                    layer.effect: ColorOverlay { color: window.accentColor }
                }

                Text {
                    Layout.fillWidth: true
                    text: "Rename collection"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: collectionDialogs.isGlass ? window.accentColor : window.textColor
                }
            }

            Rectangle {
                id: renameCollectionContainer
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.06) : window.cardColor1
                radius: 10
                border.width: renameCollectionInput.activeFocus ? 2 : 1
                border.color: renameCollectionInput.activeFocus
                    ? window.accentColor
                    : (collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.14) : window.borderColor)

                Behavior on border.color { ColorAnimation { duration: 150 } }
                Behavior on border.width { NumberAnimation { duration: 150 } }

                TextField {
                    id: renameCollectionInput
                    anchors.fill: parent
                    anchors.margins: 11
                    leftPadding: 6
                    rightPadding: 6
                    topPadding: 0
                    bottomPadding: 0
                    verticalAlignment: TextInput.AlignVCenter
                    placeholderText: "Collection name…"
                    placeholderTextColor: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.45) : window.textSecondaryColor
                    color: collectionDialogs.isGlass ? "#FFFFFF" : window.textColor
                    font.pixelSize: 13
                    selectByMouse: true
                    persistentSelection: true
                    background: Rectangle { color: "transparent" }

                    ContextMenu.menu: null

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        cursorShape: Qt.IBeamCursor
                        onPressed: function(mouse) {
                            renameCollectionInput.forceActiveFocus()
                            renameCollectionContextMenu.popup(renameCollectionInput, mouse.x, mouse.y)
                            mouse.accepted = true
                        }
                        onPressAndHold: function(mouse) {
                            renameCollectionInput.forceActiveFocus()
                            renameCollectionContextMenu.popup(renameCollectionInput, mouse.x, mouse.y)
                            mouse.accepted = true
                        }
                    }

                    Menu {
                        id: renameCollectionContextMenu
                        width: 180
                        padding: 6

                        background: Rectangle {
                            color: collectionDialogs.isGlass ? collectionDialogs.glassMenuColor : window.surfaceColor
                            radius: 12
                            border.width: 1
                            border.color: collectionDialogs.isGlass ? collectionDialogs.glassMenuBorder : window.borderColor
                        }

                        ContextMenuItem { text: "Undo"; iconName: "undo-2.svg"; enabled: renameCollectionInput.canUndo; onTriggered: renameCollectionInput.undo() }
                        ContextMenuItem { text: "Redo"; iconName: "redo-2.svg"; enabled: renameCollectionInput.canRedo; onTriggered: renameCollectionInput.redo() }
                        MenuSeparator {}
                        ContextMenuItem { text: "Cut"; iconName: "scissors.svg"; enabled: renameCollectionInput.selectedText.length > 0; onTriggered: renameCollectionInput.cut() }
                        ContextMenuItem { text: "Copy"; iconName: "copy.svg"; enabled: renameCollectionInput.selectedText.length > 0; onTriggered: renameCollectionInput.copy() }
                        ContextMenuItem { text: "Paste"; iconName: "clipboard.svg"; enabled: true; onTriggered: renameCollectionInput.paste() }
                        ContextMenuItem { text: "Delete"; iconName: "trash.svg"; enabled: renameCollectionInput.selectedText.length > 0; onTriggered: renameCollectionInput.remove(renameCollectionInput.selectionStart, renameCollectionInput.selectionEnd) }
                        MenuSeparator {}
                        ContextMenuItem { text: "Select All"; iconName: "text-select.svg"; enabled: renameCollectionInput.text.length > 0; onTriggered: renameCollectionInput.selectAll() }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: "Cancel"
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: collectionDialogs.isGlass ? Qt.rgba(1, 1, 1, 0.82) : window.textSecondaryColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: collectionDialogs.isGlass ? collectionDialogs.glassGhostColor : "transparent"
                        radius: 10
                        border.width: collectionDialogs.isGlass ? 1 : 0
                        border.color: collectionDialogs.isGlass ? collectionDialogs.glassGhostBorder : "transparent"
                    }

                    onClicked: renameCollectionDialog.close()
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    flat: true
                    hoverEnabled: true
                    enabled: renameCollectionInput.text.trim() !== ""
                        && renameCollectionInput.text.trim() !== renameCollectionDialog.currentName

                    contentItem: Text {
                        text: "Save"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: parent.enabled ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.4)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: parent.enabled
                            ? window.accentColor
                            : Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.3)
                        radius: 10
                        opacity: parent.hovered && parent.enabled ? 0.9 : 1.0
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }

                    onClicked: {
                        if (renameCollectionInput.text.trim() !== "") {
                            modManager.renameCollection(renameCollectionDialog.collectionId,
                                                        renameCollectionInput.text.trim())
                            collectionDialogs.refreshCollections()
                            collectionDetailDialog.loadCollectionMods()
                            renameCollectionDialog.close()
                            toastNotification.show("Collection renamed", "check.svg")
                        }
                    }
                }
            }
        }

        onOpened: {
            renameCollectionInput.text = renameCollectionDialog.currentName
            renameCollectionInput.forceActiveFocus()
            renameCollectionInput.selectAll()
        }
    }

    FileDialog {
        id: collectionPreviewFileDialog
        title: "Select Preview Image"
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.gif *.webp *.bmp)"]

        property string collectionId: ""

        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "")
            modManager.setCollectionPreview(collectionId, path)
            collectionDialogs.refreshCollections()
            collectionDetailDialog.loadCollectionMods()
            toastNotification.show("Preview added", "check.svg")
        }
    }
}
