import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Rectangle {
    id: delegateRoot
    property string modName: "Unknown Mod"
    property string uniqueId: ""
    property int fileCount: 0
    property string modType: "folder"
    property string modExtensions: ""
    property bool modEnabled: true
    readonly property int previewInset: 2

    readonly property string imageKey: (modType === "loose" && uniqueId !== "") ? uniqueId : modName
    readonly property var nexusUpdate: {
        var revision = modManager.nexusUpdatesRevision
        return modManager.nexusUpdateForMod(modName)
    }

    width: GridView.view ? GridView.view.width - 20 : (parent ? parent.width - 20 : 600)
    height: 256
    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
    radius: 12
    border.width: modManager.theme === "Glassmorphic" ? 2 : (cardHoverHandler.hovered ? 2 : 1)
    border.color: modManager.theme === "Glassmorphic"
        ? (cardHoverHandler.hovered
            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.8)
            : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.5))
        : (cardHoverHandler.hovered ? window.primaryColor : window.borderColor)

    gradient: modManager.theme === "Crimson" ? modCardGradient : null

    Gradient {
        id: modCardGradient
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: window.crimsonGradientTop }
        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
    }

    property real shadowIntensity: 0.0

    Behavior on shadowIntensity {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    clip: false

    layer.enabled: true
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: Qt.rgba(0, 0, 0, 0.25 + shadowIntensity * 0.15)
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 4 + shadowIntensity * 4
        shadowBlur: 1.0 + shadowIntensity * 0.5
    }

    Behavior on border.color { ColorAnimation { duration: 150 } }
    Behavior on border.width { NumberAnimation { duration: 150 } }

    HoverHandler {
        id: cardHoverHandler
        onHoveredChanged: {
            if (hovered) {
                delegateRoot.shadowIntensity = 1.0
                setSelectedModContext(modName, imageKey)
                hoveredModName = modName
            } else {
                delegateRoot.shadowIntensity = 0.0
                hoveredModName = ""
                if (!modDetailsPanel.visible) {
                    clearSelectedModContext()
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 16

        Item {
            id: previewArea
            Layout.preferredWidth: 382
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignVCenter
            clip: true

            Rectangle {
                anchors.fill: parent
                radius: 8
                color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.2) : window.cardColor1
                border.width: 1
                border.color: window.borderColor
                clip: true

                Image {
                    id: cardModImageBlur
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    source: (pageStack.currentIndex === 1) ? cardModImage.source : ""
                    visible: cardModImage.source != ""

                    asynchronous: true
                    cache: false
                    smooth: true
                    sourceSize: Qt.size(Math.ceil(parent.width * 2), Math.ceil(parent.height * 2))

                    layer.enabled: true
                    layer.effect: MultiEffect {
                        blurEnabled: true
                        blur: 1.0
                        blurMax: 64
                        blurMultiplier: 1.0
                        saturation: 0.3
                        brightness: -0.3
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(0, 0, 0, 0.4)
                    visible: cardModImage.source != ""
                }

                Image {
                    id: cardModImage
                    anchors.fill: parent

                    fillMode: Image.PreserveAspectFit
                    verticalAlignment: Image.AlignVCenter
                    horizontalAlignment: Image.AlignHCenter
                    source: {
                        if (pageStack.currentIndex !== 1) return ""
                        var trigger = modLibraryRoot.imageRefreshTrigger
                        var savedPath = modManager.getModImage(imageKey)
                        if (!savedPath || savedPath.toLowerCase().endsWith(".gif")) return ""
                        var separator = savedPath.indexOf("?") === -1 ? "?" : "&"
                        return savedPath + separator + "v=" + trigger
                    }
                    visible: source != ""

                    asynchronous: true
                    cache: false
                    smooth: true
                    mipmap: true
                }

                AnimatedImage {
                    id: cardModGif
                    anchors.fill: parent

                    fillMode: Image.PreserveAspectFit
                    source: {
                        if (pageStack.currentIndex !== 1) return ""
                        var trigger = modLibraryRoot.imageRefreshTrigger
                        var savedPath = modManager.getModImage(imageKey)
                        if (!savedPath || !savedPath.toLowerCase().endsWith(".gif")) return ""
                        var separator = savedPath.indexOf("?") === -1 ? "?" : "&"
                        return savedPath + separator + "v=" + trigger
                    }
                    visible: source != ""
                    playing: visible

                    asynchronous: true
                    cache: false
                    smooth: true
                }

                Item {
                    anchors.centerIn: parent
                    width: 48
                    height: 48
                    visible: cardModImage.source == "" && cardModGif.source == ""

                    Image {
                        anchors.fill: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/" + (modType === "loose" ? "file.svg" : "package.svg")
                        sourceSize.width: 96
                        sourceSize.height: 96
                        fillMode: Image.PreserveAspectFit

                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: modType === "loose" ? window.accentColor : window.primaryColor
                        }
                        opacity: 0.4
                    }
                }

                Item {
                    id: favoriteStar
                    width: 28
                    height: 28
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 4
                    anchors.rightMargin: 4

                    property bool isFavorite: {
                        var t = modLibraryRoot.favoriteRefreshTrigger
                        return modManager.isModFavorite(imageKey)
                    }
                    readonly property string themeFolder:
                        "qrc:/UniversalModManager/Icons/" + modManager.theme + " Theme/"

                    Image {
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        source: favoriteStar.themeFolder + "heart.svg"
                        sourceSize: Qt.size(18, 18)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        opacity: favoriteStar.isFavorite ? 0.0 : 1.0
                        layer.enabled: true
                        layer.smooth: true
                        layer.textureSize: Qt.size(width * 2, height * 2)
                        layer.effect: ColorOverlay {
                            color: window.accentColor
                        }
                        Behavior on opacity { NumberAnimation { duration: 120 } }
                    }

                    Image {
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        source: favoriteStar.themeFolder + "heart-filled.svg"
                        sourceSize: Qt.size(18, 18)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        opacity: favoriteStar.isFavorite ? 1.0 : 0.0
                        layer.enabled: true
                        layer.smooth: true
                        layer.textureSize: Qt.size(width * 2, height * 2)
                        layer.effect: ColorOverlay {
                            color: window.accentColor
                        }
                        Behavior on opacity { NumberAnimation { duration: 120 } }
                    }

                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            var currentlyFav = favoriteStar.isFavorite
                            modManager.setModFavorite(imageKey, !currentlyFav)
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: modName
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: window.textColor
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                RowLayout {
                    spacing: 6

                    Rectangle {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.2) : window.cardColor1
                        radius: 4

                        Image {
                            anchors.centerIn: parent
                            source: "qrc:/UniversalModManager/Lucide Icons/" + (modType === "loose" ? "file.svg" : "package.svg")
                            sourceSize.width: 16
                            sourceSize.height: 16
                            fillMode: Image.PreserveAspectFit
                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: modType === "loose" ? window.accentColor : window.primaryColor
                            }
                        }
                    }

                    Text {
                        text: fileCount + (fileCount === 1 ? " file" : " files")
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: window.textColor
                    }
                }

                Text {
                    text: "•"
                    font.pixelSize: 13
                    color: window.textSecondaryColor
                    opacity: 0.5
                }

                Text {
                    text: modManager.getModDetails(modName).size || "N/A"
                    font.pixelSize: 13
                    color: window.textSecondaryColor
                }

                Text {
                    text: "•"
                    font.pixelSize: 13
                    color: window.textSecondaryColor
                    opacity: 0.5
                }

                Text {
                    text: modManager.getModDetails(modName).installDate || "N/A"
                    font.pixelSize: 13
                    color: window.textSecondaryColor
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.preferredWidth: typeRow.implicitWidth + 14
                    Layout.preferredHeight: 26
                    radius: 6
                    color: {
                        return modType === "loose"
                                ? modManager.looseFileColor
                                : modManager.folderModColor
                    }

                    Row {
                        id: typeRow
                        anchors.centerIn: parent
                        spacing: 5

                        Image {
                            source: "qrc:/UniversalModManager/Lucide Icons/" + (modType === "loose" ? "file.svg" : "package.svg")
                            sourceSize.width: 14
                            sourceSize.height: 14
                            fillMode: Image.PreserveAspectFit
                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: "#FFFFFF"
                            }
                        }

                        Text {
                            text: modType === "loose" ? "Loose" : "Folder"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            color: "#FFFFFF"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                Rectangle {
                    visible: modExtensions !== ""
                    Layout.preferredWidth: extText.contentWidth + 14
                    Layout.preferredHeight: 26
                    radius: 6

                    property string extUpper: modExtensions.toUpperCase()
                    property color extColor: {
                        if (extUpper.indexOf("PAK") !== -1)  return modManager.pakColor
                        if (extUpper.indexOf("UCAS") !== -1) return modManager.ucasColor
                        if (extUpper.indexOf("UTOC") !== -1) return modManager.utocColor
                        return window.primaryColor
                    }

                    color: Qt.rgba(extColor.r, extColor.g, extColor.b, 0.25)
                    border.width: 1.5
                    border.color: extColor

                    Text {
                        id: extText
                        anchors.centerIn: parent
                        text: modExtensions.toUpperCase()
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        color: parent.extColor
                    }
                }

                Item {
                    id: idBadgeWrapper
                    Layout.preferredWidth: idBadgeRect.width
                    Layout.preferredHeight: 26
                    visible: modIdText.text !== ""

                    property color idColorObj: modManager.idColor
                    property bool isHovered: idBadgeHoverHandler.hovered

                    Rectangle {
                        id: idBadgeRect
                        anchors.centerIn: parent
                        width: modIdText.contentWidth + 14
                        height: 26
                        radius: 5
                        color: Qt.rgba(idBadgeWrapper.idColorObj.r, idBadgeWrapper.idColorObj.g, idBadgeWrapper.idColorObj.b,
                                      idBadgeWrapper.isHovered ? 0.25 : 0.15)
                        border.width: 1.5
                        border.color: Qt.rgba(idBadgeWrapper.idColorObj.r, idBadgeWrapper.idColorObj.g, idBadgeWrapper.idColorObj.b,
                                             idBadgeWrapper.isHovered ? 0.5 : 0.3)

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        Text {
                            id: modIdText
                            anchors.centerIn: parent
                            text: {
                                var modId = modManager.getModDetails(modName).modId || ""
                                return modId ? "#" + modId : ""
                            }
                            font.family: "JetBrains Mono"
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            color: idBadgeWrapper.idColorObj
                        }
                    }

                    HoverHandler {
                        id: idBadgeHoverHandler
                        cursorShape: Qt.PointingHandCursor
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            var modId = modManager.getModDetails(modName).modId || ""
                            if (modId) {
                                try {
                                    var tempInput = Qt.createQmlObject('import QtQuick; TextInput { }', parent)
                                    tempInput.text = modId
                                    tempInput.selectAll()
                                    tempInput.copy()
                                    tempInput.destroy()
                                    toastNotification.show("ID copied", "copy.svg")
                                } catch(e) {
                                    toastNotification.show("ID: " + modId, "copy.svg")
                                }
                            }
                        }
                    }
                }

                UpdateAvailableBadge {
                    Layout.preferredHeight: 26
                    visible: delegateRoot.nexusUpdate.available === true
                    tooltipText: "Click to download version " + (delegateRoot.nexusUpdate.latestVersion || "update")
                    onClicked: modManager.downloadNexusMod(delegateRoot.nexusUpdate.modId,
                                                            delegateRoot.nexusUpdate.latestFileId,
                                                            delegateRoot.nexusUpdate.name)
                }
            }

            Flow {
                id: tagsFlow
                Layout.fillWidth: true
                spacing: 6
                property var modTags: modManager.getModTags(imageKey)
                visible: modTags.length > 0
                property int refreshTrigger: tagRefreshTrigger
                property color tagColorObj: modManager.tagColor

                onRefreshTriggerChanged: {
                    modTags = modManager.getModTags(imageKey)
                }

                Repeater {
                    model: Math.min(4, tagsFlow.modTags.length)

                    Rectangle {
                        id: tagBadge
                        width: cardTagText.contentWidth + 24
                        height: 30
                        radius: 10

                        gradient: Gradient {
                            GradientStop {
                                position: 0.0
                                color: Qt.rgba(tagsFlow.tagColorObj.r, tagsFlow.tagColorObj.g, tagsFlow.tagColorObj.b, 0.85)
                            }
                            GradientStop {
                                position: 1.0
                                color: Qt.rgba(tagsFlow.tagColorObj.r * 0.8, tagsFlow.tagColorObj.g * 0.8, tagsFlow.tagColorObj.b * 0.8, 0.85)
                            }
                        }

                        border.width: 1.5
                        border.color: Qt.rgba(tagsFlow.tagColorObj.r, tagsFlow.tagColorObj.g, tagsFlow.tagColorObj.b, 0.4)

                        layer.enabled: true
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowColor: Qt.rgba(0, 0, 0, 0.25)
                            shadowHorizontalOffset: 0
                            shadowVerticalOffset: 2
                            shadowBlur: 0.3
                        }

                        Text {
                            id: cardTagText
                            anchors.centerIn: parent
                            text: tagsFlow.modTags[index]
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            color: "#FFFFFF"
                            font.letterSpacing: 0.3

                            style: Text.Raised
                            styleColor: Qt.rgba(0, 0, 0, 0.3)
                        }
                    }
                }

                Rectangle {
                    visible: tagsFlow.modTags && tagsFlow.modTags.length > 4
                    width: moreTagsText.contentWidth + 20
                    height: 30
                    radius: 8
                    color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.15) :
                           (modManager.theme === "Crimson" ? Qt.rgba(0.5, 0.5, 0.5, 0.2) :
                           (modManager.theme === "Wavey" ? Qt.rgba(0.59, 0.65, 0.98, 0.2) :
                           Qt.rgba(0, 0, 0, 0.1)))
                    border.width: 1
                    border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25) :
                                 (modManager.theme === "Crimson" ? Qt.rgba(0.5, 0.5, 0.5, 0.3) :
                                 (modManager.theme === "Wavey" ? Qt.rgba(0.59, 0.65, 0.98, 0.3) :
                                 Qt.rgba(0, 0, 0, 0.15)))

                    Text {
                        id: moreTagsText
                        anchors.centerIn: parent
                        text: tagsFlow.modTags ? "+" + (tagsFlow.modTags.length - 4) + " more" : ""
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: window.textSecondaryColor
                        opacity: 0.8
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 8

                Rectangle {
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 36
                    radius: 8

                    property color enableColor: modManager.enableButtonColor
                    property color disableColor: modManager.disableButtonColor
                    property bool isHovered: toggleHoverHandler.hovered

                    color: {
                        var baseColor = modEnabled ? disableColor : enableColor
                        return isHovered ? Qt.lighter(baseColor, 1.15) : baseColor
                    }
                    border.width: 0

                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: Qt.rgba(0, 0, 0, 0.2)
                        shadowHorizontalOffset: 0
                        shadowVerticalOffset: 2
                        shadowBlur: 0.4
                    }

                    Behavior on color { ColorAnimation { duration: 150 } }

                    scale: isHovered ? 1.05 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                    Text {
                        anchors.centerIn: parent
                        text: modEnabled ? "Disable" : "Enable"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: "#FFFFFF"
                    }

                    HoverHandler {
                        id: toggleHoverHandler
                        cursorShape: Qt.PointingHandCursor
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            var wasEnabled = modEnabled
                            modManager.toggleModStatus(modName)

                            if (wasEnabled) {
                                toastNotification.show("Mod disabled", "pause.svg")
                            } else {
                                toastNotification.show("Mod enabled", "play.svg")
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 90
                    Layout.preferredHeight: 32
                    radius: 8
                    color: folderHoverHandler.hovered ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12) :
                           (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08) : Qt.rgba(window.cardColor1.r, window.cardColor1.g, window.cardColor1.b, 0.5))
                    border.width: 1
                    border.color: Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.3)
                    opacity: folderHoverHandler.hovered ? 1.0 : 0.85

                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on opacity { NumberAnimation { duration: 150 } }

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 5

                        Image {
                            source: "qrc:/UniversalModManager/Lucide Icons/folder-open.svg"
                            sourceSize.width: 14
                            sourceSize.height: 14
                            fillMode: Image.PreserveAspectFit
                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: window.textColor
                            }
                            opacity: 0.8
                        }

                        Text {
                            text: "Folder"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            color: window.textColor
                            opacity: 0.8
                        }
                    }

                    HoverHandler {
                        id: folderHoverHandler
                        cursorShape: Qt.PointingHandCursor
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            modManager.openModFolder(modName)
                        }
                    }
                }

                Rectangle {
                    visible: modLibraryRoot.openedBundleId !== ""
                    Layout.preferredWidth: 94
                    Layout.preferredHeight: 32
                    radius: 8
                    color: removeHoverHandler.hovered ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12) :
                           (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08) : Qt.rgba(window.cardColor1.r, window.cardColor1.g, window.cardColor1.b, 0.5))
                    border.width: 1
                    border.color: Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.3)
                    opacity: removeHoverHandler.hovered ? 1.0 : 0.85

                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on opacity { NumberAnimation { duration: 150 } }

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 5

                        Image {
                            source: "qrc:/UniversalModManager/Lucide Icons/folder-minus.svg"
                            sourceSize.width: 14
                            sourceSize.height: 14
                            fillMode: Image.PreserveAspectFit
                            layer.enabled: true
                            layer.effect: ColorOverlay { color: window.textColor }
                            opacity: 0.8
                        }

                        Text {
                            text: "Remove"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            color: window.textColor
                            opacity: 0.8
                        }
                    }

                    HoverHandler {
                        id: removeHoverHandler
                        cursorShape: Qt.PointingHandCursor
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            var error = modManager.removeModsFromBundle(
                                modLibraryRoot.openedBundleId, [modName])
                            if (error && error.length > 0)
                                toastNotification.show(error, 6000)
                            else
                                toastNotification.show("Removed from bundle", "check.svg")
                        }
                    }
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        z: -1
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            clearFocus()
            openSelectedModDetails(modName, imageKey)
        }
    }
}
