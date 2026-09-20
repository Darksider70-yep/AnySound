import QtQuick 2.15
import QtQuick.Layouts 1.15
import ".."
import "../components"

Item {
    id: root

    signal requestHost()
    signal requestJoin()
    signal requestSettings()

    ColumnLayout {
        anchors.centerIn: parent
        spacing: Theme.space48
        width: Math.min(parent.width - Theme.space48, 700)

        // Header Title
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.space8

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Chorus"
                font.family: Theme.serifFont
                font.pixelSize: Theme.fontSizeTitle * 1.5
                color: Theme.fog
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Make every device a speaker."
                font.family: Theme.sansFont
                font.pixelSize: Theme.fontSizeBody
                color: Theme.mist
            }
        }

        // Two Large Action Cards
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space24

            // Share Card
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                radius: Theme.radiusPanel
                color: shareMouse.containsMouse ? Theme.deck : Qt.rgba(Theme.deck.r, Theme.deck.g, Theme.deck.b, 0.7)
                border.color: shareMouse.containsMouse ? Theme.sonar : Theme.line
                border.width: 1

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Theme.space12

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "📻"
                        font.pixelSize: 32
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Share this device's sound"
                        font.family: Theme.sansFont
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.DemiBold
                        color: Theme.fog
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Broadcast system audio to nearby listeners"
                        font.family: Theme.sansFont
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.mist
                    }
                }

                MouseArea {
                    id: shareMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.requestHost()
                }
            }

            // Join Card
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                radius: Theme.radiusPanel
                color: joinMouse.containsMouse ? Theme.deck : Qt.rgba(Theme.deck.r, Theme.deck.g, Theme.deck.b, 0.7)
                border.color: joinMouse.containsMouse ? Theme.sonar : Theme.line
                border.width: 1

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Theme.space12

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "🎧"
                        font.pixelSize: 32
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Play sound from another device"
                        font.family: Theme.sansFont
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.DemiBold
                        color: Theme.fog
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Sync playback in microsecond harmony"
                        font.family: Theme.sansFont
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.mist
                    }
                }

                MouseArea {
                    id: joinMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.requestJoin()
                }
            }
        }

        // Settings Button
        SecondaryButton {
            Layout.alignment: Qt.AlignHCenter
            text: "Settings"
            onClicked: root.requestSettings()
        }
    }
}
