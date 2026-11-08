import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Qt5Compat.GraphicalEffects

// Lists the Lua folders created on startup for profiles that could not get one
// when they were first added, for example before the app ran as administrator.
Window {
    id: dialog

    property var appWindow: null
    property var profiles: []
    property real contentOpacity: 1
    property real contentScale: 1
    property real entranceOffset: 0

    readonly property color surfaceColor: appWindow
        ? (modManager.theme === "Glassmorphic"
            ? Qt.rgba(0.12, 0.12, 0.12, 0.98)
            : appWindow.surfaceColor)
        : "#FFFFFF"
    readonly property color textColor: appWindow ? appWindow.textColor : "#3E3E3E"
    readonly property color primaryColor: appWindow ? appWindow.primaryColor : "#BE9F7E"
    readonly property color lineColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.16)
    readonly property bool darkTextOnPrimary:
        (0.2126 * primaryColor.r + 0.7152 * primaryColor.g + 0.0722 * primaryColor.b) > 0.55

    function showProfiles(list) {
        profiles = list || []
        if (profiles.length === 0)
            return

        contentOpacity = 0
        contentScale = 0.94
        entranceOffset = 8

        visible = true
        Qt.callLater(centerOnAppWindow)
        raise()
        requestActivate()
        entranceAnimation.restart()
    }

    function centerOnAppWindow() {
        if (!appWindow)
            return
        x = Math.round(appWindow.x + (appWindow.width - width) / 2)
        y = Math.round(appWindow.y + (appWindow.height - height) / 2)
    }

    title: "Lua profiles restored"
    width: 560
    height: Math.max(300, Math.min(720, contentColumn.implicitHeight))
    visible: false
    color: surfaceColor
    modality: Qt.ApplicationModal
    transientParent: appWindow
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowSystemMenuHint

    Component.onCompleted: titleBarController.attachToWindow(dialog)

    ParallelAnimation {
        id: entranceAnimation
        NumberAnimation { target: dialog; property: "contentOpacity"; to: 1; duration: 220 }
        NumberAnimation { target: dialog; property: "contentScale"; to: 1; duration: 320; easing.type: Easing.OutQuint }
        NumberAnimation { target: dialog; property: "entranceOffset"; to: 0; duration: 320; easing.type: Easing.OutQuint }
    }

    Item {
        id: contentRoot
        anchors.fill: parent
        opacity: dialog.contentOpacity
        scale: dialog.contentScale
        transform: Translate { y: dialog.entranceOffset }
        transformOrigin: Item.Center

        ColumnLayout {
            id: contentColumn
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                Layout.topMargin: 20
                Layout.bottomMargin: 16
                spacing: 14

                Rectangle {
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 42
                    radius: 11
                    color: Qt.rgba(dialog.primaryColor.r, dialog.primaryColor.g,
                                   dialog.primaryColor.b, 0.14)

                    Image {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        source: "qrc:/UniversalModManager/Lucide Icons/folder-check.svg"
                        sourceSize: Qt.size(20, 20)
                        smooth: true
                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: dialog.primaryColor
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Text {
                        Layout.fillWidth: true
                        text: "Lua profiles restored"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        color: dialog.textColor
                        wrapMode: Text.Wrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: dialog.profiles.length === 1
                              ? "Universal Manager created the missing Lua folder for this profile."
                              : "Universal Manager created the missing Lua folders for these profiles."
                        font.pixelSize: 12
                        color: Qt.rgba(dialog.textColor.r, dialog.textColor.g, dialog.textColor.b, 0.62)
                        wrapMode: Text.Wrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: dialog.lineColor
                opacity: 0.6
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 22
                Layout.rightMargin: 22
                Layout.topMargin: 16
                Layout.bottomMargin: 18
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: dialog.profiles.length === 1
                          ? "This profile can now load Lua mods:"
                          : "These profiles can now load Lua mods:"
                    font.pixelSize: 12
                    color: Qt.rgba(dialog.textColor.r, dialog.textColor.g, dialog.textColor.b, 0.62)
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(profileColumn.implicitHeight + 8, 300)
                    radius: 12
                    color: Qt.rgba(dialog.textColor.r, dialog.textColor.g, dialog.textColor.b, 0.035)
                    border.width: 1
                    border.color: dialog.lineColor

                    Flickable {
                        anchors.fill: parent
                        anchors.margins: 4
                        clip: true
                        contentWidth: width
                        contentHeight: profileColumn.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        Column {
                            id: profileColumn
                            width: parent.width
                            spacing: 0

                            Repeater {
                                model: dialog.profiles

                                delegate: Item {
                                    width: profileColumn.width
                                    height: 58

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 8
                                        spacing: 12

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                font.pixelSize: 13
                                                font.weight: Font.DemiBold
                                                color: dialog.textColor
                                                elide: Text.ElideRight
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.path
                                                font.pixelSize: 11
                                                color: Qt.rgba(dialog.textColor.r, dialog.textColor.g,
                                                               dialog.textColor.b, 0.58)
                                                elide: Text.ElideMiddle
                                            }
                                        }

                                        Rectangle {
                                            Layout.preferredWidth: 70
                                            Layout.preferredHeight: 30
                                            radius: 8
                                            color: openMouse.containsMouse
                                                ? Qt.lighter(dialog.primaryColor, 1.08)
                                                : dialog.primaryColor

                                            Text {
                                                anchors.centerIn: parent
                                                text: "Open"
                                                font.pixelSize: 12
                                                font.weight: Font.DemiBold
                                                color: dialog.darkTextOnPrimary ? "#1A1A1A" : "#FFFFFF"
                                            }

                                            MouseArea {
                                                id: openMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: modManager.openFolderPath(modelData.path)
                                            }
                                        }
                                    }

                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        width: parent.width
                                        height: 1
                                        color: dialog.lineColor
                                        opacity: 0.5
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                color: "transparent"

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: dialog.lineColor
                    opacity: 0.6
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 22
                    anchors.rightMargin: 22

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        Layout.preferredWidth: 88
                        Layout.preferredHeight: 38
                        radius: 9
                        color: closeMouse.containsMouse
                            ? Qt.darker(dialog.primaryColor, 1.1)
                            : dialog.primaryColor

                        Text {
                            anchors.centerIn: parent
                            text: "Close"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: dialog.darkTextOnPrimary ? "#1A1A1A" : "#FFFFFF"
                        }

                        MouseArea {
                            id: closeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dialog.close()
                        }
                    }
                }
            }
        }
    }
}
