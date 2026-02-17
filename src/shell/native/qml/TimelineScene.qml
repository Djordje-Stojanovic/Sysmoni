// ============================================================================
// Aura Timeline Scene — DVR Area Chart
// ============================================================================
//
// Three-series area chart: CPU, Memory, GPU. Resolution-agnostic — all
// dimensions derive from canvas width/height as fractions. Zero magic pixel
// values. Repaints only when data changes (~every 1 second).
//
// Visual hierarchy (back to front):
//   1. Background gradient
//   2. Grid lines + Y-axis labels (rendered in canvas)
//   3. Memory area fill + stroke (teal)
//   4. GPU area fill + stroke (amber/gold)
//   5. CPU area fill + stroke (accent blue / lavender)
//   6. Latest-value glow dots
//   7. Time axis labels (QML Text items, below canvas)
//   8. Source badge + legend pills (QML items, above canvas)
//
// ============================================================================

import QtQuick 2.15

Rectangle {
    id: root
    color: "transparent"
    clip: true

    // ── C++ bridge properties ────────────────────────────────────────────────
    property var timelinePoints: []
    property string timelineSource: "none"
    property string themeMode: "dark_blue"
    property real accentRed: 0.231
    property real accentGreen: 0.510
    property real accentBlue: 0.965
    property int severityLevel: 0
    property bool gpuAvailable: false

    // ── Analytics properties ──────────────────────────────────────────────
    property bool dvrStatsAvailable: false
    property var dvrStats: ({})
    property bool showStats: false
    property bool exportRequested: false
    property string exportStatus: ""

    // ── Legend toggle state ───────────────────────────────────────────────
    property bool showCpu: true
    property bool showMem: true
    property bool showGpu: true

    readonly property bool pinkMode: themeMode === "pink_cute"
    readonly property bool hasData: timelinePoints.length >= 2

    // ── Resolution-agnostic helpers ──────────────────────────────────────────
    // All text sizes derive from root height. Clamp to stay readable at any
    // resolution from 200px to 4000px+.
    readonly property real labelSize: Math.max(8, Math.min(14, Math.round(root.height * 0.045)))
    readonly property real smallSize: Math.max(7, Math.min(12, Math.round(root.height * 0.035)))
    readonly property real badgeSize: Math.max(7, Math.min(11, Math.round(root.height * 0.033)))

    // Margins as fractions of container
    readonly property real marginH: Math.max(4, root.width * 0.015)
    readonly property real marginV: Math.max(4, root.height * 0.03)
    readonly property real yAxisWidth: Math.max(28, root.width * 0.06)

    // ── Color tokens ─────────────────────────────────────────────────────────
    readonly property color clrBgTop: pinkMode ? "#1d0c16" : "#0a1020"
    readonly property color clrBgBot: pinkMode ? "#150a12" : "#060b14"
    readonly property color clrTextMuted: pinkMode ? "#d887b1" : "#4d6d87"
    readonly property color clrTextSecondary: pinkMode ? "#ffb3d9" : "#8badc4"

    // CPU: accent / lavender
    readonly property var cpuStrokeColor: pinkMode
        ? [0.753, 0.518, 0.988, 0.85] : [accentRed, accentGreen, accentBlue, 0.80]
    // Memory: teal / mint
    readonly property var memStrokeColor: pinkMode
        ? [0.431, 0.906, 0.718, 0.75] : [0.20, 0.70, 0.65, 0.65]
    // GPU: gold / coral
    readonly property var gpuStrokeColor: pinkMode
        ? [1.0, 0.690, 0.533, 0.75] : [0.961, 0.620, 0.043, 0.65]

    // ── Trigger repaint on data, theme, or visibility change ─────────────────
    onTimelinePointsChanged: chartCanvas.requestPaint()
    onThemeModeChanged: chartCanvas.requestPaint()
    onGpuAvailableChanged: chartCanvas.requestPaint()
    onShowCpuChanged: chartCanvas.requestPaint()
    onShowMemChanged: chartCanvas.requestPaint()
    onShowGpuChanged: chartCanvas.requestPaint()

    // ── Background gradient ──────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.clrBgTop }
            GradientStop { position: 1.0; color: root.clrBgBot }
        }
    }

    // ── Source badge (top-left) ──────────────────────────────────────────────
    Rectangle {
        id: sourceBadge
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: marginH
        anchors.topMargin: marginV
        width: badgeText.implicitWidth + badgeSize * 1.4
        height: badgeText.implicitHeight + badgeSize * 0.6
        radius: height * 0.3
        color: root.timelineSource === "dvr"
            ? Qt.rgba(0.96, 0.62, 0.04, 0.15)
            : Qt.rgba(accentRed, accentGreen, accentBlue, 0.12)
        border.width: 1
        border.color: root.timelineSource === "dvr"
            ? Qt.rgba(0.96, 0.62, 0.04, 0.35)
            : Qt.rgba(accentRed, accentGreen, accentBlue, 0.25)
        visible: root.hasData

        Text {
            id: badgeText
            anchors.centerIn: parent
            text: root.timelineSource === "dvr" ? "DVR" : "LIVE"
            color: root.timelineSource === "dvr"
                ? Qt.rgba(0.96, 0.62, 0.04, 0.90)
                : root.clrTextSecondary
            font.pixelSize: badgeSize
            font.weight: Font.Bold
            font.letterSpacing: 1.5
            font.family: "Segoe UI"
        }
    }

    // ── Legend pills (top-right) — click to toggle series ───────────────────
    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: marginH
        anchors.topMargin: marginV
        spacing: Math.max(4, root.width * 0.012)
        visible: root.hasData

        Repeater {
            model: {
                var items = [
                    { label: "CPU", sc: root.cpuStrokeColor, key: "cpu" },
                    { label: "MEM", sc: root.memStrokeColor, key: "mem" }
                ]
                if (root.gpuAvailable)
                    items.push({ label: "GPU", sc: root.gpuStrokeColor, key: "gpu" })
                return items
            }
            Rectangle {
                property bool seriesVisible: modelData.key === "cpu" ? root.showCpu
                                           : modelData.key === "mem" ? root.showMem
                                           : root.showGpu
                width: pillText.implicitWidth + badgeSize * 1.2
                height: pillText.implicitHeight + badgeSize * 0.5
                radius: height * 0.3
                color: seriesVisible
                    ? Qt.rgba(modelData.sc[0], modelData.sc[1], modelData.sc[2], 0.15)
                    : "transparent"
                border.width: 1
                border.color: Qt.rgba(modelData.sc[0], modelData.sc[1], modelData.sc[2],
                    seriesVisible ? modelData.sc[3] * 0.7 : 0.20)
                opacity: seriesVisible ? 1.0 : 0.45

                Text {
                    id: pillText
                    anchors.centerIn: parent
                    text: modelData.label
                    color: Qt.rgba(modelData.sc[0], modelData.sc[1], modelData.sc[2], modelData.sc[3])
                    font.pixelSize: badgeSize
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1
                    font.family: "Segoe UI"
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (modelData.key === "cpu") root.showCpu = !root.showCpu
                        else if (modelData.key === "mem") root.showMem = !root.showMem
                        else root.showGpu = !root.showGpu
                    }
                }
            }
        }
    }

    // ── Stats + Export pills (top-right, below legend) ─────────────────────
    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: marginH
        anchors.topMargin: marginV + badgeSize * 2.2
        spacing: Math.max(4, root.width * 0.012)
        visible: root.hasData

        Rectangle {
            width: statsPillText.implicitWidth + badgeSize * 1.2
            height: statsPillText.implicitHeight + badgeSize * 0.5
            radius: height * 0.3
            color: root.showStats ? Qt.rgba(accentRed, accentGreen, accentBlue, 0.20) : "transparent"
            border.width: 1
            border.color: Qt.rgba(accentRed, accentGreen, accentBlue, root.showStats ? 0.50 : 0.25)
            visible: root.dvrStatsAvailable

            Text {
                id: statsPillText
                anchors.centerIn: parent
                text: "STATS"
                color: Qt.rgba(accentRed, accentGreen, accentBlue, root.showStats ? 1.0 : 0.6)
                font.pixelSize: badgeSize
                font.weight: Font.DemiBold
                font.letterSpacing: 1
                font.family: "Segoe UI"
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.showStats = !root.showStats
            }
        }

        Rectangle {
            width: exportPillText.implicitWidth + badgeSize * 1.2
            height: exportPillText.implicitHeight + badgeSize * 0.5
            radius: height * 0.3
            color: "transparent"
            border.width: 1
            border.color: Qt.rgba(0.96, 0.62, 0.04, 0.25)

            Text {
                id: exportPillText
                anchors.centerIn: parent
                text: "EXPORT"
                color: Qt.rgba(0.96, 0.62, 0.04, 0.7)
                font.pixelSize: badgeSize
                font.weight: Font.DemiBold
                font.letterSpacing: 1
                font.family: "Segoe UI"
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.exportRequested = true
            }
        }
    }

    // ── Stats overlay (bottom-left of chart area) ────────────────────────
    Rectangle {
        id: statsOverlay
        anchors.left: chartCanvas.left
        anchors.bottom: chartCanvas.bottom
        anchors.leftMargin: 4
        anchors.bottomMargin: 4
        width: statsColumn.implicitWidth + Math.round(16 * (root.height / 300))
        height: statsColumn.implicitHeight + Math.round(12 * (root.height / 300))
        radius: 4
        color: root.pinkMode ? Qt.rgba(0.12, 0.04, 0.09, 0.85) : Qt.rgba(0.03, 0.05, 0.10, 0.85)
        border.width: 1
        border.color: Qt.rgba(accentRed, accentGreen, accentBlue, 0.15)
        visible: root.showStats && root.dvrStatsAvailable && root.hasData
        z: 10

        Column {
            id: statsColumn
            anchors.centerIn: parent
            spacing: 2

            Text {
                text: {
                    var s = root.dvrStats
                    if (!s || s.cpuAvg === undefined) return "CPU: --"
                    return "CPU: avg " + Number(s.cpuAvg).toFixed(0) + "%" +
                           " | p95 " + Number(s.cpuP95).toFixed(0) + "%" +
                           " | max " + Number(s.cpuMax).toFixed(0) + "%" +
                           " | \u03c3 " + Number(s.cpuStddev).toFixed(1)
                }
                color: root.clrTextSecondary
                font.pixelSize: root.smallSize
                font.family: "Segoe UI"
            }
            Text {
                text: {
                    var s = root.dvrStats
                    if (!s || s.memAvg === undefined) return "MEM: --"
                    return "MEM: avg " + Number(s.memAvg).toFixed(0) + "%" +
                           " | p95 " + Number(s.memP95).toFixed(0) + "%" +
                           " | max " + Number(s.memMax).toFixed(0) + "%" +
                           " | \u03c3 " + Number(s.memStddev).toFixed(1)
                }
                color: root.clrTextSecondary
                font.pixelSize: root.smallSize
                font.family: "Segoe UI"
            }
        }
    }

    // ── Export toast ──────────────────────────────────────────────────────
    Rectangle {
        id: exportToast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: marginV * 2
        width: toastText.implicitWidth + Math.round(20 * (root.height / 300))
        height: toastText.implicitHeight + Math.round(10 * (root.height / 300))
        radius: height / 2
        color: root.exportStatus.indexOf("failed") >= 0
            ? Qt.rgba(0.93, 0.27, 0.27, 0.85) : Qt.rgba(0.13, 0.77, 0.37, 0.85)
        visible: false
        z: 20

        Text {
            id: toastText
            anchors.centerIn: parent
            text: root.exportStatus
            color: "#ffffff"
            font.pixelSize: root.smallSize
            font.weight: Font.DemiBold
            font.family: "Segoe UI"
        }

        SequentialAnimation {
            id: toastAnim
            PropertyAction { target: exportToast; property: "visible"; value: true }
            NumberAnimation { target: exportToast; property: "opacity"; from: 0; to: 1; duration: 200 }
            PauseAnimation { duration: 2500 }
            NumberAnimation { target: exportToast; property: "opacity"; from: 1; to: 0; duration: 400 }
            PropertyAction { target: exportToast; property: "visible"; value: false }
        }
    }

    onExportStatusChanged: {
        if (exportStatus.length > 0) toastAnim.restart()
    }

    // ── Main chart canvas ────────────────────────────────────────────────────
    Canvas {
        id: chartCanvas
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: timeAxisRow.top
        // Margins as fractions — resolution agnostic
        anchors.leftMargin: yAxisWidth + marginH
        anchors.rightMargin: marginH
        anchors.topMargin: marginV * 2.2
        anchors.bottomMargin: marginV * 0.3

        onPaint: {
            var ctx = getContext("2d")
            var cw = width
            var ch = height
            if (cw < 10 || ch < 10) return

            ctx.clearRect(0, 0, cw, ch)

            var pts = root.timelinePoints
            var n = pts.length
            if (n < 2) return

            // ── Derived dimensions (all from canvas size) ──
            var strokeW = Math.max(1.0, ch * 0.008)
            var dotR = Math.max(2.0, ch * 0.018)
            var glowR = dotR * 2.5
            var gridAlpha = root.pinkMode ? 0.07 : 0.05
            var fontSize = Math.max(8, Math.round(ch * 0.065))

            // ── Grid lines at 25%, 50%, 75% ──
            var gridColor = Qt.rgba(root.accentRed, root.accentGreen, root.accentBlue, gridAlpha)
            ctx.strokeStyle = gridColor
            ctx.lineWidth = 1
            ctx.setLineDash([Math.max(2, cw * 0.004), Math.max(3, cw * 0.006)])
            for (var g = 1; g <= 3; g++) {
                var gy = ch * (1.0 - g * 0.25)
                ctx.beginPath()
                ctx.moveTo(0, gy)
                ctx.lineTo(cw, gy)
                ctx.stroke()
            }
            ctx.setLineDash([])

            // ── Y-axis labels (in canvas for pixel-perfect placement) ──
            ctx.font = fontSize + "px 'Cascadia Mono', 'Consolas', monospace"
            ctx.textAlign = "right"
            ctx.textBaseline = "middle"
            var labelColor = root.pinkMode ? "rgba(216, 135, 177, 0.5)" : "rgba(77, 109, 135, 0.5)"
            ctx.fillStyle = labelColor
            ctx.fillText("100%", -Math.max(4, cw * 0.015), ch * 0.02)
            ctx.fillText("75%",  -Math.max(4, cw * 0.015), ch * 0.25)
            ctx.fillText("50%",  -Math.max(4, cw * 0.015), ch * 0.50)
            ctx.fillText("25%",  -Math.max(4, cw * 0.015), ch * 0.75)
            ctx.fillText("0%",   -Math.max(4, cw * 0.015), ch * 0.98)

            // ── Compute point arrays ──
            var cpuPts = new Array(n)
            var memPts = new Array(n)
            var gpuPts = root.gpuAvailable ? new Array(n) : null
            for (var i = 0; i < n; i++) {
                var x = (i / (n - 1)) * cw
                cpuPts[i] = { x: x, y: ch * (1.0 - pts[i].cpuPercent / 100.0) }
                memPts[i] = { x: x, y: ch * (1.0 - pts[i].memPercent / 100.0) }
                if (gpuPts !== null)
                    gpuPts[i] = { x: x, y: ch * (1.0 - pts[i].gpuPercent / 100.0) }
            }

            // ── Helper: draw smooth quadratic curve through points ──
            function traceSmoothPath(points) {
                ctx.moveTo(points[0].x, points[0].y)
                for (var j = 1; j < points.length - 1; j++) {
                    var mx = (points[j].x + points[j+1].x) * 0.5
                    var my = (points[j].y + points[j+1].y) * 0.5
                    ctx.quadraticCurveTo(points[j].x, points[j].y, mx, my)
                }
                ctx.lineTo(points[points.length - 1].x, points[points.length - 1].y)
            }

            // ── Helper: draw area fill + stroke for a series ──
            function drawSeries(points, fillStops, strokeRgba) {
                // Area fill
                ctx.beginPath()
                traceSmoothPath(points)
                ctx.lineTo(cw, ch)
                ctx.lineTo(0, ch)
                ctx.closePath()
                var grad = ctx.createLinearGradient(0, 0, 0, ch)
                for (var s = 0; s < fillStops.length; s++)
                    grad.addColorStop(fillStops[s][0], fillStops[s][1])
                ctx.fillStyle = grad
                ctx.fill()

                // Stroke
                ctx.beginPath()
                traceSmoothPath(points)
                ctx.strokeStyle = Qt.rgba(strokeRgba[0], strokeRgba[1], strokeRgba[2], strokeRgba[3])
                ctx.lineWidth = strokeW
                ctx.lineJoin = "round"
                ctx.lineCap = "round"
                ctx.stroke()
            }

            // ── Helper: draw glow dot at latest point ──
            function drawGlowDot(pt, rgba) {
                // Outer glow
                ctx.beginPath()
                ctx.arc(pt.x, pt.y, glowR, 0, Math.PI * 2, false)
                ctx.fillStyle = Qt.rgba(rgba[0], rgba[1], rgba[2], rgba[3] * 0.20)
                ctx.fill()
                // Inner dot
                ctx.beginPath()
                ctx.arc(pt.x, pt.y, dotR, 0, Math.PI * 2, false)
                ctx.fillStyle = Qt.rgba(rgba[0], rgba[1], rgba[2], rgba[3])
                ctx.fill()
            }

            // ── Draw series back-to-front (respecting toggle state) ──

            // 1. Memory (teal — furthest back)
            var mc = root.memStrokeColor
            if (root.showMem) {
                var memFill = root.pinkMode
                    ? [[0.0, Qt.rgba(0.431, 0.906, 0.718, 0.22)], [1.0, Qt.rgba(0.431, 0.906, 0.718, 0.01)]]
                    : [[0.0, Qt.rgba(0.20, 0.70, 0.65, 0.20)], [1.0, Qt.rgba(0.20, 0.70, 0.65, 0.01)]]
                drawSeries(memPts, memFill, mc)
            }

            // 2. GPU (gold — middle layer, only if available and visible)
            if (gpuPts !== null && root.showGpu) {
                var gc = root.gpuStrokeColor
                var gpuFill = root.pinkMode
                    ? [[0.0, Qt.rgba(1.0, 0.690, 0.533, 0.20)], [1.0, Qt.rgba(1.0, 0.690, 0.533, 0.01)]]
                    : [[0.0, Qt.rgba(0.961, 0.620, 0.043, 0.18)], [1.0, Qt.rgba(0.961, 0.620, 0.043, 0.01)]]
                drawSeries(gpuPts, gpuFill, gc)
            }

            // 3. CPU (accent — front, most prominent)
            var cc = root.cpuStrokeColor
            if (root.showCpu) {
                var cpuFill = root.pinkMode
                    ? [[0.0, Qt.rgba(0.753, 0.518, 0.988, 0.28)],
                       [0.5, Qt.rgba(1.0, 0.302, 0.651, 0.15)],
                       [1.0, Qt.rgba(1.0, 0.690, 0.533, 0.01)]]
                    : [[0.0, Qt.rgba(accentRed, accentGreen, accentBlue, 0.26)],
                       [1.0, Qt.rgba(accentRed, accentGreen, accentBlue, 0.01)]]
                drawSeries(cpuPts, cpuFill, cc)
            }

            // ── Glow dots at latest values ──
            if (root.showMem)
                drawGlowDot(memPts[n - 1], mc)
            if (gpuPts !== null && root.showGpu)
                drawGlowDot(gpuPts[n - 1], gc)
            if (root.showCpu)
                drawGlowDot(cpuPts[n - 1], cc)
        }
    }

    // ── Time axis labels (bottom) ────────────────────────────────────────────
    Row {
        id: timeAxisRow
        anchors.left: chartCanvas.left
        anchors.right: chartCanvas.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: marginV * 0.5
        visible: root.hasData
        height: smallSize * 1.5

        Repeater {
            model: 5
            Item {
                width: timeAxisRow.width / 5
                height: parent.height

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    text: {
                        var pts = root.timelinePoints
                        if (pts.length < 2) return ""
                        var span = pts[pts.length - 1].timestamp - pts[0].timestamp
                        if (span <= 0) return "now"
                        var secsAgo = span * (1.0 - index / 4.0)
                        if (secsAgo < 1) return "now"
                        if (secsAgo < 60) return Math.round(secsAgo) + "s"
                        if (secsAgo < 3600) return Math.round(secsAgo / 60) + "m"
                        return Math.round(secsAgo / 3600) + "h"
                    }
                    color: root.clrTextMuted
                    font.pixelSize: smallSize
                    font.family: "Cascadia Mono, Consolas, monospace"
                    opacity: 0.7
                }
            }
        }
    }

    // ── Empty state ──────────────────────────────────────────────────────────
    Column {
        anchors.centerIn: parent
        spacing: labelSize * 0.5
        visible: !root.hasData
        opacity: 0.5

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Collecting data..."
            color: root.clrTextMuted
            font.pixelSize: labelSize
            font.weight: Font.DemiBold
            font.letterSpacing: 1
            font.family: "Segoe UI"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Timeline will appear after 2 samples"
            color: root.clrTextMuted
            font.pixelSize: smallSize
            font.family: "Segoe UI"
            opacity: 0.6
        }
    }
}
