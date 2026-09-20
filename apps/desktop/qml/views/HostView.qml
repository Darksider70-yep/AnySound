import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import ".."
import "../components"

Item {
    id: root

    property string pinCode: "1234"
    property var clientDevices: []
    property real masterVolume: 1.0
    property int targetLatencyMs: 300
    property bool showSpeakerNotice: true

    signal stopSharing()
    signal setClientVolume(int deviceId, real volume)
    signal setClientMute(int deviceId, bool muted)
    signal setClientOffset(int deviceId, int offsetMs)
    signal setMasterVolume(real volume)
    signal setTargetLatency(int latencyMs)

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header Bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: Theme.deck

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space24
                anchors.rightMargin: Theme.space24

                Text {
                    text: "Sharing this computer's sound"
                    font.family: Theme.serifFont
                    font.pixelSize: Theme.fontSizeTitle
                    color: Theme.fog
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    spacing: Theme.space12

                    Text {
                        text: "Code"
                        font.family: Theme.sansFont
                        font.pixelSize: Theme.fontSizeSecondary
                        color: Theme.mist
                    }

                    Rectangle {
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 36
                        radius: Theme.radiusControl
                        color: Theme.harbor
                        border.color: Theme.sonar
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: root.pinCode
                            font.family: Theme.sansFont
                            font.pixelSize: 20
                            font.weight: Font.Bold
                            font.letterSpacing: 2
                            color: Theme.sonar
                        }
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.line
            }
        }

        // Speaker notice banner
        Rectangle {
            visible: root.showSpeakerNotice
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: Qt.rgba(Theme.drift.r, Theme.drift.g, Theme.drift.b, 0.15)
            border.color: Theme.drift
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space24
                anchors.rightMargin: Theme.space24

                Text {
                    text: "ℹ This computer's own speakers should be off while sharing, or they'll play ahead of the others."
                    font.family: Theme.sansFont
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.fog
                    Layout.fillWidth: true
                }

                Text {
                    text: "Got it"
                    font.family: Theme.sansFont
                    font.pixelSize: Theme.fontSizeCaption
                    font.weight: Font.Bold
                    color: Theme.drift

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.showSpeakerNotice = false
                    }
                }
            }
        }

        // Main 2-column Area
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left Column: Room View
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.harbor

                RoomView {
                    anchors.fill: parent
                    devices: root.clientDevices
                }

                Text {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: Theme.space16
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Room view"
                    font.family: Theme.sansFont
                    font.pixelSize: Theme.fontSizeSecondary
                    color: Theme.mist
                }
            }

            // Divider Hairline
            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: Theme.line
            }

            // Right Column: Devices List
            Rectangle {
                Layout.preferredWidth: 380
                Layout.fillHeight: true
                color: Theme.deck

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // List Header
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        color: Theme.deck

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.space16
                            anchors.rightMargin: Theme.space16

                            Text {
                                text: "Listening devices (" + root.clientDevices.length + ")"
                                font.family: Theme.sansFont
                                font.pixelSize: Theme.fontSizeSecondary
                                font.weight: Font.DemiBold
                                color: Theme.fog
                            }
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: Theme.line
                        }
                    }

                    // Empty State or Device Rows
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Text {
                            visible: root.clientDevices.length === 0
                            anchors.centerIn: parent
                            width: parent.width - Theme.space32
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            text: "Nobody's listening yet.\nOpen Chorus on another device and enter code " + root.pinCode + "."
                            font.family: Theme.sansFont
                            font.pixelSize: Theme.fontSizeSecondary
                            color: Theme.mist
                        }

                        ListView {
                            visible: root.clientDevices.length > 0
                            anchors.fill: parent
                            model: root.clientDevices
                            clip: true

                            delegate: DeviceRow {
                                width: parent.width
                                deviceId: modelData.id || 0
                                deviceName: modelData.name || "Client"
                                platform: modelData.platform || "Device"
                                syncErrorMs: (modelData.last_stats && modelData.last_stats.sync_error_us) ? (modelData.last_stats.sync_error_us / 1000.0) : 0.0
                                syncState: modelData.authenticated ? "tight" : "connecting"
                                volume: modelData.volume !== undefined ? modelData.volume : 1.0
                                isMuted: modelData.is_muted || false
                                offsetMs: modelData.offset_ms || 0

                                onVolumeChanged: (vol) => root.setClientVolume(deviceId, vol)
                                onMuteToggled: (muted) => root.setClientMute(deviceId, muted)
                                onOffsetChanged: (off) => root.setClientOffset(deviceId, off)
                            }
                        }
                    }
                }
            }
        }

        // Bottom Bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: Theme.deck

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: Theme.line
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space24
                anchors.rightMargin: Theme.space24
                spacing: Theme.space24

                // Master Volume
                RowLayout {
                    spacing: Theme.space8
                    Text {
                        text: "Master Volume"
                        font.family: Theme.sansFont
                        font.pixelSize: Theme.fontSizeSecondary
                        color: Theme.mist
                    }

                    VolumeSlider {
                        value: root.masterVolume
                        onValueModified: (v) => root.setMasterVolume(v)
                    }
                }

                // Target Latency
                RowLayout {
                    spacing: Theme.space8
                    Text {
                        text: "Delay between devices"
                        font.family: Theme.sansFont
                        font.pixelSize: Theme.fontSizeSecondary
                        color: Theme.mist
                    }

                    DelayStepper {
                        offsetMs: root.targetLatencyMs
                        onOffsetChanged: (lat) => root.setTargetLatency(Math.max(100, Math.min(1000, lat)))
                    }
                }

                Item { Layout.fillWidth: true }

                SecondaryButton {
                    text: "Stop sharing"
                    onClicked: root.stopSharing()
                }
            }
        }
    }
}
