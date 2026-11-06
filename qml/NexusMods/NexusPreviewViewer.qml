import QtQuick
import QtQuick.Effects
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: previewViewer
    anchors.fill: parent
    z: 20000
    visible: opacity > 0
    opacity: 0
    focus: visible

    property int viewerIndex: -1

    property var viewerImages: []
    readonly property var images:
        viewerImages.length > 0 ? viewerImages : Window.window.previewImages
    readonly property var currentImage:
        (viewerIndex >= 0 && viewerIndex < images.length) ? images[viewerIndex] : null
    readonly property string caption:
        (currentImage && currentImage.caption) ? currentImage.caption : ""

    Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

    function openAt(index) {
        if (index < 0 || index >= Window.window.previewImages.length) return
        viewerImages = []
        viewerIndex = index
        opacity = 1
        forceActiveFocus()
    }

    function openImages(list, index) {
        if (!list || index < 0 || index >= list.length) return
        viewerImages = list
        viewerIndex = index
        opacity = 1
        forceActiveFocus()
    }

    function closeViewer() {
        viewerIndex = -1
        opacity = 0
        viewerImages = []
    }

    function step(delta) {
        if (images.length < 2) return
        viewerIndex = (viewerIndex + delta + images.length) % images.length
    }

    onVisibleChanged: if (visible) forceActiveFocus()
    Keys.onEscapePressed: closeViewer()
    Keys.onLeftPressed: step(-1)
    Keys.onRightPressed: step(1)

    Rectangle {
        anchors.fill: parent
        color: "#F20B0C12"

        MouseArea {
            anchors.fill: parent
            onClicked: previewViewer.closeViewer()
            onWheel: function(wheel) { wheel.accepted = true }
        }
    }

    Item {
        anchors.fill: parent
        anchors.leftMargin: 92
        anchors.rightMargin: 92
        anchors.topMargin: 64
        anchors.bottomMargin: 78

        Image {
            anchors.fill: parent
            source: previewViewer.currentImage ? (previewViewer.currentImage.thumbUrl || "") : ""
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            opacity: 0.3
            visible: fullPreviewImage.status !== Image.Ready
        }

        Image {
            id: fullPreviewImage
            anchors.fill: parent
            source: previewViewer.currentImage ? (previewViewer.currentImage.fullUrl || "") : ""
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            smooth: true
            opacity: status === Image.Ready ? 1 : 0

            layer.enabled: Window.window.blurAdultMedia
            layer.effect: MultiEffect {
                blurEnabled: true
                blur: 0.7
                blurMax: 64
            }

            Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

            MouseArea {
                anchors.fill: parent
                onWheel: function(wheel) { wheel.accepted = true }
            }
        }

        BusyIndicator {
            anchors.centerIn: parent
            width: 36
            height: 36
            running: fullPreviewImage.status === Image.Loading
            visible: running
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 24
        height: 34
        width: previewCounter.implicitWidth + 26
        radius: 17
        color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.16)

        Text {
            id: previewCounter
            anchors.centerIn: parent
            text: Math.max(0, previewViewer.viewerIndex + 1) + " / " + previewViewer.images.length
            font.pixelSize: 12
            font.weight: Font.DemiBold
            color: "#FFFFFF"
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        width: 40
        height: 40
        radius: 20
        color: closePreviewMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(1, 1, 1, 0.1)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.18)

        Behavior on color { ColorAnimation { duration: 100 } }

        Text {
            anchors.centerIn: parent
            text: "✕"
            font.pixelSize: 15
            font.weight: Font.Bold
            color: "#FFFFFF"
        }

        MouseArea {
            id: closePreviewMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: previewViewer.closeViewer()
        }
    }

    Rectangle {
        visible: previewViewer.images.length > 1
        anchors.left: parent.left
        anchors.leftMargin: 22
        anchors.verticalCenter: parent.verticalCenter
        width: 50
        height: 50
        radius: 25
        color: previousPreviewMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(1, 1, 1, 0.09)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.15)

        Behavior on color { ColorAnimation { duration: 100 } }

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -2
            text: "‹"
            font.pixelSize: 32
            color: "#FFFFFF"
        }

        MouseArea {
            id: previousPreviewMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: previewViewer.step(-1)
        }
    }

    Rectangle {
        visible: previewViewer.images.length > 1
        anchors.right: parent.right
        anchors.rightMargin: 22
        anchors.verticalCenter: parent.verticalCenter
        width: 50
        height: 50
        radius: 25
        color: nextPreviewMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(1, 1, 1, 0.09)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.15)

        Behavior on color { ColorAnimation { duration: 100 } }

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -2
            text: "›"
            font.pixelSize: 32
            color: "#FFFFFF"
        }

        MouseArea {
            id: nextPreviewMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: previewViewer.step(1)
        }
    }

    Rectangle {
        visible: previewViewer.caption.length > 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        height: 34
        width: Math.min(previewCaption.implicitWidth + 28, previewViewer.width - 220)
        radius: 8
        color: Qt.rgba(0, 0, 0, 0.6)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.12)

        Text {
            id: previewCaption
            anchors.centerIn: parent
            width: parent.width - 24
            text: previewViewer.caption
            textFormat: Text.PlainText
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 12
            color: "#FFFFFF"
        }
    }

    Image {
        visible: false
        asynchronous: true
        cache: true
        source: {
            var list = previewViewer.images
            if (list.length < 2 || previewViewer.viewerIndex < 0) return ""
            var image = list[(previewViewer.viewerIndex + 1) % list.length]
            return image ? (image.fullUrl || "") : ""
        }
    }

    Image {
        visible: false
        asynchronous: true
        cache: true
        source: {
            var list = previewViewer.images
            if (list.length < 2 || previewViewer.viewerIndex < 0) return ""
            var image = list[(previewViewer.viewerIndex - 1 + list.length) % list.length]
            return image ? (image.fullUrl || "") : ""
        }
    }
}
