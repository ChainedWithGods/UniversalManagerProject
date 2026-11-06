import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Popup {
    id: colorPicker

    required property string targetProperty
    required property color currentColor

    width: 870
    height: 600
    modal: true
    focus: true
    anchors.centerIn: parent
    padding: 0
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    property real hue: 0
    property real saturation: 0.7
    property real value: 0.8
    property color selectedColor: Qt.hsva(0, 0.7, 0.8, 1.0)
    property bool liveUpdate: false
    property color originalColor: "#5EC4D9"
    property bool committed: false

    readonly property color panelColor: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.075) : window.cardColor3
    readonly property color panelBorderColor: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.16) : Qt.alpha(window.borderColor, 0.75)
    readonly property color subtleBorderColor: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.11) : Qt.alpha(window.borderColor, 0.55)
    readonly property color fieldBackgroundColor: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.06) : Qt.lighter(window.cardColor3, 1.04)
    readonly property color accentGlowColor: Qt.rgba(selectedColor.r, selectedColor.g, selectedColor.b, modManager.theme === "Glassmorphic" ? 0.16 : 0.08)

    function clamp01(number) {
        return Math.max(0, Math.min(1, number))
    }

    function clampByte(number) {
        return Math.max(0, Math.min(255, number))
    }

    function roundByte(channel) {
        return Math.round(channel * 255)
    }

    function updateSelectedColor() {
        selectedColor = Qt.hsva(hue / 360, saturation, value, 1.0)
    }

    function formatHex(colorValue) {
        return colorValue.toString().substring(1, 7).toUpperCase()
    }

    function syncFromColor(colorValue, previewTarget) {
        var resolvedHue = colorValue.hsvHue >= 0 ? colorValue.hsvHue * 360 : hue
        var shouldPreview = previewTarget !== false

        liveUpdate = false
        hue = Math.max(0, Math.min(359.99, resolvedHue))
        saturation = clamp01(colorValue.hsvSaturation)
        value = clamp01(colorValue.hsvValue)
        updateSelectedColor()
        liveUpdate = true

        if (shouldPreview)
            updateColor()
    }

    function setFromHex(hexText) {
        var normalized = hexText.replace("#", "")
        if (/^[0-9A-Fa-f]{6}$/.test(normalized))
            syncFromColor(Qt.color("#" + normalized))
    }

    function setFromRgb(redText, greenText, blueText) {
        if (redText === "" || greenText === "" || blueText === "")
            return

        var red = clampByte(parseInt(redText, 10))
        var green = clampByte(parseInt(greenText, 10))
        var blue = clampByte(parseInt(blueText, 10))

        if (isNaN(red) || isNaN(green) || isNaN(blue))
            return

        syncFromColor(Qt.rgba(red / 255, green / 255, blue / 255, 1.0))
    }

    function updateFromSpectrum(xPos, yPos, areaWidth, areaHeight) {
        var clampedX = Math.max(0, Math.min(areaWidth, xPos))
        var clampedY = Math.max(0, Math.min(areaHeight, yPos))
        saturation = areaWidth > 0 ? clamp01(clampedX / areaWidth) : 0
        value = areaHeight > 0 ? clamp01(1 - (clampedY / areaHeight)) : 0
    }

    function updateHueFromPosition(yPos, areaHeight) {
        var clampedY = Math.max(0, Math.min(areaHeight, yPos))
        hue = areaHeight > 0 ? clamp01(clampedY / areaHeight) * 359.99 : 0
    }

    function applyColorToTarget(colorString) {
        if (targetProperty === "idColor") modManager.setIdColor(colorString)
        else if (targetProperty === "enableButtonColor") modManager.setEnableButtonColor(colorString)
        else if (targetProperty === "disableButtonColor") modManager.setDisableButtonColor(colorString)
        else if (targetProperty === "folderModColor") modManager.setFolderModColor(colorString)
        else if (targetProperty === "looseFileColor") modManager.setLooseFileColor(colorString)
        else if (targetProperty === "tagColor") modManager.setTagColor(colorString)
        else if (targetProperty === "pakColor") modManager.setPakColor(colorString)
        else if (targetProperty === "ucasColor") modManager.setUcasColor(colorString)
        else if (targetProperty === "utocColor") modManager.setUtocColor(colorString)
        else if (targetProperty === "navBarColor") modManager.setNavBarColor(colorString)
        else if (targetProperty === "navBarIconColor") modManager.setNavBarIconColor(colorString)
        else if (targetProperty === "accentColor") {
            var c = Qt.color(colorString)
            window.primaryColor = c
            window.accentColor = Qt.lighter(c, 1.3)
            modManager.setAccentColor(c.toString())
        }
        else if (targetProperty === "cpuUsageBarColor") modManager.setCpuUsageBarColor(colorString)
        else if (targetProperty === "ramUsageBarColor") modManager.setRamUsageBarColor(colorString)
        else if (targetProperty === "driveUsageBarColor") modManager.setDriveUsageBarColor(colorString)
    }

    function updateColor() {
        applyColorToTarget(selectedColor.toString())
    }

    function revertColor() {
        applyColorToTarget(originalColor.toString())
    }

    onHueChanged: {
        updateSelectedColor()
        if (liveUpdate)
            liveUpdateTimer.restart()
    }

    onSaturationChanged: {
        updateSelectedColor()
        if (liveUpdate)
            liveUpdateTimer.restart()
    }

    onValueChanged: {
        updateSelectedColor()
        if (liveUpdate)
            liveUpdateTimer.restart()
    }

    Timer {
        id: liveUpdateTimer
        interval: 40
        repeat: false
        onTriggered: colorPicker.updateColor()
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 220; easing.type: Easing.OutQuad }
        NumberAnimation { property: "scale"; from: 0.98; to: 1.0; duration: 220; easing.type: Easing.OutCubic }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 150; easing.type: Easing.InQuad }
        NumberAnimation { property: "scale"; from: 1.0; to: 0.99; duration: 150; easing.type: Easing.InQuad }
    }

    onOpened: {
        originalColor = currentColor
        committed = false
        syncFromColor(currentColor, false)
    }

    onClosed: {
        if (!committed)
            revertColor()
    }

    background: Rectangle {
        color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.08, 0.09, 0.13, 0.98) : window.surfaceColor
        radius: 30
        border.width: modManager.theme === "Glassmorphic" ? 1.4 : 1
        border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.22) : window.borderColor

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, modManager.theme === "Glassmorphic" ? 0.05 : 0.02) }
                GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0.0) }
            }
        }

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.50)
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 18
            shadowBlur: 0.85
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Text {
                    text: "Pick a Color"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    color: window.textColor
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            spacing: 16

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 24
                color: colorPicker.panelColor
                border.width: 1
                border.color: colorPicker.panelBorderColor

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 360
                        spacing: 16

                        Rectangle {
                            id: spectrumPanel
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 22
                            color: Qt.hsva(colorPicker.hue / 360, 1.0, 1.0, 1.0)
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.15)
                            clip: true

                            Rectangle {
                                anchors.fill: parent
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: "#FFFFFFFF" }
                                    GradientStop { position: 1.0; color: "#00FFFFFF" }
                                }
                            }

                            Rectangle {
                                anchors.fill: parent
                                gradient: Gradient {
                                    orientation: Gradient.Vertical
                                    GradientStop { position: 0.0; color: "#00000000" }
                                    GradientStop { position: 1.0; color: "#FF000000" }
                                }
                            }

                            Rectangle {
                                width: 20
                                height: 20
                                radius: 10
                                x: Math.max(0, Math.min(spectrumPanel.width - width, (colorPicker.saturation * spectrumPanel.width) - width / 2))
                                y: Math.max(0, Math.min(spectrumPanel.height - height, ((1 - colorPicker.value) * spectrumPanel.height) - height / 2))
                                color: "transparent"
                                border.width: 2.5
                                border.color: "#FFFFFF"

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 6
                                    height: 6
                                    radius: 3
                                    color: "#FFFFFF"
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -1
                                    radius: width / 2
                                    color: "transparent"
                                    border.width: 1
                                    border.color: Qt.rgba(0, 0, 0, 0.45)
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                cursorShape: Qt.CrossCursor
                                preventStealing: true

                                function updateAt(mouse) {
                                    colorPicker.updateFromSpectrum(mouse.x, mouse.y, width, height)
                                }

                                onPressed: (mouse) => updateAt(mouse)
                                onPositionChanged: (mouse) => {
                                    if (pressed)
                                        updateAt(mouse)
                                }
                                onReleased: {

                                    liveUpdateTimer.stop()
                                    colorPicker.updateColor()
                                }
                            }
                        }

                        Rectangle {
                            id: hueBar
                            Layout.preferredWidth: 32
                            Layout.fillHeight: true
                            radius: 16
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.15)
                            clip: true

                            gradient: Gradient {
                                orientation: Gradient.Vertical
                                GradientStop { position: 0.0; color: "#FF0000" }
                                GradientStop { position: 0.166; color: "#FFFF00" }
                                GradientStop { position: 0.333; color: "#00FF00" }
                                GradientStop { position: 0.5; color: "#00FFFF" }
                                GradientStop { position: 0.666; color: "#0000FF" }
                                GradientStop { position: 0.833; color: "#FF00FF" }
                                GradientStop { position: 1.0; color: "#FF0000" }
                            }

                            Rectangle {
                                width: hueBar.width + 12
                                height: 12
                                radius: 6
                                x: -6
                                y: Math.max(0, Math.min(hueBar.height - height, (colorPicker.hue / 359.99) * hueBar.height - height / 2))
                                color: Qt.rgba(1, 1, 1, 0.94)
                                border.width: 1
                                border.color: Qt.rgba(0, 0, 0, 0.20)

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -1
                                    radius: width / 2
                                    color: "transparent"
                                    border.width: 1
                                    border.color: Qt.rgba(0, 0, 0, 0.30)
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                cursorShape: Qt.SizeVerCursor
                                preventStealing: true

                                function updateAt(mouse) {
                                    colorPicker.updateHueFromPosition(mouse.y, height)
                                }

                                onPressed: (mouse) => updateAt(mouse)
                                onPositionChanged: (mouse) => {
                                    if (pressed)
                                        updateAt(mouse)
                                }
                                onReleased: {

                                    liveUpdateTimer.stop()
                                    colorPicker.updateColor()
                                }
                            }
                        }
                    }

                }
            }

            Rectangle {
                Layout.preferredWidth: 296
                Layout.fillHeight: true
                radius: 24
                color: colorPicker.panelColor
                border.width: 1
                border.color: colorPicker.panelBorderColor
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Flickable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: width
                        contentHeight: sidebarColumn.height
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        Column {
                            id: sidebarColumn
                            width: parent.width
                            spacing: 12

                        Rectangle {
                            width: parent.width
                            height: 88
                            radius: 20
                            color: Qt.rgba(1, 1, 1, 0.05)
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.08)

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                anchors.bottomMargin: 16
                                spacing: 8

                                Text {
                                    text: "Preview"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    color: window.textColor
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 42
                                    radius: 14
                                    color: colorPicker.selectedColor
                                    border.width: 1
                                    border.color: Qt.rgba(1, 1, 1, 0.20)
                                }
                            }
                        }

                        ColumnLayout {
                            width: parent.width
                            spacing: 8

                            Text {
                                text: "Hex"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: window.textColor
                            }

                            TextField {
                                id: hexInput
                                Layout.fillWidth: true
                                text: colorPicker.formatHex(colorPicker.selectedColor)
                                placeholderText: "RRGGBB"
                                maximumLength: 6
                                color: window.textColor
                                font.pixelSize: 13
                                font.family: "Consolas"
                                font.weight: Font.DemiBold
                                selectByMouse: true
                                leftPadding: 14
                                rightPadding: 14
                                topPadding: 12
                                bottomPadding: 12
                                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhPreferUppercase
                                validator: RegularExpressionValidator { regularExpression: /[0-9A-Fa-f]{0,6}/ }
                                ContextMenu.menu: Menu {
                                    id: hexContextMenu
                                    onAboutToShow: Qt.callLater(close)
                                }

                                property bool isEditing: false

                                background: Rectangle {
                                    radius: 14
                                    color: colorPicker.fieldBackgroundColor
                                    border.width: hexInput.activeFocus ? 1.5 : 1
                                    border.color: hexInput.activeFocus ? colorPicker.selectedColor : colorPicker.subtleBorderColor

                                    Behavior on border.color { ColorAnimation { duration: 140 } }
                                    Behavior on border.width { NumberAnimation { duration: 140 } }
                                }

                                Connections {
                                    target: colorPicker

                                    function onSelectedColorChanged() {
                                        if (!hexInput.isEditing)
                                            hexInput.text = colorPicker.formatHex(colorPicker.selectedColor)
                                    }
                                }

                                onTextEdited: {
                                    isEditing = true
                                    var upper = text.toUpperCase()
                                    if (text !== upper) {
                                        var cursor = cursorPosition
                                        text = upper
                                        cursorPosition = cursor
                                    }

                                    if (text.length === 6)
                                        colorPicker.setFromHex(text)
                                }

                                onAccepted: {
                                    colorPicker.setFromHex(text)
                                    isEditing = false
                                }

                                onEditingFinished: {
                                    colorPicker.setFromHex(text)
                                    isEditing = false
                                }
                            }
                        }

                        ColumnLayout {
                            width: parent.width
                            spacing: 8

                            Text {
                                text: "RGB"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: window.textColor
                            }

                            RowLayout {
                                width: parent.width
                                spacing: 8

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 44
                                    radius: 12
                                    color: colorPicker.fieldBackgroundColor
                                    border.width: 1
                                    border.color: colorPicker.subtleBorderColor

                                    Text {
                                        anchors.centerIn: parent
                                        text: String(colorPicker.roundByte(colorPicker.selectedColor.r))
                                        color: window.textColor
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 44
                                    radius: 12
                                    color: colorPicker.fieldBackgroundColor
                                    border.width: 1
                                    border.color: colorPicker.subtleBorderColor

                                    Text {
                                        anchors.centerIn: parent
                                        text: String(colorPicker.roundByte(colorPicker.selectedColor.g))
                                        color: window.textColor
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 44
                                    radius: 12
                                    color: colorPicker.fieldBackgroundColor
                                    border.width: 1
                                    border.color: colorPicker.subtleBorderColor

                                    Text {
                                        anchors.centerIn: parent
                                        text: String(colorPicker.roundByte(colorPicker.selectedColor.b))
                                        color: window.textColor
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            width: parent.width
                            spacing: 8

                            Text {
                                text: "HSB"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: window.textColor
                            }

                            RowLayout {
                                width: parent.width
                                spacing: 8

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 56
                                    radius: 14
                                    color: Qt.rgba(1, 1, 1, 0.04)
                                    border.width: 1
                                    border.color: Qt.rgba(1, 1, 1, 0.08)

                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 3

                                        Text {
                                            text: "H"
                                            font.pixelSize: 10
                                            font.weight: Font.Bold
                                            color: window.textSecondaryColor
                                            anchors.horizontalCenter: parent.horizontalCenter
                                        }

                                        Text {
                                            text: Math.round(colorPicker.hue) + "°"
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                            color: window.textColor
                                            anchors.horizontalCenter: parent.horizontalCenter
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 56
                                    radius: 14
                                    color: Qt.rgba(1, 1, 1, 0.04)
                                    border.width: 1
                                    border.color: Qt.rgba(1, 1, 1, 0.08)

                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 3

                                        Text {
                                            text: "S"
                                            font.pixelSize: 10
                                            font.weight: Font.Bold
                                            color: window.textSecondaryColor
                                            anchors.horizontalCenter: parent.horizontalCenter
                                        }

                                        Text {
                                            text: Math.round(colorPicker.saturation * 100) + "%"
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                            color: window.textColor
                                            anchors.horizontalCenter: parent.horizontalCenter
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 56
                                    radius: 14
                                    color: Qt.rgba(1, 1, 1, 0.04)
                                    border.width: 1
                                    border.color: Qt.rgba(1, 1, 1, 0.08)

                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 3

                                        Text {
                                            text: "B"
                                            font.pixelSize: 10
                                            font.weight: Font.Bold
                                            color: window.textSecondaryColor
                                            anchors.horizontalCenter: parent.horizontalCenter
                                        }

                                        Text {
                                            text: Math.round(colorPicker.value * 100) + "%"
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                            color: window.textColor
                                            anchors.horizontalCenter: parent.horizontalCenter
                                        }
                                    }
                                }
                            }
                        }

                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        id: cancelButton
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        text: "Cancel"
                        flat: true
                        hoverEnabled: true
                        onClicked: colorPicker.close()

                        contentItem: Text {
                            text: cancelButton.text
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: cancelButton.hovered ? window.textColor : window.textSecondaryColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 16
                            color: cancelButton.hovered ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(1, 1, 1, 0.07)
                            border.width: 1.2
                            border.color: cancelButton.hovered ? Qt.rgba(1, 1, 1, 0.28) : colorPicker.subtleBorderColor

                            Behavior on color { ColorAnimation { duration: 160 } }
                            Behavior on border.color { ColorAnimation { duration: 160 } }
                        }
                    }

                    Button {
                        id: applyButton
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        text: "Apply"
                        flat: true
                        hoverEnabled: true
                        onClicked: {
                            committed = true
                            updateColor()
                            colorPicker.close()
                        }

                        contentItem: Text {
                            text: applyButton.text
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: applyButton.hovered ? window.textColor : window.textSecondaryColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 16
                            color: applyButton.hovered ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(1, 1, 1, 0.07)
                            border.width: 1.2
                            border.color: applyButton.hovered ? Qt.rgba(1, 1, 1, 0.28) : colorPicker.subtleBorderColor

                            Behavior on color { ColorAnimation { duration: 160 } }
                            Behavior on border.color { ColorAnimation { duration: 160 } }
                        }
                    }
                }

                }
            }
        }
    }
}
