import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Item {
    id: root

    property real value: 1.0 // 0.0 to 1.0
    signal valueModified(real newValue)

    implicitWidth: 160
    implicitHeight: 44

    Slider {
        id: slider
        anchors.fill: parent
        from: 0.0
        to: 1.0
        value: root.value
        stepSize: 0.01

        onMoved: {
            root.valueModified(slider.value);
        }

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            implicitWidth: 160
            implicitHeight: 4
            width: slider.availableWidth
            height: 4
            radius: 2
            color: Theme.line

            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                color: Theme.sonar
                radius: 2
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            implicitWidth: 20
            implicitHeight: 20
            radius: 10
            color: slider.pressed ? Theme.sonar : Theme.fog
            border.color: Theme.harbor
            border.width: 2
        }
    }
}
