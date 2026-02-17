// ============================================================================
// Aura Cockpit Scene — QML Render Surface
// ============================================================================
//
// DESIGN INTENT: Addictive Visual Experience
//
//   This scene is intentionally crafted to be addictive. The arc gauges,
//   sparkline trails, holographic shimmer, mood-reactive colors, and kawaii
//   pink mode are all designed to make monitoring system telemetry feel like
//   a premium visual experience users can't stop watching. We are transparent
//   about this goal: every animation, color shift, and glow exists to create
//   a dopamine loop that rewards sustained attention.
//
// ============================================================================

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

    // ── Extended sensor properties ────────────────────────────────────────────
    property var perCoreCpu: []
    property int coreCount: 0
    property bool gpuAvailable: false
    property real gpuPercent: 0.0
    property real vramPercent: 0.0
    property real vramUsedBytes: 0
    property real vramTotalBytes: 0
    property real diskReadBps: 0.0
    property real diskWriteBps: 0.0
    property real netRecvBps: 0.0
    property real netSentBps: 0.0
    property bool thermalAvailable: false
    property real thermalHottest: 0.0
    property var thermalSensors: []

    // ── Analytics bridge properties ────────────────────────────────────────
    property bool healthAvailable: false
    property real healthOverall: 50.0
    property real healthCpu: 50.0
    property real healthMemory: 50.0
    property real healthDisk: 50.0
    property real healthNetwork: 50.0
    property int cpuTrend: 0    // 0=stable, 1=rising, 2=falling
    property int memoryTrend: 0
    property bool smoothingActive: false
    property var activeAlerts: []

    // Smoothed health for gauge animation
    property real smoothHealth: 50.0
    property bool hasHealthSample: false
    onHealthOverallChanged: {
        if (!hasHealthSample) { smoothHealth = healthOverall; hasHealthSample = true }
        else { smoothHealth = smoothHealth * 0.80 + healthOverall * 0.20 }
    }

    // Smoothed GPU value
    property real smoothGpu: 0.0
    property bool hasGpuSample: false
    onGpuPercentChanged: {
        if (!hasGpuSample) { smoothGpu = gpuPercent; hasGpuSample = true }
        else { smoothGpu = smoothGpu * 0.85 + gpuPercent * 0.15 }
    }

    // Responsive size category thresholds
    property string sizeCategory: root.height < 400 ? "compact"
                                : root.height < 600 ? "regular"
                                : root.height < 800 ? "comfortable"
                                : "spacious"
    property bool showExtended: true
    property bool showGpuThermal: true
    property bool showSpacious: sizeCategory === "spacious"

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

    // ── Kawaii mood-reactive palette (pink mode only) ──────────────────────
    property real moodBlend: {
        if (!pinkMode) return 0
        var load = (cpuPercent + memoryPercent) / 2.0
        if (load < 30) return 0.0
        if (load < 65) return (load - 30) / 35.0 * 0.33
        if (load < 85) return 0.33 + (load - 65) / 20.0 * 0.34
        return 0.67 + Math.min(1.0, (load - 85) / 15.0) * 0.33
    }
    Behavior on moodBlend { NumberAnimation { duration: 2000; easing.type: Easing.InOutQuad } }

    readonly property color clrLavender: "#c084fc"
    readonly property color clrPeach:    "#ffb088"
    readonly property color clrMint:     "#6ee7b7"
    readonly property color clrGold:     "#fbbf24"
    readonly property color clrCoral:    "#f47272"
    readonly property color clrMoodAccent: {
        if (!pinkMode) return clrAccent
        var m = moodBlend
        if (m < 0.33) {
            var t1 = m / 0.33
            return Qt.rgba(0.753 + (1.0 - 0.753) * t1, 0.518 + (0.302 - 0.518) * t1, 0.988 + (0.651 - 0.988) * t1, 1.0)
        }
        if (m < 0.67) {
            var t2 = (m - 0.33) / 0.34
            return Qt.rgba(1.0, 0.302 + (0.690 - 0.302) * t2, 0.651 + (0.533 - 0.651) * t2, 1.0)
        }
        var t3 = (m - 0.67) / 0.33
        return Qt.rgba(1.0 + (0.957 - 1.0) * t3, 0.690 + (0.447 - 0.690) * t3, 0.533 + (0.447 - 0.533) * t3, 1.0)
    }

    // Shimmer phase for holographic gauge arcs (pink mode)
    property real shimmerPhase: 0
    NumberAnimation on shimmerPhase {
        running: root.pinkMode && root.motionScale >= 0.05
        from: 0; to: 1; duration: 3000
        loops: Animation.Infinite
    }

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
            // Lavender → Hot Pink → Peach → Coral
            if (pct <= 30) {
                t = pct / 30.0
                r = 0.753 + (1.000 - 0.753) * t
                g = 0.518 + (0.302 - 0.518) * t
                b = 0.988 + (0.651 - 0.988) * t
            } else if (pct <= 65) {
                t = (pct - 30) / 35.0
                r = 1.000
                g = 0.302 + (0.690 - 0.302) * t
                b = 0.651 + (0.533 - 0.651) * t
            } else if (pct <= 85) {
                t = (pct - 65) / 20.0
                r = 1.000 + (0.957 - 1.000) * t
                g = 0.690 + (0.447 - 0.690) * t
                b = 0.533 + (0.447 - 0.533) * t
            } else {
                t = (pct - 85) / 15.0
                r = 0.957 + (0.937 - 0.957) * t
                g = 0.447 + (0.267 - 0.447) * t
                b = 0.447 + (0.267 - 0.447) * t
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

    // ── Health gauge color (inverted: high=green, low=red) ─────────────────
    function healthGaugeColor(score, alpha) {
        var r, g, b
        if (score < 50) {
            var t = score / 50.0
            r = 0.937 + (0.961 - 0.937) * t
            g = 0.267 + (0.620 - 0.267) * t
            b = 0.267 + (0.043 - 0.267) * t
        } else if (score < 80) {
            var t2 = (score - 50) / 30.0
            r = 0.961 + (0.133 - 0.961) * t2
            g = 0.620 + (0.773 - 0.620) * t2
            b = 0.043 + (0.369 - 0.043) * t2
        } else {
            var t3 = (score - 80) / 20.0
            r = 0.133 + (0.133 - 0.133) * t3
            g = 0.773 + (0.773 - 0.773) * t3
            b = 0.369 + (0.369 - 0.369) * t3
        }
        return Qt.rgba(
            Math.max(0, Math.min(1, r)),
            Math.max(0, Math.min(1, g)),
            Math.max(0, Math.min(1, b)),
            alpha
        )
    }

    // ── Health gauge arc drawing ──────────────────────────────────────────
    function drawHealthGauge(ctx, w, h, score, sf) {
        ctx.clearRect(0, 0, w, h)
        var cx = w / 2, cy = h / 2
        var r = w / 2 - Math.max(6, Math.round(10 * sf))
        var trackW = Math.max(5, Math.round(11 * sf))
        var startAngle = Math.PI * 0.75
        var fullSweep = Math.PI * 1.50
        var endAngle = startAngle + fullSweep * (score / 100.0)

        // Track arc
        ctx.beginPath()
        ctx.arc(cx, cy, r, startAngle, startAngle + fullSweep, false)
        ctx.strokeStyle = root.clrTrack
        ctx.lineWidth = trackW
        ctx.lineCap = "butt"
        ctx.stroke()

        // Tick marks at 25%, 50%, 75%
        ctx.strokeStyle = root.clrTrackTick
        ctx.lineWidth = 2
        var ticks = [0.25, 0.50, 0.75]
        for (var i = 0; i < ticks.length; i++) {
            var ta = startAngle + fullSweep * ticks[i]
            ctx.beginPath()
            ctx.arc(cx, cy, r, ta, ta + 0.012, false)
            ctx.lineWidth = trackW + 2
            ctx.stroke()
        }

        // Value arc
        if (score > 0.2) {
            var gc = root.healthGaugeColor(score, 1.0)
            var grad = ctx.createLinearGradient(
                cx + r * Math.cos(startAngle), cy + r * Math.sin(startAngle),
                cx + r * Math.cos(endAngle), cy + r * Math.sin(endAngle)
            )
            var gcStart = root.healthGaugeColor(Math.max(0, score * 0.5), 1.0)
            grad.addColorStop(0.0, Qt.rgba(gcStart.r, gcStart.g, gcStart.b, 0.85))
            grad.addColorStop(1.0, Qt.rgba(gc.r, gc.g, gc.b, 1.00))
            ctx.beginPath()
            ctx.arc(cx, cy, r, startAngle, endAngle, false)
            ctx.strokeStyle = grad
            ctx.lineWidth = trackW
            ctx.lineCap = "round"
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

    // ── Trend arrow helper ───────────────────────────────────────────────
    function trendArrow(direction) {
        if (direction === 1) return "\u2191"  // Rising
        if (direction === 2) return "\u2193"  // Falling
        return "\u2192"                        // Stable
    }
    function trendColor(direction) {
        if (direction === 1) return "#f47272"  // Red — rising usage is bad
        if (direction === 2) return "#6ee7b7"  // Green — falling usage is good
        return root.clrTextMuted               // Muted for stable
    }
    function trendOpacity(direction) {
        return direction === 0 ? 0.4 : 0.85
    }

    // ── Rate formatting helper ──────────────────────────────────────────────
    function formatRate(bytesPerSec) {
        if (bytesPerSec >= 1073741824) return (bytesPerSec / 1073741824).toFixed(1) + " GB/s"
        if (bytesPerSec >= 1048576)    return (bytesPerSec / 1048576).toFixed(1) + " MB/s"
        if (bytesPerSec >= 1024)       return (bytesPerSec / 1024).toFixed(1) + " KB/s"
        return Math.round(bytesPerSec) + " B/s"
    }

    // ── Arc gauge drawing helper (reused for CPU, Memory, GPU) ──────────────
    function drawArcGauge(ctx, w, h, pct, sf) {
        ctx.clearRect(0, 0, w, h)
        var cx = w / 2, cy = h / 2
        var r = w / 2 - Math.max(6, Math.round(10 * sf))
        var trackW = Math.max(5, Math.round(11 * sf))
        var startAngle = Math.PI * 0.75
        var fullSweep = Math.PI * 1.50
        var endAngle = startAngle + fullSweep * (pct / 100.0)

        // Track arc
        ctx.beginPath()
        ctx.arc(cx, cy, r, startAngle, startAngle + fullSweep, false)
        ctx.strokeStyle = root.clrTrack
        ctx.lineWidth = trackW
        ctx.lineCap = "butt"
        ctx.stroke()

        // Tick marks at 25%, 50%, 75%
        ctx.strokeStyle = root.clrTrackTick
        ctx.lineWidth = 2
        var ticks = [0.25, 0.50, 0.75]
        for (var i = 0; i < ticks.length; i++) {
            var ta = startAngle + fullSweep * ticks[i]
            ctx.beginPath()
            ctx.arc(cx, cy, r, ta, ta + 0.012, false)
            ctx.lineWidth = trackW + 2
            ctx.stroke()
        }

        // Value arc
        if (pct > 0.2) {
            var gc = root.gaugeColor(pct, 1.0)
            var grad = ctx.createLinearGradient(
                cx + r * Math.cos(startAngle), cy + r * Math.sin(startAngle),
                cx + r * Math.cos(endAngle), cy + r * Math.sin(endAngle)
            )
            var gcStart = root.gaugeColor(Math.max(0, pct * 0.5), 1.0)
            grad.addColorStop(0.0, Qt.rgba(gcStart.r, gcStart.g, gcStart.b, 0.85))
            grad.addColorStop(1.0, Qt.rgba(gc.r, gc.g, gc.b, 1.00))
            ctx.beginPath()
            ctx.arc(cx, cy, r, startAngle, endAngle, false)
            ctx.strokeStyle = grad
            ctx.lineWidth = trackW
            ctx.lineCap = "round"
            ctx.stroke()

            // Leading dot highlight
            var dotX = cx + r * Math.cos(endAngle)
            var dotY = cy + r * Math.sin(endAngle)
            ctx.beginPath()
            ctx.arc(dotX, dotY, trackW * 0.55, 0, Math.PI * 2, false)
            ctx.fillStyle = Qt.rgba(gc.r, gc.g, gc.b, 1.0)
            ctx.fill()

            // Holographic shimmer sweep (pink mode)
            if (root.pinkMode && root.motionScale >= 0.05) {
                var shimA = startAngle + fullSweep * (pct / 100.0) * root.shimmerPhase
                var shimX = cx + r * Math.cos(shimA)
                var shimY = cy + r * Math.sin(shimA)
                var shimR = trackW * 1.8
                var shimGrad = ctx.createRadialGradient(shimX, shimY, 0, shimX, shimY, shimR)
                shimGrad.addColorStop(0.0, Qt.rgba(1, 1, 1, 0.50))
                shimGrad.addColorStop(0.25, Qt.rgba(0.753, 0.518, 0.988, 0.30))
                shimGrad.addColorStop(0.6, Qt.rgba(gc.r, gc.g, gc.b, 0.12))
                shimGrad.addColorStop(1.0, "transparent")
                ctx.beginPath()
                ctx.arc(shimX, shimY, shimR, 0, Math.PI * 2, false)
                ctx.fillStyle = shimGrad
                ctx.fill()
            }
        }
    }

    // ── Glow halo drawing helper ────────────────────────────────────────────
    function drawGlowHalo(ctx, w, h, pct, parentW, sf) {
        ctx.clearRect(0, 0, w, h)
        var cx = w / 2, cy = h / 2
        var r = (parentW / 2) + 6
        var startAngle = Math.PI * 0.75
        var sweepAngle = Math.PI * 1.50 * (pct / 100.0)
        var gc = root.gaugeColor(pct, 1.0)
        ctx.beginPath()
        ctx.arc(cx, cy, r, startAngle, startAngle + sweepAngle, false)
        ctx.strokeStyle = Qt.rgba(gc.r, gc.g, gc.b, 0.30)
        ctx.lineWidth = Math.max(6, 18 * sf)
        ctx.lineCap = "round"
        ctx.stroke()
    }

    // ── Dual sparkline drawing helper ───────────────────────────────────────
    function drawDualSparkline(ctx, w, h, samples1, samples2, color1, color2) {
        ctx.clearRect(0, 0, w, h)
        if (samples1.length < 2 && samples2.length < 2) return

        // Collect all values and use 90th percentile as scale max (dampens spikes)
        var allVals = []
        for (var m = 0; m < samples1.length; m++) allVals.push(samples1[m])
        for (var mm = 0; mm < samples2.length; mm++) allVals.push(samples2[mm])
        allVals.sort(function(a, b) { return a - b })
        var p90Idx = Math.floor(allVals.length * 0.90)
        var p90Val = allVals[Math.min(p90Idx, allVals.length - 1)]
        var absMax = allVals[allVals.length - 1]
        // Use 90th percentile with 20% headroom, but never less than absolute max * 0.5
        var maxVal = Math.max(1, Math.max(absMax * 0.5, p90Val * 1.2))

        function drawLine(samples, clr) {
            if (samples.length < 2) return
            var n = samples.length, pad = 2, ch = h - pad
            var pts = []
            for (var i = 0; i < n; i++) {
                var norm = Math.min(1.0, samples[i] / maxVal)  // clamp spikes to top
                pts.push({x: (i / (n - 1)) * w, y: ch - norm * ch + pad})
            }
            // Fill
            ctx.beginPath()
            ctx.moveTo(pts[0].x, pts[0].y)
            for (var j = 1; j < n - 1; j++) {
                ctx.quadraticCurveTo(pts[j].x, pts[j].y, (pts[j].x + pts[j+1].x)/2, (pts[j].y + pts[j+1].y)/2)
            }
            ctx.lineTo(pts[n-1].x, pts[n-1].y)
            ctx.lineTo(w, ch + pad); ctx.lineTo(0, ch + pad); ctx.closePath()
            var fg = ctx.createLinearGradient(0, 0, 0, ch)
            fg.addColorStop(0.0, Qt.rgba(clr.r, clr.g, clr.b, 0.22))
            fg.addColorStop(1.0, Qt.rgba(clr.r, clr.g, clr.b, 0.02))
            ctx.fillStyle = fg; ctx.fill()
            // Stroke
            ctx.beginPath()
            ctx.moveTo(pts[0].x, pts[0].y)
            for (var k = 1; k < n - 1; k++) {
                ctx.quadraticCurveTo(pts[k].x, pts[k].y, (pts[k].x + pts[k+1].x)/2, (pts[k].y + pts[k+1].y)/2)
            }
            ctx.lineTo(pts[n-1].x, pts[n-1].y)
            ctx.strokeStyle = Qt.rgba(clr.r, clr.g, clr.b, 0.70)
            ctx.lineWidth = 1.5; ctx.lineJoin = "round"; ctx.stroke()
            // Dot
            var lp = pts[n-1]
            ctx.beginPath(); ctx.arc(lp.x, lp.y, 2.5, 0, Math.PI * 2, false)
            ctx.fillStyle = Qt.rgba(clr.r, clr.g, clr.b, 1.0); ctx.fill()
        }
        drawLine(samples1, color1)
        drawLine(samples2, color2)
    }

    // ── Responsive layout properties ─────────────────────────────────────────
    // Continuous scale — smooth across any window size
    property real scaleFactor: {
        var base = Math.max(0.4, Math.min(2.0, root.height / 600.0))
        var widthBoost = Math.max(0, Math.min(0.3, (root.width / 1200.0 - 1.0) * 0.15))
        return base + widthBoost
    }
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

    // Fluid resize animations — smooth 60fps transitions
    Behavior on scaleFactor { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Behavior on gaugeSize { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Behavior on scaledMargin { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on scaledSpacing { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on fontTitle { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on fontGaugeValue { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on fontGaugeLabel { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on fontSparkLabel { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on fontSparkValue { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
    Behavior on fontStatus { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

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

    // Corner vignette — top fade (mood-tinted in pink mode)
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: parent.height * 0.35
        radius: root.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.pinkMode
                ? Qt.rgba(root.clrMoodAccent.r * 0.3, root.clrMoodAccent.g * 0.15, root.clrMoodAccent.b * 0.25, 0.10 + root.accentIntensity * 0.06)
                : Qt.rgba(root.accentRed * 0.5, root.accentGreen * 0.5, root.accentBlue * 0.5, 0.06 + root.accentIntensity * 0.04) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // =========================================================================
    // AURORA NEBULA — ambient color clouds that drift slowly (pink mode)
    // =========================================================================
    Canvas {
        id: auroraCanvas
        anchors.fill: parent
        visible: root.pinkMode
        opacity: 0.35
        z: 0

        property real phase: 0
        NumberAnimation on phase {
            running: root.pinkMode && root.motionScale >= 0.05
            from: 0; to: Math.PI * 2; duration: 20000
            loops: Animation.Infinite
        }

        Timer {
            running: root.pinkMode && root.motionScale >= 0.05
            interval: 80
            repeat: true
            onTriggered: auroraCanvas.requestPaint()
        }

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var w = width, h = height, p = phase

            // 4 drifting nebula blobs
            var blobs = [
                { x: w * (0.25 + 0.15 * Math.sin(p * 0.7)), y: h * (0.3 + 0.1 * Math.cos(p * 0.5)), r: w * 0.35, c: root.clrLavender },
                { x: w * (0.72 + 0.12 * Math.cos(p * 0.6)), y: h * (0.25 + 0.12 * Math.sin(p * 0.8)), r: w * 0.30, c: root.clrAccent },
                { x: w * (0.5 + 0.18 * Math.sin(p * 0.4 + 1)), y: h * (0.65 + 0.1 * Math.cos(p * 0.3)), r: w * 0.32, c: root.clrPeach },
                { x: w * (0.3 + 0.1 * Math.cos(p * 0.9 + 2)), y: h * (0.8 + 0.08 * Math.sin(p * 0.7)), r: w * 0.25, c: root.clrMint }
            ]

            for (var i = 0; i < blobs.length; i++) {
                var b = blobs[i]
                var grad = ctx.createRadialGradient(b.x, b.y, 0, b.x, b.y, b.r)
                grad.addColorStop(0.0, Qt.rgba(b.c.r, b.c.g, b.c.b, 0.18))
                grad.addColorStop(0.5, Qt.rgba(b.c.r, b.c.g, b.c.b, 0.06))
                grad.addColorStop(1.0, "transparent")
                ctx.fillStyle = grad
                ctx.beginPath()
                ctx.arc(b.x, b.y, b.r, 0, Math.PI * 2)
                ctx.fill()
            }
        }
    }

    // ── Side glow rails (pink mode) ────────────────────────────────────────
    Rectangle {
        visible: root.pinkMode
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: 2
        opacity: 0.5
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.3; color: root.clrLavender }
            GradientStop { position: 0.5; color: root.clrMoodAccent }
            GradientStop { position: 0.7; color: root.clrPeach }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
    Rectangle {
        visible: root.pinkMode
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: 2
        opacity: 0.5
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.3; color: root.clrPeach }
            GradientStop { position: 0.5; color: root.clrMoodAccent }
            GradientStop { position: 0.7; color: root.clrLavender }
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

        // ── Kawaii constellation pattern ────────────────────────────────────
        Canvas {
            id: constellationCanvas
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.globalAlpha = 0.18
                var seed = Math.floor(width * 7 + height * 13)
                var dots = []
                for (var i = 0; i < 18; i++) {
                    seed = (seed * 9301 + 49297) % 233280
                    var dx = (seed / 233280.0) * width
                    seed = (seed * 9301 + 49297) % 233280
                    var dy = (seed / 233280.0) * height
                    dots.push({x: dx, y: dy})
                    ctx.beginPath()
                    ctx.arc(dx, dy, 1.5 * root.scaleFactor, 0, Math.PI * 2)
                    ctx.fillStyle = i % 2 === 0 ? "#c084fc" : "#6ee7b7"
                    ctx.fill()
                }
                ctx.strokeStyle = Qt.rgba(0.753, 0.518, 0.988, 0.12)
                ctx.lineWidth = 0.5
                for (var a = 0; a < dots.length; a++) {
                    for (var b = a + 1; b < dots.length; b++) {
                        var ddx = dots[a].x - dots[b].x, ddy = dots[a].y - dots[b].y
                        if (Math.sqrt(ddx * ddx + ddy * ddy) < width * 0.25) {
                            ctx.beginPath()
                            ctx.moveTo(dots[a].x, dots[a].y)
                            ctx.lineTo(dots[b].x, dots[b].y)
                            ctx.stroke()
                        }
                    }
                }
            }
            Connections {
                target: root
                function onWidthChanged() { constellationCanvas.requestPaint() }
                function onHeightChanged() { constellationCanvas.requestPaint() }
            }
        }

        // ── Sakura blossom — bottom-left ───────────────────────────────────
        Canvas {
            id: sakuraDecor
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: Math.round(16 * root.scaleFactor)
            anchors.bottomMargin: Math.round(14 * root.scaleFactor)
            width: Math.max(16, root.width * 0.045)
            height: width
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                var cx = width / 2, cy = height / 2, r = width * 0.38
                for (var i = 0; i < 5; i++) {
                    var angle = (i * Math.PI * 2 / 5) - Math.PI / 2
                    var nextAngle = ((i + 1) * Math.PI * 2 / 5) - Math.PI / 2
                    var px = cx + r * Math.cos(angle)
                    var py = cy + r * Math.sin(angle)
                    var cpDist = r * 1.1
                    var cpx = cx + cpDist * Math.cos((angle + nextAngle) / 2)
                    var cpy = cy + cpDist * Math.sin((angle + nextAngle) / 2)
                    ctx.beginPath()
                    ctx.moveTo(cx, cy)
                    ctx.quadraticCurveTo(cpx, cpy, px, py)
                    ctx.quadraticCurveTo(cx + r * 0.3 * Math.cos(angle + 0.3),
                                         cy + r * 0.3 * Math.sin(angle + 0.3), cx, cy)
                    ctx.fillStyle = i % 2 === 0 ? "#ffb088" : "#ff92c5"
                    ctx.globalAlpha = 0.6
                    ctx.fill()
                }
                ctx.globalAlpha = 0.8
                ctx.beginPath()
                ctx.arc(cx, cy, width * 0.08, 0, Math.PI * 2)
                ctx.fillStyle = "#fbbf24"
                ctx.fill()
            }
            Connections {
                target: root
                function onWidthChanged() { sakuraDecor.requestPaint() }
            }
        }

        // ── Crescent moon — near title, ultra-slow rotation ────────────────
        Item {
            id: crescentMoon
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: Math.round(50 * root.scaleFactor)
            anchors.topMargin: Math.round(8 * root.scaleFactor)
            width: Math.max(10, root.width * 0.028)
            height: width

            RotationAnimation on rotation {
                from: 0; to: 360; duration: 120000
                running: root.pinkMode && root.motionScale >= 0.05
                loops: Animation.Infinite
            }

            Canvas {
                id: crescentCanvas
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    var cx = width / 2, cy = height / 2, r = width * 0.42
                    ctx.beginPath()
                    ctx.arc(cx, cy, r, 0, Math.PI * 2)
                    ctx.fillStyle = "#c084fc"
                    ctx.globalAlpha = 0.55
                    ctx.fill()
                    ctx.globalCompositeOperation = "destination-out"
                    ctx.beginPath()
                    ctx.arc(cx + r * 0.35, cy - r * 0.15, r * 0.75, 0, Math.PI * 2)
                    ctx.fill()
                    ctx.globalCompositeOperation = "source-over"
                }
                Connections {
                    target: root
                    function onWidthChanged() { crescentCanvas.requestPaint() }
                }
            }
        }

        // ── Pulsing heartbeat heart — bottom area ──────────────────────────
        Canvas {
            id: pulsingHeart
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: Math.round(parent.width * 0.12)
            anchors.bottomMargin: Math.round(parent.height * 0.15)
            width: Math.max(12, root.width * 0.03)
            height: width

            property real breathScale: 1.0
            SequentialAnimation on breathScale {
                running: root.pinkMode && root.motionScale >= 0.05
                loops: Animation.Infinite
                NumberAnimation { to: 1.1; duration: 1200; easing.type: Easing.InOutSine }
                NumberAnimation { to: 0.9; duration: 1200; easing.type: Easing.InOutSine }
            }

            transform: Scale {
                origin.x: pulsingHeart.width / 2
                origin.y: pulsingHeart.height / 2
                xScale: pulsingHeart.breathScale
                yScale: pulsingHeart.breathScale
            }

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                var sz = width * 0.4
                var cx = width / 2, cy = height / 2
                ctx.beginPath()
                ctx.moveTo(cx, cy + sz * 0.15)
                ctx.bezierCurveTo(cx - sz * 0.5, cy - sz * 0.4, cx - sz, cy + sz * 0.05, cx, cy + sz * 0.6)
                ctx.moveTo(cx, cy + sz * 0.15)
                ctx.bezierCurveTo(cx + sz * 0.5, cy - sz * 0.4, cx + sz, cy + sz * 0.05, cx, cy + sz * 0.6)
                ctx.fillStyle = "#ff4da6"
                ctx.globalAlpha = 0.65
                ctx.fill()
            }
            Connections {
                target: root
                function onWidthChanged() { pulsingHeart.requestPaint() }
            }
        }

        // ── Ribbon bow — near title underline ──────────────────────────────
        Canvas {
            id: ribbonDecor
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: Math.round(parent.width * 0.28)
            anchors.topMargin: Math.round(22 * root.scaleFactor)
            width: Math.max(14, root.width * 0.04)
            height: Math.max(8, root.width * 0.02)
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                var cx = width / 2, cy = height / 2
                ctx.globalAlpha = 0.45
                ctx.beginPath()
                ctx.moveTo(cx, cy)
                ctx.bezierCurveTo(cx - width * 0.35, cy - height * 0.8,
                                  cx - width * 0.5, cy + height * 0.3, cx, cy)
                ctx.fillStyle = "#fbbf24"
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(cx, cy)
                ctx.bezierCurveTo(cx + width * 0.35, cy - height * 0.8,
                                  cx + width * 0.5, cy + height * 0.3, cx, cy)
                ctx.fillStyle = "#fbbf24"
                ctx.fill()
                ctx.beginPath()
                ctx.arc(cx, cy, Math.min(width, height) * 0.12, 0, Math.PI * 2)
                ctx.fillStyle = "#f59e0b"
                ctx.globalAlpha = 0.6
                ctx.fill()
            }
            Connections {
                target: root
                function onWidthChanged() { ribbonDecor.requestPaint() }
            }
        }

        // ── Floating sparkle particle system ───────────────────────────────
        Canvas {
            id: sparkleCanvas
            anchors.fill: parent
            visible: root.motionScale >= 0.05

            property var particles: []
            property int maxParticles: root.qualityHint > 0 ? 18 : 35
            property var sparkleColors: ["#ff4da6", "#c084fc", "#ffb088", "#6ee7b7", "#fbbf24"]

            Timer {
                running: root.pinkMode && root.motionScale >= 0.05 && sparkleCanvas.visible
                interval: Math.max(33, Math.round(50 / Math.max(0.1, root.motionScale)))
                repeat: true
                onTriggered: sparkleCanvas.tickParticles()
            }

            function tickParticles() {
                var dt = 0.05
                while (particles.length < maxParticles) {
                    var rnd = Math.random()
                    var shape = rnd < 0.4 ? "star" : rnd < 0.7 ? "dot" : rnd < 0.88 ? "heart" : "diamond"
                    particles.push({
                        x: Math.random() * width,
                        y: height + Math.random() * 20,
                        vx: (Math.random() - 0.5) * 0.4,
                        vy: -(0.3 + Math.random() * 0.9),
                        size: (2 + Math.random() * 5) * root.scaleFactor,
                        phase: Math.random() * Math.PI * 2,
                        life: 0,
                        maxLife: 4 + Math.random() * 8,
                        colorIdx: Math.floor(Math.random() * sparkleColors.length),
                        shape: shape,
                        spin: (Math.random() - 0.5) * 0.04
                    })
                }
                for (var i = particles.length - 1; i >= 0; i--) {
                    var p = particles[i]
                    p.life += dt
                    if (p.life >= p.maxLife) { particles.splice(i, 1); continue }
                    p.x += p.vx + Math.sin(p.phase + p.life * 1.2) * 0.4
                    p.y += p.vy
                    p.phase += p.spin
                }
                requestPaint()
            }

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                for (var i = 0; i < particles.length; i++) {
                    var p = particles[i]
                    var fadeIn = Math.min(1, p.life / 0.5)
                    var fadeOut = Math.max(0, 1 - Math.max(0, p.life - p.maxLife + 1.0))
                    var alpha = Math.min(fadeIn, fadeOut) * 0.7
                    if (alpha <= 0.01) continue
                    ctx.save()
                    ctx.globalAlpha = alpha
                    ctx.translate(p.x, p.y)
                    ctx.rotate(p.phase * 0.5)
                    var s = p.size
                    ctx.fillStyle = sparkleColors[p.colorIdx]
                    if (p.shape === "star") {
                        ctx.beginPath()
                        ctx.moveTo(0, -s)
                        ctx.lineTo(s * 0.25, -s * 0.25)
                        ctx.lineTo(s, 0)
                        ctx.lineTo(s * 0.25, s * 0.25)
                        ctx.lineTo(0, s)
                        ctx.lineTo(-s * 0.25, s * 0.25)
                        ctx.lineTo(-s, 0)
                        ctx.lineTo(-s * 0.25, -s * 0.25)
                        ctx.closePath()
                        ctx.fill()
                    } else if (p.shape === "heart") {
                        var hs = s * 0.6
                        ctx.beginPath()
                        ctx.moveTo(0, hs * 0.3)
                        ctx.bezierCurveTo(-hs * 0.5, -hs * 0.4, -hs, hs * 0.1, 0, hs)
                        ctx.moveTo(0, hs * 0.3)
                        ctx.bezierCurveTo(hs * 0.5, -hs * 0.4, hs, hs * 0.1, 0, hs)
                        ctx.fill()
                    } else if (p.shape === "diamond") {
                        var ds = s * 0.7
                        ctx.beginPath()
                        ctx.moveTo(0, -ds)
                        ctx.lineTo(ds * 0.5, 0)
                        ctx.lineTo(0, ds)
                        ctx.lineTo(-ds * 0.5, 0)
                        ctx.closePath()
                        ctx.fill()
                    } else {
                        var dr = s * 0.6
                        var grad = ctx.createRadialGradient(0, 0, 0, 0, 0, dr)
                        grad.addColorStop(0, sparkleColors[p.colorIdx])
                        grad.addColorStop(1, "transparent")
                        ctx.fillStyle = grad
                        ctx.beginPath()
                        ctx.arc(0, 0, dr, 0, Math.PI * 2)
                        ctx.fill()
                    }
                    ctx.restore()
                }
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
                text: root.pinkMode ? "\u2729 AURA \u2729" : "AURA  COCKPIT"
                anchors.centerIn: parent
                color: root.pinkMode ? root.clrMoodAccent : root.clrTextMuted
                font.pixelSize: root.fontTitle
                font.weight: root.pinkMode ? Font.DemiBold : Font.Medium
                font.letterSpacing: Math.round(4 * root.scaleFactor)
                Behavior on color { ColorAnimation { duration: 2000; easing.type: Easing.InOutQuad } }
            }

            // Title underline — gradient in pink mode, solid in blue
            Rectangle {
                anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
                width: titleLabel.width * 0.6
                height: 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: root.pinkMode ? root.clrLavender : root.accentColor(0.20 + root.accentIntensity * 0.15) }
                    GradientStop { position: 0.5; color: root.pinkMode ? root.clrAccent : root.accentColor(0.20 + root.accentIntensity * 0.15) }
                    GradientStop { position: 1.0; color: root.pinkMode ? root.clrPeach : root.accentColor(0.20 + root.accentIntensity * 0.15) }
                }
                opacity: root.pinkMode ? 0.7 : 1.0
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

                Canvas {
                    id: cpuGlowCanvas
                    anchors.centerIn: parent
                    width: parent.width + Math.round(12 + 12 * root.scaleFactor)
                    height: parent.height + Math.round(12 + 12 * root.scaleFactor)
                    opacity: 0.35 + root.accentIntensity * 0.20
                    onPaint: root.drawGlowHalo(getContext("2d"), width, height, root.smoothCpu, parent.width, root.scaleFactor)
                    Connections {
                        target: root
                        function onSmoothCpuChanged() { cpuGlowCanvas.requestPaint() }
                        function onAccentIntensityChanged() { cpuGlowCanvas.requestPaint() }
                    }
                }

                Canvas {
                    id: cpuArcCanvas
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height
                    onPaint: root.drawArcGauge(getContext("2d"), width, height, root.smoothCpu, root.scaleFactor)
                    Connections {
                        target: root
                        function onSmoothCpuChanged() { cpuArcCanvas.requestPaint() }
                        function onAccentIntensityChanged() { cpuArcCanvas.requestPaint() }
                        function onShimmerPhaseChanged() { if (root.pinkMode) cpuArcCanvas.requestPaint() }
                    }
                }

                // Center text
                Column {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 4
                    spacing: 2

                    Text {
                        id: cpuValueText
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.smoothCpu.toFixed(0) + "%"
                        color: root.clrTextPrimary
                        font.pixelSize: root.fontGaugeValue
                        font.weight: Font.Bold

                        property real prevValue: 0
                        property real bounceScale: 1.0

                        transform: Scale {
                            origin.x: cpuValueText.width / 2
                            origin.y: cpuValueText.height / 2
                            xScale: cpuValueText.bounceScale
                            yScale: cpuValueText.bounceScale
                        }

                        SequentialAnimation {
                            id: cpuBounceAnim
                            NumberAnimation { target: cpuValueText; property: "bounceScale"; to: 1.08; duration: 100; easing.type: Easing.OutQuad }
                            NumberAnimation { target: cpuValueText; property: "bounceScale"; to: 0.97; duration: 120; easing.type: Easing.InOutSine }
                            NumberAnimation { target: cpuValueText; property: "bounceScale"; to: 1.0; duration: 150; easing.type: Easing.OutBounce }
                        }

                        onTextChanged: {
                            if (!root.pinkMode || root.motionScale < 0.05) return
                            var val = root.smoothCpu
                            if (Math.abs(val - prevValue) > 5) cpuBounceAnim.restart()
                            prevValue = val
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "CPU"
                        color: root.clrTextSecondary
                        font.pixelSize: root.fontGaugeLabel
                        font.weight: Font.Medium
                        font.letterSpacing: 2.5
                    }

                    // Trend arrow
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: root.healthAvailable
                        text: root.trendArrow(root.cpuTrend)
                        color: root.trendColor(root.cpuTrend)
                        font.pixelSize: Math.max(7, root.fontGaugeLabel * 0.75)
                        font.weight: Font.Bold
                        opacity: root.trendOpacity(root.cpuTrend)
                    }

                    // Kawaii mood face (pink mode)
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: root.pinkMode
                        text: root.smoothCpu < 30 ? "\u2661" : root.smoothCpu < 65 ? "\u2727" : root.smoothCpu < 85 ? "\u2606" : "\u2662"
                        color: root.clrMoodAccent
                        font.pixelSize: Math.max(6, root.fontGaugeLabel * 0.7)
                        opacity: 0.6
                        Behavior on text { SequentialAnimation {
                            NumberAnimation { target: parent; property: "opacity"; to: 0; duration: 150 }
                            PropertyAction { }
                            NumberAnimation { target: parent; property: "opacity"; to: 0.6; duration: 300 }
                        }}
                    }
                }
            }

            // ── Memory gauge ─────────────────────────────────────────────────
            Item {
                id: memGaugeItem
                width: root.effectiveGaugeSize
                height: root.effectiveGaugeSize

                Canvas {
                    id: memGlowCanvas
                    anchors.centerIn: parent
                    width: parent.width + Math.round(12 + 12 * root.scaleFactor)
                    height: parent.height + Math.round(12 + 12 * root.scaleFactor)
                    opacity: 0.35 + root.accentIntensity * 0.20
                    onPaint: root.drawGlowHalo(getContext("2d"), width, height, root.smoothMem, parent.width, root.scaleFactor)
                    Connections {
                        target: root
                        function onSmoothMemChanged() { memGlowCanvas.requestPaint() }
                        function onAccentIntensityChanged() { memGlowCanvas.requestPaint() }
                    }
                }

                Canvas {
                    id: memArcCanvas
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height
                    onPaint: root.drawArcGauge(getContext("2d"), width, height, root.smoothMem, root.scaleFactor)
                    Connections {
                        target: root
                        function onSmoothMemChanged() { memArcCanvas.requestPaint() }
                        function onAccentIntensityChanged() { memArcCanvas.requestPaint() }
                        function onShimmerPhaseChanged() { if (root.pinkMode) memArcCanvas.requestPaint() }
                    }
                }

                // Center text
                Column {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 4
                    spacing: 2

                    Text {
                        id: memValueText
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.smoothMem.toFixed(0) + "%"
                        color: root.clrTextPrimary
                        font.pixelSize: root.fontGaugeValue
                        font.weight: Font.Bold

                        property real prevValue: 0
                        property real bounceScale: 1.0

                        transform: Scale {
                            origin.x: memValueText.width / 2
                            origin.y: memValueText.height / 2
                            xScale: memValueText.bounceScale
                            yScale: memValueText.bounceScale
                        }

                        SequentialAnimation {
                            id: memBounceAnim
                            NumberAnimation { target: memValueText; property: "bounceScale"; to: 1.08; duration: 100; easing.type: Easing.OutQuad }
                            NumberAnimation { target: memValueText; property: "bounceScale"; to: 0.97; duration: 120; easing.type: Easing.InOutSine }
                            NumberAnimation { target: memValueText; property: "bounceScale"; to: 1.0; duration: 150; easing.type: Easing.OutBounce }
                        }

                        onTextChanged: {
                            if (!root.pinkMode || root.motionScale < 0.05) return
                            var val = root.smoothMem
                            if (Math.abs(val - prevValue) > 5) memBounceAnim.restart()
                            prevValue = val
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "MEM"
                        color: root.clrTextSecondary
                        font.pixelSize: root.fontGaugeLabel
                        font.weight: Font.Medium
                        font.letterSpacing: 2.5
                    }

                    // Trend arrow
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: root.healthAvailable
                        text: root.trendArrow(root.memoryTrend)
                        color: root.trendColor(root.memoryTrend)
                        font.pixelSize: Math.max(7, root.fontGaugeLabel * 0.75)
                        font.weight: Font.Bold
                        opacity: root.trendOpacity(root.memoryTrend)
                    }

                    // Kawaii mood face (pink mode)
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: root.pinkMode
                        text: root.smoothMem < 40 ? "\u2661" : root.smoothMem < 70 ? "\u2727" : root.smoothMem < 85 ? "\u2606" : "\u2662"
                        color: root.clrMoodAccent
                        font.pixelSize: Math.max(6, root.fontGaugeLabel * 0.7)
                        opacity: 0.6
                    }
                }
            }
            // ── GPU gauge — same size as CPU/MEM, visible whenever data exists
            Item {
                id: gpuGaugeItem
                width: root.effectiveGaugeSize
                height: root.effectiveGaugeSize
                visible: root.gpuAvailable

                Canvas {
                    id: gpuGlowCanvas
                    anchors.centerIn: parent
                    width: parent.width + Math.round(10 + 10 * root.scaleFactor)
                    height: parent.height + Math.round(10 + 10 * root.scaleFactor)
                    opacity: 0.35 + root.accentIntensity * 0.20
                    onPaint: root.drawGlowHalo(getContext("2d"), width, height, root.smoothGpu, parent.width, root.scaleFactor)
                    Connections {
                        target: root
                        function onSmoothGpuChanged() { gpuGlowCanvas.requestPaint() }
                    }
                }

                Canvas {
                    id: gpuArcCanvas
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height
                    onPaint: root.drawArcGauge(getContext("2d"), width, height, root.smoothGpu, root.scaleFactor)
                    Connections {
                        target: root
                        function onSmoothGpuChanged() { gpuArcCanvas.requestPaint() }
                        function onShimmerPhaseChanged() { if (root.pinkMode) gpuArcCanvas.requestPaint() }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 4
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.smoothGpu.toFixed(0) + "%"
                        color: root.clrTextPrimary
                        font.pixelSize: root.fontGaugeValue
                        font.weight: Font.Bold
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "GPU"
                        color: root.clrTextSecondary
                        font.pixelSize: root.fontGaugeLabel
                        font.weight: Font.Medium
                        font.letterSpacing: 2.5
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.vramTotalBytes > 0
                              ? (root.vramUsedBytes / 1073741824).toFixed(1) + "/" + (root.vramTotalBytes / 1073741824).toFixed(0) + " GB"
                              : ""
                        color: root.clrTextMuted
                        font.pixelSize: Math.max(6, Math.round(root.fontGaugeLabel * 0.8))
                        visible: root.vramTotalBytes > 0
                    }
                }
            }
            // ── Health gauge — visible when analytics engine available ────
            Item {
                id: healthGaugeItem
                width: root.effectiveGaugeSize
                height: root.effectiveGaugeSize
                visible: root.healthAvailable

                Canvas {
                    id: healthGlowCanvas
                    anchors.centerIn: parent
                    width: parent.width + Math.round(10 + 10 * root.scaleFactor)
                    height: parent.height + Math.round(10 + 10 * root.scaleFactor)
                    opacity: 0.30 + root.accentIntensity * 0.15
                    onPaint: root.drawGlowHalo(getContext("2d"), width, height, root.smoothHealth, parent.width, root.scaleFactor)
                    Connections {
                        target: root
                        function onSmoothHealthChanged() { healthGlowCanvas.requestPaint() }
                    }
                }

                Canvas {
                    id: healthArcCanvas
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height
                    onPaint: root.drawHealthGauge(getContext("2d"), width, height, root.smoothHealth, root.scaleFactor)
                    Connections {
                        target: root
                        function onSmoothHealthChanged() { healthArcCanvas.requestPaint() }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 4
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.smoothHealth.toFixed(0)
                        color: root.clrTextPrimary
                        font.pixelSize: root.fontGaugeValue
                        font.weight: Font.Bold
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "HEALTH"
                        color: root.clrTextSecondary
                        font.pixelSize: root.fontGaugeLabel
                        font.weight: Font.Medium
                        font.letterSpacing: 2.5
                    }
                }
            }

            // ── Health sub-score pills (below health gauge) ──────────────
            Column {
                visible: root.healthAvailable && root.showExtended
                anchors.verticalCenter: parent.verticalCenter
                spacing: Math.round(3 * root.scaleFactor)

                Repeater {
                    model: [
                        { label: "CPU", score: root.healthCpu },
                        { label: "MEM", score: root.healthMemory },
                        { label: "DISK", score: root.healthDisk },
                        { label: "NET", score: root.healthNetwork }
                    ]
                    Rectangle {
                        width: Math.max(44, Math.round(52 * root.scaleFactor))
                        height: Math.max(14, Math.round(16 * root.scaleFactor))
                        radius: height * 0.3
                        color: root.healthGaugeColor(modelData.score, 0.12)
                        border.width: 1
                        border.color: root.healthGaugeColor(modelData.score, 0.30)

                        Text {
                            anchors.centerIn: parent
                            text: modelData.label + " " + modelData.score.toFixed(0)
                            color: root.healthGaugeColor(modelData.score, 0.90)
                            font.pixelSize: Math.max(7, Math.round(root.fontGaugeLabel * 0.7))
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.5
                        }
                    }
                }
            }
        } // Row gaugeRow

        // ── Between-gauge kawaii connector (pink mode) ─────────────────────
        Item {
            visible: root.pinkMode
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(16 * root.scaleFactor)
            Layout.topMargin: -Math.round(4 * root.scaleFactor)

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Math.round(6 * root.scaleFactor)

                Repeater {
                    model: 5
                    Text {
                        text: index === 2 ? "\u2665" : "\u00b7"
                        color: index === 2 ? root.clrMoodAccent : root.clrLavender
                        font.pixelSize: index === 2 ? Math.round(10 * root.scaleFactor) : Math.round(5 * root.scaleFactor)
                        opacity: index === 2 ? 0.7 : 0.35

                        property real breathScale: 1.0
                        SequentialAnimation on breathScale {
                            running: root.pinkMode && root.motionScale >= 0.05 && index === 2
                            loops: Animation.Infinite
                            NumberAnimation { to: 1.2; duration: 800; easing.type: Easing.InOutSine }
                            NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
                        }
                        transform: Scale {
                            origin.x: width / 2; origin.y: height / 2
                            xScale: breathScale; yScale: breathScale
                        }
                    }
                }
            }
        }

        // ── Alert badge ──────────────────────────────────────────────────────
        Rectangle {
            id: alertBadge
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: alertBadgeText.implicitHeight + Math.round(8 * root.scaleFactor)
            Layout.preferredWidth: alertBadgeText.implicitWidth + Math.round(24 * root.scaleFactor)
            visible: root.activeAlerts.length > 0
            radius: height / 2
            color: Qt.rgba(0.93, 0.27, 0.27, 0.15)
            border.width: 1
            border.color: Qt.rgba(0.93, 0.27, 0.27, 0.45)

            property real pulseOpacity: 1.0
            SequentialAnimation on pulseOpacity {
                running: alertBadge.visible
                loops: Animation.Infinite
                NumberAnimation { to: 0.6; duration: 800; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
            }

            Text {
                id: alertBadgeText
                anchors.centerIn: parent
                text: root.activeAlerts.length + (root.activeAlerts.length === 1 ? " ALERT" : " ALERTS")
                color: Qt.rgba(0.93, 0.27, 0.27, parent.pulseOpacity)
                font.pixelSize: Math.max(8, Math.round(root.fontGaugeLabel * 0.85))
                font.weight: Font.Bold
                font.letterSpacing: 1.5
            }
        }

        // ── Per-core CPU VU meter ────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(Math.max(34, 56 * root.scaleFactor))
            Layout.leftMargin: root.scaledMargin
            Layout.rightMargin: root.scaledMargin
            visible: root.coreCount > 0 && root.showExtended

            // Label row
            Rectangle {
                anchors { left: parent.left; top: parent.top }
                width: coreLabel.implicitWidth + (root.pinkMode ? Math.round(12 * root.scaleFactor) : 0)
                height: coreLabel.implicitHeight + (root.pinkMode ? Math.round(4 * root.scaleFactor) : 0)
                radius: root.pinkMode ? height / 2 : 0
                color: root.pinkMode ? Qt.rgba(1, 0.3, 0.65, 0.08) : "transparent"
                Text {
                    id: coreLabel
                    anchors.centerIn: parent
                    text: root.pinkMode ? "\u2728 Cores" : "CORES"
                    color: root.clrTextMuted
                    font.pixelSize: root.fontSparkLabel
                    font.weight: Font.Medium
                    font.letterSpacing: 2.5
                }
            }

            Text {
                anchors { right: parent.right; top: parent.top }
                text: {
                    var cores = root.perCoreCpu
                    if (!cores || cores.length === 0) return ""
                    var maxPct = 0, maxIdx = 0
                    for (var i = 0; i < cores.length; i++) {
                        if (cores[i] > maxPct) { maxPct = cores[i]; maxIdx = i }
                    }
                    return root.coreCount + " threads  \u2022  hottest #" + maxIdx + " " + Math.round(maxPct) + "%"
                }
                color: root.clrTextSecondary
                font.pixelSize: Math.max(7, root.fontSparkValue - 1)
                font.weight: Font.Bold
                font.letterSpacing: 0.3
            }

            Canvas {
                id: perCoreCanvas
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: parent.height - Math.round(Math.max(12, 18 * root.scaleFactor))

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    var cores = root.perCoreCpu
                    if (!cores || cores.length === 0) return
                    var n = cores.length
                    var gap = Math.max(1, Math.round(2 * root.scaleFactor))
                    var totalGaps = gap * (n - 1)
                    var barW = Math.max(2, Math.floor((width - totalGaps) / n))
                    var totalW = barW * n + gap * (n - 1)
                    var offsetX = Math.floor((width - totalW) / 2)
                    var maxH = height - 2

                    // Find hottest core for highlight
                    var maxPct = 0, maxIdx = 0
                    for (var m = 0; m < n; m++) {
                        if (cores[m] > maxPct) { maxPct = cores[m]; maxIdx = m }
                    }

                    for (var i = 0; i < n; i++) {
                        var pct = cores[i]
                        var bh = Math.max(1, (pct / 100.0) * maxH)
                        var x = offsetX + i * (barW + gap)
                        var y = height - bh

                        // Track background
                        ctx.fillStyle = root.clrTrack
                        ctx.fillRect(x, 0, barW, height)

                        // Value bar with rounded top
                        var gc = root.gaugeColor(pct, i === maxIdx ? 1.0 : 0.75)
                        ctx.fillStyle = gc
                        ctx.beginPath()
                        var radius = Math.min(barW / 2, 3)
                        ctx.moveTo(x, height)
                        ctx.lineTo(x, y + radius)
                        ctx.arcTo(x, y, x + radius, y, radius)
                        ctx.arcTo(x + barW, y, x + barW, y + radius, radius)
                        ctx.lineTo(x + barW, height)
                        ctx.closePath()
                        ctx.fill()
                    }
                }
                Connections {
                    target: root
                    function onPerCoreCpuChanged() { perCoreCanvas.requestPaint() }
                }
            }
        }

        Item { Layout.preferredHeight: root.scaledSpacing * 1.3; Layout.fillWidth: true }

        // ── Section divider (animated in pink mode) ───────────────────────
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.pinkMode ? Math.round(8 * root.scaleFactor) : 1

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.82
                height: 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.2; color: root.pinkMode ? root.clrLavender : root.accentColor(0.15 + root.accentIntensity * 0.08) }
                    GradientStop { position: 0.5; color: root.pinkMode ? root.clrMoodAccent : root.accentColor(0.15 + root.accentIntensity * 0.08) }
                    GradientStop { position: 0.8; color: root.pinkMode ? root.clrPeach : root.accentColor(0.15 + root.accentIntensity * 0.08) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
                opacity: root.pinkMode ? 0.45 : 1.0
            }

            // Traveling light dot on divider (pink mode)
            Rectangle {
                id: dividerDot1
                visible: root.pinkMode && root.motionScale >= 0.05
                width: Math.round(12 * root.scaleFactor)
                height: 3
                radius: 1.5
                y: (parent.height - height) / 2
                color: root.clrMoodAccent
                opacity: 0.6

                property real travel: 0
                NumberAnimation on travel {
                    running: root.pinkMode && root.motionScale >= 0.05
                    from: 0; to: 1; duration: 4000
                    loops: Animation.Infinite
                }
                x: parent.width * 0.09 + travel * parent.width * 0.82
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
            Rectangle {
                anchors { left: parent.left; top: parent.top }
                width: cpuSparkLabel.implicitWidth + (root.pinkMode ? Math.round(12 * root.scaleFactor) : 0)
                height: cpuSparkLabel.implicitHeight + (root.pinkMode ? Math.round(4 * root.scaleFactor) : 0)
                radius: root.pinkMode ? height / 2 : 0
                color: root.pinkMode ? Qt.rgba(1, 0.3, 0.65, 0.08) : "transparent"
                Text {
                    id: cpuSparkLabel
                    anchors.centerIn: parent
                    text: root.pinkMode ? "\u2728 Processor" : "CPU  HISTORY"
                    color: root.clrTextMuted
                    font.pixelSize: root.fontSparkLabel
                    font.weight: Font.Medium
                    font.letterSpacing: 2.5
                }
            }

            Text {
                anchors { right: parent.right; top: parent.top }
                text: root.smoothCpu.toFixed(1) + "%"
                color: root.gaugeColor(root.smoothCpu, 1.0)
                font.pixelSize: root.fontSparkValue
                font.weight: Font.Bold
                font.letterSpacing: 0.5
            }

            // Chart canvas — auto-scales Y-axis to observed range
            Canvas {
                id: cpuSparkCanvas
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: parent.height - Math.round(Math.max(12, 18 * root.scaleFactor))

                property var samples: []
                property real lastDotX: 0
                property real lastDotY: 0

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

                    var n = samples.length
                    var pad = 2
                    var cw = width
                    var ch = height - pad

                    // Auto-scale: find observed min/max with padding
                    var sMin = 100, sMax = 0
                    for (var s = 0; s < n; s++) {
                        sMin = Math.min(sMin, samples[s])
                        sMax = Math.max(sMax, samples[s])
                    }
                    var range = sMax - sMin
                    var padding = Math.max(5, range * 0.2)  // at least 5% padding
                    var yMin = Math.max(0, Math.floor(sMin - padding))
                    var yMax = Math.min(100, Math.ceil(sMax + padding))
                    if (yMax - yMin < 10) { // enforce minimum 10% visible range
                        var mid = (yMin + yMax) / 2
                        yMin = Math.max(0, mid - 5)
                        yMax = Math.min(100, mid + 5)
                    }
                    var yRange = yMax - yMin

                    // Zone fills (pink mode) — safe/warn/danger background bands
                    if (root.pinkMode) {
                        var zones = [
                            { pctFrom: 0, pctTo: 50, color: Qt.rgba(0.431, 0.906, 0.718, 0.04) },
                            { pctFrom: 50, pctTo: 80, color: Qt.rgba(1.0, 0.690, 0.533, 0.04) },
                            { pctFrom: 80, pctTo: 100, color: Qt.rgba(0.957, 0.447, 0.447, 0.05) }
                        ]
                        for (var zi = 0; zi < zones.length; zi++) {
                            var zf = zones[zi]
                            var zy1 = ch - Math.max(0, Math.min(1, (zf.pctTo - yMin) / yRange)) * ch + pad
                            var zy2 = ch - Math.max(0, Math.min(1, (zf.pctFrom - yMin) / yRange)) * ch + pad
                            if (zy2 > zy1) {
                                ctx.fillStyle = zf.color
                                ctx.fillRect(0, zy1, cw, zy2 - zy1)
                            }
                        }
                    }

                    var pts = []
                    for (var i = 0; i < n; i++) {
                        var x = (i / (n - 1)) * cw
                        var norm = (samples[i] - yMin) / yRange
                        var y = ch - norm * ch + pad
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

                    var acR = root.accentRed, acG = root.accentGreen, acB = root.accentBlue
                    var fillGrad = ctx.createLinearGradient(0, 0, 0, ch)
                    if (root.pinkMode) {
                        fillGrad.addColorStop(0.0, Qt.rgba(0.753, 0.518, 0.988, 0.30))
                        fillGrad.addColorStop(0.5, Qt.rgba(1.0, 0.302, 0.651, 0.18))
                        fillGrad.addColorStop(1.0, Qt.rgba(1.0, 0.690, 0.533, 0.02))
                    } else {
                        fillGrad.addColorStop(0.0, Qt.rgba(acR, acG, acB, 0.28))
                        fillGrad.addColorStop(1.0, Qt.rgba(acR, acG, acB, 0.02))
                    }
                    ctx.fillStyle = fillGrad
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
                    ctx.lineWidth = 1.5
                    ctx.lineJoin = "round"
                    ctx.stroke()

                    // Latest value dot
                    var lp = pts[n-1]
                    ctx.beginPath()
                    ctx.arc(lp.x, lp.y, 2.5, 0, Math.PI * 2, false)
                    ctx.fillStyle = Qt.rgba(acR, acG, acB, 1.0)
                    ctx.fill()
                    lastDotX = lp.x; lastDotY = lp.y

                    // Y-axis scale labels (top = max, bottom = min)
                    ctx.font = Math.max(7, Math.round(8 * root.scaleFactor)) + "px 'Segoe UI'"
                    ctx.fillStyle = Qt.rgba(acR, acG, acB, 0.40)
                    ctx.textAlign = "left"
                    ctx.fillText(Math.round(yMax) + "%", 2, 10)
                    ctx.fillText(Math.round(yMin) + "%", 2, ch)
                }

                Connections {
                    target: root
                    function onAccentRedChanged()   { cpuSparkCanvas.requestPaint() }
                    function onAccentGreenChanged() { cpuSparkCanvas.requestPaint() }
                    function onAccentBlueChanged()  { cpuSparkCanvas.requestPaint() }
                }
            }

            // Pulsing glow dot at sparkline endpoint (pink mode)
            Rectangle {
                id: cpuSparkGlow
                visible: root.pinkMode && cpuSparkCanvas.samples.length > 1
                x: cpuSparkCanvas.x + cpuSparkCanvas.lastDotX - width / 2
                y: cpuSparkCanvas.y + cpuSparkCanvas.lastDotY - height / 2
                width: 8 * root.scaleFactor
                height: width
                radius: width / 2
                color: root.clrMoodAccent
                opacity: 0.6

                property real breathScale: 1.0
                SequentialAnimation on breathScale {
                    running: root.pinkMode && root.motionScale >= 0.05
                    loops: Animation.Infinite
                    NumberAnimation { to: 1.4; duration: 800; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
                }
                transform: Scale {
                    origin.x: cpuSparkGlow.width / 2
                    origin.y: cpuSparkGlow.height / 2
                    xScale: cpuSparkGlow.breathScale
                    yScale: cpuSparkGlow.breathScale
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

            Rectangle {
                id: memSparkPill
                anchors { left: parent.left; top: parent.top }
                width: memSparkLabel.implicitWidth + (root.pinkMode ? Math.round(12 * root.scaleFactor) : 0)
                height: memSparkLabel.implicitHeight + (root.pinkMode ? Math.round(4 * root.scaleFactor) : 0)
                radius: root.pinkMode ? height / 2 : 0
                color: root.pinkMode ? Qt.rgba(1, 0.3, 0.65, 0.08) : "transparent"
                Text {
                    id: memSparkLabel
                    anchors.centerIn: parent
                    text: root.pinkMode ? "\u2728 Memory" : "MEM  HISTORY"
                    color: root.clrTextMuted
                    font.pixelSize: root.fontSparkLabel
                    font.weight: Font.Medium
                    font.letterSpacing: 2.5
                }
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
                property real lastDotX: 0
                property real lastDotY: 0

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

                    // Auto-scale: find observed min/max with padding
                    var sMin = 100, sMax = 0
                    for (var s = 0; s < n; s++) {
                        sMin = Math.min(sMin, samples[s])
                        sMax = Math.max(sMax, samples[s])
                    }
                    var range = sMax - sMin
                    var padding = Math.max(5, range * 0.2)
                    var yMin = Math.max(0, Math.floor(sMin - padding))
                    var yMax = Math.min(100, Math.ceil(sMax + padding))
                    if (yMax - yMin < 10) {
                        var mid = (yMin + yMax) / 2
                        yMin = Math.max(0, mid - 5)
                        yMax = Math.min(100, mid + 5)
                    }
                    var yRange = yMax - yMin

                    var pts = []
                    for (var i = 0; i < n; i++) {
                        var x = (i / (n - 1)) * cw
                        var norm = (samples[i] - yMin) / yRange
                        var y = ch - norm * ch + pad
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
                    if (root.pinkMode) {
                        fillGrad.addColorStop(0.0, Qt.rgba(0.753, 0.518, 0.988, 0.28))
                        fillGrad.addColorStop(0.5, Qt.rgba(0.431, 0.906, 0.718, 0.16))
                        fillGrad.addColorStop(1.0, Qt.rgba(1.0, 0.690, 0.533, 0.02))
                    } else {
                        fillGrad.addColorStop(0.0, Qt.rgba(0.15, 0.65, 0.78, 0.28))
                        fillGrad.addColorStop(1.0, Qt.rgba(0.15, 0.65, 0.78, 0.02))
                    }
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
                    lastDotX = lp.x; lastDotY = lp.y

                    // Y-axis scale labels
                    ctx.font = Math.max(7, Math.round(8 * root.scaleFactor)) + "px 'Segoe UI'"
                    ctx.fillStyle = Qt.rgba(0.15, 0.65, 0.78, 0.40)
                    ctx.textAlign = "left"
                    ctx.fillText(Math.round(yMax) + "%", 2, 10)
                    ctx.fillText(Math.round(yMin) + "%", 2, ch)
                }
            }

            // Pulsing glow dot at sparkline endpoint (pink mode)
            Rectangle {
                id: memSparkGlow
                visible: root.pinkMode && memSparkCanvas.samples.length > 1
                x: memSparkCanvas.x + memSparkCanvas.lastDotX - width / 2
                y: memSparkCanvas.y + memSparkCanvas.lastDotY - height / 2
                width: 8 * root.scaleFactor
                height: width
                radius: width / 2
                color: root.clrMint
                opacity: 0.6

                property real breathScale: 1.0
                SequentialAnimation on breathScale {
                    running: root.pinkMode && root.motionScale >= 0.05
                    loops: Animation.Infinite
                    NumberAnimation { to: 1.4; duration: 800; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
                }
                transform: Scale {
                    origin.x: memSparkGlow.width / 2
                    origin.y: memSparkGlow.height / 2
                    xScale: memSparkGlow.breathScale
                    yScale: memSparkGlow.breathScale
                }
            }
        }

        Item { Layout.preferredHeight: root.scaledSpacing * 0.6; Layout.fillWidth: true; visible: root.gpuAvailable }

        // ── Sparkline — GPU ─────────────────────────────────────────────────
        Item {
            id: gpuSparkRow
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Math.round(Math.max(28, 48 * root.scaleFactor))
            Layout.leftMargin: root.scaledMargin
            Layout.rightMargin: root.scaledMargin
            visible: root.gpuAvailable

            // Label row
            Rectangle {
                anchors { left: parent.left; top: parent.top }
                width: gpuSparkLabel.implicitWidth + (root.pinkMode ? Math.round(12 * root.scaleFactor) : 0)
                height: gpuSparkLabel.implicitHeight + (root.pinkMode ? Math.round(4 * root.scaleFactor) : 0)
                radius: root.pinkMode ? height / 2 : 0
                color: root.pinkMode ? Qt.rgba(1.0, 0.69, 0.53, 0.08) : "transparent"
                Text {
                    id: gpuSparkLabel
                    anchors.centerIn: parent
                    text: root.pinkMode ? "\u2728 Graphics" : "GPU  HISTORY"
                    color: root.clrTextMuted
                    font.pixelSize: root.fontSparkLabel
                    font.weight: Font.Medium
                    font.letterSpacing: 2.5
                }
            }

            Text {
                anchors { right: parent.right; top: parent.top }
                text: {
                    var vramText = ""
                    if (root.vramTotalBytes > 0) {
                        vramText = "  " + (root.vramUsedBytes / 1073741824).toFixed(1) + "/" + (root.vramTotalBytes / 1073741824).toFixed(0) + " GB"
                    }
                    return root.smoothGpu.toFixed(1) + "%" + vramText
                }
                color: root.pinkMode ? Qt.rgba(1.0, 0.69, 0.53, 1.0) : Qt.rgba(0.961, 0.620, 0.043, 1.0)
                font.pixelSize: root.fontSparkValue
                font.weight: Font.Bold
                font.letterSpacing: 0.5
            }

            // Chart canvas
            Canvas {
                id: gpuSparkCanvas
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: parent.height - Math.round(Math.max(12, 18 * root.scaleFactor))

                property var samples: []
                property real lastDotX: 0
                property real lastDotY: 0

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

                    var n = samples.length
                    var pad = 2
                    var cw = width
                    var ch = height - pad

                    // Auto-scale Y
                    var sMin = 100, sMax = 0
                    for (var s = 0; s < n; s++) {
                        sMin = Math.min(sMin, samples[s])
                        sMax = Math.max(sMax, samples[s])
                    }
                    var range = sMax - sMin
                    var padding = Math.max(5, range * 0.2)
                    var yMin = Math.max(0, Math.floor(sMin - padding))
                    var yMax = Math.min(100, Math.ceil(sMax + padding))
                    if (yMax - yMin < 10) {
                        var mid = (yMin + yMax) / 2
                        yMin = Math.max(0, mid - 5)
                        yMax = Math.min(100, mid + 5)
                    }
                    var yRange = yMax - yMin

                    // Zone fills (pink mode)
                    if (root.pinkMode) {
                        var zones = [
                            { pctFrom: 0, pctTo: 50, color: Qt.rgba(0.431, 0.906, 0.718, 0.04) },
                            { pctFrom: 50, pctTo: 80, color: Qt.rgba(1.0, 0.690, 0.533, 0.04) },
                            { pctFrom: 80, pctTo: 100, color: Qt.rgba(0.957, 0.447, 0.447, 0.05) }
                        ]
                        for (var zi = 0; zi < zones.length; zi++) {
                            var zf = zones[zi]
                            var zy1 = ch - Math.max(0, Math.min(1, (zf.pctTo - yMin) / yRange)) * ch + pad
                            var zy2 = ch - Math.max(0, Math.min(1, (zf.pctFrom - yMin) / yRange)) * ch + pad
                            if (zy2 > zy1) {
                                ctx.fillStyle = zf.color
                                ctx.fillRect(0, zy1, cw, zy2 - zy1)
                            }
                        }
                    }

                    var pts = []
                    for (var i = 0; i < n; i++) {
                        var x = (i / (n - 1)) * cw
                        var norm = (samples[i] - yMin) / yRange
                        var y = ch - norm * ch + pad
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

                    var fillGrad = ctx.createLinearGradient(0, 0, 0, ch)
                    if (root.pinkMode) {
                        fillGrad.addColorStop(0.0, Qt.rgba(1.0, 0.690, 0.533, 0.28))
                        fillGrad.addColorStop(0.5, Qt.rgba(0.957, 0.447, 0.447, 0.15))
                        fillGrad.addColorStop(1.0, Qt.rgba(0.961, 0.620, 0.043, 0.02))
                    } else {
                        fillGrad.addColorStop(0.0, Qt.rgba(0.961, 0.620, 0.043, 0.26))
                        fillGrad.addColorStop(1.0, Qt.rgba(0.961, 0.620, 0.043, 0.02))
                    }
                    ctx.fillStyle = fillGrad
                    ctx.fill()

                    // Crisp stroke
                    ctx.beginPath()
                    ctx.moveTo(pts[0].x, pts[0].y)
                    for (var k = 1; k < n - 1; k++) {
                        var lmx = (pts[k].x + pts[k+1].x) / 2
                        var lmy = (pts[k].y + pts[k+1].y) / 2
                        ctx.quadraticCurveTo(pts[k].x, pts[k].y, lmx, lmy)
                    }
                    ctx.lineTo(pts[n-1].x, pts[n-1].y)
                    ctx.strokeStyle = root.pinkMode
                        ? Qt.rgba(1.0, 0.690, 0.533, 0.70)
                        : Qt.rgba(0.961, 0.620, 0.043, 0.65)
                    ctx.lineWidth = 1.5
                    ctx.lineJoin = "round"
                    ctx.stroke()

                    // Latest value dot
                    var lp = pts[n-1]
                    ctx.beginPath()
                    ctx.arc(lp.x, lp.y, 2.5, 0, Math.PI * 2, false)
                    ctx.fillStyle = root.pinkMode
                        ? Qt.rgba(1.0, 0.690, 0.533, 1.0)
                        : Qt.rgba(0.961, 0.620, 0.043, 1.0)
                    ctx.fill()
                    lastDotX = lp.x; lastDotY = lp.y

                    // Y-axis scale labels
                    ctx.font = Math.max(7, Math.round(8 * root.scaleFactor)) + "px 'Segoe UI'"
                    ctx.fillStyle = root.pinkMode
                        ? Qt.rgba(1.0, 0.690, 0.533, 0.40)
                        : Qt.rgba(0.961, 0.620, 0.043, 0.40)
                    ctx.textAlign = "left"
                    ctx.fillText(Math.round(yMax) + "%", 2, 10)
                    ctx.fillText(Math.round(yMin) + "%", 2, ch)
                }

                Connections {
                    target: root
                    function onSmoothGpuChanged() { gpuSparkCanvas.pushSample(root.smoothGpu) }
                }
            }

            // Pulsing glow dot at sparkline endpoint (pink mode)
            Rectangle {
                id: gpuSparkGlow
                visible: root.pinkMode && gpuSparkCanvas.samples.length > 1
                x: gpuSparkCanvas.x + gpuSparkCanvas.lastDotX - width / 2
                y: gpuSparkCanvas.y + gpuSparkCanvas.lastDotY - height / 2
                width: 8 * root.scaleFactor
                height: width
                radius: width / 2
                color: Qt.rgba(1.0, 0.690, 0.533, 1.0)
                opacity: 0.6

                property real breathScale: 1.0
                SequentialAnimation on breathScale {
                    running: root.pinkMode && root.motionScale >= 0.05
                    loops: Animation.Infinite
                    NumberAnimation { to: 1.4; duration: 800; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
                }
                transform: Scale {
                    origin.x: gpuSparkGlow.width / 2
                    origin.y: gpuSparkGlow.height / 2
                    xScale: gpuSparkGlow.breathScale
                    yScale: gpuSparkGlow.breathScale
                }
            }
        }

        Item { Layout.preferredHeight: root.scaledSpacing * 0.8; Layout.fillWidth: true }

        // ── Disk I/O sparklines ──────────────────────────────────────────────
        Item {
            id: diskSparkRow
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Math.round(Math.max(28, 48 * root.scaleFactor))
            Layout.leftMargin: root.scaledMargin
            Layout.rightMargin: root.scaledMargin
            visible: root.showExtended

            Rectangle {
                anchors { left: parent.left; top: parent.top }
                width: diskSparkLabel.implicitWidth + (root.pinkMode ? Math.round(12 * root.scaleFactor) : 0)
                height: diskSparkLabel.implicitHeight + (root.pinkMode ? Math.round(4 * root.scaleFactor) : 0)
                radius: root.pinkMode ? height / 2 : 0
                color: root.pinkMode ? Qt.rgba(1, 0.3, 0.65, 0.08) : "transparent"
                Text {
                    id: diskSparkLabel
                    anchors.centerIn: parent
                    text: root.pinkMode ? "\u2728 Storage" : "DISK  I/O"
                    color: root.clrTextMuted
                    font.pixelSize: root.fontSparkLabel
                    font.weight: Font.Medium
                    font.letterSpacing: 2.5
                }
            }
            Text {
                anchors { right: parent.right; top: parent.top }
                text: root.formatRate(root.diskReadBps) + " R / " + root.formatRate(root.diskWriteBps) + " W"
                color: root.clrTextSecondary
                font.pixelSize: Math.max(7, root.fontSparkValue - 1)
                font.weight: Font.Bold
                font.letterSpacing: 0.3
            }

            Canvas {
                id: diskSparkCanvas
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: parent.height - Math.round(Math.max(12, 18 * root.scaleFactor))

                property var readSamples: []
                property var writeSamples: []

                function pushSamples(r, w) {
                    readSamples.push(r); writeSamples.push(w)
                    var cap = root.qualityHint > 0 ? 80 : 120
                    if (readSamples.length > cap) { readSamples.shift(); writeSamples.shift() }
                    requestPaint()
                }

                onPaint: root.drawDualSparkline(
                    getContext("2d"), width, height,
                    readSamples, writeSamples,
                    root.pinkMode ? Qt.rgba(0.933, 0.306, 0.643, 1) : Qt.rgba(0.024, 0.714, 0.831, 1),
                    root.pinkMode ? Qt.rgba(0.961, 0.620, 0.498, 1) : Qt.rgba(0.961, 0.620, 0.043, 1)
                )
            }
            Connections {
                target: root
                function onDiskReadBpsChanged() { diskSparkCanvas.pushSamples(root.diskReadBps, root.diskWriteBps) }
            }
        }

        Item { Layout.preferredHeight: root.scaledSpacing * 0.6; Layout.fillWidth: true; visible: root.showExtended }

        // ── Network I/O sparklines ───────────────────────────────────────────
        Item {
            id: netSparkRow
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Math.round(Math.max(28, 48 * root.scaleFactor))
            Layout.leftMargin: root.scaledMargin
            Layout.rightMargin: root.scaledMargin
            visible: root.showExtended

            Rectangle {
                anchors { left: parent.left; top: parent.top }
                width: netSparkLabel.implicitWidth + (root.pinkMode ? Math.round(12 * root.scaleFactor) : 0)
                height: netSparkLabel.implicitHeight + (root.pinkMode ? Math.round(4 * root.scaleFactor) : 0)
                radius: root.pinkMode ? height / 2 : 0
                color: root.pinkMode ? Qt.rgba(1, 0.3, 0.65, 0.08) : "transparent"
                Text {
                    id: netSparkLabel
                    anchors.centerIn: parent
                    text: root.pinkMode ? "\u2728 Internet" : "NETWORK"
                    color: root.clrTextMuted
                    font.pixelSize: root.fontSparkLabel
                    font.weight: Font.Medium
                    font.letterSpacing: 2.5
                }
            }
            Text {
                anchors { right: parent.right; top: parent.top }
                text: root.formatRate(root.netRecvBps) + " \u2193 / " + root.formatRate(root.netSentBps) + " \u2191"
                color: root.clrTextSecondary
                font.pixelSize: Math.max(7, root.fontSparkValue - 1)
                font.weight: Font.Bold
                font.letterSpacing: 0.3
            }

            Canvas {
                id: netSparkCanvas
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: parent.height - Math.round(Math.max(12, 18 * root.scaleFactor))

                property var recvSamples: []
                property var sentSamples: []

                function pushSamples(r, s) {
                    recvSamples.push(r); sentSamples.push(s)
                    var cap = root.qualityHint > 0 ? 80 : 120
                    if (recvSamples.length > cap) { recvSamples.shift(); sentSamples.shift() }
                    requestPaint()
                }

                onPaint: root.drawDualSparkline(
                    getContext("2d"), width, height,
                    recvSamples, sentSamples,
                    root.pinkMode ? Qt.rgba(0.933, 0.400, 0.700, 1) : Qt.rgba(0.180, 0.800, 0.440, 1),
                    root.pinkMode ? Qt.rgba(0.800, 0.306, 0.900, 1) : Qt.rgba(0.231, 0.510, 0.965, 1)
                )
            }
            Connections {
                target: root
                function onNetRecvBpsChanged() { netSparkCanvas.pushSamples(root.netRecvBps, root.netSentBps) }
            }
        }

        Item { Layout.preferredHeight: root.scaledSpacing * 0.6; Layout.fillWidth: true; visible: root.showExtended }

        // ── Thermal badges ───────────────────────────────────────────────────
        Flow {
            id: thermalFlow
            Layout.fillWidth: true
            Layout.leftMargin: root.scaledMargin
            Layout.rightMargin: root.scaledMargin
            spacing: Math.round(4 * root.scaleFactor)
            visible: root.thermalAvailable && root.showGpuThermal && root.thermalSensors.length > 0

            Repeater {
                model: root.thermalSensors
                delegate: Rectangle {
                    width: thermalBadgeText.implicitWidth + Math.round(12 * root.scaleFactor)
                    height: Math.round(Math.max(18, 22 * root.scaleFactor))
                    radius: height / 2
                    color: {
                        var temp = modelData.current
                        var crit = modelData.hasCritical ? modelData.critical : 105
                        var high = modelData.hasHigh ? modelData.high : 85
                        if (temp >= crit) return Qt.rgba(0.937, 0.267, 0.267, 0.35)
                        if (temp >= high) return Qt.rgba(0.961, 0.620, 0.043, 0.25)
                        if (temp >= 50)  return Qt.rgba(0.961, 0.800, 0.200, 0.15)
                        return Qt.rgba(root.accentRed, root.accentGreen, root.accentBlue, 0.12)
                    }
                    border.width: 1
                    border.color: {
                        var temp = modelData.current
                        var crit = modelData.hasCritical ? modelData.critical : 105
                        var high = modelData.hasHigh ? modelData.high : 85
                        if (temp >= crit) return Qt.rgba(0.937, 0.267, 0.267, 0.60)
                        if (temp >= high) return Qt.rgba(0.961, 0.620, 0.043, 0.40)
                        return Qt.rgba(root.accentRed, root.accentGreen, root.accentBlue, 0.20)
                    }

                    // Inner highlight glow (pink mode)
                    Rectangle {
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        height: parent.height * 0.5
                        radius: parent.radius
                        visible: root.pinkMode
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.08) }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }

                    Text {
                        id: thermalBadgeText
                        anchors.centerIn: parent
                        text: modelData.label + " " + Math.round(modelData.current) + "\u00b0C"
                        color: {
                            var temp = modelData.current
                            var crit = modelData.hasCritical ? modelData.critical : 105
                            if (temp >= crit) return "#ef4444"
                            return root.clrTextSecondary
                        }
                        font.pixelSize: Math.max(7, Math.round(9 * root.scaleFactor))
                        font.weight: Font.Medium
                        font.letterSpacing: 0.5
                    }
                }
            }
        }

        Item { Layout.preferredHeight: root.scaledSpacing * 0.8; Layout.fillWidth: true }

        // ── Final divider (animated in pink mode) ────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.pinkMode ? Math.round(8 * root.scaleFactor) : 1

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.82
                height: 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.2; color: root.pinkMode ? root.clrPeach : root.accentColor(0.12 + root.frostIntensity * 0.06) }
                    GradientStop { position: 0.5; color: root.pinkMode ? root.clrMoodAccent : root.accentColor(0.12 + root.frostIntensity * 0.06) }
                    GradientStop { position: 0.8; color: root.pinkMode ? root.clrLavender : root.accentColor(0.12 + root.frostIntensity * 0.06) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
                opacity: root.pinkMode ? 0.45 : 1.0
            }

            // Traveling light dot (reverse direction)
            Rectangle {
                visible: root.pinkMode && root.motionScale >= 0.05
                width: Math.round(12 * root.scaleFactor)
                height: 3
                radius: 1.5
                y: (parent.height - height) / 2
                color: root.clrLavender
                opacity: 0.6

                property real travel: 0
                NumberAnimation on travel {
                    running: root.pinkMode && root.motionScale >= 0.05
                    from: 1; to: 0; duration: 5000
                    loops: Animation.Infinite
                }
                x: parent.width * 0.09 + travel * parent.width * 0.82
            }
        }

        Item { Layout.preferredHeight: root.scaledSpacing * 0.8; Layout.fillWidth: true }

        // ── Status line ──────────────────────────────────────────────────────
        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 6

            // Kawaii prefix (pink mode)
            Text {
                visible: root.pinkMode
                text: "\u2661"
                color: root.clrMoodAccent
                font.pixelSize: root.fontStatus
                anchors.verticalCenter: parent.verticalCenter
                opacity: 0.6
            }

            // Pulse indicator dot
            Rectangle {
                id: statusDot
                width: root.pinkMode ? 6 : 5
                height: width
                radius: width / 2
                anchors.verticalCenter: parent.verticalCenter
                color: root.pinkMode ? root.clrMoodAccent : root.accentColor(0.60 + root.accentIntensity * 0.40)

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
                color: root.pinkMode ? root.clrTextSecondary : root.clrTextMuted
                font.pixelSize: root.fontStatus
                font.letterSpacing: root.pinkMode ? 1.0 : 0.5
                font.weight: root.pinkMode ? Font.Medium : Font.Normal
                elide: Text.ElideRight
                width: root.width * 0.65
                horizontalAlignment: Text.AlignLeft
            }

            // Kawaii suffix (pink mode)
            Text {
                visible: root.pinkMode
                text: "\u2661"
                color: root.clrMoodAccent
                font.pixelSize: root.fontStatus
                anchors.verticalCenter: parent.verticalCenter
                opacity: 0.6
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
        border.width: root.pinkMode ? 1.5 : 1
        border.color: root.pinkMode
            ? Qt.rgba(root.clrMoodAccent.r, root.clrMoodAccent.g, root.clrMoodAccent.b, 0.35)
            : root.accentColor(Math.min(0.45, root.accentAlpha + root.accentIntensity * 0.20))

        Behavior on border.color {
            ColorAnimation { duration: root.pinkMode ? 2000 : 400; easing.type: Easing.OutCubic }
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
