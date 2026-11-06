import QtQuick
import QtQuick.Layouts
import QtWebEngine

Window {
    required property var dialog
    id: videoWindow
    width: 960
    height: 640
    minimumWidth: 640
    minimumHeight: 400
    visible: false
    color: "#0B0B0B"
    title: videoId.length > 0 ? ("YouTube - " + videoId) : "YouTube"
    transientParent: dialog
    modality: Qt.WindowModal
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint |
        Qt.WindowMaximizeButtonHint | Qt.WindowSystemMenuHint

    property string videoId: ""
    property bool playbackFailed: false
    property int playbackChecks: 0

    function openVideo(id) {
        if (!id)
            return
        videoId = id
        playbackFailed = false
        playbackChecks = 0

        webView.url = "https://www.youtube.com/watch?v=" + id
        visible = true
        raise()
        requestActivate()
    }

    function openExternally() {
        Qt.openUrlExternally("https://www.youtube.com/watch?v=" + videoWindow.videoId)
    }

    function probePlayback() {
        playbackChecks += 1
        webView.runJavaScript("(function(){"
            + "if(document.querySelector('.ytp-error'))return 'blocked';"
            + "var t=document.body?document.body.innerText:'';"
            + "if(/video unavailable|can't play this video|cannot play this video/i.test(t))return 'blocked';"
            + "var v=document.querySelector('video');"
            + "if(!v)return 'none';"
            + "if(v.error)return 'error';"
            + "if(v.readyState>=2&&v.currentTime>0)return 'playing';"
            + "return 'waiting';"
            + "})()", function(result) {
            var state = result ? result.toString() : ""
            if (state === "playing") {
                playbackProbe.stop()
            } else if (state === "blocked" || state === "error"
                       || (state === "none" && videoWindow.playbackChecks >= 8)
                       || videoWindow.playbackChecks >= 20) {
                videoWindow.playbackFailed = true
                playbackProbe.stop()
            }
        })
    }

    Timer {
        id: playbackProbe
        interval: 800
        repeat: true
        running: videoWindow.visible && !videoWindow.playbackFailed
                 && videoWindow.playbackChecks < 20
        onTriggered: videoWindow.probePlayback()
    }

    onVisibleChanged: {
        if (!visible) {
            playbackProbe.stop()
            playbackFailed = false
            playbackChecks = 0
            webView.url = "about:blank"
        }
    }

    Rectangle {
        id: videoBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 40
        color: "#111111"

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            text: "YouTube"
            color: "#FFFFFF"
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }

    }

    WebEngineView {
        id: webView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: videoBar.bottom
        anchors.bottom: parent.bottom
        backgroundColor: "#000000"
        settings.playbackRequiresUserGesture: false
    }

    Rectangle {
        anchors.fill: webView
        visible: videoWindow.playbackFailed
        color: "#0B0B0B"
        z: 10

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 80, 440)
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: "This video can't play here"
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: "#FFFFFF"
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                text: "Some videos can't play in the embedded browser, either because this Qt build lacks the required codec or because the video restricts embedded playback. Open it in your default browser instead."
                font.pixelSize: 12
                color: "#B8B8B8"
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: openFallbackLabel.implicitWidth + 32
                Layout.preferredHeight: 36
                radius: 8
                color: fallbackMouse.containsMouse ? "#E8A33D" : "#D4902A"

                Text {
                    id: openFallbackLabel
                    anchors.centerIn: parent
                    text: "Open on YouTube"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: "#FFFFFF"
                }

                MouseArea {
                    id: fallbackMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: videoWindow.openExternally()
                }
            }
        }
    }
}
