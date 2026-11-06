import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Item {
    id: legacyDialogs
    anchors.fill: parent

    component ModernButton: Rectangle {
        property string text: "Button"
        property string iconText: ""
        property bool isPrimary: false
        signal clicked()

        implicitWidth: buttonLayout.implicitWidth + 32
        implicitHeight: 46
        radius: 23
        color: {
            if (modManager.theme === "Glassmorphic") {
                if (isPrimary) {
                    return btnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.35) : Qt.rgba(1, 1, 1, 0.3)
                } else {
                    return btnMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(1, 1, 1, 0.2)
                }
            } else {
                if (isPrimary) {
                    return btnMouse.containsMouse ? Qt.lighter(window.primaryColor, 1.15) : window.primaryColor
                } else {
                    return btnMouse.containsMouse ? window.borderColor : "transparent"
                }
            }
        }
        border.width: modManager.theme === "Glassmorphic" ? 1 : (isPrimary ? 0 : 2)
        border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.45) : window.primaryColor

        layer.enabled: btnMouse.containsMouse
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowOpacity: 0.2
            shadowBlur: 0.3
            shadowVerticalOffset: 4
        }

        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on scale { NumberAnimation { duration: 100 } }

        RowLayout {
            id: buttonLayout
            anchors.centerIn: parent
            spacing: 8

            Text {
                text: iconText
                font.pixelSize: 16
                visible: iconText !== ""
            }

            Text {
                text: parent.parent.text
                font.pixelSize: 14
                font.weight: modManager.theme === "Glassmorphic" ? 600 : 500
                color: {
                    if (modManager.theme === "Glassmorphic") {
                        return "#FFFFFF"
                    } else {
                        return isPrimary ? "#FFFFFF" : window.primaryColor
                    }
                }
            }
        }

        MouseArea {
            id: btnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
            onPressed: parent.scale = 0.95
            onReleased: parent.scale = 1.0
            onExited: parent.scale = 1.0
        }
    }

    Popup {
        id: themePopup
        width: 550
        height: 600
        modal: true
        focus: true
        anchors.centerIn: parent
        dim: true
        padding: 0

        background: Rectangle {
            radius: window.cardRadius
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.15)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            border.width: 1
            gradient: modManager.theme === "Cozy" ? settingsDialogCozyGradient
                    : modManager.theme === "Crimson" ? settingsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? settingsDialogWaveyGradient
                    : null

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: window.shadowEnabled
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: "#20000000"
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 8
                shadowBlur: 0.5
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 70
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 25
                    spacing: 4

                    Text {
                        text: "Select Theme"
                        font.pixelSize: 22
                        font.weight: 600
                        color: window.textColor
                    }

                    Text {
                        text: "Choose your preferred visual style."
                        font.pixelSize: 13
                        color: window.textSecondaryColor
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"

                ScrollView {
                    id: themeScroll
                    anchors.fill: parent
                    anchors.margins: 25
                    anchors.topMargin: 10
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    Column {
                        id: themeList
                        width: parent.width
                        spacing: 12

                        ThemeCard { width: parent.width; height: 130; themeName: "Cozy"; themeDescription: "Warm & comfortable"; previewBg: "#F5F2ED"; previewPrimary: "#8B7355"; previewAccent: "#D4A574"; previewCard1: "#E8F5E9"; previewCard2: "#FFF3E0"; isSelected: modManager.theme === "Cozy"; onClicked: modManager.setTheme("Cozy") }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 75
                color: "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 25
                    spacing: 12

                    Item { Layout.fillWidth: true }

                    ModernButton {
                        text: "Cancel"
                        isPrimary: false
                        onClicked: themePopup.close()
                    }

                    ModernButton {
                        text: "Apply"
                        isPrimary: true
                        onClicked: themePopup.close()
                    }
                }
            }
        }
    }

    component ThemeCard: Rectangle {
        id: card
        property string themeName: "Theme"
        property string themeDescription: "Description"
        property color previewBg: "#FFFFFF"
        property color previewPrimary: "#000000"
        property color previewAccent: "#666666"
        property color previewCard1: "#F0F0F0"
        property color previewCard2: "#E0E0E0"
        property bool isSelected: false
        signal clicked()

        implicitWidth: 240
        implicitHeight: 130

        color: window.surfaceColor
        radius: 12
        border.width: isSelected ? 3 : 2
        border.color: isSelected ? previewPrimary : window.borderColor

        Behavior on color { ColorAnimation { duration: 200 } }
        Behavior on border.color { ColorAnimation { duration: 200 } }
        Behavior on scale { NumberAnimation { duration: 150 } }

        MouseArea {
            id: cardMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
            onPressed: parent.scale = 0.97
            onReleased: parent.scale = 1.0
            onExited: parent.scale = 1.0
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 15
            spacing: 12

            ColumnLayout {
                spacing: 3

                RowLayout {
                    spacing: 8

                    Text {
                        text: themeName
                        font.pixelSize: 16
                        font.weight: 600
                        color: previewPrimary
                    }

                    Rectangle {
                        width: 18
                        height: 18
                        radius: 9
                        color: previewPrimary
                        opacity: isSelected ? 1.0 : 0.0
                        visible: isSelected

                        Text {
                            anchors.centerIn: parent
                            text: ""
                            font.pixelSize: 12
                            font.weight: 700
                            color: "#FFFFFF"
                        }
                    }
                }

                Text {
                    text: themeDescription
                    font.pixelSize: 12
                    color: window.textSecondaryColor
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: previewBg
                radius: 8
                clip: true

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 35
                        color: previewCard1
                        radius: 6
                    }

                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 35
                        color: previewCard2
                        radius: 6
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: previewAccent
                        radius: 6
                        opacity: 0.8
                    }
                }
            }
        }
    }

    Popup {
        id: customColorDialog
        width: 440
        height: 580
        modal: true
        focus: true
        anchors.centerIn: parent
        padding: 0
        dim: true

        property real hue: 180
        property real saturation: 0.7
        property real value: 0.8
        property color selectedColor: Qt.hsva(hue / 360, saturation, value, 1.0)
        property bool liveUpdate: false
        property color originalColor: "#5EC4D9"

        onHueChanged: if (liveUpdate) applyColorLive()
        onSaturationChanged: if (liveUpdate) applyColorLive()
        onValueChanged: if (liveUpdate) applyColorLive()

        function applyColorLive() {
            window.primaryColor = selectedColor
            window.accentColor = Qt.lighter(selectedColor, 1.3)
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 200; easing.type: Easing.OutQuad }
            NumberAnimation { property: "scale"; from: 0.9; to: 1.0; duration: 200; easing.type: Easing.OutBack }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 150; easing.type: Easing.InQuad }
            NumberAnimation { property: "scale"; from: 1.0; to: 0.95; duration: 150; easing.type: Easing.InQuad }
        }

        onOpened: {

            originalColor = window.primaryColor

            liveUpdate = false
            var currentColor = window.primaryColor
            hue = currentColor.hsvHue * 360
            saturation = currentColor.hsvSaturation
            value = currentColor.hsvValue
            liveUpdate = true
        }

        background: Rectangle {
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 1
            border.color: modManager.theme === "Glassmorphic"
                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.4)
                : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.10) : window.borderColor)
            gradient: modManager.theme === "Cozy" ? settingsDialogCozyGradient
                    : modManager.theme === "Crimson" ? settingsDialogCrimsonGradient
                    : modManager.theme === "Wavey" ? settingsDialogWaveyGradient
                    : null

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 12
                shadowBlur: 0.6
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 32
            spacing: 24

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    text: ""
                    font.pixelSize: 28
                }

                Text {
                    text: "Custom Color Picker"
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: window.textColor
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 24
                    color: customColorDialog.selectedColor
                    border.width: 3
                    border.color: "#FFFFFF"

                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: Qt.rgba(0, 0, 0, 0.3)
                        shadowVerticalOffset: 2
                        shadowBlur: 0.4
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: ""
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: window.textColor
                        Layout.fillWidth: true
                    }

                    Text {
                        text: Math.round(customColorDialog.hue) + "°"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: window.textSecondaryColor
                    }
                }

                Slider {
                    Layout.fillWidth: true
                    from: 0
                    to: 360
                    value: customColorDialog.hue
                    onMoved: customColorDialog.hue = value

                    background: Rectangle {
                        x: parent.leftPadding
                        y: parent.topPadding + parent.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 8
                        width: parent.availableWidth
                        height: implicitHeight
                        radius: 4

                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#FF0000" }
                            GradientStop { position: 0.166; color: "#FFFF00" }
                            GradientStop { position: 0.333; color: "#00FF00" }
                            GradientStop { position: 0.5; color: "#00FFFF" }
                            GradientStop { position: 0.666; color: "#0000FF" }
                            GradientStop { position: 0.833; color: "#FF00FF" }
                            GradientStop { position: 1.0; color: "#FF0000" }
                        }
                    }

                    handle: Rectangle {
                        x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                        y: parent.topPadding + parent.availableHeight / 2 - height / 2
                        implicitWidth: 24
                        implicitHeight: 24
                        radius: 12
                        color: Qt.hsva(customColorDialog.hue / 360, 1.0, 1.0, 1.0)
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

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: ""
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: window.textColor
                        Layout.fillWidth: true
                    }

                    Text {
                        text: Math.round(customColorDialog.saturation * 100) + "%"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: window.textSecondaryColor
                    }
                }

                Slider {
                    Layout.fillWidth: true
                    from: 0.0
                    to: 1.0
                    value: customColorDialog.saturation
                    onMoved: customColorDialog.saturation = value

                    background: Rectangle {
                        x: parent.leftPadding
                        y: parent.topPadding + parent.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 8
                        width: parent.availableWidth
                        height: implicitHeight
                        radius: 4

                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#FFFFFF" }
                            GradientStop { position: 1.0; color: Qt.hsva(customColorDialog.hue / 360, 1.0, customColorDialog.value, 1.0) }
                        }
                    }

                    handle: Rectangle {
                        x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                        y: parent.topPadding + parent.availableHeight / 2 - height / 2
                        implicitWidth: 24
                        implicitHeight: 24
                        radius: 12
                        color: Qt.hsva(customColorDialog.hue / 360, customColorDialog.saturation, customColorDialog.value, 1.0)
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

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: ""
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: window.textColor
                        Layout.fillWidth: true
                    }

                    Text {
                        text: Math.round(customColorDialog.value * 100) + "%"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: window.textSecondaryColor
                    }
                }

                Slider {
                    Layout.fillWidth: true
                    from: 0.0
                    to: 1.0
                    value: customColorDialog.value
                    onMoved: customColorDialog.value = value

                    background: Rectangle {
                        x: parent.leftPadding
                        y: parent.topPadding + parent.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 8
                        width: parent.availableWidth
                        height: implicitHeight
                        radius: 4

                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#000000" }
                            GradientStop { position: 1.0; color: Qt.hsva(customColorDialog.hue / 360, customColorDialog.saturation, 1.0, 1.0) }
                        }
                    }

                    handle: Rectangle {
                        x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                        y: parent.topPadding + parent.availableHeight / 2 - height / 2
                        implicitWidth: 24
                        implicitHeight: 24
                        radius: 12
                        color: customColorDialog.selectedColor
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

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    text: "#"
                    font.pixelSize: 16
                    font.weight: Font.Bold
                    color: window.textColor
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.15) : window.cardColor3
                    radius: 12
                    border.width: hexInput.activeFocus ? 2 : 1
                    border.color: hexInput.activeFocus ? customColorDialog.selectedColor : (modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.3) : window.borderColor)

                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    TextInput {
                        id: hexInput
                        anchors.fill: parent
                        anchors.margins: 12
                        text: customColorDialog.selectedColor.toString().substring(1, 7).toUpperCase()
                        font.pixelSize: 14
                        font.family: "Consolas"
                        font.weight: Font.Bold
                        color: window.textColor
                        verticalAlignment: Text.AlignVCenter
                        selectByMouse: true
                        maximumLength: 6

                        onAccepted: {
                            var hexColor = "#" + text
                            if (/^#[0-9A-Fa-f]{6}$/.test(hexColor)) {
                                var color = Qt.color(hexColor)
                                customColorDialog.hue = color.hsvHue * 360
                                customColorDialog.saturation = color.hsvSaturation
                                customColorDialog.value = color.hsvValue
                            }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: 4 }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    text: "Cancel"
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        color: parent.hovered ? window.textColor : window.textSecondaryColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter

                        Behavior on color { ColorAnimation { duration: 200 } }
                    }

                    background: Rectangle {
                        color: parent.hovered ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(1, 1, 1, 0.08)
                        radius: 24
                        border.width: 1.5
                        border.color: Qt.rgba(1, 1, 1, 0.3)

                        Behavior on color { ColorAnimation { duration: 200 } }
                    }

                    onClicked: {

                        window.primaryColor = customColorDialog.originalColor
                        window.accentColor = Qt.lighter(customColorDialog.originalColor, 1.3)
                        customColorDialog.close()
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    text: "Done"
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: parent.hovered ? Qt.lighter(customColorDialog.selectedColor, 1.2) : customColorDialog.selectedColor
                        radius: 24
                        border.width: 2
                        border.color: "#FFFFFF"

                        Behavior on color { ColorAnimation { duration: 200 } }

                        layer.enabled: true
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowColor: Qt.rgba(0, 0, 0, 0.3)
                            shadowVerticalOffset: 3
                            shadowBlur: 0.4
                        }
                    }

                    onClicked: {

                        var colorString = customColorDialog.selectedColor.toString()
                        modManager.setAccentColor(colorString)
                        customColorDialog.close()
                    }
                }
            }
        }
    }
}
