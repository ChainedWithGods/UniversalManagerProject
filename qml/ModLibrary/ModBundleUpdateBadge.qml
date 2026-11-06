import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects

Rectangle {
    id: root

    property string versionLabel: ""
    property string tooltipText: ""
    signal clicked()

    readonly property color accent: modManager.theme === "Cozy" ? window.primaryColor : window.accentColor
    readonly property bool hovered: hoverArea.containsMouse

    implicitWidth: Math.max(frontLabel.implicitWidth, backRow.implicitWidth) + 26
    implicitHeight: 36
    radius: 18
    color: "transparent"

    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop {
            position: 0.0
            color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.08)
        }
        GradientStop {
            position: 0.55
            color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.22)
        }
        GradientStop {
            position: 1.0
            color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.08)
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.16)
        opacity: root.hovered ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 140 } }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b,
                              root.hovered ? 0.8 : 0.5)

        Behavior on border.color { ColorAnimation { duration: 140 } }
    }

    Item {
        id: contentViewport
        anchors.fill: parent
        clip: true

        Item {
            id: frontFace
            width: contentViewport.width
            height: contentViewport.height
            y: root.hovered ? -root.height : 0

            Behavior on y {
                NumberAnimation { duration: 230; easing.type: Easing.OutCubic }
            }

            Text {
                id: frontLabel
                anchors.centerIn: parent
                text: root.versionLabel
                font.pixelSize: 13
                font.weight: Font.Bold
                color: root.accent
            }
        }

        Item {
            id: backFace
            width: contentViewport.width
            height: contentViewport.height
            y: root.hovered ? 0 : root.height

            Behavior on y {
                NumberAnimation { duration: 230; easing.type: Easing.OutCubic }
            }

            Row {
                id: backRow
                anchors.centerIn: parent
                spacing: 6

                Image {
                    width: 15
                    height: 15
                    anchors.verticalCenter: parent.verticalCenter
                    source: "qrc:/UniversalModManager/Lucide Icons/download.svg"
                    sourceSize: Qt.size(30, 30)
                    layer.enabled: true
                    layer.effect: ColorOverlay { color: root.accent }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Download"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    color: root.accent
                }
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
