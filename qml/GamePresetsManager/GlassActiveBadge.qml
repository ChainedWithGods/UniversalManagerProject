import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Item {
    id: badge

    property bool paused: false

    implicitWidth: 90
    implicitHeight: 28

    readonly property color accent: modManager.accentColor
    readonly property color wordColor: Qt.rgba(
        accent.r * 0.62 + 0.38,
        accent.g * 0.62 + 0.38,
        accent.b * 0.62 + 0.38,
        1.0)

    property real phase: 0

    property int laps: 0
    property real lastPhase: 0

    onPhaseChanged: {
        if (phase < lastPhase)
            laps++
        lastPhase = phase
        lightCanvas.requestPaint()
    }

    NumberAnimation on phase {
        from: 0
        to: 1
        duration: 2200
        loops: Animation.Infinite
        running: badge.visible && !badge.paused
        easing.type: Easing.InOutQuart
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        border.width: 1
        border.color: Qt.rgba(badge.accent.r, badge.accent.g, badge.accent.b, 0.24)

        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Qt.rgba(0.15 + badge.accent.r * 0.05,
                               0.14 + badge.accent.g * 0.05,
                               0.18 + badge.accent.b * 0.05,
                               0.94)
            }
            GradientStop {
                position: 0.52
                color: Qt.rgba(0.12 + badge.accent.r * 0.04,
                               0.11 + badge.accent.g * 0.04,
                               0.15 + badge.accent.b * 0.04,
                               0.94)
            }
            GradientStop {
                position: 1.0
                color: Qt.rgba(0.09 + badge.accent.r * 0.03,
                               0.08 + badge.accent.g * 0.03,
                               0.12 + badge.accent.b * 0.03,
                               0.94)
            }
        }
    }

    Canvas {
        id: lightCanvas
        anchors.fill: parent
        renderStrategy: Canvas.Immediate

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var w = width
            var h = height
            var r = h / 2
            var straight = w - 2 * r
            var arc = Math.PI * r
            var perimeter = 2 * straight + 2 * arc

            function pointAt(t) {
                var u = ((t % 1) + 1) % 1
                var s = u * perimeter
                if (s < straight)
                    return [r + s, 0.5]
                if (s < straight + arc) {
                    var a = -Math.PI / 2 + ((s - straight) / arc) * Math.PI
                    return [w - r + r * Math.cos(a), r + r * Math.sin(a)]
                }
                if (s < 2 * straight + arc)
                    return [w - r - (s - straight - arc), h - 0.5]
                var b = Math.PI / 2 + ((s - 2 * straight - arc) / arc) * Math.PI
                return [r + r * Math.cos(b), r + r * Math.sin(b)]
            }

            var steps = 30
            var tail = 0.24
            for (var i = 0; i < steps; i++) {
                var t0 = badge.phase - tail + tail * (i / steps)
                var t1 = badge.phase - tail + tail * ((i + 1) / steps)
                var p0 = pointAt(t0)
                var p1 = pointAt(t1)
                var k = i / (steps - 1)
                ctx.strokeStyle = "rgba(255, 255, 255, " + (0.9 * k * k + 0.05 * k).toFixed(3) + ")"
                ctx.lineWidth = 1
                ctx.beginPath()
                ctx.moveTo(p0[0], p0[1])
                ctx.lineTo(p1[0], p1[1])
                ctx.stroke()
            }

            var head0 = pointAt(badge.phase - 0.006)
            var head1 = pointAt(badge.phase)
            ctx.strokeStyle = "#FFFFFF"
            ctx.lineWidth = 1.4
            ctx.beginPath()
            ctx.moveTo(head0[0], head0[1])
            ctx.lineTo(head1[0], head1[1])
            ctx.stroke()

            var lead = 0.05
            var fSteps = 6
            for (var f = 0; f < fSteps; f++) {
                var u0 = badge.phase + lead * (f / fSteps)
                var u1 = badge.phase + lead * ((f + 1) / fSteps)
                var q0 = pointAt(u0)
                var q1 = pointAt(u1)
                ctx.strokeStyle = "rgba(255, 255, 255, " + (0.5 * (1 - f / fSteps)).toFixed(3) + ")"
                ctx.lineWidth = 1
                ctx.beginPath()
                ctx.moveTo(q0[0], q0[1])
                ctx.lineTo(q1[0], q1[1])
                ctx.stroke()
            }
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()
    }

    RowLayout {
        anchors.centerIn: parent
        spacing: 6

        Item {
            Layout.preferredWidth: 13
            Layout.preferredHeight: 13
            Layout.alignment: Qt.AlignVCenter

            Image {
                id: starImage
                anchors.fill: parent
                source: "qrc:/UniversalModManager/Icons/Glassmorphic Theme/General & Help/sparkle.svg"
                sourceSize.width: 26
                sourceSize.height: 26
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: false
            }

            ColorOverlay {
                anchors.fill: starImage
                source: starImage
                color: Qt.rgba(badge.accent.r, badge.accent.g, badge.accent.b, 0.85)
                smooth: true
            }

            rotation: (badge.laps * 270) + badge.phase * 270
        }

        Text {
            text: "ACTIVE"
            font.pixelSize: 10
            font.weight: Font.ExtraBold
            font.letterSpacing: 1.5
            color: badge.wordColor
            renderType: Text.QtRendering
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
