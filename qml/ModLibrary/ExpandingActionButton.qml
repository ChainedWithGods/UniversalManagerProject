import QtQuick
import Qt5Compat.GraphicalEffects

Item {
    id: root

    property string label: ""
    property string iconSource: ""
    property color accentColor: "#FF4545"
    property color idleColor: "#141414"
    property color idleBorderColor: "transparent"
    property color idleContentColor: "#FFFFFF"

    signal clicked()

    readonly property bool expanded: hoverArea.containsMouse

    readonly property color contentColor: {
        if (!root.expanded)
            return root.idleContentColor
        var accent = root.accentColor
        var luminance = 0.299 * accent.r + 0.587 * accent.g + 0.114 * accent.b
        return luminance > 0.55 ? "#1A1A1A" : "#FFFFFF"
    }

    implicitWidth: 92
    implicitHeight: 32

    Rectangle {
        id: pill
        width: root.expanded ? 92 : 32
        height: 32
        x: (root.width - width) / 2
        y: 0
        radius: root.expanded ? 16 : 10
        color: root.expanded ? root.accentColor : root.idleColor
        border.width: 1
        border.color: root.expanded ? root.accentColor : root.idleBorderColor
        clip: true

        Behavior on width {
            NumberAnimation {
                duration: 300
                easing.type: Easing.Bezier
                easing.bezierCurve: [0.25, 0.1, 0.25, 1.0, 1.0, 1.0]
            }
        }
        Behavior on radius {
            NumberAnimation {
                duration: 300
                easing.type: Easing.Bezier
                easing.bezierCurve: [0.25, 0.1, 0.25, 1.0, 1.0, 1.0]
            }
        }
        Behavior on color {
            ColorAnimation {
                duration: 300
                easing.type: Easing.Bezier
                easing.bezierCurve: [0.25, 0.1, 0.25, 1.0, 1.0, 1.0]
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: 300
                easing.type: Easing.Bezier
                easing.bezierCurve: [0.25, 0.1, 0.25, 1.0, 1.0, 1.0]
            }
        }

        Item {
            id: iconItem
            width: 64
            height: 64
            x: (pill.width - width) / 2
            y: (pill.height - height) / 2 + (root.expanded ? 19 : 0)
            scale: root.expanded ? 0.5 : 0.22

            Behavior on scale {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.Bezier
                    easing.bezierCurve: [0.25, 0.1, 0.25, 1.0, 1.0, 1.0]
                }
            }
            Behavior on y {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.Bezier
                    easing.bezierCurve: [0.25, 0.1, 0.25, 1.0, 1.0, 1.0]
                }
            }

            Image {
                anchors.fill: parent
                source: root.iconSource
                sourceSize: Qt.size(64, 64)
                layer.enabled: true
                layer.effect: ColorOverlay {
                    color: root.contentColor
                    Behavior on color { ColorAnimation { duration: 180 } }
                }
            }
        }

        Text {
            text: root.label
            font.pixelSize: 11
            font.weight: Font.Bold
            color: root.contentColor
            x: (pill.width - width) / 2
            y: root.expanded ? 5 : -14
            scale: root.expanded ? 1 : 0.15
            opacity: root.expanded ? 1 : 0
            transformOrigin: Item.Center

            Behavior on color { ColorAnimation { duration: 180 } }

            Behavior on y {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.Bezier
                    easing.bezierCurve: [0.25, 0.1, 0.25, 1.0, 1.0, 1.0]
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.Bezier
                    easing.bezierCurve: [0.25, 0.1, 0.25, 1.0, 1.0, 1.0]
                }
            }
            Behavior on opacity {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.Bezier
                    easing.bezierCurve: [0.25, 0.1, 0.25, 1.0, 1.0, 1.0]
                }
            }
        }

        MouseArea {
            id: hoverArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clicked()
        }
    }
}
