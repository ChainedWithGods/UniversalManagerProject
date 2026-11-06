import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

MenuItem {
    id: menuItem
    property string iconName: ""
    property bool isDanger: false
    property string shortcutText: ""

    implicitWidth: 180
    implicitHeight: 32
    width: parent ? parent.width : implicitWidth
    height: 32

    contentItem: RowLayout {
        spacing: 12

        Image {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            source: menuItem.iconName !== "" ? "qrc:/UniversalModManager/Lucide Icons/" + menuItem.iconName : ""
            sourceSize: Qt.size(16, 16)
            visible: menuItem.iconName !== ""
            opacity: menuItem.enabled ? 1.0 : 0.5

            layer.enabled: true
            layer.effect: ColorOverlay {
                color: menuItem.isDanger ? "#EF4444" : ((modManager.theme === "Glassmorphic" || modManager.theme === "Wavey") ? "#FFFFFF" : window.textColor)
            }
        }

        Text {
            Layout.fillWidth: true
            text: menuItem.text
            font.pixelSize: 13
            font.weight: Font.Medium
            color: menuItem.isDanger ? "#EF4444" : ((modManager.theme === "Glassmorphic" || modManager.theme === "Wavey") ? "#FFFFFF" : window.textColor)
            opacity: menuItem.enabled ? 1.0 : 0.5
        }

        Text {
            text: menuItem.shortcutText
            font.pixelSize: 11
            color: (modManager.theme === "Glassmorphic" || modManager.theme === "Wavey") ? Qt.rgba(1, 1, 1, 0.5) : window.textSecondaryColor
            visible: menuItem.shortcutText !== ""
            opacity: menuItem.enabled ? 1.0 : 0.5
        }
    }

    background: Rectangle {
        color: menuItem.highlighted
            ? (modManager.theme === "Glassmorphic"
                ? Qt.rgba(1, 1, 1, 0.10)
                : (modManager.theme === "Crimson"
                    ? Qt.rgba(0.65, 0.40, 0.95, 0.20)
                    : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)))
            : "transparent"
        radius: 8

        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
