import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import Qt5Compat.GraphicalEffects

Window {
    id: dialog

    property var appWindow: null

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

    signal showSettingsRequested()

    title: "Downloads folder required"
    width: 460
    height: 260
    minimumWidth: 420
    minimumHeight: 246
    visible: false
    color: surfaceColor
    modality: Qt.ApplicationModal
    transientParent: appWindow
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowSystemMenuHint

    Component.onCompleted: titleBarController.attachToWindow(dialog)

    function begin() {
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.topMargin: 16
            Layout.bottomMargin: 14
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: "Downloads folder required"
                font.pixelSize: 17
                font.weight: Font.DemiBold
                color: dialog.textColor
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
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.topMargin: 18
            Layout.bottomMargin: 18
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: 9
                    color: Qt.rgba(dialog.primaryColor.r, dialog.primaryColor.g,
                                   dialog.primaryColor.b, 0.14)

                    Image {
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        source: "qrc:/UniversalModManager/Lucide Icons/folder-cog.svg"
                        sourceSize: Qt.size(18, 18)
                        smooth: true
                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: dialog.primaryColor
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: "Universal Manager needs a downloads folder before it can download mods."
                    font.pixelSize: 13
                    color: dialog.textColor
                    wrapMode: Text.Wrap
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.leftMargin: 48
                text: "Open Settings → Configuration Paths and set a Downloads Folder. You can change it at any time."
                font.pixelSize: 12
                lineHeight: 1.3
                color: Qt.rgba(dialog.textColor.r, dialog.textColor.g, dialog.textColor.b, 0.6)
                wrapMode: Text.Wrap
            }
        }

        Item { Layout.fillHeight: true }

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
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 10

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredWidth: 92
                    Layout.preferredHeight: 36
                    radius: 8
                    color: okayMouse.containsMouse
                        ? Qt.rgba(dialog.textColor.r, dialog.textColor.g, dialog.textColor.b, 0.06)
                        : "transparent"
                    border.width: 1
                    border.color: dialog.lineColor

                    Text {
                        anchors.centerIn: parent
                        text: "Okay"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: dialog.textColor
                    }

                    MouseArea {
                        id: okayMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: dialog.close()
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 118
                    Layout.preferredHeight: 36
                    radius: 8
                    color: showMouse.containsMouse
                        ? Qt.lighter(dialog.primaryColor, 1.08)
                        : dialog.primaryColor

                    Text {
                        anchors.centerIn: parent
                        text: "Show me"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: dialog.darkTextOnPrimary ? "#1A1A1A" : "#FFFFFF"
                    }

                    MouseArea {
                        id: showMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            dialog.close()
                            dialog.showSettingsRequested()
                        }
                    }
                }
            }
        }
    }
}
