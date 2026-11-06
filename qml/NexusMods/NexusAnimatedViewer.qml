import QtQuick
import QtWebEngine

Window {
    required property var dialog
    id: avifWindow
    width: 1100
    height: 760
    minimumWidth: 640
    minimumHeight: 400
    visible: false
    color: "#000000"
    title: "Universal Manager Animated Viewer"
    transientParent: dialog
    modality: Qt.WindowModal
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint |
        Qt.WindowMaximizeButtonHint | Qt.WindowSystemMenuHint

    function openAvif(url) {
        if (!url)
            return
        avifView.loadHtml('<html><head><style>html,body{margin:0;padding:0;width:100%;height:100%;background:#000;overflow:hidden;display:flex;align-items:center;justify-content:center}img{display:block;max-width:100%;max-height:100%;object-fit:contain}</style></head><body><img src="' + url + '"></body></html>',
            "https://raw.githubusercontent.com/")
        visible = true
        raise()
        requestActivate()
    }

    onVisibleChanged: {
        if (!visible)
            avifView.url = "about:blank"
    }

    WebEngineView {
        id: avifView
        anchors.fill: parent
        backgroundColor: "#000000"
        onContextMenuRequested: function(request) { request.accepted = true }
    }
}
