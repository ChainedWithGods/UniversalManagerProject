import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Item {
    id: badge

    property bool paused: false

    implicitWidth: 90
    implicitHeight: 28

    readonly property color accent: themeConfig.cozy.accentColor
    readonly property color wordColor: Qt.rgba(
        accent.r * 0.80 + 0.231 * 0.20,
        accent.g * 0.80 + 0.165 * 0.20,
        accent.b * 0.80 + 0.102 * 0.20,
        1.0)

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        border.width: 1.5
        border.color: Qt.rgba(badge.accent.r, badge.accent.g, badge.accent.b, 0.8)

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#FFFDF9" }
            GradientStop { position: 0.7; color: "#FBF0DF" }
            GradientStop { position: 1.0; color: "#F9E8D0" }
        }
    }

    RowLayout {
        anchors.centerIn: parent
        spacing: 6

        Item {
            id: heart
            Layout.preferredWidth: 13
            Layout.preferredHeight: 13
            Layout.alignment: Qt.AlignVCenter

            Image {
                id: heartImage
                anchors.fill: parent
                source: "qrc:/UniversalModManager/Lucide Icons/Heart-Cozy.SVG"
                sourceSize.width: 26
                sourceSize.height: 26
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: false
            }

            ColorOverlay {
                anchors.fill: heartImage
                source: heartImage
                color: badge.accent
                smooth: true
            }

            SequentialAnimation on scale {
                loops: Animation.Infinite
                running: badge.visible && !badge.paused
                NumberAnimation { to: 1.24; duration: 208; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: 208; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.15; duration: 234; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: 208; easing.type: Easing.InOutSine }
                PauseAnimation { duration: 1742 }
            }
        }

        Text {
            text: "ACTIVE"
            font.pixelSize: 10
            font.weight: Font.ExtraBold
            font.letterSpacing: 1.5
            color: badge.wordColor
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
