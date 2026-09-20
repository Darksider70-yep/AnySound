import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import ".."
import "../components"

Item {
    id: root

    property var discoveredHosts: []
    property bool isScanning: true

    signal joinHost(string hostIp, int tcpPort, string pin)
    signal cancel()

    // PIN dialog state
    property bool showPinDialog: false
    property string selectedHostIp: ""
    property int selectedHostPort: 47800
    property string selectedHostName: ""
    property string enteredPin: ""

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.space48, 560)
        spacing: Theme.space24

        // Title Header
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "Nearby"
                font.family: Theme.serifFont
                font.pixelSize: Theme.fontSizeTitle
                color: Theme.fog
            }

            Item { Layout.fillWidth: true }

            SecondaryButton {
                text: "Back"
                onClicked: root.cancel()
            }
        }

        // Hairline Separator
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.line
        }

        // Host List / Empty State
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 280
            radius: Theme.radiusPanel
            color: Theme.deck
            border.color: Theme.line
            border.width: 1

            // Empty state
            ColumnLayout {
                visible: root.discoveredHosts.length === 0
                anchors.centerIn: parent
                width: parent.width - Theme.space48
                spacing: Theme.space12

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "No devices found yet."
                    font.family: Theme.sansFont
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: Font.DemiBold
                    color: Theme.fog
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: "Make sure you're on the same Wi-Fi as the sharing device.\nSome networks block broadcast discovery; you can also enter the IP directly below."
                    font.family: Theme.sansFont
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.mist
                }
            }

            // Discovered Hosts ListView
            ListView {
                visible: root.discoveredHosts.length > 0
                anchors.fill: parent
                model: root.discoveredHosts
                clip: true

                delegate: Rectangle {
                    width: parent.width
                    height: 64
                    color: itemMouse.containsMouse ? Theme.line : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.space16
                        anchors.rightMargin: Theme.space16

                        ColumnLayout {
                            spacing: Theme.space4
                            Text {
                                text: modelData.name || "Chorus Host"
                                font.family: Theme.sansFont
                                font.pixelSize: Theme.fontSizeBody
                                font.weight: Font.DemiBold
                                color: Theme.fog
                            }
                            Text {
                                text: modelData.address + ":" + modelData.tcp_port
                                font.family: Theme.sansFont
                                font.pixelSize: Theme.fontSizeCaption
                                color: Theme.mist
                            }
                        }

                        Item { Layout.fillWidth: true }

                        PrimaryButton {
                            text: "Join"
                            onClicked: {
                                root.selectedHostIp = modelData.address;
                                root.selectedHostPort = modelData.tcp_port;
                                root.selectedHostName = modelData.name || "Chorus Host";
                                root.enteredPin = "";
                                root.showPinDialog = true;
                            }
                        }
                    }

                    MouseArea {
                        id: itemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        z: -1
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Theme.line
                    }
                }
            }
        }

        // Manual IP / PIN fallback
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space12

            Text {
                text: "Not listed?"
                font.family: Theme.sansFont
                font.pixelSize: Theme.fontSizeSecondary
                color: Theme.mist
            }

            SecondaryButton {
                text: "Enter address manually"
                onClicked: {
                    root.selectedHostIp = "127.0.0.1";
                    root.selectedHostPort = 47800;
                    root.selectedHostName = "Manual Host";
                    root.enteredPin = "";
                    root.showPinDialog = true;
                }
            }
        }
    }

    // PIN Entry Modal Dialog
    Rectangle {
        visible: root.showPinDialog
        anchors.fill: parent
        color: Qt.rgba(Theme.harbor.r, Theme.harbor.g, Theme.harbor.b, 0.85)

        Rectangle {
            anchors.centerIn: parent
            width: 380
            height: 280
            radius: Theme.radiusPanel
            color: Theme.deck
            border.color: Theme.line
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.space24
                spacing: Theme.space16

                Text {
                    text: "Enter 4-Digit Code"
                    font.family: Theme.serifFont
                    font.pixelSize: Theme.fontSizeTitle
                    color: Theme.fog
                }

                Text {
                    text: "Enter the code shown on " + root.selectedHostName
                    font.family: Theme.sansFont
                    font.pixelSize: Theme.fontSizeCaption
                    color: Theme.mist
                }

                // PIN Input Box
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    radius: Theme.radiusControl
                    color: Theme.harbor
                    border.color: pinInput.activeFocus ? Theme.sonar : Theme.line
                    border.width: 1

                    TextInput {
                        id: pinInput
                        anchors.centerIn: parent
                        font.family: Theme.sansFont
                        font.pixelSize: 24
                        font.weight: Font.Bold
                        font.letterSpacing: 8
                        color: Theme.fog
                        maximumLength: 4
                        inputMask: "9999"
                        focus: root.showPinDialog
                        onTextChanged: root.enteredPin = text
                        onAccepted: {
                            if (root.enteredPin.length === 4) {
                                root.showPinDialog = false;
                                root.joinHost(root.selectedHostIp, root.selectedHostPort, root.enteredPin);
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space12

                    SecondaryButton {
                        Layout.fillWidth: true
                        text: "Cancel"
                        onClicked: root.showPinDialog = false
                    }

                    PrimaryButton {
                        Layout.fillWidth: true
                        text: "Connect"
                        enabled: root.enteredPin.length === 4
                        onClicked: {
                            root.showPinDialog = false;
                            root.joinHost(root.selectedHostIp, root.selectedHostPort, root.enteredPin);
                        }
                    }
                }
            }
        }
    }
}
