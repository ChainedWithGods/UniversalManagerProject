import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

MenuItem {
    id: menuItem
    property string iconName: ""
    property string shortcutText: ""
    property bool isDanger: false

    height: visible ? implicitHeight : 0
    implicitHeight: 38
    padding: 6
    leftPadding: 12
    rightPadding: 12

    contentItem: RowLayout {
        spacing: 12
        opacity: menuItem.enabled ? 1.0 : 0.45

        Image {
            source: menuItem.iconName !== "" ? "qrc:/UniversalModManager/Lucide Icons/" + menuItem.iconName : ""
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            sourceSize.width: 32
            sourceSize.height: 32
            fillMode: Image.PreserveAspectFit
            visible: menuItem.iconName !== ""

            layer.enabled: true
            layer.effect: ColorOverlay {
                color: menuItem.isDanger ? "#EF4444" :
                       (modManager.theme === "Cozy" ? "#2C2C2C" :
                       ((modManager.theme === "Glassmorphic" || modManager.theme === "Wavey") ? "#FFFFFF" : window.textColor))
            }
        }

        Text {
            text: menuItem.text
            font.pixelSize: 14
            font.weight: Font.Medium
            color: menuItem.isDanger ? "#EF4444" :
                   (modManager.theme === "Cozy" ? "#2C2C2C" :
                   ((modManager.theme === "Glassmorphic" || modManager.theme === "Wavey") ? "#FFFFFF" : window.textColor))
            Layout.fillWidth: true
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            text: menuItem.shortcutText
            font.pixelSize: 12
            color: modManager.theme === "Cozy" ? "#6B6B6B" :
                   ((modManager.theme === "Glassmorphic" || modManager.theme === "Wavey") ? Qt.rgba(1, 1, 1, 0.5) : window.textSecondaryColor)
            visible: menuItem.shortcutText !== ""
            Layout.alignment: Qt.AlignRight
        }
    }

    background: Rectangle {
        radius: 8
        color: menuItem.highlighted && menuItem.enabled
            ? (menuItem.isDanger
                ? Qt.rgba(0.93, 0.26, 0.26, 0.1)
                : (modManager.theme === "Cozy"
                    ? Qt.rgba(0, 0, 0, 0.05)
                    : ((modManager.theme === "Glassmorphic" || modManager.theme === "Wavey")
                        ? Qt.rgba(1, 1, 1, 0.15)
                        : Qt.rgba(0, 0, 0, 0.05))))
            : "transparent"

        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
