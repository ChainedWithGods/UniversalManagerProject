import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects
import QtQuick.Dialogs

Item {
    id: contentPart
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 0
            anchors.bottomMargin: 24
            spacing: 50

            ColumnLayout {
                id: mainContent
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 25

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 20

                    RowLayout {
                        spacing: 12

                        Text {
                            text: "All Profiles"
                            font.pixelSize: 20
                            font.weight: Font.Bold
                            color: window.textColor
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                ScrollView {
                    id: cardsScrollView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    Flickable {
                        id: cardsFlickable
                        contentWidth: width
                        contentHeight: cardsFlow.height + 20

                        Flow {
                            id: cardsFlow
                            anchors.horizontalCenter: parent.horizontalCenter
                            property int cardWidth: 382
                            property int cardSpacing: 30
                            property int totalCards: (modManager ? modManager.gamePresets.length : 0) + 1
                            property int availableColumns: Math.max(1, Math.floor((cardsFlickable.width + cardSpacing) / (cardWidth + cardSpacing)))
                            property int displayedColumns: Math.max(1, Math.min(totalCards, availableColumns))
                            width: displayedColumns > 0 ? (displayedColumns * cardWidth) + ((displayedColumns - 1) * cardSpacing) : 0
                            spacing: cardSpacing

                            Repeater {
                                model: modManager ? modManager.gamePresets : []

                                delegate: Item {
                                    id: cardContainer
                                    required property var modelData
                                    required property int index

                                    width: 382
                                    height: 215

                                    property bool menuBtnHovered: menuBtnMouseArea.containsMouse

                                    MouseArea {
                                        id: cardMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor

                                        onClicked: function(mouse) {

                                            if (mouse.x > parent.width - 52 && mouse.y < 52) return
                                            modManager.setActivePreset(gameCard.presetId)
                                        }
                                        onPositionChanged: function(mouse) {
                                            var mx = Math.max(0.1, Math.min(0.9, mouse.x / width))
                                            var my = Math.max(0.1, Math.min(0.9, mouse.y / height))
                                            gameCard.mouseX = mx
                                            gameCard.mouseY = my
                                        }
                                        onExited: {
                                            gameCard.mouseX = 0.5
                                            gameCard.mouseY = 0.5
                                        }
                                    }

                                    MouseArea {
                                        id: menuBtnMouseArea
                                        x: parent.width - 52
                                        y: 20
                                        width: 32
                                        height: 32
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        z: 10
                                        onClicked: cardMenu.open()
                                    }

                                    Rectangle {
                                        id: gameCard
                                        anchors.fill: parent

                                        property string presetId: cardContainer.modelData.id
                                        property string presetName: cardContainer.modelData.name
                                        property int modCount: cardContainer.modelData.modCount || 0
                                        property bool isActive: modManager.activePresetId === cardContainer.modelData.id
                                        property bool isHovered: cardMouse.containsMouse
                                        property bool isCozyTheme: modManager && modManager.theme === "Cozy"
                                        property bool isCrimsonTheme: modManager && modManager.theme === "Crimson"
                                        property string iconColorVal: cardContainer.index % 2 === 0 ? "#7C5CFC" : "#FF6B4A"
                                        property string previewImagePath: cardContainer.modelData.previewImage || ""
                                        property bool hasPreview: previewImagePath !== ""
                                        property bool blurBackground: cardContainer.modelData.blurBackground || false

                                        property real mouseX: 0.5
                                        property real mouseY: 0.5
                                        property real tiltX: isHovered ? (mouseY - 0.5) * -15 : 0
                                        property real tiltY: isHovered ? (mouseX - 0.5) * 15 : 0
                                        property real stretchX: isHovered ? 1.0 + (mouseX - 0.5) * 0.02 : 1.0
                                        property real stretchY: isHovered ? 1.0 + (mouseY - 0.5) * 0.02 : 1.0

                                        radius: 0
                                        color: (modManager.theme === "Crimson" && !hasPreview) ? "transparent" : (isGlassmorphic ? window.surfaceColor : window.surfaceColor)
                                        clip: true
                                        border.width: isGlassmorphic
                                                     ? 1
                                                     : ((isCozyTheme || isCrimsonTheme) ? ((isHovered || isActive) ? 2 : 1) : 0)
                                        border.color: isGlassmorphic
                                                     ? Qt.rgba(1, 1, 1, 0.3)
                                                     : (isCozyTheme
                                                        ? ((isHovered || isActive) ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.7))
                                                        : (isCrimsonTheme
                                                           ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, (isHovered || isActive) ? 0.95 : 0.65)
                                                           : "transparent"))
                                        Behavior on border.width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                        Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }

                                        gradient: (modManager.theme === "Crimson" && !hasPreview) ? profileCardGradient : null

                                        Gradient {
                                            id: profileCardGradient
                                            GradientStop { position: 0.0; color: window.crimsonGradientTop }
                                            GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                                        }

                                        transform: [
                                            Scale {
                                                origin.x: gameCard.width / 2
                                                origin.y: gameCard.height / 2
                                                xScale: gameCard.stretchX
                                                yScale: gameCard.stretchY
                                            },
                                            Rotation {
                                                origin.x: gameCard.width / 2
                                                origin.y: gameCard.height / 2
                                                axis { x: 1; y: 0; z: 0 }
                                                angle: gameCard.tiltX
                                            },
                                            Rotation {
                                                origin.x: gameCard.width / 2
                                                origin.y: gameCard.height / 2
                                                axis { x: 0; y: 1; z: 0 }
                                                angle: gameCard.tiltY
                                            }
                                        ]

                                        Behavior on tiltX { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                        Behavior on tiltY { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                        Behavior on stretchX { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                        Behavior on stretchY { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                                        layer.enabled: !gamePresetsManagerPage.isGlassmorphic
                                        layer.smooth: true
                                        layer.samples: 4
                                        layer.textureSize: Qt.size(width * 2, height * 2)
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: Qt.rgba(0, 0, 0, 0.06)
                                            shadowHorizontalOffset: 0
                                            shadowVerticalOffset: 8
                                            shadowBlur: 0.5
                                        }

                                    Item {
                                        id: previewImageContainer
                                        anchors.fill: parent
                                        visible: gameCard.hasPreview
                                        clip: true

                                        Item {
                                            id: backgroundBlurLayer
                                            anchors.fill: parent
                                            visible: !gameCard.blurBackground
                                            clip: true

                                            Image {
                                                anchors.centerIn: parent
                                                width: parent.width + 60
                                                height: parent.height + 60
                                                source: gameCard.previewImagePath
                                                fillMode: Image.PreserveAspectCrop
                                                asynchronous: true
                                                cache: false
                                                smooth: true
                                                sourceSize: Qt.size(Math.ceil(parent.width * 2), Math.ceil(parent.height * 2))

                                                layer.enabled: true
                                                layer.effect: MultiEffect {
                                                    blurEnabled: true
                                                    blurMax: 64
                                                    blur: 1.0
                                                    saturation: 0.3
                                                    brightness: -0.3
                                                }
                                            }

                                            Rectangle {
                                                anchors.fill: parent
                                                color: Qt.rgba(0, 0, 0, 0.4)
                                            }
                                        }

                                        AnimatedImage {
                                            id: previewImage
                                            anchors.fill: parent
                                            source: gameCard.previewImagePath

                                            fillMode: Image.PreserveAspectFit
                                            playing: true
                                            asynchronous: true
                                            cache: false
                                            smooth: true
                                            mipmap: false
                                            visible: !gameCard.blurBackground
                                        }

                                        Item {
                                            id: blurredPreviewContainer
                                            anchors.fill: parent
                                            visible: gameCard.blurBackground
                                            clip: true

                                            AnimatedImage {
                                                id: blurredPreviewSource
                                                anchors.centerIn: parent
                                                width: parent.width + 60
                                                height: parent.height + 60
                                                source: gameCard.previewImagePath
                                                fillMode: Image.PreserveAspectCrop
                                                playing: true
                                                asynchronous: true
                                                cache: false
                                                smooth: true
                                                mipmap: false

                                                layer.enabled: true
                                                layer.effect: MultiEffect {
                                                    blurEnabled: true
                                                    blurMax: 64
                                                    blur: 0.6
                                                    saturation: 0.9
                                                }
                                            }
                                        }

                                        Rectangle {
                                            anchors.fill: parent
                                            gradient: Gradient {
                                                GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.1) }
                                                GradientStop { position: 0.5; color: Qt.rgba(0, 0, 0, 0.2) }
                                                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.5) }
                                            }
                                        }
                                    }

                                    Item {
                                        id: cardTopContent
                                        anchors.top: parent.top
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        height: 60
                                    }

                                    Rectangle {
                                        id: menuBtn
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        anchors.topMargin: 20
                                        anchors.rightMargin: 20
                                        width: 32
                                        height: 32
                                        radius: 16
                                        color: cardContainer.menuBtnHovered ? (gameCard.hasPreview ? Qt.rgba(255, 255, 255, 0.2) : Qt.rgba(0, 0, 0, 0.05)) : "transparent"

                                        Row {
                                            anchors.centerIn: parent
                                            spacing: 3

                                            Repeater {
                                                model: 3
                                                Rectangle {
                                                    width: 4
                                                    height: 4
                                                    radius: 2
                                                    color: gameCard.hasPreview ? "#FFFFFF" : "#BBBBBB"
                                                }
                                            }
                                        }

                                        Menu {
                                            id: cardMenu
                                            x: (gameCard.width - width) / 2 - menuBtn.x + 60
                                            y: (gameCard.height - implicitHeight) / 2 - menuBtn.y
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
                                                    ? Qt.rgba(1, 1, 1, 0.10)
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

                                            RuntimeContextMenuItem {
                                                text: "Set Preview"
                                                iconName: "image.svg"
                                                onTriggered: {
                                                    gamePresetsManagerPage.selectedPresetIdForPreview = gameCard.presetId
                                                    previewImageDialog.open()
                                                }
                                            }

                                            RuntimeContextMenuItem {
                                                text: "Remove Preview"
                                                iconName: "x.svg"
                                                visible: gameCard.hasPreview
                                                height: visible ? 32 : 0
                                                onTriggered: modManager.removePresetPreview(gameCard.presetId)
                                            }

                                            RuntimeContextMenuItem {
                                                text: "Blur BG"
                                                iconName: "droplet.svg"
                                                onTriggered: modManager.togglePresetBlur(gameCard.presetId)
                                            }

                                            RuntimeContextMenuItem {
                                                text: "Rename"
                                                iconName: "pencil.svg"
                                                onTriggered: {
                                                    renameDialog.targetId = gameCard.presetId
                                                    renameDialog.currentName = gameCard.presetName
                                                    renameDialog.open()
                                                }
                                            }

                                            MenuSeparator {}

                                            RuntimeContextMenuItem {
                                                text: "Remove"
                                                iconName: "trash.svg"
                                                isDanger: true
                                                onTriggered: {
                                                    deleteConfirmDialog.targetPresetId = gameCard.presetId
                                                    deleteConfirmDialog.targetPresetName = gameCard.presetName
                                                    deleteConfirmDialog.open()
                                                }
                                            }
                                        }
                                    }

                                    Item {
                                        id: footerContainer
                                        anchors.bottom: parent.bottom
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        height: 80
                                        clip: true

                                        MultiEffect {
                                            visible: gameCard.hasPreview
                                            anchors.fill: parent
                                            source: ShaderEffectSource {
                                                sourceItem: previewImageContainer
                                                sourceRect: Qt.rect(0, gameCard.height - 80, gameCard.width, 80)
                                                live: true
                                            }
                                            blurEnabled: true
                                            blurMax: 64
                                            blur: 1.0
                                            saturation: 0.2
                                        }

                                        Rectangle {
                                            anchors.fill: parent
                                            color: gameCard.hasPreview ? Qt.rgba(0, 0, 0, 0.5) : Qt.rgba(0, 0, 0, 0.02)
                                        }

                                        Rectangle {
                                            anchors.top: parent.top
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            height: 1
                                            color: gameCard.hasPreview ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(0, 0, 0, 0.04)
                                        }

                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 24
                                            anchors.rightMargin: 24
                                            anchors.topMargin: 12
                                            anchors.bottomMargin: 12
                                            spacing: 8

                                            Text {
                                                id: gameNameText
                                                text: gameCard.presetName
                                                font.pixelSize: 16
                                                font.weight: Font.Bold
                                                color: gameCard.hasPreview ? "#FFFFFF" : window.textColor
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight

                                                layer.enabled: gameCard.hasPreview && !gamePresetsManagerPage.isGlassmorphic
                                                layer.effect: MultiEffect {
                                                    shadowEnabled: true
                                                    shadowColor: Qt.rgba(0, 0, 0, 0.5)
                                                    shadowHorizontalOffset: 1
                                                    shadowVerticalOffset: 1
                                                    shadowBlur: 0.3
                                                }
                                            }

                                            RowLayout {
                                                Layout.fillWidth: true

                                                Column {
                                                    spacing: 2

                                                    Text {
                                                        text: "MODS"
                                                        font.pixelSize: 10
                                                        font.weight: Font.DemiBold
                                                        font.letterSpacing: 0.5
                                                        color: gameCard.hasPreview ? Qt.rgba(255, 255, 255, 0.7) : window.textSecondaryColor
                                                        opacity: gameCard.hasPreview ? 1.0 : 0.7
                                                    }

                                                    Text {
                                                        text: gameCard.modCount.toString()
                                                        font.pixelSize: 14
                                                        font.weight: Font.Bold
                                                        color: gameCard.hasPreview ? "#FFFFFF" : window.textColor
                                                    }
                                                }

                                                Item { Layout.fillWidth: true }

                                                Item {
                                                    id: activeBadgeContainer
                                                    visible: opacity > 0
                                                    width: 90
                                                    height: 28
                                                    opacity: gameCard.isActive ? 1.0 : 0.0

                                                    layer.enabled: true
                                                    layer.smooth: true
                                                    layer.textureSize: Qt.size(width * 2, height * 2)

                                                    Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.OutBack } }

                                                    property bool isCrimsonTheme: modManager && modManager.theme === "Crimson"
                                                    property bool isCozyTheme: modManager && modManager.theme === "Cozy"
                                                    property bool isGlassTheme: modManager && modManager.theme === "Glassmorphic"
                                                    property bool isWaveyTheme: modManager && modManager.theme === "Wavey"

                                                    property real gradientAngle: 0
                                                    NumberAnimation on gradientAngle {
                                                        from: 0
                                                        to: 360
                                                        duration: activeBadgeContainer.isCrimsonTheme ? 6000 : 3000
                                                        loops: Animation.Infinite
                                                        running: gameCard.isActive && !activeBadgeContainer.isCrimsonTheme
                                                                 && !activeBadgeContainer.isGlassTheme && !window.animationsPaused
                                                        easing.type: Easing.Linear
                                                    }

                                                    property real cozyPulse: 1.0
                                                    SequentialAnimation on cozyPulse {
                                                        loops: Animation.Infinite
                                                        running: gameCard.isActive && !activeBadgeContainer.isCozyTheme && !window.animationsPaused

                                                        NumberAnimation { to: 1.08; duration: 150; easing.type: Easing.OutQuad }
                                                        NumberAnimation { to: 1.0; duration: 150; easing.type: Easing.InQuad }
                                                        NumberAnimation { to: 1.05; duration: 120; easing.type: Easing.OutQuad }
                                                        NumberAnimation { to: 1.0; duration: 120; easing.type: Easing.InQuad }
                                                        PauseAnimation { duration: 1200 }
                                                    }

                                                    property real sparklePhase: 0
                                                    NumberAnimation on sparklePhase {
                                                        from: 0
                                                        to: 1
                                                        duration: 5000
                                                        loops: Animation.Infinite
                                                        running: gameCard.isActive && activeBadgeContainer.isGlassTheme && !window.animationsPaused
                                                        easing.type: Easing.Linear
                                                    }

                                                    property real waveyPhase: 0
                                                    NumberAnimation on waveyPhase {
                                                        from: 0
                                                        to: Math.PI * 2
                                                        duration: 1500
                                                        loops: Animation.Infinite
                                                        running: gameCard.isActive && !activeBadgeContainer.isWaveyTheme && !window.animationsPaused
                                                        easing.type: Easing.Linear
                                                    }

                                                    property color cozyColor1: '#7300d1'
                                                    property color cozyColor2: '#800064'
                                                    property color cozyAccent: '#ffee00'

                                                    property color waveyColor1: window.primaryColor
                                                    property color waveyColor2: Qt.rgba(
                                                        window.primaryColor.r * 0.6 + 1.0 * 0.4,
                                                        window.primaryColor.g * 0.6 + 1.0 * 0.4,
                                                        window.primaryColor.b * 0.6 + 1.0 * 0.4,
                                                        1.0
                                                    )
                                                    property color waveyAccent: Qt.rgba(
                                                        window.primaryColor.r * 0.7 + 1.0 * 0.3,
                                                        window.primaryColor.g * 0.7 + 1.0 * 0.3,
                                                        window.primaryColor.b * 0.7 + 1.0 * 0.3,
                                                        1.0
                                                    )

                                                    property color glassAccent: window.accentColor

                                                    property color crimsonColor1: {
                                                        var t = (Math.sin(gradientAngle * Math.PI / 180) + 1) / 2
                                                        return Qt.rgba(
                                                            0 + t * 1,
                                                            1 - t * 1,
                                                            1,
                                                            1
                                                        )
                                                    }
                                                    property color crimsonColor2: {
                                                        var t = (Math.cos(gradientAngle * Math.PI / 180) + 1) / 2
                                                        return Qt.rgba(
                                                            0.5 + t * 0.5,
                                                            0,
                                                            1,
                                                            1
                                                        )
                                                    }

                                                    property color standardColor: "#2ECC71"

                                                    property color activeColor: isCrimsonTheme ? crimsonColor1 : (isCozyTheme ? cozyAccent : (isGlassTheme ? glassAccent : (isWaveyTheme ? waveyAccent : standardColor)))

                                                    Rectangle {
                                                        id: activeBadge
                                                        anchors.centerIn: parent
                                                        width: parent.width
                                                        height: parent.height
                                                        radius: height / 2
                                                        scale: 1.0
                                                        visible: !activeBadgeContainer.isCrimsonTheme
                                                                 && !activeBadgeContainer.isGlassTheme
                                                                 && !activeBadgeContainer.isCozyTheme
                                                                 && !activeBadgeContainer.isWaveyTheme

                                                        gradient: Gradient {
                                                            orientation: Gradient.Horizontal
                                                            GradientStop {
                                                                position: 0.0
                                                                color: activeBadgeContainer.isCrimsonTheme
                                                                    ? Qt.rgba(activeBadgeContainer.crimsonColor1.r, activeBadgeContainer.crimsonColor1.g, activeBadgeContainer.crimsonColor1.b, 0.25)
                                                                    : activeBadgeContainer.isCozyTheme
                                                                        ? Qt.rgba(activeBadgeContainer.cozyColor1.r, activeBadgeContainer.cozyColor1.g, activeBadgeContainer.cozyColor1.b, 0.3)
                                                                        : activeBadgeContainer.isGlassTheme
                                                                            ? Qt.rgba(1, 1, 1, 0.15)
                                                                            : activeBadgeContainer.isWaveyTheme
                                                                                ? Qt.rgba(activeBadgeContainer.waveyColor1.r, activeBadgeContainer.waveyColor1.g, activeBadgeContainer.waveyColor1.b, 0.25)
                                                                                : Qt.rgba(46/255, 204/255, 113/255, 0.25)
                                                            }
                                                            GradientStop {
                                                                position: 0.5 + 0.3 * Math.sin(activeBadgeContainer.gradientAngle * Math.PI / 180)
                                                                color: activeBadgeContainer.isCrimsonTheme
                                                                    ? Qt.rgba(activeBadgeContainer.crimsonColor2.r, activeBadgeContainer.crimsonColor2.g, activeBadgeContainer.crimsonColor2.b, 0.35)
                                                                    : activeBadgeContainer.isCozyTheme
                                                                        ? Qt.rgba(activeBadgeContainer.cozyColor2.r, activeBadgeContainer.cozyColor2.g, activeBadgeContainer.cozyColor2.b, 0.4)
                                                                        : activeBadgeContainer.isGlassTheme
                                                                            ? Qt.rgba(activeBadgeContainer.glassAccent.r, activeBadgeContainer.glassAccent.g, activeBadgeContainer.glassAccent.b, 0.2)
                                                                            : activeBadgeContainer.isWaveyTheme
                                                                                ? Qt.rgba(activeBadgeContainer.waveyColor2.r, activeBadgeContainer.waveyColor2.g, activeBadgeContainer.waveyColor2.b, 0.35)
                                                                                : Qt.rgba(39/255, 174/255, 96/255, 0.35)
                                                            }
                                                            GradientStop {
                                                                position: 1.0
                                                                color: activeBadgeContainer.isCrimsonTheme
                                                                    ? Qt.rgba(activeBadgeContainer.crimsonColor1.r, activeBadgeContainer.crimsonColor1.g, activeBadgeContainer.crimsonColor1.b, 0.25)
                                                                    : activeBadgeContainer.isCozyTheme
                                                                        ? Qt.rgba(activeBadgeContainer.cozyColor1.r, activeBadgeContainer.cozyColor1.g, activeBadgeContainer.cozyColor1.b, 0.3)
                                                                        : activeBadgeContainer.isGlassTheme
                                                                            ? Qt.rgba(1, 1, 1, 0.1)
                                                                            : activeBadgeContainer.isWaveyTheme
                                                                                ? Qt.rgba(activeBadgeContainer.waveyColor1.r, activeBadgeContainer.waveyColor1.g, activeBadgeContainer.waveyColor1.b, 0.25)
                                                                                : Qt.rgba(46/255, 204/255, 113/255, 0.25)
                                                            }
                                                        }

                                                        border.width: activeBadgeContainer.isGlassTheme ? 1 : 1.5
                                                        border.color: activeBadgeContainer.isCrimsonTheme
                                                            ? Qt.rgba(activeBadgeContainer.activeColor.r, activeBadgeContainer.activeColor.g, activeBadgeContainer.activeColor.b, 0.8)
                                                            : activeBadgeContainer.isCozyTheme
                                                                ? Qt.rgba(activeBadgeContainer.cozyAccent.r, activeBadgeContainer.cozyAccent.g, activeBadgeContainer.cozyAccent.b, 0.7)
                                                                : activeBadgeContainer.isGlassTheme
                                                                    ? Qt.rgba(1, 1, 1, 0.3)
                                                                    : activeBadgeContainer.isWaveyTheme
                                                                        ? Qt.rgba(activeBadgeContainer.waveyAccent.r, activeBadgeContainer.waveyAccent.g, activeBadgeContainer.waveyAccent.b, 0.7)
                                                                        : Qt.rgba(46/255, 204/255, 113/255, 0.6)

                                                        clip: true

                                                        Repeater {
                                                            model: activeBadgeContainer.isGlassTheme ? 4 : 0
                                                            Rectangle {
                                                                id: sparkle
                                                                property real baseX: (index * 22) + 10
                                                                property real phase: (activeBadgeContainer.sparklePhase + index * 0.25) % 1.0

                                                                x: baseX + Math.sin(phase * Math.PI * 2) * 3
                                                                y: activeBadge.height * 0.3 + Math.cos(phase * Math.PI * 2 + index) * 4
                                                                width: 3
                                                                height: 3
                                                                radius: 1.5
                                                                color: "#FFFFFF"
                                                                opacity: 0.4 + 0.4 * Math.sin(phase * Math.PI * 2)
                                                            }
                                                        }

                                                        Row {
                                                            anchors.centerIn: parent
                                                            spacing: 6

                                                            Item {
                                                                width: 14
                                                                height: 14
                                                                anchors.verticalCenter: parent.verticalCenter

                                                                Rectangle {
                                                                    anchors.centerIn: parent
                                                                    width: 14
                                                                    height: 14
                                                                    radius: 7
                                                                    color: "transparent"
                                                                    border.width: 1.5
                                                                    border.color: activeBadgeContainer.activeColor
                                                                    opacity: 0.5
                                                                    rotation: activeBadgeContainer.gradientAngle
                                                                    visible: !activeBadgeContainer.isCozyTheme && !activeBadgeContainer.isGlassTheme && !activeBadgeContainer.isWaveyTheme

                                                                    Rectangle {
                                                                        width: 4
                                                                        height: 1.5
                                                                        color: activeBadgeContainer.activeColor
                                                                        anchors.horizontalCenter: parent.horizontalCenter
                                                                        anchors.top: parent.top
                                                                        anchors.topMargin: -0.75
                                                                    }
                                                                }

                                                                Item {
                                                                    anchors.centerIn: parent
                                                                    width: 14
                                                                    height: 14
                                                                    visible: !activeBadgeContainer.isCozyTheme && !activeBadgeContainer.isGlassTheme && !activeBadgeContainer.isWaveyTheme

                                                                    Image {
                                                                        id: checkIcon
                                                                        anchors.fill: parent
                                                                        source: "qrc:/UniversalModManager/Lucide Icons/check.svg"
                                                                        sourceSize: Qt.size(14, 14)
                                                                        visible: false
                                                                    }

                                                                    ColorOverlay {
                                                                        anchors.fill: checkIcon
                                                                        source: checkIcon
                                                                        color: activeBadgeContainer.activeColor
                                                                    }
                                                                }

                                                                Item {
                                                                    anchors.centerIn: parent
                                                                    width: 20
                                                                    height: 20
                                                                    visible: activeBadgeContainer.isCozyTheme
                                                                    scale: activeBadgeContainer.cozyPulse

                                                                    Image {
                                                                        id: heartIcon
                                                                        anchors.fill: parent
                                                                        source: "qrc:/UniversalModManager/Lucide Icons/Heart-Cozy.SVG"
                                                                        sourceSize: Qt.size(20, 20)
                                                                        visible: false
                                                                    }

                                                                    ColorOverlay {
                                                                        anchors.fill: heartIcon
                                                                        source: heartIcon
                                                                        color: activeBadgeContainer.cozyAccent
                                                                    }
                                                                }

                                                                Item {
                                                                    anchors.centerIn: parent
                                                                    width: 18
                                                                    height: 18
                                                                    visible: activeBadgeContainer.isGlassTheme
                                                                    opacity: 0.7 + 0.3 * Math.sin(activeBadgeContainer.sparklePhase * Math.PI * 2)
                                                                    rotation: activeBadgeContainer.sparklePhase * 360

                                                                    Image {
                                                                        id: starIcon
                                                                        anchors.fill: parent
                                                                        source: "qrc:/UniversalModManager/Lucide Icons/Glassmorphic-Star.svg"
                                                                        sourceSize: Qt.size(18, 18)
                                                                        visible: false
                                                                    }

                                                                    ColorOverlay {
                                                                        anchors.fill: starIcon
                                                                        source: starIcon
                                                                        color: activeBadgeContainer.glassAccent
                                                                    }
                                                                }

                                                                Item {
                                                                    anchors.centerIn: parent
                                                                    width: 20
                                                                    height: 14
                                                                    visible: activeBadgeContainer.isWaveyTheme

                                                                    Canvas {
                                                                        id: waveyBadgeCanvas
                                                                        anchors.fill: parent

                                                                        property real waveOffset: activeBadgeContainer.waveyPhase

                                                                        onWaveOffsetChanged: requestPaint()

                                                                        onPaint: {
                                                                            var ctx = getContext("2d")
                                                                            ctx.clearRect(0, 0, width, height)

                                                                            var w = width
                                                                            var h = height

                                                                            for (var layer = 0; layer < 3; layer++) {
                                                                                ctx.beginPath()

                                                                                var layerPhase = layer * Math.PI / 3
                                                                                var amplitude = 2.5 - layer * 0.5
                                                                                var yBase = h * (0.35 + layer * 0.15) + Math.sin(waveOffset + layerPhase) * 1.5
                                                                                var alpha = 1.0 - layer * 0.25

                                                                                ctx.moveTo(0, h)

                                                                                for (var x = 0; x <= w; x += 1) {
                                                                                    var normalizedX = x / w
                                                                                    var waveY = yBase +
                                                                                        Math.sin(normalizedX * Math.PI * 3) * Math.cos(waveOffset + layerPhase) * amplitude +
                                                                                        Math.sin(normalizedX * Math.PI * 5) * Math.sin(waveOffset * 2 + layerPhase) * (amplitude * 0.3)
                                                                                    ctx.lineTo(x, waveY)
                                                                                }

                                                                                ctx.lineTo(w, h)
                                                                                ctx.closePath()

                                                                                var colorPhase = (waveOffset + layer * 0.5) % (Math.PI * 2)
                                                                                var colorShift = (Math.sin(colorPhase) + 1) / 2

                                                                                var baseColor = activeBadgeContainer.waveyColor1
                                                                                var lightColor = Qt.rgba(0.7, 0.85, 1.0, 1.0)

                                                                                ctx.fillStyle = Qt.rgba(
                                                                                    baseColor.r * (1 - colorShift) + lightColor.r * colorShift,
                                                                                    baseColor.g * (1 - colorShift) + lightColor.g * colorShift,
                                                                                    baseColor.b * (1 - colorShift) + lightColor.b * colorShift,
                                                                                    alpha * 0.85
                                                                                )
                                                                                ctx.fill()
                                                                            }
                                                                        }
                                                                    }

                                                                    Repeater {
                                                                        model: 3
                                                                        Rectangle {
                                                                            property real particlePhase: (activeBadgeContainer.waveyPhase / (Math.PI * 2) + index * 0.33) % 1.0

                                                                            x: 4 + index * 6 + Math.sin(activeBadgeContainer.waveyPhase + index * Math.PI * 2 / 3) * 2
                                                                            y: 2 + Math.sin(activeBadgeContainer.waveyPhase * 2 + index * Math.PI * 2 / 3) * 3
                                                                            width: 2
                                                                            height: 2
                                                                            radius: 1
                                                                            color: "#FFFFFF"
                                                                            opacity: 0.5 + 0.5 * Math.sin(activeBadgeContainer.waveyPhase + index * Math.PI * 2 / 3)
                                                                        }
                                                                    }
                                                                }
                                                            }

                                                            Text {
                                                                id: activeBadgeText
                                                                text: "ACTIVE"
                                                                font.pixelSize: 10
                                                                font.weight: Font.Bold
                                                                font.letterSpacing: 1.0
                                                                color: activeBadgeContainer.isWaveyTheme
                                                                    ? Qt.rgba(0.85, 0.92, 1.0, 1.0)
                                                                    : activeBadgeContainer.activeColor
                                                                anchors.verticalCenter: parent.verticalCenter
                                                                renderType: Text.NativeRendering
                                                            }
                                                        }
                                                    }

                                                    scale: gameCard.isActive ? 1.0 : 0.5
                                                    Behavior on scale {
                                                        NumberAnimation {
                                                            duration: 400
                                                            easing.type: Easing.OutBack
                                                            easing.overshoot: 1.5
                                                        }
                                                    }

                                                    CrimsonActiveBadge {
                                                        anchors.centerIn: parent
                                                        visible: activeBadgeContainer.isCrimsonTheme
                                                        paused: window.animationsPaused
                                                    }

                                                    GlassActiveBadge {
                                                        anchors.centerIn: parent
                                                        visible: activeBadgeContainer.isGlassTheme
                                                        paused: window.animationsPaused
                                                    }

                                                    CozyActiveBadge {
                                                        anchors.centerIn: parent
                                                        visible: activeBadgeContainer.isCozyTheme
                                                        paused: window.animationsPaused
                                                    }

                                                    WaveyActiveBadge {
                                                        anchors.centerIn: parent
                                                        visible: activeBadgeContainer.isWaveyTheme
                                                        paused: window.animationsPaused
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    }
                                }
                            }

                            Rectangle {
                                id: addCard
                                width: 382
                                height: 215
                                property bool isCozyTheme: modManager && modManager.theme === "Cozy"
                                property bool isCrimsonTheme: modManager && modManager.theme === "Crimson"
                                radius: 20
                                color: modManager.theme === "Crimson" ? "transparent" : (addCardMouse.containsMouse ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.05) : "transparent")
                                border.width: (isCozyTheme || isCrimsonTheme) ? (addCardMouse.containsMouse ? 2 : 1) : 2
                                border.color: isCozyTheme
                                              ? (addCardMouse.containsMouse ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.7))
                                              : (isCrimsonTheme
                                                 ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, addCardMouse.containsMouse ? 0.95 : 0.65)
                                                 : (addCardMouse.containsMouse ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.5)))

                                gradient: modManager.theme === "Crimson" ? addProfileCardGradient : null

                                Gradient {
                                    id: addProfileCardGradient
                                    GradientStop { position: 0.0; color: window.crimsonGradientTop }
                                    GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                                }

                                Behavior on color { ColorAnimation { duration: 200 } }
                                Behavior on border.color { ColorAnimation { duration: 200 } }

                                scale: addCardMouse.containsMouse ? 1.02 : 1.0
                                Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                                MouseArea {
                                    id: addCardMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        createPresetDialog.open()
                                        focus = false
                                    }
                                }

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 16

                                    Rectangle {
                                        width: 64
                                        height: 64
                                        radius: 20
                                        color: addCardMouse.containsMouse ? window.accentColor : Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g, window.textSecondaryColor.b, 0.1)
                                        anchors.horizontalCenter: parent.horizontalCenter

                                        Behavior on color { ColorAnimation { duration: 200 } }

                                        Text {
                                            anchors.centerIn: parent
                                            text: "+"
                                            font.pixelSize: 32
                                            font.weight: Font.Light
                                            color: addCardMouse.containsMouse ? "#FFFFFF" : window.textSecondaryColor

                                            Behavior on color { ColorAnimation { duration: 200 } }
                                        }
                                    }

                                    Text {
                                        text: "Create New Profile"
                                        font.pixelSize: 16
                                        font.weight: Font.Bold
                                        color: addCardMouse.containsMouse ? window.accentColor : window.textSecondaryColor
                                        anchors.horizontalCenter: parent.horizontalCenter

                                        Behavior on color { ColorAnimation { duration: 200 } }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: statsPanel
                Layout.preferredWidth: 320
                Layout.alignment: Qt.AlignTop
                implicitHeight: statsPanelContent.implicitHeight + 48
                radius: 16
                color: modManager.theme === "Crimson" ? "transparent" : (isGlassmorphic ? window.surfaceColor : window.surfaceColor)
                border.width: isGlassmorphic ? 1 : 0
                border.color: isGlassmorphic ? Qt.rgba(1, 1, 1, 0.3) : "transparent"

                gradient: modManager.theme === "Crimson" ? statsPanelGradient : null

                Gradient {
                    id: statsPanelGradient
                    GradientStop { position: 0.0; color: window.crimsonGradientTop }
                    GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                }

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: !isGlassmorphic
                    shadowColor: Qt.rgba(0, 0, 0, 0.08)
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 10
                    shadowBlur: 0.6
                }

                ColumnLayout {
                    id: statsPanelContent
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 16

                    Text {
                        text: "Global Stats"
                        font.pixelSize: 15
                        font.weight: Font.Bold
                        color: window.textColor
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "Added Games"
                            font.pixelSize: 14
                            color: window.textSecondaryColor
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: gamePresetsManagerPage.totalProfiles.toString()
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: window.textColor
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "Total Mods"
                            font.pixelSize: 14
                            color: window.textSecondaryColor
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: gamePresetsManagerPage.totalMods.toString()
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: window.textColor
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 8

                        Text {
                            text: "Active Mods"
                            font.pixelSize: 14
                            color: window.textSecondaryColor
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: gamePresetsManagerPage.totalActiveMods.toString()
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: window.textColor
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: 12
                        Layout.bottomMargin: 4
                        height: 1
                        color: Qt.rgba(0, 0, 0, 0.08)
                    }

                    Text {
                        text: "Game Scan"
                        font.pixelSize: 15
                        font.weight: Font.Bold
                        color: window.textColor
                    }

                    Text {
                        text: "Mod folder type: " + selectedModsFolderType
                        font.pixelSize: 13
                        color: window.textSecondaryColor
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        focusPolicy: Qt.NoFocus

                        background: Rectangle {
                            color: parent.down ? Qt.darker(window.accentColor, 1.1) : window.accentColor
                            radius: 12
                        }

                        contentItem: Text {
                            text: "𝗦𝗰𝗮𝗻 𝗳𝗼𝗿 𝗨𝗻𝗿𝗲𝗮𝗹 𝗘𝗻𝗴𝗶𝗻𝗲 𝗴𝗮𝗺𝗲𝘀"
                            font.pixelSize: 15
                            font.weight: Font.Bold
                            color: "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            gameScanDialog.open()
                            focus = false
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: 12
                        Layout.bottomMargin: 4
                        height: 1
                        color: Qt.rgba(0, 0, 0, 0.08)
                    }

                    Text {
                        text: "System"
                        font.pixelSize: 15
                        font.weight: Font.Bold
                        color: window.textColor
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: "CPU"
                                font.pixelSize: 14
                                color: window.textSecondaryColor
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: modManager ? modManager.cpuUsage.toFixed(1) + "%" : "0%"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: modManager && modManager.cpuUsage > 80 ? "#EF4444" :
                                       modManager && modManager.cpuUsage > 50 ? "#F59E0B" : window.textColor
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 10
                            radius: 5
                            color: Qt.rgba(0, 0, 0, 0.08)
                            clip: true

                            Rectangle {
                                id: cpuBarRect
                                width: Math.round(parent.width * (modManager ? modManager.cpuUsage / 100 : 0))
                                height: parent.height
                                radius: 5

                                property color barColor: modManager ? modManager.cpuUsageBarColor : "#4CAF50"

                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: Qt.lighter(cpuBarRect.barColor, 1.4) }
                                    GradientStop { position: 1.0; color: cpuBarRect.barColor }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: "RAM"
                                font.pixelSize: 14
                                color: window.textSecondaryColor
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: modManager ? modManager.ramUsageText : "0 / 0 GB"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: modManager && modManager.ramUsage > 85 ? "#EF4444" :
                                       modManager && modManager.ramUsage > 70 ? "#F59E0B" : window.textColor
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 10
                            radius: 5
                            color: Qt.rgba(0, 0, 0, 0.08)
                            clip: true

                            Rectangle {
                                id: ramBarRect
                                width: Math.round(parent.width * (modManager ? modManager.ramUsage / 100 : 0))
                                height: parent.height
                                radius: 5

                                property color barColor: modManager ? modManager.ramUsageBarColor : "#3B82F6"

                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: Qt.lighter(ramBarRect.barColor, 1.4) }
                                    GradientStop { position: 1.0; color: ramBarRect.barColor }
                                }
                            }
                        }
                    }

                }
            }
        }
    }
}
