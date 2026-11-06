import QtQuick
import QtQuick.Window

Window {
    id: splashWindow
    width: 760
    height: 510
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2
    flags: Qt.SplashScreen | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    visible: true

    property bool mainLoaded: false
    property bool transitionStarted: false
    property bool fadeOutComplete: false
    property bool mainWindowVisible: false
    property var pendingMainWindow: null

    function tryCloseSplash() {
        if (fadeOutComplete && mainWindowVisible) {
            splashWindow.visible = false
            splashWindow.close()
        }
    }

    function showMainWindowIfReady() {
        if (!fadeOutComplete) return
        if (!pendingMainWindow) return
        if (mainWindowVisible) return

        pendingMainWindow.show()
        mainWindowVisible = true
        tryCloseSplash()
    }

    function loadMainApp() {
        if (mainLoaded) return
        mainLoaded = true

        if (typeof qmlEngine !== "undefined" && qmlEngine) {
            qmlEngine.load("qrc:/UniversalModManager/qml/main.qml")
            return
        }
        var component = Qt.createComponent("qrc:/UniversalModManager/qml/main.qml")
        if (component.status === Component.Ready) {
            var mainWindow = component.createObject(null)
            if (mainWindow) {
                pendingMainWindow = mainWindow
                if (pendingMainWindow.hasOwnProperty("visible")) {
                    pendingMainWindow.visible = false
                }
                showMainWindowIfReady()
            } else {
                console.error("Error creating main window object")
            }
        } else {
            console.error("Error loading main.qml:", component.errorString())
            component.statusChanged.connect(function() {
                if (component.status === Component.Ready) {
                    var mainWindow = component.createObject(null)
                    pendingMainWindow = mainWindow
                    if (pendingMainWindow && pendingMainWindow.hasOwnProperty("visible")) {
                        pendingMainWindow.visible = false
                    }
                    showMainWindowIfReady()
                } else if (component.status === Component.Error) {
                    console.error("Error loading main.qml:", component.errorString())
                }
            })
        }
    }

    Image {
        id: splashImage
        anchors.fill: parent
        source: "qrc:/UniversalModManager/assets/Splash.png"
        fillMode: Image.PreserveAspectFit
        smooth: true

        property bool appeared: false
        opacity: splashWindow.transitionStarted ? 0.0 : (appeared ? 1.0 : 0.0)

        Behavior on opacity {
            NumberAnimation { duration: 400; easing.type: Easing.InOutQuad }
        }

        Timer {
            id: splashTimer
            interval: 1200
            onTriggered: {
                splashWindow.transitionStarted = true
                loadMainApp()
                showTimer.start()
            }
        }

        Timer {
            id: showTimer
            interval: 400
            onTriggered: {
                fadeOutComplete = true
                showMainWindowIfReady()
                tryCloseSplash()
            }
        }

        Component.onCompleted: {
            splashImage.appeared = true
            splashTimer.start()
        }
    }

    Connections {
        target: (typeof qmlEngine !== "undefined" && qmlEngine) ? qmlEngine : null
        function onObjectCreated(obj, objUrl) {
            if (!obj || !objUrl) return
            var urlString = objUrl.toString()
            if (urlString.endsWith("/qml/main.qml") || urlString.endsWith("main.qml")) {
                pendingMainWindow = obj
                if (pendingMainWindow && pendingMainWindow.hasOwnProperty("visible")) {
                    pendingMainWindow.visible = false
                }
                showMainWindowIfReady()
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: true
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onPressed: function(mouse) { mouse.accepted = true }
        onReleased: function(mouse) { mouse.accepted = true }
        onClicked: function(mouse) { mouse.accepted = true }
        onDoubleClicked: function(mouse) { mouse.accepted = true }
    }
}
