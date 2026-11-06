import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

ColumnLayout {
    id: control
    property string label: ""
    property real value: 0
    property real from: 0
    property real to: 1
    property real stepSize: 0.05
    signal moved(real value)

    Layout.fillWidth: true
    spacing: 12

    onValueChanged: slider.value = value

    RowLayout {
        Layout.fillWidth: true

        Text {
            text: control.label
            font.pixelSize: 15
            font.weight: 600
            color: window.textColor
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.preferredWidth: 50
            Layout.preferredHeight: 28
            color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.18)
                : Qt.rgba(1, 1, 1, 0.15)
            radius: 14
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.35)
                : Qt.rgba(1, 1, 1, 0.3)

            Text {
                anchors.centerIn: parent
                text: Math.round(control.value * 100) + "%"
                font.pixelSize: 12
                font.weight: Font.Bold
                color: modManager.theme === "Glassmorphic" ? window.accentColor : window.primaryColor
            }
        }
    }

    Slider {
        id: slider
        Layout.fillWidth: true
        from: control.from
        to: control.to
        value: control.value
        stepSize: control.stepSize
        onMoved: control.moved(slider.value)
        onPressedChanged: {
            if (!pressed)
                control.moved(slider.value)
        }

        background: Rectangle {
            x: parent.leftPadding
            y: parent.topPadding + parent.availableHeight / 2 - height / 2
            implicitWidth: 200
            implicitHeight: 8
            width: parent.availableWidth
            height: implicitHeight
            radius: 4
            color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.12)
                : Qt.rgba(1, 1, 1, 0.15)
            border.width: 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.22)
                : Qt.rgba(1, 1, 1, 0.2)

            Rectangle {
                width: parent.parent.visualPosition * parent.width
                height: parent.height
                radius: 4
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0.0
                        color: modManager.theme === "Glassmorphic" ? window.accentColor : window.primaryColor
                    }
                    GradientStop {
                        position: 1.0
                        color: modManager.theme === "Glassmorphic"
                            ? Qt.lighter(window.accentColor, 1.18)
                            : Qt.lighter(window.primaryColor, 1.3)
                    }
                }
            }
        }

        handle: Rectangle {
            x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
            y: parent.topPadding + parent.availableHeight / 2 - height / 2
            implicitWidth: 24
            implicitHeight: 24
            radius: 12
            color: modManager.theme === "Glassmorphic" ? window.accentColor : window.primaryColor
            border.color: "#FFFFFF"
            border.width: 3

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowVerticalOffset: 2
                shadowBlur: 0.4
            }

            Behavior on scale { NumberAnimation { duration: 100 } }
            scale: parent.pressed ? 0.9 : 1.0
        }
    }
}
