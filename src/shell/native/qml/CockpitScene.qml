import QtQuick 2.15

Rectangle {
    id: root
    color: "#080e18"
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

    // ── Smoothed animated values used by Canvas gauges ───────────────────────
    property real smoothCpu: 0.0
    property real smoothMem: 0.0

    Behavior on smoothCpu {
        NumberAnimation { duration: Math.max(120, Math.round(600 * root.motionScale)); easing.type: Easing.OutCubic }
    }
    Behavior on smoothMem {
        NumberAnimation { duration: Math.max(120, Math.round(600 * root.motionScale)); easing.type: Easing.OutCubic }
    }
    Behavior on accentIntensity {
        NumberAnimation { duration: Math.max(80, Math.round(400 * root.motionScale)); easing.type: Easing.OutCubic }
    }

    onCpuPercentChanged:    { smoothCpu = cpuPercent;    cpuSparkCanvas.pushSample(cpuPercent) }
    onMemoryPercentChanged: { smoothMem = memoryPercent; memSparkCanvas.pushSample(memoryPercent) }

    // ── Derived accent color helpers ─────────────────────────────────────────
    function accentColor(alpha) {
        return Qt.rgba(accentRed, accentGreen, accentBlue, alpha)
    }

    function severityColor(level, alpha) {
        if (level >= 3) return Qt.rgba(0.93, 0.27, 0.27, alpha)
        if (level === 2) return Qt.rgba(0.96, 0.62, 0.04, alpha)
        if (level === 1) return Qt.rgba(0.93, 0.73, 0.12, alpha)
        return Qt.rgba(accentRed, accentGreen, accentBlue, alpha)
    }

    // Gauge arc color: 0–50 = blue→cyan, 50–80 = cyan→amber, 80–100 = amber→red
    function gaugeColor(pct, alpha) {
        var t, r, g, b
        if (pct <= 50) {
            t = pct / 50.0
            r = 0.231 + (0.024 - 0.231) * t   // #3b82f6 → #06b6d4
            g = 0.510 + (0.714 - 0.510) * t
            b = 0.965 + (0.831 - 0.965) * t
        } else if (pct <= 80) {
            t = (pct - 50) / 30.0
            r = 0.024 + (0.961 - 0.024) * t   // #06b6d4 → #f59e0b
            g = 0.714 + (0.620 - 0.714) * t
            b = 0.831 + (0.043 - 0.831) * t
        } else {
            t = (pct - 80) / 20.0
            r = 0.961 + (0.937 - 0.961) * t   // #f59e0b → #ef4444
            g = 0.620 + (0.267 - 0.620) * t
            b = 0.043 + (0.267 - 0.043) * t
        }
        return Qt.rgba(
            Math.max(0, Math.min(1, r)),
            Math.max(0, Math.min(1, g)),
            Math.max(0, Math.min(1, b)),
            alpha
        )
    }

    // ── Gauge size ────────────────────────────────────────────────────────────
    property real gaugeSize: Math.min(Math.max(root.width * 0.30, 110), 160)

    // ── Layout constants ──────────────────────────────────────────────────────
    property real margin: 20

    // =========================================================================
    // LAYER 0 — background vignette / depth
    // =========================================================================
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0c1829" }
            GradientStop { position: 0.55; color: "#080e18" }
            GradientStop { position: 1.0; color: "#050a11" }
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
    // CONTENT COLUMN
    // =========================================================================
    Column {
        id: contentColumn
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: root.margin }
        spacing: 0

        // ── Title band ───────────────────────────────────────────────────────
        Item {
            width: parent.width
            height: 28

            Text {
                id: titleLabel
                text: "AURA  COCKPIT"
                anchors.centerIn: parent
                color: "#4a6d8a"
                font.pixelSize: 11
                font.weight: Font.Medium
                font.letterSpacing: 4
            }

            // Subtle title underline glow
            Rectangle {
                anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
                width: titleLabel.width * 0.6
                height: 1
                color: root.accentColor(0.20 + root.accentIntensity * 0.15)
            }
        }

        Item { width: 1; height: 14 } // spacer

        // ── Gauge pair ───────────────────────────────────────────────────────
        Row {
            id: gaugeRow
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Math.max(root.width * 0.08, 24)

            // ── CPU gauge ────────────────────────────────────────────────────
            Item {
                id: cpuGaugeItem
                width: root.gaugeSize
                height: root.gaugeSize

                // Outer glow halo — drawn first (behind)
                Canvas {
                    id: cpuGlowCanvas
                    anchors.centerIn: parent
                    width: parent.width + 24
                    height: parent.height + 24
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
                        ctx.lineWidth   = 18
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
                        var r  = width  / 2 - 10
                        var trackW = 11
                        var startAngle = Math.PI * 0.75            // 135°
                        var fullSweep  = Math.PI * 1.50            // 270°
                        var endAngle   = startAngle + fullSweep * (root.smoothCpu / 100.0)

                        // Track arc
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, startAngle, startAngle + fullSweep, false)
                        ctx.strokeStyle = "#1a2940"
                        ctx.lineWidth   = trackW
                        ctx.lineCap     = "butt"
                        ctx.stroke()

                        // Tick marks at 25%, 50%, 75%
                        ctx.strokeStyle = "#0c1829"
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
                        text: root.cpuPercent.toFixed(0) + "%"
                        color: "#e8f4ff"
                        font.pixelSize: Math.max(22, root.gaugeSize * 0.20)
                        font.weight: Font.Bold
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "CPU"
                        color: "#7ba8c8"
                        font.pixelSize: 9
                        font.weight: Font.Medium
                        font.letterSpacing: 2.5
                    }
                }
            }

            // ── Memory gauge ─────────────────────────────────────────────────
            Item {
                id: memGaugeItem
                width: root.gaugeSize
                height: root.gaugeSize

                // Outer glow halo
                Canvas {
                    id: memGlowCanvas
                    anchors.centerIn: parent
                    width: parent.width + 24
                    height: parent.height + 24
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
                        ctx.lineWidth   = 18
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
                        var r  = width  / 2 - 10
                        var trackW = 11
                        var startAngle = Math.PI * 0.75
                        var fullSweep  = Math.PI * 1.50
                        var endAngle   = startAngle + fullSweep * (root.smoothMem / 100.0)

                        // Track arc
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, startAngle, startAngle + fullSweep, false)
                        ctx.strokeStyle = "#1a2940"
                        ctx.lineWidth   = trackW
                        ctx.lineCap     = "butt"
                        ctx.stroke()

                        // Tick marks
                        ctx.strokeStyle = "#0c1829"
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
                        text: root.memoryPercent.toFixed(0) + "%"
                        color: "#e8f4ff"
                        font.pixelSize: Math.max(22, root.gaugeSize * 0.20)
                        font.weight: Font.Bold
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "MEM"
                        color: "#7ba8c8"
                        font.pixelSize: 9
                        font.weight: Font.Medium
                        font.letterSpacing: 2.5
                    }
                }
            }
        } // Row gaugeRow

        Item { width: 1; height: 16 } // spacer

        // ── Section divider ──────────────────────────────────────────────────
        Item {
            width: parent.width
            height: 1

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

        Item { width: 1; height: 14 } // spacer

        // ── Sparkline — CPU ──────────────────────────────────────────────────
        Item {
            id: cpuSparkRow
            anchors { left: parent.left; right: parent.right; leftMargin: root.margin; rightMargin: root.margin }
            height: 64

            // Label row
            Text {
                id: cpuSparkLabel
                anchors { left: parent.left; verticalCenter: undefined; top: parent.top }
                text: "CPU  HISTORY"
                color: "#4a6d8a"
                font.pixelSize: 9
                font.weight: Font.Medium
                font.letterSpacing: 2.5
            }

            Text {
                anchors { right: parent.right; top: parent.top }
                text: root.cpuPercent.toFixed(1) + "%"
                color: root.gaugeColor(root.cpuPercent, 1.0)
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 0.5
            }

            // Chart canvas
            Canvas {
                id: cpuSparkCanvas
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: parent.height - 18

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

                    // Build point array
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

        Item { width: 1; height: 8 } // spacer

        // ── Sparkline — Memory ───────────────────────────────────────────────
        Item {
            id: memSparkRow
            anchors { left: parent.left; right: parent.right; leftMargin: root.margin; rightMargin: root.margin }
            height: 64

            Text {
                anchors { left: parent.left; top: parent.top }
                text: "MEM  HISTORY"
                color: "#4a6d8a"
                font.pixelSize: 9
                font.weight: Font.Medium
                font.letterSpacing: 2.5
            }

            Text {
                anchors { right: parent.right; top: parent.top }
                text: root.memoryPercent.toFixed(1) + "%"
                color: root.gaugeColor(root.memoryPercent, 1.0)
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 0.5
            }

            Canvas {
                id: memSparkCanvas
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: parent.height - 18

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

                    // Filled area — memory uses a slightly warmer tint to visually separate
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

                    // Memory sparkline: tint shifted toward cyan regardless of accent
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

        Item { width: 1; height: 10 } // spacer

        // ── Final divider ────────────────────────────────────────────────────
        Item {
            width: parent.width
            height: 1

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

        Item { width: 1; height: 10 } // spacer

        // ── Status line ──────────────────────────────────────────────────────
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
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
                color: "#4a6d8a"
                font.pixelSize: 10
                font.letterSpacing: 0.5
                elide: Text.ElideRight
                width: root.width * 0.75
                horizontalAlignment: Text.AlignLeft
            }
        }

        Item { width: 1; height: root.margin } // bottom padding
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
