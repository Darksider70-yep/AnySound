import QtQuick 2.15
import ".."

Rectangle {
    id: root

    property string text: "Button"
    property bool enabled: true
    signal clicked()

    implicitWidth: Math.max(100, label.implicitWidth + Theme.space32)
    implicitHeight: 44
    radius: Theme.radiusPanel
    color: mouse.pressed ? Theme.line : Theme.deck
    border.color: Theme.line
    border.width: 1

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        font.family: Theme.sansFont
        font.pixelSize: Theme.fontSizeBody
        font.weight: Font.Medium
        color: root.enabled ? Theme.fog : Theme.mist
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        enabled: root.enabled
        onClicked: root.clicked()
    }
}
