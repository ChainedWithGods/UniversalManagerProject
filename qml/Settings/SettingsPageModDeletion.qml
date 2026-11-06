import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

ColumnLayout {
    Layout.fillWidth: true
    Layout.rightMargin: 10
    spacing: 15
    visible: generalSectionIndex === 3

    Text {
        text: "Mod Deletion Behavior"
        font.pixelSize: 20
        font.weight: 600
        color: window.textColor
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 210
        color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
        radius: window.cardRadius
        border.width: window.borderWidth
        border.color: window.borderColor

        gradient: modManager.theme === "Crimson" ? crimsonDeletionCardGradient : null

        Gradient {
            id: crimsonDeletionCardGradient
            GradientStop { position: 0.0; color: window.crimsonGradientTop }
            GradientStop { position: 1.0; color: window.crimsonGradientBottom }
        }

        layer.enabled: modManager.theme !== "Glassmorphic"
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.3)
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 4
            shadowBlur: 0.4
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 25
            spacing: 20

            RowLayout {
                Layout.fillWidth: true
                spacing: 15

                Rectangle {
                    width: modManager.theme === "Wavey" ? 60 : 52
                    height: modManager.theme === "Wavey" ? 60 : 52
                    color: "transparent"
                    radius: 14
                    border.color: "transparent"
                    border.width: 0

                    Image {
                        id: deleteModeIcon
                        anchors.centerIn: parent
                        width: modManager.theme === "Wavey" ? 40 : 32
                        height: modManager.theme === "Wavey" ? 40 : 32
                        source: modManager.theme === "Glassmorphic"
                                ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/delete mode.png"
                                : (modManager.theme === "Crimson"
                                   ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Delete mode.png"
                                   : (modManager.theme === "Wavey"
                                      ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Delete Mode.png"
                                      : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Delete Mode.png"))
                        sourceSize: Qt.size(modManager.theme === "Wavey" ? 80 : 32, modManager.theme === "Wavey" ? 80 : 32)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        antialiasing: true
                    }

                    ColorOverlay {
                        anchors.fill: deleteModeIcon
                        source: deleteModeIcon
                        color: window.primaryColor
                        visible: modManager.theme === "Glassmorphic"
                    }
                }

                Column {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: "Delete Mode"
                        font.pixelSize: 18
                        font.weight: 600
                        color: window.textColor
                    }

                    Text {
                        text: "Choose how mods should be deleted from your library."
                        font.pixelSize: 13
                        color: window.textSecondaryColor
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 15

                    DeleteOption {
                        text: "Recycle Bin"
                        description: "Safe & recoverable"
                        icon: modManager.theme === "Glassmorphic"
                           ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Recycle bin.png"
                           : (modManager.theme === "Crimson"
                              ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Recycel-bin.png"
                              : (modManager.theme === "Wavey"
                                 ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Recycle bin.png"
                                 : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Recycle bin.png"))
                        isSelected: modManager.deleteToRecycleBin === true
                        onClicked: modManager.setDeleteToRecycleBin(true)
                    }

                    DeleteOption {
                        text: "Permanent"
                        description: "Delete forever"
                        icon: modManager.theme === "Glassmorphic"
                           ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Delete forever.png"
                           : (modManager.theme === "Crimson"
                              ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Delete forever.png"
                              : (modManager.theme === "Wavey"
                                 ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Delete forever.png"
                                 : "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Delete forever.png"))
                        isSelected: modManager.deleteToRecycleBin === false
                        onClicked: modManager.setDeleteToRecycleBin(false)
                    }
            }
        }
    }

    component DeleteOption: Rectangle {
        id: deleteOptionRoot
        property string text: "Option"
        property string description: "Description"
        property string icon: ""
        property bool iconIsImage: icon.startsWith("file://") || icon.startsWith("qrc:/")
        property bool isSelected: false
        signal clicked()

        Layout.fillWidth: true
        Layout.preferredHeight: 70
        color: modManager.theme === "Glassmorphic"
            ? (isSelected
                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.25)
                : (mouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.15)))
            : (modManager.theme === "Crimson"
                ? (isSelected ? Qt.rgba(79, 70, 229, 0.2) : (mouseArea.containsMouse ? Qt.rgba(79, 70, 229, 0.1) : window.surfaceColor))
                : (modManager.theme === "Wavey"
                    ? (isSelected ? Qt.rgba(0.23, 0.45, 0.71, 0.4) : (mouseArea.containsMouse ? Qt.rgba(0.23, 0.45, 0.71, 0.25) : window.surfaceColor))
                    : (isSelected ? Qt.lighter(window.primaryColor, 1.85) : (mouseArea.containsMouse ? window.cardColor1 : window.surfaceColor))))
        radius: window.cardRadius
        border.width: modManager.theme === "Glassmorphic" ? 2 : (isSelected ? 3 : 2)
        border.color: modManager.theme === "Glassmorphic"
            ? (isSelected
                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.8)
                : Qt.rgba(1, 1, 1, 0.3))
            : (modManager.theme === "Crimson"
                ? (isSelected ? window.accentColor : window.borderColor)
                : (modManager.theme === "Wavey"
                    ? (isSelected ? Qt.rgba(0.23, 0.45, 0.71, 0.8) : window.borderColor)
                    : (isSelected ? window.primaryColor : window.borderColor)))

        Behavior on color { ColorAnimation { duration: 200 } }
        Behavior on border.color { ColorAnimation { duration: 200 } }
        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
            onPressed: parent.scale = 0.97
            onReleased: parent.scale = 1.0
            onExited: parent.scale = 1.0
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 15
            spacing: 12
            layoutDirection: Qt.LeftToRight

            Rectangle {
                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                width: modManager.theme === "Wavey" ? 48 : 40
                height: modManager.theme === "Wavey" ? 48 : 40
                color: modManager.theme === "Wavey" ? "transparent" : "transparent"
                radius: modManager.theme === "Wavey" ? 12 : 0
                border.color: modManager.theme === "Wavey" ? "transparent" : "transparent"
                border.width: modManager.theme === "Wavey" ? 0 : 0

                Image {
                    id: optionIcon
                    anchors.centerIn: parent
                    width: modManager.theme === "Wavey" ? 36 : 32
                    height: modManager.theme === "Wavey" ? 36 : 32
                    source: iconIsImage ? icon : ""
                    sourceSize: Qt.size(modManager.theme === "Wavey" ? 72 : 32, modManager.theme === "Wavey" ? 72 : 32)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    antialiasing: true
                    visible: iconIsImage
                }

                ColorOverlay {
                    anchors.fill: optionIcon
                    source: optionIcon
                    color: window.primaryColor
                    visible: modManager.theme === "Glassmorphic" && optionIcon.visible
                }

                Text {
                    anchors.centerIn: parent
                    text: icon
                    font.pixelSize: modManager.theme === "Wavey" ? 32 : 28
                    visible: !iconIsImage
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: deleteOptionRoot.text + "<br>" + deleteOptionRoot.description
                font.pixelSize: 15
                font.weight: 600
                color: window.textColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                lineHeight: 1.2
                lineHeightMode: Text.ProportionalHeight
            }
        }
    }
}
