pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Qt5Compat.GraphicalEffects

// Prompts when the GitHub releases feed has a newer build. Downloading stays
// inside the dialog; "Download later" only closes it and the next launch
// checks again.
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
    readonly property string uiState: updateChecker.state

    title: "Update available"
    width: 560
    height: 560
    minimumWidth: 480
    minimumHeight: 440
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

    function formatBytes(bytes) {
        if (!bytes || bytes < 0)
            return "0 B"
        var units = ["B", "KB", "MB", "GB"]
        var value = bytes
        var index = 0
        while (value >= 1024 && index < units.length - 1) {
            value /= 1024
            index++
        }
        return (index === 0 ? value : value.toFixed(1)) + " " + units[index]
    }

    function progressPercent() {
        if (updateChecker.bytesTotal <= 0)
            return ""
        return Math.round(updateChecker.downloadProgress * 100) + "%"
    }

    onClosing: function(close) {
        if (updateChecker.state === "downloading")
            updateChecker.cancelDownload()
    }

    component ActionButton: Rectangle {
        id: control

        property string label: ""
        property bool accent: false
        property color accentColor: dialog.primaryColor
        property color labelColor: dialog.textColor
        property color borderLineColor: dialog.lineColor
        property bool lightLabelOnAccent: dialog.darkTextOnPrimary
        readonly property bool hovered: hoverArea.containsMouse

        signal clicked()

        implicitWidth: labelText.implicitWidth + 36
        implicitHeight: 36
        radius: 8
        color: accent
            ? (hovered ? Qt.lighter(accentColor, 1.08) : accentColor)
            : (hovered ? Qt.rgba(labelColor.r, labelColor.g, labelColor.b, 0.06) : "transparent")
        border.width: accent ? 0 : 1
        border.color: borderLineColor

        Text {
            id: labelText
            anchors.centerIn: parent
            text: control.label
            font.pixelSize: 13
            font.weight: Font.DemiBold
            color: control.accent
                ? (control.lightLabelOnAccent ? "#1A1A1A" : "#FFFFFF")
                : control.labelColor
        }

        MouseArea {
            id: hoverArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: control.clicked()
        }
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
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                radius: 10
                color: Qt.rgba(dialog.primaryColor.r, dialog.primaryColor.g,
                               dialog.primaryColor.b, 0.14)

                Image {
                    anchors.centerIn: parent
                    width: 20
                    height: 20
                    source: "qrc:/UniversalModManager/Lucide Icons/cloud-download.svg"
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
                    text: dialog.uiState === "downloaded" ? "Update downloaded" : "Update available"
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    color: dialog.textColor
                }

                Text {
                    Layout.fillWidth: true
                    text: "Universal Manager " + updateChecker.latestVersion
                          + " is available. You are running " + updateChecker.currentVersion + "."
                    font.pixelSize: 12
                    color: Qt.rgba(dialog.textColor.r, dialog.textColor.g, dialog.textColor.b, 0.65)
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
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.topMargin: 16
            Layout.bottomMargin: 16
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: "What's new"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: dialog.textColor
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 12
                color: Qt.rgba(dialog.textColor.r, dialog.textColor.g, dialog.textColor.b, 0.035)
                border.width: 1
                border.color: dialog.lineColor

                Flickable {
                    id: changelogFlickable
                    anchors.fill: parent
                    anchors.margins: 14
                    clip: true
                    contentWidth: width
                    contentHeight: changelogText.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    Text {
                        id: changelogText
                        width: changelogFlickable.width
                        text: updateChecker.changelog
                        textFormat: Text.MarkdownText
                        wrapMode: Text.Wrap
                        font.pixelSize: 13
                        lineHeight: 1.15
                        color: dialog.textColor
                        onLinkActivated: function(link) {
                            Qt.openUrlExternally(link)
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                visible: dialog.uiState === "downloading"
                         || dialog.uiState === "downloaded"
                         || dialog.uiState === "failed"

                Text {
                    Layout.fillWidth: true
                    visible: dialog.uiState === "downloading"
                    text: "Pulling the latest version from GitHub..."
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: dialog.textColor
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: dialog.uiState === "downloading"
                    spacing: 8

                    Text {
                        text: "View release page"
                        font.pixelSize: 12
                        font.underline: true
                        color: dialog.primaryColor

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: updateChecker.openReleasePage()
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Text {
                        text: dialog.progressPercent()
                        font.pixelSize: 12
                        color: Qt.rgba(dialog.textColor.r, dialog.textColor.g, dialog.textColor.b, 0.65)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    radius: 3
                    visible: dialog.uiState === "downloading"
                    color: Qt.rgba(dialog.textColor.r, dialog.textColor.g, dialog.textColor.b, 0.12)

                    Rectangle {
                        width: Math.max(0, Math.min(1, updateChecker.downloadProgress)) * parent.width
                        height: parent.height
                        radius: 3
                        color: dialog.primaryColor
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: dialog.uiState === "downloading"
                    text: dialog.formatBytes(updateChecker.bytesReceived)
                          + (updateChecker.bytesTotal > 0
                             ? " of " + dialog.formatBytes(updateChecker.bytesTotal)
                             : "")
                    font.pixelSize: 11
                    color: Qt.rgba(dialog.textColor.r, dialog.textColor.g, dialog.textColor.b, 0.55)
                }

                Text {
                    Layout.fillWidth: true
                    visible: dialog.uiState === "downloaded"
                    text: "Download complete"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: dialog.textColor
                }

                Text {
                    Layout.fillWidth: true
                    visible: dialog.uiState === "downloaded"
                    text: updateChecker.downloadedFilePath
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                    color: Qt.rgba(dialog.textColor.r, dialog.textColor.g, dialog.textColor.b, 0.6)
                }

                Text {
                    Layout.fillWidth: true
                    visible: dialog.uiState === "failed"
                    text: "Download failed"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: dialog.textColor
                }

                Text {
                    Layout.fillWidth: true
                    visible: dialog.uiState === "failed"
                    text: updateChecker.errorText.length > 0
                          ? updateChecker.errorText
                          : "Open the release page to download it manually."
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    color: Qt.rgba(dialog.textColor.r, dialog.textColor.g, dialog.textColor.b, 0.65)
                }

                Text {
                    visible: dialog.uiState === "failed"
                    text: "Open release page"
                    font.pixelSize: 12
                    font.underline: true
                    color: dialog.primaryColor

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: updateChecker.openReleasePage()
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
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 10

                Item {
                    Layout.fillWidth: true
                }

                ActionButton {
                    visible: dialog.uiState === "available"
                    label: "Download later"
                    onClicked: dialog.close()
                }

                ActionButton {
                    visible: dialog.uiState === "available"
                    label: "Download now"
                    accent: true
                    onClicked: updateChecker.startDownload()
                }

                ActionButton {
                    visible: dialog.uiState === "downloading"
                    label: "Cancel"
                    onClicked: updateChecker.cancelDownload()
                }

                ActionButton {
                    visible: dialog.uiState === "downloaded"
                    label: "Close"
                    onClicked: dialog.close()
                }

                ActionButton {
                    visible: dialog.uiState === "downloaded"
                    label: "Open"
                    accent: true
                    onClicked: {
                        updateChecker.openDownloadedFile()
                        dialog.close()
                    }
                }

                ActionButton {
                    visible: dialog.uiState === "failed"
                    label: "Close"
                    onClicked: dialog.close()
                }

                ActionButton {
                    visible: dialog.uiState === "failed"
                    label: "Retry"
                    accent: true
                    onClicked: updateChecker.startDownload()
                }
            }
        }
    }
}
