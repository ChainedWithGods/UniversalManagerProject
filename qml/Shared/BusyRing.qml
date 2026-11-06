import QtQuick

Item {
    id: root

    property bool running: false
    property color ringColor: window.primaryColor
    property real ringWidth: 1.5

    width: 20
    height: 20
    visible: running

    onRunningChanged: ring.requestPaint()
    onRingColorChanged: ring.requestPaint()
    onVisibleChanged: if (visible) ring.requestPaint()

    Canvas {
        id: ring
        anchors.fill: parent
        antialiasing: true
        transformOrigin: Item.Center

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root.ringColor
            ctx.lineWidth = root.ringWidth
            ctx.lineCap = "round"
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, width / 2 - root.ringWidth - 0.5,
                    -Math.PI / 2, Math.PI * 0.9)
            ctx.stroke()
        }

        RotationAnimator on rotation {
            running: root.running && !window.focusAnimationsPaused
            from: 0
            to: 360
            duration: 1000
            loops: Animation.Infinite
            onRunningChanged: if (!running) ring.rotation = 0
        }
    }
}
