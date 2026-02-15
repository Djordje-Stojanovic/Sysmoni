import QtQuick 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: pinkMode ? "#1d0c16" : "#080e18"
    radius: 10
    clip: true

    // ── C++ bridge properties ────────────────────────────────────────────────
    property real accentIntensity: 0.0
    property real cpuPercent: 0.0
    property real memoryPercent: 0.0
    property real accentRed: 0.20
    property real accentGreen: 0.45
    property real accentBlue: 0.75
    property real accentAlpha: 0.20
    property real frostIntensity: 0.25
    property real tintStrength: 0.35
    property real ringLineWidth: 2.0
    property real ringGlowStrength: 0.25
    property real cpuAlpha: 0.70
    property real memoryAlpha: 0.70
    property int severityLevel: 0
    property real motionScale: 1.0
    property int qualityHint: 0
    property real timelineAnomalyAlpha: 0.05
    property string statusText: "Waiting for telemetry..."
    property string themeMode: "dark_blue"
    property bool pinkMode: themeMode === "pink_cute"
    property bool hasCpuSample: false
    property bool hasMemSample: false

    // ── Smoothed animated values used by Canvas gauges ───────────────────────
    property real smoothCpu: 0.0
    property real smoothMem: 0.0

    onCpuPercentChanged: {
        if (!hasCpuSample) {
            smoothCpu = cpuPercent
            hasCpuSample = true
        } else {
            smoothCpu = (smoothCpu * 0.85) + (cpuPercent * 0.15)
        }
        cpuSparkCanvas.pushSample(smoothCpu)
    }
    onMemoryPercentChanged: {
        if (!hasMemSample) {
            smoothMem = memoryPercent
            hasMemSample = true
        } else {
            smoothMem = (smoothMem * 0.85) + (memoryPercent * 0.15)
        }
        memSparkCanvas.pushSample(smoothMem)
    }

    // ── Color tokens ─────────────────────────────────────────────────────────
    readonly property color clrBgDeep: pinkMode ? "#150a12" : "#060b14"
    readonly property color clrAccent: pinkMode ? "#ff4da6" : "#3b82f6"
    readonly property color clrAccentHover: pinkMode ? "#ff92c5" : "#60a5fa"
    readonly property color clrTextPrimary: pinkMode ? "#ffe8f5" : "#e0ecf7"
    readonly property color clrTextMuted: pinkMode ? "#d887b1" : "#4d6d87"
    readonly property color clrTextSecondary: pinkMode ? "#ffb3d9" : "#8badc4"
    readonly property color clrTrack: pinkMode ? "#5f2a4b" : "#1a2940"
    readonly property color clrTrackTick: pinkMode ? "#3a1529" : "#0c1829"
    readonly property color clrBgGradTop: pinkMode ? "#3a1529" : "#0c1829"
    readonly property color clrBgGradMid: pinkMode ? "#2a1021" : "#080e18"
    readonly property color clrBgGradBot: pinkMode ? "#1a0915" : "#050a11"

    // ── Derived accent color helpers ─────────────────────────────────────────
    function accentColor(alpha) {
        return Qt.rgba(accentRed, accentGreen, accentBlue, alpha)
    }

    function uiColor(blueHex, pinkHex) {
        return pinkMode ? pinkHex : blueHex
    }

    function severityColor(level, alpha) {
        if (level >= 3) return Qt.rgba(0.93, 0.27, 0.27, alpha)
        if (level === 2) return Qt.rgba(0.96, 0.62, 0.04, alpha)
        if (level === 1) return Qt.rgba(0.93, 0.73, 0.12, alpha)
        return Qt.rgba(accentRed, accentGreen, accentBlue, alpha)
    }

    // Gauge arc color: 0-50 = blue>cyan, 50-80 = cyan>amber, 80-100 = amber>red
    function gaugeColor(pct, alpha) {
        var t, r, g, b
        if (pinkMode) {
            if (pct <= 50) {
                t = pct / 50.0
                r = 0.961 + (0.933 - 0.961) * t
                g = 0.361 + (0.306 - 0.361) * t
                b = 0.659 + (0.643 - 0.659) * t
            } else if (pct <= 80) {
                t = (pct - 50) / 30.0
                r = 0.933 + (0.961 - 0.933) * t
                g = 0.306 + (0.620 - 0.306) * t
                b = 0.643 + (0.498 - 0.643) * t
            } else {
                t = (pct - 80) / 20.0
                r = 0.961 + (0.937 - 0.961) * t
                g = 0.620 + (0.267 - 0.620) * t
                b = 0.498 + (0.267 - 0.498) * t
            }
        } else {
            if (pct <= 50) {
                t = pct / 50.0
                r = 0.231 + (0.024 - 0.231) * t
                g = 0.510 + (0.714 - 0.510) * t
                b = 0.965 + (0.831 - 0.965) * t
            } else if (pct <= 80) {
                t = (pct - 50) / 30.0
                r = 0.024 + (0.961 - 0.024) * t
                g = 0.714 + (0.620 - 0.714) * t
                b = 0.831 + (0.043 - 0.831) * t
            } else {
                t = (pct - 80) / 20.0
                r = 0.961 + (0.937 - 0.961) * t
                g = 0.620 + (0.267 - 0.620) * t
                b = 0.043 + (0.267 - 0.043) * t
            }
        }
        return Qt.rgba(
            Math.max(0, Math.min(1, r)),
            Math.max(0, Math.min(1, g)),
            Math.max(0, Math.min(1, b)),
            alpha
        )
    }

    // ── Responsive layout properties ─────────────────────────────────────────
    property real scaleFactor: Math.max(0.5, Math.min(1.5, root.height / 600.0))
    property real scaledMargin: Math.round(10 * scaleFactor)
    property real scaledSpacing: Math.round(6 * scaleFactor)
    property real gaugeSize: Math.round(Math.max(60, Math.min(220, Math.min(root.width * 0.28, root.height * 0.32))))
    property real effectiveGaugeSize: gaugeSize

    property int fontTitle: Math.round(Math.max(8, 10 * scaleFactor))
    property int fontGaugeValue: Math.round(Math.max(14, 22 * scaleFactor))
    property int fontGaugeLabel: Math.round(Math.max(7, 10 * scaleFactor))
    property int fontSparkLabel: Math.round(Math.max(7, 10 * scaleFactor))
    property int fontSparkValue: Math.round(Math.max(8, 11 * scaleFactor))
    property int fontStatus: Math.round(Math.max(8, 10 * scaleFactor))

    // =========================================================================
    // LAYER 0 — background vignette / depth
    // =========================================================================
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.clrBgGradTop }
            GradientStop { position: 0.55; color: root.clrBgGradMid }
            GradientStop { position: 1.0; color: root.clrBgGradBot }
        }
    }

    // Corner vignette — top fade
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: parent.height * 0.35
        radius: root.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(root.accentRed * 0.5, root.accentGreen * 0.5, root.accentBlue * 0.5, 0.06 + root.accentIntensity * 0.04) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // Canvas decorative accents for pink mode
    Item {
        anchors.fill: parent
        visible: root.pinkMode
        z: 0
        opacity: 0.72

        // 6-pointed star — top-left
        Canvas {
            id: starDecor
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: Math.round(12 * root.scaleFactor)
            anchors.topMargin: Math.round(10 * root.scaleFactor)
            width: Math.max(12, root.width * 0.04)
            height: width
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                var cx = width / 2, cy = height / 2
                var outer = width * 0.45, inner = width * 0.22
                // Inner glow ring
                ctx.beginPath()
                ctx.arc(cx, cy, outer * 0.7, 0, Math.PI * 2)
                ctx.strokeStyle = Qt.rgba(1, 0.3, 0.65, 0.25)
                ctx.lineWidth = 1.5
                ctx.stroke()
                // 6-pointed star
                ctx.beginPath()
                for (var i = 0; i < 12; i++) {
                    var angle = (i * Math.PI / 6) - Math.PI / 2
                    var r = (i % 2 === 0) ? outer : inner
                    var px = cx + r * Math.cos(angle)
                    var py = cy + r * Math.sin(angle)
                    if (i === 0) ctx.moveTo(px, py)
                    else ctx.lineTo(px, py)
                }
                ctx.closePath()
                var grad = ctx.createRadialGradient(cx, cy, 0, cx, cy, outer)
                grad.addColorStop(0.0, "#ff92c5")
                grad.addColorStop(1.0, "#ff4da6")
                ctx.fillStyle = grad
                ctx.fill()
            }
            Connections {
                target: root
                function onWidthChanged() { starDecor.requestPaint() }
            }
        }

        // Diamond facet — top-right
        Canvas {
            id: diamondDecor
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: Math.round(14 * root.scaleFactor)
            anchors.topMargin: Math.round(10 * root.scaleFactor)
            width: Math.max(10, root.width * 0.035)
            height: width
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                var cx = width / 2, cy = height / 2, s = width * 0.42
                ctx.save()
                ctx.translate(cx, cy)
                ctx.rotate(Math.PI / 4)
                // Outer diamond
                ctx.beginPath()
                ctx.rect(-s, -s, s * 2, s * 2)
                var grad = ctx.createLinearGradient(-s, -s, s, s)
                grad.addColorStop(0.0, "#ff4da6")
                grad.addColorStop(1.0, "#ff92c5")
                ctx.fillStyle = grad
                ctx.fill()
                // Inner highlight
                var hs = s * 0.5
                ctx.beginPath()
                ctx.rect(-hs, -hs, hs * 2, hs * 2)
                ctx.fillStyle = Qt.rgba(1, 1, 1, 0.18)
                ctx.fill()
                ctx.restore()
            }
            Connections {
                target: root
                function onWidthChanged() { diamondDecor.requestPaint() }
            }
        }

        // Heart cluster — bottom-right
        Canvas {
            id: heartDecor
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: Math.round(14 * root.scaleFactor)
            anchors.bottomMargin: Math.round(10 * root.scaleFactor)
            width: Math.max(16, root.width * 0.05)
            height: width * 0.8
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)

                function drawHeart(cx, cy, sz, clr) {
                    ctx.save()
                    ctx.translate(cx, cy)
                    ctx.beginPath()
                    ctx.moveTo(0, sz * 0.3)
                    ctx.bezierCurveTo(-sz * 0.5, -sz * 0.3, -sz, sz * 0.1, 0, sz)
                    ctx.moveTo(0, sz * 0.3)
                    ctx.bezierCurveTo(sz * 0.5, -sz * 0.3, sz, sz * 0.1, 0, sz)
                    ctx.fillStyle = clr
                    ctx.fill()
                    ctx.restore()
                }

                var base = width * 0.28
                drawHeart(width * 0.55, height * 0.10, base * 1.0, "#ff4da6")
                drawHeart(width * 0.25, height * 0.30, base * 0.7, "#ff92c5")
                drawHeart(width * 0.72, height * 0.42, base * 0.55, "#d887b1")
            }
            Connections {
                target: root
                function onWidthChanged() { heartDecor.requestPaint() }
            }
        }

        // Dot rail — left edge
        Canvas {
            id: dotRailDecor
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 6
            width: 8
            height: Math.max(40, root.height * 0.12)
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                var cx = width / 2
                var spacing = height / 6
                for (var i = 0; i < 5; i++) {
                    var dy = spacing * (i + 1)
                    var isCenter = (i === 2)
                    var r = isCenter ? 3.0 : 1.8
                    var alpha = isCenter ? 0.85 : 0.50
                    ctx.beginPath()
                    ctx.arc(cx, dy, r, 0, Math.PI * 2)
                    ctx.fillStyle = Qt.rgba(1.0, 0.30, 0.65, alpha)
                    ctx.fill()
                }
            }
            Connections {
                target: root
                function onHeightChanged() { dotRailDecor.requestPaint() }
            }
        }
    }

    // =========================================================================
    // LAYER 1 — frosted-glass panel surface
    // =========================================================================
    Rectangle {
        id: panelSurface
        anchors { fill: parent; margins: 1 }
        radius: root.radius - 1
        color: Qt.rgba(root.accentRed * 0.08, root.accentGreen * 0.08, root.accentBlue * 0.12,
                       0.18 + root.frostIntensity * 0.25)
        border.width: 1
        border.color: Qt.rgba(root.accentRed, root.accentGreen, root.accentBlue,
                              0.12 + root.accentIntensity * 0.10)
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 14; rightMargin: 14; topMargin: 10 }
        height: 2
        radius: 1
        color: root.severityColor(root.severityLevel, 0.30 + root.accentIntensity * 0.20)
        opacity: root.qualityHint > 0 ? 0.65 : 1.0
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 18; rightMargin: 18; bottomMargin: 14 }
        height: 3
        radius: 2
        color: root.severityColor(root.severityLevel, Math.max(0.08, root.timelineAnomalyAlpha))
        opacity: root.timelineAnomalyAlpha
    }

    // =========================================================================
    // CONTENT COLUMN — ColumnLayout for responsive sizing
    // =========================================================================
    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.topMargin: root.scaledMargin
        anchors.leftMargin: 0
        anchors.rightMargin: 0
        anchors.bottomMargin: 0
        spacing: 0

        // ── Title band ───────────────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(22, root.height * 0.055)

            Text {
                id: titleLabel
                text: "AURA  COCKPIT"
                anchors.centerIn: parent
                color: root.clrTextMuted
                font.pixelSize: root.fontTitle
                font.weight: Font.Medium
                font.letterSpacing: Math.round(4 * root.scaleFactor)
            }

            // Subtle title underline glow
            Rectangle {
                anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
                width: titleLabel.width * 0.6
                height: 1
                color: root.accentColor(0.20 + root.accentIntensity * 0.15)
            }
        }

        Item { Layout.preferredHeight: root.scaledSpacing; Layout.fillWidth: true }

        // ── Gauge pair ───────────────────────────────────────────────────────
        Row {
            id: gaugeRow
            Layout.alignment: Qt.AlignHCenter
            spacing: Math.round(Math.max(12, root.width * 0.05))

            // ── CPU gauge ────────────────────────────────────────────────────
            Item {
                id: cpuGaugeItem
                width: root.effectiveGaugeSize
                height: root.effectiveGaugeSize

                // Outer glow halo
                Canvas {
                    id: cpuGlowCanvas
                    anchors.centerIn: parent
                    width: parent.width + Math.round(12 + 12 * root.scaleFactor)
                    height: parent.height + Math.round(12 + 12 * root.scaleFactor)
                    opacity: 0.35 + root.accentIntensity * 0.20

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        var cx = width / 2
                        var cy = height / 2
                        var r  = (parent.width / 2) + 6
                        var startAngle = Math.PI * 0.75
                        var sweepAngle = Math.PI * 1.50 * (root.smoothCpu / 100.0)

                        var gc = root.gaugeColor(root.smoothCpu, 1.0)
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, startAngle, startAngle + sweepAngle, false)
                        ctx.strokeStyle = Qt.rgba(gc.r, gc.g, gc.b, 0.30)
                        ctx.lineWidth   = Math.max(6, 18 * root.scaleFactor)
                        ctx.lineCap     = "round"
                        ctx.stroke()
                    }

                    Connections {
                        target: root
                        function onSmoothCpuChanged() { cpuGlowCanvas.requestPaint() }
                        function onAccentIntensityChanged() { cpuGlowCanvas.requestPaint() }
                    }
                }

                // Main arc gauge canvas
                Canvas {
                    id: cpuArcCanvas
                    anchors.centerIn: parent
                    width:  parent.width
                    height: parent.height

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        var cx = width  / 2
                        var cy = height / 2
                        var r  = width  / 2 - Math.max(6, Math.round(10 * root.scaleFactor))
                        var trackW = Math.max(5, Math.round(11 * root.scaleFactor))
                        var startAngle = Math.PI * 0.75
                        var fullSweep  = Math.PI * 1.50
                        var endAngle   = startAngle + fullSweep * (root.smoothCpu / 100.0)

                        // Track arc
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, startAngle, startAngle + fullSweep, false)
                        ctx.strokeStyle = root.clrTrack
                        ctx.lineWidth   = trackW
                        ctx.lineCap     = "butt"
                        ctx.stroke()

                        // Tick marks at 25%, 50%, 75%
                        ctx.strokeStyle = root.clrTrackTick
                        ctx.lineWidth   = 2
                        var ticks = [0.25, 0.50, 0.75]
                        for (var i = 0; i < ticks.length; i++) {
                            var ta = startAngle + fullSweep * ticks[i]
                            ctx.beginPath()
                            ctx.arc(cx, cy, r, ta, ta + 0.012, false)
                            ctx.lineWidth = trackW + 2
                            ctx.stroke()
                        }

                        // Value arc
                        if (root.smoothCpu > 0.2) {
                            var gc = root.gaugeColor(root.smoothCpu, 1.0)
                            var grad = ctx.createLinearGradient(
                                cx + r * Math.cos(startAngle),
                                cy + r * Math.sin(startAngle),
                                cx + r * Math.cos(endAngle),
                                cy + r * Math.sin(endAngle)
                            )
                            var gcStart = root.gaugeColor(Math.max(0, root.smoothCpu * 0.5), 1.0)
                            grad.addColorStop(0.0, Qt.rgba(gcStart.r, gcStart.g, gcStart.b, 0.85))
                            grad.addColorStop(1.0, Qt.rgba(gc.r,      gc.g,      gc.b,      1.00))

                            ctx.beginPath()
                            ctx.arc(cx, cy, r, startAngle, endAngle, false)
                            ctx.strokeStyle = grad
                            ctx.lineWidth   = trackW
                            ctx.lineCap     = "round"
                            ctx.stroke()

                            // Leading dot highlight
                            var dotX = cx + r * Math.cos(endAngle)
                            var dotY = cy + r * Math.sin(endAngle)
                            ctx.beginPath()
                            ctx.arc(dotX, dotY, trackW * 0.55, 0, Math.PI * 2, false)
                            ctx.fillStyle = Qt.rgba(gc.r, gc.g, gc.b, 1.0)
                            ctx.fill()
                        }
                    }

                    Connections {
                        target: root
                        function onSmoothCpuChanged() { cpuArcCanvas.requestPaint() }
                        function onAccentIntensityChanged() { cpuArcCanvas.requestPaint() }
                    }
                }

                // Center text
                Column {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 4
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.smoothCpu.toFixed(0) + "%"
                        color: root.clrTextPrimary
                        font.pixelSize: root.fontGaugeValue
                        font.weight: Font.Bold
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "CPU"
                        color: root.clrTextSecondary
                        font.pixelSize: root.fontGaugeLabel
                        font.weight: Font.Medium
                        font.letterSpacing: 2.5
                    }
                }
            }

            // ── Memory gauge ─────────────────────────────────────────────────
            Item {
                id: memGaugeItem
                width: root.effectiveGaugeSize
                height: root.effectiveGaugeSize

                // Outer glow halo
                Canvas {
                    id: memGlowCanvas
                    anchors.centerIn: parent
                    width: parent.width + Math.round(12 + 12 * root.scaleFactor)
                    height: parent.height + Math.round(12 + 12 * root.scaleFactor)
                    opacity: 0.35 + root.accentIntensity * 0.20

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        var cx = width / 2
                        var cy = height / 2
                        var r  = (parent.width / 2) + 6
                        var startAngle = Math.PI * 0.75
                        var sweepAngle = Math.PI * 1.50 * (root.smoothMem / 100.0)

                        var gc = root.gaugeColor(root.smoothMem, 1.0)
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, startAngle, startAngle + sweepAngle, false)
                        ctx.strokeStyle = Qt.rgba(gc.r, gc.g, gc.b, 0.30)
                        ctx.lineWidth   = Math.max(6, 18 * root.scaleFactor)
                        ctx.lineCap     = "round"
                        ctx.stroke()
                    }

                    Connections {
                        target: root
                        function onSmoothMemChanged() { memGlowCanvas.requestPaint() }
                        function onAccentIntensityChanged() { memGlowCanvas.requestPaint() }
                    }
                }

                Canvas {
                    id: memArcCanvas
                    anchors.centerIn: parent
                    width:  parent.width
                    height: parent.height

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        var cx = width  / 2
                        var cy = height / 2
                        var r  = width  / 2 - Math.max(6, Math.round(10 * root.scaleFactor))
                        var trackW = Math.max(5, Math.round(11 * root.scaleFactor))
                        var startAngle = Math.PI * 0.75
                        var fullSweep  = Math.PI * 1.50
                        var endAngle   = startAngle + fullSweep * (root.smoothMem / 100.0)

                        // Track arc
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, startAngle, startAngle + fullSweep, false)
                        ctx.strokeStyle = root.clrTrack
                        ctx.lineWidth   = trackW
                        ctx.lineCap     = "butt"
                        ctx.stroke()

                        // Tick marks
                        ctx.strokeStyle = root.clrTrackTick
                        var ticks = [0.25, 0.50, 0.75]
                        for (var i = 0; i < ticks.length; i++) {
                            var ta = startAngle + fullSweep * ticks[i]
                            ctx.beginPath()
                            ctx.arc(cx, cy, r, ta, ta + 0.012, false)
                            ctx.lineWidth = trackW + 2
                            ctx.stroke()
                        }

                        // Value arc
                        if (root.smoothMem > 0.2) {
                            var gc = root.gaugeColor(root.smoothMem, 1.0)
                            var grad = ctx.createLinearGradient(
                                cx + r * Math.cos(startAngle),
                                cy + r * Math.sin(startAngle),
                                cx + r * Math.cos(endAngle),
                                cy + r * Math.sin(endAngle)
                            )
                            var gcStart = root.gaugeColor(Math.max(0, root.smoothMem * 0.5), 1.0)
                            grad.addColorStop(0.0, Qt.rgba(gcStart.r, gcStart.g, gcStart.b, 0.85))
                            grad.addColorStop(1.0, Qt.rgba(gc.r,      gc.g,      gc.b,      1.00))

                            ctx.beginPath()
                            ctx.arc(cx, cy, r, startAngle, endAngle, false)
                            ctx.strokeStyle = grad
                            ctx.lineWidth   = trackW
                            ctx.lineCap     = "round"
                            ctx.stroke()

                            // Leading dot
                            var dotX = cx + r * Math.cos(endAngle)
                            var dotY = cy + r * Math.sin(endAngle)
                            ctx.beginPath()
                            ctx.arc(dotX, dotY, trackW * 0.55, 0, Math.PI * 2, false)
                            ctx.fillStyle = Qt.rgba(gc.r, gc.g, gc.b, 1.0)
                            ctx.fill()
                        }
                    }

                    Connections {
                        target: root
                        function onSmoothMemChanged() { memArcCanvas.requestPaint() }
                        function onAccentIntensityChanged() { memArcCanvas.requestPaint() }
                    }
                }

                // Center text
                Column {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 4
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.smoothMem.toFixed(0) + "%"
                        color: root.clrTextPrimary
                        font.pixelSize: root.fontGaugeValue
                        font.weight: Font.Bold
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "MEM"
                        color: root.clrTextSecondary
                        font.pixelSize: root.fontGaugeLabel
                        font.weight: Font.Medium
                        font.letterSpacing: 2.5
                    }
                }
            }
        } // Row gaugeRow

        Item { Layout.preferredHeight: root.scaledSpacing * 1.3; Layout.fillWidth: true }

        // ── Section divider ──────────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 1

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.82
                height: 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.2; color: root.accentColor(0.15 + root.accentIntensity * 0.08) }
                    GradientStop { position: 0.8; color: root.accentColor(0.15 + root.accentIntensity * 0.08) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }
        }

        Item { Layout.preferredHeight: root.scaledSpacing; Layout.fillWidth: true }

        // ── Sparkline — CPU ──────────────────────────────────────────────────
        Item {
            id: cpuSparkRow
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Math.round(Math.max(28, 48 * root.scaleFactor))
            Layout.leftMargin: root.scaledMargin
            Layout.rightMargin: root.scaledMargin

            // Label row
            Text {
                id: cpuSparkLabel
                anchors { left: parent.left; top: parent.top }
                text: "CPU  HISTORY"
                color: root.clrTextMuted
                font.pixelSize: root.fontSparkLabel
                font.weight: Font.Medium
                font.letterSpacing: 2.5
            }

            Text {
                anchors { right: parent.right; top: parent.top }
                text: root.smoothCpu.toFixed(1) + "%"
                color: root.gaugeColor(root.smoothCpu, 1.0)
                font.pixelSize: root.fontSparkValue
                font.weight: Font.Bold
                font.letterSpacing: 0.5
            }

            // Chart canvas
            Canvas {
                id: cpuSparkCanvas
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: parent.height - Math.round(Math.max(12, 18 * root.scaleFactor))

                property var samples: []

                function pushSample(val) {
                    samples.push(val)
                    var cap = root.qualityHint > 0 ? 80 : 120
                    if (samples.length > cap) samples.shift()
                    requestPaint()
                }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    if (samples.length < 2) return

                    var n   = samples.length
                    var pad = 2
                    var cw  = width
                    var ch  = height - pad

                    var pts = []
                    for (var i = 0; i < n; i++) {
                        var x = (i / (n - 1)) * cw
                        var y = ch - (samples[i] / 100.0) * ch + pad
                        pts.push({x: x, y: y})
                    }

                    // Filled gradient area
                    ctx.beginPath()
                    ctx.moveTo(pts[0].x, pts[0].y)
                    for (var j = 1; j < n - 1; j++) {
                        var mx = (pts[j].x + pts[j+1].x) / 2
                        var my = (pts[j].y + pts[j+1].y) / 2
                        ctx.quadraticCurveTo(pts[j].x, pts[j].y, mx, my)
                    }
                    ctx.lineTo(pts[n-1].x, pts[n-1].y)
                    ctx.lineTo(cw, ch + pad)
                    ctx.lineTo(0, ch + pad)
                    ctx.closePath()

                    var acR = root.accentRed
                    var acG = root.accentGreen
                    var acB = root.accentBlue
                    var fillGrad = ctx.createLinearGradient(0, 0, 0, ch)
                    fillGrad.addColorStop(0.0, Qt.rgba(acR, acG, acB, 0.28))
                    fillGrad.addColorStop(1.0, Qt.rgba(acR, acG, acB, 0.02))
                    ctx.fillStyle   = fillGrad
                    ctx.fill()

                    // Crisp line on top
                    ctx.beginPath()
                    ctx.moveTo(pts[0].x, pts[0].y)
                    for (var k = 1; k < n - 1; k++) {
                        var lmx = (pts[k].x + pts[k+1].x) / 2
                        var lmy = (pts[k].y + pts[k+1].y) / 2
                        ctx.quadraticCurveTo(pts[k].x, pts[k].y, lmx, lmy)
                    }
                    ctx.lineTo(pts[n-1].x, pts[n-1].y)
                    ctx.strokeStyle = Qt.rgba(acR, acG, acB, 0.70)
                    ctx.lineWidth   = 1.5
                    ctx.lineJoin    = "round"
                    ctx.stroke()

                    // Latest value dot
                    var lp = pts[n-1]
                    ctx.beginPath()
                    ctx.arc(lp.x, lp.y, 2.5, 0, Math.PI * 2, false)
                    ctx.fillStyle = Qt.rgba(acR, acG, acB, 1.0)
                    ctx.fill()
                }

                Connections {
                    target: root
                    function onAccentRedChanged()   { cpuSparkCanvas.requestPaint() }
                    function onAccentGreenChanged() { cpuSparkCanvas.requestPaint() }
                    function onAccentBlueChanged()  { cpuSparkCanvas.requestPaint() }
                }
            }
        }

        Item { Layout.preferredHeight: root.scaledSpacing * 0.6; Layout.fillWidth: true }

        // ── Sparkline — Memory ───────────────────────────────────────────────
        Item {
            id: memSparkRow
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Math.round(Math.max(28, 48 * root.scaleFactor))
            Layout.leftMargin: root.scaledMargin
            Layout.rightMargin: root.scaledMargin

            Text {
                anchors { left: parent.left; top: parent.top }
                text: "MEM  HISTORY"
                color: root.clrTextMuted
                font.pixelSize: root.fontSparkLabel
                font.weight: Font.Medium
                font.letterSpacing: 2.5
            }

            Text {
                anchors { right: parent.right; top: parent.top }
                text: root.smoothMem.toFixed(1) + "%"
                color: root.gaugeColor(root.smoothMem, 1.0)
                font.pixelSize: root.fontSparkValue
                font.weight: Font.Bold
                font.letterSpacing: 0.5
            }

            Canvas {
                id: memSparkCanvas
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: parent.height - Math.round(Math.max(12, 18 * root.scaleFactor))

                property var samples: []

                function pushSample(val) {
                    samples.push(val)
                    var cap = root.qualityHint > 0 ? 80 : 120
                    if (samples.length > cap) samples.shift()
                    requestPaint()
                }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    if (samples.length < 2) return

                    var n   = samples.length
                    var pad = 2
                    var cw  = width
                    var ch  = height - pad

                    var pts = []
                    for (var i = 0; i < n; i++) {
                        var x = (i / (n - 1)) * cw
                        var y = ch - (samples[i] / 100.0) * ch + pad
                        pts.push({x: x, y: y})
                    }

                    // Filled area
                    ctx.beginPath()
                    ctx.moveTo(pts[0].x, pts[0].y)
                    for (var j = 1; j < n - 1; j++) {
                        var mx = (pts[j].x + pts[j+1].x) / 2
                        var my = (pts[j].y + pts[j+1].y) / 2
                        ctx.quadraticCurveTo(pts[j].x, pts[j].y, mx, my)
                    }
                    ctx.lineTo(pts[n-1].x, pts[n-1].y)
                    ctx.lineTo(cw, ch + pad)
                    ctx.lineTo(0, ch + pad)
                    ctx.closePath()

                    var fillGrad = ctx.createLinearGradient(0, 0, 0, ch)
                    fillGrad.addColorStop(0.0, Qt.rgba(0.15, 0.65, 0.78, 0.28))
                    fillGrad.addColorStop(1.0, Qt.rgba(0.15, 0.65, 0.78, 0.02))
                    ctx.fillStyle = fillGrad
                    ctx.fill()

                    ctx.beginPath()
                    ctx.moveTo(pts[0].x, pts[0].y)
                    for (var k = 1; k < n - 1; k++) {
                        var lmx = (pts[k].x + pts[k+1].x) / 2
                        var lmy = (pts[k].y + pts[k+1].y) / 2
                        ctx.quadraticCurveTo(pts[k].x, pts[k].y, lmx, lmy)
                    }
                    ctx.lineTo(pts[n-1].x, pts[n-1].y)
                    ctx.strokeStyle = Qt.rgba(0.15, 0.65, 0.78, 0.70)
                    ctx.lineWidth   = 1.5
                    ctx.lineJoin    = "round"
                    ctx.stroke()

                    // Latest dot
                    var lp = pts[n-1]
                    ctx.beginPath()
                    ctx.arc(lp.x, lp.y, 2.5, 0, Math.PI * 2, false)
                    ctx.fillStyle = Qt.rgba(0.15, 0.65, 0.78, 1.0)
                    ctx.fill()
                }
            }
        }

        Item { Layout.preferredHeight: root.scaledSpacing * 0.8; Layout.fillWidth: true }

        // ── Final divider ────────────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 1

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.82
                height: 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.2; color: root.accentColor(0.12 + root.frostIntensity * 0.06) }
                    GradientStop { position: 0.8; color: root.accentColor(0.12 + root.frostIntensity * 0.06) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }
        }

        Item { Layout.preferredHeight: root.scaledSpacing * 0.8; Layout.fillWidth: true }

        // ── Status line ──────────────────────────────────────────────────────
        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 6

            // Pulse indicator dot
            Rectangle {
                id: statusDot
                width: 5
                height: 5
                radius: 3
                anchors.verticalCenter: parent.verticalCenter
                color: root.accentColor(0.60 + root.accentIntensity * 0.40)

                SequentialAnimation on opacity {
                    running: true
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.25; duration: Math.max(160, Math.round(900 * root.motionScale)); easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.00; duration: Math.max(160, Math.round(900 * root.motionScale)); easing.type: Easing.InOutSine }
                }
            }

            Text {
                id: statusLabel
                text: root.statusText
                color: root.clrTextMuted
                font.pixelSize: root.fontStatus
                font.letterSpacing: 0.5
                elide: Text.ElideRight
                width: root.width * 0.75
                horizontalAlignment: Text.AlignLeft
            }
        }

        Item { Layout.preferredHeight: root.scaledMargin; Layout.fillWidth: true }
    } // contentColumn

    // =========================================================================
    // OUTER FRAME — accent border glow (drawn last, on top)
    // =========================================================================
    Rectangle {
        id: outerFrame
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: 1
        border.color: root.accentColor(
            Math.min(0.45, root.accentAlpha + root.accentIntensity * 0.20)
        )

        Behavior on border.color {
            ColorAnimation { duration: 400; easing.type: Easing.OutCubic }
        }
    }

    // Inner highlight edge — top-left corner catch-light
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 1
        radius: root.radius
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.3; color: Qt.rgba(root.accentRed, root.accentGreen, root.accentBlue, 0.18 + root.accentIntensity * 0.10) }
            GradientStop { position: 0.7; color: Qt.rgba(root.accentRed, root.accentGreen, root.accentBlue, 0.18 + root.accentIntensity * 0.10) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // =========================================================================
    // ACCENT GLOW PULSE — breathing border at high accentIntensity
    // =========================================================================
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: Math.max(0, root.accentIntensity * 3.0)
        border.color: root.accentColor(root.accentIntensity * 0.18)
        opacity: 0.8

        Behavior on border.width {
            NumberAnimation { duration: 400; easing.type: Easing.OutCubic }
        }
        Behavior on border.color {
            ColorAnimation { duration: 400; easing.type: Easing.OutCubic }
        }
    }
}
