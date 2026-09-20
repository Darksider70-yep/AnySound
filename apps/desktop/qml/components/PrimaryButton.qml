import QtQuick 2.15
import ".."

Rectangle {
    id: root

    property string text: "Button"
    property bool enabled: true
    signal clicked()

    implicitWidth: Math.max(120, label.implicitWidth + Theme.space32)
    implicitHeight: 44
    radius: Theme.radiusPanel
    color: root.enabled ? (mouse.pressed ? Qt.darker(Theme.sonar, 1.1) : Theme.sonar) : Theme.line

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        font.family: Theme.sansFont
        font.pixelSize: Theme.fontSizeBody
        font.weight: Font.DemiBold
        color: root.enabled ? Theme.textOnSonar : Theme.mist
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        enabled: root.enabled
        onClicked: root.clicked()
    }
}
