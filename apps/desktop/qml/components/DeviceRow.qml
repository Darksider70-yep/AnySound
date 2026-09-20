import QtQuick 2.15
import QtQuick.Layouts 1.15
import ".."

Item {
    id: root

    property int deviceId: 0
    property string deviceName: "Client Device"
    property string platform: "Windows"
    property real syncErrorMs: 0.0
    property string syncState: "tight"
    property real volume: 1.0
    property bool isMuted: false
    property int offsetMs: 0

    signal volumeChanged(real vol)
    signal muteToggled(bool muted)
    signal offsetChanged(int offset)

    implicitWidth: parent ? parent.width : 500
    implicitHeight: 64

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Theme.space16
            Layout.rightMargin: Theme.space16
            spacing: Theme.space16

            // Device Name & Sync Badge
            ColumnLayout {
                Layout.preferredWidth: 160
                spacing: Theme.space4

                Text {
                    text: root.deviceName
                    font.family: Theme.sansFont
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                    color: Theme.fog
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                SyncBadge {
                    syncErrorMs: root.syncErrorMs
                    connectionState: root.syncState
                }
            }

            // Volume Control
            RowLayout {
                spacing: Theme.space8
                Layout.preferredWidth: 160

                Text {
                    text: root.isMuted ? "Muted" : Math.round(root.volume * 100) + "%"
                    font.family: Theme.sansFont
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.mist
                    Layout.preferredWidth: 36
                }

                VolumeSlider {
                    value: root.volume
                    onValueModified: (v) => root.volumeChanged(v)
                }

                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    radius: Theme.radiusControl
                    color: muteMouse.pressed ? Theme.line : (root.isMuted ? Theme.lost : Theme.deck)
                    border.color: Theme.line
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: root.isMuted ? "✕" : "🔊"
                        font.pixelSize: 12
                        color: root.isMuted ? Theme.fog : Theme.fog
                    }

                    MouseArea {
                        id: muteMouse
                        anchors.fill: parent
                        onClicked: root.muteToggled(!root.isMuted)
                    }
                }
            }

            // Delay Stepper
            DelayStepper {
                offsetMs: root.offsetMs
                onOffsetChanged: (off) => root.offsetChanged(off)
            }
        }

        // Hairline Separator
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.line
        }
    }
}
