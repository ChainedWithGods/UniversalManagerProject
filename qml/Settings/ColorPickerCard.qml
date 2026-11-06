import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: colorCard

    required property string colorLabel
    required property color colorValue
    required property string targetProperty

    width: colorRow.implicitWidth + 24
    height: 46
    radius: 23

    color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.06) : window.cardColor3
    border.width: 1
    border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.12) : Qt.alpha(window.borderColor, 0.45)

    layer.enabled: true
    layer.effect: MultiEffect {
        id: cardShadow
        shadowEnabled: true
        shadowColor: Qt.rgba(0, 0, 0, 0.16)
        shadowVerticalOffset: 1
        shadowBlur: 6
        shadowOpacity: 0.16
    }

    MouseArea {
        id: cardMouseArea
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true
        onClicked: {
            colorPickerDialog.targetProperty = colorCard.targetProperty
            colorPickerDialog.currentColor = colorCard.colorValue
            colorPickerDialog.open()
        }
    }

    states: [
        State {
            name: "hovered"
            when: cardMouseArea.containsMouse
            PropertyChanges {
                target: colorCard
                scale: 1.03
                border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.25) : window.primaryColor
            }
        }
    ]

    transitions: [
        Transition {
            to: "hovered"
            ParallelAnimation {
                NumberAnimation { target: colorCard; property: "scale"; duration: 150; easing.type: Easing.OutCubic }
                ColorAnimation { target: colorCard; property: "border.color"; duration: 150 }
            }
        },
        Transition {
            from: "hovered"
            ParallelAnimation {
                NumberAnimation { target: colorCard; property: "scale"; duration: 150; easing.type: Easing.OutCubic }
                ColorAnimation { target: colorCard; property: "border.color"; duration: 150 }
            }
        }
    ]

    RowLayout {
        id: colorRow
        anchors.centerIn: parent
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30
            radius: 15

            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.lighter(colorCard.colorValue, 1.15) }
                GradientStop { position: 0.5; color: colorCard.colorValue }
                GradientStop { position: 1.0; color: Qt.darker(colorCard.colorValue, 1.1) }
            }

            border.width: 2
            border.color: Qt.rgba(0, 0, 0, 0.12)

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: colorCard.colorValue
                shadowBlur: 0.4
                shadowOpacity: 0.3
            }
        }

        Text {
            text: colorCard.colorLabel
            font.pixelSize: 12
            font.weight: Font.DemiBold
            color: window.textColor
            Layout.rightMargin: 4
        }
    }

}
