import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

ColumnLayout {
    property alias aesKeyFieldNew: aesKeyFieldNew
    property alias appOptionsCard: appOptionsCard

    Layout.fillWidth: true
    Layout.rightMargin: 10
    Layout.alignment: Qt.AlignTop
    spacing: 18
    visible: generalSectionIndex === 4

    Text {
        text: "Additional Advanced Settings"
        font.pixelSize: 20
        font.weight: 600
        color: window.textColor
    }

    Rectangle {
        id: launchOptionsCard
        Layout.fillWidth: true
        Layout.preferredHeight: launchOptionsContent.implicitHeight + 40
        implicitHeight: Layout.preferredHeight
        color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
        radius: window.cardRadius
        border.width: window.borderWidth
        border.color: window.borderColor

        gradient: modManager.theme === "Crimson" ? crimsonAdvancedCard1Gradient : null

        Gradient {
            id: crimsonAdvancedCard1Gradient
            GradientStop { position: 0.0; color: window.crimsonGradientTop }
            GradientStop { position: 1.0; color: window.crimsonGradientBottom }
        }

        layer.enabled: modManager.theme !== "Glassmorphic"
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.25)
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 2
            shadowBlur: 0.35
        }

        ColumnLayout {
            id: launchOptionsContent
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Text {
                text: "Assets Viewer Launch Options"
                font.pixelSize: 12
                font.weight: Font.DemiBold
                color: window.textSecondaryColor
                opacity: 0.8
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 18
                rowSpacing: 18

                Rectangle {
                    id: aesLaunchCard
                    Layout.fillWidth: true
                    Layout.preferredHeight: settingsPage.launchExpandedIndex === 0 ? 170 : 120
                    Behavior on Layout.preferredHeight { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10) : Qt.rgba(0, 0, 0, 0.03))
                    radius: 16
                    border.width: 1
                    border.color: aesLaunchCard.hovered ? Qt.lighter(window.primaryColor, 1.2) : Qt.alpha(window.borderColor, 0.7)
                    property bool hovered: false

                    gradient: modManager.theme === "Crimson" ? crimsonAesLaunchGradient : null

                    Gradient {
                        id: crimsonAesLaunchGradient
                        GradientStop { position: 0.0; color: window.crimsonGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                    }

                    HoverHandler {
                        acceptedDevices: PointerDevice.Mouse
                        onHoveredChanged: aesLaunchCard.hovered = hovered
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Item {
                            id: aesHeader
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70

                            Column {
                                anchors.centerIn: parent
                                spacing: 10

                                Rectangle {
                                    width: 32
                                    height: 32
                                    x: (parent.width - width) / 2
                                    radius: 0
                                    color: "transparent"
                                    border.width: 0
                                    border.color: "transparent"

                                    Image {
                                        id: aesIcon
                                        anchors.centerIn: parent
                                        width: 32
                                        height: 32
                                        source: modManager.theme === "Glassmorphic"
                                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/AES key.png"
                                                : (modManager.theme === "Crimson"
                                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/AES key.png"
                                                   : (modManager.theme === "Wavey"
                                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/AES key.png"
                                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/AES key.png"))
                                        sourceSize: Qt.size(36, 36)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        opacity: modManager.theme === "Glassmorphic" ? 0.0 : 1.0
                                    }

                                    ColorOverlay {
                                        anchors.fill: aesIcon
                                        source: aesIcon
                                        color: window.primaryColor
                                        visible: modManager.theme === "Glassmorphic"
                                    }
                                }

                                Column {
                                    width: parent.width
                                    spacing: 4

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "AES Encryption"
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: window.textColor
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Enable if your specific game files are encrypted."
                                        font.pixelSize: 11
                                        color: window.textSecondaryColor
                                        opacity: 0.85
                                        wrapMode: Text.WordWrap
                                        horizontalAlignment: Text.AlignHCenter
                                        width: aesLaunchCard.width - 36
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsPage.launchExpandedIndex = settingsPage.launchExpandedIndex === 0 ? -1 : 0
                            }
                        }

                        RowLayout {
                            id: aesControlsRow
                            Layout.fillWidth: true
                            visible: settingsPage.launchExpandedIndex === 0
                            opacity: visible ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 160 } }
                            spacing: 10

                            UiverseToggle {
                                id: aesEnabledSwitchNew
                                Component.onCompleted: {
                                    checked = modManager.aesKey !== ""
                                }
                                Connections {
                                    target: modManager
                                    function onAesKeyChanged() {
                                        if (modManager.aesKey === "") {
                                            aesEnabledSwitchNew.checked = false
                                        }
                                    }
                                }
                                onToggled: (value) => {
                                    checked = value
                                    if (value) {
                                        settingsPage.launchExpandedIndex = 0
                                    }
                                }
                            }

                            Rectangle {
                                id: aesKeyFieldContainer
                                Layout.fillWidth: true
                                Layout.preferredHeight: 34
                                color: "transparent"

                                TextField {
                                    id: aesKeyFieldNew
                                    ContextMenu.menu: null
                                    anchors.fill: parent
                                    placeholderText: "Enter 64-character hex key..."
                                    placeholderTextColor: modManager.theme === "Wavey"
                                        ? Qt.rgba(1, 1, 1, 0.5)
                                        : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.5)
                                    text: modManager.aesKey
                                    font.pixelSize: 12
                                    font.family: "Consolas"
                                    echoMode: TextInput.Normal
                                    verticalAlignment: TextInput.AlignVCenter
                                    selectByMouse: true
                                    persistentSelection: true
                                    enabled: aesEnabledSwitchNew.checked
                                    color: modManager.theme === "Wavey" ? "#ffffff" : window.textColor

                                    onEditingFinished: {
                                        modManager.setAesKey(text)
                                    }

                                    background: Rectangle {
                                        color: {
                                            if (aesKeyFieldNew.activeFocus) {
                                                return modManager.theme === "Glassmorphic" ?
                                                       Qt.rgba(0, 0, 0, 0.25) :
                                                       (modManager.theme === "Wavey" ?
                                                        Qt.rgba(0.12, 0.24, 0.37, 0.7) :
                                                        Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15))
                                            } else {
                                                return modManager.theme === "Crimson"
                                                    ? Qt.rgba(0.06, 0.08, 0.14, 0.95)
                                                    : (modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.18) : (modManager.theme === "Wavey" ? Qt.rgba(0.12, 0.24, 0.37, 0.6) : "#F5F5F5"))
                                            }
                                        }
                                        radius: 10
                                        border.width: aesKeyFieldNew.activeFocus ? 2 : 1
                                        border.color: aesKeyFieldNew.activeFocus ? window.primaryColor : window.borderColor

                                        Behavior on color { ColorAnimation { duration: 150 } }
                                        Behavior on border.color { ColorAnimation { duration: 150 } }
                                        Behavior on border.width { NumberAnimation { duration: 150 } }
                                    }
                                }

                                MouseArea {
                                    z: 1
                                    anchors.fill: parent
                                    acceptedButtons: Qt.RightButton
                                    cursorShape: Qt.IBeamCursor

                                    enabled: aesEnabledSwitchNew.checked
                                    onPressed: function(mouse) {
                                        aesKeyFieldNew.forceActiveFocus()
                                        aesContextMenu.popup(aesKeyFieldContainer, mouse.x, aesKeyFieldContainer.height + 4)
                                        mouse.accepted = true
                                    }
                                    onPressAndHold: function(mouse) {
                                        aesKeyFieldNew.forceActiveFocus()
                                        aesContextMenu.popup(aesKeyFieldContainer, mouse.x, aesKeyFieldContainer.height + 4)
                                        mouse.accepted = true
                                    }
                                }

                                Menu {
                                    id: aesContextMenu
                                    width: 180
                                    padding: 6

                                    background: Rectangle {
                                        color: {
                                            if (modManager.theme === "Crimson") return Qt.rgba(0.05, 0.06, 0.10, 0.98)
                                            if (modManager.theme === "Glassmorphic") return Qt.rgba(0.05, 0.05, 0.07, Math.max(0.88, Math.min(0.97, modManager.glassOpacity + 0.84)))
                                            if (modManager.theme === "Wavey") return Qt.rgba(0.08, 0.16, 0.28, 0.97)
                                            return window.surfaceColor
                                        }
                                        radius: 12
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10)
                                            : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.08) : window.borderColor)

                                        layer.enabled: true
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: Qt.rgba(0, 0, 0, 0.40)
                                            shadowHorizontalOffset: 0
                                            shadowVerticalOffset: 8
                                            shadowBlur: 0.75
                                        }
                                    }

                                    SettingsContextMenuItem {
                                        text: "Undo"
                                        iconName: "undo-2.svg"
                                        enabled: aesKeyFieldNew.canUndo
                                        onTriggered: aesKeyFieldNew.undo()
                                    }
                                    SettingsContextMenuItem {
                                        text: "Redo"
                                        iconName: "redo-2.svg"
                                        enabled: aesKeyFieldNew.canRedo
                                        onTriggered: aesKeyFieldNew.redo()
                                    }
                                    MenuSeparator {}
                                    SettingsContextMenuItem {
                                        text: "Cut"
                                        iconName: "scissors.svg"
                                        enabled: aesKeyFieldNew.selectedText.length > 0
                                        onTriggered: aesKeyFieldNew.cut()
                                    }
                                    SettingsContextMenuItem {
                                        text: "Copy"
                                        iconName: "copy.svg"
                                        enabled: aesKeyFieldNew.selectedText.length > 0
                                        onTriggered: aesKeyFieldNew.copy()
                                    }
                                    SettingsContextMenuItem {
                                        text: "Paste"
                                        iconName: "clipboard.svg"
                                        onTriggered: aesKeyFieldNew.paste()
                                    }
                                    SettingsContextMenuItem {
                                        text: "Delete"
                                        iconName: "trash.svg"
                                        enabled: aesKeyFieldNew.selectedText.length > 0
                                        onTriggered: aesKeyFieldNew.remove(aesKeyFieldNew.selectionStart, aesKeyFieldNew.selectionEnd)
                                    }
                                    MenuSeparator {}
                                    SettingsContextMenuItem {
                                        text: "Select All"
                                        iconName: "text-select.svg"
                                        enabled: aesKeyFieldNew.text.length > 0
                                        onTriggered: aesKeyFieldNew.selectAll()
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: ueLaunchCard
                    Layout.fillWidth: true
                    Layout.preferredHeight: settingsPage.launchExpandedIndex === 1 ? 170 : 120
                    Behavior on Layout.preferredHeight { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10) : Qt.rgba(0, 0, 0, 0.03))
                    radius: 16
                    border.width: 1
                    border.color: ueLaunchCard.hovered ? Qt.lighter(window.primaryColor, 1.2) : Qt.alpha(window.borderColor, 0.7)

                    gradient: modManager.theme === "Crimson" ? crimsonUeLaunchGradient : null

                    Gradient {
                        id: crimsonUeLaunchGradient
                        GradientStop { position: 0.0; color: window.crimsonGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                    }
                    property bool hovered: false

                    HoverHandler {
                        acceptedDevices: PointerDevice.Mouse
                        onHoveredChanged: ueLaunchCard.hovered = hovered
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70

                            Column {
                                anchors.centerIn: parent
                                spacing: 10

                                Rectangle {
                                    width: 32
                                    height: 32
                                    x: (parent.width - width) / 2
                                    radius: 0
                                    color: "transparent"
                                    border.width: 0
                                    border.color: "transparent"

                                    Image {
                                        id: ueIcon
                                        anchors.centerIn: parent
                                        width: 32
                                        height: 32
                                        source: modManager.theme === "Glassmorphic"
                                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Unreal Engine.png"
                                                : (modManager.theme === "Crimson"
                                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Unreal Engine.png"
                                                   : (modManager.theme === "Wavey"
                                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Unreal Engine.png"
                                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Unreal Engine.png"))
                                        sourceSize: Qt.size(36, 36)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        opacity: modManager.theme === "Glassmorphic" ? 0.0 : 1.0
                                    }

                                    ColorOverlay {
                                        anchors.fill: ueIcon
                                        source: ueIcon
                                        color: window.primaryColor
                                        visible: modManager.theme === "Glassmorphic"
                                    }
                                }

                                Column {
                                    width: parent.width
                                    spacing: 4

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Engine Version"
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: window.textColor
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Specify Unreal engine version."
                                        font.pixelSize: 11
                                        color: window.textSecondaryColor
                                        opacity: 0.85
                                        wrapMode: Text.WordWrap
                                        horizontalAlignment: Text.AlignHCenter
                                        width: ueLaunchCard.width - 36
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsPage.launchExpandedIndex = settingsPage.launchExpandedIndex === 1 ? -1 : 1
                            }
                        }

                        RowLayout {
                            id: ueControlsRow
                            Layout.fillWidth: true
                            visible: settingsPage.launchExpandedIndex === 1
                            opacity: visible ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 160 } }

                            Item {
                                id: ueVersionSelector
                                Layout.fillWidth: true
                                Layout.preferredHeight: 34

                                property var standardVersions: [
                                    "UE 6.0", "UE 5.9", "UE 5.8", "UE 5.7", "UE 5.6", "UE 5.5", "UE 5.4", "UE 5.3", "UE 5.2", "UE 5.1", "UE 5.0",
                                    "UE 4.27", "UE 4.26", "UE 4.25", "UE 4.24", "UE 4.23", "UE 4.22", "UE 4.21", "UE 4.20",
                                    "UE 4.19", "UE 4.18", "UE 4.17", "UE 4.16", "UE 4.15",
                                    "UE 4.14", "UE 4.13", "UE 4.12", "UE 4.11", "UE 4.10",
                                    "UE 4.9", "UE 4.8", "UE 4.7", "UE 4.6", "UE 4.5",
                                    "UE 4.4", "UE 4.3", "UE 4.2", "UE 4.1", "UE 4.0"
                                ]

                                property var allVersions: standardVersions.concat(modManager.customGameVersions)
                                property var filteredVersions: []
                                property bool showingCustomGames: false

                                function rebuildFilter() {
                                    var q = searchField.text ? searchField.text.toLowerCase() : ""
                                    var sourceList = showingCustomGames ? modManager.customGameVersions : standardVersions

                                    if (q.length === 0) {
                                        filteredVersions = sourceList
                                        return
                                    }
                                    var out = []
                                    for (var i = 0; i < sourceList.length; i++) {
                                        var v = sourceList[i]
                                        if (v.toLowerCase().indexOf(q) !== -1) {
                                            out.push(v)
                                        }
                                    }
                                    filteredVersions = out
                                }

                                Connections {
                                    target: modManager
                                    function onCustomGameVersionsChanged() {

                                        allVersions = standardVersions.concat(modManager.customGameVersions)
                                        rebuildFilter()
                                    }
                                }

                                Component.onCompleted: {

                                    allVersions = standardVersions.concat(modManager.customGameVersions)

                                    var isCustomGame = modManager.customGameVersions.indexOf(modManager.ueVersion) !== -1
                                    showingCustomGames = isCustomGame

                                    filteredVersions = isCustomGame ? modManager.customGameVersions : standardVersions

                                    if (!modManager.ueVersion || modManager.ueVersion === "") {
                                        modManager.setUeVersion("UE 5.3")
                                    }
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    color: modManager.theme === "Crimson"
                                        ? Qt.rgba(0.06, 0.08, 0.14, 0.95)
                                        : (modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.18) : (modManager.theme === "Wavey" ? Qt.rgba(0.12, 0.24, 0.37, 0.6) : "#F5F5F5"))
                                    radius: 10
                                    border.width: uePopup.visible ? 2 : 1
                                    border.color: uePopup.visible ? window.primaryColor : window.borderColor

                                    Behavior on border.color { ColorAnimation { duration: 150 } }
                                    Behavior on border.width { NumberAnimation { duration: 100 } }

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.left: parent.left
                                        anchors.leftMargin: 12
                                        text: modManager.ueVersion
                                        font.pixelSize: 13
                                        font.weight: Font.Medium
                                        color: modManager.theme === "Crimson" ? "#EAF2FF" : (modManager.theme === "Wavey" ? "#ffffff" : window.textColor)
                                        elide: Text.ElideRight
                                        width: parent.width - 36
                                    }

                                    Canvas {
                                        anchors.right: parent.right
                                        anchors.rightMargin: 12
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 10
                                        height: 6
                                        onPaint: {
                                            var ctx = getContext("2d")
                                            ctx.reset()
                                            ctx.moveTo(0, 0)
                                            ctx.lineTo(width, 0)
                                            ctx.lineTo(width / 2, height)
                                            ctx.closePath()
                                            ctx.fillStyle = modManager.theme === "Crimson" ? "#EAF2FF" : (modManager.theme === "Wavey" ? "#ffffff" : window.textSecondaryColor)
                                            ctx.fill()
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (uePopup.visible) {
                                            uePopup.close()
                                        } else {
                                            searchField.text = ""
                                            ueVersionSelector.rebuildFilter()
                                            uePopup.open()
                                            searchField.forceActiveFocus()
                                        }
                                    }
                                }

                                Popup {
                                    id: uePopup
                                    modal: modManager.theme === "Glassmorphic"
                                    focus: true
                                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                                    x: 0
                                    y: ueVersionSelector.height + 6
                                    width: ueVersionSelector.width
                                    padding: 10

                                    implicitHeight: Math.min(contentItem.implicitHeight + padding * 2, 460)

                                    background: Rectangle {
                                        radius: 12
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10)
                                            : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
                                        gradient: modManager.theme === "Cozy" ? settingsDialogCozyGradient
                                                : modManager.theme === "Crimson" ? settingsDialogCrimsonGradient
                                                : modManager.theme === "Wavey" ? settingsDialogWaveyGradient
                                                : null

                                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

                                        layer.enabled: true
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: Qt.rgba(0, 0, 0, 0.45)
                                            shadowHorizontalOffset: 0
                                            shadowVerticalOffset: 10
                                            shadowBlur: 0.8
                                        }
                                    }

                                    contentItem: ColumnLayout {
                                        spacing: 10

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 6

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 32
                                                radius: 8
                                                color: modManager.theme === "Crimson"
                                                    ? Qt.rgba(0.06, 0.08, 0.14, 0.6)
                                                    : (modManager.theme === "Glassmorphic"
                                                       ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.06)
                                                       : Qt.rgba(0, 0, 0, 0.08))
                                                border.width: 1
                                                border.color: modManager.theme === "Glassmorphic"
                                                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08)
                                                    : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.08) : window.borderColor)

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: 3
                                                    spacing: 3

                                                    Rectangle {
                                                        Layout.fillWidth: true
                                                        Layout.fillHeight: true
                                                        radius: 6
                                                        color: !ueVersionSelector.showingCustomGames
                                                            ? (modManager.theme === "Crimson" ? Qt.rgba(0.15, 0.20, 0.35, 0.9)
                                                               : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.15)
                                                                  : window.primaryColor))
                                                            : "transparent"

                                                        Behavior on color { ColorAnimation { duration: 150 } }

                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: "Engine Versions"
                                                            font.pixelSize: 12
                                                            font.weight: !ueVersionSelector.showingCustomGames ? Font.DemiBold : Font.Normal
                                                            color: !ueVersionSelector.showingCustomGames
                                                                ? (modManager.theme === "Crimson" ? "#EAF2FF"
                                                                   : (modManager.theme === "Glassmorphic" ? modManager.accentColor
                                                                      : (modManager.theme === "Wavey" ? "#ffffff" : "#FFFFFF")))
                                                                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.6)
                                                                   : (modManager.theme === "Glassmorphic" ? "#EAF2FF"
                                                                      : window.textSecondaryColor))
                                                            Behavior on color { ColorAnimation { duration: 150 } }
                                                        }

                                                        MouseArea {
                                                            anchors.fill: parent
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: {
                                                                if (ueVersionSelector.showingCustomGames) {
                                                                    ueVersionSelector.showingCustomGames = false
                                                                    searchField.text = ""
                                                                    ueVersionSelector.rebuildFilter()
                                                                }
                                                            }
                                                        }
                                                    }

                                                    Rectangle {
                                                        Layout.fillWidth: true
                                                        Layout.fillHeight: true
                                                        radius: 6
                                                        color: ueVersionSelector.showingCustomGames
                                                            ? (modManager.theme === "Crimson" ? Qt.rgba(0.15, 0.20, 0.35, 0.9)
                                                               : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.15)
                                                                  : window.primaryColor))
                                                            : "transparent"

                                                        Behavior on color { ColorAnimation { duration: 150 } }

                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: "Custom Games"
                                                            font.pixelSize: 12
                                                            font.weight: ueVersionSelector.showingCustomGames ? Font.DemiBold : Font.Normal
                                                            color: ueVersionSelector.showingCustomGames
                                                                ? (modManager.theme === "Crimson" ? "#EAF2FF"
                                                                   : (modManager.theme === "Glassmorphic" ? modManager.accentColor
                                                                      : (modManager.theme === "Wavey" ? "#ffffff" : "#FFFFFF")))
                                                                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.6)
                                                                   : (modManager.theme === "Glassmorphic" ? "#EAF2FF"
                                                                      : window.textSecondaryColor))
                                                            Behavior on color { ColorAnimation { duration: 150 } }
                                                        }

                                                        MouseArea {
                                                            anchors.fill: parent
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: {
                                                                if (!ueVersionSelector.showingCustomGames) {
                                                                    ueVersionSelector.showingCustomGames = true
                                                                    searchField.text = ""
                                                                    ueVersionSelector.rebuildFilter()
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        Rectangle {
                                            id: engineSearchFieldContainer
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 34
                                            color: "transparent"

                                            TextField {
                                                id: searchField
                                                ContextMenu.menu: null
                                                anchors.fill: parent
                                                placeholderText: ueVersionSelector.showingCustomGames ? "Search custom games..." : "Search UE version..."
                                                color: (modManager.theme === "Crimson" || modManager.theme === "Glassmorphic")
                                                    ? "#EAF2FF"
                                                    : (modManager.theme === "Wavey" ? "#ffffff" : window.textColor)
                                                placeholderTextColor: (modManager.theme === "Crimson" || modManager.theme === "Glassmorphic")
                                                    ? Qt.rgba(1, 1, 1, 0.55)
                                                    : (modManager.theme === "Wavey" ? Qt.rgba(1, 1, 1, 0.5) : window.textSecondaryColor)
                                                selectByMouse: true
                                                persistentSelection: true
                                                onTextChanged: ueVersionSelector.rebuildFilter()

                                                background: Rectangle {
                                                    color: modManager.theme === "Crimson"
                                                        ? Qt.rgba(0.06, 0.08, 0.14, 0.95)
                                                        : (modManager.theme === "Glassmorphic"
                                                           ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08)
                                                           : Qt.darker(window.surfaceColor, 1.03))
                                                    radius: 10
                                                    border.width: searchField.activeFocus ? 2 : 1
                                                    border.color: searchField.activeFocus ? window.primaryColor : window.borderColor
                                                }
                                            }

                                            MouseArea {
                                                z: 1
                                                anchors.fill: parent
                                                acceptedButtons: Qt.RightButton
                                                cursorShape: Qt.IBeamCursor
                                                onPressed: function(mouse) {
                                                    searchField.forceActiveFocus()
                                                    engineSearchContextMenu.popup(engineSearchFieldContainer, mouse.x, engineSearchFieldContainer.height + 4)
                                                    mouse.accepted = true
                                                }
                                                onPressAndHold: function(mouse) {
                                                    searchField.forceActiveFocus()
                                                    engineSearchContextMenu.popup(engineSearchFieldContainer, mouse.x, engineSearchFieldContainer.height + 4)
                                                    mouse.accepted = true
                                                }
                                            }

                                            Menu {
                                                id: engineSearchContextMenu
                                                width: 180
                                                padding: 6

                                                background: Rectangle {
                                                    color: {
                                                        if (modManager.theme === "Crimson") return Qt.rgba(0.05, 0.06, 0.10, 0.98)
                                                        if (modManager.theme === "Glassmorphic") return Qt.rgba(0.05, 0.05, 0.07, Math.max(0.88, Math.min(0.97, modManager.glassOpacity + 0.84)))
                                                        if (modManager.theme === "Wavey") return Qt.rgba(0.08, 0.16, 0.28, 0.97)
                                                        return window.surfaceColor
                                                    }
                                                    radius: 12
                                                    border.width: 1
                                                    border.color: modManager.theme === "Glassmorphic"
                                                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10)
                                                        : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.08) : window.borderColor)

                                                    layer.enabled: true
                                                    layer.effect: MultiEffect {
                                                        shadowEnabled: true
                                                        shadowColor: Qt.rgba(0, 0, 0, 0.40)
                                                        shadowHorizontalOffset: 0
                                                        shadowVerticalOffset: 8
                                                        shadowBlur: 0.75
                                                    }
                                                }

                                                SettingsContextMenuItem {
                                                    text: "Undo"
                                                    iconName: "undo-2.svg"
                                                    enabled: searchField.canUndo
                                                    onTriggered: searchField.undo()
                                                }
                                                SettingsContextMenuItem {
                                                    text: "Redo"
                                                    iconName: "redo-2.svg"
                                                    enabled: searchField.canRedo
                                                    onTriggered: searchField.redo()
                                                }
                                                MenuSeparator {}
                                                SettingsContextMenuItem {
                                                    text: "Cut"
                                                    iconName: "scissors.svg"
                                                    enabled: searchField.selectedText.length > 0
                                                    onTriggered: searchField.cut()
                                                }
                                                SettingsContextMenuItem {
                                                    text: "Copy"
                                                    iconName: "copy.svg"
                                                    enabled: searchField.selectedText.length > 0
                                                    onTriggered: searchField.copy()
                                                }
                                                SettingsContextMenuItem {
                                                    text: "Paste"
                                                    iconName: "clipboard.svg"
                                                    onTriggered: searchField.paste()
                                                }
                                                SettingsContextMenuItem {
                                                    text: "Delete"
                                                    iconName: "trash.svg"
                                                    enabled: searchField.selectedText.length > 0
                                                    onTriggered: searchField.remove(searchField.selectionStart, searchField.selectionEnd)
                                                }
                                                MenuSeparator {}
                                                SettingsContextMenuItem {
                                                    text: "Select All"
                                                    iconName: "text-select.svg"
                                                    enabled: searchField.text.length > 0
                                                    onTriggered: searchField.selectAll()
                                                }
                                            }
                                        }

                                        ListView {
                                            id: ueList
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: Math.min(contentHeight, 360)
                                            clip: true
                                            model: ueVersionSelector.filteredVersions
                                            currentIndex: Math.max(0, ueVersionSelector.allVersions.indexOf(modManager.ueVersion))
                                            boundsBehavior: Flickable.StopAtBounds
                                            flickDeceleration: 2200
                                            maximumFlickVelocity: 12000
                                            cacheBuffer: 480
                                            property real wheelTargetY: contentY

                                            WheelHandler {
                                                id: ueListWheelHandler
                                                target: ueList
                                                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                                onWheel: (event) => {
                                                    var maxScroll = Math.max(0, ueList.contentHeight - ueList.height)
                                                    var delta = 0
                                                    if (event.pixelDelta && event.pixelDelta.y !== 0) {
                                                        delta = event.pixelDelta.y * 1.15
                                                    } else {
                                                        delta = event.angleDelta.y / 120 * Math.max(42, ueList.height * 0.2)
                                                    }

                                                    var baseY = ueListWheelAnimation.running ? ueList.wheelTargetY : ueList.contentY
                                                    ueList.wheelTargetY = Math.max(0, Math.min(maxScroll, baseY - delta))
                                                    ueList.cancelFlick()
                                                    ueListWheelAnimation.to = ueList.wheelTargetY
                                                    ueListWheelAnimation.restart()
                                                    event.accepted = true
                                                }
                                            }

                                            NumberAnimation {
                                                id: ueListWheelAnimation
                                                target: ueList
                                                property: "contentY"
                                                duration: 110
                                                easing.type: Easing.OutCubic
                                            }

                                            ScrollBar.vertical: ScrollBar {
                                                policy: ScrollBar.AsNeeded
                                                interactive: true
                                            }

                                            delegate: ItemDelegate {
                                                width: ueList.width
                                                height: 34
                                                highlighted: (modelData === modManager.ueVersion)

                                                background: Rectangle {
                                                    color: highlighted
                                                        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, modManager.theme === "Crimson" ? 0.25 : 0.15)
                                                        : "transparent"
                                                    radius: 8
                                                }

                                                contentItem: Text {
                                                    text: modelData
                                                    font.pixelSize: 13
                                                    font.weight: highlighted ? Font.DemiBold : Font.Normal
                                                    color: (modManager.theme === "Crimson" || modManager.theme === "Glassmorphic")
                                                        ? (highlighted ? window.primaryColor : "#EAF2FF")
                                                        : (highlighted ? window.primaryColor : (modManager.theme === "Wavey" ? "#ffffff" : window.textColor))
                                                    verticalAlignment: Text.AlignVCenter
                                                    leftPadding: 10
                                                    elide: Text.ElideRight
                                                }

                                                onClicked: {
                                                    modManager.setUeVersion(modelData)
                                                    uePopup.close()
                                                }
                                            }
                                        }
                                    }

                                    onClosed: {
                                        searchField.text = ""
                                        ueVersionSelector.filteredVersions = ueVersionSelector.allVersions
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: consoleLaunchCard
                    Layout.fillWidth: true
                    Layout.preferredHeight: settingsPage.launchExpandedIndex === 2 ? 160 : 120
                    Behavior on Layout.preferredHeight { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10) : Qt.rgba(0, 0, 0, 0.03))
                    radius: 16
                    border.width: 1

                    gradient: modManager.theme === "Crimson" ? crimsonConsoleLaunchGradient : null

                    Gradient {
                        id: crimsonConsoleLaunchGradient
                        GradientStop { position: 0.0; color: window.crimsonGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                    }
                    border.color: consoleLaunchCard.hovered ? Qt.lighter(window.primaryColor, 1.2) : Qt.alpha(window.borderColor, 0.7)
                    property bool hovered: false

                    HoverHandler {
                        acceptedDevices: PointerDevice.Mouse
                        onHoveredChanged: consoleLaunchCard.hovered = hovered
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70

                            Column {
                                anchors.centerIn: parent
                                spacing: 10

                                Rectangle {
                                    width: 32
                                    height: 32
                                    x: (parent.width - width) / 2
                                    radius: 0
                                    color: "transparent"
                                    border.width: 0
                                    border.color: "transparent"

                                    Image {
                                        id: consoleIcon
                                        anchors.centerIn: parent
                                        width: 32
                                        height: 32
                                        source: modManager.theme === "Glassmorphic"
                                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/console.png"
                                                : (modManager.theme === "Crimson"
                                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/console.png"
                                                   : (modManager.theme === "Wavey"
                                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/console.png"
                                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/console.png"))
                                        sourceSize: Qt.size(36, 36)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        opacity: modManager.theme === "Glassmorphic" ? 0.0 : 1.0
                                    }

                                    ColorOverlay {
                                        anchors.fill: consoleIcon
                                        source: consoleIcon
                                        color: window.primaryColor
                                        visible: modManager.theme === "Glassmorphic"
                                    }
                                }

                                Column {
                                    width: parent.width
                                    spacing: 4

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Hide Console"
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: window.textColor
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Suppress Assets Viewer debug console window on launch."
                                        font.pixelSize: 11
                                        color: window.textSecondaryColor
                                        opacity: 0.85
                                        wrapMode: Text.WordWrap
                                        horizontalAlignment: Text.AlignHCenter
                                        width: consoleLaunchCard.width - 36
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsPage.launchExpandedIndex = settingsPage.launchExpandedIndex === 2 ? -1 : 2
                            }
                        }

                        RowLayout {
                            id: consoleControlsRow
                            Layout.fillWidth: true
                            visible: settingsPage.launchExpandedIndex === 2
                            opacity: visible ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 160 } }

                            Item { Layout.fillWidth: true }

                            UiverseToggle {
                                checked: modManager.pakViewerHideConsole
                                onToggled: (value) => modManager.setPakViewerHideConsole(value)
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                Rectangle {
                    id: minimalLaunchCard
                    Layout.fillWidth: true
                    Layout.preferredHeight: settingsPage.launchExpandedIndex === 3 ? 160 : 120
                    Behavior on Layout.preferredHeight { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10) : Qt.rgba(0, 0, 0, 0.03))
                    radius: 16
                    border.width: 1

                    gradient: modManager.theme === "Crimson" ? crimsonMinimalLaunchGradient : null

                    Gradient {
                        id: crimsonMinimalLaunchGradient
                        GradientStop { position: 0.0; color: window.crimsonGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                    }
                    border.color: minimalLaunchCard.hovered ? Qt.lighter(window.primaryColor, 1.2) : Qt.alpha(window.borderColor, 0.7)
                    property bool hovered: false

                    HoverHandler {
                        acceptedDevices: PointerDevice.Mouse
                        onHoveredChanged: minimalLaunchCard.hovered = hovered
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70

                            Column {
                                anchors.centerIn: parent
                                spacing: 10

                                Rectangle {
                                    width: 32
                                    height: 32
                                    x: (parent.width - width) / 2
                                    radius: 0
                                    color: "transparent"
                                    border.width: 0
                                    border.color: "transparent"

                                    Image {
                                        id: minimalModeIcon
                                        anchors.centerIn: parent
                                        width: 32
                                        height: 32
                                        source: modManager.theme === "Glassmorphic"
                                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/minimal mode.png"
                                                : (modManager.theme === "Crimson"
                                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/minimal mode.png"
                                                   : (modManager.theme === "Wavey"
                                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/minimal mode.png"
                                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/minimal mode.png"))
                                        sourceSize: Qt.size(36, 36)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        opacity: modManager.theme === "Glassmorphic" ? 0.0 : 1.0
                                    }

                                    ColorOverlay {
                                        anchors.fill: minimalModeIcon
                                        source: minimalModeIcon
                                        color: window.primaryColor
                                        visible: modManager.theme === "Glassmorphic"
                                    }
                                }

                                Column {
                                    width: parent.width
                                    spacing: 4

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Categorized System"
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: window.textColor
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Start AssetViewer in categorized system."
                                        font.pixelSize: 11
                                        color: window.textSecondaryColor
                                        opacity: 0.85
                                        wrapMode: Text.WordWrap
                                        horizontalAlignment: Text.AlignHCenter
                                        width: minimalLaunchCard.width - 36
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsPage.launchExpandedIndex = settingsPage.launchExpandedIndex === 3 ? -1 : 3
                            }
                        }

                        RowLayout {
                            id: minimalControlsRow
                            Layout.fillWidth: true
                            visible: settingsPage.launchExpandedIndex === 3
                            opacity: visible ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 160 } }

                            Item { Layout.fillWidth: true }

                            UiverseToggle {
                                checked: modManager.pakViewerHideUI
                                onToggled: (value) => modManager.setPakViewerHideUI(value)
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: appOptionsCard
        Layout.fillWidth: true
        Layout.preferredHeight: appOptionsContent.implicitHeight + 40
        implicitHeight: Layout.preferredHeight

        property int appExpandedIndex: -1

        color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
        radius: window.cardRadius
        border.width: window.borderWidth
        border.color: window.borderColor

        gradient: modManager.theme === "Crimson" ? crimsonAppOptionsCardGradient : null

        Gradient {
            id: crimsonAppOptionsCardGradient
            GradientStop { position: 0.0; color: window.crimsonGradientTop }
            GradientStop { position: 1.0; color: window.crimsonGradientBottom }
        }

        layer.enabled: modManager.theme !== "Glassmorphic"
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.25)
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 2
            shadowBlur: 0.35
        }

        ColumnLayout {
            id: appOptionsContent
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Text {
                text: "App Behavior"
                font.pixelSize: 12
                font.weight: Font.DemiBold
                color: window.textSecondaryColor
                opacity: 0.8
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 18
                rowSpacing: 18

                Rectangle {
                    id: autoHideNavCardNew
                    Layout.fillWidth: true
                    Layout.preferredHeight: appOptionsCard.appExpandedIndex === 0 ? 190 : 120
                    Behavior on Layout.preferredHeight { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10) : Qt.rgba(0, 0, 0, 0.03))
                    radius: 16
                    border.width: 1
                    border.color: autoHideNavCardNew.hovered ? Qt.lighter(window.primaryColor, 1.2) : Qt.alpha(window.borderColor, 0.7)

                    gradient: modManager.theme === "Crimson" ? crimsonAutoHideNavGradient : null

                    Gradient {
                        id: crimsonAutoHideNavGradient
                        GradientStop { position: 0.0; color: window.crimsonGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                    }
                    property bool hovered: false

                    HoverHandler {
                        acceptedDevices: PointerDevice.Mouse
                        onHoveredChanged: autoHideNavCardNew.hovered = hovered
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70

                            Column {
                                anchors.centerIn: parent
                                spacing: 10

                                Rectangle {
                                    width: 32
                                    height: 32
                                    x: (parent.width - width) / 2
                                    radius: 0
                                    color: "transparent"
                                    border.width: 0
                                    border.color: "transparent"

                                    Image {
                                        id: autoHideIcon
                                        anchors.centerIn: parent
                                        width: 32
                                        height: 32
                                        source: modManager.theme === "Glassmorphic"
                                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/auto-hide.png"
                                                : (modManager.theme === "Crimson"
                                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/auto-hide.png"
                                                   : (modManager.theme === "Wavey"
                                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/auto-hide.png"
                                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/auto-hide.png"))
                                        sourceSize: Qt.size(36, 36)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        opacity: modManager.theme === "Glassmorphic" ? 0.0 : 1.0
                                    }

                                    ColorOverlay {
                                        anchors.fill: autoHideIcon
                                        source: autoHideIcon
                                        color: window.primaryColor
                                        visible: modManager.theme === "Glassmorphic"
                                    }
                                }

                                Column {
                                    width: parent.width
                                    spacing: 4

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Auto-Hide Navigation bar"
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: window.textColor
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Hide the navigation bar after inactivity."
                                        font.pixelSize: 11
                                        color: window.textSecondaryColor
                                        opacity: 0.85
                                        wrapMode: Text.WordWrap
                                        horizontalAlignment: Text.AlignHCenter
                                        width: autoHideNavCardNew.width - 36
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: appOptionsCard.appExpandedIndex = appOptionsCard.appExpandedIndex === 0 ? -1 : 0
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: appOptionsCard.appExpandedIndex === 0
                            opacity: visible ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 160 } }

                            Item { Layout.fillWidth: true }

                            UiverseToggle {
                                id: bottomNavAutoHideSwitchNew
                                checked: modManager.autoHideBottomNav
                                onToggled: (value) => modManager.setAutoHideBottomNav(value)
                            }

                            Item { Layout.fillWidth: true }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: appOptionsCard.appExpandedIndex === 0 && bottomNavAutoHideSwitchNew.checked
                            opacity: visible ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 160 } }
                            spacing: 12

                            Rectangle {
                                Layout.preferredWidth: 44
                                Layout.preferredHeight: 24
                                radius: 12
                                color: modManager.theme === "Glassmorphic"
                                    ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                                    : Qt.rgba(0, 0, 0, 0.06)
                                border.width: 1
                                border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.45)

                                Text {
                                    anchors.centerIn: parent
                                    text: modManager.bottomNavAutoHideDelaySeconds + "s"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: window.textColor
                                }
                            }

                            Slider {
                                id: bottomNavDelaySlider
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                from: 1
                                to: 30
                                stepSize: 1
                                snapMode: Slider.SnapAlways
                                value: modManager.bottomNavAutoHideDelaySeconds
                                onMoved: modManager.setBottomNavAutoHideDelaySeconds(Math.round(value))

                                background: Item {
                                    implicitHeight: 28

                                    Rectangle {
                                        x: bottomNavDelaySlider.leftPadding
                                        y: (parent.height - 6) / 2
                                        width: bottomNavDelaySlider.availableWidth
                                        height: 6
                                        radius: 999
                                        color: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.12)
                                            : Qt.rgba(0, 0, 0, 0.10)
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10)
                                            : Qt.rgba(0, 0, 0, 0.12)
                                    }

                                    Rectangle {
                                        x: bottomNavDelaySlider.leftPadding
                                        y: (parent.height - 6) / 2
                                        width: bottomNavDelaySlider.visualPosition * bottomNavDelaySlider.availableWidth
                                        height: 6
                                        radius: 999
                                        color: window.primaryColor

                                        Behavior on width { NumberAnimation { duration: 90 } }
                                    }
                                }

                                handle: Rectangle {
                                    x: bottomNavDelaySlider.leftPadding + bottomNavDelaySlider.visualPosition * (bottomNavDelaySlider.availableWidth - width)
                                    y: bottomNavDelaySlider.topPadding + (bottomNavDelaySlider.availableHeight - height) / 2
                                    implicitWidth: 18
                                    implicitHeight: 18
                                    radius: 9
                                    color: "#ffffff"
                                    border.width: 2
                                    border.color: window.primaryColor

                                    layer.enabled: true
                                    layer.effect: DropShadow {
                                        horizontalOffset: 0
                                        verticalOffset: 2
                                        radius: 8
                                        samples: 16
                                        color: Qt.rgba(0, 0, 0, 0.30)
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: adminPrivilegesCardNew
                    Layout.fillWidth: true
                    Layout.preferredHeight: appOptionsCard.appExpandedIndex === 1 ? 180 : 120
                    Behavior on Layout.preferredHeight { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10) : Qt.rgba(0, 0, 0, 0.03))
                    radius: 16
                    border.width: 1
                    border.color: adminPrivilegesCardNew.hovered ? Qt.lighter(window.primaryColor, 1.2) : Qt.alpha(window.borderColor, 0.7)
                    property bool hovered: false

                    gradient: modManager.theme === "Crimson" ? crimsonAdminPrivilegesGradient : null

                    Gradient {
                        id: crimsonAdminPrivilegesGradient
                        GradientStop { position: 0.0; color: window.crimsonGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                    }

                    HoverHandler {
                        acceptedDevices: PointerDevice.Mouse
                        onHoveredChanged: adminPrivilegesCardNew.hovered = hovered
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70

                            Column {
                                anchors.centerIn: parent
                                spacing: 10

                                Rectangle {
                                    width: 32
                                    height: 32
                                    x: (parent.width - width) / 2
                                    radius: 0
                                    color: "transparent"

                                    Image {
                                        id: adminIcon
                                        anchors.centerIn: parent
                                        source: modManager.theme === "Glassmorphic"
                                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Admin.png"
                                                : (modManager.theme === "Crimson"
                                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Admin.png"
                                                   : (modManager.theme === "Wavey"
                                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Lock.png"
                                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Admin.png"))
                                        width: 32
                                        height: 32
                                        sourceSize: Qt.size(32, 32)
                                        visible: modManager.theme !== "Glassmorphic"
                                    }

                                    ColorOverlay {
                                        anchors.fill: adminIcon
                                        source: adminIcon
                                        color: window.primaryColor
                                        visible: modManager.theme === "Glassmorphic"
                                    }
                                }

                                Column {
                                    width: parent.width
                                    spacing: 4

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Administrator Mode"
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: window.textColor
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modManager.isRunningAsAdmin ? "Currently running as administrator" : "Enable if games are in C:\\Program Files or C:\\Program Files (x86) and mods fail to be deleted or enabled/disabled. Also required for Avalonium to work on games in the C: drive."
                                        font.pixelSize: 11
                                        color: window.textSecondaryColor
                                        opacity: 0.85
                                        wrapMode: Text.WordWrap
                                        horizontalAlignment: Text.AlignHCenter
                                        width: adminPrivilegesCardNew.width - 36
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: appOptionsCard.appExpandedIndex = appOptionsCard.appExpandedIndex === 1 ? -1 : 1
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: appOptionsCard.appExpandedIndex === 1
                            opacity: visible ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 160 } }
                            spacing: 10

                            Item { Layout.fillWidth: true }

                            UiverseToggle {
                                id: adminToggle
                                checked: modManager.runAsAdministrator || modManager.isRunningAsAdmin
                                onToggled: (value) => {
                                    modManager.setRunAsAdministrator(value)
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: modManager.isRunningAsAdmin ? "Restart required to apply" : (modManager.runAsAdministrator ? "App will restart with admin" : "Running as normal user")
                            font.pixelSize: 9
                            color: window.textSecondaryColor
                            opacity: 0.7
                            visible: appOptionsCard.appExpandedIndex === 1
                        }
                    }
                }

                Rectangle {
                    id: notificationsCardNew
                    Layout.fillWidth: true
                    Layout.preferredHeight: appOptionsCard.appExpandedIndex === 2 ? 160 : 120
                    Behavior on Layout.preferredHeight { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10) : Qt.rgba(0, 0, 0, 0.03))
                    radius: 16
                    border.width: 1
                    border.color: notificationsCardNew.hovered ? Qt.lighter(window.primaryColor, 1.2) : Qt.alpha(window.borderColor, 0.7)
                    property bool hovered: false

                    gradient: modManager.theme === "Crimson" ? crimsonNotificationsGradient : null

                    Gradient {
                        id: crimsonNotificationsGradient
                        GradientStop { position: 0.0; color: window.crimsonGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                    }

                    HoverHandler {
                        acceptedDevices: PointerDevice.Mouse
                        onHoveredChanged: notificationsCardNew.hovered = hovered
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70

                            Column {
                                anchors.centerIn: parent
                                spacing: 10

                                Rectangle {
                                    width: 32
                                    height: 32
                                    x: (parent.width - width) / 2
                                    radius: 0
                                    color: "transparent"
                                    border.width: 0

                                    Image {
                                        id: notificationsIcon
                                        anchors.centerIn: parent
                                        width: 32
                                        height: 32
                                        source: modManager.theme === "Glassmorphic"
                                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Notifications.png"
                                                : (modManager.theme === "Crimson"
                                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Notifications.png"
                                                   : (modManager.theme === "Wavey"
                                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Notifications.png"
                                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Notifications.png"))
                                        sourceSize: Qt.size(36, 36)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        visible: modManager.theme !== "Glassmorphic"
                                    }

                                    ColorOverlay {
                                        anchors.fill: notificationsIcon
                                        source: notificationsIcon
                                        color: window.primaryColor
                                        visible: modManager.theme === "Glassmorphic"
                                    }
                                }

                                Column {
                                    width: parent.width
                                    spacing: 4

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Notifications & Feedback"
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: window.textColor
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Show toast notifications."
                                        font.pixelSize: 11
                                        color: window.textSecondaryColor
                                        opacity: 0.85
                                        wrapMode: Text.WordWrap
                                        horizontalAlignment: Text.AlignHCenter
                                        width: notificationsCardNew.width - 36
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: appOptionsCard.appExpandedIndex = appOptionsCard.appExpandedIndex === 2 ? -1 : 2
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: appOptionsCard.appExpandedIndex === 2
                            opacity: visible ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 160 } }

                            Item { Layout.fillWidth: true }

                            UiverseToggle {
                                checked: modManager.notificationsEnabled
                                onToggled: (value) => modManager.setNotificationsEnabled(value)
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                Rectangle {
                    id: minimizeToTrayCardNew
                    Layout.fillWidth: true
                    Layout.preferredHeight: appOptionsCard.appExpandedIndex === 3 ? 160 : 120
                    Behavior on Layout.preferredHeight { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10) : Qt.rgba(0, 0, 0, 0.03))
                    radius: 16
                    border.width: 1
                    border.color: minimizeToTrayCardNew.hovered ? Qt.lighter(window.primaryColor, 1.2) : Qt.alpha(window.borderColor, 0.7)
                    property bool hovered: false

                    gradient: modManager.theme === "Crimson" ? crimsonMinimizeTrayGradient : null

                    Gradient {
                        id: crimsonMinimizeTrayGradient
                        GradientStop { position: 0.0; color: window.crimsonGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                    }

                    HoverHandler {
                        acceptedDevices: PointerDevice.Mouse
                        onHoveredChanged: minimizeToTrayCardNew.hovered = hovered
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70

                            Column {
                                anchors.centerIn: parent
                                spacing: 10

                                Rectangle {
                                    width: 32
                                    height: 32
                                    x: (parent.width - width) / 2
                                    radius: 0
                                    color: "transparent"
                                    border.width: 0

                                    Image {
                                        id: minimizeTrayIcon
                                        anchors.centerIn: parent
                                        width: 32
                                        height: 32
                                        source: modManager.theme === "Glassmorphic"
                                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Minimize.png"
                                                : (modManager.theme === "Crimson"
                                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Minimize.png"
                                                   : (modManager.theme === "Wavey"
                                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Minimize.png"
                                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Minimize.png"))
                                        sourceSize: Qt.size(36, 36)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        visible: modManager.theme !== "Glassmorphic"
                                    }

                                    ColorOverlay {
                                        anchors.fill: minimizeTrayIcon
                                        source: minimizeTrayIcon
                                        color: window.primaryColor
                                        visible: modManager.theme === "Glassmorphic"
                                    }
                                }

                                Column {
                                    width: parent.width
                                    spacing: 4

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Minimize to System Tray"
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: window.textColor
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Hide the Manager when window is closed."
                                        font.pixelSize: 11
                                        color: window.textSecondaryColor
                                        opacity: 0.85
                                        wrapMode: Text.WordWrap
                                        horizontalAlignment: Text.AlignHCenter
                                        width: minimizeToTrayCardNew.width - 36
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var wasExpanded = appOptionsCard.appExpandedIndex === 3;
                                    appOptionsCard.appExpandedIndex = wasExpanded ? -1 : 3;
                                    if (!wasExpanded) {

                                        scrollTimer.cardToScroll = minimizeToTrayCardNew;
                                        scrollTimer.restart();
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: appOptionsCard.appExpandedIndex === 3
                            opacity: visible ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 160 } }

                            Item { Layout.fillWidth: true }

                            UiverseToggle {
                                checked: modManager.minimizeToTray
                                onToggled: (value) => modManager.setMinimizeToTray(value)
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                Rectangle {
                    id: managerConsoleCardNew
                    Layout.fillWidth: true
                    Layout.preferredHeight: appOptionsCard.appExpandedIndex === 4 ? 160 : 120
                    Behavior on Layout.preferredHeight { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10) : Qt.rgba(0, 0, 0, 0.03))
                    radius: 16
                    border.width: 1
                    border.color: managerConsoleCardNew.hovered ? Qt.lighter(window.primaryColor, 1.2) : Qt.alpha(window.borderColor, 0.7)
                    property bool hovered: false

                    gradient: modManager.theme === "Crimson" ? crimsonManagerConsoleGradient : null

                    Gradient {
                        id: crimsonManagerConsoleGradient
                        GradientStop { position: 0.0; color: window.crimsonGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                    }

                    HoverHandler {
                        acceptedDevices: PointerDevice.Mouse
                        onHoveredChanged: managerConsoleCardNew.hovered = hovered
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70

                            Column {
                                anchors.centerIn: parent
                                spacing: 10

                                Rectangle {
                                    width: 32
                                    height: 32
                                    x: (parent.width - width) / 2
                                    radius: 0
                                    color: "transparent"
                                    border.width: 0

                                    Image {
                                        id: managerConsoleIcon
                                        anchors.centerIn: parent
                                        width: 32
                                        height: 32
                                        source: modManager.theme === "Glassmorphic"
                                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/console.png"
                                                : (modManager.theme === "Crimson"
                                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/console.png"
                                                   : (modManager.theme === "Wavey"
                                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/console.png"
                                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/console.png"))
                                        sourceSize: Qt.size(36, 36)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        opacity: modManager.theme === "Glassmorphic" ? 0.0 : 1.0
                                    }

                                    ColorOverlay {
                                        anchors.fill: managerConsoleIcon
                                        source: managerConsoleIcon
                                        color: window.primaryColor
                                        visible: modManager.theme === "Glassmorphic"
                                    }
                                }

                                Column {
                                    width: parent.width
                                    spacing: 4

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Universal Manager console"
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        color: window.textColor
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Show the Universal Manager debug console window on launch."
                                        font.pixelSize: 11
                                        color: window.textSecondaryColor
                                        opacity: 0.85
                                        wrapMode: Text.WordWrap
                                        horizontalAlignment: Text.AlignHCenter
                                        width: managerConsoleCardNew.width - 36
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var wasExpanded = appOptionsCard.appExpandedIndex === 4;
                                    appOptionsCard.appExpandedIndex = wasExpanded ? -1 : 4;
                                    if (!wasExpanded) {

                                        scrollTimer.cardToScroll = managerConsoleCardNew;
                                        scrollTimer.restart();
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: appOptionsCard.appExpandedIndex === 4
                            opacity: visible ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 160 } }

                            Item { Layout.fillWidth: true }

                            UiverseToggle {
                                checked: !modManager.managerHideConsole
                                onToggled: (value) => {
                                    modManager.setManagerHideConsole(!value)
                                    managerConsoleRestartDialog.open()
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
        }
    }

    component UiverseToggle : Item {
        id: uiverseToggle
        property bool checked: false
        signal toggled(bool checked)

        width: 46
        height: 24

        readonly property real _circleDiameter: 18
        readonly property real _offset: (height - _circleDiameter) / 2
        readonly property color _accent: window.primaryColor
        readonly property color _offBg: Qt.rgba(_accent.r, _accent.g, _accent.b,
                                               modManager.theme === "Cozy" ? 0.35 : 0.25)

        Rectangle {
            id: slider
            anchors.fill: parent
            radius: height / 2
            color: uiverseToggle.checked ? uiverseToggle._accent : uiverseToggle._offBg

            Behavior on color {
                ColorAnimation { duration: 200; easing.type: Easing.OutBack }
            }

            Rectangle {
                id: effectLine
                width: _circleDiameter / 2
                height: width / 2 - 1
                radius: 1
                color: "#ffffff"
                anchors.verticalCenter: parent.verticalCenter
                x: uiverseToggle.checked
                   ? (parent.width - uiverseToggle._offset - width - (width / 2))
                   : (uiverseToggle._offset + (width / 2))

                Behavior on x {
                    NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
                }
            }

            Rectangle {
                id: circle
                width: _circleDiameter
                height: _circleDiameter
                radius: width / 2
                color: "#ffffff"
                anchors.verticalCenter: parent.verticalCenter
                x: uiverseToggle.checked
                   ? (parent.width - uiverseToggle._offset - width)
                   : uiverseToggle._offset

                layer.enabled: true
                layer.effect: DropShadow {
                    horizontalOffset: 1
                    verticalOffset: 1
                    radius: 6
                    samples: 32
                    color: "#50000000"
                }

                Behavior on x {
                    NumberAnimation { duration: 200; easing.type: Easing.OutBack }
                }

                Item {
                    anchors.centerIn: parent
                    width: 12
                    height: 12

                    Text {
                        id: crossIcon
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

                    Text {
                        id: checkIcon
                        anchors.centerIn: parent
                        text: ""
                        font.pixelSize: 11
                        color: "#ffffff"
                        scale: uiverseToggle.checked ? 1 : 0
                        opacity: uiverseToggle.checked ? 1 : 0
                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                        Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                uiverseToggle.toggled(!uiverseToggle.checked)
            }
        }
    }
    }
