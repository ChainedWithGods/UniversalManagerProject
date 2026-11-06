import QtQuick

Item {
    id: badge

    property bool paused: false

    implicitWidth: 90
    implicitHeight: 28

    readonly property color accent: themeConfig.wavey.accentColor
    readonly property color cyan: themeConfig.wavey.ucasColor
    readonly property color violet: themeConfig.wavey.utocColor

    property real phaseA: 0
    property real phaseB: 0
    property real phaseC: 0

    NumberAnimation on phaseA {
        from: 0
        to: 1
        duration: 3400
        loops: Animation.Infinite
        running: badge.visible && !badge.paused
    }

    NumberAnimation on phaseB {
        from: 0
        to: 1
        duration: 1400
        loops: Animation.Infinite
        running: badge.visible && !badge.paused
    }

    NumberAnimation on phaseC {
        from: 0
        to: 1
        duration: 4600
        loops: Animation.Infinite
        running: badge.visible && !badge.paused
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        border.width: 1
        border.color: Qt.rgba(badge.accent.r, badge.accent.g, badge.accent.b, 0.55)

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1B3E68" }
            GradientStop { position: 1.0; color: "#0A1E36" }
        }
    }

    Canvas {
        id: waterCanvas
        anchors.fill: parent
        renderStrategy: Canvas.Immediate

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var w = width
            var h = height

            var inset = 1
            var radius = h / 2 - inset
            ctx.save()
            ctx.beginPath()
            ctx.moveTo(inset + radius, inset)
            ctx.lineTo(w - inset - radius, inset)
            ctx.arc(w - inset - radius, h / 2, radius, -Math.PI / 2, Math.PI / 2)
            ctx.lineTo(inset + radius, h - inset)
            ctx.arc(inset + radius, h / 2, radius, Math.PI / 2, Math.PI * 1.5)
            ctx.closePath()
            ctx.clip()

            function drawLayer(baseY, wavelength, amplitude, driftPhase, r, g, b, alpha) {
                var drift = driftPhase * wavelength
                var crestTop = baseY - amplitude
                var crestBottom = baseY + amplitude + 6

                ctx.beginPath()
                ctx.moveTo(0, h)
                for (var x = 0; x <= w; x += 1) {
                    var y = baseY + Math.sin(((x + drift) / wavelength) * Math.PI * 2) * amplitude
                    ctx.lineTo(x, y)
                }
                ctx.lineTo(w, h)
                ctx.closePath()

                var fill = ctx.createLinearGradient(0, crestTop, 0, crestBottom)
                fill.addColorStop(0, "rgba(" + r + "," + g + "," + b + ",0)")
                fill.addColorStop(0.42, "rgba(" + r + "," + g + "," + b + "," + (alpha * 0.45).toFixed(3) + ")")
                fill.addColorStop(1, "rgba(" + r + "," + g + "," + b + "," + alpha + ")")
                ctx.fillStyle = fill
                ctx.fill()
            }

            drawLayer(15, 20, 4.0, badge.phaseA, 96, 165, 250, 0.90)
            drawLayer(19, 11, 2.5, 1.0 - badge.phaseB, 34, 211, 238, 0.55)
            drawLayer(22, 15, 3.0, badge.phaseC, 129, 140, 248, 0.42)

            ctx.restore()
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()
    }

    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 1
        text: "ACTIVE"
        font.pixelSize: 10
        font.weight: Font.ExtraBold
        font.letterSpacing: 1.5
        color: Qt.rgba(0, 0.04, 0.1, 0.65)
    }

    Text {
        anchors.centerIn: parent
        text: "ACTIVE"
        font.pixelSize: 10
        font.weight: Font.ExtraBold
        font.letterSpacing: 1.5
        color: "#EAF4FF"
    }

    onPhaseAChanged: waterCanvas.requestPaint()
    onPhaseBChanged: waterCanvas.requestPaint()
    onPhaseCChanged: waterCanvas.requestPaint()
}
