import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Dialogs
import Qt.labs.folderlistmodel
import Qt5Compat.GraphicalEffects
import QtMultimedia

Item {
    id: runtimeTabDialogs

    property alias generatingDialog: generatingDialog
    property alias initializationDialog: initializationDialog
    property alias runtimeConfigDialog: runtimeConfigDialog
    property alias runtimeOutputPathDialog: runtimeOutputPathDialog
    property alias avaloniumConflictDialog: avaloniumConflictDialog
    property alias cDriveAdminDialog: cDriveAdminDialog
    property alias unsupportedVersionGameRunningDialog: unsupportedVersionGameRunningDialog
    property alias runtimeAlertDialog: runtimeAlertDialog
    property var toastNotification: null

    Gradient {
        id: cozyDialogGradient
        GradientStop { position: 0.0; color: window.cozyDialogGradientTop }
        GradientStop { position: 1.0; color: window.cozyDialogGradientBottom }
    }
    Gradient {
        id: crimsonDialogGradient
        GradientStop { position: 0.0; color: window.crimsonDialogGradientTop }
        GradientStop { position: 1.0; color: window.crimsonDialogGradientBottom }
    }
    Gradient {
        id: waveyDialogGradient
        GradientStop { position: 0.0; color: window.waveyDialogGradientTop }
        GradientStop { position: 1.0; color: window.waveyDialogGradientBottom }
    }

    component RuntimeContextMenuItem : MenuItem {
        id: menuItem
        property string iconName: ""
        property bool isDanger: false
        property string shortcutText: ""

        implicitWidth: 180
        implicitHeight: 32
        width: parent ? parent.width : implicitWidth
        height: 32

        contentItem: RowLayout {
            spacing: 12

            Image {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                source: menuItem.iconName !== "" ? "qrc:/UniversalModManager/Lucide Icons/" + menuItem.iconName : ""
                sourceSize: Qt.size(16, 16)
                visible: menuItem.iconName !== ""
                opacity: menuItem.enabled ? 1.0 : 0.5

                layer.enabled: true
                layer.effect: ColorOverlay {
                    color: menuItem.isDanger ? "#EF4444" : ((modManager.theme === "Glassmorphic" || modManager.theme === "Wavey") ? "#FFFFFF" : window.textColor)
                }
            }

            Text {
                Layout.fillWidth: true
                text: menuItem.text
                font.pixelSize: 13
                font.weight: Font.Medium
                color: menuItem.isDanger ? "#EF4444" : ((modManager.theme === "Glassmorphic" || modManager.theme === "Wavey") ? "#FFFFFF" : window.textColor)
                opacity: menuItem.enabled ? 1.0 : 0.5
            }

            Text {
                text: menuItem.shortcutText
                font.pixelSize: 11
                color: (modManager.theme === "Glassmorphic" || modManager.theme === "Wavey") ? Qt.rgba(1, 1, 1, 0.5) : window.textSecondaryColor
                visible: menuItem.shortcutText !== ""
                opacity: menuItem.enabled ? 1.0 : 0.5
            }
        }

        background: Rectangle {
            color: menuItem.highlighted
                ? (modManager.theme === "Glassmorphic"
                    ? Qt.rgba(1, 1, 1, 0.10)
                    : (modManager.theme === "Crimson"
                        ? Qt.rgba(0.65, 0.40, 0.95, 0.20)
                        : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)))
                : "transparent"
            radius: 8

            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    Dialog {
        id: generatingDialog
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        width: 420
        padding: 0

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy") return cozyDialogGradient
                if (modManager.theme === "Crimson") return crimsonDialogGradient
                if (modManager.theme === "Wavey") return waveyDialogGradient
                return null
            }

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.7)
            Behavior on opacity { NumberAnimation { duration: 250 } }
        }

        enter: Transition {
            NumberAnimation { property: "scale"; from: 0.9; to: 1.0; duration: 300; easing.type: Easing.OutBack }
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 250 }
        }

        exit: Transition {
            NumberAnimation { property: "scale"; from: 1.0; to: 0.95; duration: 200; easing.type: Easing.InCubic }
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 200 }
        }

        Connections {
            target: modManager
            function onGeneratingChanged() {
                if (modManager.generating) {
                    generatingDialog.open()
                } else {
                    generatingDialog.close()
                }
            }
        }

        contentItem: Item {
            implicitWidth: generatingDialog.width
            implicitHeight: generatingContentColumn.implicitHeight

            ColumnLayout {
                id: generatingContentColumn
                anchors.fill: parent
                anchors.margins: 0
                spacing: 0

                Item { Layout.preferredHeight: 40 }

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    width: 200
                    height: 84

                    UnrealEngineRuntimeTabAnimations {
                        anchors.centerIn: parent
                        width: 200
                        height: 60
                        running: generatingDialog.visible
                        text: "Generating..."
                        pixelSize: 26
                    }
                }

                Item { Layout.preferredHeight: 56 }
            }
        }
    }

    Dialog {
        id: initializationDialog
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        width: 420
        padding: 0

        property bool waitingInBackground: false
        property bool isSleeping: false
        property int totalDelaySeconds: 0
        property int remainingDelaySeconds: 0
        property int animationRestartTrigger: 0
        property string unsupportedVersionError: ""
        property bool gameStillRunningAfterError: false

        Timer {
            id: dialogOpenTimer
            interval: 150
            repeat: false
            onTriggered: {
                if (modManager.initializing) {
                    initializationDialog.animationRestartTrigger++
                    initializationDialog.open()
                }
            }
        }

        Timer {
            id: sleepCountdownTimer
            interval: 1000
            repeat: true
            running: initializationDialog.isSleeping && initializationDialog.visible
            onTriggered: {
                if (initializationDialog.remainingDelaySeconds > 0) {
                    initializationDialog.remainingDelaySeconds--
                } else {
                    initializationDialog.isSleeping = false
                    stop()
                }
            }
        }

        function formatDelayTime(seconds) {
            if (seconds <= 0) return "0 seconds"
            var mins = Math.floor(seconds / 60)
            var secs = seconds % 60
            if (mins > 0 && secs > 0) {
                return mins + (mins === 1 ? " minute " : " minutes ") + secs + (secs === 1 ? " second" : " seconds")
            } else if (mins > 0) {
                return mins + (mins === 1 ? " minute" : " minutes")
            } else {
                return secs + (secs === 1 ? " second" : " seconds")
            }
        }

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy") return cozyDialogGradient
                if (modManager.theme === "Crimson") return crimsonDialogGradient
                if (modManager.theme === "Wavey") return waveyDialogGradient
                return null
            }

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.7)
            Behavior on opacity { NumberAnimation { duration: 250 } }
        }

        enter: Transition {
            NumberAnimation { property: "scale"; from: 0.9; to: 1.0; duration: 300; easing.type: Easing.OutBack }
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 250 }
        }

        exit: Transition {
            NumberAnimation { property: "scale"; from: 1.0; to: 0.95; duration: 200; easing.type: Easing.InCubic }
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 200 }
        }

        Connections {
            target: modManager
            function onInitializingChanged() {
                if (modManager.initializing && !initializationDialog.waitingInBackground) {

                    initializationDialog.unsupportedVersionError = ""

                    var config = modManager.loadAvaloniumConfig()
                    var delayMinutes = parseInt(config.InitDelayMinutes) || 0
                    var delaySeconds = parseInt(config.InitDelaySeconds) || 0
                    initializationDialog.totalDelaySeconds = delayMinutes * 60 + delaySeconds
                    initializationDialog.remainingDelaySeconds = initializationDialog.totalDelaySeconds
                    initializationDialog.isSleeping = initializationDialog.totalDelaySeconds > 0

                    dialogOpenTimer.restart()
                } else if (!modManager.initializing) {

                    dialogOpenTimer.stop()

                    var initCompletedSuccessfully = modManager.runtimeReady
                    initializationDialog.waitingInBackground = false
                    initializationDialog.isSleeping = false

                    initializationDialog.gameStillRunningAfterError = false
                    initializationDialog.close()
                    if (initCompletedSuccessfully && modManager.notificationsEnabled) {
                        toastNotification.show("Avalonium initialization complete")
                        initializationCompleteSound.stop()
                        initializationCompleteSound.play()
                    }
                }
            }

            function onUniversalRuntimeLogChanged() {
                if (!initializationDialog.visible || initializationDialog.unsupportedVersionError !== "")
                    return
                var log = modManager.universalRuntimeLog
                var marker = "Initialization failed: Unsupported UE version"
                var idx = log.indexOf(marker)
                if (idx !== -1) {

                    var end = log.indexOf("\n", idx)
                    var errorLine = end !== -1 ? log.substring(idx, end).trim() : log.substring(idx).trim()

                    var bracketEnd = errorLine.lastIndexOf("] ")
                    if (bracketEnd !== -1 && bracketEnd < errorLine.indexOf("Initialization failed"))
                        errorLine = errorLine.substring(bracketEnd + 2).trim()
                    initializationDialog.unsupportedVersionError = errorLine
                    initializationDialog.isSleeping = false
                }
            }
        }

        onClosed: {
            initializationDialog.unsupportedVersionError = ""
            if (!modManager.initializing) {
                waitingInBackground = false
            }
        }

        contentItem: Item {
            implicitWidth: initializationDialog.width
            implicitHeight: contentColumn.implicitHeight

            ColumnLayout {
                id: contentColumn
                anchors.fill: parent
                anchors.margins: 0
                spacing: 0

                Item {
                    id: errorStateView
                    visible: initializationDialog.unsupportedVersionError !== ""
                    Layout.fillWidth: true
                    implicitHeight: errorStateColumn.implicitHeight

                    ColumnLayout {
                        id: errorStateColumn
                        anchors.fill: parent
                        anchors.margins: 0
                        spacing: 0

                        Item { Layout.preferredHeight: 36 }

                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48

                            Image {
                                id: errorStateAlertIcon
                                anchors.fill: parent
                                source: {
                                    if (modManager.theme === "Cozy")
                                        return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/alert.png"
                                    if (modManager.theme === "Glassmorphic")
                                        return "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/alert.png"
                                    if (modManager.theme === "Crimson")
                                        return "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/alert.png"
                                    if (modManager.theme === "Wavey")
                                        return "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/alert.png"
                                    return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/alert.png"
                                }
                                sourceSize: Qt.size(48, 48)
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                visible: modManager.theme !== "Glassmorphic"
                            }

                            ColorOverlay {
                                anchors.fill: errorStateAlertIcon
                                source: errorStateAlertIcon
                                color: window.primaryColor
                                visible: modManager.theme === "Glassmorphic"
                            }
                        }

                        Item { Layout.preferredHeight: 20 }

                        Text {
                            Layout.fillWidth: true
                            Layout.leftMargin: 40
                            Layout.rightMargin: 40
                            text: "Initialization Failed"
                            font.pixelSize: 20
                            font.weight: Font.Bold
                            color: modManager.theme === "Glassmorphic" ? "#FFFFFF" : window.textColor
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                        }

                        Item { Layout.preferredHeight: 10 }

                        Text {
                            Layout.fillWidth: true
                            Layout.leftMargin: 40
                            Layout.rightMargin: 40
                            text: initializationDialog.unsupportedVersionError
                            font.pixelSize: 13
                            color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.65) : window.textSecondaryColor
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                            lineHeight: 1.4
                        }

                        Item { Layout.preferredHeight: 28 }

                        Button {
                            id: errorCloseButton
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 150
                            Layout.preferredHeight: 48
                            Layout.bottomMargin: 28
                            flat: true
                            hoverEnabled: true

                            contentItem: Text {
                                text: "Close"
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                color: errorCloseButton.hovered ? "#FFFFFF" : window.textColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                Behavior on color { ColorAnimation { duration: 200 } }
                            }

                            background: Rectangle {
                                color: errorCloseButton.hovered
                                    ? window.primaryColor
                                    : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                                radius: 24
                                border.width: 1.5
                                border.color: errorCloseButton.hovered
                                    ? window.primaryColor
                                    : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)
                                Behavior on color { ColorAnimation { duration: 200 } }
                                Behavior on border.color { ColorAnimation { duration: 200 } }
                            }

                            onClicked: {
                                if (modManager.initializing || (typeof modManager.isGameProcessRunning === "function" && modManager.isGameProcessRunning())) {
                                    initializationDialog.gameStillRunningAfterError = true
                                }
                                initializationDialog.waitingInBackground = false
                                initializationDialog.close()
                            }
                        }
                    }
                }

                Item {
                    id: normalStateView
                    visible: initializationDialog.unsupportedVersionError === ""
                    Layout.fillWidth: true
                    implicitHeight: normalStateColumn.implicitHeight

                    ColumnLayout {
                        id: normalStateColumn
                        anchors.fill: parent
                        anchors.margins: 0
                        spacing: 0

                        Item { Layout.preferredHeight: 32 }

                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            width: 84
                            height: 84
                            visible: initializationDialog.isSleeping

                            UnrealEngineRuntimeTabAnimations.HourglassAnimation {
                                anchors.fill: parent
                                visible: initializationDialog.isSleeping
                                running: initializationDialog.visible && initializationDialog.isSleeping
                                restartTrigger: initializationDialog.animationRestartTrigger
                            }
                        }

                        Item { Layout.preferredHeight: initializationDialog.isSleeping ? 24 : 16 }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 40
                            Layout.rightMargin: 40
                            spacing: 12

                            Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 32

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 2

                                    visible: !initializationDialog.isSleeping

                                    property var letters: ["I", "n", "i", "t", "i", "a", "l", "i", "z", "i", "n", "g", " ", "A", "v", "a", "l", "o", "n", "i", "u", "m"]

                                    Repeater {
                                        model: parent.letters

                                        Text {
                                            id: animLetter
                                            text: modelData
                                            font.pixelSize: 20
                                            font.weight: Font.Bold
                                            font.letterSpacing: modelData === " " ? 8 : 0.5
                                            color: modManager.theme === "Glassmorphic" ? "#FFFFFF" : window.textColor
                                            opacity: 0

                                            layer.enabled: true
                                            layer.effect: MultiEffect {
                                                shadowEnabled: true
                                                shadowColor: window.primaryColor
                                                shadowBlur: 0.85
                                                shadowOpacity: animLetter.opacity * 0.9
                                                blur: 0.15
                                                blurMax: 16
                                                blurMultiplier: 0.8
                                            }

                                            SequentialAnimation on opacity {
                                                id: letterAnim
                                                loops: Animation.Infinite
                                                running: !initializationDialog.isSleeping && initializationDialog.visible
                                                    && initializationDialog.unsupportedVersionError === ""

                                                PauseAnimation { duration: index * 50 }

                                                NumberAnimation {
                                                    from: 0
                                                    to: 1
                                                    duration: 250
                                                    easing.type: Easing.InOutCubic
                                                }

                                                PauseAnimation { duration: 200 }

                                                NumberAnimation {
                                                    from: 1
                                                    to: 0
                                                    duration: 250
                                                    easing.type: Easing.InOutCubic
                                                }

                                                PauseAnimation { duration: (29 - index) * 50 }
                                            }

                                            Connections {
                                                target: initializationDialog
                                                function onAnimationRestartTriggerChanged() {
                                                    animLetter.opacity = 0
                                                    letterAnim.restart()
                                                }
                                            }
                                        }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: initializationDialog.isSleeping
                                    text: "Sleeping (" + initializationDialog.formatDelayTime(initializationDialog.remainingDelaySeconds) + ")"
                                    font.pixelSize: 20
                                    font.weight: Font.Bold
                                    color: modManager.theme === "Glassmorphic" ? "#FFFFFF" : window.textColor
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "Waiting for the runtime to initialize..."
                                font.pixelSize: 14
                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.7) : window.textSecondaryColor
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                                lineHeight: 1.3
                            }
                        }

                        Item { Layout.fillHeight: true; Layout.preferredHeight: 20 }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.bottomMargin: 28
                            spacing: 16

                            Button {
                                id: waitInBackgroundButton
                                Layout.preferredWidth: 150
                                Layout.preferredHeight: 48
                                text: "Wait in background"
                                flat: true
                                hoverEnabled: true

                                contentItem: Text {
                                    text: parent.text
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: waitInBackgroundButton.hovered ? window.secondaryButtonTextHoverColor : window.secondaryButtonTextColor
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }

                                background: Rectangle {
                                    color: waitInBackgroundButton.hovered ? window.secondaryButtonHoverColor : window.secondaryButtonColor
                                    radius: 26
                                    border.width: 1
                                    border.color: window.secondaryButtonBorderColor
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                }

                                onClicked: {
                                    initializationDialog.waitingInBackground = true
                                    initializationDialog.close()
                                }
                            }

                            Button {
                                id: killProcessButton
                                Layout.preferredWidth: 150
                                Layout.preferredHeight: 48
                                flat: true
                                hoverEnabled: true

                                contentItem: Text {
                                    text: "Kill Process"
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    color: killProcessButton.hovered ? "#FFFFFF" : window.textColor
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    Behavior on color { ColorAnimation { duration: 200 } }
                                }

                                background: Rectangle {
                                    color: killProcessButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                                    radius: 24
                                    border.width: 1.5
                                    border.color: killProcessButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)
                                    Behavior on color { ColorAnimation { duration: 200 } }
                                    Behavior on border.color { ColorAnimation { duration: 200 } }
                                }

                                onClicked: {
                                    modManager.killGameProcess()
                                    initializationDialog.waitingInBackground = false
                                    initializationDialog.close()
                                    toastNotification.show("Game process terminated")
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component LuaModToggle : Item {
        id: luaModToggle
        property bool checked: false
        signal toggled(bool checked)

        width: 46
        height: 24

        readonly property real _circleDiameter: 18
        readonly property real _offset: (height - _circleDiameter) / 2
        readonly property color _accent: window.primaryColor
        readonly property color _offBg: Qt.rgba(_accent.r, _accent.g, _accent.b,
                                               modManager.theme === "Cozy" ? 0.35 : 0.25)

        Rectangle {
            id: luaModToggleSlider
            anchors.fill: parent
            radius: height / 2
            color: luaModToggle.checked ? luaModToggle._accent : luaModToggle._offBg

            Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutBack } }

            Rectangle {
                width: luaModToggle._circleDiameter / 2
                height: width / 2 - 1
                radius: 1
                color: "#ffffff"
                anchors.verticalCenter: parent.verticalCenter
                x: luaModToggle.checked
                   ? (parent.width - luaModToggle._offset - width - (width / 2))
                   : (luaModToggle._offset + (width / 2))
                Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }
            }

            Rectangle {
                width: luaModToggle._circleDiameter
                height: luaModToggle._circleDiameter
                radius: width / 2
                color: "#ffffff"
                anchors.verticalCenter: parent.verticalCenter
                x: luaModToggle.checked
                   ? (parent.width - luaModToggle._offset - width)
                   : luaModToggle._offset

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(0, 0, 0, 0.32)
                    shadowHorizontalOffset: 1
                    shadowVerticalOffset: 1
                    shadowBlur: 0.45
                }

                Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }

                Text {
                    anchors.centerIn: parent
                    text: "×"
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    color: Qt.rgba(luaModToggle._accent.r, luaModToggle._accent.g, luaModToggle._accent.b, 1.0)
                    scale: luaModToggle.checked ? 0 : 1
                    opacity: luaModToggle.checked ? 0 : 1
                    Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: luaModToggle.toggled(!luaModToggle.checked)
        }
    }

    component RuntimeConfigToggle : Item {
        id: configToggle
        property string label: ""
        property bool checked: false
        signal toggled(bool checked)

        Layout.fillWidth: true
        implicitHeight: 34

        readonly property real _circleDiameter: 18
        readonly property real _offset: (24 - _circleDiameter) / 2
        readonly property color _accent: window.primaryColor
        readonly property color _offBg: Qt.rgba(_accent.r, _accent.g, _accent.b, modManager.theme === "Cozy" ? 0.35 : 0.25)

        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: 8
            color: toggleRowMouse.containsMouse
                ? (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03))
                : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }
        }

        RowLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: configToggle.label
                font.pixelSize: 13
                font.weight: Font.Medium
                color: window.textColor
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            Rectangle {
                id: slider
                Layout.preferredWidth: 46
                Layout.preferredHeight: 24
                radius: height / 2
                color: configToggle.checked ? configToggle._accent : configToggle._offBg

                Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutBack } }

                Rectangle {
                    width: configToggle._circleDiameter / 2
                    height: width / 2 - 1
                    radius: 1
                    color: "#ffffff"
                    anchors.verticalCenter: parent.verticalCenter
                    x: configToggle.checked
                       ? (parent.width - configToggle._offset - width - (width / 2))
                       : (configToggle._offset + (width / 2))

                    Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }
                }

                Rectangle {
                    id: circle
                    width: configToggle._circleDiameter
                    height: configToggle._circleDiameter
                    radius: width / 2
                    color: "#ffffff"
                    anchors.verticalCenter: parent.verticalCenter
                    x: configToggle.checked
                       ? (parent.width - configToggle._offset - width)
                       : configToggle._offset

                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: Qt.rgba(0, 0, 0, 0.32)
                        shadowHorizontalOffset: 1
                        shadowVerticalOffset: 1
                        shadowBlur: 0.45
                    }

                    Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }

                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        color: Qt.rgba(configToggle._accent.r, configToggle._accent.g, configToggle._accent.b, 1.0)
                        scale: configToggle.checked ? 0 : 1
                        opacity: configToggle.checked ? 0 : 1
                        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                        Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
                    }
                }
            }
        }

        MouseArea {
            id: toggleRowMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: configToggle.toggled(!configToggle.checked)
        }
    }

    Dialog {
        id: runtimeConfigDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(880, Math.max(360, runtimeTabDialogs.width - 48))

        height: Math.min(Math.max(480, window.height - 32),
                         Math.max(560, runtimeConfigContentColumn.implicitHeight + 44))

        property bool animateHeight: false
        Behavior on height {
            enabled: runtimeConfigDialog.animateHeight
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }
        padding: 0
        closePolicy: Popup.CloseOnEscape
        onOpened: animateHeight = true
        onClosed: animateHeight = false

        property bool dumpAllDeveloperCommands: false
        property bool unrealEngineConsole: false
        property bool inGameGUI: false
        property bool skipPatternScan: false
        property bool enableDiscoveryCache: false
        property bool verboseLog: false
        property bool autoDiscoverAESKey: false
        property bool enableLuaMods: false
        property string luaModsPath: ""
        property bool enableCppMods: false
        property string cppModsPath: ""
        property bool watchdogEnabled: false
        property string watchdogLogDirectory: ""
        property bool watchdogDumpRegisters: true
        property bool watchdogCatchFastFail: true
        property bool watchdogTerminationBreakpoint: true
        property bool watchdogAttachDebugger: false
        property bool watchdogWriteMiniDump: true
        property int watchdogMaxStackDepth: 64
        property int watchdogOtherThreadStackDepth: 8

        property string selectedProfileId: ""
        property bool sdkValidation: false
        property bool generateBuildSystem: false
        property bool sdkLibclangGeneration: false
        property string customFolderPath: ""
        property string libclangPath: ""
        property string configPath: ""

        property bool configWriteBlocked: false

        function boolValue(value) {
            return value === true || value === 1 || value === "1" || value === "true"
        }

        function delayText(value) {
            var n = parseInt(value)
            return isNaN(n) ? "0" : String(Math.max(0, n))
        }

        function intValue(value, fallback) {
            var n = parseInt(value)
            return isNaN(n) ? fallback : n
        }

        function openWithConfig() {
            animateHeight = false
            var config = modManager.loadAvaloniumConfig()
            dumpAllDeveloperCommands = boolValue(config.DumpAllDeveloperCommands)
            unrealEngineConsole = boolValue(config.UnrealEngineConsole)
            inGameGUI = boolValue(config.InGameGUI)
            skipPatternScan = boolValue(config.SkipPatternScan)
            enableDiscoveryCache = boolValue(config.EnableDiscoveryCache)
            verboseLog = boolValue(config.VerboseLog)
            autoDiscoverAESKey = boolValue(config.AutoDiscoverAESKey)
            enableLuaMods = boolValue(config.EnableLuaMods)
            luaModsPath = config.LuaModsPath || ""
            enableCppMods = boolValue(config.EnableCppMods)
            cppModsPath = config.CppModsPath || ""
            watchdogEnabled = boolValue(config.WatchdogEnabled)
            watchdogLogDirectory = config.WatchdogLogDirectory || ""
            watchdogDumpRegisters = boolValue(config.WatchdogDumpRegisters)
            watchdogCatchFastFail = boolValue(config.WatchdogCatchFastFail)
            watchdogTerminationBreakpoint = boolValue(config.WatchdogTerminationBreakpoint)
            watchdogAttachDebugger = boolValue(config.WatchdogAttachDebugger)
            watchdogWriteMiniDump = boolValue(config.WatchdogWriteMiniDump)
            watchdogMaxStackDepth = intValue(config.WatchdogMaxStackDepth, 64)
            watchdogOtherThreadStackDepth = intValue(config.WatchdogOtherThreadStackDepth, 8)
            watchdogMaxStackDepthField.text = String(Math.max(1, Math.min(256, watchdogMaxStackDepth)))
            watchdogOtherThreadStackDepthField.text = String(Math.max(0, Math.min(64, watchdogOtherThreadStackDepth)))

            var norm = function(p) {
                return (p || "").toString().replace(/\\/g, "/").replace(/\/+$/, "").toLowerCase()
            }
            var currentNormPath = norm(luaModsPath)
            var allIds = luaProfileManager.profileIds || []

            selectedProfileId = ""
            if (currentNormPath.length > 0) {
                for (var pi = 0; pi < allIds.length; pi++) {
                    var pFolder = modManager.luaProfileFolderPath(allIds[pi])
                    if (pFolder && norm(pFolder) === currentNormPath) {
                        selectedProfileId = allIds[pi]
                        break
                    }
                }
            }

            var activePresetIsShared = modManager.isPresetUsingSharedLuaMods(modManager.activePresetId)
            if (selectedProfileId === "" && modManager.activePresetId && modManager.activePresetId.length > 0 && !activePresetIsShared) {
                var isUnconfiguredIni = (config.error && config.error.length > 0)
                if (isUnconfiguredIni && currentNormPath.length === 0) {
                    var activePresetFolder = modManager.luaProfileFolderPath(modManager.activePresetId)
                    if (activePresetFolder && activePresetFolder.length > 0) {
                        selectedProfileId = modManager.activePresetId
                        luaModsPath = activePresetFolder
                    }
                }
            }

            sdkValidation = boolValue(config.SDKValidation)
            generateBuildSystem = boolValue(config.GenerateBuildSystem)
            sdkLibclangGeneration = boolValue(config.SDKLibclangGeneration)
            customFolderPath = config.SDKPath || ""
            libclangPath = config.LibclangPath || ""
            configPath = config.path || ""
            initDelaySecondsField.text = delayText(config.InitDelaySeconds)
            initDelayMinutesField.text = delayText(config.InitDelayMinutes)
            configWriteBlocked = modManager.avaloniumConfigWriteBlockReason().length > 0
            if (config.error && config.error.length > 0) {
                toastNotification.show(config.error)
            }
            luaProfilePicker.rebuildEntries()
            luaProfilePicker.currentIndex = luaProfilePicker.findProfileIndex(selectedProfileId)
            luaModsManagerView.reset()
            open()
        }

        function saveConfig() {
            var error = modManager.saveAvaloniumConfig({
                "DumpAllDeveloperCommands": dumpAllDeveloperCommands,
                "UnrealEngineConsole": unrealEngineConsole,
                "InGameGUI": inGameGUI,
                "SkipPatternScan": skipPatternScan,
                "EnableDiscoveryCache": enableDiscoveryCache,
                "VerboseLog": verboseLog,
                "AutoDiscoverAESKey": autoDiscoverAESKey,
                "EnableLuaMods": enableLuaMods,
                "LuaModsPath": luaModsPath,
                "EnableCppMods": enableCppMods,
                "CppModsPath": cppModsPath,
                "WatchdogEnabled": watchdogEnabled,
                "WatchdogLogDirectory": watchdogLogDirectory,
                "WatchdogDumpRegisters": watchdogDumpRegisters,
                "WatchdogCatchFastFail": watchdogCatchFastFail,
                "WatchdogTerminationBreakpoint": watchdogTerminationBreakpoint,
                "WatchdogAttachDebugger": watchdogAttachDebugger,
                "WatchdogWriteMiniDump": watchdogWriteMiniDump,
                "WatchdogMaxStackDepth": intValue(watchdogMaxStackDepthField.text, 64),
                "WatchdogOtherThreadStackDepth": intValue(watchdogOtherThreadStackDepthField.text, 8),
                "SDKValidation": sdkValidation,
                "GenerateBuildSystem": generateBuildSystem,
                "SDKLibclangGeneration": sdkLibclangGeneration,
                "SDKPath": customFolderPath,
                "LibclangPath": libclangPath,
                "InitDelaySeconds": parseInt(initDelaySecondsField.text) || 0,
                "InitDelayMinutes": parseInt(initDelayMinutesField.text) || 0
            })

            if (error && error.length > 0) {
                toastNotification.show(error, 6000)
                return
            }

            if (luaProfileManager) {
                luaProfileManager.activeProfileId = selectedProfileId
            }
            if (modManager.activePresetId && modManager.activePresetId.length > 0) {
                var isShared = (selectedProfileId === "" && luaModsPath === "")
                modManager.updatePresetLuaModsConfig(modManager.activePresetId, luaModsPath, isShared)
            }

            toastNotification.show("Runtime configuration saved")
            close()
        }

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy") return cozyDialogGradient
                if (modManager.theme === "Crimson") return crimsonDialogGradient
                if (modManager.theme === "Wavey") return waveyDialogGradient
                return null
            }

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.55)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 18
                shadowBlur: 0.9
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.65)
            Behavior on opacity { NumberAnimation { duration: 200 } }
        }

        enter: Transition {
            NumberAnimation { property: "scale"; from: 0.94; to: 1.0; duration: 180; easing.type: Easing.OutCubic }
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160 }
        }

        exit: Transition {
            NumberAnimation { property: "scale"; from: 1.0; to: 0.97; duration: 140; easing.type: Easing.InCubic }
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 140 }
        }

        contentItem: Item {
            implicitWidth: runtimeConfigDialog.width
            implicitHeight: runtimeConfigDialog.height

            Rectangle {
                id: luaModsManagerView
                anchors.fill: parent
                radius: 24
                color: "transparent"
                visible: opacity > 0.001
                opacity: 0
                z: 50
                clip: true

                property bool showingList: false
                property real listProgress: 0
                property var luaModsModel: []
                property bool luaModsLoading: false
                property string luaModsError: ""
                property string luaModsFolder: ""
                property int enabledCount: 0
                property bool modsReadOnly: false
                property string modsWriteWarning: ""

                property var pendingChanges: ({})

                function showList() {
                    if (showingList) return
                    showingList = true

                    refresh()
                    showListAnimation.restart()
                }

                function hideList() {
                    if (!showingList) return
                    showingList = false

                    pendingChanges = {}
                    refresh()
                    hideListAnimation.restart()
                }

                function reset() {
                    showListAnimation.stop()
                    hideListAnimation.stop()
                    showingList = false
                    listProgress = 0
                    opacity = 0.0
                    scale = 0.96
                    luaModsModel = []
                    luaModsError = ""
                    luaModsLoading = false
                    enabledCount = 0
                    modsReadOnly = false
                    modsWriteWarning = ""
                    pendingChanges = {}
                    if (typeof runtimeConfigContentColumn !== "undefined") {
                        runtimeConfigContentColumn.visible = true
                        runtimeConfigContentColumn.opacity = 1.0
                        runtimeConfigContentColumn.scale = 1.0
                    }
                }

                function refresh() {
                    luaModsLoading = true
                    luaModsError = ""

                    var activePath = runtimeConfigDialog.luaModsPath.length > 0
                        ? runtimeConfigDialog.luaModsPath : luaProfilePicker.sharedModsBase

                    modManager.registerLuaModsDisabled(activePath)
                    var res = modManager.listLuaMods(activePath)
                    if (res.length > 0 && res[0].error) {
                        luaModsError = res[0].error
                        luaModsModel = []
                    } else {
                        luaModsModel = res
                        if (res.length > 0) {
                            luaModsFolder = res[0].modsFolder || ""
                            modsReadOnly = !!res[0].readOnly
                            modsWriteWarning = modsReadOnly
                                ? "Changes can't be saved here. Restart Universal Manager as an administrator to manage these mods."
                                : ""
                        }
                        enabledCount = countEnabled()
                    }
                    luaModsLoading = false
                }

                function countEnabled() {
                    var n = 0
                    var mods = luaModsModel || []
                    for (var i = 0; i < mods.length; ++i) {
                        var name = mods[i].name

                        var isEnabled = (name in pendingChanges) ? pendingChanges[name] : !!mods[i].enabled
                        if (isEnabled) n++
                    }
                    return n
                }

                function toggleMod(name, enabled) {

                    var changes = pendingChanges
                    changes[name] = enabled
                    pendingChanges = changes
                    enabledCount = countEnabled()
                }

                function savePendingChanges() {
                    var changes = pendingChanges
                    var activePath = runtimeConfigDialog.luaModsPath.length > 0
                        ? runtimeConfigDialog.luaModsPath : luaProfilePicker.sharedModsBase
                    for (var name in changes) {
                        var error = modManager.setLuaModEnabled(name, changes[name], activePath)
                        if (error && error.length > 0) {
                            toastNotification.show(error)
                            return false
                        }
                    }
                    pendingChanges = {}
                    return true
                }

                SequentialAnimation {
                    id: showListAnimation

                    ParallelAnimation {
                        NumberAnimation { target: runtimeConfigContentColumn; property: "opacity"; to: 0.0; duration: 170; easing.type: Easing.InOutQuad }
                        NumberAnimation { target: runtimeConfigContentColumn; property: "scale"; to: 0.96; duration: 170; easing.type: Easing.InOutQuad }
                    }

                    ScriptAction {
                        script: {
                            runtimeConfigContentColumn.visible = false
                            luaModsManagerView.opacity = 0.0
                            luaModsManagerView.scale = 0.94
                            luaModsManagerView.listProgress = 1.0
                        }
                    }

                    ParallelAnimation {
                        NumberAnimation { target: luaModsManagerView; property: "opacity"; to: 1.0; duration: 240; easing.type: Easing.OutCubic }
                        NumberAnimation { target: luaModsManagerView; property: "scale"; to: 1.0; duration: 300; easing.type: Easing.OutBack }
                    }
                }

                SequentialAnimation {
                    id: hideListAnimation

                    ParallelAnimation {
                        NumberAnimation { target: luaModsManagerView; property: "opacity"; to: 0.0; duration: 170; easing.type: Easing.InOutQuad }
                        NumberAnimation { target: luaModsManagerView; property: "scale"; to: 0.94; duration: 170; easing.type: Easing.InOutQuad }
                    }

                    ScriptAction {
                        script: {
                            luaModsManagerView.listProgress = 0.0
                            runtimeConfigContentColumn.visible = true
                            runtimeConfigContentColumn.opacity = 0.0
                            runtimeConfigContentColumn.scale = 0.96
                        }
                    }

                    ParallelAnimation {
                        NumberAnimation { target: runtimeConfigContentColumn; property: "opacity"; to: 1.0; duration: 240; easing.type: Easing.OutCubic }
                        NumberAnimation { target: runtimeConfigContentColumn; property: "scale"; to: 1.0; duration: 300; easing.type: Easing.OutBack }
                    }
                }

                ColumnLayout {
                    id: luaModsListContent
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 12
                    opacity: luaModsManagerView.listProgress
                    scale: 0.96 + 0.04 * luaModsManagerView.listProgress

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            id: luaModsBackButton
                            Layout.preferredWidth: luaModsBackRow.implicitWidth + 24
                            Layout.preferredHeight: 34
                            radius: 17
                            color: luaModsBackMouse.containsMouse
                                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                                : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.04))
                            border.width: 1
                            border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.35)

                            Behavior on color { ColorAnimation { duration: 150 } }

                            Row {
                                id: luaModsBackRow
                                anchors.centerIn: parent
                                spacing: 6

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "\u25C0"
                                    font.pixelSize: 11
                                    color: window.primaryColor
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "Settings"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: window.primaryColor
                                }
                            }

                            MouseArea {
                                id: luaModsBackMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: luaModsManagerView.hideList()
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: "Lua Mods"
                                font.pixelSize: 20
                                font.weight: Font.Bold
                                color: window.textColor
                            }

                            Text {
                                Layout.fillWidth: true
                                text: luaModsManagerView.luaModsFolder || "Mods loaded from the configured Lua mods folder"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                color: window.textColor
                                opacity: 0.8
                                elide: Text.ElideMiddle
                            }
                        }

                        Rectangle {
                            radius: 10
                            Layout.rightMargin: 34
                            color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)
                            border.width: 1
                            border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.3)
                            implicitWidth: luaModsCountText.implicitWidth + 18
                            implicitHeight: 26

                            Text {
                                id: luaModsCountText
                                anchors.centerIn: parent
                                text: luaModsManagerView.enabledCount + "/" + (luaModsManagerView.luaModsModel || []).length + " enabled"
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                color: window.primaryColor
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06)
                    }

                    Rectangle {
                        id: luaModsWriteWarningBanner
                        Layout.fillWidth: true
                        Layout.preferredHeight: luaModsWriteWarningText.implicitHeight + 20
                        radius: 10
                        visible: luaModsManagerView.modsReadOnly && luaModsManagerView.modsWriteWarning.length > 0
                        color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.16)
                        border.width: 1
                        border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.5)

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            Text {
                                id: luaModsWriteWarningText
                                Layout.fillWidth: true
                                text: "Your Lua mods folder is protected by Windows. Changes can't be saved.\nRestart Universal Manager as an administrator to manage these mods."
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: modManager.theme === "Glassmorphic" ? "#FFFFFF" : window.textColor
                                wrapMode: Text.WordWrap
                            }

                            Button {
                                id: luaModsRestartAdminButton
                                Layout.preferredHeight: 30
                                Layout.preferredWidth: restartAdminLabel.implicitWidth + 28
                                flat: true
                                hoverEnabled: true

                                contentItem: Text {
                                    id: restartAdminLabel
                                    text: "Restart as Admin"
                                    font.pixelSize: 11
                                    font.weight: Font.Bold
                                    color: luaModsRestartAdminButton.hovered ? "#FFFFFF" : window.textColor
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter

                                    Behavior on color { ColorAnimation { duration: 200 } }
                                }

                                background: Rectangle {
                                    color: luaModsRestartAdminButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                                    radius: 15
                                    border.width: 1.5
                                    border.color: luaModsRestartAdminButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                                    Behavior on color { ColorAnimation { duration: 200 } }
                                    Behavior on border.color { ColorAnimation { duration: 200 } }
                                }

                                onClicked: {
                                    if (modManager.restartAsAdministrator()) {
                                        toastNotification.show("Restarting as administrator...")
                                        runtimeConfigDialog.close()
                                    } else {
                                        toastNotification.show("Failed to restart as administrator")
                                    }
                                }
                            }
                        }
                    }

                    ScrollView {
                        id: luaModsScroll
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        Column {
                            id: luaModsRowsColumn
                            width: luaModsScroll.availableWidth
                            spacing: 8

                            Repeater {
                                id: luaModsRepeater
                                model: luaModsManagerView.luaModsModel

                                delegate: Rectangle {
                                    id: luaModRow
                                    required property var modelData
                                    required property int index
                                    width: luaModsRowsColumn.width
                                    height: 54
                                    radius: 12

                                    property bool rowEnabled: !!modelData.enabled

                                    color: luaModRowMouse.containsMouse
                                        ? (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.04))
                                        : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.025))
                                    border.width: 1
                                    border.color: rowEnabled
                                        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.35)
                                        : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.07))

                                    Behavior on color { ColorAnimation { duration: 140 } }
                                    Behavior on border.color { ColorAnimation { duration: 140 } }

                                    opacity: 0
                                    transform: Translate { id: luaModRowSlide; x: -28 }

                                    Component.onCompleted: {
                                        entranceAnim.restart()
                                    }

                                    SequentialAnimation {
                                        id: entranceAnim
                                        PauseAnimation { duration: 180 + luaModRow.index * 55 }
                                        ParallelAnimation {
                                            NumberAnimation {
                                                target: luaModRow
                                                property: "opacity"
                                                from: 0; to: 1
                                                duration: 320
                                                easing.type: Easing.OutCubic
                                            }
                                            NumberAnimation {
                                                target: luaModRowSlide
                                                property: "x"
                                                from: -28
                                                to: 0
                                                duration: 380
                                                easing.type: Easing.OutBack
                                            }
                                        }
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 14
                                        anchors.rightMargin: 12
                                        spacing: 10

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            Text {
                                                Layout.fillWidth: true
                                                text: luaModRow.modelData.name || ""
                                                font.pixelSize: 13
                                                font.weight: Font.DemiBold
                                                color: luaModRow.rowEnabled ? window.textColor : window.textSecondaryColor
                                                elide: Text.ElideRight
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: !luaModRow.modelData.hasEntry ? "No main.lua found \u2014 will be skipped" : (luaModRow.rowEnabled ? "Enabled \u2014 loads with Avalonium injection " : "Disabled \u2014 skipped by the mod loader")
                                                font.pixelSize: 10
                                                color: !luaModRow.modelData.hasEntry ? Qt.rgba(0.85, 0.45, 0.2, 1.0) : window.textSecondaryColor
                                                elide: Text.ElideRight
                                            }
                                        }

                                        LuaModToggle {
                                            z: 10
                                            opacity: luaModsManagerView.modsReadOnly ? 0.45 : 1.0
                                            enabled: !luaModsManagerView.modsReadOnly
                                            checked: luaModRow.rowEnabled
                                            onToggled: (checkedValue) => {
                                                luaModRow.rowEnabled = checkedValue
                                                luaModsManagerView.toggleMod(luaModRow.modelData.name, checkedValue)
                                            }
                                        }
                                    }

                                    MouseArea {
                                        id: luaModRowMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: luaModsManagerView.modsReadOnly ? Qt.ArrowCursor : Qt.PointingHandCursor
                                        onClicked: {
                                            if (luaModsManagerView.modsReadOnly) {
                                                toastNotification.show(luaModsManagerView.modsWriteWarning, 5000)
                                                return
                                            }
                                            var newVal = !luaModRow.rowEnabled
                                            luaModRow.rowEnabled = newVal
                                            luaModsManagerView.toggleMod(luaModRow.modelData.name, newVal)
                                        }
                                    }
                                }
                            }

                            Text {                                    width: luaModsRowsColumn.width
                                    horizontalAlignment: Text.AlignHCenter
                                    visible: luaModsManagerView.luaModsError.length > 0
                                text: luaModsManagerView.luaModsError
                                font.pixelSize: 13
                                color: window.textSecondaryColor
                                wrapMode: Text.WordWrap
                            }

                            Text {                                    width: luaModsRowsColumn.width
                                    horizontalAlignment: Text.AlignHCenter
                                    visible: !luaModsManagerView.luaModsLoading && luaModsManagerView.luaModsError.length === 0 && (luaModsManagerView.luaModsModel || []).length === 0
                                text: "No Lua mods found in the mods folder.\nDrop folders containing a main.lua (or loose .lua files) into the folder and they will appear here."
                                font.pixelSize: 13
                                color: window.textSecondaryColor
                                wrapMode: Text.WordWrap
                                lineHeight: 1.35
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: luaModsManagerView.modsReadOnly
                              ? "Toggles are disabled while the mods folder is read-only."
                              : "Changes are written to mods.json when you click Save."
                        font.pixelSize: 10
                        color: window.textSecondaryColor
                        opacity: 0.8
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Item { Layout.fillWidth: true }

                        Button {
                            id: luaModsSaveButton
                            Layout.preferredHeight: 40
                            Layout.preferredWidth: luaModsSaveLabel.implicitWidth + 40
                            flat: true
                            hoverEnabled: true

                            contentItem: Text {
                                id: luaModsSaveLabel
                                text: "Save"
                                font.pixelSize: 13
                                font.weight: Font.Bold
                                color: luaModsSaveButton.hovered ? "#FFFFFF" : window.textColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                Behavior on color { ColorAnimation { duration: 200 } }
                            }

                            background: Rectangle {
                                color: luaModsSaveButton.hovered
                                    ? window.primaryColor
                                    : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                                radius: 20
                                border.width: 1.5
                                border.color: luaModsSaveButton.hovered
                                    ? window.primaryColor
                                    : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)
                                Behavior on color { ColorAnimation { duration: 200 } }
                                Behavior on border.color { ColorAnimation { duration: 200 } }
                            }

                            onClicked: {
                                if (luaModsManagerView.savePendingChanges()) {
                                    toastNotification.show("Lua mods saved")
                                    luaModsManagerView.hideList()
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: runtimeConfigCloseBtn
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 16
                anchors.rightMargin: 16
                width: 32
                height: 32
                radius: 16
                color: runtimeConfigCloseMouse.containsMouse
                    ? (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.08))
                    : "transparent"
                z: 100

                Behavior on color { ColorAnimation { duration: 150 } }

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    font.pixelSize: 13
                    font.weight: Font.Bold
                    color: window.textColor
                    opacity: runtimeConfigCloseMouse.containsMouse ? 1.0 : 0.6
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }

                MouseArea {
                    id: runtimeConfigCloseMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: runtimeConfigDialog.close()
                }
            }

            ColumnLayout {
                id: runtimeConfigContentColumn

                anchors.fill: parent
                anchors.margins: 22
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: "Configure Runtime"
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        color: window.textColor
                    }

                    Text {
                        text: "Runtime features, Lua & C++ modding, SDK output & startup timing"
                        font.pixelSize: 12
                        color: window.textSecondaryColor
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06)
                }

                Rectangle {
                    id: runtimeConfigWriteBanner
                    Layout.fillWidth: true
                    Layout.preferredHeight: runtimeConfigWriteBannerText.implicitHeight + 20
                    radius: 10
                    visible: runtimeConfigDialog.configWriteBlocked && !modManager.isRunningAsAdmin
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.16)
                    border.width: 1
                    border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.5)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        Text {
                            id: runtimeConfigWriteBannerText
                            Layout.fillWidth: true
                            text: "Avalonium.ini is protected by Windows. Changes can't be saved.\nRestart Universal Manager as an administrator to save these settings."
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            color: modManager.theme === "Glassmorphic" ? "#FFFFFF" : window.textColor
                            wrapMode: Text.WordWrap
                        }

                        Button {
                            id: runtimeConfigRestartAdminButton
                            Layout.preferredHeight: 30
                            Layout.preferredWidth: runtimeConfigRestartAdminLabel.implicitWidth + 28
                            flat: true
                            hoverEnabled: true

                            contentItem: Text {
                                id: runtimeConfigRestartAdminLabel
                                text: "Restart as Admin"
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                color: runtimeConfigRestartAdminButton.hovered ? "#FFFFFF" : window.textColor
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter

                                Behavior on color { ColorAnimation { duration: 200 } }
                            }

                            background: Rectangle {
                                color: runtimeConfigRestartAdminButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                                radius: 15
                                border.width: 1.5
                                border.color: runtimeConfigRestartAdminButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                                Behavior on color { ColorAnimation { duration: 200 } }
                                Behavior on border.color { ColorAnimation { duration: 200 } }
                            }

                            onClicked: {
                                if (modManager.restartAsAdministrator()) {
                                    toastNotification.show("Restarting as administrator...")
                                    runtimeConfigDialog.close()
                                } else {
                                    toastNotification.show("Failed to restart as administrator")
                                }
                            }
                        }
                    }
                }

                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0
                    implicitHeight: contentColumnsGrid.implicitHeight
                    contentWidth: width
                    contentHeight: contentColumnsGrid.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    GridLayout {
                        id: contentColumnsGrid
                        width: parent.width
                        columns: runtimeConfigDialog.width >= 620 ? 2 : 1
                        columnSpacing: 14
                        rowSpacing: 14

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            Layout.alignment: Qt.AlignTop
                            spacing: 14

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: runtimeFeaturesColumn.implicitHeight + 26
                                radius: 16
                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                                border.width: 1.5
                                border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)

                                ColumnLayout {
                                    id: runtimeFeaturesColumn
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 6

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: "Runtime Features"
                                            font.pixelSize: 14
                                            font.weight: Font.Bold
                                            color: window.textColor
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "Injection & in-game tools"
                                            font.pixelSize: 11
                                            color: window.textSecondaryColor
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Item { Layout.preferredHeight: 2 }

                                    RuntimeConfigToggle {
                                        label: "In-Game GUI"
                                        checked: runtimeConfigDialog.inGameGUI
                                        onToggled: (value) => runtimeConfigDialog.inGameGUI = value
                                    }

                                    RuntimeConfigToggle {
                                        label: "Unlock Unreal Engine Console"
                                        checked: runtimeConfigDialog.unrealEngineConsole
                                        onToggled: (value) => runtimeConfigDialog.unrealEngineConsole = value
                                    }

                                    RuntimeConfigToggle {
                                        label: "Skip Pattern Scan"
                                        checked: runtimeConfigDialog.skipPatternScan
                                        onToggled: (value) => runtimeConfigDialog.skipPatternScan = value
                                    }

                                    RuntimeConfigToggle {
                                        label: "Enable Encrypted Cache"
                                        checked: runtimeConfigDialog.enableDiscoveryCache
                                        onToggled: (value) => runtimeConfigDialog.enableDiscoveryCache = value
                                    }

                                    RuntimeConfigToggle {
                                        label: "Verbose Log"
                                        checked: runtimeConfigDialog.verboseLog
                                        onToggled: (value) => runtimeConfigDialog.verboseLog = value
                                    }

                                    RuntimeConfigToggle {
                                        label: "Dump Developer Commands"
                                        checked: runtimeConfigDialog.dumpAllDeveloperCommands
                                        onToggled: (value) => runtimeConfigDialog.dumpAllDeveloperCommands = value
                                    }

                                    RuntimeConfigToggle {
                                        label: "Auto-discover AES Key"
                                        checked: runtimeConfigDialog.autoDiscoverAESKey
                                        onToggled: (value) => runtimeConfigDialog.autoDiscoverAESKey = value
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: startupColumn.implicitHeight + 26
                                radius: 16
                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                                border.width: 1.5
                                border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)

                                ColumnLayout {
                                    id: startupColumn
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 8

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: "Startup Delay"
                                            font.pixelSize: 14
                                            font.weight: Font.Bold
                                            color: window.textColor
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "Wait before hooking process"
                                            font.pixelSize: 11
                                            color: window.textSecondaryColor
                                            elide: Text.ElideRight
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            Text {
                                                text: "Seconds"
                                                font.pixelSize: 11
                                                font.weight: Font.Medium
                                                color: window.textSecondaryColor
                                            }

                                            Rectangle {
                                                id: secondsFieldContainer
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 36
                                                color: "transparent"

                                                TextField {
                                                    id: initDelaySecondsField
                                                    ContextMenu.menu: null
                                                    anchors.fill: parent
                                                    text: "0"
                                                    selectByMouse: true
                                                    persistentSelection: true
                                                    validator: IntValidator { bottom: 0 }
                                                    inputMethodHints: Qt.ImhDigitsOnly
                                                    horizontalAlignment: TextInput.AlignHCenter
                                                    verticalAlignment: TextInput.AlignVCenter
                                                    color: window.textColor
                                                    font.pixelSize: 13

                                                    background: Rectangle {
                                                        radius: 10
                                                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.16) : Qt.rgba(255, 255, 255, 0.45)
                                                        border.width: initDelaySecondsField.activeFocus ? 2 : 1
                                                        border.color: initDelaySecondsField.activeFocus ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.28)
                                                        Behavior on border.color { ColorAnimation { duration: 150 } }
                                                    }
                                                }

                                                MouseArea {
                                                    z: 1
                                                    anchors.fill: parent
                                                    acceptedButtons: Qt.RightButton
                                                    cursorShape: Qt.IBeamCursor
                                                    onPressed: function(mouse) {
                                                        initDelaySecondsField.forceActiveFocus()
                                                        secondsContextMenu.popup(secondsFieldContainer, mouse.x, secondsFieldContainer.height + 4)
                                                        mouse.accepted = true
                                                    }
                                                    onPressAndHold: function(mouse) {
                                                        initDelaySecondsField.forceActiveFocus()
                                                        secondsContextMenu.popup(secondsFieldContainer, mouse.x, secondsFieldContainer.height + 4)
                                                        mouse.accepted = true
                                                    }
                                                }

                                                Menu {
                                                    id: secondsContextMenu
                                                    width: 180
                                                    padding: 6

                                                    background: Rectangle {
                                                        color: {
                                                            if (modManager.theme === "Crimson") return Qt.rgba(0.05, 0.06, 0.10, 0.98)
                                                            if (modManager.theme === "Glassmorphic") return Qt.rgba(0.05, 0.05, 0.07, Math.max(0.88, Math.min(0.97, modManager.glassOpacity + 0.84)))
                                                            if (modManager.theme === "Wavey") return Qt.rgba(0.08, 0.16, 0.28, 0.97)
                                                            return window.surfaceColor
                                                        }
                                                        radius: 12
                                                        border.width: 1
                                                        border.color: modManager.theme === "Glassmorphic"
                                                            ? Qt.rgba(1, 1, 1, 0.10)
                                                            : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.08) : window.borderColor)

                                                        layer.enabled: true
                                                        layer.effect: MultiEffect {
                                                            shadowEnabled: true
                                                            shadowColor: Qt.rgba(0, 0, 0, 0.40)
                                                            shadowHorizontalOffset: 0
                                                            shadowVerticalOffset: 8
                                                            shadowBlur: 0.75
                                                        }
                                                    }

                                                    RuntimeContextMenuItem {
                                                        text: "Undo"
                                                        iconName: "undo-2.svg"
                                                        enabled: initDelaySecondsField.canUndo
                                                        onTriggered: initDelaySecondsField.undo()
                                                    }
                                                    RuntimeContextMenuItem {
                                                        text: "Redo"
                                                        iconName: "redo-2.svg"
                                                        enabled: initDelaySecondsField.canRedo
                                                        onTriggered: initDelaySecondsField.redo()
                                                    }
                                                    MenuSeparator {}
                                                    RuntimeContextMenuItem {
                                                        text: "Cut"
                                                        iconName: "scissors.svg"
                                                        enabled: initDelaySecondsField.selectedText.length > 0
                                                        onTriggered: initDelaySecondsField.cut()
                                                    }
                                                    RuntimeContextMenuItem {
                                                        text: "Copy"
                                                        iconName: "copy.svg"
                                                        enabled: initDelaySecondsField.selectedText.length > 0
                                                        onTriggered: initDelaySecondsField.copy()
                                                    }
                                                    RuntimeContextMenuItem {
                                                        text: "Paste"
                                                        iconName: "clipboard.svg"
                                                        onTriggered: initDelaySecondsField.paste()
                                                    }
                                                    RuntimeContextMenuItem {
                                                        text: "Delete"
                                                        iconName: "trash.svg"
                                                        enabled: initDelaySecondsField.selectedText.length > 0
                                                        onTriggered: initDelaySecondsField.remove(initDelaySecondsField.selectionStart, initDelaySecondsField.selectionEnd)
                                                    }
                                                    MenuSeparator {}
                                                    RuntimeContextMenuItem {
                                                        text: "Select All"
                                                        iconName: "text-select.svg"
                                                        enabled: initDelaySecondsField.text.length > 0
                                                        onTriggered: initDelaySecondsField.selectAll()
                                                    }
                                                }
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            Text {
                                                text: "Minutes"
                                                font.pixelSize: 11
                                                font.weight: Font.Medium
                                                color: window.textSecondaryColor
                                            }

                                            Rectangle {
                                                id: minutesFieldContainer
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 36
                                                color: "transparent"

                                                TextField {
                                                    id: initDelayMinutesField
                                                    ContextMenu.menu: null
                                                    anchors.fill: parent
                                                    text: "0"
                                                    selectByMouse: true
                                                    persistentSelection: true
                                                    validator: IntValidator { bottom: 0 }
                                                    inputMethodHints: Qt.ImhDigitsOnly
                                                    horizontalAlignment: TextInput.AlignHCenter
                                                    verticalAlignment: TextInput.AlignVCenter
                                                    color: window.textColor
                                                    font.pixelSize: 13

                                                    background: Rectangle {
                                                        radius: 10
                                                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.16) : Qt.rgba(255, 255, 255, 0.45)
                                                        border.width: initDelayMinutesField.activeFocus ? 2 : 1
                                                        border.color: initDelayMinutesField.activeFocus ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.28)
                                                        Behavior on border.color { ColorAnimation { duration: 150 } }
                                                    }
                                                }

                                                MouseArea {
                                                    z: 1
                                                    anchors.fill: parent
                                                    acceptedButtons: Qt.RightButton
                                                    cursorShape: Qt.IBeamCursor
                                                    onPressed: function(mouse) {
                                                        initDelayMinutesField.forceActiveFocus()
                                                        minutesContextMenu.popup(minutesFieldContainer, mouse.x, minutesFieldContainer.height + 4)
                                                        mouse.accepted = true
                                                    }
                                                    onPressAndHold: function(mouse) {
                                                        initDelayMinutesField.forceActiveFocus()
                                                        minutesContextMenu.popup(minutesFieldContainer, mouse.x, minutesFieldContainer.height + 4)
                                                        mouse.accepted = true
                                                    }
                                                }

                                                Menu {
                                                    id: minutesContextMenu
                                                    width: 180
                                                    padding: 6

                                                    background: Rectangle {
                                                        color: {
                                                            if (modManager.theme === "Crimson") return Qt.rgba(0.05, 0.06, 0.10, 0.98)
                                                            if (modManager.theme === "Glassmorphic") return Qt.rgba(0.05, 0.05, 0.07, Math.max(0.88, Math.min(0.97, modManager.glassOpacity + 0.84)))
                                                            if (modManager.theme === "Wavey") return Qt.rgba(0.08, 0.16, 0.28, 0.97)
                                                            return window.surfaceColor
                                                        }
                                                        radius: 12
                                                        border.width: 1
                                                        border.color: modManager.theme === "Glassmorphic"
                                                            ? Qt.rgba(1, 1, 1, 0.10)
                                                            : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.08) : window.borderColor)

                                                        layer.enabled: true
                                                        layer.effect: MultiEffect {
                                                            shadowEnabled: true
                                                            shadowColor: Qt.rgba(0, 0, 0, 0.40)
                                                            shadowHorizontalOffset: 0
                                                            shadowVerticalOffset: 8
                                                            shadowBlur: 0.75
                                                        }
                                                    }

                                                    RuntimeContextMenuItem {
                                                        text: "Undo"
                                                        iconName: "undo-2.svg"
                                                        enabled: initDelayMinutesField.canUndo
                                                        onTriggered: initDelayMinutesField.undo()
                                                    }
                                                    RuntimeContextMenuItem {
                                                        text: "Redo"
                                                        iconName: "redo-2.svg"
                                                        enabled: initDelayMinutesField.canRedo
                                                        onTriggered: initDelayMinutesField.redo()
                                                    }
                                                    MenuSeparator {}
                                                    RuntimeContextMenuItem {
                                                        text: "Cut"
                                                        iconName: "scissors.svg"
                                                        enabled: initDelayMinutesField.selectedText.length > 0
                                                        onTriggered: initDelayMinutesField.cut()
                                                    }
                                                    RuntimeContextMenuItem {
                                                        text: "Copy"
                                                        iconName: "copy.svg"
                                                        enabled: initDelayMinutesField.selectedText.length > 0
                                                        onTriggered: initDelayMinutesField.copy()
                                                    }
                                                    RuntimeContextMenuItem {
                                                        text: "Paste"
                                                        iconName: "clipboard.svg"
                                                        onTriggered: initDelayMinutesField.paste()
                                                    }
                                                    RuntimeContextMenuItem {
                                                        text: "Delete"
                                                        iconName: "trash.svg"
                                                        enabled: initDelayMinutesField.selectedText.length > 0
                                                        onTriggered: initDelayMinutesField.remove(initDelayMinutesField.selectionStart, initDelayMinutesField.selectionEnd)
                                                    }
                                                    MenuSeparator {}
                                                    RuntimeContextMenuItem {
                                                        text: "Select All"
                                                        iconName: "text-select.svg"
                                                        enabled: initDelayMinutesField.text.length > 0
                                                        onTriggered: initDelayMinutesField.selectAll()
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            Layout.alignment: Qt.AlignTop
                            spacing: 14

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: luaModsColumn.implicitHeight + 26
                                radius: 16
                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                                border.width: 1.5
                                border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)

                                ColumnLayout {
                                    id: luaModsColumn
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 6

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: "Lua Mods"
                                            font.pixelSize: 14
                                            font.weight: Font.Bold
                                            color: window.textColor
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "Scripting & mod loader"
                                            font.pixelSize: 11
                                            color: window.textSecondaryColor
                                            elide: Text.ElideRight
                                        }

                                        Rectangle {
                                            id: luaModsAdvancedButton
                                            visible: runtimeConfigDialog.enableLuaMods
                                            implicitWidth: advancedPillRow.implicitWidth + 16
                                            implicitHeight: 26
                                            radius: 13
                                            color: luaModsAdvancedMouse.containsMouse
                                                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                                                : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08))
                                            border.width: 1
                                            border.color: luaModsAdvancedMouse.containsMouse
                                                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.6)
                                                : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.28)
                                            Behavior on color { ColorAnimation { duration: 150 } }
                                            Behavior on border.color { ColorAnimation { duration: 150 } }

                                            RowLayout {
                                                id: advancedPillRow
                                                anchors.centerIn: parent
                                                spacing: 4

                                                Text {
                                                    text: "Manage Lua Mods"
                                                    font.pixelSize: 11
                                                    font.weight: Font.DemiBold
                                                    color: luaModsAdvancedMouse.containsMouse ? window.primaryColor : window.textSecondaryColor
                                                    Behavior on color { ColorAnimation { duration: 150 } }
                                                }
                                                Text {
                                                    text: "\u2192"
                                                    font.pixelSize: 11
                                                    color: luaModsAdvancedMouse.containsMouse ? window.primaryColor : window.textSecondaryColor
                                                    Behavior on color { ColorAnimation { duration: 150 } }
                                                }
                                            }

                                            MouseArea {
                                                id: luaModsAdvancedMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (luaModsManagerView.showingList) {
                                                        luaModsManagerView.hideList()
                                                    } else {
                                                        luaModsManagerView.showList()
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Item { Layout.preferredHeight: 2 }

                                    RuntimeConfigToggle {
                                        label: "Enable Lua Mods"
                                        checked: runtimeConfigDialog.enableLuaMods
                                        onToggled: (value) => runtimeConfigDialog.enableLuaMods = value
                                    }

                                    LuaProfilePicker {
                                        id: luaProfilePicker
                                        visible: runtimeConfigDialog.enableLuaMods
                                        height: 46
                                    }

                                    Rectangle {
                                        id: luaModsPathRow
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 46
                                        visible: runtimeConfigDialog.enableLuaMods
                                        radius: 10
                                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(0, 0, 0, 0.02)
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 6
                                            spacing: 8

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 32
                                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.16) : Qt.rgba(255, 255, 255, 0.45)
                                                radius: 8
                                                border.width: 1
                                                border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.28)

                                                Text {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 10
                                                    anchors.rightMargin: 10
                                                    text: runtimeConfigDialog.luaModsPath || "Default 'Mods' folder next to Avalonium.dll"
                                                    font.pixelSize: 11
                                                    color: runtimeConfigDialog.luaModsPath ? window.textColor : window.textSecondaryColor
                                                    verticalAlignment: Text.AlignVCenter
                                                    elide: Text.ElideMiddle
                                                }
                                            }

                                            Button {
                                                id: browseLuaModsButton
                                                text: "Browse"
                                                Layout.preferredWidth: 70
                                                Layout.preferredHeight: 32
                                                flat: true
                                                hoverEnabled: true

                                                contentItem: Text {
                                                    text: parent.text
                                                    font.pixelSize: 12
                                                    font.weight: Font.DemiBold
                                                    color: browseLuaModsButton.hovered ? "#FFFFFF" : window.textColor
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }

                                                background: Rectangle {
                                                    color: browseLuaModsButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                                                    radius: 8
                                                    border.width: 1
                                                    border.color: browseLuaModsButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                                                    Behavior on color { ColorAnimation { duration: 150 } }
                                                    Behavior on border.color { ColorAnimation { duration: 150 } }
                                                }

                                                onClicked: luaModsPathDialog.open()
                                            }
                                        }
                                    }

                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: cppModsColumn.implicitHeight + 26
                                radius: 16
                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                                border.width: 1.5
                                border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)

                                ColumnLayout {
                                    id: cppModsColumn
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 6

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: "C++ Mods"
                                            font.pixelSize: 14
                                            font.weight: Font.Bold
                                            color: window.textColor
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "Native mods"
                                            font.pixelSize: 11
                                            color: window.textSecondaryColor
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Item { Layout.preferredHeight: 2 }

                                    RuntimeConfigToggle {
                                        label: "Enable C++ Mods"
                                        checked: runtimeConfigDialog.enableCppMods
                                        onToggled: (value) => runtimeConfigDialog.enableCppMods = value
                                    }

                                    Rectangle {
                                        id: cppModsPathRow
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 46
                                        visible: runtimeConfigDialog.enableCppMods
                                        radius: 10
                                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(0, 0, 0, 0.02)
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 6
                                            spacing: 8

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 32
                                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.16) : Qt.rgba(255, 255, 255, 0.45)
                                                radius: 8
                                                border.width: 1
                                                border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.28)

                                                Text {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 10
                                                    anchors.rightMargin: 10
                                                    text: runtimeConfigDialog.cppModsPath || "Default 'Mods' folder next to Avalonium.dll"
                                                    font.pixelSize: 11
                                                    color: runtimeConfigDialog.cppModsPath ? window.textColor : window.textSecondaryColor
                                                    verticalAlignment: Text.AlignVCenter
                                                    elide: Text.ElideMiddle
                                                }
                                            }

                                            Button {
                                                id: browseCppModsButton
                                                text: "Browse"
                                                Layout.preferredWidth: 70
                                                Layout.preferredHeight: 32
                                                flat: true
                                                hoverEnabled: true

                                                contentItem: Text {
                                                    text: parent.text
                                                    font.pixelSize: 12
                                                    font.weight: Font.DemiBold
                                                    color: browseCppModsButton.hovered ? "#FFFFFF" : window.textColor
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }

                                                background: Rectangle {
                                                    color: browseCppModsButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                                                    radius: 8
                                                    border.width: 1
                                                    border.color: browseCppModsButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                                                    Behavior on color { ColorAnimation { duration: 150 } }
                                                    Behavior on border.color { ColorAnimation { duration: 150 } }
                                                }

                                                onClicked: cppModsPathDialog.open()
                                            }
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: watchdogColumn.implicitHeight + 26
                                radius: 16
                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                                border.width: 1.5
                                border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)

                                ColumnLayout {
                                    id: watchdogColumn
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 6

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: "Crash Watchdog"
                                            font.pixelSize: 14
                                            font.weight: Font.Bold
                                            color: window.textColor
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "AggressiveDog crash reports"
                                            font.pixelSize: 11
                                            color: window.textSecondaryColor
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Item { Layout.preferredHeight: 2 }

                                    RuntimeConfigToggle {
                                        label: "Enable Crash Watchdog"
                                        checked: runtimeConfigDialog.watchdogEnabled
                                        onToggled: (value) => runtimeConfigDialog.watchdogEnabled = value
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 46
                                        visible: runtimeConfigDialog.watchdogEnabled
                                        radius: 10
                                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(0, 0, 0, 0.02)
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 6
                                            spacing: 8

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 32
                                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.16) : Qt.rgba(255, 255, 255, 0.45)
                                                radius: 8
                                                border.width: 1
                                                border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.28)

                                                Text {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 10
                                                    anchors.rightMargin: 10
                                                    text: runtimeConfigDialog.watchdogLogDirectory || "CrashHandler/Crashes (next to Avalonium.dll)"
                                                    font.pixelSize: 11
                                                    color: runtimeConfigDialog.watchdogLogDirectory ? window.textColor : window.textSecondaryColor
                                                    verticalAlignment: Text.AlignVCenter
                                                    elide: Text.ElideMiddle
                                                }
                                            }

                                            Button {
                                                id: browseWatchdogLogButton
                                                text: "Browse"
                                                Layout.preferredWidth: 70
                                                Layout.preferredHeight: 32
                                                flat: true
                                                hoverEnabled: true

                                                contentItem: Text {
                                                    text: parent.text
                                                    font.pixelSize: 12
                                                    font.weight: Font.DemiBold
                                                    color: browseWatchdogLogButton.hovered ? "#FFFFFF" : window.textColor
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }

                                                background: Rectangle {
                                                    color: browseWatchdogLogButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                                                    radius: 8
                                                    border.width: 1
                                                    border.color: browseWatchdogLogButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                                                    Behavior on color { ColorAnimation { duration: 150 } }
                                                    Behavior on border.color { ColorAnimation { duration: 150 } }
                                                }

                                                onClicked: watchdogLogDirectoryDialog.open()
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12
                                        visible: runtimeConfigDialog.watchdogEnabled

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            RuntimeConfigToggle {
                                                label: "Dump Registers"
                                                checked: runtimeConfigDialog.watchdogDumpRegisters
                                                onToggled: (value) => runtimeConfigDialog.watchdogDumpRegisters = value
                                            }
                                            RuntimeConfigToggle {
                                                label: "Catch Fast-Fail"
                                                checked: runtimeConfigDialog.watchdogCatchFastFail
                                                onToggled: (value) => runtimeConfigDialog.watchdogCatchFastFail = value
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            RuntimeConfigToggle {
                                                label: "Write Mini-Dump"
                                                checked: runtimeConfigDialog.watchdogWriteMiniDump
                                                onToggled: (value) => runtimeConfigDialog.watchdogWriteMiniDump = value
                                            }
                                            RuntimeConfigToggle {
                                                label: "Attach Debugger"
                                                checked: runtimeConfigDialog.watchdogAttachDebugger
                                                onToggled: (value) => runtimeConfigDialog.watchdogAttachDebugger = value
                                            }
                                        }
                                    }

                                    RuntimeConfigToggle {
                                        Layout.fillWidth: true
                                        visible: runtimeConfigDialog.watchdogEnabled
                                        label: "Termination Breakpoint"
                                        checked: runtimeConfigDialog.watchdogTerminationBreakpoint
                                        onToggled: (value) => runtimeConfigDialog.watchdogTerminationBreakpoint = value
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12
                                        visible: runtimeConfigDialog.watchdogEnabled

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 100
                                            spacing: 4

                                            Text {
                                                text: "Max Stack Depth"
                                                font.pixelSize: 11
                                                font.weight: Font.Medium
                                                color: window.textSecondaryColor
                                            }

                                            Rectangle {
                                                id: watchdogMaxStackDepthContainer
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 36
                                                color: "transparent"

                                                TextField {
                                                    id: watchdogMaxStackDepthField
                                                    ContextMenu.menu: null
                                                    anchors.fill: parent
                                                    text: "64"
                                                    selectByMouse: true
                                                    persistentSelection: true
                                                    validator: IntValidator { bottom: 1; top: 256 }
                                                    inputMethodHints: Qt.ImhDigitsOnly
                                                    horizontalAlignment: TextInput.AlignHCenter
                                                    verticalAlignment: TextInput.AlignVCenter
                                                    color: window.textColor
                                                    font.pixelSize: 13
                                                    onTextEdited: runtimeConfigDialog.watchdogMaxStackDepth = intValue(text, 64)

                                                    background: Rectangle {
                                                        radius: 10
                                                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.16) : Qt.rgba(255, 255, 255, 0.45)
                                                        border.width: watchdogMaxStackDepthField.activeFocus ? 2 : 1
                                                        border.color: watchdogMaxStackDepthField.activeFocus ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.28)
                                                        Behavior on border.color { ColorAnimation { duration: 150 } }
                                                    }
                                                }

                                                MouseArea {
                                                    z: 1
                                                    anchors.fill: parent
                                                    acceptedButtons: Qt.RightButton
                                                    cursorShape: Qt.IBeamCursor
                                                    onPressed: function(mouse) {
                                                        watchdogMaxStackDepthField.forceActiveFocus()
                                                        watchdogDepthContextMenu.targetField = watchdogMaxStackDepthField
                                                        watchdogDepthContextMenu.popup(watchdogMaxStackDepthContainer, mouse.x, watchdogMaxStackDepthContainer.height + 4)
                                                        mouse.accepted = true
                                                    }
                                                    onPressAndHold: function(mouse) {
                                                        watchdogMaxStackDepthField.forceActiveFocus()
                                                        watchdogDepthContextMenu.targetField = watchdogMaxStackDepthField
                                                        watchdogDepthContextMenu.popup(watchdogMaxStackDepthContainer, mouse.x, watchdogMaxStackDepthContainer.height + 4)
                                                        mouse.accepted = true
                                                    }
                                                }
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 100
                                            spacing: 4

                                            Text {
                                                text: "Other Thread Stack Depth"
                                                font.pixelSize: 11
                                                font.weight: Font.Medium
                                                color: window.textSecondaryColor
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }

                                            Rectangle {
                                                id: watchdogOtherThreadStackDepthContainer
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 36
                                                color: "transparent"

                                                TextField {
                                                    id: watchdogOtherThreadStackDepthField
                                                    ContextMenu.menu: null
                                                    anchors.fill: parent
                                                    text: "8"
                                                    selectByMouse: true
                                                    persistentSelection: true
                                                    validator: IntValidator { bottom: 0; top: 64 }
                                                    inputMethodHints: Qt.ImhDigitsOnly
                                                    horizontalAlignment: TextInput.AlignHCenter
                                                    verticalAlignment: TextInput.AlignVCenter
                                                    color: window.textColor
                                                    font.pixelSize: 13
                                                    onTextEdited: runtimeConfigDialog.watchdogOtherThreadStackDepth = intValue(text, 8)

                                                    background: Rectangle {
                                                        radius: 10
                                                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.16) : Qt.rgba(255, 255, 255, 0.45)
                                                        border.width: watchdogOtherThreadStackDepthField.activeFocus ? 2 : 1
                                                        border.color: watchdogOtherThreadStackDepthField.activeFocus ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.28)
                                                        Behavior on border.color { ColorAnimation { duration: 150 } }
                                                    }
                                                }

                                                MouseArea {
                                                    z: 1
                                                    anchors.fill: parent
                                                    acceptedButtons: Qt.RightButton
                                                    cursorShape: Qt.IBeamCursor
                                                    onPressed: function(mouse) {
                                                        watchdogOtherThreadStackDepthField.forceActiveFocus()
                                                        watchdogDepthContextMenu.targetField = watchdogOtherThreadStackDepthField
                                                        watchdogDepthContextMenu.popup(watchdogOtherThreadStackDepthContainer, mouse.x, watchdogOtherThreadStackDepthContainer.height + 4)
                                                        mouse.accepted = true
                                                    }
                                                    onPressAndHold: function(mouse) {
                                                        watchdogOtherThreadStackDepthField.forceActiveFocus()
                                                        watchdogDepthContextMenu.targetField = watchdogOtherThreadStackDepthField
                                                        watchdogDepthContextMenu.popup(watchdogOtherThreadStackDepthContainer, mouse.x, watchdogOtherThreadStackDepthContainer.height + 4)
                                                        mouse.accepted = true
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                Menu {
                                    id: watchdogDepthContextMenu
                                    width: 180
                                    padding: 6
                                    property TextField targetField: null

                                    background: Rectangle {
                                        color: {
                                            if (modManager.theme === "Crimson") return Qt.rgba(0.05, 0.06, 0.10, 0.98)
                                            if (modManager.theme === "Glassmorphic") return Qt.rgba(0.05, 0.05, 0.07, Math.max(0.88, Math.min(0.97, modManager.glassOpacity + 0.84)))
                                            if (modManager.theme === "Wavey") return Qt.rgba(0.08, 0.16, 0.28, 0.97)
                                            return window.surfaceColor
                                        }
                                        radius: 12
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic"
                                            ? Qt.rgba(1, 1, 1, 0.10)
                                            : (modManager.theme === "Crimson" ? Qt.rgba(1, 1, 1, 0.08) : window.borderColor)

                                        layer.enabled: true
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: Qt.rgba(0, 0, 0, 0.40)
                                            shadowHorizontalOffset: 0
                                            shadowVerticalOffset: 8
                                            shadowBlur: 0.75
                                        }
                                    }

                                    RuntimeContextMenuItem {
                                        text: "Undo"
                                        iconName: "undo-2.svg"
                                        enabled: watchdogDepthContextMenu.targetField !== null && watchdogDepthContextMenu.targetField.canUndo
                                        onTriggered: {
                                            if (watchdogDepthContextMenu.targetField)
                                                watchdogDepthContextMenu.targetField.undo()
                                        }
                                    }
                                    RuntimeContextMenuItem {
                                        text: "Redo"
                                        iconName: "redo-2.svg"
                                        enabled: watchdogDepthContextMenu.targetField !== null && watchdogDepthContextMenu.targetField.canRedo
                                        onTriggered: {
                                            if (watchdogDepthContextMenu.targetField)
                                                watchdogDepthContextMenu.targetField.redo()
                                        }
                                    }
                                    MenuSeparator {}
                                    RuntimeContextMenuItem {
                                        text: "Cut"
                                        iconName: "scissors.svg"
                                        enabled: watchdogDepthContextMenu.targetField !== null && watchdogDepthContextMenu.targetField.selectedText.length > 0
                                        onTriggered: {
                                            if (watchdogDepthContextMenu.targetField)
                                                watchdogDepthContextMenu.targetField.cut()
                                        }
                                    }
                                    RuntimeContextMenuItem {
                                        text: "Copy"
                                        iconName: "copy.svg"
                                        enabled: watchdogDepthContextMenu.targetField !== null && watchdogDepthContextMenu.targetField.selectedText.length > 0
                                        onTriggered: {
                                            if (watchdogDepthContextMenu.targetField)
                                                watchdogDepthContextMenu.targetField.copy()
                                        }
                                    }
                                    RuntimeContextMenuItem {
                                        text: "Paste"
                                        iconName: "clipboard.svg"
                                        onTriggered: {
                                            if (watchdogDepthContextMenu.targetField)
                                                watchdogDepthContextMenu.targetField.paste()
                                        }
                                    }
                                    RuntimeContextMenuItem {
                                        text: "Delete"
                                        iconName: "trash.svg"
                                        enabled: watchdogDepthContextMenu.targetField !== null && watchdogDepthContextMenu.targetField.selectedText.length > 0
                                        onTriggered: {
                                            if (watchdogDepthContextMenu.targetField)
                                                watchdogDepthContextMenu.targetField.remove(watchdogDepthContextMenu.targetField.selectionStart, watchdogDepthContextMenu.targetField.selectionEnd)
                                        }
                                    }
                                    MenuSeparator {}
                                    RuntimeContextMenuItem {
                                        text: "Select All"
                                        iconName: "text-select.svg"
                                        enabled: watchdogDepthContextMenu.targetField !== null && watchdogDepthContextMenu.targetField.text.length > 0
                                        onTriggered: {
                                            if (watchdogDepthContextMenu.targetField)
                                                watchdogDepthContextMenu.targetField.selectAll()
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: sdkGenerationColumn.implicitHeight + 26
                                radius: 16
                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                                border.width: 1.5
                                border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)

                                ColumnLayout {
                                    id: sdkGenerationColumn
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 6

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: "SDK Generation"
                                            font.pixelSize: 14
                                            font.weight: Font.Bold
                                            color: window.textColor
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "Paths & build outputs"
                                            font.pixelSize: 11
                                            color: window.textSecondaryColor
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Item { Layout.preferredHeight: 2 }

                                    RuntimeConfigToggle {
                                        label: "SDK Path"
                                        checked: runtimeConfigDialog.sdkValidation
                                        onToggled: (value) => runtimeConfigDialog.sdkValidation = value
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 46
                                        visible: runtimeConfigDialog.sdkValidation
                                        radius: 10
                                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(0, 0, 0, 0.02)
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 6
                                            spacing: 8

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 32
                                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.16) : Qt.rgba(255, 255, 255, 0.45)
                                                radius: 8
                                                border.width: 1
                                                border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.28)

                                                Text {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 10
                                                    anchors.rightMargin: 10
                                                    text: runtimeConfigDialog.customFolderPath || "No folder selected"
                                                    font.pixelSize: 11
                                                    color: runtimeConfigDialog.customFolderPath ? window.textColor : window.textSecondaryColor
                                                    verticalAlignment: Text.AlignVCenter
                                                    elide: Text.ElideMiddle
                                                }
                                            }

                                            Button {
                                                id: browseFolderButton
                                                text: "Browse"
                                                Layout.preferredWidth: 70
                                                Layout.preferredHeight: 32
                                                flat: true
                                                hoverEnabled: true

                                                contentItem: Text {
                                                    text: parent.text
                                                    font.pixelSize: 12
                                                    font.weight: Font.DemiBold
                                                    color: browseFolderButton.hovered ? "#FFFFFF" : window.textColor
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }

                                                background: Rectangle {
                                                    color: browseFolderButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                                                    radius: 8
                                                    border.width: 1
                                                    border.color: browseFolderButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                                                    Behavior on color { ColorAnimation { duration: 150 } }
                                                    Behavior on border.color { ColorAnimation { duration: 150 } }
                                                }

                                                onClicked: customFolderDialog.open()
                                            }
                                        }
                                    }

                                    RuntimeConfigToggle {
                                        label: "SDK Libclang Generation"
                                        checked: runtimeConfigDialog.sdkLibclangGeneration
                                        onToggled: (value) => runtimeConfigDialog.sdkLibclangGeneration = value
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 46
                                        visible: runtimeConfigDialog.sdkLibclangGeneration
                                        radius: 10
                                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(0, 0, 0, 0.02)
                                        border.width: 1
                                        border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 6
                                            spacing: 8

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 32
                                                color: modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.16) : Qt.rgba(255, 255, 255, 0.45)
                                                radius: 8
                                                border.width: 1
                                                border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.28)

                                                Text {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 10
                                                    anchors.rightMargin: 10
                                                    text: runtimeConfigDialog.libclangPath || "No path selected"
                                                    font.pixelSize: 11
                                                    color: runtimeConfigDialog.libclangPath ? window.textColor : window.textSecondaryColor
                                                    verticalAlignment: Text.AlignVCenter
                                                    elide: Text.ElideMiddle
                                                }
                                            }

                                            Button {
                                                id: browseLibclangButton
                                                text: "Browse"
                                                Layout.preferredWidth: 70
                                                Layout.preferredHeight: 32
                                                flat: true
                                                hoverEnabled: true

                                                contentItem: Text {
                                                    text: parent.text
                                                    font.pixelSize: 12
                                                    font.weight: Font.DemiBold
                                                    color: browseLibclangButton.hovered ? "#FFFFFF" : window.textColor
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }

                                                background: Rectangle {
                                                    color: browseLibclangButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                                                    radius: 8
                                                    border.width: 1
                                                    border.color: browseLibclangButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                                                    Behavior on color { ColorAnimation { duration: 150 } }
                                                    Behavior on border.color { ColorAnimation { duration: 150 } }
                                                }

                                                onClicked: libclangPathDialog.open()
                                            }
                                        }
                                    }

                                    RuntimeConfigToggle {
                                        label: "Generate Build System"
                                        checked: runtimeConfigDialog.generateBuildSystem
                                        onToggled: (value) => runtimeConfigDialog.generateBuildSystem = value
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: runtimeConfigDialog.configPath
                            ? ("Config: " + runtimeConfigDialog.configPath.replace(/\\/g, "/").split("/").pop())
                            : ""
                        font.pixelSize: 11
                        color: window.textSecondaryColor
                        opacity: 0.75
                        elide: Text.ElideMiddle
                        visible: text.length > 0
                    }

                    Item {
                        Layout.fillWidth: true
                        visible: !runtimeConfigDialog.configPath
                    }

                    Button {
                        id: runtimeConfigCancelButton
                        text: "Cancel"
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 40
                        flat: true
                        hoverEnabled: true

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            color: runtimeConfigCancelButton.hovered ? window.secondaryButtonTextHoverColor : window.secondaryButtonTextColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        background: Rectangle {
                            color: runtimeConfigCancelButton.hovered ? window.secondaryButtonHoverColor : window.secondaryButtonColor
                            radius: 20
                            border.width: 1
                            border.color: window.secondaryButtonBorderColor

                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: runtimeConfigDialog.close()
                    }

                    Button {
                        id: runtimeConfigSaveButton
                        text: "Save"
                        Layout.preferredWidth: 130
                        Layout.preferredHeight: 40
                        flat: true
                        hoverEnabled: true

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 14
                            font.weight: Font.Bold
                            color: runtimeConfigSaveButton.hovered ? "#FFFFFF" : window.textColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        background: Rectangle {
                            color: runtimeConfigSaveButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                            radius: 20
                            border.width: 1.5
                            border.color: runtimeConfigSaveButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: runtimeConfigDialog.saveConfig()
                    }
                }
            }
        }
    }
    FolderDialog {
        id: runtimeOutputPathDialog
        title: "Select Runtime Generator Output Folder"
        onAccepted: modManager.setRuntimeOutputPath(selectedFolder.toString().replace("file:///", ""))
    }

    FolderDialog {
        id: customFolderDialog
        title: "Select Custom Folder Path"
        onAccepted: runtimeConfigDialog.customFolderPath = selectedFolder.toString().replace("file:///", "")
    }

    FileDialog {
        id: libclangPathDialog
        title: "Select LibclangPath (libclang.dll)"
        fileMode: FileDialog.OpenFile
        nameFilters: ["DLL files (*.dll)", "All files (*)"]
        onAccepted: runtimeConfigDialog.libclangPath = selectedFile.toString().replace("file:///", "")
    }

    FolderDialog {
        id: luaModsPathDialog
        title: "Select Lua Mods Folder"
        onAccepted: {
            var raw = selectedFolder.toString().replace("file:///", "").replace(/^\/([A-Za-z]:)/, "$1")
            runtimeConfigDialog.luaModsPath = raw
            var norm = function(p) { return (p || "").toString().replace(/\\/g, "/").replace(/\/+$/, "").toLowerCase() }
            var nPath = norm(raw)
            var matchedId = ""
            var allIds = luaProfileManager.profileIds || []
            for (var i = 0; i < allIds.length; i++) {
                var pF = modManager.luaProfileFolderPath(allIds[i])
                if (pF && norm(pF) === nPath) {
                    matchedId = allIds[i]
                    break
                }
            }
            runtimeConfigDialog.selectedProfileId = matchedId
            luaProfilePicker.currentIndex = luaProfilePicker.findProfileIndex(matchedId)
        }
    }

    FolderDialog {
        id: cppModsPathDialog
        title: "Select C++ Mods Folder"
        onAccepted: {
            var raw = selectedFolder.toString().replace("file:///", "").replace(/^\/([A-Za-z]:)/, "$1")
            runtimeConfigDialog.cppModsPath = raw
        }
    }

    FolderDialog {
        id: watchdogLogDirectoryDialog
        title: "Select Watchdog Reports Folder"
        onAccepted: {
            var raw = selectedFolder.toString().replace("file:///", "").replace(/^\/([A-Za-z]:)/, "$1")
            runtimeConfigDialog.watchdogLogDirectory = raw
        }
    }

    component LuaProfilePicker : Rectangle {
        id: luaProfilePickerRoot
        Layout.fillWidth: true
        Layout.preferredHeight: 46
        radius: 10
        color: (pickerRowMouse.containsMouse || profilePopup.visible)
            ? (modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.22) : Qt.rgba(255, 255, 255, 0.60))
            : (modManager.theme === "Glassmorphic" ? Qt.rgba(0, 0, 0, 0.16) : Qt.rgba(255, 255, 255, 0.45))
        border.width: (pickerRowMouse.containsMouse || profilePopup.visible) ? 1.5 : 1
        border.color: (pickerRowMouse.containsMouse || profilePopup.visible)
            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.45)
            : (modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.28))
        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }

                property int currentIndex: 0
        property string filterText: ""

        readonly property var filteredEntries: {
            var src = profileEntries
            var f = filterText.toLowerCase()
            if (f.length === 0) return src
            var out = []
            for (var i = 0; i < src.length; i++) {
                var e = src[i]
                if ((e.name || "").toLowerCase().indexOf(f) !== -1 ||
                    (e.folder || "").toLowerCase().indexOf(f) !== -1) {
                    out.push(e)
                }
            }
            return out
        }

        function modCountForFolder(folder) {
            var res = modManager.listLuaMods(folder)
            if (!res || res.length === 0) return -1
            if (res[0].error !== undefined) return -1
            var n = 0
            for (var k = 0; k < res.length; k++) {
                if (res[k].error === undefined) n++
            }
            return n
        }

        function badgeTextFor(entry) {
            if (entry.count >= 0) return entry.count === 1 ? "1 mod" : entry.count + " mods"
            return entry.id === "" ? "Shared" : "Profile"
        }

        function isEntrySelected(entry) {
            var cur = profileEntries.length > currentIndex ? profileEntries[currentIndex] : null
            return cur !== null && cur.id === entry.id
        }

        property var profileEntries: []

        function rebuildEntries() {
            var ids = luaProfileManager.profileIds || []
            var entries = [{ id: "", name: "Default (shared Mods folder)", folder: "", count: -1 }]
            for (var i = 0; i < ids.length; i++) {
                var pid = ids[i]
                var pname = ""
                var allPresets = modManager.gamePresets || []
                for (var j = 0; j < allPresets.length; j++) {
                    if (allPresets[j].id === pid) {
                        pname = allPresets[j].name
                        break
                    }
                }
                var folder = modManager.luaProfileFolderPath(pid)
                if (folder && folder.length > 0) {
                    entries.push({ id: pid, name: pname || pid, folder: folder, count: modCountForFolder(folder) })
                }
            }
            entries[0].count = modCountForFolder(sharedModsBase)
            profileEntries = entries
        }

        Connections {
            target: luaProfileManager
            function onProfileFoldersChanged() { luaProfilePickerRoot.rebuildEntries() }
            function onProfilesChanged() { luaProfilePickerRoot.rebuildEntries() }
        }
        Connections {
            target: modManager
            function onGamePresetsChanged() { luaProfilePickerRoot.rebuildEntries() }
        }

        function findProfileIndex(profileId) {
            for (var i = 0; i < profileEntries.length; i++) {
                if (profileEntries[i].id === profileId) return i
            }
            return 0
        }

        function selectIndex(idx) {
            if (idx < 0 || idx >= profileEntries.length) return
            currentIndex = idx
            var entry = profileEntries[idx]
            runtimeConfigDialog.selectedProfileId = entry.id

            runtimeConfigDialog.luaModsPath = entry.folder || ""
        }
        function selectEntry(entry) {
            for (var i = 0; i < profileEntries.length; i++) {
                if (profileEntries[i].id === entry.id) {
                    selectIndex(i)
                    break
                }
            }
        }

        readonly property string sharedModsBase: {
            var p = modManager.defaultLuaModsDirectory()
            return p ? String(p).replace(/\\/g, "/") : ""
        }

        onVisibleChanged: {
            if (visible) {
                currentIndex = findProfileIndex(runtimeConfigDialog.selectedProfileId)
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 12
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: (luaProfilePickerRoot.currentIndex >= 0
                && luaProfilePickerRoot.currentIndex < luaProfilePickerRoot.profileEntries.length)
                ? luaProfilePickerRoot.profileEntries[luaProfilePickerRoot.currentIndex].name
                : "Select a Lua profile..."
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: runtimeConfigDialog.selectedProfileId === "" ? window.textSecondaryColor : window.textColor
                elide: Text.ElideRight
            }

            Text {
                text: profilePopup.visible ? "▲" : "▼"
                font.pixelSize: 10
                color: window.textSecondaryColor
                verticalAlignment: Text.AlignVCenter
            }
        }

        MouseArea {
            id: pickerRowMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor

            onPressed: {
                if (profilePopup.visible) {
                    profilePopup.close()
                } else {
                    luaProfilePickerRoot.rebuildEntries()
                    profilePopup.open()
                }
            }
        }

        Popup {
            id: profilePopup
            x: 0
            y: luaProfilePickerRoot.height + 6
            width: luaProfilePickerRoot.width
            height: Math.max(180, Math.min(340, profileList.contentHeight + 77))
            padding: 0
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

            onOpened: {
                profileSearchInput.text = ""
                luaProfilePickerRoot.rebuildEntries()
            }

            background: Rectangle {
                                radius: 16
                color: modManager.theme === "Glassmorphic"
                    ? Qt.rgba(0.10, 0.10, 0.15, 0.98)
                    : (modManager.theme === "Wavey" ? Qt.rgba(0.08, 0.16, 0.28, 0.97)
                    : window.surfaceColor)
                border.width: 1.5
                border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.25) : window.borderColor

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(0, 0, 0, 0.45)
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 10
                    shadowBlur: 0.8
                }
            }

            contentItem: ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    radius: 12
                    color: modManager.theme === "Glassmorphic"
                        ? Qt.rgba(1, 1, 1, 0.08)
                        : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.06)
                    border.width: 1
                    border.color: profileSearchInput.activeFocus ? window.primaryColor : window.borderColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        TextInput {
                            id: profileSearchInput
                            Layout.fillWidth: true
                            font.pixelSize: 13
                            color: window.textColor
                            verticalAlignment: TextInput.AlignVCenter
                            clip: true
                            selectByMouse: true

                            Text {
                                anchors.fill: parent
                                text: "Search profiles by name or folder..."
                                font.pixelSize: 13
                                color: window.textSecondaryColor
                                verticalAlignment: Text.AlignVCenter
                                visible: !profileSearchInput.text && !profileSearchInput.activeFocus
                            }

                            onTextChanged: luaProfilePickerRoot.filterText = text
                        }

                        Text {
                            text: "Clear"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: clearProfileSearchMouse.containsMouse ? window.primaryColor : window.textSecondaryColor
                            visible: profileSearchInput.text.length > 0
                            verticalAlignment: Text.AlignVCenter

                            MouseArea {
                                id: clearProfileSearchMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: profileSearchInput.text = ""
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.4)
                }

                ListView {
                    id: profileList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: luaProfilePickerRoot.filteredEntries

                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        id: profileDelegateItem
                        width: profileList.width - (profileList.contentHeight > profileList.height ? 12 : 0)
                        height: (modelData.folder && modelData.folder.length > 0) ? 52 : 40
                        radius: 10
                        color: profileDelegateMouse.containsMouse
                            ? (modManager.theme === "Glassmorphic"
                                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.18)
                                : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12))
                            : (luaProfilePickerRoot.isEntrySelected(modelData)
                                ? (modManager.theme === "Glassmorphic"
                                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.10)
                                    : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08))
                                : "transparent")
                        border.width: luaProfilePickerRoot.isEntrySelected(modelData) ? 1 : 0
                        border.color: window.primaryColor
                        Behavior on color { ColorAnimation { duration: 120 } }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            anchors.topMargin: 4
                            anchors.bottomMargin: 4
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name || "Unknown"
                                    font.pixelSize: 13
                                    font.weight: luaProfilePickerRoot.isEntrySelected(modelData) ? Font.Bold : Font.DemiBold
                                    color: luaProfilePickerRoot.isEntrySelected(modelData)
                                        ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : window.primaryColor)
                                        : window.textColor
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    Layout.preferredHeight: 20
                                    Layout.preferredWidth: profileBadgeText.implicitWidth + 12
                                    radius: 10
                                    color: modManager.theme === "Glassmorphic"
                                        ? Qt.rgba(1, 1, 1, 0.12)
                                        : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08)

                                    Text {
                                        id: profileBadgeText
                                        anchors.centerIn: parent
                                        text: luaProfilePickerRoot.badgeTextFor(modelData)
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                        color: window.textSecondaryColor
                                    }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.folder || ""
                                font.pixelSize: 11
                                color: window.textSecondaryColor
                                opacity: 0.8
                                elide: Text.ElideMiddle
                                visible: text.length > 0
                            }
                        }

                        MouseArea {
                            id: profileDelegateMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                luaProfilePickerRoot.selectEntry(modelData)
                                profilePopup.close()
                                if (toastNotification) toastNotification.show("Lua profile: " + (modelData.name || "Default"))
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: luaProfilePickerRoot.profileEntries.length === 0 ? "No Lua profiles found" : "No matching profiles"
                        font.pixelSize: 13
                        color: window.textSecondaryColor
                        visible: profileList.count === 0
                    }
                }
            }
        }
    }

    Dialog {
        id: avaloniumConflictDialog
        modal: true
        closePolicy: Popup.CloseOnEscape
        anchors.centerIn: Overlay.overlay
        width: 480
        padding: 0

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy") return cozyDialogGradient
                if (modManager.theme === "Crimson") return crimsonDialogGradient
                if (modManager.theme === "Wavey") return waveyDialogGradient
                return null
            }

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.7)

            Behavior on opacity {
                NumberAnimation { duration: 250 }
            }
        }

        enter: Transition {
            NumberAnimation {
                property: "scale"
                from: 0.9
                to: 1.0
                duration: 300
                easing.type: Easing.OutBack
            }
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 250
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "scale"
                from: 1.0
                to: 0.95
                duration: 200
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }

        contentItem: Item {
            implicitWidth: avaloniumConflictDialog.width
            implicitHeight: conflictContentColumn.implicitHeight

            ColumnLayout {
                id: conflictContentColumn
                anchors.fill: parent
                anchors.margins: 0
                spacing: 0

                Item { Layout.preferredHeight: 32 }

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 68
                    Layout.preferredHeight: 68

                    Image {
                        id: alertIcon
                        anchors.fill: parent
                        source: {
                            if (modManager.theme === "Cozy")
                                return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/alert.png"
                            if (modManager.theme === "Glassmorphic")
                                return "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/alert.png"
                            if (modManager.theme === "Crimson")
                                return "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/alert.png"
                            if (modManager.theme === "Wavey")
                                return "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/alert.png"
                            return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/alert.png"
                        }
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        visible: modManager.theme !== "Glassmorphic"
                    }

                    ColorOverlay {
                        anchors.fill: alertIcon
                        source: alertIcon
                        color: window.primaryColor
                        visible: modManager.theme === "Glassmorphic"
                    }
                }

                Item { Layout.preferredHeight: 20 }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 40
                    Layout.rightMargin: 40
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: "Avalonium Conflict Detected!"
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        color: modManager.theme === "Glassmorphic" ? "#FFFFFF" : window.textColor
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "You're trying to inject into a game but there is already Avalonium.dll (or UniversalManager.dll) next to the game.\n\nRemove the DLL from the Binaries\\Win64 folder and then re-inject."
                        font.pixelSize: 15
                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.7) : window.textSecondaryColor
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        lineHeight: 1.4
                    }
                }

                Item { Layout.fillHeight: true; Layout.preferredHeight: 28 }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: 32
                    spacing: 16

                    Button {
                        id: conflictGotItButton
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 48
                        text: "Got it"
                        flat: true
                        hoverEnabled: true

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 15
                            font.weight: Font.Medium
                            color: conflictGotItButton.hovered ? window.secondaryButtonTextHoverColor : window.secondaryButtonTextColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        background: Rectangle {
                            color: conflictGotItButton.hovered ? window.secondaryButtonHoverColor : window.secondaryButtonColor
                            radius: 26
                            border.width: 1
                            border.color: window.secondaryButtonBorderColor

                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: avaloniumConflictDialog.close()
                    }

                    Button {
                        id: conflictDeleteButton
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 48
                        flat: true
                        hoverEnabled: true

                        contentItem: Text {
                            text: "Delete it now"
                            font.pixelSize: 15
                            font.weight: Font.Bold
                            color: conflictDeleteButton.hovered ? "#FFFFFF" : window.textColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        background: Rectangle {
                            color: conflictDeleteButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                            radius: 24
                            border.width: 1.5
                            border.color: conflictDeleteButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {

                            avaloniumConflictDialog.close()

                            if (modManager.deleteConflictingAvaloniumDll()) {
                                toastNotification.show("Conflicting DLL deleted successfully")
                            } else {

                                if (!modManager.isRunningAsAdmin && modManager.isGameOnCDrive()) {
                                    toastNotification.show("Administrator privileges required to delete DLL on C: drive. Restart Mod Manager as admin and try again.", 5000)
                                } else if (!modManager.isRunningAsAdmin) {
                                    toastNotification.show("Failed to delete DLL. It may be in use by the game or require administrator privileges. Try closing the game and/or restarting as admin.", 6000)
                                } else {
                                    toastNotification.show("Failed to delete the conflicting DLL. The file may be in use by the game. Please close the game and try again.", 5000)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: modManager
        function onAvaloniumConflictDetected() {
            avaloniumConflictDialog.open()
        }
    }

    Dialog {
        id: cDriveAdminDialog
        modal: true
        closePolicy: Popup.CloseOnEscape
        anchors.centerIn: Overlay.overlay
        width: 480
        padding: 0

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy") return cozyDialogGradient
                if (modManager.theme === "Crimson") return crimsonDialogGradient
                if (modManager.theme === "Wavey") return waveyDialogGradient
                return null
            }

            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.7)

            Behavior on opacity {
                NumberAnimation { duration: 250 }
            }
        }

        enter: Transition {
            NumberAnimation {
                property: "scale"
                from: 0.9
                to: 1.0
                duration: 300
                easing.type: Easing.OutBack
            }
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 250
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "scale"
                from: 1.0
                to: 0.95
                duration: 200
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
        }

        contentItem: Item {
            implicitWidth: cDriveAdminDialog.width
            implicitHeight: cDriveContentColumn.implicitHeight

            ColumnLayout {
                id: cDriveContentColumn
                anchors.fill: parent
                anchors.margins: 0
                spacing: 0

                Item { Layout.preferredHeight: 32 }

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 68
                    Layout.preferredHeight: 68
                    source: {
                        if (modManager.theme === "Cozy")
                            return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Admin.png"
                        if (modManager.theme === "Glassmorphic")
                            return "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Admin.png"
                        if (modManager.theme === "Crimson")
                            return "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Admin.png"
                        if (modManager.theme === "Wavey")
                            return "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/admin.png"
                        return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Admin.png"
                    }
                    fillMode: Image.PreserveAspectFit
                    smooth: true

                    layer.enabled: modManager.theme === "Glassmorphic"
                    layer.effect: ColorOverlay {
                        color: modManager.accentColor
                    }
                }

                Item { Layout.preferredHeight: 20 }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 40
                    Layout.rightMargin: 40
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: "Hey You!"
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        color: modManager.theme === "Glassmorphic" ? "#FFFFFF" : window.textColor
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "You're trying to launch a game on the C: drive.\n\nRestart Universal Manager as an administrator and try again."
                        font.pixelSize: 15
                        color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.7) : window.textSecondaryColor
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        lineHeight: 1.4
                    }
                }

                Item { Layout.fillHeight: true; Layout.preferredHeight: 28 }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: 32
                    spacing: 16

                    Button {
                        id: cDriveGotItButton
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 48
                        text: "Got it"
                        flat: true
                        hoverEnabled: true

                        contentItem: Text {
                            text: parent.text
                            font.pixelSize: 15
                            font.weight: Font.Medium
                            color: cDriveGotItButton.hovered ? window.secondaryButtonTextHoverColor : window.secondaryButtonTextColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        background: Rectangle {
                            color: cDriveGotItButton.hovered ? window.secondaryButtonHoverColor : window.secondaryButtonColor
                            radius: 26
                            border.width: 1
                            border.color: window.secondaryButtonBorderColor

                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: cDriveAdminDialog.close()
                    }

                    Button {
                        id: cDriveRestartButton
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 48
                        flat: true
                        hoverEnabled: true

                        contentItem: Text {
                            text: "Restart as Admin"
                            font.pixelSize: 15
                            font.weight: Font.Bold
                            color: cDriveRestartButton.hovered ? "#FFFFFF" : window.textColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter

                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        background: Rectangle {
                            color: cDriveRestartButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                            radius: 24
                            border.width: 1.5
                            border.color: cDriveRestartButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                            Behavior on color { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            if (modManager.restartAsAdministrator()) {
                                toastNotification.show("Restarting as administrator...")
                                cDriveAdminDialog.close()
                            } else {
                                toastNotification.show("Failed to restart as administrator")
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: modManager
        function onCDriveAdminRequired() {
            cDriveAdminDialog.open()
        }
    }

    Dialog {
        id: unsupportedVersionGameRunningDialog
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        width: 420
        padding: 0

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text:       modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }

        background: Rectangle {
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy")  return cozyDialogGradient
                if (modManager.theme === "Crimson")  return crimsonDialogGradient
                if (modManager.theme === "Wavey") return waveyDialogGradient
                return null
            }
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.7)
            Behavior on opacity { NumberAnimation { duration: 250 } }
        }

        enter: Transition {
            NumberAnimation { property: "scale";   from: 0.9;  to: 1.0;  duration: 300; easing.type: Easing.OutBack }
            NumberAnimation { property: "opacity"; from: 0;   to: 1;    duration: 250 }
        }
        exit: Transition {
            NumberAnimation { property: "scale";   from: 1.0;  to: 0.95; duration: 200; easing.type: Easing.InCubic }
            NumberAnimation { property: "opacity"; from: 1;   to: 0;    duration: 200 }
        }

        Connections {
            target: modManager
            function onInitializingChanged() {
                if (!modManager.initializing && unsupportedVersionGameRunningDialog.visible) {
                    initializationDialog.gameStillRunningAfterError = false
                    unsupportedVersionGameRunningDialog.close()
                }
            }
        }

        Timer {
            interval: 500
            repeat: true
            running: unsupportedVersionGameRunningDialog.visible
            onTriggered: {
                if (!modManager.isGameProcessRunning()) {
                    initializationDialog.gameStillRunningAfterError = false
                    unsupportedVersionGameRunningDialog.close()
                }
            }
        }

        onClosed: {
            if (!modManager.isGameProcessRunning()) {
                initializationDialog.gameStillRunningAfterError = false
            }
        }

        contentItem: Item {
            implicitWidth:  unsupportedVersionGameRunningDialog.width
            implicitHeight: gameRunningColumn.implicitHeight

            ColumnLayout {
                id: gameRunningColumn
                anchors.fill: parent
                anchors.margins: 0
                spacing: 0

                Item { Layout.preferredHeight: 36 }

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48

                    Image {
                        id: gameRunningAlertIcon
                        anchors.fill: parent
                        source: {
                            if (modManager.theme === "Cozy")
                                return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/alert.png"
                            if (modManager.theme === "Glassmorphic")
                                return "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/alert.png"
                            if (modManager.theme === "Crimson")
                                return "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/alert.png"
                            if (modManager.theme === "Wavey")
                                return "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/alert.png"
                            return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/alert.png"
                        }
                        sourceSize: Qt.size(48, 48)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        visible: modManager.theme !== "Glassmorphic"
                    }

                    ColorOverlay {
                        anchors.fill: gameRunningAlertIcon
                        source: gameRunningAlertIcon
                        color: window.primaryColor
                        visible: modManager.theme === "Glassmorphic"
                    }
                }

                Item { Layout.preferredHeight: 20 }

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: 40
                    Layout.rightMargin: 40
                    text: "Game Still Running"
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: modManager.theme === "Glassmorphic" ? "#FFFFFF" : window.textColor
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Item { Layout.preferredHeight: 10 }

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: 40
                    Layout.rightMargin: 40
                    text: "The game is currently running. Please close or terminate it before performing this action."
                    font.pixelSize: 13
                    color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.65) : window.textSecondaryColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1.4
                }

                Item { Layout.preferredHeight: 28 }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: 28
                    spacing: 16

                    Button {
                        id: gameRunningOkayButton
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 48
                        flat: true
                        hoverEnabled: true

                        contentItem: Text {
                            text: "Okay"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        background: Rectangle {
                            color: gameRunningOkayButton.hovered
                                ? (modManager.theme === "Glassmorphic"
                                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.12)
                                    : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08))
                                : (modManager.theme === "Glassmorphic"
                                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08)
                                    : "transparent")
                            radius: 24
                            border.width: 1
                            border.color: window.borderColor
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        onClicked: {
                            unsupportedVersionGameRunningDialog.close()
                        }
                    }

                    Button {
                        id: gameRunningKillButton
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 48
                        flat: true
                        hoverEnabled: true

                        contentItem: Text {
                            text: "Kill Process"
                            font.pixelSize: 14
                            font.weight: Font.Bold
                            color: gameRunningKillButton.hovered ? "#FFFFFF" : window.textColor
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        background: Rectangle {
                            color: gameRunningKillButton.hovered
                                ? window.primaryColor
                                : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                            radius: 24
                            border.width: 1.5
                            border.color: gameRunningKillButton.hovered
                                ? window.primaryColor
                                : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)
                            Behavior on color       { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                        }

                        onClicked: {
                            modManager.killGameProcess()
                            unsupportedVersionGameRunningDialog.close()
                            toastNotification.show("Game process terminated")
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: runtimeAlertDialog
        modal: true
        closePolicy: Popup.CloseOnEscape
        anchors.centerIn: Overlay.overlay
        width: 420
        padding: 0

        property string titleText: ""
        property string messageText: ""

        function showMessage(title, message) {
            titleText = title
            messageText = message
            open()
        }

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text:       modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }

        background: Rectangle {
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy")  return cozyDialogGradient
                if (modManager.theme === "Crimson")  return crimsonDialogGradient
                if (modManager.theme === "Wavey") return waveyDialogGradient
                return null
            }
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.10, 0.10, 0.15, 0.98) : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.4)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.7)
            Behavior on opacity { NumberAnimation { duration: 250 } }
        }

        enter: Transition {
            NumberAnimation { property: "scale";   from: 0.9;  to: 1.0;  duration: 300; easing.type: Easing.OutBack }
            NumberAnimation { property: "opacity"; from: 0;   to: 1;    duration: 250 }
        }
        exit: Transition {
            NumberAnimation { property: "scale";   from: 1.0;  to: 0.95; duration: 200; easing.type: Easing.InCubic }
            NumberAnimation { property: "opacity"; from: 1;   to: 0;    duration: 200 }
        }

        contentItem: Item {
            implicitWidth:  runtimeAlertDialog.width
            implicitHeight: alertDialogColumn.implicitHeight

            ColumnLayout {
                id: alertDialogColumn
                anchors.fill: parent
                anchors.margins: 0
                spacing: 0

                Item { Layout.preferredHeight: 36 }

                Item {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48

                    Image {
                        id: runtimeAlertIcon
                        anchors.fill: parent
                        source: {
                            if (modManager.theme === "Cozy")
                                return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/alert.png"
                            if (modManager.theme === "Glassmorphic")
                                return "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/alert.png"
                            if (modManager.theme === "Crimson")
                                return "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/alert.png"
                            if (modManager.theme === "Wavey")
                                return "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/alert.png"
                            return "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/alert.png"
                        }
                        sourceSize: Qt.size(48, 48)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        visible: modManager.theme !== "Glassmorphic"
                    }

                    ColorOverlay {
                        anchors.fill: runtimeAlertIcon
                        source: runtimeAlertIcon
                        color: window.primaryColor
                        visible: modManager.theme === "Glassmorphic"
                    }
                }

                Item { Layout.preferredHeight: 20 }

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: 40
                    Layout.rightMargin: 40
                    text: runtimeAlertDialog.titleText
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: modManager.theme === "Glassmorphic" ? "#FFFFFF" : window.textColor
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Item { Layout.preferredHeight: 10 }

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: 40
                    Layout.rightMargin: 40
                    text: runtimeAlertDialog.messageText
                    font.pixelSize: 13
                    color: modManager.theme === "Glassmorphic" ? Qt.rgba(1, 1, 1, 0.65) : window.textSecondaryColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1.4
                }

                Item { Layout.preferredHeight: 28 }

                Button {
                    id: alertOkayButton
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 48
                    Layout.bottomMargin: 28
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: "Okay"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    background: Rectangle {
                        color: alertOkayButton.hovered
                            ? (modManager.theme === "Glassmorphic"
                                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.12)
                                : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08))
                            : (modManager.theme === "Glassmorphic"
                                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08)
                                : "transparent")
                        radius: 24
                        border.width: 1
                        border.color: window.borderColor
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    onClicked: {
                        runtimeAlertDialog.close()
                    }
                }
            }
        }
    }
}
