import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Item {
    id: backupDialogs
    anchors.fill: parent

    property alias backupProgressBar: backupProgressBar
    property alias backupProgressDialog: backupProgressDialog
    property alias backupProgressText: backupProgressText
    property alias backupTypeDialog: backupModsDialogs.backupTypeDialog
    property alias deleteFinishTimer: deleteFinishTimer
    property alias deleteProgressDialog: deleteProgressDialog
    property alias installModDialog: installModDialog
    property alias resultDialog: resultDialog

    BackupModsDialogs {
        id: backupModsDialogs
        anchors.fill: parent

        onBackupRequested: function(names) {
            backupProgressDialog.open()
            modManager.backupModsAsync(names)
        }
    }

    FileDialog {
        id: installModDialog
        title: "Select Mod File(s) to Install"
        fileMode: FileDialog.OpenFiles
        nameFilters: [
            "Mod Files (*.zip *.rar *.7z *.pak *.ucas *.utoc)",
            "Archives (*.zip *.rar *.7z)",
            "UE Files (*.pak *.ucas *.utoc)",
            "-x Mods ( *.pak-x *.ucas-x *.utoc-x)",
            "All Files (*.*)"
        ]
        onAccepted: {
            var files = []
            for (var i = 0; i < selectedFiles.length; i++) {
                files.push(selectedFiles[i].toString().replace("file:///", ""))
            }
            installingModCount = selectedFiles.length
            installProgressDialog.open()
            modManager.installModsAsync(files)
        }
    }

    Dialog {
        id: backupProgressDialog
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        width: 500
        height: 260
        padding: 0

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.15, 0.15, 0.15, 0.98) : window.surfaceColor
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy") return sharedCozyDialogGradient
                if (modManager.theme === "Crimson") return sharedCrimsonDialogGradient
                if (modManager.theme === "Wavey") return sharedWaveyDialogGradient
                return null
            }

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.6)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.85)

            Behavior on opacity {
                NumberAnimation { duration: 250 }
            }
        }

        enter: Transition {
            NumberAnimation {
                property: "scale"
                from: 0.9
                to: 1.0
                duration: 250
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
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

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 100
                color: "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 32
                    spacing: 16

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "Backing Up Mods"
                            font.pixelSize: 24
                            font.weight: Font.DemiBold
                            color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                            font.family: "Segoe UI Variable Display"
                        }

                        Text {
                            id: backupProgressText
                            text: "Preparing..."
                            font.pixelSize: 14
                            font.weight: Font.Normal
                            color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                            opacity: 0.85
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            ProgressBar {
                id: backupProgressBar
                Layout.fillWidth: true
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                Layout.preferredHeight: 8
                from: 0
                to: 100
                value: 0
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }

    Dialog {
        id: deleteProgressDialog
        property int currentCount: 0
        property int totalCount: 0
        property string currentModName: ""
        property color loaderAccentStart: window.primaryColor
        property color loaderAccentEnd: window.accentColor
        property color loaderHighlight: window.iconColor
        property color loaderTrackColor: modManager.theme === "Glassmorphic"
            ? Qt.rgba(window.iconColor.r, window.iconColor.g, window.iconColor.b, 0.18)
            : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)
        property bool isFinishing: false
        property string finishMessage: ""
        property bool pendingSuccess: true
        property int pendingDeletedCount: 0
        property int pendingTotalCount: 0
        property double openedAtMs: 0
        property int minimumVisibleMs: 1700
        property int finishHoldMs: 520
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        width: 500
        height: 292
        padding: 0

        onOpened: {
            currentCount = 0
            totalCount = 0
            currentModName = ""
            isFinishing = false
            finishMessage = ""
            openedAtMs = Date.now()
        }

        onClosed: deleteFinishTimer.stop()

        Timer {
            id: deleteFinishTimer
            repeat: false
            onTriggered: {
                deleteProgressDialog.close()

                if (deleteProgressDialog.pendingSuccess) {
                    var isPermanent = !modManager.deleteToRecycleBin
                    var count = deleteProgressDialog.pendingDeletedCount
                    var message = isPermanent ?
                        (count === 1 ? "Permanently deleted 1 mod" : "Permanently deleted " + count + " mods") :
                        (count === 1 ? "Moved 1 mod to Recycle Bin" : "Moved " + count + " mods to Recycle Bin")
                    toastNotification.show(message, "trash.svg")
                } else {
                    var count = deleteProgressDialog.pendingDeletedCount
                    var total = deleteProgressDialog.pendingTotalCount
                    var message = count === 1 ? "Deleted 1 of " + total + " mod" + (total === 1 ? "" : "s") : "Deleted " + count + " of " + total + " mods"
                    toastNotification.show(message, "triangle-alert.svg")
                }

                deleteProgressDialog.isFinishing = false
            }
        }

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.15, 0.15, 0.15, 0.98) : window.surfaceColor
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy") return sharedCozyDialogGradient
                if (modManager.theme === "Crimson") return sharedCrimsonDialogGradient
                if (modManager.theme === "Wavey") return sharedWaveyDialogGradient
                return null
            }

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.6)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.85)

            Behavior on opacity {
                NumberAnimation { duration: 250 }
            }
        }

        enter: Transition {
            NumberAnimation {
                property: "scale"
                from: 0.9
                to: 1.0
                duration: 250
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
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

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 96
                color: "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 28
                    spacing: 16

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "Deleting Mods"
                            font.pixelSize: 24
                            font.weight: Font.DemiBold
                            color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                            font.family: "Segoe UI Variable Display"
                        }

                        Text {
                            id: deleteProgressText
                            text: deleteProgressDialog.isFinishing ?
                                  deleteProgressDialog.finishMessage :
                                  (deleteProgressDialog.currentModName !== "" ?
                                   "Deleting: " + deleteProgressDialog.currentModName :
                                   "Preparing selected mods for deletion...")
                            font.pixelSize: 14
                            font.weight: Font.Normal
                            color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                            opacity: 0.85
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24

                Rectangle {
                    id: deleteLoaderStage
                    anchors.fill: parent
                    radius: 20
                    clip: true
                    color: modManager.theme === "Glassmorphic"
                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.04)
                        : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.035)
                    border.width: 1
                    border.color: modManager.theme === "Glassmorphic"
                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.08)
                        : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08)

                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        opacity: 0.95
                        gradient: Gradient {
                            GradientStop {
                                position: 0.0
                                color: modManager.theme === "Glassmorphic"
                                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.03)
                                    : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.03)
                            }
                            GradientStop { position: 0.52; color: "transparent" }
                            GradientStop {
                                position: 1.0
                                color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.03)
                            }
                        }
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width - 72
                        height: 2
                        radius: 1
                        opacity: 0.68
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 0.18; color: deleteProgressDialog.loaderTrackColor }
                            GradientStop { position: 0.5; color: Qt.rgba(deleteProgressDialog.loaderAccentEnd.r, deleteProgressDialog.loaderAccentEnd.g, deleteProgressDialog.loaderAccentEnd.b, 0.32) }
                            GradientStop { position: 0.82; color: deleteProgressDialog.loaderTrackColor }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }

                    Rectangle {
                        id: deleteTrackSweep
                        width: 60
                        height: 6
                        radius: 3
                        y: deleteLoaderStage.height / 2 - height / 2
                        x: deleteLoaderStage.width
                        rotation: 0
                        opacity: 0.38
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 0.5; color: Qt.rgba(deleteProgressDialog.loaderHighlight.r, deleteProgressDialog.loaderHighlight.g, deleteProgressDialog.loaderHighlight.b, 0.28) }
                            GradientStop { position: 1.0; color: "transparent" }
                        }

                        SequentialAnimation on x {
                            running: deleteProgressDialog.visible
                            loops: Animation.Infinite
                            PauseAnimation { duration: 90 }
                            NumberAnimation {
                                from: deleteLoaderStage.width
                                to: -deleteTrackSweep.width
                                duration: 1220
                                easing.type: Easing.InOutQuad
                            }
                            PauseAnimation { duration: 150 }
                        }
                    }

                    Rectangle {
                        id: deleteCenterCard
                        width: 66
                        height: 82
                        radius: 12
                        anchors.centerIn: parent
                        z: 4
                        border.width: 1
                        border.color: Qt.rgba(
                            deleteProgressDialog.loaderHighlight.r,
                            deleteProgressDialog.loaderHighlight.g,
                            deleteProgressDialog.loaderHighlight.b,
                            modManager.theme === "Glassmorphic" ? 0.32 : 0.22
                        )
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: deleteProgressDialog.loaderAccentStart }
                            GradientStop { position: 1.0; color: deleteProgressDialog.loaderAccentEnd }
                        }

                        Rectangle {
                            x: 10
                            y: 11
                            width: 34
                            height: 5
                            radius: 2.5
                            color: Qt.rgba(1, 1, 1, 0.94)
                        }

                        Rectangle {
                            x: 10
                            y: 21
                            width: 22
                            height: 5
                            radius: 2.5
                            color: Qt.rgba(1, 1, 1, 0.76)
                        }

                    }

                    Repeater {
                        model: 7

                        delegate: Rectangle {
                            property int itemIndex: index
                            property real midX: deleteLoaderStage.width * 0.5
                            property real baseY: deleteLoaderStage.height / 2 - height / 2 + (itemIndex % 2 === 0 ? -9 : 7)
                            width: itemIndex % 2 === 0 ? 24 : 30
                            height: itemIndex % 2 === 0 ? 32 : 38
                            radius: 8
                            x: deleteLoaderStage.width + 40
                            y: baseY + 4
                            opacity: 0
                            scale: 0.68
                            rotation: itemIndex % 2 === 0 ? 5 : -5
                            z: x > midX - width / 2 ? 2 : 5
                            border.width: 1
                            border.color: Qt.rgba(
                                deleteProgressDialog.loaderHighlight.r,
                                deleteProgressDialog.loaderHighlight.g,
                                deleteProgressDialog.loaderHighlight.b,
                                modManager.theme === "Glassmorphic" ? 0.24 : 0.18
                            )
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop {
                                    position: 0.0
                                    color: Qt.rgba(
                                               deleteProgressDialog.loaderAccentStart.r,
                                               deleteProgressDialog.loaderAccentStart.g,
                                               deleteProgressDialog.loaderAccentStart.b,
                                               0.22 + (itemIndex * 0.035)
                                           )
                                }
                                GradientStop {
                                    position: 1.0
                                    color: Qt.rgba(
                                               deleteProgressDialog.loaderAccentEnd.r,
                                               deleteProgressDialog.loaderAccentEnd.g,
                                               deleteProgressDialog.loaderAccentEnd.b,
                                               0.14 + (itemIndex * 0.028)
                                           )
                                }
                            }

                            Rectangle {
                                x: 6
                                y: 7
                                width: parent.width * 0.58
                                height: 3
                                radius: 1.5
                                color: Qt.rgba(1, 1, 1, 0.84)
                            }

                            Rectangle {
                                x: 6
                                y: 13
                                width: parent.width * 0.36
                                height: 3
                                radius: 1.5
                                color: Qt.rgba(1, 1, 1, 0.6)
                            }

                            SequentialAnimation on x {
                                running: deleteProgressDialog.visible
                                loops: Animation.Infinite
                                PauseAnimation { duration: itemIndex * 175 }
                                NumberAnimation {
                                    from: deleteLoaderStage.width + 40
                                    to: deleteLoaderStage.width * 0.61
                                    duration: 540
                                    easing.type: Easing.OutQuad
                                }
                                NumberAnimation {
                                    to: deleteLoaderStage.width * 0.42
                                    duration: 340
                                    easing.type: Easing.InOutQuad
                                }
                                NumberAnimation {
                                    to: -width - 48
                                    duration: 500
                                    easing.type: Easing.InQuad
                                }
                            }

                            SequentialAnimation on y {
                                running: deleteProgressDialog.visible
                                loops: Animation.Infinite
                                PauseAnimation { duration: itemIndex * 175 }
                                NumberAnimation {
                                    from: baseY + 5
                                    to: baseY - 4
                                    duration: 540
                                    easing.type: Easing.OutQuad
                                }
                                NumberAnimation {
                                    from: baseY - 4
                                    to: baseY + (itemIndex % 2 === 0 ? -1 : 1)
                                    duration: 340
                                    easing.type: Easing.InOutQuad
                                }
                                NumberAnimation {
                                    from: baseY + (itemIndex % 2 === 0 ? -1 : 1)
                                    to: baseY + 4
                                    duration: 500
                                    easing.type: Easing.InQuad
                                }
                            }

                            SequentialAnimation on scale {
                                running: deleteProgressDialog.visible
                                loops: Animation.Infinite
                                PauseAnimation { duration: itemIndex * 175 }
                                NumberAnimation { from: 0.68; to: 0.9; duration: 540; easing.type: Easing.OutQuad }
                                NumberAnimation { from: 0.9; to: 0.98; duration: 340; easing.type: Easing.InOutQuad }
                                NumberAnimation { from: 0.98; to: 0.52; duration: 500; easing.type: Easing.InQuad }
                            }

                            SequentialAnimation on rotation {
                                running: deleteProgressDialog.visible
                                loops: Animation.Infinite
                                PauseAnimation { duration: itemIndex * 175 }
                                NumberAnimation {
                                    from: itemIndex % 2 === 0 ? 5 : -5
                                    to: itemIndex % 2 === 0 ? 1 : -1
                                    duration: 540
                                    easing.type: Easing.OutQuad
                                }
                                NumberAnimation {
                                    to: itemIndex % 2 === 0 ? -2 : 2
                                    duration: 340
                                    easing.type: Easing.InOutQuad
                                }
                                NumberAnimation {
                                    to: itemIndex % 2 === 0 ? -5 : 5
                                    duration: 500
                                    easing.type: Easing.InQuad
                                }
                            }

                            SequentialAnimation on opacity {
                                running: deleteProgressDialog.visible
                                loops: Animation.Infinite
                                PauseAnimation { duration: itemIndex * 175 }
                                NumberAnimation { from: 0.0; to: 0.58; duration: 150; easing.type: Easing.OutQuad }
                                PauseAnimation { duration: 910 }
                                NumberAnimation { from: 0.58; to: 0.0; duration: 320; easing.type: Easing.InQuad }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.topMargin: 12

                Text {
                    text: deleteProgressDialog.totalCount > 0 ?
                          (deleteProgressDialog.currentCount + " of " + deleteProgressDialog.totalCount + " completed") :
                          "Starting..."
                    font.pixelSize: 12
                    color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: (deleteProgressDialog.totalCount > 0 ?
                           Math.round((deleteProgressDialog.currentCount / deleteProgressDialog.totalCount) * 100) : 0) + "%"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.topMargin: 6
                Layout.bottomMargin: 22
                text: "Please wait while your selected mods are being removed."
                font.pixelSize: 12
                color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                wrapMode: Text.WordWrap
            }
        }
    }

    Dialog {
        id: resultDialog
        property string resultMessage: ""
        property bool isSuccess: true

        modal: true
        anchors.centerIn: Overlay.overlay
        width: 520
        height: Math.max(340, resultDialogContent.implicitHeight)
        padding: 0

        palette {
            windowText: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
            text: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
        }
        background: Rectangle {
            color: modManager.theme === "Glassmorphic" ? Qt.rgba(0.15, 0.15, 0.15, 0.98) : window.surfaceColor
            radius: 24
            border.width: modManager.theme === "Glassmorphic" ? 1.5 : 2
            border.color: modManager.theme === "Glassmorphic" ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g, modManager.accentColor.b, 0.25) : window.borderColor

            gradient: {
                if (modManager.theme === "Cozy") return sharedCozyDialogGradient
                if (modManager.theme === "Crimson") return sharedCrimsonDialogGradient
                if (modManager.theme === "Wavey") return sharedWaveyDialogGradient
                return null
            }

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.6)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 20
                shadowBlur: 1.0
            }
        }

        Overlay.modal: Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.85)

            Behavior on opacity {
                NumberAnimation { duration: 250 }
            }
        }

        enter: Transition {
            NumberAnimation {
                property: "scale"
                from: 0.9
                to: 1.0
                duration: 250
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 200
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
            id: resultDialogContent
            implicitWidth: 420
            implicitHeight: resultDialogColumn.implicitHeight + 64

            ColumnLayout {
                id: resultDialogColumn
                anchors.fill: parent
                anchors.margins: 32
                spacing: 20

                Item { Layout.preferredHeight: 8 }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 80
                    height: 80
                    radius: 40
                    color: "transparent"

                    Item {
                        anchors.centerIn: parent
                        width: 56
                        height: 56

                        Image {
                            id: resultIcon
                            anchors.centerIn: parent
                            width: 56
                            height: 56
                            source: resultDialog.isSuccess ?
                                   (modManager.theme === "Glassmorphic" ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/Checkmark.png" :
                                    modManager.theme === "Crimson" ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/Checkmark2.png" :
                                    modManager.theme === "Wavey" ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/Checkmark2.png" :
                                    "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/Checkmark.png")
                                   :
                                   (modManager.theme === "Glassmorphic" ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/failed2.png" :
                                    modManager.theme === "Crimson" ? "qrc:/UniversalModManager/Icons/Crimson Theme/General & Help/failed2.png" :
                                    modManager.theme === "Wavey" ? "qrc:/UniversalModManager/Icons/Wavey Theme/General & Help/failed2.png" :
                                    "qrc:/UniversalModManager/Icons/Cozy Theme/General & Help/failed.png")
                            fillMode: Image.PreserveAspectFit
                            sourceSize.width: 128
                            sourceSize.height: 128
                            smooth: true
                            antialiasing: true
                            mipmap: true
                            visible: modManager.theme !== "Glassmorphic"
                        }

                        ColorOverlay {
                            anchors.fill: resultIcon
                            source: resultIcon
                            color: modManager.theme === "Glassmorphic" ? window.primaryColor : "transparent"
                            visible: modManager.theme === "Glassmorphic"
                            smooth: true
                            antialiasing: true

                            layer.enabled: modManager.theme === "Glassmorphic"
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: window.primaryColor
                                shadowBlur: 0.7
                                shadowOpacity: 0.6
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: resultDialog.isSuccess ? "Success" : "Error"
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: resultDialog.resultMessage
                        font.pixelSize: 15
                        color: modManager.theme === "Glassmorphic" ? modManager.accentColor : window.textColor
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        lineHeight: 1.4
                    }
                }

                Item { Layout.fillHeight: true }

                Button {
                    id: okButton
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 140
                    Layout.preferredHeight: 48
                    flat: true
                    hoverEnabled: true

                    contentItem: Text {
                        text: "Okay"
                        font.pixelSize: 15
                        font.weight: Font.Bold
                        color: okButton.hovered ? "#FFFFFF" : window.textColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter

                        Behavior on color { ColorAnimation { duration: 200 } }
                    }

                    background: Rectangle {
                        color: okButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15)
                        radius: 24
                        border.width: 1.5
                        border.color: okButton.hovered ? window.primaryColor : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.4)

                        Behavior on color { ColorAnimation { duration: 200 } }
                        Behavior on border.color { ColorAnimation { duration: 200 } }
                    }

                    onClicked: resultDialog.close()
                }

                Item { Layout.preferredHeight: 8 }
            }
        }
    }

}
