import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

ColumnLayout {
    Layout.fillWidth: true
    Layout.rightMargin: 10
    spacing: 24
    visible: generalSectionIndex === 9

    Text {
        text: "Third-Party Licenses"
        font.pixelSize: 18
        font.weight: 600
        color: window.textColor
        Layout.topMargin: 16
    }

    Text {
        text: "Universal Manager Project uses the following open-source software."
        font.pixelSize: 12
        color: window.textSecondaryColor
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 16
        rowSpacing: 16

        Rectangle {
            property bool hovered: qtLicenseMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonQtLicenseCardGradient : null
            Gradient {
                id: crimsonQtLicenseCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: qtLicenseMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://www.gnu.org/licenses/lgpl-3.0.en.html")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: qtLicenseIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/layout-panel-left.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: qtLicenseIcon; source: qtLicenseIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Qt 6 Framework"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "framework powering Universal Manager"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "LGPL-3.0 License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: qtLicenseMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: cscMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonCscCardGradient : null
            Gradient {
                id: crimsonCscCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: cscMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/filoe/cscore")
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16

                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: cscIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/audio-lines.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: cscIcon; source: cscIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "CSCore"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Audio library for .NET (MS-PL)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "MS-PL License"; font.pixelSize: 11; color: window.primaryColor }
                }

                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: cscMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: cue4parseLicMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonCue4LicCardGradient : null
            Gradient {
                id: crimsonCue4LicCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: cue4parseLicMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/FabianFG/CUE4Parse/blob/master/LICENSE")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: cue4LicIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/code.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: cue4LicIcon; source: cue4LicIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "CUE4Parse"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Unreal Engine package parser (Apache-2.0)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "Apache-2.0 License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: cue4parseLicMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: toolkitMvvmMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonToolkitMvvmCardGradient : null
            Gradient {
                id: crimsonToolkitMvvmCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: toolkitMvvmMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/CommunityToolkit/dotnet")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: toolkitMvvmIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/blocks.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: toolkitMvvmIcon; source: toolkitMvvmIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "CommunityToolkit.Mvvm"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "MVVM Toolkit for .NET (MIT)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "MIT License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: toolkitMvvmMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: toolkitWinUIMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonToolkitWinUICardGradient : null
            Gradient {
                id: crimsonToolkitWinUICardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: toolkitWinUIMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/CommunityToolkit/Windows")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: toolkitWinUIIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/layout-panel-left.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: toolkitWinUIIcon; source: toolkitWinUIIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "CommunityToolkit.WinUI"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "WinUI Controls & helpers (MIT)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "MIT License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: toolkitWinUIMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: webview2MouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonWebview2CardGradient : null
            Gradient {
                id: crimsonWebview2CardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: webview2MouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://learn.microsoft.com/en-us/microsoft-edge/webview2/")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: webview2Icon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/globe.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: webview2Icon; source: webview2Icon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Microsoft WebView2"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Embedded Chromium browser (BSD-3-Clause)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "BSD-3-Clause License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: webview2MouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: winAppSdkMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonWinAppSdkCardGradient : null
            Gradient {
                id: crimsonWinAppSdkCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: winAppSdkMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: winAppSdkIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/app-window.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: winAppSdkIcon; source: winAppSdkIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Windows App SDK"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Windows app development platform (MIT)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "MIT License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: winAppSdkMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: newtonsoftMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonNewtonsoftCardGradient : null
            Gradient {
                id: crimsonNewtonsoftCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: newtonsoftMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/JamesNK/Newtonsoft.Json")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: newtonsoftIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/braces.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: newtonsoftIcon; source: newtonsoftIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Newtonsoft.Json"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Popular JSON framework for .NET (MIT)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "MIT License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: newtonsoftMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: opentkMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonOpentkCardGradient : null
            Gradient {
                id: crimsonOpentkCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: opentkMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/opentk/opentk")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: opentkIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/box.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: opentkIcon; source: opentkIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "OpenTK"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "OpenGL/OpenAL bindings for .NET (MIT)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "MIT License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: opentkMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: serilogMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonSerilogCardGradient : null
            Gradient {
                id: crimsonSerilogCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: serilogMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://serilog.net")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: serilogIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/file-text.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: serilogIcon; source: serilogIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Serilog"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Diagnostic logging library for .NET (Apache-2.0)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "Apache-2.0 License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: serilogMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: imagesharpMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonImagesharpCardGradient : null
            Gradient {
                id: crimsonImagesharpCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: imagesharpMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/SixLabors/ImageSharp")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: imagesharpIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/image.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: imagesharpIcon; source: imagesharpIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "SixLabors.ImageSharp"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Cross-platform 2D graphics library (Six Labors Split License)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "Six Labors Split License 1.0"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: imagesharpMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: skiasharpMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonSkiasharpCardGradient : null
            Gradient {
                id: crimsonSkiasharpCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: skiasharpMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/mono/SkiaSharp")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: skiasharpIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/palette.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: skiasharpIcon; source: skiasharpIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "SkiaSharp"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "2D graphics API for .NET (MIT)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "MIT License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: skiasharpMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: svgSkiaMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonSvgSkiaCardGradient : null
            Gradient {
                id: crimsonSvgSkiaCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: svgSkiaMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/wieslawsoltes/Svg.Skia")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: svgSkiaIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/file-image.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: svgSkiaIcon; source: svgSkiaIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Svg.Skia"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "SVG rendering via SkiaSharp (MIT)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "MIT License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: svgSkiaMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: vgmstreamMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonVgmstreamCardGradient : null
            Gradient {
                id: crimsonVgmstreamCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: vgmstreamMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/vgmstream/vgmstream")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: vgmstreamIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/music.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: vgmstreamIcon; source: vgmstreamIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "vgmstream"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Video game music stream decoder (ISC)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "ISC License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: vgmstreamMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: imguiHookMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonImguiHookCardGradient : null
            Gradient {
                id: crimsonImguiHookCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: imguiHookMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/Sh0ckFR/Universal-Dear-ImGui-Hook")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: imguiHookIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/anchor.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: imguiHookIcon; source: imguiHookIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Universal ImGui Hook"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Dear ImGui hooking library by Sh0ckFR (MIT)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "MIT License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: imguiHookMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: minhookMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonMinhookCardGradient : null
            Gradient {
                id: crimsonMinhookCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: minhookMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/tsudakageyu/minhook")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: minhookIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/cable.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: minhookIcon; source: minhookIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "MinHook"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Minimalistic API hooking library (BSD-2-Clause)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "BSD-2-Clause License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: minhookMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: winSdkBuildMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonWinSdkBuildCardGradient : null
            Gradient {
                id: crimsonWinSdkBuildCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: winSdkBuildMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://learn.microsoft.com/en-us/legal/windows-sdk/license")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: winSdkBuildIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/wrench.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: winSdkBuildIcon; source: winSdkBuildIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Windows SDK Build Tools"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Build tools for Windows apps (Microsoft License)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "Microsoft License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: winSdkBuildMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: cue4NoticeMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonCue4NoticeCardGradient : null
            Gradient {
                id: crimsonCue4NoticeCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: cue4NoticeMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/FabianFG/CUE4Parse/blob/master/NOTICE")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: cue4NoticeIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/file-text.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: cue4NoticeIcon; source: cue4NoticeIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "CUE4Parse NOTICE"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Third-party notices for CUE4Parse dependencies"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "Apache-2.0 NOTICE"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: cue4NoticeMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: flaticonMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonFlaticonCardGradient : null
            Gradient {
                id: crimsonFlaticonCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: flaticonMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://www.flaticon.com/authors/magnific")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: flaticonIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/image.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: flaticonIcon; source: flaticonIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Flaticon / Magnific"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "This project uses Icons from Flation by Magnific (Attribution Required)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "Attribution License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: flaticonMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: serilogConsoleMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonSerilogConsoleCardGradient : null
            Gradient {
                id: crimsonSerilogConsoleCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: serilogConsoleMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/serilog/serilog-sinks-console")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: serilogConsoleIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/code.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: serilogConsoleIcon; source: serilogConsoleIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Serilog.Sinks.Console"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Console logging sink for Serilog (Apache-2.0)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "Apache-2.0 License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: serilogConsoleMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: serilogFileMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonSerilogFileCardGradient : null
            Gradient {
                id: crimsonSerilogFileCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: serilogFileMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/serilog/serilog-sinks-file")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: serilogFileIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/file-text.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: serilogFileIcon; source: serilogFileIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Serilog.Sinks.File"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "File logging sink for Serilog (Apache-2.0)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "Apache-2.0 License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: serilogFileMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: lucideLicenseMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonLucideLicenseCardGradient : null
            Gradient {
                id: crimsonLucideLicenseCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: lucideLicenseMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://lucide.dev")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: lucideLicenseIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/image.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: lucideLicenseIcon; source: lucideLicenseIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Lucide Icons"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Beautiful & consistent SVG icon set used throughout the UI"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "ISC License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: lucideLicenseMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: systemCmdLineMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonSystemCmdLineCardGradient : null
            Gradient {
                id: crimsonSystemCmdLineCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: systemCmdLineMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/dotnet/command-line-api")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: systemCmdLineIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/terminal.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: systemCmdLineIcon; source: systemCmdLineIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "System.CommandLine"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Command-line parsing library for .NET (MIT)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "MIT License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: systemCmdLineMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: newtonsoftLicMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonNewtonsoftLicCardGradient : null
            Gradient {
                id: crimsonNewtonsoftLicCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: newtonsoftLicMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/JamesNK/Newtonsoft.Json")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: newtonsoftLicIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/braces.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: newtonsoftLicIcon; source: newtonsoftLicIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Newtonsoft.Json"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Popular JSON framework for .NET (MIT)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "MIT License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: newtonsoftLicMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: uiverseMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonUiverseCardGradient : null
            Gradient {
                id: crimsonUiverseCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: uiverseMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://uiverse.io/")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: uiverseIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/brush.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: uiverseIcon; source: uiverseIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "Uiverse.io"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Universal Manager used some inspiration design from uiverse.io"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "MIT License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: uiverseMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

        Rectangle {
            property bool hovered: fmodelMouseArea.containsMouse
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic"
                ? (hovered ? window.glassSurfaceHover : window.surfaceColor)
                : (hovered ? Qt.lighter(window.surfaceColor, 1.05) : window.surfaceColor))
            radius: 16
            border.width: 1
            border.color: hovered ? window.primaryColor : window.borderColor
            scale: hovered ? 1.01 : 1
            transformOrigin: Item.Center
            gradient: modManager.theme === "Crimson" ? crimsonFmodelCardGradient : null
            Gradient {
                id: crimsonFmodelCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }
            Behavior on color { ColorAnimation { duration: 160 } }
            Behavior on border.color { ColorAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            MouseArea {
                id: fmodelMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/4sval/FModel")
            }

            RowLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 16
                Rectangle {
                    width: 48; height: 48; radius: 12
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1)
                    Image {
                        id: fmodelIcon; anchors.centerIn: parent
                        source: "qrc:/UniversalModManager/Lucide Icons/package-search.svg"
                        sourceSize: Qt.size(24, 24); opacity: 0.8
                    }
                    ColorOverlay {
                        anchors.fill: fmodelIcon; source: fmodelIcon
                        color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor
                        visible: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: "FModel"; font.pixelSize: 15; font.weight: Font.DemiBold; color: window.textColor }
                    Text { text: "Unreal Engine asset explorer (GPL-3.0)"; font.pixelSize: 12; color: window.textSecondaryColor; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: "GPL-3.0 License"; font.pixelSize: 11; color: window.primaryColor }
                }
                Image {
                    source: "qrc:/UniversalModManager/Lucide Icons/external-link.svg"
                    sourceSize: Qt.size(16, 16)
                    opacity: fmodelMouseArea.containsMouse ? 0.8 : 0.4
                    layer.enabled: modManager.theme === "Glassmorphic" || modManager.theme === "Crimson" || modManager.theme === "Wavey"
                    layer.effect: ColorOverlay { color: modManager.theme === "Wavey" ? window.accentColor : window.primaryColor }
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }
        }

    }

    Item { Layout.fillHeight: true }
}
