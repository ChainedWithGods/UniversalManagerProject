import QtQuick

Item {
    id: badge

    property bool paused: false

    implicitWidth: 90
    implicitHeight: 28

    property real phase: 0

    function bezierEase(x) {
        if (x <= 0)
            return 0
        if (x >= 1)
            return 1
        var lo = 0
        var hi = 1
        var u = 0
        for (var i = 0; i < 24; i++) {
            u = (lo + hi) / 2
            var xu = 3 * (1 - u) * (1 - u) * u * 0.35 + 3 * (1 - u) * u * u * 0.15 + u * u * u
            if (xu < x)
                lo = u
            else
                hi = u
        }
        u = (lo + hi) / 2
        return 3 * (1 - u) * u * u + u * u * u
    }

    function smoothstep(x) {
        var s = Math.max(0, Math.min(1, x))
        return s * s * (3 - 2 * s)
    }

    function flashAmount(t) {
        if (t < 0.18)
            return smoothstep(t / 0.18)
        if (t < 0.45)
            return 1 - smoothstep((t - 0.18) / 0.27)
        return 0
    }

    property real flash: flashAmount(phase)

    NumberAnimation on phase {
        from: 0
        to: 1
        duration: 2800
        loops: Animation.Infinite
        running: badge.visible && !badge.paused
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        renderStrategy: Canvas.Immediate

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var w = width
            var h = height
            var cut = 7
            var mid = h / 2
            var t = badge.phase

            function tagPath() {
                ctx.beginPath()
                ctx.moveTo(cut, 0.5)
                ctx.lineTo(w - cut, 0.5)
                ctx.lineTo(w - 0.5, mid)
                ctx.lineTo(w - cut, h - 0.5)
                ctx.lineTo(cut, h - 0.5)
                ctx.lineTo(0.5, mid)
                ctx.closePath()
            }

            tagPath()
            var fill = ctx.createLinearGradient(0, 0, 0, h)
            fill.addColorStop(0, "rgba(170, 36, 70, 0.32)")
            fill.addColorStop(1, "rgba(120, 18, 44, 0.22)")
            ctx.fillStyle = fill
            ctx.fill()

            ctx.save()
            tagPath()
            ctx.clip()

            var bandWidth = w * 0.42
            var bandTop = -0.45 * h
            var bandBottom = 1.45 * h
            var skew = (bandBottom - bandTop) * Math.tan(16 * Math.PI / 180)
            var bandX = -1.8 * bandWidth + badge.bezierEase(Math.min(1, t / 0.55)) * (6.0 * bandWidth)

            var sheen = ctx.createLinearGradient(bandX, 0, bandX + bandWidth, 0)
            sheen.addColorStop(0, "rgba(255, 196, 196, 0)")
            sheen.addColorStop(0.25, "rgba(255, 196, 196, 0.30)")
            sheen.addColorStop(0.5, "rgba(255, 255, 255, 0.58)")
            sheen.addColorStop(0.75, "rgba(255, 196, 196, 0.26)")
            sheen.addColorStop(1, "rgba(255, 196, 196, 0)")
            ctx.beginPath()
            ctx.moveTo(bandX + skew / 2, bandTop)
            ctx.lineTo(bandX + bandWidth + skew / 2, bandTop)
            ctx.lineTo(bandX + bandWidth - skew / 2, bandBottom)
            ctx.lineTo(bandX - skew / 2, bandBottom)
            ctx.closePath()
            ctx.fillStyle = sheen
            ctx.fill()

            var lineWidth = w * 0.36
            var lineX
            if (t < 0.08)
                lineX = -1.2 * lineWidth
            else if (t < 0.55)
                lineX = -1.2 * lineWidth + badge.bezierEase((t - 0.08) / 0.47) * (4.0 * lineWidth)
            else
                lineX = 2.8 * lineWidth

            var lineOpacity = 0
            if (t < 0.08)
                lineOpacity = t / 0.08
            else if (t < 0.55)
                lineOpacity = 1
            else if (t < 0.62)
                lineOpacity = 1 - (t - 0.55) / 0.07

            if (lineOpacity > 0) {
                var line = ctx.createLinearGradient(lineX, 0, lineX + lineWidth, 0)
                line.addColorStop(0, "rgba(242, 176, 176, 0)")
                line.addColorStop(0.33, "rgba(242, 176, 176, 0.95)")
                line.addColorStop(0.5, "rgba(255, 225, 225, 1)")
                line.addColorStop(0.67, "rgba(242, 176, 176, 0.95)")
                line.addColorStop(1, "rgba(242, 176, 176, 0)")
                ctx.globalAlpha = lineOpacity
                ctx.fillStyle = line
                ctx.fillRect(lineX, h - 1.5, lineWidth, 1.5)
                ctx.globalAlpha = 1
            }

            ctx.restore()

            var f = badge.flash
            tagPath()
            ctx.lineWidth = 1
            ctx.strokeStyle = "rgba(" + Math.round(199 + 41 * f) + ", "
                                       + Math.round(53 + 97 * f) + ", "
                                       + Math.round(88 + 62 * f) + ", "
                                       + (0.55 + 0.40 * f).toFixed(3) + ")"
            ctx.stroke()

            ctx.strokeStyle = "rgba(240, 163, 163, 0.22)"
            ctx.beginPath()
            ctx.moveTo(cut + 1, h - 0.5)
            ctx.lineTo(w - cut - 1, h - 0.5)
            ctx.stroke()
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()
    }

    Text {
        anchors.centerIn: parent
        text: "ACTIVE"
        font.pixelSize: 10
        font.weight: Font.Bold
        font.letterSpacing: 1.4
        color: "#FFE9EA"
        renderType: Text.QtRendering
    }

    Text {
        anchors.centerIn: parent
        text: "ACTIVE"
        font.pixelSize: 10
        font.weight: Font.Bold
        font.letterSpacing: 1.4
        color: "#FFFFFF"
        opacity: badge.flash
        renderType: Text.QtRendering
    }

    onPhaseChanged: canvas.requestPaint()
}
