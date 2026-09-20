import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import ".."
import "../components"

Item {
    id: root

    signal close()

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.space48, 540)
        spacing: Theme.space24

        // Header
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "Settings"
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

        // Settings Panel
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radiusPanel
            color: Theme.deck
            border.color: Theme.line
            border.width: 1
            implicitHeight: settingsLayout.implicitHeight + Theme.space32

            ColumnLayout {
                id: settingsLayout
                anchors.fill: parent
                anchors.margins: Theme.space24
                spacing: Theme.space24

                // Theme selection
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "Theme"
                        font.family: Theme.sansFont
                        font.pixelSize: Theme.fontSizeBody
                        color: Theme.fog
                    }

                    Item { Layout.fillWidth: true }

                    RowLayout {
                        spacing: Theme.space8

                        SecondaryButton {
                            text: "Dark"
                            onClicked: Theme.themeMode = 1
                        }

                        SecondaryButton {
                            text: "Light"
                            onClicked: Theme.themeMode = 2
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.line
                }

                // Reduced motion
                RowLayout {
                    Layout.fillWidth: true

                    ColumnLayout {
                        spacing: Theme.space4
                        Text {
                            text: "Reduced motion"
                            font.family: Theme.sansFont
                            font.pixelSize: Theme.fontSizeBody
                            color: Theme.fog
                        }
                        Text {
                            text: "Replaces pulsing sonar animations with static indicators"
                            font.family: Theme.sansFont
                            font.pixelSize: Theme.fontSizeCaption
                            color: Theme.mist
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Switch {
                        checked: Theme.reducedMotion
                        onToggled: Theme.reducedMotion = checked
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.line
                }

                // Advanced Network Ports Info
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space8

                    Text {
                        text: "Network Configuration"
                        font.family: Theme.sansFont
                        font.pixelSize: Theme.fontSizeSecondary
                        font.weight: Font.DemiBold
                        color: Theme.fog
                    }

                    Text {
                        text: "TCP Control: 47800 | UDP Audio: 47801 | Discovery Beacon: 47803"
                        font.family: Theme.sansFont
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.mist
                    }
                }
            }
        }
    }
}
