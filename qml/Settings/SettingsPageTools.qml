import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

ColumnLayout {
    id: toolsSection
    spacing: 16
    Layout.fillWidth: true

    ColumnLayout {
        Layout.fillWidth: true
        Layout.rightMargin: 10
        spacing: 15
        visible: generalSectionIndex === 1

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                text: "Paths & Locations"
                font.pixelSize: 20
                font.weight: 600
                color: window.textColor
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                id: openAppDataBtn
                implicitHeight: 32
                implicitWidth: appDataBtnText.implicitWidth + 24
                radius: 7
                clip: true
                color: appDataBtnMouse.containsMouse
                    ? (modManager.theme === "Glassmorphic"
                        ? Qt.rgba(1, 1, 1, 0.12)
                        : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08))
                    : "transparent"
                border.width: 1
                border.color: appDataBtnMouse.containsMouse ? window.primaryColor : window.borderColor

                Behavior on color { ColorAnimation { duration: 180 } }
                Behavior on border.color { ColorAnimation { duration: 180 } }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    height: 2
                    width: appDataBtnMouse.containsMouse ? parent.width : 0
                    radius: 1
                    color: window.primaryColor
                    opacity: 0.85
                    Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                }

                Text {
                    id: appDataBtnText
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: -1
                    text: "Data Folder"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: appDataBtnMouse.containsMouse ? window.primaryColor : window.textColor
                    Behavior on color { ColorAnimation { duration: 180 } }
                }

                MouseArea {
                    id: appDataBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: modManager.openAppDataFolder()
                }

                ToolTip {
                    id: appDataToolTip
                    visible: appDataBtnMouse.containsMouse
                    text: "Universal Manager Data Folder"
                    delay: 350
                    padding: 8
                    contentItem: Text { text: appDataToolTip.text; color: window.textColor; font.pixelSize: 11 }
                    background: Rectangle { radius: 8; color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor; border.color: window.borderColor }
                }
            }

            Rectangle {
                width: 32
                height: 32
                radius: 16
                color: helpMouse.containsMouse
                    ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                    : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08)
                border.width: 1
                border.color: helpMouse.containsHover ? window.primaryColor : window.borderColor

                Image {
                    id: helpIconImg
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                    source: "qrc:/UniversalModManager/Lucide Icons/circle-question-mark.svg"
                    sourceSize: Qt.size(32, 32)
                    visible: modManager.theme === "Cozy"
                }

                ColorOverlay {
                    anchors.fill: helpIconImg
                    source: helpIconImg
                    color: window.primaryColor
                    visible: modManager.theme !== "Cozy"
                }

                MouseArea {
                    id: helpMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        pathsHelpDialog.helpTabIndex = 0
                        pathsHelpDialog.open()
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            PathSettingCard {
                iconSource: modManager.theme === "Glassmorphic"
                            ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/exe.png"
                            : (modManager.theme === "Crimson"
                               ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/exe-file.png"
                               : (modManager.theme === "Wavey"
                                  ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/exe.png"
                                  : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/exe.png"))
                label: "Game Executable"
                value: modManager.gamePath ? modManager.gamePath : "Not configured"
                onClicked: {
                    gamePathDialog.open()
                    focus = false
                }
            }

            PathSettingCard {
                iconSource: modManager.theme === "Glassmorphic"
                            ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Mods Dirctory.png"
                            : (modManager.theme === "Crimson"
                               ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Mods Dirctory.png"
                               : (modManager.theme === "Wavey"
                                  ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Mods-directory.png"
                                  : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Mods directory.png"))
                label: "Mods Directory"
                value: modManager.modsPath ? modManager.modsPath : "Not configured"
                onClicked: {
                    modsPathDialog.open()
                    focus = false
                }
                isImportant: true
            }

            PathSettingCard {
                iconSource: modManager.theme === "Glassmorphic"
                            ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/downloads .png"
                            : (modManager.theme === "Crimson"
                               ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/download.png"
                               : (modManager.theme === "Wavey"
                                  ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/downloads.png"
                                  : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Downloads.png"))
                label: "Downloads Folder"
                value: modManager.downloadsPath ? modManager.downloadsPath : "Not configured"
                onClicked: {
                    downloadsPathDialog.open()
                    focus = false
                }
            }

            PathSettingCard {
                iconSource: modManager.theme === "Glassmorphic"
                            ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/backup.png"
                            : (modManager.theme === "Crimson"
                               ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/backup.png"
                               : (modManager.theme === "Wavey"
                                  ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Backup.png"
                                  : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Backup.png"))
                label: "Backup Location"
                value: modManager.backupPath ? modManager.backupPath : "Not configured"
                onClicked: {
                    backupPathDialog.open()
                    focus = false
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.rightMargin: 10
        spacing: 15
        visible: generalSectionIndex === 2

        Text {
            text: "Extraction tool"
            font.pixelSize: 20
            font.weight: 600
            color: window.textColor
        }

        Rectangle {
            id: archiveToolCard
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
            radius: window.cardRadius
            border.width: window.borderWidth + settingsPage.archiveToolHighlightProgress * 1.5
            border.color: settingsPage.archiveToolHighlightProgress > 0.01
                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b,
                          0.45 + settingsPage.archiveToolHighlightProgress * 0.35)
                : window.borderColor
            clip: true

            gradient: modManager.theme === "Crimson" ? crimsonArchiveCardGradient : null

            Gradient {
                id: crimsonArchiveCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }

            layer.enabled: modManager.theme !== "Glassmorphic"
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.3)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 4
                shadowBlur: 0.4
            }

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b,
                               modManager.theme === "Glassmorphic" ? 0.10 : 0.08)
                opacity: settingsPage.archiveToolHighlightProgress
            }

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border.width: 2
                border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b,
                                      0.55 * settingsPage.archiveToolHighlightProgress)
                opacity: settingsPage.archiveToolHighlightProgress
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 8

                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 12
                    color: "transparent"
                    border.color: "transparent"
                    border.width: 0

                    Image {
                        id: zipIcon
                        anchors.centerIn: parent
                        width: modManager.theme === "Crimson" ? 44 : 32
                        height: modManager.theme === "Crimson" ? 44 : 32
                        scale: 1.0 + settingsPage.archiveToolHighlightProgress * 0.08
                        source: modManager.theme === "Glassmorphic"
                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/zip.png"
                                : (modManager.theme === "Crimson"
                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/zip.png"
                                   : (modManager.theme === "Wavey"
                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/zip.png"
                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/zip.png"))
                        sourceSize: Qt.size(width * 2, height * 2)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        antialiasing: true
                        visible: modManager.theme !== "Glassmorphic"
                    }

                    ColorOverlay {
                        anchors.fill: zipIcon
                        source: zipIcon
                        color: window.primaryColor
                        visible: modManager.theme === "Glassmorphic"
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 12
                    color: "transparent"
                    border.color: "transparent"
                    border.width: 0

                    Image {
                        id: rarIcon
                        anchors.centerIn: parent
                        width: modManager.theme === "Crimson" ? 44 : 32
                        height: modManager.theme === "Crimson" ? 44 : 32
                        scale: 1.0 + settingsPage.archiveToolHighlightProgress * 0.08
                        source: modManager.theme === "Glassmorphic"
                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/rar.png"
                                : (modManager.theme === "Crimson"
                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/rar.png"
                                   : (modManager.theme === "Wavey"
                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/rar.png"
                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/rar.png"))
                        sourceSize: Qt.size(width * 2, height * 2)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        antialiasing: true
                    }

                    ColorOverlay {
                        anchors.fill: rarIcon
                        source: rarIcon
                        color: window.primaryColor
                        visible: modManager.theme === "Glassmorphic"
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "7-Zip / WinRAR"
                        font.pixelSize: 16
                        font.weight: 600
                        color: window.textColor
                    }

                    Text {
                        text: modManager.sevenZipStatusText
                        font.pixelSize: 13
                        color: modManager.sevenZipAvailable ? window.textSecondaryColor : "#DC2626"
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }

                Row {
                    spacing: 10

                    Button {
                        id: autoScanButton
                        text: "AUTO SCAN"
                        width: 120
                        height: 44
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
                            font.letterSpacing: 1
                            color: {
                                if (modManager.theme === "Glassmorphic") {
                                    return parent.hovered ? "#FFFFFF" : window.textSecondaryColor
                                } else {
                                    return "#FFFFFF"
                                }
                            }
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        background: Rectangle {
                            color: {
                                if (modManager.theme === "Glassmorphic") {
                                    return parent.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.1)
                                } else {
                                    return window.primaryColor
                                }
                            }
                            radius: 22
                            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 0
                            border.color: modManager.theme === "Glassmorphic" ? (parent.hovered ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(1, 1, 1, 0.35)) : "transparent"

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            scanningPopup.open()
                            scanTimer.start()
                            modManager.autoDetectSevenZipPath()
                        }
                    }

                    Button {
                        id: browseButton
                        text: "BROWSE"
                        width: 120
                        height: 44
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
                            font.letterSpacing: 1
                            color: {
                                if (modManager.theme === "Glassmorphic") {
                                    return parent.hovered ? "#FFFFFF" : window.textSecondaryColor
                                } else {
                                    return "#FFFFFF"
                                }
                            }
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        background: Rectangle {
                            color: {
                                if (modManager.theme === "Glassmorphic") {
                                    return parent.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.1)
                                } else {
                                    return window.primaryColor
                                }
                            }
                            radius: 22
                            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 0
                            border.color: modManager.theme === "Glassmorphic" ? (parent.hovered ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(1, 1, 1, 0.35)) : "transparent"

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            sevenZipPathDialog.open()
                            focus = false
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.rightMargin: 10
        spacing: 15
        visible: generalSectionIndex === 2

        Text {
            text: "Unreal Engine Assets Viewer"
            font.pixelSize: 20
            font.weight: 600
            color: window.textColor
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
            radius: window.cardRadius
            border.width: window.borderWidth
            border.color: window.borderColor

            gradient: modManager.theme === "Crimson" ? crimsonModelViewerCardGradient : null

            Gradient {
                id: crimsonModelViewerCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }

            layer.enabled: modManager.theme !== "Glassmorphic"
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.3)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 4
                shadowBlur: 0.4
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 20

                Rectangle {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56
                    color: "transparent"
                    radius: 14
                    border.color: "transparent"
                    border.width: 0

                    Image {
                        id: modelViewerIcon
                        anchors.centerIn: parent
                        width: 32
                        height: 32
                        source: modManager.theme === "Glassmorphic"
                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/3d-model.png"
                                : (modManager.theme === "Crimson"
                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/3d-model.png"
                                   : (modManager.theme === "Wavey"
                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/3d-model.png"
                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/3d-model.png"))
                        sourceSize: Qt.size(32, 32)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        antialiasing: true
                    }

                    ColorOverlay {
                        anchors.fill: modelViewerIcon
                        source: modelViewerIcon
                        color: window.primaryColor
                        visible: modManager.theme === "Glassmorphic"
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "Assets Viewer"
                        font.pixelSize: 16
                        font.weight: 600
                        color: window.textColor
                    }

                    Text {
                        text: modManager.pakViewerStatusText
                        font.pixelSize: 13
                        color: modManager.pakViewerAvailable ? window.textSecondaryColor : "#DC2626"
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }

                Row {
                    spacing: 10

                    Button {
                        id: launchPakViewerButton
                        text: "LAUNCH"
                        width: 120
                        height: 44
                        flat: true
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        scale: hovered ? 1.05 : 1.0
                        enabled: modManager.pakViewerAvailable

                        Behavior on scale {
                            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                        }

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 12
                            font.weight: Font.Black
                            font.letterSpacing: 1
                            color: {
                                if (modManager.theme === "Glassmorphic") {
                                    return parent.hovered ? "#FFFFFF" : window.textSecondaryColor
                                } else {
                                    return "#FFFFFF"
                                }
                            }
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        background: Rectangle {
                            color: {
                                if (modManager.theme === "Glassmorphic") {
                                    return parent.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.1)
                                } else {
                                    return window.accentColor
                                }
                            }
                            radius: 22
                            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 0
                            border.color: modManager.theme === "Glassmorphic" ? (parent.hovered ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(1, 1, 1, 0.35)) : "transparent"

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {

                            if (aesKeyFieldNew.text !== modManager.aesKey) {
                                modManager.setAesKey(aesKeyFieldNew.text)
                            }
                            modManager.launchPakViewerStandalone()
                            focus = false
                        }
                    }

                    Button {
                        id: browsePakViewerButton
                        text: "BROWSE"
                        width: 120
                        height: 44
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
                            font.letterSpacing: 1
                            color: {
                                if (modManager.theme === "Glassmorphic") {
                                    return parent.hovered ? "#FFFFFF" : window.textSecondaryColor
                                } else {
                                    return "#FFFFFF"
                                }
                            }
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        background: Rectangle {
                            color: {
                                if (modManager.theme === "Glassmorphic") {
                                    return parent.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.1)
                                } else {
                                    return window.primaryColor
                                }
                            }
                            radius: 22
                            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 0
                            border.color: modManager.theme === "Glassmorphic" ? (parent.hovered ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(1, 1, 1, 0.35)) : "transparent"

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            pakViewerPathDialog.open()
                            focus = false
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.rightMargin: 10
        spacing: 15
        visible: generalSectionIndex === 2

        Text {
            text: "Advanced Conflict Detector"
            font.pixelSize: 20
            font.weight: 600
            color: window.textColor
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
            radius: window.cardRadius
            border.width: window.borderWidth
            border.color: window.borderColor

            gradient: modManager.theme === "Crimson" ? crimsonConflictDetectorCardGradient : null

            Gradient {
                id: crimsonConflictDetectorCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }

            layer.enabled: modManager.theme !== "Glassmorphic"
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.3)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 4
                shadowBlur: 0.4
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 20

                Rectangle {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56
                    color: "transparent"
                    radius: 14
                    border.color: "transparent"
                    border.width: 0

                    Image {
                        id: conflictDetectorIcon
                        anchors.centerIn: parent
                        width: 32
                        height: 32
                        source: modManager.theme === "Glassmorphic"
                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Detect_header.png"
                                : (modManager.theme === "Crimson"
                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Detect_header.png"
                                   : (modManager.theme === "Wavey"
                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Detector.png"
                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Detect_heder.png"))
                        sourceSize: Qt.size(64, 64)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        antialiasing: true

                        layer.enabled: modManager.theme === "Glassmorphic"
                        layer.smooth: true
                        layer.textureSize: Qt.size(width * 2, height * 2)
                        layer.effect: ColorOverlay {
                            color: modManager.accentColor
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "Conflict Detector"
                        font.pixelSize: 16
                        font.weight: 600
                        color: window.textColor
                    }

                    Text {
                        text: modManager.conflictDetectorStatusText
                        font.pixelSize: 13
                        color: modManager.conflictDetectorAvailable ? window.textSecondaryColor : "#DC2626"
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }

                Row {
                    spacing: 10

                    Button {
                        id: browseConflictDetectorButton
                        text: "BROWSE"
                        width: 120
                        height: 44
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
                            font.letterSpacing: 1
                            color: {
                                if (modManager.theme === "Glassmorphic") {
                                    return parent.hovered ? "#FFFFFF" : window.textSecondaryColor
                                } else {
                                    return "#FFFFFF"
                                }
                            }
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        background: Rectangle {
                            color: {
                                if (modManager.theme === "Glassmorphic") {
                                    return parent.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.1)
                                } else {
                                    return window.primaryColor
                                }
                            }
                            radius: 22
                            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 0
                            border.color: modManager.theme === "Glassmorphic" ? (parent.hovered ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(1, 1, 1, 0.35)) : "transparent"

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            conflictDetectorPathDialog.open()
                            focus = false
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.rightMargin: 10
        spacing: 15
        visible: generalSectionIndex === 2

        Text {
            text: "Unreal Engine Version Detector"
            font.pixelSize: 20
            font.weight: 600
            color: window.textColor
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
            radius: window.cardRadius
            border.width: window.borderWidth
            border.color: window.borderColor

            gradient: modManager.theme === "Crimson" ? crimsonUEDetectorCardGradient : null

            Gradient {
                id: crimsonUEDetectorCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }

            layer.enabled: modManager.theme !== "Glassmorphic"
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.3)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 4
                shadowBlur: 0.4
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 20

                Rectangle {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56
                    color: "transparent"
                    radius: 14
                    border.color: "transparent"
                    border.width: 0

                    Image {
                        id: ueDetectorIcon
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
                        sourceSize: Qt.size(64, 64)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        antialiasing: true

                        layer.enabled: modManager.theme === "Glassmorphic"
                        layer.smooth: true
                        layer.textureSize: Qt.size(width * 2, height * 2)
                        layer.effect: ColorOverlay {
                            color: modManager.accentColor
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "Version Detector"
                        font.pixelSize: 16
                        font.weight: 600
                        color: window.textColor
                    }

                    Text {
                        text: modManager.ueDetectorStatusText
                        font.pixelSize: 13
                        color: modManager.ueDetectorAvailable ? window.textSecondaryColor : "#DC2626"
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }

                Row {
                    spacing: 10

                    Button {
                        id: browseUEDetectorButton
                        text: "BROWSE"
                        width: 120
                        height: 44
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
                            font.letterSpacing: 1
                            color: {
                                if (modManager.theme === "Glassmorphic") {
                                    return parent.hovered ? "#FFFFFF" : window.textSecondaryColor
                                } else {
                                    return "#FFFFFF"
                                }
                            }
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        background: Rectangle {
                            color: {
                                if (modManager.theme === "Glassmorphic") {
                                    return parent.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.1)
                                } else {
                                    return window.primaryColor
                                }
                            }
                            radius: 22
                            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 0
                            border.color: modManager.theme === "Glassmorphic" ? (parent.hovered ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(1, 1, 1, 0.35)) : "transparent"

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            ueDetectorPathDialog.open()
                            focus = false
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.rightMargin: 10
        spacing: 15
        visible: generalSectionIndex === 1

        Text {
            text: "Assets Viewer game configuration"
            font.pixelSize: 20
            font.weight: 600
            color: window.textColor
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12

            PathSettingCard {
                iconSource: modManager.theme === "Glassmorphic"
                            ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/paks folder.png"
                            : (modManager.theme === "Crimson"
                               ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/paks.png"
                               : (modManager.theme === "Wavey"
                                  ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/paks.png"
                                  : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/paks.png"))
                label: "Game Paks Folder"
                value: modManager.gamePaksPath ? modManager.gamePaksPath : "Not configured"
                onClicked: {
                    gamePaksPathDialog.open()
                    focus = false
                }
                isImportant: true
            }

            PathSettingCard {
                iconSource: modManager.theme === "Glassmorphic"
                            ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Mappings.usmap.png"
                            : (modManager.theme === "Crimson"
                               ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Mappings.usmap.png"
                               : (modManager.theme === "Wavey"
                                  ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Mappings.usmap.png"
                                  : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Mappings.usmap.png"))
                label: "Usmap File"
                value: modManager.usmapFilePath ? modManager.usmapFilePath : "Not set"
                onClicked: {
                    usmapFileDialog.open()
                    focus = false
                }
            }

        }
    }

    component PathSettingCard: Rectangle {
        property string icon: ""
        property string iconSource: ""
        property string label: "Path"
        property string value: "Not set"
        property bool isImportant: false
        property string tooltipText: ""
        signal clicked()

        Layout.fillWidth: true
        Layout.preferredHeight: 80
        color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
        radius: window.cardRadius
        border.width: window.borderWidth
        border.color: pathMouse.containsMouse ? window.primaryColor : window.borderColor

        gradient: modManager.theme === "Crimson" ? crimsonPathCardGradient : null

        Gradient {
            id: crimsonPathCardGradient
            GradientStop { position: 0.0; color: window.crimsonGradientTop }
            GradientStop { position: 1.0; color: window.crimsonGradientBottom }
        }

        Behavior on border.color { ColorAnimation { duration: 200 } }

        layer.enabled: pathMouse.containsMouse && modManager.theme !== "Glassmorphic"
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.3)
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 4
            shadowBlur: 0.3
        }

        MouseArea {
            id: pathMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()

            ToolTip {
                visible: tooltipText !== "" && pathMouse.containsMouse
                delay: 400
                padding: 15

                contentItem: Text {
                    text: tooltipText
                    color: window.textColor
                    font.pixelSize: 14
                    wrapMode: Text.Wrap
                    textFormat: Text.StyledText
                    lineHeight: 1.2
                }

                background: Rectangle {
                    radius: 12
                    color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor
                    border.width: 1
                    border.color: window.borderColor

                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: Qt.rgba(0, 0, 0, 0.3)
                        shadowBlur: 0.5
                        shadowVerticalOffset: 4
                    }
                }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 15

            Rectangle {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                color: "transparent"
                radius: 12
                border.color: "transparent"
                border.width: 0

                Image {
                    id: pathIcon
                    anchors.centerIn: parent
                    width: 32
                    height: 32
                    source: iconSource
                    sourceSize: Qt.size(32, 32)
                    visible: iconSource !== "" && modManager.theme !== "Glassmorphic"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    antialiasing: true
                }

                ColorOverlay {
                    anchors.fill: pathIcon
                    source: pathIcon
                    color: window.primaryColor
                    visible: modManager.theme === "Glassmorphic" && iconSource !== ""
                }

                Text {
                    anchors.centerIn: parent
                    text: icon
                    font.pixelSize: 24
                    visible: iconSource === ""
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: label
                    font.pixelSize: 15
                    font.weight: 600
                    color: window.textColor
                }

                Text {
                    text: value
                    font.pixelSize: 13
                    color: value === "Not configured" ? "#F44336" : window.textSecondaryColor
                    elide: Text.ElideLeft
                    Layout.fillWidth: true
                }
            }

            Text {
                text: ">"
                font.pixelSize: 24
                color: window.textSecondaryColor
                opacity: 0.4
            }
        }
    }
}
