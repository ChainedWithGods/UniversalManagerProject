import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

ColumnLayout {
    id: nexusSettingsRoot
    Layout.fillWidth: true
    Layout.rightMargin: 10
    spacing: 18

    property bool showApiKey: false
    property bool nxmRegistered: false

    Component.onCompleted: nxmRegistered = modManager.isNxmProtocolRegistered()
    readonly property bool signedIn: nexusOAuth.authenticated

    readonly property bool panelTheme: modManager.theme === "Crimson" || modManager.theme === "Wavey"
    readonly property bool crimsonTheme: modManager.theme === "Crimson"

    function highlightApiKey() {
        apiKeySplitAnim.restart()
        Qt.callLater(function() { apiKeyField.forceActiveFocus() })
    }

    SequentialAnimation {
        id: apiKeySplitAnim

        ParallelAnimation {
            NumberAnimation { target: apiKeySplitLeft; property: "opacity"; from: 0; to: 1; duration: 120 }
            NumberAnimation { target: apiKeySplitRight; property: "opacity"; from: 0; to: 1; duration: 120 }
            NumberAnimation {
                target: apiKeySplitLeft
                property: "width"
                from: 0
                to: Math.max(0, apiKeyFieldBox.width / 2 - 6)
                duration: 340
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: apiKeySplitRight
                property: "width"
                from: 0
                to: Math.max(0, apiKeyFieldBox.width / 2 - 6)
                duration: 340
                easing.type: Easing.OutCubic
            }
        }

        PauseAnimation { duration: 240 }

        ParallelAnimation {
            NumberAnimation { target: apiKeySplitLeft; property: "opacity"; to: 0; duration: 280 }
            NumberAnimation { target: apiKeySplitRight; property: "opacity"; to: 0; duration: 280 }
        }

        ScriptAction {
            script: {
                apiKeySplitLeft.width = 0
                apiKeySplitRight.width = 0
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 4

        Text {
            Layout.fillWidth: true
            text: "Nexus Mods Integration"
            font.pixelSize: 26
            font.weight: Font.Bold
            color: window.textColor
        }

        Text {
            Layout.fillWidth: true
            text: "Connect your Nexus Mods account to search, install, and update mods directly."
            font.pixelSize: 13
            color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.65)
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 170
        radius: nexusSettingsRoot.panelTheme ? 12 : 18
        color: nexusSettingsRoot.panelTheme && !nexusSettingsRoot.crimsonTheme
            ? window.surfaceColor
            : "transparent"
        border.width: nexusSettingsRoot.panelTheme ? window.borderWidth : 0
        border.color: window.borderColor

        layer.enabled: nexusSettingsRoot.panelTheme
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.30)
            shadowVerticalOffset: 4
            shadowBlur: 0.45
        }

        Gradient {
            id: crimsonBannerGradient
            GradientStop { position: 0.0; color: window.crimsonGradientTop }
            GradientStop { position: 1.0; color: window.crimsonGradientBottom }
        }

        Gradient {
            id: accentBannerGradient
            GradientStop {
                position: 0.0
                color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
            }
            GradientStop {
                position: 1.0
                color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.02)
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: parent.border.width
            radius: Math.max(0, parent.radius - parent.border.width)
            visible: nexusSettingsRoot.crimsonTheme || !nexusSettingsRoot.panelTheme
            gradient: nexusSettingsRoot.crimsonTheme ? crimsonBannerGradient : accentBannerGradient
        }

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 24
            spacing: 18

            Rectangle {
                Layout.preferredWidth: 72
                Layout.preferredHeight: 72
                radius: 36
                color: window.surfaceColor
                border.width: 1
                border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.35)

                Image {
                    id: bannerAvatar
                    anchors.fill: parent
                    anchors.margins: 3
                    source: nexusOAuth.authenticated ? nexusOAuth.avatarUrl : ""
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                    layer.enabled: true
                    layer.effect: OpacityMask {
                        maskSource: Rectangle {
                            width: bannerAvatar.width
                            height: bannerAvatar.height
                            radius: 33
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: bannerAvatar.status !== Image.Ready && nexusOAuth.authenticated
                    text: nexusOAuth.userName.length > 0 ? nexusOAuth.userName.charAt(0).toUpperCase() : ""
                    font.pixelSize: 28
                    font.weight: Font.Bold
                    color: window.primaryColor
                }

                Image {
                    anchors.centerIn: parent
                    width: 30
                    height: 30
                    visible: !nexusOAuth.authenticated
                    source: "qrc:/UniversalModManager/Lucide Icons/user.svg"
                    sourceSize: Qt.size(30, 30)
                    smooth: true

                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: window.primaryColor
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                spacing: 5

                Text {
                    Layout.fillWidth: true
                    text: nexusOAuth.authenticated
                        ? (nexusOAuth.userName.length > 0 ? nexusOAuth.userName : "Nexus Mods member")
                        : "Nexus Mods Account"
                    font.pixelSize: 21
                    font.weight: Font.Bold
                    color: window.textColor
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: nexusOAuth.authenticated
                        ? (nexusOAuth.premium
                            ? "Premium member · Signed in securely"
                            : (nexusOAuth.supporter ? "Supporter · Signed in securely" : "Member · Signed in securely"))
                        : "Sign in to browse, install and update mods."
                    font.pixelSize: 13
                    color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.7)
                }

                RowLayout {
                    Layout.alignment: Qt.AlignLeft
                    spacing: 6

                    Image {
                        id: bannerShield
                        Layout.preferredWidth: 12
                        Layout.preferredHeight: 12
                        source: "qrc:/UniversalModManager/Lucide Icons/shield-check.svg"
                        sourceSize: Qt.size(12, 12)
                        smooth: true

                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.8)
                        }
                    }

                    Text {
                        text: "OAuth 2.0 with PKCE · session tokens encrypted with Windows DPAPI"
                        font.pixelSize: 11
                        color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.55)
                    }
                }
            }

        }

        Rectangle {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 20
            anchors.bottomMargin: 16
            width: 124
            height: 40
            radius: 10
            color: nexusOAuth.authenticated
                ? (accountAuthMouse.containsMouse
                    ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.12)
                    : Qt.rgba(window.surfaceColor.r, window.surfaceColor.g, window.surfaceColor.b, 0.8))
                : (accountAuthMouse.containsMouse
                    ? Qt.darker(window.primaryColor, 1.08)
                    : window.primaryColor)
            border.width: nexusOAuth.authenticated ? 1 : 0
            border.color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.18)

            Text {
                anchors.centerIn: parent
                text: nexusOAuth.authenticated ? "Sign Out" : "Sign In"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: nexusOAuth.authenticated ? window.textColor : "#FFFFFF"
            }

            MouseArea {
                id: accountAuthMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (nexusOAuth.authenticated) {
                        nexusOAuth.signOut()
                    } else {
                        window.openNexusSignIn()
                    }
                }
            }
        }
    }

    Text {
        text: "PREFERENCES"
        font.pixelSize: 10
        font.weight: Font.DemiBold
        font.letterSpacing: 1.3
        color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.5)
        Layout.topMargin: 6
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: prefsRows.implicitHeight + (nexusSettingsRoot.panelTheme ? 24 : 0)
        radius: nexusSettingsRoot.panelTheme ? 12 : 0
        color: nexusSettingsRoot.panelTheme && !nexusSettingsRoot.crimsonTheme ? window.surfaceColor : "transparent"
        border.width: nexusSettingsRoot.panelTheme ? window.borderWidth : 0
        border.color: window.borderColor

        Rectangle {
            anchors.fill: parent
            anchors.margins: parent.border.width
            radius: Math.max(0, parent.radius - parent.border.width)
            visible: nexusSettingsRoot.crimsonTheme
            gradient: crimsonPrefsGradient
        }

        Gradient {
            id: crimsonPrefsGradient
            GradientStop { position: 0.0; color: window.crimsonGradientTop }
            GradientStop { position: 1.0; color: window.crimsonGradientBottom }
        }

    ColumnLayout {
        id: prefsRows
        anchors.fill: parent
        anchors.margins: nexusSettingsRoot.panelTheme ? 12 : 0
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            radius: 12
            color: navRowHover.hovered
                ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.04)
                : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }

            HoverHandler { id: navRowHover }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 14

                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: 10
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)

                    Image {
                        id: navigationIcon
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        source: "qrc:/UniversalModManager/Lucide Icons/gamepad-2.svg"
                        sourceSize: Qt.size(18, 18)
                        smooth: true

                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: window.primaryColor
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: "Navigation Bar"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: window.textColor
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Add a fast-access Nexus Mods button to the floating pill."
                        font.pixelSize: 11
                        color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.6)
                        wrapMode: Text.WordWrap
                    }
                }

                UiverseToggle {
                    checked: modManager.nexusEnabled
                    onToggled: (value) => modManager.setNexusEnabled(value)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            color: Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.55)
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            radius: 12
            color: nxmRowHover.hovered
                ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.04)
                : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }

            HoverHandler { id: nxmRowHover }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 14

                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: 10
                    color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.14)

                    Image {
                        id: nxmIcon
                        anchors.centerIn: parent
                        width: 17
                        height: 17
                        source: "qrc:/UniversalModManager/Lucide Icons/link.svg"
                        sourceSize: Qt.size(17, 17)
                        smooth: true

                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: window.accentColor
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: "Download Links"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: window.textColor
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Pass download keys from the Nexus website to Universal Manager."
                        font.pixelSize: 11
                        color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.6)
                        wrapMode: Text.WordWrap
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 104
                    Layout.preferredHeight: 32
                    radius: 8
                    color: nexusSettingsRoot.nxmRegistered
                        ? "#10B981"
                        : (nxmMouse.containsMouse ? Qt.darker(window.accentColor, 1.1) : window.accentColor)
                    Behavior on color { ColorAnimation { duration: 120 } }

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 5

                        Image {
                            Layout.preferredWidth: 12
                            Layout.preferredHeight: 12
                            source: "qrc:/UniversalModManager/Lucide Icons/check.svg"
                            sourceSize: Qt.size(12, 12)
                            smooth: true
                            visible: nexusSettingsRoot.nxmRegistered

                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: "#FFFFFF"
                            }
                        }

                        Text {
                            text: nexusSettingsRoot.nxmRegistered ? "Linked" : "Associate"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                        }
                    }

                    MouseArea {
                        id: nxmMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (modManager.registerNxmProtocol()) {
                                nexusSettingsRoot.nxmRegistered = true
                            }
                        }
                    }

                    ToolTip {
                        id: nxmTip
                        visible: nxmMouse.containsMouse
                        delay: 100
                        padding: 10
                        text: nexusSettingsRoot.nxmRegistered
                            ? "Universal Manager handles Nexus download links on this PC. Use Remove to unlink it."
                            : "Registers Universal Manager for Nexus download links, so 'Mod manager download' button on Nexus website start the download here."

                        contentItem: Text {
                            width: 260
                            text: nxmTip.text
                            wrapMode: Text.Wrap
                            lineHeight: 1.3
                            color: window.textColor
                            font.pixelSize: 11
                        }

                        background: Rectangle {
                            radius: 8
                            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor
                            border.width: 1
                            border.color: window.borderColor
                        }
                    }
                }

                Rectangle {
                    id: nxmRemoveButton
                    visible: nexusSettingsRoot.nxmRegistered
                    Layout.preferredWidth: 92
                    Layout.preferredHeight: 32
                    radius: 8
                    color: nxmRemoveMouse.containsMouse
                        ? Qt.rgba(0.94, 0.27, 0.27, 0.12)
                        : "transparent"
                    border.width: 1
                    border.color: nxmRemoveMouse.containsMouse
                        ? "#EF4444"
                        : Qt.rgba(0.94, 0.27, 0.27, 0.55)
                    Behavior on color { ColorAnimation { duration: 120 } }
                    Behavior on border.color { ColorAnimation { duration: 120 } }

                    Text {
                        anchors.centerIn: parent
                        text: "Remove"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: "#EF4444"
                    }

                    MouseArea {
                        id: nxmRemoveMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (modManager.unregisterNxmProtocol()) {
                                nexusSettingsRoot.nxmRegistered = false
                            }
                        }
                    }

                    ToolTip {
                        id: nxmRemoveTip
                        visible: nxmRemoveMouse.containsMouse
                        delay: 100
                        padding: 10
                        text: "Unregisters Universal Manager from Nexus download links. 'Mod manager download' on the Nexus website will no longer open here until you associate again."

                        contentItem: Text {
                            width: 260
                            text: nxmRemoveTip.text
                            wrapMode: Text.Wrap
                            lineHeight: 1.3
                            color: window.textColor
                            font.pixelSize: 11
                        }

                        background: Rectangle {
                            radius: 8
                            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor
                            border.width: 1
                            border.color: window.borderColor
                        }
                    }
                }
            }
        }
    }
    }

    Text {
        text: "API ACCESS"
        font.pixelSize: 10
        font.weight: Font.DemiBold
        font.letterSpacing: 1.3
        color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.5)
        Layout.topMargin: 6
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: apiRows.implicitHeight + (nexusSettingsRoot.panelTheme ? 24 : 0)
        radius: nexusSettingsRoot.panelTheme ? 12 : 0
        color: nexusSettingsRoot.panelTheme && !nexusSettingsRoot.crimsonTheme ? window.surfaceColor : "transparent"
        border.width: nexusSettingsRoot.panelTheme ? window.borderWidth : 0
        border.color: window.borderColor

        Rectangle {
            anchors.fill: parent
            anchors.margins: parent.border.width
            radius: Math.max(0, parent.radius - parent.border.width)
            visible: nexusSettingsRoot.crimsonTheme
            gradient: crimsonApiGradient
        }

        Gradient {
            id: crimsonApiGradient
            GradientStop { position: 0.0; color: window.crimsonGradientTop }
            GradientStop { position: 1.0; color: window.crimsonGradientBottom }
        }

    ColumnLayout {
        id: apiRows
        anchors.fill: parent
        anchors.margins: nexusSettingsRoot.panelTheme ? 12 : 0
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                text: "Manual API Key"
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: window.textColor
            }

            Rectangle {
                Layout.preferredHeight: 20
                Layout.preferredWidth: legacyLabel.implicitWidth + 16
                radius: 10
                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.07)

                Text {
                    id: legacyLabel
                    anchors.centerIn: parent
                    text: "LEGACY"
                    font.pixelSize: 9
                    font.weight: Font.Bold
                    font.letterSpacing: 0.8
                    color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.5)
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                id: getApiKeyText
                text: "Get your API Key"
                font.pixelSize: 12
                font.weight: Font.Medium
                color: getApiKeyMouse.containsMouse ? Qt.darker(window.accentColor, 1.15) : window.accentColor
                opacity: getApiKeyMouse.containsMouse ? 0.85 : 1

                MouseArea {
                    id: getApiKeyMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        modManager.openNexusUrl("https://www.nexusmods.com/users/myaccount?tab=api")
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: nexusOAuth.authenticated
                ? "You are signed in with your Nexus Mods account, so this key is not used. Sign out above to fall back to a manual key."
                : "Only needed if you prefer not to sign in. The secure account sign-in above is recommended and enables the full Nexus Mods API."
            font.pixelSize: 12
            color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.6)
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            enabled: !nexusOAuth.authenticated
            opacity: nexusOAuth.authenticated ? 0.5 : 1.0

            Rectangle {
                id: apiKeyFieldBox
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                radius: 9
                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, apiKeyField.activeFocus ? 0.07 : 0.04)
                border.width: 1
                border.color: apiKeyField.activeFocus
                    ? window.primaryColor
                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.16)
                Behavior on border.color { ColorAnimation { duration: 150 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 8
                    spacing: 8

                    TextField {
                        id: apiKeyField
                        ContextMenu.menu: AppTextContextMenu {
                            editor: apiKeyField
                        }
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: modManager.nexusApiKey
                        echoMode: nexusSettingsRoot.showApiKey ? TextInput.Normal : TextInput.Password
                        placeholderText: "Paste your Nexus Mods personal API key here..."
                        placeholderTextColor: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.4)
                        color: window.textColor
                        font.pixelSize: 13
                        font.family: "Consolas"
                        verticalAlignment: TextInput.AlignVCenter
                        background: null
                        selectByMouse: true

                        onEditingFinished: {
                            modManager.setNexusApiKey(text)
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 28
                        Layout.preferredHeight: 28
                        radius: 14
                        color: eyeMouse.containsMouse
                            ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.1)
                            : "transparent"

                        Image {
                            id: apiKeyLockIcon
                            anchors.centerIn: parent
                            width: 15
                            height: 15
                            source: "qrc:/UniversalModManager/Lucide Icons/lock.svg"
                            sourceSize: Qt.size(15, 15)
                            smooth: true
                            visible: !nexusSettingsRoot.showApiKey

                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.7)
                            }
                        }

                        Image {
                            id: apiKeyEyeIcon
                            anchors.centerIn: parent
                            width: 15
                            height: 15
                            source: "qrc:/UniversalModManager/Lucide Icons/eye.svg"
                            sourceSize: Qt.size(15, 15)
                            smooth: true
                            visible: nexusSettingsRoot.showApiKey

                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.7)
                            }
                        }

                        MouseArea {
                            id: eyeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: nexusSettingsRoot.showApiKey = !nexusSettingsRoot.showApiKey
                        }
                    }
                }

                Rectangle {
                    id: apiKeySplitLeft
                    height: 2
                    radius: 1
                    width: 0
                    opacity: 0
                    color: window.primaryColor
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 1
                    x: parent.width / 2 - width
                }

                Rectangle {
                    id: apiKeySplitRight
                    height: 2
                    radius: 1
                    width: 0
                    opacity: 0
                    color: window.primaryColor
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 1
                    x: parent.width / 2
                }
            }

            Rectangle {
                Layout.preferredWidth: 140
                Layout.preferredHeight: 42
                radius: 9
                color: validateMouse.containsMouse ? Qt.darker(window.primaryColor, 1.08) : window.primaryColor
                Behavior on color { ColorAnimation { duration: 120 } }

                Text {
                    anchors.centerIn: parent
                    text: modManager.nexusValidationStatus === "checking" ? "Verifying..." : "Test Connection"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: "#FFFFFF"
                }

                MouseArea {
                    id: validateMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        modManager.setNexusApiKey(apiKeyField.text)
                        modManager.validateNexusApiKey()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: modManager.nexusValidationStatus === "valid" ? 112 : 46
            radius: 10
            visible: modManager.nexusValidationStatus !== "idle"
            color: {
                if (modManager.nexusValidationStatus === "valid") return Qt.rgba(0.05, 0.65, 0.35, 0.10)
                if (modManager.nexusValidationStatus === "checking") return Qt.rgba(0.2, 0.5, 0.9, 0.10)
                return Qt.rgba(0.9, 0.25, 0.2, 0.10)
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Image {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        source: modManager.nexusValidationStatus === "valid"
                            ? "qrc:/UniversalModManager/Lucide Icons/circle-check.svg"
                            : (modManager.nexusValidationStatus === "checking"
                                ? "qrc:/UniversalModManager/Lucide Icons/loader.svg"
                                : "qrc:/UniversalModManager/Lucide Icons/triangle-alert.svg")
                        sourceSize: Qt.size(16, 16)
                        smooth: true

                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: modManager.nexusValidationStatus === "valid"
                                ? "#10B981"
                                : (modManager.nexusValidationStatus === "checking" ? "#3B82F6" : "#EF4444")
                        }
                    }

                    Text {
                        text: modManager.nexusValidationMessage
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: window.textColor
                    }

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        visible: modManager.nexusValidationStatus === "valid"
                        Layout.preferredHeight: 24
                        Layout.preferredWidth: premiumBadgeText.implicitWidth + 16
                        radius: 12
                        color: modManager.nexusIsPremium
                            ? "#10B981"
                            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.16)

                        Text {
                            id: premiumBadgeText
                            anchors.centerIn: parent
                            text: modManager.nexusIsPremium ? "Premium Member" : "Standard Member"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            color: modManager.nexusIsPremium
                                ? "#FFFFFF"
                                : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.75)
                        }
                    }
                }

                RowLayout {
                    visible: modManager.nexusValidationStatus === "valid"
                    Layout.fillWidth: true
                    spacing: 24

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Text {
                                text: "Hourly API Limit:"
                                font.pixelSize: 11
                                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.7)
                            }
                            Text {
                                text: modManager.nexusHourlyRemaining + " / " + modManager.nexusHourlyLimit + " left"
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                color: window.textColor
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 6
                            radius: 3
                            color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.15)

                            Rectangle {
                                height: parent.height
                                radius: parent.radius
                                width: modManager.nexusHourlyLimit > 0
                                    ? Math.max(0, Math.min(parent.width, parent.width * (modManager.nexusHourlyRemaining / modManager.nexusHourlyLimit)))
                                    : parent.width
                                color: "#3B82F6"
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Text {
                                text: "Daily API Limit:"
                                font.pixelSize: 11
                                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.7)
                            }
                            Text {
                                text: modManager.nexusDailyRemaining + " / " + modManager.nexusDailyLimit + " left"
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                color: window.textColor
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 6
                            radius: 3
                            color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.15)

                            Rectangle {
                                height: parent.height
                                radius: parent.radius
                                width: modManager.nexusDailyLimit > 0
                                    ? Math.max(0, Math.min(parent.width, parent.width * (modManager.nexusDailyRemaining / modManager.nexusDailyLimit)))
                                    : parent.width
                                color: "#10B981"
                            }
                        }
                    }
                }
            }
        }
    }
    }

    Item { Layout.preferredHeight: 20 }

    component UiverseToggle: Item {
        id: uiverseToggle
        property bool checked: false
        signal toggled(bool checked)

        implicitWidth: 46
        implicitHeight: 24

        readonly property real _circleDiameter: 18
        readonly property real _offset: (height - _circleDiameter) / 2
        readonly property color _accent: window.primaryColor
        readonly property color _offBg: Qt.rgba(_accent.r, _accent.g, _accent.b, modManager.theme === "Cozy" ? 0.35 : 0.25)

        Rectangle {
            id: slider
            anchors.fill: parent
            radius: height / 2
            color: uiverseToggle.checked ? uiverseToggle._accent : uiverseToggle._offBg

            Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutBack } }

            Rectangle {
                width: uiverseToggle._circleDiameter / 2
                height: width / 2 - 1
                radius: 1
                color: "#ffffff"
                anchors.verticalCenter: parent.verticalCenter
                x: uiverseToggle.checked
                   ? (parent.width - uiverseToggle._offset - width - (width / 2))
                   : (uiverseToggle._offset + (width / 2))

                Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }
            }

            Rectangle {
                id: circle
                width: uiverseToggle._circleDiameter
                height: uiverseToggle._circleDiameter
                radius: width / 2
                color: "#ffffff"
                anchors.verticalCenter: parent.verticalCenter
                x: uiverseToggle.checked
                   ? (parent.width - uiverseToggle._offset - width)
                   : uiverseToggle._offset

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(0, 0, 0, 0.32)
                    shadowHorizontalOffset: 1
                    shadowVerticalOffset: 1
                    shadowBlur: 0.45
                }

                Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }

                Text {
                    anchors.centerIn: parent
                    text: "×"
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    color: Qt.rgba(uiverseToggle._accent.r, uiverseToggle._accent.g, uiverseToggle._accent.b, 1.0)
                    scale: uiverseToggle.checked ? 0 : 1
                    opacity: uiverseToggle.checked ? 0 : 1
                    Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: uiverseToggle.toggled(!uiverseToggle.checked)
            }
        }
    }
}
