import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtWebEngine
import Qt5Compat.GraphicalEffects

Window {
    id: signInWindow

    property var appWindow: null
    property var webProfile: null

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

    property string navigationError: ""
    property string currentHost: "users.nexusmods.com"

    title: "Sign in to Nexus Mods"
    width: 560
    height: 780
    minimumWidth: 460
    minimumHeight: 600
    visible: false
    color: surfaceColor
    modality: Qt.ApplicationModal
    transientParent: appWindow
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowSystemMenuHint

    Component.onCompleted: titleBarController.attachToWindow(signInWindow)

    function begin() {
        if (!webProfile) {
            console.warn("Nexus sign-in profile is not ready.")
            return
        }
        navigationError = ""
        nexusOAuth.beginSignIn()
        centerOnAppWindow()
        visible = true
        raise()
        requestActivate()
    }

    function centerOnAppWindow() {
        if (!appWindow)
            return
        x = Math.round(appWindow.x + (appWindow.width - width) / 2)
        y = Math.round(appWindow.y + (appWindow.height - height) / 2)
    }

    onVisibleChanged: {
        if (visible)
            centerOnAppWindow()
    }

    onClosing: function(close) {
        if (nexusOAuth.signingIn)
            nexusOAuth.cancelSignIn()
    }

    Shortcut {
        sequence: "Esc"

        enabled: signInWindow.visible
        onActivated: signInWindow.close()
    }

    Connections {
        target: nexusOAuth

        function onAuthenticationChanged() {
            if (nexusOAuth.authenticated)
                signInWindow.close()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: signInWindow.surfaceColor

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 68
                color: "transparent"

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 20
                    anchors.rightMargin: 16
                    spacing: 2

                    Text {
                        text: "Nexus Mods"
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        color: signInWindow.textColor
                    }

                    Text {
                        text: "Secure sign-in · " + signInWindow.currentHost
                        font.pixelSize: 12
                        color: signInWindow.secondaryTextColor
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: signInWindow.borderColor
                opacity: 0.6
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                Loader {
                    id: browserLoader
                    anchors.fill: parent
                    active: signInWindow.visible && !nexusOAuth.authenticated
                    sourceComponent: browserComponent
                }

                Component {
                    id: browserComponent

                    Item {
                        anchors.fill: parent

                        WebEngineView {
                            id: authView
                            anchors.fill: parent
                            profile: signInWindow.webProfile
                            backgroundColor: "#FFFFFF"
                            zoomFactor: 1.0
                            url: nexusOAuth.authorizationUrl

                            onNavigationRequested: function(request) {
                                var target = request.url.toString()
                                var lower = target.toLowerCase()

                                if (lower.indexOf("nxm://oauth/callback") === 0) {

                                    request.reject()
                                    nexusOAuth.handleCallbackUrl(request.url)
                                } else if (lower.indexOf("https://") === 0
                                           || lower.indexOf("about:") === 0) {
                                    request.accept()
                                } else {

                                    request.reject()
                                }
                            }

                            onNewWindowRequested: function(request) {
                                var target = request.requestedUrl.toString().toLowerCase()
                                if (target.indexOf("https://") === 0 || target.indexOf("about:") === 0)
                                    request.openIn(authView)
                            }

                            onFeaturePermissionRequested: function(securityOrigin, feature) {

                                authView.grantFeaturePermission(securityOrigin, feature, false)
                            }

                            onContextMenuRequested: function(request) {

                                request.accepted = true
                            }

                            onTooltipRequested: function(request) {

                                request.accepted = true
                            }

                            onUrlChanged: {
                                if (!url)
                                    return
                                var target = url.toString()
                                var host = url.host
                                if (target.indexOf("https://") === 0
                                        && host !== undefined && host.length > 0)
                                    signInWindow.currentHost = host
                            }

                            onLoadingChanged: function(loadRequest) {
                                if (!loadRequest)
                                    return
                                var target = loadRequest.url ? loadRequest.url.toString().toLowerCase() : ""
                                if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                                    signInWindow.navigationError = ""
                                } else if (loadRequest.status === WebEngineView.LoadFailedStatus
                                           && target.indexOf("nxm://") !== 0) {
                                    signInWindow.navigationError =
                                        "Could not load the Nexus Mods sign-in page. Check your internet connection and try again."
                                }
                            }

                            onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceID) {

                            }
                        }

                        Rectangle {
                            anchors.top: parent.top
                            height: 3
                            width: parent.width * Math.max(0, Math.min(1, authView.loadProgress / 100))
                            color: signInWindow.primaryColor
                            visible: authView.loading
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: "#F7F7F8"
                            visible: authView.loading && authView.loadProgress < 100

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 14

                                BusyIndicator {
                                    Layout.alignment: Qt.AlignHCenter
                                    running: signInWindow.visible
                                }

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "Opening Nexus Mods…"
                                    font.pixelSize: 13
                                    color: "#6B7280"
                                }
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 46
                            visible: signInWindow.navigationError.length > 0 && !authView.loading
                            color: Qt.rgba(0.94, 0.27, 0.27, 0.96)

                            Text {
                                anchors.fill: parent
                                anchors.margins: 10
                                text: signInWindow.navigationError
                                font.pixelSize: 12
                                color: "#FFFFFF"
                                wrapMode: Text.WordWrap
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    visible: nexusOAuth.signingIn && nexusOAuth.busy
                    color: Qt.rgba(0, 0, 0, 0.35)

                    Rectangle {
                        anchors.centerIn: parent
                        width: 320
                        height: 150
                        radius: 16
                        color: signInWindow.surfaceColor
                        border.width: 1
                        border.color: signInWindow.borderColor

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 12

                            BusyIndicator {
                                Layout.alignment: Qt.AlignHCenter
                                running: parent.visible
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Completing secure sign-in…"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: signInWindow.textColor
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Exchanging the authorization code for tokens."
                                font.pixelSize: 12
                                color: signInWindow.secondaryTextColor
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    visible: nexusOAuth.lastError.length > 0
                        && !nexusOAuth.signingIn
                        && !nexusOAuth.authenticated
                    color: signInWindow.surfaceColor

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 80, 430)
                        spacing: 14

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 56
                            Layout.preferredHeight: 56
                            radius: 28
                            color: Qt.rgba(0.94, 0.27, 0.27, 0.12)
                            border.width: 1
                            border.color: Qt.rgba(0.94, 0.27, 0.27, 0.35)

                            Text {
                                anchors.centerIn: parent
                                text: "⚠"
                                font.pixelSize: 26
                                color: "#EF4444"
                            }
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "Sign-in didn't complete"
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                            color: signInWindow.textColor
                        }

                        Text {
                            Layout.fillWidth: true
                            text: nexusOAuth.lastError
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 13
                            color: signInWindow.secondaryTextColor
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 12

                            Rectangle {
                                Layout.preferredHeight: 38
                                Layout.preferredWidth: 120
                                radius: 8
                                color: retryMouse.containsMouse
                                    ? Qt.darker(signInWindow.primaryColor, 1.08)
                                    : signInWindow.primaryColor

                                Text {
                                    anchors.centerIn: parent
                                    text: "Try Again"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    color: "#FFFFFF"
                                }

                                MouseArea {
                                    id: retryMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: signInWindow.begin()
                                }
                            }

                            Rectangle {
                                Layout.preferredHeight: 38
                                Layout.preferredWidth: 100
                                radius: 8
                                color: cancelMouse.containsMouse
                                    ? Qt.rgba(signInWindow.textColor.r, signInWindow.textColor.g,
                                              signInWindow.textColor.b, 0.1)
                                    : "transparent"
                                border.width: 1
                                border.color: signInWindow.borderColor

                                Text {
                                    anchors.centerIn: parent
                                    text: "Cancel"
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: signInWindow.textColor
                                }

                                MouseArea {
                                    id: cancelMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: signInWindow.close()
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: signInWindow.borderColor
                opacity: 0.6
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 54
                color: "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 8

                    Image {
                        id: footerShieldIcon
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                        source: "qrc:/UniversalModManager/Lucide Icons/shield-check.svg"
                        sourceSize: Qt.size(14, 14)
                        smooth: true

                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: Qt.rgba(signInWindow.textColor.r, signInWindow.textColor.g,
                                           signInWindow.textColor.b, 0.5)
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "OAuth 2.0 with PKCE · Your Nexus Mods password never reaches Universal Manager."
                        font.pixelSize: 11
                        color: Qt.rgba(signInWindow.textColor.r, signInWindow.textColor.g,
                                       signInWindow.textColor.b, 0.5)
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
