import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtWebEngine
import Qt5Compat.GraphicalEffects

Window {
    id: downloadWindow

    property var appWindow: null
    property var webProfile: null
    property string targetUrl: ""
    property string modTitle: ""
    property string currentHost: "www.nexusmods.com"
    property string navigationError: ""
    property bool loading: false
    property bool pageLoaded: false
    property bool nxmCaptured: false

    readonly property string frameScanScript: "(function(){"
        + "var widget=document.querySelector('mod-file-download[is-nmm-download=true]');"
        + "var embedded=widget?widget.getAttribute('download-url')||'':'';"
        + "var wanted=new URLSearchParams(location.search).get('file_id')||'';"
        + "var widgetFile=widget?widget.getAttribute('file-id')||'':'';"
        + "if(embedded.toLowerCase().indexOf('nxm://')===0&&(!wanted||wanted===widgetFile))return embedded;"
        + "var links=document.querySelectorAll('a[href^=\"nxm://\"]');"
        + "var body=document.body?document.body.innerText:'';"
        + "var starting=/your download is starting|download didn/i.test(body);"
        + "for(var i=0;i<links.length;i++){"
        + "var link=links[i];var rect=link.getBoundingClientRect();"
        + "if(rect.width<=0&&rect.height<=0)continue;"
        + "var text=link.textContent||'';"
        + "if(/start download manually|download manually/i.test(text)||starting)return link.href;"
        + "}"
        + "return '';"
        + "})()"

    readonly property string filesTabScript: "(function(){"
        + "var bodyText=document.body?document.body.innerText:'';"
        + "if(/main files|old files|mod manager download|manual download|slow download|fast download/i.test(bodyText))return 'active';"
        + "var tab=document.querySelector('a[href*=\"tab=files\"]');"
        + "if(!tab){"
        + "var items=document.querySelectorAll('a,button,[role=\"tab\"]');"
        + "for(var i=0;i<items.length;i++){"
        + "var text=(items[i].textContent||'').trim();"
        + "if(/^files(\\s*\\d+)?$/i.test(text)){tab=items[i];break;}"
        + "}"
        + "}"
        + "if(!tab)return '';"
        + "var active=(tab.classList&&tab.classList.contains('active'))"
        + "||tab.getAttribute('aria-selected')==='true'"
        + "||tab.getAttribute('aria-current')==='page';"
        + "if(active)return 'active';"
        + "tab.click();"
        + "return 'clicked';"
        + "})()"

    property int filesTabAttempts: 0

    property bool browserVisible: false
    readonly property int compactWidth: 440
    readonly property int compactHeight: 160

    readonly property color surfaceColor: appWindow
        ? (modManager.theme === "Glassmorphic"
            ? Qt.rgba(0.12, 0.12, 0.12, 0.98)
            : appWindow.surfaceColor)
        : "#FFFFFF"
    readonly property color textColor: appWindow ? appWindow.textColor : "#3E3E3E"
    readonly property color primaryColor: appWindow ? appWindow.primaryColor : "#BE9F7E"
    readonly property color accentColor: appWindow ? appWindow.accentColor : "#D4A574"
    readonly property color borderColor: appWindow ? appWindow.borderColor : "#E0E0E0"
    readonly property color secondaryTextColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.62)

    title: "Universal Manager Downloader"
    width: compactWidth
    height: compactHeight
    minimumWidth: 380
    minimumHeight: 150
    visible: false
    color: surfaceColor
    modality: Qt.ApplicationModal
    transientParent: appWindow
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowSystemMenuHint

    Component.onCompleted: titleBarController.attachToWindow(downloadWindow)

    Timer {
        id: browserFallbackTimer
        interval: 8000
        repeat: false
        onTriggered: downloadWindow.expandToBrowser()
    }

    function expandToBrowser() {
        if (browserVisible)
            return
        browserFallbackTimer.stop()
        browserVisible = true
        minimumWidth = 760
        minimumHeight = 560
        width = appWindow ? Math.min(1060, Math.max(760, appWindow.width - 80)) : 1060
        height = appWindow ? Math.min(820, Math.max(560, appWindow.height - 80)) : 820
        centerOnAppWindow()
        raise()
        requestActivate()
    }

    function begin(url, title) {
        if (!webProfile) {
            console.warn("Nexus download profile is not ready.")
            return
        }
        targetUrl = url
        modTitle = title || ""
        navigationError = ""
        pageLoaded = false
        loading = false
        nxmCaptured = false
        filesTabAttempts = 0
        browserVisible = false
        width = compactWidth
        height = compactHeight
        minimumWidth = 380
        minimumHeight = 150
        centerOnAppWindow()
        visible = true
        raise()
        requestActivate()
        browserFallbackTimer.restart()
    }

    function centerOnAppWindow() {
        if (!appWindow)
            return
        x = Math.round(appWindow.x + (appWindow.width - width) / 2)
        y = Math.round(appWindow.y + (appWindow.height - height) / 2)
    }

    function finishNxm(nxmUrl) {
        if (!nxmUrl || nxmCaptured)
            return

        browserFallbackTimer.stop()
        nxmCaptured = true
        modManager.handleNxmUrl(nxmUrl)
        close()
        if (appWindow) {
            appWindow.raise()
            appWindow.requestActivate()
        }
    }

    function reloadPage() {
        if (!browserLoader.item)
            return
        browserLoader.active = false
        Qt.callLater(function() {
            browserLoader.active = true
        })
    }

    onVisibleChanged: {
        if (visible)
            centerOnAppWindow()
        else
            browserFallbackTimer.stop()
    }

    Shortcut {
        sequence: "Esc"

        enabled: downloadWindow.visible
        onActivated: downloadWindow.close()
    }

    Connections {
        target: modManager

        function onNexusDownloadPageRequested(url, modName) {
            downloadWindow.begin(url, modName)
        }
    }

    Connections {
        target: nexusOAuth

        function onAuthenticationChanged() {
            if (nexusOAuth.authenticated && downloadWindow.visible)
                downloadWindow.reloadPage()
        }
    }

    Connections {
        target: downloadWindow.webProfile

        function onDownloadRequested(download) {
            if (!download || !downloadWindow.visible)
                return
            downloadWindow.nxmCaptured = true
            var url = download.url ? String(download.url) : ""
            var fileName = download.suggestedFileName ? String(download.suggestedFileName) : ""
            download.cancel()
            if (url.length > 0)
                downloadManager.enqueue(url, fileName, "", modManager.downloadsPath)
            downloadWindow.close()
            if (appWindow) {
                appWindow.raise()
                appWindow.requestActivate()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: downloadWindow.surfaceColor

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 66
                color: "transparent"

                visible: downloadWindow.browserVisible

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 16
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: 9
                        color: Qt.rgba(downloadWindow.accentColor.r, downloadWindow.accentColor.g,
                                       downloadWindow.accentColor.b, 0.16)
                        border.width: 1
                        border.color: Qt.rgba(downloadWindow.accentColor.r, downloadWindow.accentColor.g,
                                              downloadWindow.accentColor.b, 0.32)

                        Image {
                            anchors.centerIn: parent
                            source: "qrc:/UniversalModManager/Lucide Icons/cloud-download.svg"
                            sourceSize: Qt.size(17, 17)
                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: downloadWindow.accentColor
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 1

                        Text {
                            Layout.fillWidth: true
                            text: downloadWindow.modTitle !== ""
                                ? downloadWindow.modTitle
                                : "Nexus Mods"
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            color: downloadWindow.textColor
                            elide: Text.ElideRight
                        }
                    }

                    BusyRing {
                        Layout.alignment: Qt.AlignVCenter
                        visible: downloadWindow.browserVisible
                        running: downloadWindow.browserVisible
                                 && downloadWindow.loading && !downloadWindow.pageLoaded
                        ringColor: downloadWindow.primaryColor
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: downloadWindow.borderColor
                opacity: 0.6
                visible: downloadWindow.browserVisible
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                Loader {
                    id: browserLoader
                    anchors.fill: parent
                    active: downloadWindow.visible
                    sourceComponent: browserComponent
                }

                Rectangle {
                    anchors.fill: parent
                    visible: downloadWindow.navigationError.length > 0 && !downloadWindow.loading
                    color: downloadWindow.surfaceColor
                    z: 10

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 90, 520)
                        spacing: 12

                        Text {
                            Layout.fillWidth: true
                            text: "Could not open Nexus Mods"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            color: downloadWindow.textColor
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            Layout.fillWidth: true
                            text: downloadWindow.navigationError
                            font.pixelSize: 12
                            color: downloadWindow.secondaryTextColor
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 130
                            Layout.preferredHeight: 38
                            radius: 9
                            color: retryArea.containsMouse
                                ? Qt.lighter(downloadWindow.primaryColor, 1.06)
                                : downloadWindow.primaryColor

                            Text {
                                anchors.centerIn: parent
                                text: "Try again"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: "#FFFFFF"
                            }

                            MouseArea {
                                id: retryArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: downloadWindow.reloadPage()
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    z: 20
                    visible: !downloadWindow.browserVisible
                    color: downloadWindow.surfaceColor

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: parent.width - 48
                        spacing: 10

                        BusyRing {
                            Layout.alignment: Qt.AlignHCenter
                            running: !downloadWindow.browserVisible
                            ringColor: downloadWindow.primaryColor
                        }

                        Text {
                            Layout.fillWidth: true
                            text: downloadWindow.navigationError.length > 0
                                ? "Could not fetch the download link"
                                : "Fetching download link..."
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: downloadWindow.textColor
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            Layout.fillWidth: true
                            text: downloadWindow.navigationError.length > 0
                                ? "Opening the Nexus page..."
                                : "The download starts automatically when Nexus responds."
                            font.pixelSize: 11
                            color: downloadWindow.secondaryTextColor
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                        }
                    }
                }

            }
        }
    }

    Component {
        id: browserComponent

        WebEngineView {
            id: downloadView
            anchors.fill: parent
            profile: downloadWindow.webProfile
            url: downloadWindow.targetUrl
            backgroundColor: "#111318"
            zoomFactor: 1.0

            function scanForNxm() {
                if (downloadWindow.nxmCaptured)
                    return
                var frame = downloadView.mainFrame
                if (frame)
                    scanFrame(frame)
            }

            function scanFrame(frame) {
                if (!frame || downloadWindow.nxmCaptured)
                    return
                frame.runJavaScript(downloadWindow.frameScanScript, function(result) {
                    var url = result ? result.toString() : ""
                    if (!downloadWindow.nxmCaptured && url.toLowerCase().indexOf("nxm://") === 0)
                        downloadWindow.finishNxm(url)
                })
                var children = frame.children || []
                for (var i = 0; i < children.length; ++i)
                    scanFrame(children[i])
            }

            Timer {
                interval: 350
                repeat: true
                running: downloadWindow.visible
                         && downloadWindow.navigationError === "" && !downloadWindow.nxmCaptured
                onTriggered: downloadView.scanForNxm()
            }

            Timer {
                id: filesTabTimer
                interval: 900
                repeat: true
                running: downloadWindow.visible && downloadWindow.pageLoaded
                         && downloadWindow.targetUrl.indexOf("tab=files") !== -1
                         && downloadWindow.filesTabAttempts < 3
                onTriggered: {
                    downloadWindow.filesTabAttempts += 1
                    if (!downloadView.mainFrame)
                        return
                    downloadView.mainFrame.runJavaScript(downloadWindow.filesTabScript, function(result) {
                        var outcome = result ? result.toString() : ""
                        if (outcome === "active" || outcome === "clicked")
                            filesTabTimer.stop()
                    })
                }
            }

            onNavigationRequested: function(request) {
                var target = request.url.toString()
                var lower = target.toLowerCase()

                if (lower.indexOf("nxm://") === 0) {
                    request.reject()
                    downloadWindow.finishNxm(target)
                } else if (lower.indexOf("https://") === 0 || lower.indexOf("about:") === 0) {
                    request.accept()
                } else {
                    request.reject()
                }
            }

            onNewWindowRequested: function(request) {
                if (!request)
                    return
                var target = request.requestedUrl.toString()
                var lower = target.toLowerCase()
                if (lower.indexOf("nxm://") === 0) {
                    downloadWindow.finishNxm(target)
                } else if (lower.indexOf("https://") === 0 || lower.indexOf("about:") === 0) {
                    request.openIn(downloadView)
                }
            }

            onFeaturePermissionRequested: function(securityOrigin, feature) {
                downloadView.grantFeaturePermission(securityOrigin, feature, false)
            }

            onContextMenuRequested: function(request) {
                request.accepted = true
            }

            onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceID) {

            }

            onLoadingChanged: function(loadRequest) {
                if (!loadRequest)
                    return
                var target = loadRequest.url ? loadRequest.url.toString().toLowerCase() : ""
                if (loadRequest.status === WebEngineView.LoadStartedStatus) {
                    downloadWindow.loading = true
                } else if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                    downloadWindow.loading = false
                    downloadWindow.pageLoaded = true
                    downloadWindow.navigationError = ""
                } else if (loadRequest.status === WebEngineView.LoadStoppedStatus) {
                    downloadWindow.loading = false
                } else if (loadRequest.status === WebEngineView.LoadFailedStatus) {
                    downloadWindow.loading = false
                    if (target.indexOf("nxm://") !== 0) {
                        downloadWindow.navigationError =
                            "The Nexus Mods page could not be loaded. Check your internet connection and try again."
                        downloadWindow.expandToBrowser()
                    }
                }
            }

            onUrlChanged: {
                if (!url)
                    return
                var host = url.host
                if (url.toString().indexOf("https://") === 0 && host !== undefined && host.length > 0)
                    downloadWindow.currentHost = host
            }
        }
    }
}
