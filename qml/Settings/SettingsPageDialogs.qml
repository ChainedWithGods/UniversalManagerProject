import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Item {
    id: settingsDialogs
    anchors.fill: parent

    property alias gamesManagerWelcomeDialog: gamesManagerWelcomeDialog
    property alias managerConsoleRestartDialog: managerConsoleRestartDialog
    property alias pathsHelpDialog: pathsHelpDialog
    property alias scanningPopup: scanningPopup
    property alias scanSuccessPopup: scanSuccessPopup
    property alias scanFailedPopup: scanFailedPopup

    Popup {
        id: gamesManagerWelcomeDialog
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        x: Math.round((settingsPage.width - width) / 2)
        y: Math.round((settingsPage.height - height) / 2)
        width: Math.min(640, settingsPage.width - 48)
        padding: 28

        background: Rectangle {
            radius: 16
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(255, 255, 255, 0.1)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? settingsDialogCozyGradient
                    : modManager.theme === "Crimson" ? settingsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? settingsDialogWaveyGradient
                    : null

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"
            layer.enabled: true
            layer.effect: DropShadow {
                transparentBorder: true
                radius: modManager.theme === "Glassmorphic" ? 32 : 24
                samples: modManager.theme === "Glassmorphic" ? 65 : 49
                color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.5) : Qt.rgba(0, 0, 0, 0.35)
                horizontalOffset: 0
                verticalOffset: 10
            }
        }

        Overlay.modal: Rectangle {
            color: Qt.rgba(0, 0, 0, modManager.theme === "Glassmorphic" ? 0.35 : 0.45)
        }

        contentItem: ColumnLayout {
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: 0
                Layout.alignment: Qt.AlignRight

                Rectangle {
                    width: 34
                    height: 34
                    radius: 10
                    color: closeMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                    border.width: closeMouse.containsMouse ? 1 : 0
                    border.color: window.borderColor
                    Image {
                        id: closeIcon
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                        sourceSize: Qt.size(32, 32)
                        opacity: closeMouse.containsMouse ? 0.9 : 0.5
                    }
                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: gamesManagerWelcomeDialog.close()
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: "Welcome to Games Manager!"
                font.pixelSize: 24
                font.weight: Font.Bold
                color: window.textColor
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: "How to setup profiles?"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                color: window.textSecondaryColor
                wrapMode: Text.WordWrap
                Layout.topMargin: 4
            }

            Item { Layout.preferredHeight: 18 }

            Rectangle {
                id: optionManualCard
                Layout.fillWidth: true
                implicitHeight: optionManualCardContent.implicitHeight + 28
                radius: 14
                color: Qt.rgba(0, 0, 0, 0.03)
                border.width: 1
                border.color: window.borderColor

                ColumnLayout {
                    id: optionManualCardContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 14
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: "Option 1 — Manual"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: window.textColor
                    }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        textFormat: Text.RichText
                        font.pixelSize: 13
                        color: window.textSecondaryColor
                        text:
                            "Click the " +
                            "<span style='color:" + window.primaryColor + "'><b>\"Create New Profile\"</b></span>" +
                            " card and fill the profile details yourself." +
                            "<br><br>" +
                            "<b>You'll set:</b>" +
                            "<ul style='margin:6px 0 0 18px; padding:0;'>" +
                            "<li>Game executable (.exe)</li>" +
                            "<li>Mods folder</li>" +
                            "<li>Unreal Engine version</li>" +
                            "</ul>"
                    }
                }
            }

            Item { Layout.preferredHeight: 12 }

            Rectangle {
                id: optionAutoCard
                Layout.fillWidth: true
                implicitHeight: optionAutoCardContent.implicitHeight + 28
                radius: 14
                color: Qt.rgba(0, 0, 0, 0.03)
                border.width: 1
                border.color: window.borderColor

                ColumnLayout {
                    id: optionAutoCardContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 14
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: "Option 2 — Automatic"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: window.textColor
                    }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        textFormat: Text.RichText
                        font.pixelSize: 13
                        color: window.textSecondaryColor
                        text:
                            "Click " +
                            "<span style='color:" + window.primaryColor + "'><b>\"Scan for Unreal Engine games\"</b></span>" +
                            " to let Universal Manager configure everything automatically." +
                            "<br><br>" +
                            "<b>It will:</b>" +
                            "<ul style='margin:6px 0 0 18px; padding:0;'>" +
                            "<li>Detect the game's executable (.exe)</li>" +
                            "<li>Set the Paks folder</li>" +
                            "<li>Set (or help you create) the Mods folder</li>" +
                            "<li>Pick the correct Unreal Engine version</li>" +
                            "<li>Auto-name the profile from the detected game</li>" +
                            "<li>Auto-detect the Mods folder, whether it's ~Mods/Mods/LogicMods</li>" +
                            "</ul>"
                    }
                }
            }

            Item { Layout.preferredHeight: 18 }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Layout.alignment: Qt.AlignRight

                CheckBox {
                    id: gamesManagerDontShowAgainCheckbox
                    text: "Don't show this message again"
                    checked: modManager.gamesManagerWelcomeShown
                    onCheckedChanged: modManager.gamesManagerWelcomeShown = checked

                    indicator: Rectangle {
                        implicitWidth: 20
                        implicitHeight: 20
                        x: gamesManagerDontShowAgainCheckbox.leftPadding
                        y: parent.height / 2 - height / 2
                        radius: 4
                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.1) : "transparent"
                        border.color: gamesManagerDontShowAgainCheckbox.checked ? window.primaryColor : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.4) : window.borderColor)
                        border.width: 2

                        Rectangle {
                            width: 12
                            height: 12
                            x: 4
                            y: 4
                            radius: 2
                            color: window.primaryColor
                            visible: gamesManagerDontShowAgainCheckbox.checked
                        }
                    }

                    contentItem: Text {
                        text: gamesManagerDontShowAgainCheckbox.text
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                        leftPadding: gamesManagerDontShowAgainCheckbox.indicator.width + 8
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    id: gamesManagerLearnHowButton
                    width: 120
                    height: 40
                    radius: 12
                    color: learnHowMouse.containsMouse
                        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)
                        : "transparent"
                    border.width: 1
                    border.color: window.primaryColor

                    Text {
                        anchors.centerIn: parent
                        text: "Learn How"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: window.primaryColor
                    }

                    MouseArea {
                        id: learnHowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            settingsTabIndex = 0
                            generalSectionIndex = 8
                            gamesManagerWelcomeDialog.close()
                        }
                    }
                }

                Rectangle {
                    id: gamesManagerOkButton
                    width: 120
                    height: 40
                    radius: 12
                    color: okMouse.containsMouse ? Qt.lighter(window.primaryColor, 1.05) : window.primaryColor
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.18)

                    Text {
                        anchors.centerIn: parent
                        text: "Got it"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: "white"
                    }

                    MouseArea {
                        id: okMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            gamesManagerWelcomeDialog.close()
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: managerConsoleRestartDialog
        modal: true
        focus: true
        anchors.centerIn: parent
        width: Math.min(560, settingsPage.width - 48)
        padding: 28
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            radius: 20
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.15)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? settingsDialogCozyGradient
                    : modManager.theme === "Crimson" ? settingsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? settingsDialogWaveyGradient
                    : null

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"
            layer.enabled: true
            layer.effect: DropShadow {
                transparentBorder: true
                radius: 24
                samples: 49
                color: Qt.rgba(0, 0, 0, 0.35)
                horizontalOffset: 0
                verticalOffset: 10
            }
        }

        Overlay.modal: Rectangle {
            color: Qt.rgba(0, 0, 0, modManager.theme === "Glassmorphic" ? 0.35 : 0.45)
        }

        contentItem: ColumnLayout {
            spacing: 18

            Text {
                Layout.fillWidth: true
                text: "Restart Universal Manager?"
                font.pixelSize: 22
                font.weight: Font.Bold
                color: window.textColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                text: "Universal Manager needs to restart to apply. Would you like to restart now?"
                font.pixelSize: 14
                color: window.textSecondaryColor
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 12

                Rectangle {
                    id: laterButton
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 48
                    radius: 24
                    color: laterMouse.containsMouse
                        ? (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.12) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08))
                        : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08) : "transparent")
                    border.width: 1
                    border.color: window.borderColor
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text {
                        anchors.centerIn: parent
                        text: "Nah"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                    }

                    MouseArea {
                        id: laterMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: managerConsoleRestartDialog.close()
                    }
                }

                Rectangle {
                    id: restartButton
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 48
                    radius: 24
                    color: restartMouse.containsMouse ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                    border.width: 1.5
                    border.color: restartMouse.containsMouse ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)
                    Behavior on color { ColorAnimation { duration: 200 } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }

                    Text {
                        anchors.centerIn: parent
                        text: "Restart Now"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        color: restartMouse.containsMouse ? "#FFFFFF" : window.textColor
                        Behavior on color { ColorAnimation { duration: 200 } }
                    }

                    MouseArea {
                        id: restartMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            managerConsoleRestartDialog.close()
                            window._forceQuit = true
                            modManager.restartApplication()
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: pathsHelpDialog
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        x: Math.round((settingsPage.width - width) / 2)
        y: Math.round((settingsPage.height - height) / 2)
        width: Math.min(760, settingsPage.width - 48)
        height: Math.min(contentItem.implicitHeight, settingsPage.height - 80)
        padding: 0

        background: Rectangle {
            radius: 16
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.15)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? settingsDialogCozyGradient
                    : modManager.theme === "Crimson" ? settingsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? settingsDialogWaveyGradient
                    : null

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"
            layer.enabled: true
            layer.effect: DropShadow {
                transparentBorder: true
                radius: 24
                samples: 49
                color: Qt.rgba(0, 0, 0, 0.35)
                horizontalOffset: 0
                verticalOffset: 10
            }
        }

        Overlay.modal: Rectangle {
            color: Qt.rgba(0, 0, 0, modManager.theme === "Glassmorphic" ? 0.35 : 0.45)
        }

        property int helpTabIndex: 0

        contentItem: ColumnLayout {
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: 0
                Layout.margins: 20

                Text {
                    text: "Paths & Locations Help"
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: window.textColor
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    width: 34
                    height: 34
                    radius: 10
                    color: closeHelpMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                    border.width: closeHelpMouse.containsMouse ? 1 : 0
                    border.color: window.borderColor
                    Image {
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                        sourceSize: Qt.size(32, 32)
                        opacity: closeHelpMouse.containsMouse ? 0.9 : 0.5
                    }
                    MouseArea {
                        id: closeHelpMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: pathsHelpDialog.close()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 0
                Layout.leftMargin: 20
                Layout.rightMargin: 20

                Repeater {
                    model: ["USMAP File", "Game Paks", "Game Executable"]

                    Rectangle {
                        Layout.fillWidth: true
                        height: 40
                        radius: 8
                        color: pathsHelpDialog.helpTabIndex === index
                            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                            : (helpTabMouse.containsMouse
                                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08)
                                : "transparent")

                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            font.pixelSize: 13
                            font.weight: pathsHelpDialog.helpTabIndex === index ? Font.DemiBold : Font.Normal
                            color: pathsHelpDialog.helpTabIndex === index ? window.primaryColor : window.textColor
                        }

                        MouseArea {
                            id: helpTabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: pathsHelpDialog.helpTabIndex = index
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: window.borderColor
                Layout.topMargin: 12
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.margins: 20
                currentIndex: pathsHelpDialog.helpTabIndex
                implicitHeight: pathsHelpDialog.helpTabIndex === 0 ? usmapContent.implicitHeight :
                               pathsHelpDialog.helpTabIndex === 1 ? paksContent.implicitHeight :
                               exeContent.implicitHeight

                ColumnLayout {
                    id: usmapContent
                    Layout.fillWidth: true
                    spacing: 14

                        Text {
                            Layout.fillWidth: true
                            text: "What is a USMAP file?"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            color: window.textColor
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "USMAP files serve as a dictionary for Unreal Engine games."
                            font.pixelSize: 13
                            color: window.textSecondaryColor
                            wrapMode: Text.WordWrap
                            lineHeight: 1.4
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Since modern UE4 and UE5 titles use Unversioned Properties to strip metadata for performance, game data appears anonymous to external tools."
                            font.pixelSize: 13
                            color: window.textSecondaryColor
                            wrapMode: Text.WordWrap
                            lineHeight: 1.4
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "A usmap file provides the necessary type and property mappings to restore those names, making the data readable for modding and analysis."
                            font.pixelSize: 13
                            color: window.textSecondaryColor
                            wrapMode: Text.WordWrap
                            lineHeight: 1.4
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: window.borderColor
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Why Universal Manager needs it"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: window.textColor
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: window.primaryColor
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 6
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: "Structure Export"
                                    font.pixelSize: 13; font.weight: Font.DemiBold
                                    color: window.textColor
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Without it, mod structure data export will not work."
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.4
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: window.primaryColor
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 6
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: "Assets Viewer Compatibility"
                                    font.pixelSize: 13; font.weight: Font.DemiBold
                                    color: window.textColor
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Required if your game doesn't load correctly, as it provides the mapping data the tool needs to parse your game's assets."
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.4
                                }
                            }
                        }
                }

                ColumnLayout {
                    id: paksContent
                    Layout.fillWidth: true
                    spacing: 14

                        Text {
                            Layout.fillWidth: true
                            text: "What is the Game Paks Folder?"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            color: window.textColor
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "The Game Paks Folder is the directory where your game's Paks/IOStore files are stored."
                            font.pixelSize: 13
                            color: window.textSecondaryColor
                            wrapMode: Text.WordWrap
                            lineHeight: 1.4
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "In Unreal Engine games, this is typically located inside the game's Content/Paks directory."
                            font.pixelSize: 13
                            color: window.textSecondaryColor
                            wrapMode: Text.WordWrap
                            lineHeight: 1.4
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "These files contain compressed game assets such as textures, models, sounds, materials, etc."
                            font.pixelSize: 13
                            color: window.textSecondaryColor
                            wrapMode: Text.WordWrap
                            lineHeight: 1.4
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: window.borderColor
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Why it's needed"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: window.textColor
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: window.primaryColor
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 6
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: "Structure Export"
                                    font.pixelSize: 13; font.weight: Font.DemiBold
                                    color: window.textColor
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Reads pak contents to export mod structures."
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.4
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: window.primaryColor
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 6
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: "Conflict Detection"
                                    font.pixelSize: 13; font.weight: Font.DemiBold
                                    color: window.textColor
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Scans paks to detect file collisions between mods."
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.4
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: window.primaryColor
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 6
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: "Unreal Engine Assets Viewer"
                                    font.pixelSize: 13; font.weight: Font.DemiBold
                                    color: window.textColor
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Allows you to view assets in the game such as mods or the game assets."
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.4
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: window.primaryColor
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 6
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: "Mod Installation"
                                    font.pixelSize: 13; font.weight: Font.DemiBold
                                    color: window.textColor
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Knows where to place new mod files."
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.4
                                }
                            }
                    }
                }

                ColumnLayout {
                    id: exeContent
                    Layout.fillWidth: true
                    spacing: 14

                        Text {
                            Layout.fillWidth: true
                            text: "Why is the Game Executable needed?"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            color: window.textColor
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "The Game Executable path points to your game's main .exe file. This is the entry point that launches the game."
                            font.pixelSize: 13
                            color: window.textSecondaryColor
                            wrapMode: Text.WordWrap
                            lineHeight: 1.4
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: window.borderColor
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Why Universal Manager needs it"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: window.textColor
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: window.primaryColor
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 6
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: "Launch Game"
                                    font.pixelSize: 13; font.weight: Font.DemiBold
                                    color: window.textColor
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Starts the game directly from Universal Manager."
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.4
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: window.primaryColor
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 6
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: "DLL Injection"
                                    font.pixelSize: 13; font.weight: Font.DemiBold
                                    color: window.textColor
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Injects Avalonium runtime engine into the running process."
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.4
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: window.primaryColor
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 6
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: "Game Detection"
                                    font.pixelSize: 13; font.weight: Font.DemiBold
                                    color: window.textColor
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Auto-detects Unreal Engine version and paths relative to the executable location."
                                    font.pixelSize: 12
                                    color: window.textSecondaryColor
                                    wrapMode: Text.WordWrap
                                    lineHeight: 1.4
                                }
                            }
                    }
                }
            }
        }
    }

    Popup {
        id: scanningPopup
        width: 220
        height: 120
        modal: true
        focus: true
        anchors.centerIn: parent
        padding: 30
        dim: true

        background: Rectangle {
            radius: window.cardRadius
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.15)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? settingsDialogCozyGradient
                    : modManager.theme === "Crimson" ? settingsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? settingsDialogWaveyGradient
                    : null

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"
        }

        Column {
            anchors.centerIn: parent
            spacing: 15

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: scanningPopup.visible
            }

            Text {
                text: "Scanning..."
                font.pixelSize: 16
                color: window.textColor
            }
        }
    }

    Popup {
        id: scanSuccessPopup
        width: 440
        height: 320
        modal: true
        focus: true
        anchors.centerIn: parent
        padding: 0
        dim: true

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutQuad }
            NumberAnimation { property: "scale"; from: 0.9; to: 1.0; duration: 200; easing.type: Easing.OutBack }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 150; easing.type: Easing.InQuad }
            NumberAnimation { property: "scale"; from: 1.0; to: 0.95; duration: 150; easing.type: Easing.InQuad }
        }

