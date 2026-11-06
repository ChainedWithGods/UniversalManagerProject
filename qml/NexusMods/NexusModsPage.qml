import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Pane {
    id: nexusPageRoot
    padding: 0
    background: Rectangle { color: "transparent" }

    property bool showFilters: false

    readonly property bool hasNexusCredentials: nexusOAuth.authenticated
        || modManager.nexusApiKey.trim().length > 0

    property var displayedMods: []
    property bool catalogExiting: false
    property bool animateCatalogEntry: false

    readonly property color deepPanelColor: {
        if (modManager.theme === "Crimson") return Qt.rgba(0.05, 0.06, 0.10, 0.98)
        if (modManager.theme === "Glassmorphic") return Qt.rgba(0.12, 0.12, 0.12, 0.98)
        if (modManager.theme === "Wavey") return Qt.rgba(0.08, 0.16, 0.28, 0.97)
        return window.surfaceColor
    }

    function applyDisplayedMods() {
        var nextMods = modManager.nexusModsList
        if (!nextMods)
            nextMods = []

        var shouldAnimate = displayedMods.length === 0 && nextMods.length > 0
        animateCatalogEntry = shouldAnimate
        if (shouldAnimate)
            catalogEntryResetTimer.restart()
        else
            catalogEntryResetTimer.stop()

        displayedMods = nextMods
    }

    function handleNexusModsChanged() {
        if (catalogExiting)
            return

        if (modManager.nexusModsList.length === 0
                && displayedMods.length > 0
                && !nexusOAuth.authenticated
                && modManager.nexusApiKey.trim().length === 0) {
            catalogExiting = true
            catalogExitTimer.restart()
            return
        }

        applyDisplayedMods()
    }

    Connections {
        target: modManager
        function onNexusModsListChanged() { nexusPageRoot.handleNexusModsChanged() }
    }

    Connections {
        target: nexusOAuth
        function onAuthenticationChanged() {
            if (!nexusOAuth.authenticated)
                return

            catalogExitTimer.stop()
            nexusPageRoot.catalogExiting = false
            if (nexusPageRoot.displayedMods.length > 0)
                nexusPageRoot.displayedMods = []
        }
    }

    Connections {
        target: downloadManager
        function onDownloadStarted() {
            if (modDetailsDialog.visible)
                modDetailsDialog.minimizeToCorner()
        }
    }

    function minimizeModDetailsWindow() {
        if (modDetailsDialog.visible)
            modDetailsDialog.minimizeToCorner()
    }

    readonly property bool modDetailsMinimized: modDetailsDialog.parked

    function restoreModDetails() {
        modDetailsDialog.restoreFromMinimized()
    }

    function closeParkedModDetails() {
        modDetailsDialog.closeDetails()
    }

    Timer {
        id: catalogExitTimer
        interval: 620
        repeat: false
        onTriggered: {
            nexusPageRoot.displayedMods = []
            nexusPageRoot.catalogExiting = false
        }
    }

    Timer {
        id: catalogEntryResetTimer
        interval: 1400
        repeat: false
        onTriggered: nexusPageRoot.animateCatalogEntry = false
    }

    Component.onCompleted: applyDisplayedMods()

    function formatCount(num) {
        if (!num || num < 0) return "0"
        if (num >= 1000000) return (num / 1000000).toFixed(1) + "M"
        if (num >= 1000) return (num / 1000).toFixed(1) + "k"
        return num.toString()
    }

    function authorColor(name) {
        var colors = ["#e57373", "#f06292", "#ba68c8", "#9575cd", "#7986cb",
                       "#64b5f6", "#4fc3f7", "#4dd0e1", "#4db6ac", "#81c784",
                       "#aed581", "#fff176", "#ffd54f", "#ff8a65"]
        if (!name) return colors[0]
        var hash = 0
        for (var i = 0; i < name.length; i++) {
            hash = name.charCodeAt(i) + ((hash << 5) - hash)
        }
        return colors[Math.abs(hash) % colors.length]
    }

    function formatDate(dateStr) {
        if (!dateStr) return ""
        var date = new Date(dateStr)
        if (isNaN(date.getTime())) return ""
        var now = new Date()
        var diffMs = now - date
        var diffDays = Math.floor(diffMs / (1000 * 60 * 60 * 24))
        if (diffDays === 0) return "Today"
        if (diffDays === 1) return "Yesterday"
        if (diffDays < 7) return diffDays + " days ago"
        if (diffDays < 30) return Math.floor(diffDays / 7) + " weeks ago"
        if (diffDays < 365) return Math.floor(diffDays / 30) + " months ago"
        return Math.floor(diffDays / 365) + " years ago"
    }

    function formatAbsoluteDate(dateStr) {
        if (!dateStr) return ""
        var date = new Date(dateStr)
        if (isNaN(date.getTime())) return ""
        var months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
        var dd = date.getDate()
        var mm = months[date.getMonth()]
        var yyyy = date.getFullYear()
        return (dd < 10 ? "0" + dd : dd) + " " + mm + " " + yyyy
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.bottomMargin: 0
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: parent.width
            spacing: 16

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true

                RowLayout {
                    spacing: 10

                    Text {
                        text: "Nexus Mods"
                        font.pixelSize: 26
                        font.weight: Font.Bold
                        color: window.textColor
                    }

                    Rectangle {

                        visible: nexusOAuth.authenticated || modManager.nexusApiKey.trim().length > 0
                        Layout.preferredHeight: 26
                        Layout.preferredWidth: badgeRow.implicitWidth + 18
                        radius: 13
                        color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                        border.width: 1
                        border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.35)

                        RowLayout {
                            id: badgeRow
                            anchors.centerIn: parent
                            spacing: 6

                            Text {
                                text: (modManager.nexusGameDomain ? modManager.nexusGameDomain.toUpperCase() : "STELLAR BLADE")
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                color: window.primaryColor
                            }

                            Text {
                                visible: modManager.nexusTotalModsCount > 0
                                text: "• " + modManager.nexusTotalModsCount + " Mods"
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.8)
                            }
                        }
                    }
                }

                Text {
                    text: modManager.nexusUsername.length > 0
                        ? ("Signed in as " + modManager.nexusUsername + (modManager.nexusIsPremium ? " · Premium" : " · Member"))
                        : "Discover, install, and update community mods directly inside Universal Manager."
                    font.pixelSize: 13
                    color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.6)
                }
            }

            Item { Layout.fillWidth: true }

            Item {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18

                Image {
                    id: refreshIcon
                    anchors.fill: parent
                    source: "qrc:/UniversalModManager/Lucide Icons/refresh-cw.svg"
                    sourceSize: Qt.size(18, 18)
                    smooth: true
                    opacity: refreshMouse.containsMouse ? 1.0 : 0.8

                    layer.enabled: true
                    layer.smooth: true
                    layer.textureSize: Qt.size(width * 2, height * 2)
                    layer.effect: ColorOverlay {
                        color: window.primaryColor
                    }

                    RotationAnimation {
                        target: refreshIcon
                        from: 0
                        to: 360
                        duration: 800
                        loops: Animation.Infinite
                        running: modManager.nexusIsLoadingMods
                    }
                }

                MouseArea {
                    id: refreshMouse
                    anchors.fill: parent
                    anchors.margins: -10
                    hoverEnabled: true

                    enabled: !modManager.nexusIsLoadingMods
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        modManager.fetchNexusMods(modManager.nexusCurrentPage)
                    }
                }
            }

            NexusGameSelector {
                panelColor: nexusPageRoot.deepPanelColor
                visible: modManager.nexusGamesList.length > 0
            }

            Rectangle {
                visible: !nexusOAuth.authenticated
                Layout.preferredHeight: 42
                Layout.preferredWidth: signInLabel.implicitWidth + 44
                radius: 10
                color: signInMouse.containsMouse
                    ? Qt.darker(window.primaryColor, 1.08)
                    : window.primaryColor

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 8

                    Image {
                        id: signInLockIcon
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                        source: "qrc:/UniversalModManager/Lucide Icons/lock.svg"
                        sourceSize: Qt.size(14, 14)
                        smooth: true

                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: "#FFFFFF"
                        }
                    }

                    Text {
                        id: signInLabel
                        text: nexusOAuth.signingIn ? "Signing in…" : "Sign In"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: "#FFFFFF"
                    }
                }

                MouseArea {
                    id: signInMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: window.openNexusSignIn()
                }
            }

            Rectangle {
                id: nexusAccountButton
                visible: nexusOAuth.authenticated
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                radius: 21
                color: accountButtonMouse.containsMouse
                    ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.16)
                    : "transparent"
                border.width: 2
                border.color: nexusAccountMenu.opened
                    ? window.primaryColor
                    : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.55)

                Image {
                    id: accountButtonAvatar
                    anchors.fill: parent
                    anchors.margins: 2
                    source: nexusOAuth.avatarUrl
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                    layer.enabled: true
                    layer.effect: OpacityMask {
                        maskSource: Rectangle {
                            width: accountButtonAvatar.width
                            height: accountButtonAvatar.height
                            radius: width / 2
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: accountButtonAvatar.status !== Image.Ready
                        && nexusOAuth.userName.length > 0
                    text: nexusOAuth.userName.length > 0
                        ? nexusOAuth.userName.charAt(0).toUpperCase()
                        : ""
                    font.pixelSize: 16
                    font.weight: Font.Bold
                    color: window.primaryColor
                }

                Image {
                    id: accountButtonPlaceholderIcon
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    source: "qrc:/UniversalModManager/Lucide Icons/user.svg"
                    sourceSize: Qt.size(18, 18)
                    smooth: true
                    visible: accountButtonAvatar.status !== Image.Ready
                        && nexusOAuth.userName.length === 0

                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: window.primaryColor
                    }
                }

                Rectangle {
                    visible: nexusOAuth.premium
                    width: 15
                    height: 15
                    radius: 7.5
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    color: "#F59E0B"
                    border.width: 1.5
                    border.color: window.surfaceColor

                    Text {
                        anchors.centerIn: parent
                        text: "★"
                        font.pixelSize: 8
                        color: "#FFFFFF"
                    }
                }

                MouseArea {
                    id: accountButtonMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (nexusAccountMenu.opened)
                            nexusAccountMenu.close()
                        else
                            nexusAccountMenu.open()
                    }
                }
            }
        }

        Popup {
            id: nexusAccountMenu
            parent: nexusAccountButton
            x: nexusAccountButton.width - width
            y: nexusAccountButton.height + 8
            width: 276
            padding: 0
            modal: false
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

            enter: Transition {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 130 }
            }
            exit: Transition {
                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 90 }
            }

            background: Rectangle {
                color: modManager.theme === "Glassmorphic"
                    ? Qt.rgba(0.1, 0.1, 0.1, 0.98)
                    : window.surfaceColor
                radius: 14
                border.width: 1
                border.color: window.borderColor

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(0, 0, 0, 0.35)
                    shadowVerticalOffset: 6
                    shadowBlur: 0.5
                }
            }

            contentItem: ColumnLayout {
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.topMargin: 16
                    Layout.bottomMargin: 14
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 46
                        Layout.preferredHeight: 46
                        radius: 23
                        color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.14)
                        border.width: 1
                        border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.35)

                        Image {
                            id: menuAccountAvatar
                            anchors.fill: parent
                            anchors.margins: 2
                            source: nexusOAuth.avatarUrl
                            fillMode: Image.PreserveAspectCrop
                            visible: status === Image.Ready
                            layer.enabled: true
                            layer.effect: OpacityMask {
                                maskSource: Rectangle {
                                    width: menuAccountAvatar.width
                                    height: menuAccountAvatar.height
                                    radius: width / 2
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: menuAccountAvatar.status !== Image.Ready
                                && nexusOAuth.userName.length > 0
                            text: nexusOAuth.userName.length > 0
                                ? nexusOAuth.userName.charAt(0).toUpperCase()
                                : ""
                            font.pixelSize: 18
                            font.weight: Font.Bold
                            color: window.primaryColor
                        }

                        Image {
                            id: menuAccountPlaceholderIcon
                            anchors.centerIn: parent
                            width: 20
                            height: 20
                            source: "qrc:/UniversalModManager/Lucide Icons/user.svg"
                            sourceSize: Qt.size(20, 20)
                            smooth: true
                            visible: menuAccountAvatar.status !== Image.Ready
                                && nexusOAuth.userName.length === 0

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
                            Layout.fillWidth: true
                            text: nexusOAuth.userName.length > 0
                                ? nexusOAuth.userName
                                : "Nexus Mods member"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: window.textColor
                            elide: Text.ElideRight
                        }

                        Text {
                            text: nexusOAuth.premium
                                ? "Premium member"
                                : (nexusOAuth.supporter ? "Supporter" : "Member")
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            color: nexusOAuth.premium ? "#F59E0B" : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.6)
                        }

                        RowLayout {
                            spacing: 4
                            visible: nexusOAuth.authenticated

                            Image {
                                id: accountShieldMiniIcon
                                Layout.preferredWidth: 10
                                Layout.preferredHeight: 10
                                source: "qrc:/UniversalModManager/Lucide Icons/shield-check.svg"
                                sourceSize: Qt.size(10, 10)
                                smooth: true

                                layer.enabled: true
                                layer.effect: ColorOverlay {
                                    color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.45)
                                }
                            }

                            Text {
                                text: "Signed in securely"
                                font.pixelSize: 10
                                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.45)
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    color: window.borderColor
                    opacity: 0.6
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 6
                    Layout.rightMargin: 6
                    Layout.topMargin: 6
                    Layout.preferredHeight: 42
                    radius: 8
                    color: accountSiteMouse.containsMouse
                        ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.07)
                        : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        Image {
                            id: accountSiteIcon
                            Layout.preferredWidth: 15
                            Layout.preferredHeight: 15
                            source: "qrc:/UniversalModManager/Lucide Icons/user.svg"
                            sourceSize: Qt.size(15, 15)
                            smooth: true

                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.75)
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Nexus Mods account"
                            font.pixelSize: 13
                            color: window.textColor
                        }

                        Image {
                            id: accountExternalIcon
                            Layout.preferredWidth: 13
                            Layout.preferredHeight: 13
                            source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                            sourceSize: Qt.size(13, 13)
                            smooth: true

                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.4)
                            }
                        }
                    }

                    MouseArea {
                        id: accountSiteMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            nexusAccountMenu.close()
                            modManager.openNexusUrl("https://www.nexusmods.com/users/myaccount")
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 6
                    Layout.rightMargin: 6
                    Layout.topMargin: 2
                    Layout.bottomMargin: 6
                    Layout.preferredHeight: 42
                    radius: 8
                    color: signOutMouse.containsMouse
                        ? Qt.rgba(0.94, 0.27, 0.27, 0.12)
                        : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        Image {
                            id: signOutIcon
                            Layout.preferredWidth: 15
                            Layout.preferredHeight: 15
                            source: "qrc:/UniversalModManager/Lucide Icons/log-out.svg"
                            sourceSize: Qt.size(15, 15)
                            smooth: true

                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: "#EF4444"
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Sign Out"
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: "#EF4444"
                        }
                    }

                    MouseArea {
                        id: signOutMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            nexusAccountMenu.close()
                            nexusOAuth.signOut()
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 320
                Layout.preferredHeight: 40
                radius: 10
                color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
                border.width: 1
                border.color: searchTextInput.activeFocus ? window.primaryColor : window.borderColor

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 10
                    spacing: 8

                    Item {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16

                        Image {
                            id: searchIcon
                            anchors.fill: parent
                            source: "qrc:/UniversalModManager/Lucide Icons/search.svg"
                            sourceSize: Qt.size(16, 16)
                            smooth: true
                        }

                        ColorOverlay {
                            anchors.fill: searchIcon
                            source: searchIcon
                            color: window.primaryColor
                        }
                    }

                    TextField {
                        id: searchTextInput
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        placeholderText: "Search mods by title..."
                        placeholderTextColor: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.4)
                        color: window.textColor
                        font.pixelSize: 13
                        verticalAlignment: TextInput.AlignVCenter
                        background: null
                        selectByMouse: true

                        ContextMenu.menu: null

                        onAccepted: {
                            searchTimer.stop()
                            modManager.setNexusSearchQuery(text)
                        }

                        onTextChanged: {
                            searchTimer.restart()
                        }

                        Timer {
                            id: searchTimer
                            interval: 500
                            repeat: false
                            onTriggered: {
                                modManager.setNexusSearchQuery(searchTextInput.text)
                            }
                        }
                    }

                    Rectangle {
                        visible: searchTextInput.text.length > 0
                        width: 20
                        height: 20
                        radius: 10
                        color: clearSearchMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.2) : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            font.pixelSize: 11
                            color: window.textColor
                        }

                        MouseArea {
                            id: clearSearchMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                searchTextInput.text = ""
                                modManager.setNexusSearchQuery("")
                            }
                        }
                    }
                }
            }

            RowLayout {
                spacing: 8

                Repeater {
                    model: [
                        { label: "Most Downloaded", field: "DOWNLOADS" },
                        { label: "Most Endorsed", field: "ENDORSEMENTS" },
                        { label: "Recently Updated", field: "UPDATED_TIME" },
                        { label: "Newest", field: "CREATED_TIME" }
                    ]

                    delegate: Rectangle {
                        required property var modelData
                        property bool isSelected: modManager.nexusSortField === modelData.field
                        Layout.preferredHeight: 36
                        Layout.preferredWidth: sortPillText.implicitWidth + 20
                        radius: 18
                        color: isSelected ? window.primaryColor : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08)
                        border.width: 1
                        border.color: isSelected ? window.primaryColor : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.15)

                        Behavior on color { ColorAnimation { duration: 150 } }

                        Text {
                            id: sortPillText
                            anchors.centerIn: parent
                            text: modelData.label
                            font.pixelSize: 12
                            font.weight: isSelected ? Font.DemiBold : Font.Normal
                            color: isSelected ? "#FFFFFF" : window.textColor
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                modManager.setNexusSortField(modelData.field)
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredHeight: 36
                Layout.preferredWidth: filterBtnRow.implicitWidth + 24
                radius: 18
                color: (nexusPageRoot.showFilters || modManager.nexusFilterActiveCount > 0)
                    ? window.primaryColor
                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08)
                border.width: 1
                border.color: (nexusPageRoot.showFilters || modManager.nexusFilterActiveCount > 0)
                    ? window.primaryColor
                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.15)

                Behavior on color { ColorAnimation { duration: 150 } }

                RowLayout {
                    id: filterBtnRow
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        text: "Filters"
                        font.pixelSize: 12
                        font.weight: (nexusPageRoot.showFilters || modManager.nexusFilterActiveCount > 0) ? Font.DemiBold : Font.Normal
                        color: (nexusPageRoot.showFilters || modManager.nexusFilterActiveCount > 0) ? "#FFFFFF" : window.textColor
                    }

                    Rectangle {
                        visible: modManager.nexusFilterActiveCount > 0
                        width: 18
                        height: 18
                        radius: 9
                        color: (nexusPageRoot.showFilters || modManager.nexusFilterActiveCount > 0) ? "#FFFFFF" : window.primaryColor

                        Text {
                            anchors.centerIn: parent
                            text: modManager.nexusFilterActiveCount.toString()
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            color: (nexusPageRoot.showFilters || modManager.nexusFilterActiveCount > 0) ? window.primaryColor : "#FFFFFF"
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        nexusPageRoot.showFilters = !nexusPageRoot.showFilters
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: nexusPageRoot.showFilters ? 16 : 0

            NexusFilterPanel {
                id: filterSidebar
                page: nexusPageRoot
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

            ColumnLayout {
                id: signedOutState
                anchors.centerIn: parent
                spacing: 16
                visible: !nexusOAuth.authenticated
                    && modManager.nexusApiKey.trim().length === 0
                    && !nexusPageRoot.catalogExiting

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Sign in to Nexus Mods"
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    color: window.textColor
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Use your Nexus Mods account to browse, install, and update mods directly."
                    font.pixelSize: 13
                    color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.6)
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 190
                        Layout.preferredHeight: 40
                        radius: 8
                        color: openSignInMouse.containsMouse
                            ? Qt.darker(window.primaryColor, 1.08)
                            : window.primaryColor

                        Text {
                            anchors.centerIn: parent
                            text: "Sign In with Nexus Mods"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            id: openSignInMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: window.openNexusSignIn()
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 40
                        radius: 8
                        color: useApiKeyMouse.containsMouse
                            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                            : "transparent"
                        border.width: 1
                        border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.55)

                        Text {
                            anchors.centerIn: parent
                            text: "Use API Key"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: window.primaryColor
                        }

                        MouseArea {
                            id: useApiKeyMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: window.openNexusApiKeySettings()
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                visible: modManager.nexusIsLoadingMods && nexusPageRoot.displayedMods.length === 0
                    && (nexusOAuth.authenticated || modManager.nexusApiKey.trim().length > 0)

                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: modManager.nexusIsLoadingMods
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Fetching mods from Nexus GraphQL API..."
                    font.pixelSize: 13
                    color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.6)
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                visible: !modManager.nexusIsLoadingMods && nexusPageRoot.displayedMods.length === 0
                    && (nexusOAuth.authenticated || modManager.nexusApiKey.trim().length > 0)

                Image {
                    id: emptyStateIcon
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    source: "qrc:/UniversalModManager/Lucide Icons/search-x.svg"
                    sourceSize: Qt.size(36, 36)
                    smooth: true

                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: window.primaryColor
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "No mods found matching your criteria"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: window.textColor
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Try adjusting your search terms or sort order."
                    font.pixelSize: 13
                    color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.6)
                }
            }

            GridView {
                id: modsGrid
                anchors.fill: parent
                clip: true
                visible: nexusPageRoot.displayedMods.length > 0
                enabled: !nexusPageRoot.catalogExiting
                interactive: !nexusPageRoot.catalogExiting
                property int columns: 4
                cellWidth: Math.floor(width / columns)
                cellHeight: 460

                cacheBuffer: cellHeight * 4
                model: nexusPageRoot.displayedMods

                property real scrollTarget: 0

                onContentYChanged: {
                    if (!modsScrollAnim.running)
                        scrollTarget = contentY
                }

                onMovementStarted: modsScrollAnim.stop()

                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: (event) => {
                        event.accepted = true
                        const maxY = Math.max(0, modsGrid.contentHeight - modsGrid.height)
                        const targetY = Math.max(0, Math.min(maxY,
                            modsGrid.scrollTarget - event.angleDelta.y * 1.5))
                        if (targetY === modsGrid.scrollTarget)
                            return
                        modsGrid.scrollTarget = targetY
                        const distance = Math.abs(targetY - modsGrid.contentY)
                        modsScrollAnim.stop()
                        modsScrollAnim.to = targetY
                        modsScrollAnim.duration = Math.max(160, Math.min(500, distance * 0.15))
                        modsScrollAnim.start()
                    }
                }

                NumberAnimation {
                    id: modsScrollAnim
                    target: modsGrid
                    property: "contentY"
                    easing.type: Easing.OutQuart
                }

                delegate: Item {
                    id: modDelegate
                    width: modsGrid.cellWidth
                    height: modsGrid.cellHeight

                    required property var modelData
                    required property int index

                    readonly property size thumbSourceSize: Qt.size(768, 460)

                    opacity: nexusPageRoot.animateCatalogEntry ? 0 : 1
                    scale: nexusPageRoot.animateCatalogEntry ? 0.92 : 1
                    transformOrigin: Item.Center

                    property bool revealStarted: false

                    function revealIfVisible() {
                        if (revealStarted || nexusPageRoot.animateCatalogEntry)
                            return
                        const viewTop = modsGrid.contentY
                        const viewBottom = viewTop + modsGrid.height
                        if (y + height > viewTop && y < viewBottom) {
                            revealStarted = true
                            revealAnimation.start()
                        }
                    }

                    Connections {
                        target: modsGrid
                        function onContentYChanged() { modDelegate.revealIfVisible() }
                        function onHeightChanged() { modDelegate.revealIfVisible() }
                    }

                    ParallelAnimation {
                        id: revealAnimation
                        NumberAnimation {
                            target: modDelegate
                            property: "opacity"
                            from: 0
                            to: 1
                            duration: 200
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: modDelegate
                            property: "scale"
                            from: 0.97
                            to: 1
                            duration: 200
                            easing.type: Easing.OutCubic
                        }
                    }

                    transform: Translate {
                        id: entryShift
                        y: nexusPageRoot.animateCatalogEntry ? 18 : 0
                    }

                    Component.onCompleted: {
                        if (nexusPageRoot.animateCatalogEntry) {

                            revealStarted = true
                            entranceAnimation.start()
                        } else {
                            opacity = 0
                            scale = 0.97

                            Qt.callLater(revealIfVisible)
                        }
                    }

                    Connections {
                        target: nexusPageRoot
                        function onCatalogExitingChanged() {
                            if (nexusPageRoot.catalogExiting)
                                exitAnimation.restart()
                        }
                    }

                    SequentialAnimation {
                        id: entranceAnimation
                        PauseAnimation {
                            duration: (index % 4) * 35 + Math.floor(index / 4) * 20
                        }
                        ParallelAnimation {
                            NumberAnimation {
                                target: modDelegate
                                property: "opacity"
                                from: 0
                                to: 1
                                duration: 280
                                easing.type: Easing.OutCubic
                            }
                            NumberAnimation {
                                target: modDelegate
                                property: "scale"
                                from: 0.92
                                to: 1
                                duration: 400
                                easing.type: Easing.OutBack
                            }
                            NumberAnimation {
                                target: entryShift
                                property: "y"
                                from: 18
                                to: 0
                                duration: 330
                                easing.type: Easing.OutCubic
                            }
                        }
                    }

                    SequentialAnimation {
                        id: exitAnimation
                        PauseAnimation {
                            duration: (index % 4) * 32 + Math.floor(index / 4) * 22
                        }
                        ParallelAnimation {
                            NumberAnimation {
                                target: modDelegate
                                property: "opacity"
                                from: 1
                                to: 0
                                duration: 230
                                easing.type: Easing.InCubic
                            }
                            NumberAnimation {
                                target: modDelegate
                                property: "scale"
                                from: 1
                                to: 0.72
                                duration: 260
                                easing.type: Easing.InBack
                            }
                            NumberAnimation {
                                target: modDelegate
                                property: "rotation"
                                from: 0
                                to: index % 2 === 0 ? -4 : 4
                                duration: 260
                                easing.type: Easing.InCubic
                            }
                            NumberAnimation {
                                target: entryShift
                                property: "y"
                                from: 0
                                to: 28
                                duration: 260
                                easing.type: Easing.InCubic
                            }
                        }
                    }

                    Rectangle {
                        id: cardShadow
                        x: modCard.x
                        y: modCard.y
                        width: modCard.width
                        height: modCard.height
                        radius: modCard.radius
                        color: window.surfaceColor
                        visible: modManager.theme !== "Glassmorphic"

                        layer.enabled: true
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowColor: Qt.rgba(0, 0, 0, 0.22)
                            shadowVerticalOffset: 3
                            shadowHorizontalOffset: 0
                            shadowBlur: 0.28
                        }
                    }

                    Rectangle {
                        id: modCard
                        x: 7
                        y: 7
                        width: parent.width - 14
                        height: parent.height - 14

                        radius: 0
                        border.width: window.borderWidth
                        border.color: window.borderColor
                        color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? (cardMouse.containsMouse ? window.glassSurfaceHover : window.surfaceColor) : (cardMouse.containsMouse ? Qt.lighter(window.surfaceColor, 1.03) : window.surfaceColor))

                        scale: cardMouse.containsMouse ? 1.02 : 1
                        transformOrigin: Item.Center

                        Behavior on scale {
                            NumberAnimation {
                                duration: 220
                                easing.type: Easing.OutBack
                                easing.overshoot: 1.4
                            }
                        }

                        Behavior on border.color {
                            ColorAnimation { duration: 150 }
                        }

                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 230
                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.2) : Qt.rgba(0, 0, 0, 0.3)
                                clip: true

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: modDetailsDialog.open(modelData)
                                }

                                Image {
                                    id: modImageBlur
                                    anchors.fill: parent
                                    fillMode: Image.PreserveAspectCrop
                                    source: modelData.pictureUrl || ""
                                    sourceSize: modDelegate.thumbSourceSize
                                    visible: !!modelData.pictureUrl
                                    asynchronous: true
                                    cache: true

                                    opacity: status === Image.Ready ? 1 : 0
                                    Behavior on opacity {
                                        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                                    }

                                    scale: cardMouse.containsMouse ? 1.06 : 1
                                    Behavior on scale {
                                        NumberAnimation { duration: 320; easing.type: Easing.OutCubic }
                                    }

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

                                Image {
                                    id: modImage
                                    anchors.fill: parent
                                    source: modelData.pictureUrl || ""
                                    sourceSize: modDelegate.thumbSourceSize
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    mipmap: true
                                    visible: !!modelData.pictureUrl
                                    asynchronous: true
                                    cache: true

                                    opacity: status === Image.Ready ? 1 : 0
                                    Behavior on opacity {
                                        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                                    }

                                    scale: cardMouse.containsMouse ? 1.06 : 1
                                    Behavior on scale {
                                        NumberAnimation { duration: 320; easing.type: Easing.OutCubic }
                                    }

                                    layer.enabled: !!modelData.adultContent && nexusOAuth.adultContentBlurred
                                    layer.effect: MultiEffect {
                                        blurEnabled: true
                                        blur: 0.65
                                        blurMax: 48
                                    }

                                    onStatusChanged: {

                                        if (status === Image.Ready && implicitWidth > 0 && implicitHeight > 0) {
                                            var imageAspect = implicitWidth / implicitHeight
                                            var containerAspect = parent.width / parent.height
                                            fillMode = Math.abs(imageAspect - containerAspect) > 0.3
                                                ? Image.PreserveAspectFit
                                                : Image.PreserveAspectCrop
                                        }
                                    }
                                }

                                Image {
                                    id: fallbackImageIcon
                                    anchors.centerIn: parent
                                    width: 56
                                    height: 56
                                    source: "qrc:/UniversalModManager/Lucide Icons/image.svg"
                                    sourceSize: Qt.size(56, 56)
                                    smooth: true
                                    visible: false
                                }

                                ColorOverlay {
                                    anchors.fill: fallbackImageIcon
                                    source: fallbackImageIcon
                                    color: window.textSecondaryColor
                                    opacity: 0.2
                                    visible: !modelData.pictureUrl
                                }

                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.right: parent.right
                                    anchors.margins: 8
                                    height: 22
                                    width: versionText.implicitWidth + 14
                                    radius: 11
                                    color: Qt.rgba(0, 0, 0, 0.68)
                                    border.color: Qt.rgba(255, 255, 255, 0.2)
                                    border.width: 1

                                    Text {
                                        id: versionText
                                        anchors.centerIn: parent
                                        text: {
                                            var latestVersion = modManager.nexusLatestFileVersions[modelData.modId]
                                            var v = (latestVersion || modelData.version || "1.0").toString().trim()
                                            return (v.startsWith("v") || v.startsWith("V")) ? v : ("v" + v)
                                        }
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                        color: "#FFFFFF"
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.margins: 12
                                Layout.topMargin: 10
                                spacing: 6

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name || "Unnamed Mod"
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    color: window.textColor
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Item {
                                        Layout.preferredWidth: 14
                                        Layout.preferredHeight: 14

                                        Image {
                                            id: authorIcon
                                            anchors.fill: parent
                                            source: "qrc:/UniversalModManager/Lucide Icons/user.svg"
                                            sourceSize: Qt.size(14, 14)
                                            smooth: true
                                            visible: false
                                        }

                                        ColorOverlay {
                                            anchors.fill: authorIcon
                                            source: authorIcon
                                            color: modManager.theme === "Glassmorphic"
                                                ? window.accentColor
                                                : authorColor(modelData.author)
                                        }
                                    }

                                    Text {
                                        text: modelData.author || "Unknown"
                                        font.pixelSize: 12
                                        color: modManager.theme === "Glassmorphic"
                                            ? window.accentColor
                                            : window.textSecondaryColor
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    RowLayout {
                                        spacing: 3

                                        Item {
                                            Layout.preferredWidth: 13
                                            Layout.preferredHeight: 13

                                            Image {
                                                id: downloadsIcon
                                                anchors.fill: parent
                                                source: "qrc:/UniversalModManager/Lucide Icons/download.svg"
                                                sourceSize: Qt.size(13, 13)
                                                smooth: true
                                                visible: false
                                            }

                                            ColorOverlay {
                                                anchors.fill: downloadsIcon
                                                source: downloadsIcon
                                                color: modManager.theme === "Glassmorphic" ? window.accentColor : window.textSecondaryColor
                                            }
                                        }

                                        Text {
                                            text: formatCount(modelData.downloads || 0)
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                            color: modManager.theme === "Glassmorphic" ? window.accentColor : window.textSecondaryColor
                                        }
                                    }

                                    RowLayout {
                                        spacing: 3

                                        Item {
                                            Layout.preferredWidth: 13
                                            Layout.preferredHeight: 13

                                            Image {
                                                id: endorsementsIcon
                                                anchors.fill: parent
                                                source: "qrc:/UniversalModManager/Lucide Icons/thumbs-up.svg"
                                                sourceSize: Qt.size(13, 13)
                                                smooth: true
                                                visible: false
                                            }

                                            ColorOverlay {
                                                anchors.fill: endorsementsIcon
                                                source: endorsementsIcon
                                                color: modManager.theme === "Glassmorphic" ? window.accentColor : window.textSecondaryColor
                                            }
                                        }

                                        Text {
                                            text: formatCount(modelData.endorsements || 0)
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                            color: modManager.theme === "Glassmorphic" ? window.accentColor : window.textSecondaryColor
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    visible: !!(modelData.category) || modelData.adultContent === true

                                    Text {
                                        text: modelData.category || ""
                                        font.pixelSize: 11
                                        font.weight: Font.DemiBold
                                        color: modManager.theme === "Glassmorphic"
                                            ? window.accentColor
                                            : window.textSecondaryColor
                                        visible: text.length > 0
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: "•"
                                        font.pixelSize: 11
                                        color: modManager.theme === "Glassmorphic"
                                            ? window.accentColor
                                            : window.textSecondaryColor
                                        visible: modelData.adultContent === true && !!(modelData.category)
                                    }

                                    Rectangle {
                                        visible: modelData.adultContent === true
                                        height: 18
                                        radius: 4
                                        color: Qt.rgba(1, 0.2, 0.2, 0.15)
                                        border.color: Qt.rgba(1, 0.2, 0.2, 0.4)
                                        border.width: 1
                                        width: adultTxt.implicitWidth + 8

                                        Text {
                                            id: adultTxt
                                            anchors.centerIn: parent
                                            text: "18+ Adult"
                                            font.pixelSize: 10
                                            font.weight: Font.Bold
                                            color: "#ff4d4d"
                                        }
                                    }

                                    Item { Layout.fillWidth: true }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    visible: !!(modelData.updatedAt || modelData.createdAt)

                                    RowLayout {
                                        spacing: 4

                                        Item {
                                            Layout.preferredWidth: 12
                                            Layout.preferredHeight: 12

                                            Image {
                                                id: clockIcon
                                                anchors.fill: parent
                                                source: "qrc:/UniversalModManager/Lucide Icons/clock.svg"
                                                sourceSize: Qt.size(12, 12)
                                                smooth: true
                                                visible: false
                                            }

                                            ColorOverlay {
                                                anchors.fill: clockIcon
                                                source: clockIcon
                                                color: modManager.theme === "Glassmorphic"
                                                    ? window.accentColor
                                                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.55)
                                            }
                                        }

                                        Text {
                                            text: formatDate(modelData.updatedAt || modelData.createdAt)
                                            font.pixelSize: 11
                                            font.weight: Font.Medium
                                            color: modManager.theme === "Glassmorphic"
                                                ? window.accentColor
                                                : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.6)
                                        }
                                    }

                                    Item { Layout.fillWidth: true }

                                    RowLayout {
                                        spacing: 4

                                        Item {
                                            Layout.preferredWidth: 12
                                            Layout.preferredHeight: 12

                                            Image {
                                                id: calendarIcon
                                                anchors.fill: parent
                                                source: "qrc:/UniversalModManager/Lucide Icons/calendar.svg"
                                                sourceSize: Qt.size(12, 12)
                                                smooth: true
                                                visible: false
                                            }

                                            ColorOverlay {
                                                anchors.fill: calendarIcon
                                                source: calendarIcon
                                                color: modManager.theme === "Glassmorphic"
                                                    ? window.accentColor
                                                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.55)
                                            }
                                        }

                                        Text {
                                            text: formatAbsoluteDate(modelData.updatedAt || modelData.createdAt)
                                            font.pixelSize: 11
                                            font.weight: Font.Medium
                                            color: modManager.theme === "Glassmorphic"
                                                ? window.accentColor
                                                : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.6)
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    text: modelData.summary || "No description provided."
                                    font.pixelSize: 11
                                    color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.65)
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: Math.max(2, Math.floor(height / (font.pixelSize * 1.4)))
                                    elide: Text.ElideRight
                                    clip: true
                                }
                            }
                        }

                        MouseArea {
                            id: cardMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            z: 1
                            onClicked: {
                                modDetailsDialog.open(modelData)
                            }
                        }

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            height: 2
                            width: cardMouse.containsMouse ? parent.width : 0
                            color: window.primaryColor
                            z: 2

                            Behavior on width {
                                NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
                            }
                        }
                    }
                }
            }
        }
    }

    }

    NexusModsWindow {
        id: modDetailsDialog
        appWindow: window
    }

}
