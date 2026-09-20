import QtQuick 2.15
import QtQuick.Layouts 1.15
import ".."

Item {
    id: root

    property int offsetMs: 0
    signal offsetChanged(int newOffset)

    implicitWidth: layout.implicitWidth
    implicitHeight: 44

    RowLayout {
        id: layout
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space8

        Rectangle {
            id: decBtn
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            radius: Theme.radiusControl
            color: decMouse.pressed ? Theme.line : Theme.deck
            border.color: Theme.line
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "−"
                font.family: Theme.sansFont
                font.pixelSize: 18
                color: Theme.fog
            }

            MouseArea {
                id: decMouse
                anchors.fill: parent
                onClicked: {
                    const step = (mouse.modifiers & Qt.ShiftModifier) ? 10 : 1;
                    const val = Math.max(-500, root.offsetMs - step);
                    root.offsetChanged(val);
                }
            }
        }

        Text {
            Layout.preferredWidth: 64
            horizontalAlignment: Text.AlignHCenter
            text: (root.offsetMs > 0 ? "+" : "") + root.offsetMs + " ms"
            font.family: Theme.sansFont
            font.pixelSize: Theme.fontSizeSecondary
            font.weight: Font.Medium
            color: Theme.fog
        }

        Rectangle {
            id: incBtn
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            radius: Theme.radiusControl
            color: incMouse.pressed ? Theme.line : Theme.deck
            border.color: Theme.line
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "+"
                font.family: Theme.sansFont
                font.pixelSize: 18
                color: Theme.fog
            }

            MouseArea {
                id: incMouse
                anchors.fill: parent
                onClicked: {
                    const step = (mouse.modifiers & Qt.ShiftModifier) ? 10 : 1;
                    const val = Math.min(500, root.offsetMs + step);
                    root.offsetChanged(val);
                }
            }
        }
    }
}