background: Rectangle {
            radius: window.cardRadius
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.4)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? settingsDialogCozyGradient
                    : modManager.theme === "Crimson" ? settingsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? settingsDialogWaveyGradient
                    : null

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 12
                shadowBlur: 0.6
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 32
            spacing: 20

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 56
                height: 56
                color: "transparent"
                radius: 28

                Image {
                    id: successIcon
                    anchors.centerIn: parent
                    source: modManager.theme === "Glassmorphic"
                            ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Checkmark.png"
                            : (modManager.theme === "Crimson"
                               ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Checkmark.png"
                               : (modManager.theme === "Wavey"
                                  ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Checkmark.png"
                                  : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Checkmark.png"))
                    width: 56
                    height: 56
                    sourceSize: Qt.size(56, 56)
                    fillMode: Image.PreserveAspectFit
                    visible: modManager.theme !== "Glassmorphic"
                }

                ColorOverlay {
                    anchors.fill: successIcon
                    source: successIcon
                    color: window.primaryColor
                    visible: modManager.theme === "Glassmorphic"
                }
            }

            Column {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Scan Successful"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    color: window.textColor
                }

                Text {
                    width: parent.width
                    text: "Found archiver at:\n" + modManager.sevenZipPath
                    wrapMode: Text.WordWrap
                    color: window.textSecondaryColor
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1.4
                }
            }

            Item { Layout.preferredHeight: 10 }

            Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 140
                Layout.preferredHeight: 48
                text: "Okay"
                flat: true
                hoverEnabled: true

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 15
                    font.weight: Font.Bold
                    color: parent.hovered ? "#FFFFFF" : window.textColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    Behavior on color { ColorAnimation { duration: 200 } }
                }

                background: Rectangle {
                    color: parent.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                    radius: 24
                    border.width: 1.5
                    border.color: parent.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                    Behavior on color { ColorAnimation { duration: 200 } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }
                }

                onClicked: scanSuccessPopup.close()
            }
        }
    }

    Popup {
        id: scanFailedPopup
        width: 480
        height: 420
        modal: true
        focus: true
        anchors.centerIn: parent
        padding: 0
        dim: true

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutQuad }
            NumberAnimation { property: "scale"; from: 0.9; to: 1.0; duration: 200; easing.type: Easing.OutBack }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 150; easing.type: Easing.InQuad }
            NumberAnimation { property: "scale"; from: 1.0; to: 0.95; duration: 150; easing.type: Easing.InQuad }
        }

