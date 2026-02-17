// ============================================================================
// Aura Process Scene — Interactive Process Manager Panel
// ============================================================================
//
// Glassmorphism-styled process list with CPU/memory bars, sort controls,
// and kill-process confirmation overlay. Theme-aware (dark_blue / pink_cute).
//
// ============================================================================

import QtQuick 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: "transparent"
    clip: true

    // ── C++ bridge properties ────────────────────────────────────────────────
    property string themeMode: "dark_blue"
    property bool pinkMode: themeMode === "pink_cute"
    property real accentRed: 0.20
    property real accentGreen: 0.45
    property real accentBlue: 0.75
    property int severityLevel: 0

    // ── Responsive scaling ───────────────────────────────────────────────────
    property real scaleFactor: Math.max(0.4, Math.min(2.0, root.height / 600.0))

    // ── Color tokens (matching CockpitScene.qml) ────────────────────────────
    readonly property color clrBgDeep: pinkMode ? "#150a12" : "#060b14"
    readonly property color clrBgSurface: pinkMode ? "#1d0c16" : "#0a1020"
    readonly property color clrAccent: pinkMode ? "#ff4da6" : "#3b82f6"
    readonly property color clrAccentHover: pinkMode ? "#ff92c5" : "#60a5fa"
    readonly property color clrTextPrimary: pinkMode ? "#ffe8f5" : "#e0ecf7"
    readonly property color clrTextSecondary: pinkMode ? "#ffb3d9" : "#8badc4"
    readonly property color clrTextMuted: pinkMode ? "#d887b1" : "#4d6d87"
    readonly property color clrTrack: pinkMode ? "#5f2a4b" : "#1a2940"
    readonly property color clrDanger: pinkMode ? "#ff4466" : "#ef4444"
    readonly property color clrDangerHover: pinkMode ? "#ff6688" : "#f87171"
    readonly property color clrSuccess: pinkMode ? "#6ee7b7" : "#34d399"
    readonly property color clrMemBar: pinkMode ? "#c084fc" : "#06b6d4"

    // ── Helper functions ─────────────────────────────────────────────────────
    function gaugeColor(pct) {
        if (pinkMode) {
            if (pct <= 30) return Qt.rgba(0.75, 0.52, 0.99, 1.0)       // lavender
            if (pct <= 70) return Qt.rgba(1.0, 0.30, 0.65, 1.0)        // hot pink
            return Qt.rgba(0.96, 0.45, 0.45, 1.0)                       // coral
        }
        if (pct <= 50) {
            var t1 = pct / 50.0
            return Qt.rgba(0.23 * (1 - t1) + 0.06 * t1, 0.51 * (1 - t1) + 0.71 * t1, 0.96 * (1 - t1) + 0.84 * t1, 1.0)
        }
        if (pct <= 80) {
            var t2 = (pct - 50) / 30.0
            return Qt.rgba(0.06 * (1 - t2) + 0.96 * t2, 0.71 * (1 - t2) + 0.62 * t2, 0.84 * (1 - t2) + 0.04 * t2, 1.0)
        }
        var t3 = (pct - 80) / 20.0
        return Qt.rgba(0.96 * (1 - t3) + 0.93 * t3, 0.62 * (1 - t3) + 0.27 * t3, 0.04 * (1 - t3) + 0.27 * t3, 1.0)
    }

    function formatMemory(bytes) {
        if (bytes >= 1073741824) return (bytes / 1073741824).toFixed(1) + " GB"
        if (bytes >= 1048576) return (bytes / 1048576).toFixed(0) + " MB"
        if (bytes >= 1024) return (bytes / 1024).toFixed(0) + " KB"
        return bytes + " B"
    }

    // ── Background ───────────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: clrBgDeep
        radius: 6

        // Subtle frost border
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            radius: parent.radius
            border.width: 1
            border.color: Qt.rgba(clrAccent.r, clrAccent.g, clrAccent.b, 0.15)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Math.round(12 * scaleFactor)
        spacing: Math.round(8 * scaleFactor)

        // ── Header ───────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(32 * scaleFactor)
            spacing: Math.round(8 * scaleFactor)

            Text {
                text: pinkMode ? "RUNNING APPS" : "PROCESSES"
                color: clrTextPrimary
                font.pixelSize: Math.round(13 * scaleFactor)
                font.weight: Font.Bold
                font.letterSpacing: 1.5
            }

            // Process count badge
            Rectangle {
                width: countText.implicitWidth + Math.round(12 * scaleFactor)
                height: Math.round(20 * scaleFactor)
                radius: height / 2
                color: Qt.rgba(clrAccent.r, clrAccent.g, clrAccent.b, 0.2)
                border.width: 1
                border.color: Qt.rgba(clrAccent.r, clrAccent.g, clrAccent.b, 0.3)

                Text {
                    id: countText
                    anchors.centerIn: parent
                    text: processModel ? processModel.processCount : "0"
                    color: clrAccent
                    font.pixelSize: Math.round(10 * scaleFactor)
                    font.weight: Font.DemiBold
                }
            }

            Item { Layout.fillWidth: true }

            // Sort pills
            Row {
                spacing: Math.round(4 * scaleFactor)

                Repeater {
                    model: [
                        { label: "CPU", col: 0 },
                        { label: "MEM", col: 1 },
                        { label: "NAME", col: 2 }
                    ]

                    Rectangle {
                        width: pillText.implicitWidth + Math.round(16 * scaleFactor)
                        height: Math.round(22 * scaleFactor)
                        radius: height / 2
                        color: processModel && processModel.sortColumn === modelData.col
                            ? Qt.rgba(clrAccent.r, clrAccent.g, clrAccent.b, 0.3)
                            : Qt.rgba(clrTrack.r, clrTrack.g, clrTrack.b, 0.5)
                        border.width: 1
                        border.color: processModel && processModel.sortColumn === modelData.col
                            ? Qt.rgba(clrAccent.r, clrAccent.g, clrAccent.b, 0.5)
                            : "transparent"

                        Text {
                            id: pillText
                            anchors.centerIn: parent
                            text: modelData.label + (processModel && processModel.sortColumn === modelData.col
                                ? (processModel.sortDescending ? " \u25BC" : " \u25B2") : "")
                            color: processModel && processModel.sortColumn === modelData.col
                                ? clrAccent : clrTextMuted
                            font.pixelSize: Math.round(9 * scaleFactor)
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.5
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (processModel) processModel.setSortMode(modelData.col)
                            }
                        }
                    }
                }
            }
        }

        // Separator
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Qt.rgba(clrAccent.r, clrAccent.g, clrAccent.b, 0.15)
        }

        // ── Column headers ───────────────────────────────────────────────────
        // Plain Item with absolute positioning — avoids RowLayout recursive
        // rearrange when children reference parent.width.
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(20 * scaleFactor)

            Text {
                anchors.verticalCenter: parent.verticalCenter
                x: 0
                width: parent.width * 0.30
                text: pinkMode ? "APP" : "PROCESS"
                color: clrTextMuted
                font.pixelSize: Math.round(9 * scaleFactor)
                font.weight: Font.DemiBold
                font.letterSpacing: 1.0
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                x: parent.width * 0.30
                width: parent.width * 0.10
                text: "PID"
                color: clrTextMuted
                font.pixelSize: Math.round(9 * scaleFactor)
                font.weight: Font.DemiBold
                font.letterSpacing: 1.0
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                x: parent.width * 0.40
                width: parent.width * 0.24
                text: "CPU"
                color: clrTextMuted
                font.pixelSize: Math.round(9 * scaleFactor)
                font.weight: Font.DemiBold
                font.letterSpacing: 1.0
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                x: parent.width * 0.64
                width: parent.width * 0.24
                text: pinkMode ? "MEMORY" : "MEM"
                color: clrTextMuted
                font.pixelSize: Math.round(9 * scaleFactor)
                font.weight: Font.DemiBold
                font.letterSpacing: 1.0
            }
        }

        // ── Process ListView ─────────────────────────────────────────────────
        ListView {
            id: processListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: processModel
            clip: true
            spacing: Math.round(2 * scaleFactor)
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 400

            delegate: Rectangle {
                id: rowDelegate
                width: processListView.width
                height: Math.round(34 * root.scaleFactor)
                radius: 4
                color: rowMouse.containsMouse
                    ? Qt.rgba(clrAccent.r, clrAccent.g, clrAccent.b, 0.08)
                    : (index % 2 === 0
                        ? Qt.rgba(clrBgSurface.r, clrBgSurface.g, clrBgSurface.b, 0.5)
                        : "transparent")

                property bool hovered: rowMouse.containsMouse

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }

                // Plain Item with absolute positioning — no layout engine.
                Item {
                    anchors.fill: parent
                    anchors.leftMargin: Math.round(8 * root.scaleFactor)
                    anchors.rightMargin: Math.round(8 * root.scaleFactor)

                    // Process name + instance count
                    Item {
                        anchors.verticalCenter: parent.verticalCenter
                        x: 0
                        width: parent.width * 0.30
                        height: nameText.height

                        Text {
                            id: nameText
                            anchors.left: parent.left
                            width: (model.instanceCount > 1)
                                ? parent.width - countBadge.width - Math.round(4 * root.scaleFactor)
                                : parent.width
                            text: model.name || ""
                            color: root.clrTextPrimary
                            font.pixelSize: Math.round(11 * root.scaleFactor)
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            id: countBadge
                            anchors.left: nameText.right
                            anchors.leftMargin: Math.round(3 * root.scaleFactor)
                            anchors.verticalCenter: parent.verticalCenter
                            visible: model.instanceCount > 1
                            width: countLabel.implicitWidth + Math.round(8 * root.scaleFactor)
                            height: Math.round(14 * root.scaleFactor)
                            radius: height / 2
                            color: Qt.rgba(root.clrAccent.r, root.clrAccent.g, root.clrAccent.b, 0.2)

                            Text {
                                id: countLabel
                                anchors.centerIn: parent
                                text: "\u00D7" + model.instanceCount
                                color: root.clrTextMuted
                                font.pixelSize: Math.round(8 * root.scaleFactor)
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    // PID
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        x: parent.width * 0.30
                        width: parent.width * 0.10
                        text: model.pid || ""
                        color: root.clrTextMuted
                        font.pixelSize: Math.round(10 * root.scaleFactor)
                        font.family: "Consolas"
                    }

                    // CPU bar
                    Item {
                        x: parent.width * 0.40
                        width: parent.width * 0.24
                        anchors.top: parent.top
                        anchors.topMargin: Math.round(4 * root.scaleFactor)
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: Math.round(4 * root.scaleFactor)

                        Text {
                            anchors.top: parent.top
                            text: (model.cpuPercent !== undefined ? model.cpuPercent.toFixed(1) : "0.0") + "%"
                            color: root.gaugeColor(model.cpuPercent || 0)
                            font.pixelSize: Math.round(9 * root.scaleFactor)
                            font.weight: Font.DemiBold
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: Math.round(4 * root.scaleFactor)
                            radius: 2
                            color: root.clrTrack

                            Rectangle {
                                width: parent.width * Math.min(1.0, (model.cpuPercent || 0) / 100.0)
                                height: parent.height
                                radius: 2
                                color: root.gaugeColor(model.cpuPercent || 0)

                                Behavior on width {
                                    NumberAnimation { duration: 300; easing.type: Easing.OutQuad }
                                }
                            }
                        }
                    }

                    // Memory bar
                    Item {
                        x: parent.width * 0.64
                        width: parent.width * 0.24
                        anchors.top: parent.top
                        anchors.topMargin: Math.round(4 * root.scaleFactor)
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: Math.round(4 * root.scaleFactor)

                        Text {
                            anchors.top: parent.top
                            text: root.formatMemory(model.memoryBytes || 0)
                            color: root.clrMemBar
                            font.pixelSize: Math.round(9 * root.scaleFactor)
                            font.weight: Font.DemiBold
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: Math.round(4 * root.scaleFactor)
                            radius: 2
                            color: root.clrTrack

                            Rectangle {
                                width: parent.width * Math.min(1.0, (model.memoryPercent || 0) / 100.0)
                                height: parent.height
                                radius: 2
                                color: root.clrMemBar

                                Behavior on width {
                                    NumberAnimation { duration: 300; easing.type: Easing.OutQuad }
                                }
                            }
                        }
                    }

                    // Kill button — anchored to right edge, never clipped
                    Rectangle {
                        anchors.right: parent.right
                        anchors.rightMargin: Math.round(4 * root.scaleFactor)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(28 * root.scaleFactor)
                        height: Math.round(20 * root.scaleFactor)
                        radius: height / 2
                        opacity: rowDelegate.hovered ? (killBtnMouse.containsMouse ? 1.0 : 0.8) : 0.0
                        color: killBtnMouse.containsMouse ? root.clrDangerHover : root.clrDanger

                        Behavior on opacity {
                            NumberAnimation { duration: 120; easing.type: Easing.OutQuad }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "\u00D7"
                            color: "#ffffff"
                            font.pixelSize: Math.round(13 * root.scaleFactor)
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            id: killBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            enabled: rowDelegate.hovered
                            onClicked: {
                                if (processModel) processModel.requestKill(model.pid)
                            }
                        }
                    }
                }

            }

            // Row entrance animation (non-deprecated pattern)
            add: Transition {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutQuad }
            }
        }

        // ── Error display ────────────────────────────────────────────────────
        Text {
            Layout.fillWidth: true
            visible: processModel && processModel.lastError !== ""
            text: processModel ? processModel.lastError : ""
            color: clrDanger
            font.pixelSize: Math.round(10 * scaleFactor)
            wrapMode: Text.WordWrap
        }
    }

    // ── Kill Confirmation Overlay ────────────────────────────────────────────
    Rectangle {
        id: killOverlay
        anchors.fill: parent
        visible: processModel && processModel.pendingKillPid !== 0
        color: Qt.rgba(0, 0, 0, 0.65)
        z: 100

        // Click outside to cancel
        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (processModel) processModel.cancelKill()
            }
        }

        // Glass dialog
        Rectangle {
            id: killDialog
            anchors.centerIn: parent
            width: Math.round(320 * scaleFactor)
            height: Math.round(160 * scaleFactor)
            radius: 12
            color: Qt.rgba(clrBgSurface.r, clrBgSurface.g, clrBgSurface.b, 0.95)
            border.width: 1
            border.color: Qt.rgba(clrDanger.r, clrDanger.g, clrDanger.b, 0.4)

            // Prevent click-through
            MouseArea { anchors.fill: parent }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Math.round(20 * scaleFactor)
                spacing: Math.round(12 * scaleFactor)

                Text {
                    Layout.fillWidth: true
                    text: pinkMode ? "End this app?" : "Terminate Process?"
                    color: clrDanger
                    font.pixelSize: Math.round(15 * scaleFactor)
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    Layout.fillWidth: true
                    text: (processModel ? processModel.pendingKillName : "") +
                          " (PID " + (processModel ? processModel.pendingKillPid : "") + ")"
                    color: clrTextSecondary
                    font.pixelSize: Math.round(12 * scaleFactor)
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Math.round(12 * scaleFactor)

                    // Cancel button
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.round(34 * scaleFactor)
                        radius: 6
                        color: cancelMouse.containsMouse
                            ? Qt.rgba(clrTrack.r, clrTrack.g, clrTrack.b, 0.8)
                            : Qt.rgba(clrTrack.r, clrTrack.g, clrTrack.b, 0.5)
                        border.width: 1
                        border.color: Qt.rgba(clrTextMuted.r, clrTextMuted.g, clrTextMuted.b, 0.3)

                        Text {
                            anchors.centerIn: parent
                            text: pinkMode ? "Nevermind" : "Cancel"
                            color: clrTextSecondary
                            font.pixelSize: Math.round(12 * scaleFactor)
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            id: cancelMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (processModel) processModel.cancelKill()
                            }
                        }
                    }

                    // Confirm kill button
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.round(34 * scaleFactor)
                        radius: 6
                        color: confirmMouse.containsMouse ? clrDangerHover : clrDanger

                        Text {
                            anchors.centerIn: parent
                            text: pinkMode ? "End App" : "Kill Process"
                            color: "#ffffff"
                            font.pixelSize: Math.round(12 * scaleFactor)
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            id: confirmMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (processModel) processModel.confirmKill()
                            }
                        }
                    }
                }
            }
        }

        // Entrance animation
        opacity: 0
        states: State {
            when: killOverlay.visible
            PropertyChanges { target: killOverlay; opacity: 1 }
        }
        transitions: Transition {
            NumberAnimation { property: "opacity"; duration: 150; easing.type: Easing.OutQuad }
        }
    }

    // ── Kill result toast ────────────────────────────────────────────────────
    Rectangle {
        id: killToast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.round(20 * scaleFactor)
        width: toastText.implicitWidth + Math.round(32 * scaleFactor)
        height: Math.round(32 * scaleFactor)
        radius: height / 2
        color: killToastSuccess
            ? Qt.rgba(clrSuccess.r, clrSuccess.g, clrSuccess.b, 0.2)
            : Qt.rgba(clrDanger.r, clrDanger.g, clrDanger.b, 0.2)
        border.width: 1
        border.color: killToastSuccess
            ? Qt.rgba(clrSuccess.r, clrSuccess.g, clrSuccess.b, 0.4)
            : Qt.rgba(clrDanger.r, clrDanger.g, clrDanger.b, 0.4)
        visible: false
        opacity: 0
        z: 101

        property bool killToastSuccess: true

        Text {
            id: toastText
            anchors.centerIn: parent
            text: ""
            color: parent.killToastSuccess ? clrSuccess : clrDanger
            font.pixelSize: Math.round(11 * scaleFactor)
            font.weight: Font.DemiBold
        }

        SequentialAnimation {
            id: toastAnim
            PropertyAction { target: killToast; property: "visible"; value: true }
            NumberAnimation { target: killToast; property: "opacity"; to: 1.0; duration: 150 }
            PauseAnimation { duration: 2500 }
            NumberAnimation { target: killToast; property: "opacity"; to: 0; duration: 300 }
            PropertyAction { target: killToast; property: "visible"; value: false }
        }
    }

    Connections {
        target: processModel
        function onKillCompleted(success, message) {
            killToast.killToastSuccess = success
            toastText.text = message
            toastAnim.restart()
        }
    }
}
