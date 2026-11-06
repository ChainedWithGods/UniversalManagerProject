import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Rectangle {
    id: root
    property bool compact: false
    property string label: "Update Available"
    property string tooltipText: ""
    signal clicked()

    readonly property color pillBorderColor: Qt.rgba(window.primaryColor.r,
                                                      window.primaryColor.g,
                                                      window.primaryColor.b, 0.65)

    implicitWidth: badgeLabel.implicitWidth + (compact ? 16 : 22)
    implicitHeight: compact ? 18 : 24
    radius: compact ? 4 : 5
    color: "transparent"

    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop {
            position: 0.0
            color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.22)
        }
        GradientStop {
            position: 0.55
            color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.5)
        }
        GradientStop {
            position: 1.0
            color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.22)
        }
    }

    Item {
        id: waveMask
        anchors.fill: parent
        layer.enabled: root.visible
        visible: false

        Rectangle {
            anchors.fill: parent
            radius: root.radius
            color: "white"
        }
    }

    Item {
        id: waveLayer
        anchors.fill: parent

        layer.enabled: root.visible
        layer.live: waveAnim.running
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: waveMask
        }

        Rectangle {
            id: wave
            width: Math.max(24, root.width * 0.5)
            height: root.height
            radius: root.radius

            gradient: Gradient {
                orientation: Gradient.Horizontal

                GradientStop {
                    position: 0.0
                    color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.0)
                }
                GradientStop {
                    position: 0.5
                    color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.45)
                }
                GradientStop {
                    position: 1.0
                    color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.0)
                }
            }

            x: -width

            NumberAnimation on x {
                id: waveAnim
                running: root.visible && hoverArea.containsMouse
                         && !window.focusAnimationsPaused
                from: -wave.width
                to: root.width
                duration: 2000
                loops: Animation.Infinite
                easing.type: Easing.InOutSine
                onRunningChanged: if (!running) wave.x = -wave.width
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: 1
        border.color: root.pillBorderColor
    }

    Text {
        id: badgeLabel
        anchors.centerIn: parent
        text: root.label
        font.pixelSize: root.compact ? 9 : 10
        font.weight: Font.Bold
        color: modManager.theme === "Cozy" ? "#3E3E3E" : "#FFFFFF"
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTip {
        id: tip
        visible: hoverArea.containsMouse && root.tooltipText.length > 0
        text: root.tooltipText
        delay: 400
        padding: 8

        contentItem: Text {
            text: tip.text
            color: window.textColor
            font.pixelSize: 11
        }

        background: Rectangle {
            radius: 7
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor
            border.width: 1
            border.color: window.borderColor
        }
    }
}