background: Rectangle {
            radius: window.cardRadius
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.4)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? settingsDialogCozyGradient
                    : modManager.theme === "Crimson" ? settingsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? settingsDialogWaveyGradient
                    : null

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 12
                shadowBlur: 0.6
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 32
            spacing: 18

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 56
                height: 56
                color: "transparent"
                radius: 28

                Image {
                    id: failedIcon
                    anchors.centerIn: parent
                    source: modManager.theme === "Glassmorphic"
                            ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/failed.png"
                            : (modManager.theme === "Crimson"
                               ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/failed.png"
                               : (modManager.theme === "Wavey"
                                  ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/failed.png"
                                  : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/failed.png"))
                    width: 56
                    height: 56
                    sourceSize: Qt.size(56, 56)
                    fillMode: Image.PreserveAspectFit
                    visible: modManager.theme !== "Glassmorphic"
                }

                ColorOverlay {
                    anchors.fill: failedIcon
                    source: failedIcon
                    color: window.primaryColor
                    visible: modManager.theme === "Glassmorphic"
                }
            }

            Column {
                Layout.fillWidth: true
                spacing: 14

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Scan Failed"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    color: window.textColor
                }

                Text {
                    width: parent.width
                    text: "Couldn't find 7-Zip or WinRAR automatically."
                    wrapMode: Text.WordWrap
                    color: window.textSecondaryColor
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1.4
                }

                Rectangle {
                    width: parent.width
                    height: downloadLinks.height + 28
                    color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.1) : Qt.rgba(0, 0, 0, 0.03)
                    radius: window.cardRadius
                    border.width: 1
                    border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.2) : Qt.rgba(0, 0, 0, 0.08)

                    Column {
                        id: downloadLinks
                        anchors.centerIn: parent
                        spacing: 10

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Download from:"
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: window.textSecondaryColor
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 20

                            Button {
                                text: "7-Zip Official"
                                flat: true
                                hoverEnabled: true

                                contentItem: Text {
                                    text: parent.text
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    font.underline: parent.hovered
                                    color: window.primaryColor
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                background: Rectangle {
                                    color: parent.hovered ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1) : "transparent"
                                    radius: 8
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }

                                onClicked: Qt.openUrlExternally("https://www.7-zip.org/")
                            }

                            Rectangle {
                                width: 1
                                height: 20
                                anchors.verticalCenter: parent.verticalCenter
                                color: window.borderColor
                            }

                            Button {
                                text: "WinRAR Official"
                                flat: true
                                hoverEnabled: true

                                contentItem: Text {
                                    text: parent.text
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    font.underline: parent.hovered
                                    color: window.primaryColor
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                background: Rectangle {
                                    color: parent.hovered ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.1) : "transparent"
                                    radius: 8
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }

                                onClicked: Qt.openUrlExternally("https://www.win-rar.com/")
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 10 }

            Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 140
                Layout.preferredHeight: 48
                text: "Okay"
                flat: true
                hoverEnabled: true

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 15
                    font.weight: Font.Bold
                    color: parent.hovered ? "#FFFFFF" : window.textColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    Behavior on color { ColorAnimation { duration: 200 } }
                }

                background: Rectangle {
                    color: parent.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                    radius: 24
                    border.width: 1.5
                    border.color: parent.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                    Behavior on color { ColorAnimation { duration: 200 } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }
                }

                onClicked: scanFailedPopup.close()
            }
        }
    }
}
