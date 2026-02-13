import QtQuick 2.15

Rectangle {
    id: root
    color: "#0d2034"
    radius: 8

    property real accentIntensity: 0.0

    SequentialAnimation on accentIntensity {
        loops: Animation.Infinite
        NumberAnimation { to: 1.0; duration: 1200; easing.type: Easing.InOutQuad }
        NumberAnimation { to: 0.0; duration: 1200; easing.type: Easing.InOutQuad }
    }

    Rectangle {
        id: gaugeRing
        width: Math.min(parent.width, parent.height) * 0.55
        height: width
        radius: width / 2
        anchors.centerIn: parent
        color: "transparent"
        border.width: 8
        border.color: Qt.rgba(0.30 + (root.accentIntensity * 0.45), 0.58, 0.80, 0.95)
    }

    Text {
        text: "Native Cockpit"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 18
        color: "#d7ebff"
        font.pixelSize: 20
    }

    Text {
        text: "Widgets + QML bridge active"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        color: "#9fc8e8"
        font.pixelSize: 13
    }
}

