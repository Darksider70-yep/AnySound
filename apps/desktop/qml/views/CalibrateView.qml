import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import ".."
import "../components"

Item {
    id: root

    property int offsetMs: 0
    property bool isPlayingClicks: false

    signal saveOffset(int offset)
    signal close()

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.space48, 540)
        spacing: Theme.space24

        // Header
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "Fine-tune Delay"
                font.family: Theme.serifFont
                font.pixelSize: Theme.fontSizeTitle
                color: Theme.fog
            }

            Item { Layout.fillWidth: true }

            SecondaryButton {
                text: "Done"
                onClicked: root.close()
            }
        }

        // Instructions Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            radius: Theme.radiusPanel
            color: Theme.deck
            border.color: Theme.line
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.space16

                Text {
                    text: "Both devices will play a short click together. Adjust delay until you hear one unified click."
                    font.family: Theme.sansFont
                    font.pixelSize: Theme.fontSizeSecondary
                    color: Theme.mist
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        // Main Adjustment Card
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radiusPanel
            color: Theme.deck
            border.color: Theme.line
            border.width: 1
            implicitHeight: calibLayout.implicitHeight + Theme.space32

            ColumnLayout {
                id: calibLayout
                anchors.fill: parent
                anchors.margins: Theme.space24
                spacing: Theme.space24

                // Big Readout
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: (root.offsetMs > 0 ? "+" : "") + root.offsetMs + " ms"
                    font.family: Theme.serifFont
                    font.pixelSize: Theme.fontSizeSync
                    color: Theme.sonar
                }

                // Slider (-500 to +500 ms)
                Slider {
                    Layout.fillWidth: true
                    from: -500
                    to: 500
                    value: root.offsetMs
                    stepSize: 1
                    onMoved: root.offsetMs = Math.round(value)

                    background: Rectangle {
                        x: parent.leftPadding
                        y: parent.topPadding + parent.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 4
                        width: parent.availableWidth
                        height: 4
                        radius: 2
                        color: Theme.line

                        Rectangle {
                            x: Math.min(parent.width / 2, (parent.width / 2) + ((root.offsetMs / 500.0) * (parent.width / 2)))
                            width: Math.abs((root.offsetMs / 500.0) * (parent.width / 2))
                            height: parent.height
                            color: Theme.sonar
                            radius: 2
                        }
                    }

                    handle: Rectangle {
                        x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                        y: parent.topPadding + parent.availableHeight / 2 - height / 2
                        implicitWidth: 20
                        implicitHeight: 20
                        radius: 10
                        color: parent.pressed ? Theme.sonar : Theme.fog
                    }
                }

                // Stepper Buttons (-10, -1, +1, +10)
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Theme.space12

                    SecondaryButton {
                        text: "−10 ms"
                        onClicked: root.offsetMs = Math.max(-500, root.offsetMs - 10)
                    }

                    SecondaryButton {
                        text: "−1 ms"
                        onClicked: root.offsetMs = Math.max(-500, root.offsetMs - 1)
                    }

                    SecondaryButton {
                        text: "+1 ms"
                        onClicked: root.offsetMs = Math.min(500, root.offsetMs + 1)
                    }

                    SecondaryButton {
                        text: "+10 ms"
                        onClicked: root.offsetMs = Math.min(500, root.offsetMs + 10)
                    }
                }
            }
        }

        // Action Buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space16

            SecondaryButton {
                Layout.fillWidth: true
                text: root.isPlayingClicks ? "Stop test clicks" : "Play test clicks"
                onClicked: root.isPlayingClicks = !root.isPlayingClicks
            }

            PrimaryButton {
                Layout.fillWidth: true
                text: "Save offset"
                onClicked: {
                    root.saveOffset(root.offsetMs);
                    root.close();
                }
            }
        }
    }
}
