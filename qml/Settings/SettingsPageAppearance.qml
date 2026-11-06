import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

ColumnLayout {
    id: appearanceSection
    spacing: 40
    Layout.fillWidth: true

    Rectangle {
        id: themeCardContainer
        Layout.fillWidth: true
        Layout.rightMargin: 10
        visible: generalSectionIndex === 0
        implicitHeight: generalSectionIndex === 0 ? themeCardContent.implicitHeight + 60 : 0
        color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
        radius: window.cardRadius
        border.width: window.borderWidth
        border.color: window.borderColor

        gradient: modManager.theme === "Crimson" ? crimsonThemeCardGradient : null

        Gradient {
            id: crimsonThemeCardGradient
            GradientStop { position: 0.0; color: window.crimsonGradientTop }
            GradientStop { position: 1.0; color: window.crimsonGradientBottom }
        }

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.3)
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 4
            shadowBlur: 0.4
        }

        ColumnLayout {
            id: themeCardContent
            anchors.fill: parent
            anchors.margins: 30
            spacing: 25

            RowLayout {
                Layout.fillWidth: true
                spacing: 15

                Rectangle {
                    width: 40
                    height: 40
                    color: "transparent"
                    radius: 14
                    Layout.alignment: Qt.AlignVCenter

                    Image {
                        anchors.centerIn: parent
                        width: 32
                        height: 32
                        source: "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Paint.png"
                        sourceSize: Qt.size(32, 32)
                        smooth: true
                        antialiasing: true
                    }
                }

                Column {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 4

                    Text {
                        text: "Theme"
                        font.pixelSize: 18
                        font.weight: 600
                        color: window.textColor
                    }

                    Text {
                        text: "Choose a visual style."
                        font.pixelSize: 13
                        color: window.textSecondaryColor
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                Rectangle {
                    id: cozyThemeCard
                    Layout.fillWidth: true
                    Layout.preferredHeight: 160
                    Layout.alignment: Qt.AlignTop
                    readonly property color hoverAccent: themeConfig.cozy.accentColor
                    radius: 20
                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.14) : window.surfaceColor)
                    border.width: modManager.theme === "Cozy" ? 3 : 2
                    border.color: modManager.theme === "Cozy" ? window.primaryColor : window.borderColor

                    gradient: modManager.theme === "Crimson" ? cozyCrimsonGradient : null

                    Gradient {
                        id: cozyCrimsonGradient
                        GradientStop { position: 0.0; color: window.crimsonThemeCardGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonThemeCardGradientBottom }
                    }

                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowHorizontalOffset: 0
                        shadowVerticalOffset: cozyMouse.containsMouse ? 8 : 4
                        shadowBlur: cozyMouse.containsMouse ? 0.8 : 0.45
                        shadowColor: cozyMouse.containsMouse
                            ? Qt.rgba(cozyThemeCard.hoverAccent.r, cozyThemeCard.hoverAccent.g, cozyThemeCard.hoverAccent.b, 0.62)
                            : Qt.rgba(0, 0, 0, 0.28)
                    }

                    MouseArea {
                        id: cozyMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            switchTheme("Cozy")
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Rectangle {
                                width: 48
                                height: 48
                                radius: 12
                                color: "transparent"
                                border.width: 0
                                border.color: "transparent"

                                Image {
                                    anchors.centerIn: parent
                                    width: 40
                                    height: 40
                                    source: "qrc:/UniversalModManager/Icons/Cozy Theme/Cozy theme.png"
                                    sourceSize: Qt.size(128, 128)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    antialiasing: true
                                    mipmap: true
                                    cache: false
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: "Cozy"
                                    font.pixelSize: 16
                                    font.weight: 600
                                    color: window.textColor
                                }

                                Text {
                                    text: "Warm & comfortable"
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                }
                            }

                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 12
                            color: Qt.rgba(0, 0, 0, 0.3)

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Rectangle {
                                    Layout.fillHeight: true
                                    Layout.preferredWidth: 6
                                    radius: 3
                                    color: themeConfig.cozy.accentColor
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 12
                                        radius: 6
                                        color: themeConfig.cozy.accentColor
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 10
                                        radius: 5
                                        color: "#FFF9F2"
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 10
                                        radius: 5
                                        color: Qt.rgba(139, 115, 85, 0.6)
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: glassmorphicThemeCard
                    Layout.fillWidth: true
                    Layout.preferredHeight: 160
                    Layout.alignment: Qt.AlignTop
                    readonly property color hoverAccent: modManager.accentColor
                    radius: 20
                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.14) : window.surfaceColor)
                    border.width: modManager.theme === "Glassmorphic" ? 3 : 2
                    border.color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.borderColor

                    gradient: modManager.theme === "Crimson" ? glassCrimsonGradient : null

                    Gradient {
                        id: glassCrimsonGradient
                        GradientStop { position: 0.0; color: window.crimsonThemeCardGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonThemeCardGradientBottom }
                    }

                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowHorizontalOffset: 0
                        shadowVerticalOffset: glassMouse.containsMouse ? 8 : 4
                        shadowBlur: glassMouse.containsMouse ? 0.8 : 0.45
                        shadowColor: glassMouse.containsMouse
                            ? Qt.rgba(glassmorphicThemeCard.hoverAccent.r, glassmorphicThemeCard.hoverAccent.g, glassmorphicThemeCard.hoverAccent.b, 0.62)
                            : Qt.rgba(0, 0, 0, 0.28)
                    }

                    MouseArea {
                        id: glassMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            switchTheme("Glassmorphic")
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Rectangle {
                                width: 48
                                height: 48
                                radius: 12
                                color: "transparent"
                                border.width: 0
                                border.color: "transparent"

                                Image {
                                    anchors.centerIn: parent
                                    width: 40
                                    height: 40
                                    source: "qrc:/UniversalModManager/Icons/Glassmorphic Theme/Glassmorphic theme.png"
                                    sourceSize: Qt.size(128, 128)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    antialiasing: true
                                    mipmap: true
                                    cache: false
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: "Glassmorphic"
                                    font.pixelSize: 16
                                    font.weight: 600
                                    color: window.textColor
                                }

                                Text {
                                    text: "Modern & Customizable"
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                }
                            }

                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 12
                            color: Qt.rgba(0, 0, 0, 0.3)

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Rectangle {
                                    Layout.fillHeight: true
                                    Layout.preferredWidth: 6
                                    radius: 3
                                    color: modManager.accentColor
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 12
                                        radius: 6
                                        color: modManager.accentColor
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 10
                                        radius: 5
                                        color: modManager.navBarColor
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 10
                                        radius: 5
                                        color: window.navBarIconColor
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: crimsonThemeCard
                    Layout.fillWidth: true
                    Layout.preferredHeight: 160
                    Layout.alignment: Qt.AlignTop
                    readonly property color hoverAccent: themeConfig.crimson.accentColor
                    radius: 20
                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.14) : window.surfaceColor)
                    border.width: modManager.theme === "Crimson" ? 3 : 2
                    border.color: modManager.theme === "Crimson" ? window.primaryColor : window.borderColor

                    gradient: modManager.theme === "Crimson" ? crimsonCardGradient : null

                    Gradient {
                        id: crimsonCardGradient
                        GradientStop { position: 0.0; color: window.crimsonThemeCardGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonThemeCardGradientBottom }
                    }

                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowHorizontalOffset: 0
                        shadowVerticalOffset: crimsonMouse.containsMouse ? 8 : 4
                        shadowBlur: crimsonMouse.containsMouse ? 0.8 : 0.45
                        shadowColor: crimsonMouse.containsMouse
                            ? Qt.rgba(crimsonThemeCard.hoverAccent.r, crimsonThemeCard.hoverAccent.g, crimsonThemeCard.hoverAccent.b, 0.62)
                            : Qt.rgba(0, 0, 0, 0.28)
                    }

                    MouseArea {
                        id: crimsonMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            switchTheme("Crimson")
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Rectangle {
                                width: 48
                                height: 48
                                radius: 12
                                color: "transparent"
                                border.width: 0
                                border.color: "transparent"

                                Image {
                                    anchors.centerIn: parent
                                    width: 40
                                    height: 40
                                    source: "qrc:/UniversalModManager/Icons/Crimson Theme/Crimson Theme.png"
                                    sourceSize: Qt.size(128, 128)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    antialiasing: true
                                    mipmap: true
                                    cache: false
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: "Crimson"
                                    font.pixelSize: 16
                                    font.weight: 600
                                    color: window.textColor
                                }

                                Text {
                                    text: "Deep & dramatic"
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                }
                            }

                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 12
                            color: Qt.rgba(0, 0, 0, 0.3)

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Rectangle {
                                    Layout.fillHeight: true
                                    Layout.preferredWidth: 6
                                    radius: 3
                                    color: themeConfig.crimson.accentColor
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 12
                                        radius: 6
                                        color: themeConfig.crimson.accentColor
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 10
                                        radius: 5
                                        color: themeConfig.crimson.primaryColor
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 10
                                        radius: 5
                                        color: themeConfig.crimson.borderColor
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: waveyThemeCard
                    Layout.fillWidth: true
                    Layout.preferredHeight: 160
                    Layout.alignment: Qt.AlignTop
                    readonly property color hoverAccent: themeConfig.wavey.accentColor
                    radius: 20
                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.14) : window.surfaceColor)
                    border.width: modManager.theme === "Wavey" ? 3 : 2
                    border.color: modManager.theme === "Wavey" ? window.primaryColor : window.borderColor

                    gradient: modManager.theme === "Crimson" ? waveyCrimsonGradient : null

                    Gradient {
                        id: waveyCrimsonGradient
                        GradientStop { position: 0.0; color: window.crimsonThemeCardGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonThemeCardGradientBottom }
                    }

                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowHorizontalOffset: 0
                        shadowVerticalOffset: waveyMouse.containsMouse ? 8 : 4
                        shadowBlur: waveyMouse.containsMouse ? 0.8 : 0.45
                        shadowColor: waveyMouse.containsMouse
                            ? Qt.rgba(waveyThemeCard.hoverAccent.r, waveyThemeCard.hoverAccent.g, waveyThemeCard.hoverAccent.b, 0.62)
                            : Qt.rgba(0, 0, 0, 0.28)
                    }

                    MouseArea {
                        id: waveyMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            switchTheme("Wavey")
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Rectangle {
                                width: 48
                                height: 48
                                radius: 12
                                color: "transparent"
                                border.width: 0
                                border.color: "transparent"

                                Image {
                                    anchors.centerIn: parent
                                    width: 32
                                    height: 32
                                    source: "qrc:/UniversalModManager/Icons/Wavey Theme/wave.png"
                                    sourceSize: Qt.size(32, 32)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: "Wavey"
                                    font.pixelSize: 16
                                    font.weight: 600
                                    color: window.textColor
                                }

                                Text {
                                    text: "Wavy Blue Glow"
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 12
                            color: Qt.rgba(0, 0, 0, 0.3)

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Rectangle {
                                    Layout.fillHeight: true
                                    Layout.preferredWidth: 6
                                    radius: 3
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: "#60a5fa" }
                                        GradientStop { position: 1.0; color: "#1e3a8a" }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 12
                                        radius: 6
                                        color: themeConfig.wavey.accentColor
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 10
                                        radius: 5
                                        color: themeConfig.wavey.primaryColor
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 10
                                        radius: 5
                                        color: "#1e3a8a"
                                    }
                                }
                            }
                        }
                    }
                }
            }

        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.rightMargin: 10
        implicitHeight: (generalSectionIndex === 0 && modManager.theme === "Glassmorphic") ? glassColumn.implicitHeight + 50 : 0
        color: window.surfaceColor
        radius: 24
        border.width: 1.5
        border.color: Qt.rgba(1, 1, 1, 0.3)
        visible: generalSectionIndex === 0 && modManager.theme === "Glassmorphic"

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.4)
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 8
            shadowBlur: 0.6
        }

        ColumnLayout {
            id: glassColumn
            anchors.fill: parent
            anchors.margins: 30
            spacing: 25

            RowLayout {
                Layout.fillWidth: true

                Rectangle {
                    Layout.preferredWidth: 60
                    Layout.preferredHeight: 60
                    color: "transparent"
                    radius: 16
                    border.width: 0
                    border.color: "transparent"

                    Image {
                        id: glassAppearanceIcon
                        anchors.centerIn: parent
                        width: 32
                        height: 32
                        source: "qrc:/UniversalModManager/Lucide Icons/palette.svg"
                        sourceSize: Qt.size(128, 128)
                        fillMode: Image.PreserveAspectFit
                        smooth: true

                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: window.primaryColor
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "Glassmorphic Appearance"
                        font.pixelSize: 22
                        font.weight: 600
                        color: window.textColor
                    }

                    Text {
                        text: "Customize the app appearance to your liking."
                        font.pixelSize: 14
                        color: window.textSecondaryColor
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: false

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: ""
                            font.pixelSize: 18
                        }

                        Text {
                            text: "Taskbar Intensity"
                            font.pixelSize: 15
                            font.weight: 600
                            color: window.textColor
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            Layout.preferredWidth: 50
                            Layout.preferredHeight: 28
                            color: Qt.rgba(1, 1, 1, 0.15)
                            radius: 14
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.3)

                            Text {
                                anchors.centerIn: parent
                                text: Math.round(modManager.glassBlur * 100) + "%"
                                font.pixelSize: 12
                                font.weight: Font.Bold
                                color: window.primaryColor
                            }
                        }
                    }

                    Slider {
                        Layout.fillWidth: true
                        from: 0.0
                        to: 1.0
                        value: modManager.glassBlur
                        stepSize: 0.05
                        onMoved: modManager.setGlassBlur(value)

                        background: Rectangle {
                            x: parent.leftPadding
                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
                            implicitWidth: 200
                            implicitHeight: 8
                            width: parent.availableWidth
                            height: implicitHeight
                            radius: 4
                            color: Qt.rgba(1, 1, 1, 0.15)
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.2)

                            Rectangle {
                                width: parent.parent.visualPosition * parent.width
                                height: parent.height
                                radius: 4
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: window.primaryColor }
                                    GradientStop { position: 1.0; color: Qt.lighter(window.primaryColor, 1.3) }
                                }
                            }
                        }

                        handle: Rectangle {
                            x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
                            implicitWidth: 24
                            implicitHeight: 24
                            radius: 12
                            color: window.primaryColor
                            border.color: "#FFFFFF"
                            border.width: 3

                            layer.enabled: true
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                                shadowVerticalOffset: 2
                                shadowBlur: 0.4
                            }

                            Behavior on scale { NumberAnimation { duration: 100 } }
                            scale: parent.pressed ? 0.9 : 1.0
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "wallpaper blur intensity"
                            font.pixelSize: 15
                            font.weight: 600
                            color: window.textColor
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            Layout.preferredWidth: 50
                            Layout.preferredHeight: 28
                            color: Qt.rgba(1, 1, 1, 0.15)
                            radius: 14
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.3)

                            Text {
                                anchors.centerIn: parent
                                text: Math.round(modManager.glassBlur * 100) + "%"
                                font.pixelSize: 12
                                font.weight: Font.Bold
                                color: window.primaryColor
                            }
                        }
                    }

                    Slider {
                        Layout.fillWidth: true
                        from: 0.0
                        to: 1.0
                        value: modManager.glassBlur
                        stepSize: 0.05
                        onMoved: modManager.setGlassBlur(value)

                        background: Rectangle {
                            x: parent.leftPadding
                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
                            implicitWidth: 200
                            implicitHeight: 8
                            width: parent.availableWidth
                            height: implicitHeight
                            radius: 4
                            color: Qt.rgba(1, 1, 1, 0.15)
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.2)

                            Rectangle {
                                width: parent.parent.visualPosition * parent.width
                                height: parent.height
                                radius: 4
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: window.primaryColor }
                                    GradientStop { position: 1.0; color: Qt.lighter(window.primaryColor, 1.3) }
                                }
                            }
                        }

                        handle: Rectangle {
                            x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
                            implicitWidth: 24
                            implicitHeight: 24
                            radius: 12
                            color: window.primaryColor
                            border.color: "#FFFFFF"
                            border.width: 3

                            layer.enabled: true
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                                shadowVerticalOffset: 2
                                shadowBlur: 0.4
                            }

                            Behavior on scale { NumberAnimation { duration: 100 } }
                            scale: parent.pressed ? 0.9 : 1.0
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "Glass Opacity"
                            font.pixelSize: 15
                            font.weight: 600
                            color: window.textColor
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            Layout.preferredWidth: 50
                            Layout.preferredHeight: 28
                            color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.18)
                                : Qt.rgba(1, 1, 1, 0.15)
                            radius: 14
                            border.width: 1
                            border.color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.35)
                                : Qt.rgba(1, 1, 1, 0.3)

                            Text {
                                anchors.centerIn: parent
                                text: Math.round(modManager.glassOpacity * 100) + "%"
                                font.pixelSize: 12
                                font.weight: Font.Bold
                                color: modManager.theme === "Glassmorphic" ? window.accentColor : window.primaryColor
                            }
                        }
                    }

                    Slider {
                        Layout.fillWidth: true
                        from: 0.05
                        to: 0.5
                        value: modManager.glassOpacity
                        stepSize: 0.05
                        onMoved: modManager.setGlassOpacity(value)

                        background: Rectangle {
                            x: parent.leftPadding
                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
                            implicitWidth: 200
                            implicitHeight: 8
                            width: parent.availableWidth
                            height: implicitHeight
                            radius: 4
                            color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.12)
                                : Qt.rgba(1, 1, 1, 0.15)
                            border.width: 1
                            border.color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.22)
                                : Qt.rgba(1, 1, 1, 0.2)

                            Rectangle {
                                width: parent.parent.visualPosition * parent.width
                                height: parent.height
                                radius: 4
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: modManager.theme === "Glassmorphic" ? window.accentColor : window.primaryColor }
                                    GradientStop { position: 1.0; color: modManager.theme === "Glassmorphic" ? Qt.lighter(window.accentColor, 1.18) : Qt.lighter(window.primaryColor, 1.3) }
                                }
                            }
                        }

                        handle: Rectangle {
                            x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                            y: parent.topPadding + parent.availableHeight / 2 - height / 2
                            implicitWidth: 24
                            implicitHeight: 24
                            radius: 12
                            color: modManager.theme === "Glassmorphic" ? window.accentColor : window.primaryColor
                            border.color: "#FFFFFF"
                            border.width: 3

                            layer.enabled: true
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                                shadowVerticalOffset: 2
                                shadowBlur: 0.4
                            }

                            Behavior on scale { NumberAnimation { duration: 100 } }
                            scale: parent.pressed ? 0.9 : 1.0
                        }
                    }
                }

                GlassSurfaceSlider {
                    Layout.fillWidth: true
                    label: "Surface Darkness"
                    value: modManager.glassDarkness
                    from: 0.0
                    to: 1.0
                    stepSize: 0.05
                    onMoved: function(newValue) { modManager.setGlassDarkness(newValue) }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.rightMargin: 10
                spacing: 15
                visible: generalSectionIndex === 0

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "Element Colors & Accents"
                        font.pixelSize: 15
                        font.weight: 600
                        color: window.textColor
                        Layout.fillWidth: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Button {
                        id: advancedUiColorsButton
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 44
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: 1.0

                        contentItem: Item {
                            anchors.fill: parent

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 20
                                anchors.rightMargin: 20
                                text: showAdvancedUiColors ? "Hide Advanced UI Colors" : "Advanced UI Colors"
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                color: advancedUiColorsButton.hovered ? "#FFFFFF" : window.textColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideNone
                                wrapMode: Text.NoWrap

                                Behavior on color { ColorAnimation { duration: 200 } }
                            }
                        }

                        background: Rectangle {
                            color: advancedUiColorsButton.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.12)
                            radius: 20
                            border.width: 1.5
                            border.color: advancedUiColorsButton.hovered ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(1, 1, 1, 0.3)

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }

                            layer.enabled: advancedUiColorsButton.hovered
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Qt.rgba(0, 0, 0, 0.3)
                                shadowVerticalOffset: 4
                                shadowBlur: 0.4
                            }
                        }

                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                        onHoveredChanged: {
                            if (hovered) {
                                scale = 1.05
                            } else {
                                scale = 1.0
                            }
                        }

                        onClicked: showAdvancedUiColors = !showAdvancedUiColors
                    }

                    Button {
                        id: uiAccentButton
                        Layout.preferredWidth: 160
                        Layout.preferredHeight: 44
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: 1.0

                        contentItem: Item {
                            anchors.fill: parent

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                text: showUiAccentColors ? "Hide UI Accent Color" : "UI Accent Color"
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                color: uiAccentButton.hovered ? "#FFFFFF" : window.textColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideNone
                                wrapMode: Text.NoWrap

                                Behavior on color { ColorAnimation { duration: 200 } }
                            }
                        }

                        background: Rectangle {
                            color: uiAccentButton.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.12)
                            radius: 20
                            border.width: 1.5
                            border.color: uiAccentButton.hovered ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(1, 1, 1, 0.3)

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }

                            layer.enabled: uiAccentButton.hovered
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Qt.rgba(0, 0, 0, 0.3)
                                shadowVerticalOffset: 4
                                shadowBlur: 0.4
                            }
                        }

                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                        onHoveredChanged: {
                            if (hovered) {
                                scale = 1.05
                            } else {
                                scale = 1.0
                            }
                        }

                        onClicked: showUiAccentColors = !showUiAccentColors
                    }

                    Button {
                        id: navBarColorButton
                        Layout.preferredWidth: 190
                        Layout.preferredHeight: 44
                        text: "Navigation Bar Color"
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        visible: modManager.theme === "Glassmorphic"
                        scale: 1.0

                        contentItem: Item {
                            anchors.fill: parent

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 18
                                anchors.rightMargin: 18
                                text: navBarColorButton.text
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                color: navBarColorButton.hovered ? "#FFFFFF" : window.textColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideNone
                                wrapMode: Text.NoWrap

                                Behavior on color { ColorAnimation { duration: 200 } }
                            }
                        }

                        background: Rectangle {
                            color: navBarColorButton.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.12)
                            radius: 20
                            border.width: 1.5
                            border.color: navBarColorButton.hovered ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(1, 1, 1, 0.3)

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }

                            layer.enabled: navBarColorButton.hovered
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Qt.rgba(0, 0, 0, 0.3)
                                shadowVerticalOffset: 4
                                shadowBlur: 0.4
                            }
                        }

                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                        onHoveredChanged: {
                            if (hovered) {
                                scale = 1.05
                            } else {
                                scale = 1.0
                            }
                        }

                        onClicked: {
                            navBarColorDialog.open()
                            focus = false
                        }
                    }

                    Button {
                        id: navBarIconColorButton
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 44
                        text: "Navigation Icons Color"
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        visible: modManager.theme === "Glassmorphic"
                        scale: 1.0

                        contentItem: Item {
                            anchors.fill: parent

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 18
                                anchors.rightMargin: 18
                                text: navBarIconColorButton.text
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                color: navBarIconColorButton.hovered ? "#FFFFFF" : window.textColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideNone
                                wrapMode: Text.NoWrap

                                Behavior on color { ColorAnimation { duration: 200 } }
                            }
                        }

                        background: Rectangle {
                            color: navBarIconColorButton.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.12)
                            radius: 20
                            border.width: 1.5
                            border.color: navBarIconColorButton.hovered ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(1, 1, 1, 0.3)

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }

                            layer.enabled: navBarIconColorButton.hovered
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Qt.rgba(0, 0, 0, 0.3)
                                shadowVerticalOffset: 4
                                shadowBlur: 0.4
                            }
                        }

                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                        onHoveredChanged: {
                            if (hovered) {
                                scale = 1.05
                            } else {
                                scale = 1.0
                            }
                        }

                        onClicked: {
                            navBarIconColorDialog.open()
                            focus = false
                        }
                    }

                    Button {
                        id: performanceBarColorsButton
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 44
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        visible: modManager.theme === "Glassmorphic"
                        scale: 1.0

                        contentItem: Item {
                            anchors.fill: parent

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 18
                                anchors.rightMargin: 18
                                text: showPerformanceBarColors ? "Hide Bar Colors" : "Performance Bar Colors"
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                color: performanceBarColorsButton.hovered ? "#FFFFFF" : window.textColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideNone
                                wrapMode: Text.NoWrap

                                Behavior on color { ColorAnimation { duration: 200 } }
                            }
                        }

                        background: Rectangle {
                            color: performanceBarColorsButton.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.12)
                            radius: 20
                            border.width: 1.5
                            border.color: performanceBarColorsButton.hovered ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(1, 1, 1, 0.3)

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }

                            layer.enabled: performanceBarColorsButton.hovered
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Qt.rgba(0, 0, 0, 0.3)
                                shadowVerticalOffset: 4
                                shadowBlur: 0.4
                            }
                        }

                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                        onHoveredChanged: {
                            if (hovered) {
                                scale = 1.05
                            } else {
                                scale = 1.0
                            }
                        }

                        onClicked: showPerformanceBarColors = !showPerformanceBarColors
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: modManager.theme === "Glassmorphic" && showPerformanceBarColors

                    Button {
                        id: cpuBarColorButton
                        width: 120
                        height: 36
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: 1.0

                        contentItem: Item {
                            anchors.fill: parent

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 6

                                Rectangle {
                                    width: 20
                                    height: 4
                                    radius: 2
                                    color: modManager ? modManager.cpuUsageBarColor : "#10B981"
                                }

                                Text {
                                    text: "CPU Bar"
                                    font.pixelSize: 12
                                    font.weight: Font.Bold
                                    color: cpuBarColorButton.hovered ? "#FFFFFF" : window.textColor

                                    Behavior on color { ColorAnimation { duration: 200 } }
                                }
                            }
                        }

                        background: Rectangle {
                            color: cpuBarColorButton.hovered ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.06)
                            radius: 18
                            border.width: 1
                            border.color: cpuBarColorButton.hovered ? Qt.rgba(1, 1, 1, 0.35) : Qt.rgba(1, 1, 1, 0.18)

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                        onHoveredChanged: scale = hovered ? 1.04 : 1.0
                        onClicked: cpuBarColorDialog.open()
                    }

                    Button {
                        id: ramBarColorButton
                        width: 120
                        height: 36
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: 1.0

                        contentItem: Item {
                            anchors.fill: parent

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 6

                                Rectangle {
                                    width: 20
                                    height: 4
                                    radius: 2
                                    color: modManager ? modManager.ramUsageBarColor : "#3B82F6"
                                }

                                Text {
                                    text: "RAM Bar"
                                    font.pixelSize: 12
                                    font.weight: Font.Bold
                                    color: ramBarColorButton.hovered ? "#FFFFFF" : window.textColor

                                    Behavior on color { ColorAnimation { duration: 200 } }
                                }
                            }
                        }

                        background: Rectangle {
                            color: ramBarColorButton.hovered ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.06)
                            radius: 18
                            border.width: 1
                            border.color: ramBarColorButton.hovered ? Qt.rgba(1, 1, 1, 0.35) : Qt.rgba(1, 1, 1, 0.18)

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                        onHoveredChanged: scale = hovered ? 1.04 : 1.0
                        onClicked: ramBarColorDialog.open()
                    }

                    Button {
                        id: driveBarColorButton
                        width: 130
                        height: 36
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: 1.0

                        contentItem: Item {
                            anchors.fill: parent

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 6

                                Rectangle {
                                    width: 20
                                    height: 4
                                    radius: 2
                                    color: modManager ? modManager.driveUsageBarColor : "#8B5CF6"
                                }

                                Text {
                                    text: "Drive Bars"
                                    font.pixelSize: 12
                                    font.weight: Font.Bold
                                    color: driveBarColorButton.hovered ? "#FFFFFF" : window.textColor

                                    Behavior on color { ColorAnimation { duration: 200 } }
                                }
                            }
                        }

                        background: Rectangle {
                            color: driveBarColorButton.hovered ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.06)
                            radius: 18
                            border.width: 1
                            border.color: driveBarColorButton.hovered ? Qt.rgba(1, 1, 1, 0.35) : Qt.rgba(1, 1, 1, 0.18)

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                        onHoveredChanged: scale = hovered ? 1.04 : 1.0
                        onClicked: driveBarColorDialog.open()
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 20
                    visible: showAdvancedUiColors

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        spacing: 16

                        Rectangle {
                            Layout.preferredWidth: 50
                            Layout.preferredHeight: 50
                            radius: 12
                            color: "transparent"
                            border.width: 0
                            border.color: "transparent"

                            Item {
                                anchors.centerIn: parent
                                width: 28
                                height: 28

                                Image {
                                    id: cardElementColorsIcon
                                    anchors.fill: parent
                                    source: "qrc:/UniversalModManager/Lucide Icons/box.svg"
                                    sourceSize: Qt.size(112, 112)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    antialiasing: true
                                    opacity: 0.9
                                    visible: modManager.theme !== "Glassmorphic"
                                }

                                ColorOverlay {
                                    anchors.fill: parent
                                    source: cardElementColorsIcon
                                    color: window.primaryColor
                                    visible: modManager.theme === "Glassmorphic"
                                    opacity: 0.95
                                }
                            }
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: "Card Element Colors"
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                color: window.textColor
                            }

                            Text {
                                text: "Customize how the app UI looks (cards, tags, buttons, etc)."
                                font.pixelSize: 12
                                color: window.textSecondaryColor
                                opacity: 0.85
                            }
                        }
                    }

                    Rectangle {
                        id: advancedSegmentBar
                        Layout.preferredWidth: 420
                        Layout.preferredHeight: 42
                        radius: 12
                        color: modManager.theme === "Glassmorphic" ?
                            Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08) :
                            Qt.rgba(0, 0, 0, 0.1)
                        border.width: 1.5
                        border.color: modManager.theme === "Glassmorphic" ?
                            Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.15) :
                            window.borderColor

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 5
                            spacing: 6

                            Repeater {
                                model: ["All", "Core", "Files", "Controls"]

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: 8
                                    property int segmentIndex: index
                                    property bool active: advancedUiSegmentIndex === segmentIndex
                                    property bool hovered: false

                                    color: active ?
                                        (modManager.theme === "Glassmorphic" ?
                                            Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.25) :
                                            window.primaryColor) :
                                        (hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent")
                                    border.width: active ? 1.5 : (hovered ? 1 : 0)
                                    border.color: active ?
                                        (modManager.theme === "Glassmorphic" ?
                                            Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.6) :
                                            Qt.rgba(1, 1, 1, 0.3)) :
                                        (hovered ? Qt.rgba(1, 1, 1, 0.15) : "transparent")

                                    Behavior on color { ColorAnimation { duration: 160 } }
                                    Behavior on border.color { ColorAnimation { duration: 160 } }

                                    layer.enabled: active && modManager.theme === "Glassmorphic"
                                    layer.effect: MultiEffect {
                                        shadowEnabled: true
                                        shadowColor: window.primaryColor
                                        shadowVerticalOffset: 0
                                        shadowHorizontalOffset: 0
                                        shadowBlur: 12
                                        shadowOpacity: 0.4
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData
                                        font.pixelSize: 13
                                        font.weight: active ? Font.DemiBold : Font.Medium
                                        color: active ? "#FFFFFF" : (hovered ? window.textColor : window.textSecondaryColor)

                                        Behavior on color { ColorAnimation { duration: 160 } }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onEntered: parent.hovered = true
                                        onExited: parent.hovered = false
                                        onClicked: settingsPage.advancedUiSegmentIndex = segmentIndex
                                    }
                                }
                            }
                        }
                    }

                    Flow {
                        id: advancedUiColorsFlow
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            id: idColorCard
                            width: idColorRow.implicitWidth + 24
                            height: 46
                            radius: 23
                            color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.06) : window.cardColor3
                            border.width: 1
                            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.12) : Qt.alpha(window.borderColor, 0.45)
                            visible: advancedUiSegmentIndex === 0 || advancedUiSegmentIndex === 1

                            layer.enabled: true
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Qt.rgba(0, 0, 0, 0.16)
                                shadowVerticalOffset: 1
                                shadowBlur: 6
                                shadowOpacity: 0.16
                            }

                            property bool hovered: false

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                onEntered: idColorCard.hovered = true
                                onExited: idColorCard.hovered = false
                                onClicked: {
                                    colorPickerDialog.targetProperty = "idColor"
                                    colorPickerDialog.currentColor = modManager.idColor
                                    colorPickerDialog.open()
                                }
                            }

                            scale: hovered ? 1.03 : 1.0
                            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            states: [
                                State {
                                    name: "hovered"
                                    when: idColorCard.hovered
                                    PropertyChanges {
                                        target: idColorCard
                                        border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.25) : window.primaryColor
                                    }
                                }
                            ]

                            RowLayout {
                                id: idColorRow
                                anchors.centerIn: parent
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 26
                                    Layout.preferredHeight: 26
                                    radius: 13

                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: Qt.lighter(modManager.idColor, 1.15) }
                                        GradientStop { position: 0.5; color: modManager.idColor }
                                        GradientStop { position: 1.0; color: Qt.darker(modManager.idColor, 1.1) }
                                    }

                                    border.width: 2
                                    border.color: Qt.rgba(0, 0, 0, 0.12)

                                    layer.enabled: true
                                    layer.effect: MultiEffect {
                                        shadowEnabled: true
                                        shadowColor: modManager.idColor
                                        shadowBlur: 0.4
                                        shadowOpacity: 0.3
                                    }
                                }

                                Text {
                                    text: "ID Color"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: window.textColor
                                    Layout.rightMargin: 4
                                }
                            }
                        }

                        ColorPickerCard {
                            colorLabel: "Folder Mod"
                            colorValue: modManager.folderModColor
                            targetProperty: "folderModColor"
                            visible: advancedUiSegmentIndex === 0 || advancedUiSegmentIndex === 2
                        }

                        ColorPickerCard {
                            colorLabel: "Loose File"
                            colorValue: modManager.looseFileColor
                            targetProperty: "looseFileColor"
                            visible: advancedUiSegmentIndex === 0 || advancedUiSegmentIndex === 2
                        }

                        ColorPickerCard {
                            colorLabel: "PAK"
                            colorValue: modManager.pakColor
                            targetProperty: "pakColor"
                            visible: advancedUiSegmentIndex === 0 || advancedUiSegmentIndex === 2
                        }

                        ColorPickerCard {
                            colorLabel: "UCAS"
                            colorValue: modManager.ucasColor
                            targetProperty: "ucasColor"
                            visible: advancedUiSegmentIndex === 0 || advancedUiSegmentIndex === 2
                        }

                        ColorPickerCard {
                            colorLabel: "UTOC"
                            colorValue: modManager.utocColor
                            targetProperty: "utocColor"
                            visible: advancedUiSegmentIndex === 0 || advancedUiSegmentIndex === 2
                        }

                        ColorPickerCard {
                            colorLabel: "Tags"
                            colorValue: modManager.tagColor
                            targetProperty: "tagColor"
                            visible: advancedUiSegmentIndex === 0 || advancedUiSegmentIndex === 1
                        }

                        ColorPickerCard {
                            colorLabel: "Enable Button"
                            colorValue: modManager.enableButtonColor
                            targetProperty: "enableButtonColor"
                            visible: advancedUiSegmentIndex === 0 || advancedUiSegmentIndex === 3
                        }

                        ColorPickerCard {
                            colorLabel: "Disable Button"
                            colorValue: modManager.disableButtonColor
                            targetProperty: "disableButtonColor"
                            visible: advancedUiSegmentIndex === 0 || advancedUiSegmentIndex === 3
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Qt.rgba(1, 1, 1, 0.08)
                    Layout.topMargin: 8
                    Layout.bottomMargin: 8
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: showUiAccentColors

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Rectangle {
                            Layout.preferredWidth: 44
                            Layout.preferredHeight: 44
                            radius: 10
                            color: "transparent"
                            border.width: 0
                            border.color: "transparent"

                            Item {
                                anchors.centerIn: parent
                                width: 26
                                height: 26

                                Image {
                                    id: uiAccentIcon
                                    anchors.fill: parent
                                    source: "qrc:/UniversalModManager/Lucide Icons/palette.svg"
                                    sourceSize: Qt.size(104, 104)
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    antialiasing: true
                                    opacity: 0.9
                                    visible: modManager.theme !== "Glassmorphic"
                                }

                                ColorOverlay {
                                    anchors.fill: parent
                                    source: uiAccentIcon
                                    color: window.primaryColor
                                    visible: modManager.theme === "Glassmorphic"
                                    opacity: 0.95
                                }
                            }
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: "UI Accent Color"
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                color: window.textColor
                            }

                            Text {
                                text: "Choose your preferred accent color."
                                font.pixelSize: 11
                                color: window.textSecondaryColor
                                opacity: 0.8
                            }
                        }
                    }

                    Row {
                        Layout.fillWidth: true
                        spacing: 8

                        AccentColorButton { colorValue: "#5EC4D9"; colorName: "Cyan" }
                        AccentColorButton { colorValue: "#A78BFA"; colorName: "Purple" }
                        AccentColorButton { colorValue: "#FB7185"; colorName: "Pink" }
                        AccentColorButton { colorValue: "#34D399"; colorName: "Green" }
                        AccentColorButton { colorValue: "#FBBF24"; colorName: "Amber" }
                        AccentColorButton { colorValue: "#F87171"; colorName: "Red" }
                        AccentColorButton { colorValue: "#60A5FA"; colorName: "Blue" }
                        AccentColorButton { colorValue: "#C084FC"; colorName: "Violet" }
                        AccentColorButton { colorValue: "#EC4899"; colorName: "Magenta" }
                        AccentColorButton { colorValue: "#10B981"; colorName: "Emerald" }
                        AccentColorButton { colorValue: "#F59E0B"; colorName: "Orange" }
                        AccentColorButton { colorValue: "#8B5CF6"; colorName: "Indigo" }
                    }

                    Button {
                        id: customColorButtonAdvanced
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 40
                        text: "Custom Color"
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus

                        contentItem: Item {
                            anchors.fill: parent

                            Text {
                                anchors.centerIn: parent
                                text: customColorButtonAdvanced.text
                                font.pixelSize: 12
                                font.weight: Font.Bold
                                color: customColorButtonAdvanced.hovered ? "#FFFFFF" : window.textColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter

                                Behavior on color { ColorAnimation { duration: 150 } }
                            }
                        }

                        background: Rectangle {
                            color: customColorButtonAdvanced.hovered ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.08)
                            radius: 18
                            border.width: 1
                            border.color: customColorButtonAdvanced.hovered ? Qt.rgba(1, 1, 1, 0.4) : Qt.rgba(1, 1, 1, 0.2)

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: {
                            colorPickerDialog.targetProperty = "accentColor"
                            colorPickerDialog.currentColor = modManager.accentColor
                            colorPickerDialog.open()
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Qt.rgba(1, 1, 1, 0.08)
                    Layout.topMargin: 6
                    Layout.bottomMargin: 8
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.rightMargin: 10
                spacing: 12
                visible: generalSectionIndex === 0

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "Custom Wallpaper"
                        font.pixelSize: 15
                        font.weight: 600
                        color: window.textColor
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        color: Qt.rgba(1, 1, 1, 0.12)
                        radius: 12
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.25)

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            text: modManager.wallpaperPath !== "" ? modManager.wallpaperPath.split("/").pop() : "Using gradient background is recommended"
                            font.pixelSize: 13
                            color: window.textSecondaryColor
                            elide: Text.ElideMiddle
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        id: wallpaperSelectButton
                        text: modManager.wallpaperPath !== "" ? "CHANGE" : "SELECT"
                        Layout.preferredWidth: 110
                        Layout.preferredHeight: 52
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: hovered ? 1.05 : 1.0

                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 12
                            font.weight: Font.Black
                            font.letterSpacing: 0.5
                            color: parent.hovered ? "#FFFFFF" : window.textColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        background: Rectangle {
                            color: parent.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.15)
                            radius: 12
                            border.width: 1.5
                            border.color: parent.hovered ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(1, 1, 1, 0.35)

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            wallpaperDialog.open()
                            focus = false
                        }
                    }

                    Button {
                        visible: modManager.wallpaperPath !== ""
                        Layout.preferredWidth: 52
                        Layout.preferredHeight: 52
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: hovered ? 1.05 : 1.0

                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }

                        contentItem: Image {
                            width: 20
                            height: 20
                            source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                            sourceSize: Qt.size(80, 80)
                            fillMode: Image.PreserveAspectFit
                            smooth: true

                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: "#F87171"
                            }
                        }

                        background: Rectangle {
                            color: parent.hovered ? Qt.rgba(248, 113, 113, 0.2) : "transparent"
                            radius: 12
                            border.width: 1.5
                            border.color: parent.hovered ? "#F87171" : Qt.rgba(1, 1, 1, 0.3)

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: modManager.setWallpaperPath("")
                    }
                }
            }
        }
    }

    component AccentColorButton: Rectangle {
        property string colorValue: "#5EC4D9"
        property string colorName: "Color"

        width: 64
        height: 42
        radius: 14
        color: colorValue
        border.width: window.primaryColor === colorValue ? 3 : 0
        border.color: "#FFFFFF"

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.3)
            shadowVerticalOffset: 2
            shadowBlur: 0.4
        }

        Behavior on scale { NumberAnimation { duration: 150 } }
        Behavior on border.width { NumberAnimation { duration: 150 } }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                modManager.setAccentColor(colorValue)
                window.primaryColor = colorValue
                window.accentColor = Qt.lighter(colorValue, 1.3)
            }
            onEntered: if (!pressed) parent.scale = 1.06
            onPressed: parent.scale = 0.9
            onReleased: parent.scale = containsMouse ? 1.06 : 1.0
            onExited: parent.scale = 1.0
        }

        Rectangle {
            visible: window.primaryColor === colorValue
            anchors.centerIn: parent
            width: 20
            height: 20
            radius: 10
            color: "transparent"
            border.width: 3
            border.color: "#FFFFFF"

            Text {
                anchors.centerIn: parent
                text: ""
                font.pixelSize: 14
                font.weight: Font.Bold
                color: "#FFFFFF"
            }
        }
    }
}
