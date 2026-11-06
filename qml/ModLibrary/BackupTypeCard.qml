import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Rectangle {
    id: card
    property string title
    property string description
    property string iconName
    property color iconColor: window.primaryColor
    property string countLabel: ""
    signal clicked

    implicitHeight: 190
    radius: 18
    color: modManager.theme === "Crimson" ? "transparent" :
           (modManager.theme === "Glassmorphic" ?
           (mouseArea.containsMouse ? Qt.rgba(0.25, 0.25, 0.25, 0.95) : Qt.rgba(0.2, 0.2, 0.2, 0.95)) :
           (modManager.theme === "Wavey" ?
           (mouseArea.containsMouse ? Qt.rgba(0.1, 0.05, 0.2, 0.8) : Qt.rgba(0.05, 0.02, 0.1, 0.6)) :
           window.cardColor1))
    border.width: mouseArea.containsMouse ? 1.5 : 1
    border.color: mouseArea.containsMouse ? window.primaryColor :
        ((modManager.theme === "Glassmorphic" || modManager.theme === "Wavey") ? Qt.rgba(1, 1, 1, 0.18) : window.borderColor)

    gradient: modManager.theme === "Crimson" ? backupCardGradient : null

    Gradient {
        id: backupCardGradient
        GradientStop { position: 0.0; color: window.crimsonGradientTop }
        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
    }

    Behavior on color { ColorAnimation { duration: 200 } }
    Behavior on border.color { ColorAnimation { duration: 200 } }
    Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    Rectangle {
        anchors.fill: parent
        radius: card.radius
        color: modManager.theme === "Cozy" ? "#000000" : "#FFFFFF"
        opacity: mouseArea.containsMouse && modManager.theme !== "Cozy" ? 0.03 : 0.0
        Behavior on opacity { NumberAnimation { duration: 180 } }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: card.clicked()
        onPressed: card.scale = 0.97
        onReleased: card.scale = 1.0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 0

        Image {
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            Layout.bottomMargin: 14
            source: iconName !== "" ? "qrc:/UniversalModManager/Lucide Icons/" + iconName : ""
            sourceSize.width: 22
            sourceSize.height: 22
            fillMode: Image.PreserveAspectFit
            visible: iconName !== ""
            layer.enabled: true
            layer.smooth: true
            layer.textureSize: Qt.size(width * 2, height * 2)
            layer.effect: ColorOverlay {
                color: iconColor
            }
        }

        Text {
            text: title
            font.pixelSize: 17
            font.weight: Font.DemiBold
            font.family: "Segoe UI Variable Display"
            color: modManager.theme === "Cozy" ? window.textColor : "#FFFFFF"
        }

        Text {
            Layout.fillWidth: true
            Layout.topMargin: 4
            text: description
            font.pixelSize: 12
            color: modManager.theme === "Cozy" ? window.textColor : "#FFFFFF"
            opacity: modManager.theme === "Cozy" ? 0.68 : 0.7
            wrapMode: Text.WordWrap
            lineHeight: 1.35
        }

        Item {
            Layout.fillHeight: true
            Layout.minimumHeight: 10
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: window.separatorColor
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 10
            Layout.preferredHeight: 26
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: countLabel
                font.pixelSize: 11
                color: modManager.theme === "Cozy" ? window.textColor : "#FFFFFF"
                opacity: 0.56
                elide: Text.ElideRight
            }

            Rectangle {
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                radius: 13
                color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.08)
                border.width: 1
                border.color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.32)
                scale: mouseArea.containsMouse ? 1.06 : 1.0
                Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

                Image {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    source: "qrc:/UniversalModManager/Lucide Icons/chevron-right.svg"
                    sourceSize.width: 14
                    sourceSize.height: 14
                    fillMode: Image.PreserveAspectFit
                    layer.enabled: true
                    layer.smooth: true
                    layer.textureSize: Qt.size(width * 2, height * 2)
                    layer.effect: ColorOverlay {
                        color: window.accentColor
                    }
                }
            }
        }
    }
}
