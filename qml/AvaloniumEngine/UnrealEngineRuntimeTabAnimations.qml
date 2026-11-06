import QtQuick
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Item {
    id: root
    width: 200
    height: 60

    property bool running: true
    property string text: "Generating..."
    property int pixelSize: 24

    readonly property var activePalette: {
        var t = modManager.theme.toLowerCase()
        if (t === "glassmorphic") return themeConfig.glassmorphic
        if (t === "crimson") return themeConfig.crimson
        if (t === "wavey") return themeConfig.wavey
        return themeConfig.cozy
    }

    readonly property color colorTop: activePalette.generatingGradientTop || window.primaryColor
    readonly property color colorBottom: activePalette.generatingGradientBottom || window.accentColor

    property real waveOffset: 0

    NumberAnimation on waveOffset {
        loops: Animation.Infinite
        from: 0
        to: 2
        duration: 3000
        running: root.running
        easing.type: Easing.InOutSine
    }

    property real shimmerOffset: 0

    NumberAnimation on shimmerOffset {
        loops: Animation.Infinite
        from: 0
        to: 2
        duration: 2200
        running: root.running
        easing.type: Easing.InOutCubic
    }

    property real shinePosition: 0

    NumberAnimation on shinePosition {
        loops: Animation.Infinite
        from: -0.5
        to: 1.5
        duration: 4000
        running: root.running
        easing.type: Easing.InOutQuad
    }

    Text {
        id: textElement
        text: root.text
        font.pixelSize: root.pixelSize
        font.weight: Font.Bold
        font.letterSpacing: 2
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        anchors.fill: parent
        visible: false
    }

    LinearGradient {
        id: liquidGradient
        anchors.fill: parent
        source: textElement
        cached: true

        start: Qt.point(0, 0)
        end: Qt.point(0, parent.height)

        gradient: Gradient {
            GradientStop {
                position: Math.max(0, Math.min(1, (0.0 + root.waveOffset) % 2 / 2))
                color: Qt.lighter(root.colorTop, 1.4)
            }
            GradientStop {
                position: Math.max(0, Math.min(1, (0.25 + root.waveOffset) % 2 / 2))
                color: root.colorTop
            }
            GradientStop {
                position: Math.max(0, Math.min(1, (0.5 + root.waveOffset) % 2 / 2))
                color: root.colorBottom
            }
            GradientStop {
                position: Math.max(0, Math.min(1, (0.75 + root.waveOffset) % 2 / 2))
                color: Qt.lighter(root.colorBottom, 1.3)
            }
            GradientStop {
                position: Math.max(0, Math.min(1, (1.0 + root.waveOffset) % 2 / 2))
                color: Qt.rgba(1, 1, 1, 0.6)
            }
            GradientStop {
                position: Math.max(0, Math.min(1, (1.25 + root.waveOffset) % 2 / 2))
                color: root.colorTop
            }
            GradientStop {
                position: Math.max(0, Math.min(1, (1.5 + root.waveOffset) % 2 / 2))
                color: Qt.lighter(root.colorTop, 1.3)
            }
        }
    }

    Item {
        anchors.fill: parent
        visible: false

        LinearGradient {
            id: shimmerLayer
            anchors.fill: parent

            start: Qt.point(0, 0)
            end: Qt.point(0, parent.height)

            gradient: Gradient {
                GradientStop {
                    position: Math.max(0, Math.min(1, (0.0 + root.shimmerOffset) % 2 / 2))
                    color: "transparent"
                }
                GradientStop {
                    position: Math.max(0, Math.min(1, (0.4 + root.shimmerOffset) % 2 / 2))
                    color: Qt.rgba(1, 1, 1, 0.3)
                }
                GradientStop {
                    position: Math.max(0, Math.min(1, (0.6 + root.shimmerOffset) % 2 / 2))
                    color: Qt.rgba(1, 1, 1, 0.5)
                }
                GradientStop {
                    position: Math.max(0, Math.min(1, (1.0 + root.shimmerOffset) % 2 / 2))
                    color: "transparent"
                }
            }
        }
    }

    Blend {
        anchors.fill: parent
        source: liquidGradient
        foregroundSource: shimmerLayer
        mode: "lighten"
        cached: true
    }

    Glow {
        anchors.fill: parent
        source: liquidGradient
        radius: 16
        samples: 32
        color: Qt.lighter(root.colorBottom, 1.2)
        opacity: 0.5 * (0.3 + 0.7 * Math.abs(Math.sin(root.shinePosition * Math.PI)))
    }

    component HourglassAnimation : Item {
        id: hourglassRoot
        property bool running: true
        property int restartTrigger: 0

        readonly property real unit: width / 56.0
        readonly property int cycleDuration: 2000

        readonly property color orbitStrongColor: modManager.theme === "Glassmorphic"
            ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.88)
            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.82)
        readonly property color orbitSoftColor: modManager.theme === "Glassmorphic"
            ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.46)
            : Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g, window.textSecondaryColor.b, 0.62)
        readonly property color frameColor: modManager.theme === "Glassmorphic"
            ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.96)
            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.92)
        readonly property color frameShadowColor: modManager.theme === "Glassmorphic"
            ? Qt.rgba(0, 0, 0, 0.28)
            : Qt.rgba(0, 0, 0, 0.12)
        readonly property color glassColor: modManager.theme === "Glassmorphic"
            ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.08)
            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.035)

        readonly property color sandColor: window.accentColor
        readonly property color sandShadowColor: Qt.rgba(
            Qt.darker(window.accentColor, 1.4).r,
            Qt.darker(window.accentColor, 1.4).g,
            Qt.darker(window.accentColor, 1.4).b,
            0.55)

        property real cyclePhase: 0.0

        SequentialAnimation on cyclePhase {
            loops: Animation.Infinite
            running: hourglassRoot.running
            ScriptAction { script: hourglassRoot.cyclePhase = 0.0 }
            NumberAnimation {
                from: 0.0
                to: 1.0
                duration: hourglassRoot.cycleDuration
                easing.type: Easing.Linear
            }
        }

        Canvas {
            id: hgOrbitArcThick
            anchors.fill: parent
            opacity: 0.0
            rotation: 118

            property real startAngle: Math.PI * 0.96
            property real spanAngle: Math.PI * 0.34
            property real arcRadius: 24.5
            property real arcLineWidth: 2.5
            property color arcColor: hourglassRoot.orbitStrongColor

            onArcColorChanged: requestPaint()
            Component.onCompleted: requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.save()
                ctx.scale(hourglassRoot.unit, hourglassRoot.unit)
                ctx.strokeStyle = arcColor
                ctx.lineWidth = arcLineWidth
                ctx.lineCap = "round"
                ctx.beginPath()
                ctx.arc(28, 28, arcRadius, startAngle, startAngle + spanAngle, false)
                ctx.stroke()
                ctx.restore()
            }

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: hourglassRoot.running
                NumberAnimation { from: 0.0; to: 0.92; duration: 220; easing.type: Easing.OutCubic }
                NumberAnimation { from: 0.92; to: 0.0; duration: 580; easing.type: Easing.InCubic }
                PauseAnimation { duration: 1200 }
            }

            SequentialAnimation on rotation {
                loops: Animation.Infinite
                running: hourglassRoot.running
                NumberAnimation { from: 118; to: 188; duration: 400; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.33, 1.0, 0.67, 1.0] }
                NumberAnimation { from: 188; to: 256; duration: 400; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.33, 0.0, 0.67, 0.0] }
                PauseAnimation { duration: 1200 }
            }
        }

        Canvas {
            id: hgOrbitArcMedium
            anchors.fill: parent
            opacity: 0.0
            rotation: -154

            property real startAngle: Math.PI * 0.92
            property real spanAngle: Math.PI * 0.24
            property real arcRadius: 24.5
            property real arcLineWidth: 1.75
            property color arcColor: hourglassRoot.orbitSoftColor

            onArcColorChanged: requestPaint()
            Component.onCompleted: requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.save()
                ctx.scale(hourglassRoot.unit, hourglassRoot.unit)
                ctx.strokeStyle = arcColor
                ctx.lineWidth = arcLineWidth
                ctx.lineCap = "round"
                ctx.beginPath()
                ctx.arc(28, 28, arcRadius, startAngle, startAngle + spanAngle, false)
                ctx.stroke()
                ctx.restore()
            }

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: hourglassRoot.running
                PauseAnimation { duration: 160 }
                NumberAnimation { from: 0.0; to: 0.84; duration: 240; easing.type: Easing.OutCubic }
                NumberAnimation { from: 0.84; to: 0.0; duration: 240; easing.type: Easing.InCubic }
                PauseAnimation { duration: 1360 }
            }

            SequentialAnimation on rotation {
                loops: Animation.Infinite
                running: hourglassRoot.running
                PauseAnimation { duration: 160 }
                NumberAnimation { from: -154; to: -88; duration: 240; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.33, 1.0, 0.67, 1.0] }
                NumberAnimation { from: -88; to: -20; duration: 240; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.33, 0.0, 0.67, 0.0] }
                PauseAnimation { duration: 1360 }
            }
        }

        Canvas {
            id: hgOrbitArcThin
            anchors.fill: parent
            opacity: 0.0
            rotation: -18

            property real startAngle: Math.PI * 0.90
            property real spanAngle: Math.PI * 0.17
            property real arcRadius: 24.5
            property real arcLineWidth: 1.0
            property color arcColor: hourglassRoot.orbitSoftColor

            onArcColorChanged: requestPaint()
            Component.onCompleted: requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.save()
                ctx.scale(hourglassRoot.unit, hourglassRoot.unit)
                ctx.strokeStyle = arcColor
                ctx.lineWidth = arcLineWidth
                ctx.lineCap = "round"
                ctx.beginPath()
                ctx.arc(28, 28, arcRadius, startAngle, startAngle + spanAngle, false)
                ctx.stroke()
                ctx.restore()
            }

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: hourglassRoot.running
                PauseAnimation { duration: 80 }
                NumberAnimation { from: 0.0; to: 0.62; duration: 400; easing.type: Easing.OutCubic }
                NumberAnimation { from: 0.62; to: 0.0; duration: 400; easing.type: Easing.InCubic }
                PauseAnimation { duration: 1120 }
            }

            SequentialAnimation on rotation {
                loops: Animation.Infinite
                running: hourglassRoot.running
                PauseAnimation { duration: 80 }
                NumberAnimation { from: -18; to: 44; duration: 400; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.33, 1.0, 0.67, 1.0] }
                NumberAnimation { from: 44; to: 112; duration: 400; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.33, 0.0, 0.67, 0.0] }
                PauseAnimation { duration: 1120 }
            }
        }

        Item {
            id: hgModel
            anchors.fill: parent
            transformOrigin: Item.Center
            rotation: -180
            scale: 1.0

            SequentialAnimation on rotation {
                loops: Animation.Infinite
                running: hourglassRoot.running
                NumberAnimation { from: -180; to: 0; duration: 480; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.83, 0.0, 0.17, 1.0] }
                PauseAnimation { duration: 1520 }
            }

            SequentialAnimation on scale {
                loops: Animation.Infinite
                running: hourglassRoot.running
                NumberAnimation { from: 1.0; to: 0.97; duration: 240; easing.type: Easing.InOutCubic }
                NumberAnimation { from: 0.97; to: 1.0; duration: 240; easing.type: Easing.OutCubic }
                PauseAnimation { duration: 1520 }
            }

            Canvas {
                id: hgCanvas
                anchors.fill: parent

                property real topSandEmpty: 0.0
                property real bottomSandFill: 0.0
                property real streamProgress: 0.0

                property color frameColor: hourglassRoot.frameColor
                property color frameShadowColor: hourglassRoot.frameShadowColor
                property color glassColor: hourglassRoot.glassColor
                property color sandColor: hourglassRoot.sandColor
                property color sandShadowColor: hourglassRoot.sandShadowColor

                onTopSandEmptyChanged: requestPaint()
                onBottomSandFillChanged: requestPaint()
                onStreamProgressChanged: requestPaint()
                onFrameColorChanged: requestPaint()
                onFrameShadowColorChanged: requestPaint()
                onGlassColorChanged: requestPaint()
                onSandColorChanged: requestPaint()
                onSandShadowColorChanged: requestPaint()
                Component.onCompleted: requestPaint()

                SequentialAnimation on topSandEmpty {
                    loops: Animation.Infinite
                    running: hourglassRoot.running
                    ScriptAction { script: hgCanvas.topSandEmpty = 0.0 }
                    PauseAnimation { duration: 200 }
                    NumberAnimation { from: 0.0; to: 0.115; duration: 100; easing.type: Easing.Linear }
                    NumberAnimation { from: 0.115; to: 1.0; duration: 720; easing.type: Easing.Linear }
                    PauseAnimation { duration: 980 }
                }

                SequentialAnimation on bottomSandFill {
                    loops: Animation.Infinite
                    running: hourglassRoot.running
                    ScriptAction { script: hgCanvas.bottomSandFill = 0.0 }
                    PauseAnimation { duration: 620 }
                    NumberAnimation { from: 0.0; to: 1.0; duration: 500; easing.type: Easing.Linear }
                    PauseAnimation { duration: 880 }
                }

                SequentialAnimation on streamProgress {
                    loops: Animation.Infinite
                    running: hourglassRoot.running
                    ScriptAction { script: hgCanvas.streamProgress = 0.0 }
                    PauseAnimation { duration: 200 }
                    NumberAnimation { from: 0.0; to: 1.0; duration: 1200; easing.type: Easing.BezierSpline; easing.bezierCurve: [0.12, 0.0, 0.39, 0.0] }
                    PauseAnimation { duration: 600 }
                }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.save()
                    ctx.scale(hourglassRoot.unit, hourglassRoot.unit)

                    var ox = 15
                    var oy = 10
                    var w = 26
                    var h = 36
                    var cx = ox + w / 2
                    var topLipY = oy + 4
                    var waistY = oy + 18
                    var bottomLipY = oy + h - 4

                    var pTop = topSandEmpty
                    var pBottom = bottomSandFill
                    var pStream = streamProgress

                    function topChamberPath() {
                        ctx.beginPath()
                        ctx.moveTo(ox + 2, topLipY)
                        ctx.lineTo(ox + w - 2, topLipY)
                        ctx.bezierCurveTo(ox + w - 2, oy + 10, cx + 7, oy + 12.5, cx + 2, waistY - 1)
                        ctx.lineTo(cx + 1, waistY)
                        ctx.lineTo(cx - 1, waistY)
                        ctx.lineTo(cx - 2, waistY - 1)
                        ctx.bezierCurveTo(cx - 7, oy + 12.5, ox + 2, oy + 10, ox + 2, topLipY)
                        ctx.closePath()
                    }

                    function bottomChamberPath() {
                        ctx.beginPath()
                        ctx.moveTo(cx - 2, waistY + 1)
                        ctx.lineTo(cx + 2, waistY + 1)
                        ctx.bezierCurveTo(cx + 7, oy + 23.5, ox + w - 2, oy + 27, ox + w - 2, bottomLipY)
                        ctx.lineTo(ox + 2, bottomLipY)
                        ctx.bezierCurveTo(ox + 2, oy + 27, cx - 7, oy + 23.5, cx - 2, waistY + 1)
                        ctx.closePath()
                    }

                    function topInnerClipPath() {
                        ctx.beginPath()
                        ctx.moveTo(ox + 4.4, topLipY + 1.0)
                        ctx.lineTo(ox + w - 4.4, topLipY + 1.0)
                        ctx.bezierCurveTo(ox + w - 4.2, oy + 10.2, cx + 6.0, oy + 12.1, cx + 1.6, waistY - 1.6)
                        ctx.lineTo(cx - 1.6, waistY - 1.6)
                        ctx.bezierCurveTo(cx - 6.0, oy + 12.1, ox + 4.2, oy + 10.2, ox + 4.4, topLipY + 1.0)
                        ctx.closePath()
                    }

                    function bottomInnerClipPath() {
                        ctx.beginPath()
                        ctx.moveTo(cx - 1.6, waistY + 1.6)
                        ctx.lineTo(cx + 1.6, waistY + 1.6)
                        ctx.bezierCurveTo(cx + 6.0, oy + 23.1, ox + w - 4.2, oy + 26.8, ox + w - 4.4, bottomLipY - 1.0)
                        ctx.lineTo(ox + 4.4, bottomLipY - 1.0)
                        ctx.bezierCurveTo(ox + 4.2, oy + 26.8, cx - 6.0, oy + 23.1, cx - 1.6, waistY + 1.6)
                        ctx.closePath()
                    }

                    ctx.fillStyle = frameColor
                    ctx.fillRect(ox - 1, oy + 1, w + 2, 4)
                    ctx.fillRect(ox - 1, oy + h - 5, w + 2, 4)

                    ctx.save()
                    topInnerClipPath()
                    ctx.clip()
                    ctx.fillStyle = glassColor
                    ctx.fillRect(ox + 3.8, oy + 5.0, w - 7.6, waistY - oy - 5.6)

                    if (pTop < 1.0) {
                        var topSurfaceY = topLipY + 0.6 + pTop * 13.4
                        ctx.fillStyle = sandColor
                        ctx.fillRect(ox + 2.9, topSurfaceY, w - 5.8, waistY - topSurfaceY + 1.0)

                        ctx.fillStyle = sandShadowColor
                        ctx.fillRect(ox + 3.7, topSurfaceY + 1.1, w - 7.4, Math.max(0, waistY - topSurfaceY - 0.6))

                        ctx.strokeStyle = Qt.rgba(sandColor.r, sandColor.g, sandColor.b, 0.95)
                        ctx.lineWidth = 0.95
                        ctx.beginPath()
                        ctx.moveTo(ox + 3.5, topSurfaceY + 0.3)
                        ctx.quadraticCurveTo(cx, topSurfaceY - 0.65, ox + w - 3.5, topSurfaceY + 0.3)
                        ctx.stroke()
                    }
                    ctx.restore()

                    ctx.save()
                    bottomInnerClipPath()
                    ctx.clip()
                    ctx.fillStyle = glassColor
                    ctx.fillRect(ox + 3.8, waistY + 1.8, w - 7.6, h - (waistY - oy) - 4.6)

                    if (pBottom > 0.0) {
                        var bottomHeight = pBottom * 12.2
                        var moundTopY = bottomLipY - bottomHeight

                        var moundScale = Math.min(1.0, pBottom * 2.0)

                        ctx.save()
                        ctx.translate(cx, bottomLipY)
                        ctx.scale(moundScale, 1.0)
                        ctx.translate(-cx, -bottomLipY)

                        ctx.fillStyle = sandColor
                        ctx.fillRect(ox + 2.9, moundTopY, w - 5.8, bottomLipY - moundTopY)

                        ctx.fillStyle = sandShadowColor
                        ctx.fillRect(ox + 3.8, moundTopY + 1.0, w - 7.6, Math.max(0, bottomLipY - moundTopY - 1.0))

                        ctx.strokeStyle = Qt.rgba(sandColor.r, sandColor.g, sandColor.b, 0.92)
                        ctx.lineWidth = 1.0
                        ctx.beginPath()
                        ctx.moveTo(ox + 3.6, moundTopY + 0.25)
                        ctx.quadraticCurveTo(cx, moundTopY - 1.0, ox + w - 3.6, moundTopY + 0.25)
                        ctx.stroke()
                        ctx.restore()
                    }
                    ctx.restore()

                    ctx.strokeStyle = frameColor
                    ctx.lineWidth = 2.0
                    ctx.lineJoin = "round"
                    topChamberPath()
                    ctx.stroke()
                    bottomChamberPath()
                    ctx.stroke()

                    ctx.lineWidth = 2.3
                    ctx.beginPath()
                    ctx.moveTo(cx, waistY - 0.5)
                    ctx.lineTo(cx, waistY + 2.4)
                    ctx.stroke()

                    if (pStream > 0.0 && pStream < 0.85) {
                        var streamTop = waistY + 1.2
                        var streamBottom = bottomLipY - 1.0

                        var currentBottom = streamTop
                        var currentTop = streamTop

                        if (pStream < 0.15) {
                            currentBottom = streamTop + (streamBottom - streamTop) * (pStream / 0.15)
                        } else {
                            currentBottom = streamBottom
                        }

                        if (pStream > 0.50) {
                            currentTop = streamTop + (streamBottom - streamTop) * ((pStream - 0.50) / 0.35)
                        }

                        if (currentBottom > currentTop) {
                            ctx.strokeStyle = Qt.rgba(sandColor.r, sandColor.g, sandColor.b, 0.95)
                            ctx.lineWidth = 1.4
                            ctx.lineCap = "round"
                            ctx.beginPath()
                            ctx.moveTo(cx, currentTop)
                            ctx.lineTo(cx, currentBottom)
                            ctx.stroke()

                            var grainTravel = currentTop + (currentBottom - currentTop) * pStream
                            if (grainTravel > currentTop + 4 && grainTravel < currentBottom - 2) {
                                ctx.strokeStyle = Qt.rgba(sandColor.r, sandColor.g, sandColor.b, 0.8)
                                ctx.lineWidth = 1.0
                                ctx.beginPath()
                                ctx.moveTo(cx - 0.3, grainTravel - 2)
                                ctx.lineTo(cx - 0.3, grainTravel - 1)
                                ctx.moveTo(cx + 0.3, grainTravel)
                                ctx.lineTo(cx + 0.3, grainTravel + 1)
                                ctx.stroke()
                            }
                        }
                    }

                    ctx.strokeStyle = frameShadowColor
                    ctx.lineWidth = 1.0
                    ctx.beginPath()
                    ctx.moveTo(ox + 4, topLipY + 0.8)
                    ctx.lineTo(ox + w - 4, topLipY + 0.8)
                    ctx.moveTo(ox + 4, bottomLipY - 0.8)
                    ctx.lineTo(ox + w - 4, bottomLipY - 0.8)
                    ctx.stroke()
                    ctx.restore()
                }
            }
        }
    }

    component BinaryRainAnimation : Item {
        id: binaryRainRoot
        property bool running: true
        property int restartTrigger: 0

        clip: false

        Canvas {
            id: rainCanvas
            anchors.fill: parent

            property var cols: []
            property int charSize: 13

            property int numCols: Math.max(1, Math.floor(width  / charSize))
            property int numRows: Math.max(1, Math.floor(height / charSize)) + 4

            property color headColor:  Qt.rgba(1, 1, 1, 1)
            property color trailColor: window.primaryColor

            function buildCols() {
                var arr = []
                for (var i = 0; i < numCols; i++) {
                    var trail = 5 + Math.floor(Math.random() * 9)
                    var chars = []
                    for (var j = 0; j < numRows + trail + 2; j++)
                        chars.push(Math.random() > 0.5 ? "1" : "0")
                    arr.push({
                        head:     -(Math.floor(Math.random() * numRows)),
                        speed:    0.25 + Math.random() * 0.55,
                        trailLen: trail,
                        chars:    chars
                    })
                }
                cols = arr
            }

            Component.onCompleted: buildCols()
            onNumColsChanged: buildCols()

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                ctx.font         = "bold " + (charSize - 1) + "px 'Cascadia Mono','JetBrains Mono',monospace"
                ctx.textAlign    = "left"
                ctx.textBaseline = "top"

                var tc = trailColor
                var hc = headColor

                for (var i = 0; i < cols.length; i++) {
                    var col  = cols[i]
                    var head = Math.floor(col.head)

                    for (var j = 0; j <= col.trailLen; j++) {
                        var row = head - j
                        if (row < 0 || row >= numRows) continue

                        var t = j / col.trailLen
                        var a

                        if (j === 0) {
                            a = 1.0
                            ctx.fillStyle = "rgba(" + Math.round(hc.r*255) + ","
                                                    + Math.round(hc.g*255) + ","
                                                    + Math.round(hc.b*255) + ","
                                                    + a + ")"
                        } else {
                            a = Math.pow(1.0 - t, 1.8) * 0.92
                            if (a < 0.03) continue
                            ctx.fillStyle = "rgba(" + Math.round(tc.r*255) + ","
                                                    + Math.round(tc.g*255) + ","
                                                    + Math.round(tc.b*255) + ","
                                                    + a + ")"
                        }

                        ctx.fillText(
                            col.chars[row] || "0",
                            i * charSize + 1,
                            row * charSize
                        )
                    }
                }
            }
        }

        Canvas {
            id: glowCanvas
            anchors.fill: parent
            opacity: 0.5

            layer.enabled: true
            layer.effect: MultiEffect {
                blurEnabled:    true
                blur:           0.55
                blurMax:        20
                blurMultiplier: 1.0
            }

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                ctx.font         = "bold " + (rainCanvas.charSize - 1) + "px 'Cascadia Mono','JetBrains Mono',monospace"
                ctx.textAlign    = "left"
                ctx.textBaseline = "top"

                var cs   = rainCanvas.charSize
                var tc   = rainCanvas.trailColor
                var hc   = rainCanvas.headColor
                var cols = rainCanvas.cols

                for (var i = 0; i < cols.length; i++) {
                    var col  = cols[i]
                    var head = Math.floor(col.head)

                    for (var j = 0; j <= Math.min(3, col.trailLen); j++) {
                        var row = head - j
                        if (row < 0 || row >= rainCanvas.numRows) continue
                        var a = (j === 0) ? 1.0 : Math.max(0, 0.65 - j * 0.18)
                        var c = (j === 0) ? hc : tc
                        ctx.fillStyle = "rgba(" + Math.round(c.r*255) + ","
                                                + Math.round(c.g*255) + ","
                                                + Math.round(c.b*255) + ","
                                                + a + ")"
                        ctx.fillText(col.chars[row] || "0", i * cs + 1, row * cs)
                    }
                }
            }
        }

        Timer {
            id: rainTick
            interval: 45
            repeat:   true
            running:  binaryRainRoot.running

            onTriggered: {
                var cols    = rainCanvas.cols
                var numRows = rainCanvas.numRows

                for (var i = 0; i < cols.length; i++) {
                    var col = cols[i]
                    col.head += col.speed

                    if (Math.random() > 0.70) {
                        var r = Math.floor(col.head) - Math.floor(Math.random() * col.trailLen)
                        if (r >= 0 && r < col.chars.length)
                            col.chars[r] = Math.random() > 0.5 ? "1" : "0"
                    }

                    if (Math.random() > 0.52) {
                        var h = Math.floor(col.head)
                        if (h >= 0 && h < col.chars.length)
                            col.chars[h] = Math.random() > 0.5 ? "1" : "0"
                    }

                    if (col.head > numRows + col.trailLen + 1) {
                        col.head     = -(Math.floor(Math.random() * 6))
                        col.speed    = 0.25 + Math.random() * 0.55
                        col.trailLen = 5 + Math.floor(Math.random() * 9)
                        for (var j = 0; j < col.chars.length; j++)
                            col.chars[j] = Math.random() > 0.5 ? "1" : "0"
                    }
                }

                rainCanvas.requestPaint()
                glowCanvas.requestPaint()
            }
        }

        onRunningChanged:        { if (running) rainCanvas.buildCols() }
        onRestartTriggerChanged: rainCanvas.buildCols()
    }
}
