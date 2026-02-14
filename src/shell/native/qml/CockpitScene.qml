import QtQuick 2.15

Rectangle {
    id: root
    color: "#0d2034"
    radius: 8
    clip: true

    property real accentIntensity: 0.0
    property real cpuPercent: 0.0
    property real memoryPercent: 0.0
    property string statusText: "Waiting for telemetry..."

    Behavior on accentIntensity {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    Text {
        id: titleLabel
        text: "AURA COCKPIT"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 14
        color: "#d7ebff"
        font.pixelSize: 18
        font.weight: Font.DemiBold
        font.letterSpacing: 2
    }

    Row {
        id: gaugeRow
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: titleLabel.bottom
        anchors.topMargin: 20
        spacing: 40

        Item {
            id: cpuGauge
            width: Math.min(root.width * 0.28, 160)
            height: width

            Rectangle {
                id: cpuRing
                anchors.centerIn: parent
                width: parent.width
                height: width
                radius: width / 2
                color: "transparent"
                border.width: 6 + (root.accentIntensity * 4)
                border.color: Qt.rgba(
                    0.25 + (root.cpuPercent / 100.0) * 0.55,
                    0.55 - (root.cpuPercent / 100.0) * 0.30,
                    0.80 - (root.cpuPercent / 100.0) * 0.40,
                    0.90
                )

                Behavior on border.width {
                    NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
                }
                Behavior on border.color {
                    ColorAnimation { duration: 300 }
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    text: "CPU"
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "#9fc8e8"
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    font.letterSpacing: 1
                }

                Text {
                    text: root.cpuPercent.toFixed(1) + "%"
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "#e0f0ff"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                }
            }
        }

        Item {
            id: memGauge
            width: Math.min(root.width * 0.28, 160)
            height: width

            Rectangle {
                id: memRing
                anchors.centerIn: parent
                width: parent.width
                height: width
                radius: width / 2
                color: "transparent"
                border.width: 6 + (root.accentIntensity * 4)
                border.color: Qt.rgba(
                    0.30 + (root.memoryPercent / 100.0) * 0.45,
                    0.45,
                    0.70 + (root.memoryPercent / 100.0) * 0.15,
                    0.90
                )

                Behavior on border.width {
                    NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
                }
                Behavior on border.color {
                    ColorAnimation { duration: 300 }
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    text: "MEM"
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "#9fc8e8"
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    font.letterSpacing: 1
                }

                Text {
                    text: root.memoryPercent.toFixed(1) + "%"
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: "#e0f0ff"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                }
            }
        }
    }

    Rectangle {
        id: divider
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: gaugeRow.bottom
        anchors.topMargin: 16
        width: parent.width * 0.75
        height: 1
        color: Qt.rgba(0.30, 0.55, 0.78, 0.30 + root.accentIntensity * 0.20)
    }

    Text {
        id: statusLabel
        text: root.statusText
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: divider.bottom
        anchors.topMargin: 12
        color: "#9fc8e8"
        font.pixelSize: 12
        elide: Text.ElideRight
        width: parent.width * 0.85
        horizontalAlignment: Text.AlignHCenter
    }

    Rectangle {
        id: accentGlow
        anchors.fill: parent
        color: "transparent"
        border.width: 2
        border.color: Qt.rgba(
            0.20 + root.accentIntensity * 0.50,
            0.45 + root.accentIntensity * 0.25,
            0.75,
            0.15 + root.accentIntensity * 0.35
        )
        radius: root.radius

        Behavior on border.color {
            ColorAnimation { duration: 250 }
        }
    }
}
