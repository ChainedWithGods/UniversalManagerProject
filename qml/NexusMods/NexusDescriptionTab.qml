import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine

Item {

    readonly property string scanState: computeScanState()
    readonly property string scanStatusLabel: {
        if (scanState === "pending")
            return "Some files not scanned"
        if (scanState === "failed")
            return "Some files may be unsafe"
        if (scanState === "checking")
            return "Checking..."
        if (scanState === "unknown")
            return "Unknown"
        return "Safe to use"
    }
    readonly property color scanStatusColor: {
        if (scanState === "pending")
            return "#E0A126"
        if (scanState === "failed")
            return "#E05A4F"
        if (scanState === "checking" || scanState === "unknown")
            return Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.6)
        return "#2ecc71"
    }
    readonly property color scanCardColor: {
        if (scanState === "pending")
            return Window.window.panelColor(Qt.rgba(0.88, 0.62, 0.1, 0.08))
        if (scanState === "failed")
            return Window.window.panelColor(Qt.rgba(0.85, 0.25, 0.2, 0.08))
        if (scanState === "checking" || scanState === "unknown")
            return Window.window.panelColor(Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.04))
        return Window.window.panelColor(Qt.rgba(0.15, 0.7, 0.3, 0.08))
    }
    readonly property color scanCardBorder: {
        if (scanState === "pending")
            return Qt.rgba(0.88, 0.62, 0.1, 0.3)
        if (scanState === "failed")
            return Qt.rgba(0.85, 0.25, 0.2, 0.3)
        if (scanState === "checking" || scanState === "unknown")
            return Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.15)
        return Qt.rgba(0.15, 0.7, 0.3, 0.3)
    }

    function computeScanState() {
        var files = modManager.currentNexusModFiles || []
        if (modManager.isLoadingNexusModFiles)
            return "checking"
        if (files.length === 0)
            return "unknown"

        var pending = false
        var failed = false
        for (var i = 0; i < files.length; ++i) {
            var status = String(files[i].virusScanStatus || "").toUpperCase()
            if (status === "QUEUED" || status === "WAITING_REPORT")
                pending = true
            else if (status === "QUARANTINED" || status === "REPORT_ERROR"
                     || status === "FILE_NOT_FOUND" || status === "MOD_DOES_NOT_EXIST")
                failed = true
        }
        if (failed)
            return "failed"
        if (pending)
            return "pending"
        return "safe"
    }

    function resetScroll() {
        descFlick.contentY = 0
    }

    function scrollDescriptionBy(deltaY) {
        const maxY = Math.max(0, descFlick.contentHeight - descFlick.height)
        const targetY = Math.max(0, Math.min(maxY,
            descFlick.scrollTarget - deltaY * 3.5))
        descFlick.scrollTarget = targetY
        const distance = Math.abs(targetY - descFlick.contentY)
        descScrollAnim.stop()
        descScrollAnim.to = targetY
        descScrollAnim.duration = Math.max(160, Math.min(500, distance * 0.35))
        descScrollAnim.start()
    }

    Flickable {
        id: descFlick
        anchors.fill: parent
        contentWidth: width
        contentHeight: descCenteredWrapper.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        onWidthChanged: viewRebuildTimer.restart()

        onHeightChanged: viewRebuildTimer.restart()

        readonly property real contentW: Window.window.descriptionCardWidth

        property real scrollTarget: 0

        onContentYChanged: {
            if (!descScrollAnim.running)
                scrollTarget = contentY
        }

        onMovementStarted: descScrollAnim.stop()

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: (event) => {
                event.accepted = true
                scrollDescriptionBy(event.angleDelta.y)
            }
        }

        NumberAnimation {
            id: descScrollAnim
            target: descFlick
            property: "contentY"
            easing.type: Easing.OutQuart
        }

        Item {
            id: descCenteredWrapper
            width: descFlick.contentW
            anchors.horizontalCenter: parent.horizontalCenter

            implicitHeight: Math.max(descCol.implicitHeight + 40, descFlick.height)

            ColumnLayout {
                id: descCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 20
                height: Math.max(implicitHeight, parent.height - 40)
                spacing: 18

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: summaryCol.implicitHeight + 24
                    radius: 8
                    color: Window.window.panelColor(Qt.rgba(Window.window.primaryColor.r, Window.window.primaryColor.g, Window.window.primaryColor.b, 0.08))
                    border.width: 1
                    border.color: Qt.rgba(Window.window.primaryColor.r, Window.window.primaryColor.g, Window.window.primaryColor.b, 0.25)

                    ColumnLayout {
                        id: summaryCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 4

                        Text {
                            text: "ABOUT THIS MOD"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            color: Window.window.primaryColor
                        }

                        Text {
                            Layout.fillWidth: true
                            text: (modManager.currentNexusModDetails && modManager.currentNexusModDetails.summary)
                                ? modManager.currentNexusModDetails.summary
                                : (Window.window.modData.summary || "No summary provided.")
                            font.pixelSize: 13
                            lineHeight: 1.3
                            wrapMode: Text.Wrap
                            color: Window.window.textColor
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        radius: 6
                        color: Window.window.panelColor(Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.04))
                        border.width: 1
                        border.color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.08)

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 2
                            Text {
                                text: "Author"
                                font.pixelSize: 10
                                color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.5)
                            }
                            Text {
                                text: (modManager.currentNexusModDetails && modManager.currentNexusModDetails.author) || Window.window.modData.author || "Unknown"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: Window.window.textColor
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        radius: 6
                        color: Window.window.panelColor(Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.04))
                        border.width: 1
                        border.color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.08)

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 2
                            Text {
                                text: "Uploaded by"
                                font.pixelSize: 10
                                color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.5)
                            }
                            Text {
                                text: (modManager.currentNexusModDetails && modManager.currentNexusModDetails.uploader) || Window.window.modData.uploader || "Unknown"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: Window.window.textColor
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        radius: 6
                        color: scanCardColor
                        border.width: 1
                        border.color: scanCardBorder

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 2
                            Text {
                                text: "Virus scan"
                                font.pixelSize: 10
                                color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.5)
                            }
                            Text {
                                text: scanStatusLabel
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: scanStatusColor
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: chunksColumn.implicitHeight + 36
                    radius: 8
                    color: Window.window.panelColor(Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.02))
                    border.width: 1
                    border.color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.08)

                    Column {
                        id: chunksColumn
                        anchors.top: parent.top
                        anchors.topMargin: 18
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.min(parent.width - 36, Window.window.contentMaxWidth)
                        spacing: 0

                        Component {
                            id: avifWebComponent

                            WebEngineView {
                                property string mediaUrl: ""
                                backgroundColor: "#000000"
                                activeFocusOnPress: false

                                enabled: false

                                onMediaUrlChanged: {
                                    if (mediaUrl.length === 0)
                                        return
                                    loadHtml('<html><head><style>html,body{margin:0;padding:0;width:100%;height:100%;background:#000;overflow:hidden}img{display:block;width:100%;height:100%;object-fit:contain}</style></head><body><img src="' + mediaUrl + '"></body></html>',
                                        "https://raw.githubusercontent.com/")
                                }
                            }
                        }

                        Repeater {
                            model: Window.window.descriptionBlocks

                            delegate: Item {
                                id: chunkItem
                                required property int index
                                required property var modelData

                                readonly property bool isVideo: !!modelData && modelData.kind === "video"
                                readonly property bool isAvif: !!modelData && modelData.kind === "avif"
                                readonly property real cardWidth: Math.min(width, 560)
                                readonly property real cardHeight: cardWidth * 9 / 16
                                readonly property real avifWidth: isAvif && modelData.width ? modelData.width : 1000
                                readonly property real avifHeight: isAvif && modelData.height ? modelData.height : 562
                                readonly property real avifCardWidth: Math.min(width, avifWidth)
                                readonly property real avifCardHeight: avifCardWidth * avifHeight / avifWidth

                                property bool avifLoaded: false
                                onNearViewportChanged: if (nearViewport) avifLoaded = true
                                readonly property bool nearViewport: {

                                    var scroll = descFlick.contentY
                                    var pos = chunkItem.y
                                    var top = chunkItem.mapToItem(descFlick, 0, 0).y
                                    return top + chunkItem.height > -600
                                        && top < descFlick.height + 600
                                }

                                width: chunksColumn.width
                                height: isVideo ? cardHeight + 24
                                    : (isAvif ? avifCardHeight + 24 : chunkText.implicitHeight)

                                Text {
                                    id: chunkText
                                    width: parent.width
                                    textFormat: Text.RichText
                                    wrapMode: Text.Wrap
                                    font.pixelSize: 13
                                    font.family: "Segoe UI, sans-serif"
                                    color: Window.window.textColor
                                    baseUrl: "https://www.nexusmods.com"
                                    text: (chunkItem.isVideo || chunkItem.isAvif || !chunkItem.modelData)
                                        ? "" : (chunkItem.modelData.html || "")
                                    onLinkActivated: (link) => Window.window.handleDescriptionLink(link)

                                    visible: !chunkItem.isVideo && !chunkItem.isAvif
                                        && chunkItem.nearViewport
                                }

                                MouseArea {
                                    anchors.fill: chunkText
                                    visible: chunkText.visible
                                    hoverEnabled: true
                                    acceptedButtons: Qt.LeftButton
                                    cursorShape: chunkText.linkAt(mouseX, mouseY) !== ""
                                        ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: (mouse) => {
                                        var link = chunkText.linkAt(mouse.x, mouse.y)
                                        if (link.length > 0)
                                            Window.window.handleDescriptionLink(link)
                                    }
                                }

                                Rectangle {
                                    id: videoCard
                                    visible: chunkItem.isVideo && chunkItem.nearViewport
                                    width: chunkItem.cardWidth
                                    height: chunkItem.cardHeight
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.top: parent.top
                                    anchors.topMargin: 12
                                    color: "#000000"
                                    border.width: 1
                                    border.color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.15)
                                    clip: true

                                    Image {
                                        id: videoThumb
                                        anchors.fill: parent

                                        source: chunkItem.isVideo
                                            ? "https://img.youtube.com/vi/" + chunkItem.modelData.videoId + "/hqdefault.jpg"
                                            : ""
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        cache: true
                                    }

                                    Rectangle {
                                        anchors.fill: parent
                                        color: Qt.rgba(0, 0, 0, 0.12)
                                    }

                                    Rectangle {
                                        width: 68
                                        height: 48
                                        radius: 12
                                        anchors.centerIn: parent
                                        color: "#CC0000"

                                        Text {
                                            anchors.centerIn: parent
                                            text: "▶"
                                            color: "#FFFFFF"
                                            font.pixelSize: 24
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Window.window.openVideo(chunkItem.modelData.videoId)
                                    }
                                }

                                Rectangle {
                                    id: avifCard
                                    visible: chunkItem.isAvif && chunkItem.nearViewport
                                    width: chunkItem.avifCardWidth
                                    height: chunkItem.avifCardHeight
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.top: parent.top
                                    anchors.topMargin: 12
                                    color: "#000000"
                                    border.width: 1
                                    border.color: Qt.rgba(Window.window.textColor.r, Window.window.textColor.g, Window.window.textColor.b, 0.15)
                                    clip: true

                                    Loader {
                                        id: avifLoader
                                        anchors.fill: parent
                                        active: chunkItem.isAvif && chunkItem.avifLoaded
                                        property string mediaUrl: chunkItem.isAvif
                                            ? chunkItem.modelData.url : ""

                                        sourceComponent: avifWebComponent
                                        onLoaded: if (item)
                                            item.mediaUrl = mediaUrl
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.LeftButton
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (chunkItem.modelData.href)
                                                Window.window.handleDescriptionLink(chunkItem.modelData.href)
                                            else
                                                Window.window.openAvif(chunkItem.modelData.url)
                                        }
                                    }

                                    WheelHandler {
                                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                        onWheel: (event) => {
                                            event.accepted = true
                                            scrollDescriptionBy(event.angleDelta.y)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
