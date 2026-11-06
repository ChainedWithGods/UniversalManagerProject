import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Rectangle {
    id: fileRow

    required property string fileName
    required property string fileSize
    required property string fileExtension
    property string relativePath: ""
    property bool showRelativePath: false

    readonly property string displayName: {
        var raw = showRelativePath && relativePath !== "" ? relativePath : fileName
        var lower = raw.toLowerCase()
        var suffixes = [".pak", ".ucas", ".utoc"]
        for (var i = 0; i < suffixes.length; ++i) {
            if (lower.endsWith(suffixes[i])) {
                raw = raw.slice(0, raw.length - suffixes[i].length)
                if (raw.toLowerCase().endsWith("_p"))
                    raw = raw.slice(0, raw.length - 2)
                break
            }
        }
        return raw
    }
    readonly property string extUpper: (fileExtension || "").toUpperCase()
    readonly property color extColor: modManager.theme === "Glassmorphic"
        ? modManager.accentColor
        : (modManager.theme === "Wavey" ? "#60a5fa" : window.primaryColor)

    width: parent ? parent.width : 0
    implicitHeight: 46
    radius: 9
    color: modManager.theme === "Cozy" ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.035)
    border.width: 1
    border.color: modManager.theme === "Cozy" ? "#ECE4DA" : Qt.rgba(1, 1, 1, 0.055)

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 10

        Image {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            source: "qrc:/UniversalModManager/Lucide Icons/file-box.svg"
            sourceSize: Qt.size(16, 16)
            layer.enabled: true
            layer.effect: ColorOverlay {
                color: fileRow.extColor
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 1

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: fileRow.displayName
                font.family: "JetBrains Mono"
                font.pixelSize: 12
                font.weight: Font.Medium
                color: modManager.theme === "Cozy" ? "#2B2B2B" : "#F1F5F9"
                elide: Text.ElideMiddle
            }

            Text {
                visible: fileRow.fileSize !== ""
                text: fileRow.fileSize
                font.pixelSize: 11
                color: modManager.theme === "Cozy" ? "#6F6F6F" : "#94A3B8"
            }
        }

        Rectangle {
            Layout.preferredWidth: 50
            Layout.minimumWidth: 50
            Layout.preferredHeight: 22
            radius: 5
            color: Qt.rgba(fileRow.extColor.r, fileRow.extColor.g, fileRow.extColor.b, 0.14)
            border.width: 1.5
            border.color: Qt.rgba(fileRow.extColor.r, fileRow.extColor.g, fileRow.extColor.b, 0.4)

            Text {
                anchors.centerIn: parent
                text: fileRow.extUpper.length > 0 ? fileRow.extUpper : "FILE"
                font.family: "JetBrains Mono"
                font.pixelSize: 9
                font.weight: Font.Bold
                color: fileRow.extColor
            }
        }
    }
}
