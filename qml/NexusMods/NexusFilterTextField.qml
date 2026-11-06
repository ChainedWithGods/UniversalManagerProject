import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

TextField {
    id: filterField
    Layout.fillWidth: true
    Layout.preferredHeight: 34
    leftPadding: 10
    rightPadding: 10
    font.pixelSize: 12
    color: window.textColor
    placeholderTextColor: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.4)
    selectByMouse: true

    ContextMenu.menu: null

    background: Rectangle {
        radius: 7
        color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, filterField.activeFocus ? 0.07 : 0.04)
        border.width: 1
        border.color: filterField.activeFocus ? window.primaryColor : window.borderColor
        Behavior on border.color { ColorAnimation { duration: 150 } }
    }
}
