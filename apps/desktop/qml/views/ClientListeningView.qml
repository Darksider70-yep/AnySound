import QtQuick 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"

Item {
    id: root

    property string hostName: "Daksh's laptop"
    property real syncErrorMs: 0.0
    property string syncState: "tight"
    property real volume: 1.0
    property bool isMuted: false
    property int offsetMs: 0
    property var clientStats: ({})

    signal setVolume(real vol)
    signal setMute(bool muted)
    signal setOffset(int offsetMs)
    signal calibrate()
    signal leave()

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.space48, 500)
        spacing: Theme.space32

        // Header
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Listening to " + root.hostName
            font.family: Theme.serifFont
            font.pixelSize: Theme.fontSizeTitle
            color: Theme.fog
        }

        // Hero Sync Readout Card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            radius: Theme.radiusPanel
            color: Theme.deck
            border.color: Theme.line
            border.width: 1

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.space8

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "±" + Math.abs(Math.round(root.syncErrorMs)) + " ms"
                    font.family: Theme.serifFont
                    font.pixelSize: Theme.fontSizeSync
                    color: {
                        const absErr = Math.abs(root.syncErrorMs);
                        if (absErr <= 5) return Theme.sonar;
                        if (absErr <= 20) return Theme.drift;
                        return Theme.lost;
                    }
                }

                SyncBadge {
                    Layout.alignment: Qt.AlignHCenter
                    syncErrorMs: root.syncErrorMs
                    connectionState: root.syncState
                }
            }
        }

        // Controls Panel
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radiusPanel
            color: Theme.deck
            border.color: Theme.line
            border.width: 1
            implicitHeight: controlsLayout.implicitHeight + Theme.space32

            ColumnLayout {
                id: controlsLayout
                anchors.fill: parent
                anchors.margins: Theme.space16
                spacing: Theme.space16

                // Volume Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space16

                    Text {
                        text: "Volume"
                        font.family: Theme.sansFont
                        font.pixelSize: Theme.fontSizeBody
                        color: Theme.fog
                        Layout.preferredWidth: 64
                    }

                    VolumeSlider {
                        Layout.fillWidth: true
                        value: root.volume
                        onValueModified: (v) => root.setVolume(v)
                    }

                    Text {
                        text: Math.round(root.volume * 100) + "%"
                        font.family: Theme.sansFont
                        font.pixelSize: Theme.fontSizeSecondary
                        color: Theme.mist
                        Layout.preferredWidth: 40
                    }
                }

                // Divider
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.line
                }

                // Delay Row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space16

                    Text {
                        text: "Delay"
                        font.family: Theme.sansFont
                        font.pixelSize: Theme.fontSizeBody
                        color: Theme.fog
                        Layout.preferredWidth: 64
                    }

                    DelayStepper {
                        offsetMs: root.offsetMs
                        onOffsetChanged: (off) => root.setOffset(off)
                    }

                    Item { Layout.fillWidth: true }

                    SecondaryButton {
                        text: "Fine-tune"
                        onClicked: root.calibrate()
                    }
                }
            }
        }

        // Leave Action
        SecondaryButton {
            Layout.alignment: Qt.AlignHCenter
            text: "Leave"
            onClicked: root.leave()
        }
    }
}
