import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import "."
import "views"

ApplicationWindow {
    id: appWindow
    visible: true
    width: 960
    height: 640
    minimumWidth: 800
    minimumHeight: 540
    title: "Chorus — Synchronized LAN Audio"
    color: Theme.harbor

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: homeView

        pushEnter: Transition { PropertyAnimation { property: "opacity"; from: 0; to: 1; duration: 150 } }
        pushExit: Transition { PropertyAnimation { property: "opacity"; from: 1; to: 0; duration: 150 } }
        popEnter: Transition { PropertyAnimation { property: "opacity"; from: 0; to: 1; duration: 150 } }
        popExit: Transition { PropertyAnimation { property: "opacity"; from: 1; to: 0; duration: 150 } }
    }

    // Component Views
    Component {
        id: homeView
        HomeView {
            onRequestHost: {
                if (typeof uiController !== "undefined") {
                    uiController.startHost();
                }
                stackView.push(hostView);
            }
            onRequestJoin: {
                if (typeof uiController !== "undefined") {
                    uiController.startScan();
                }
                stackView.push(clientFindView);
            }
            onRequestSettings: stackView.push(settingsView)
        }
    }

    Component {
        id: hostView
        HostView {
            pinCode: (typeof uiController !== "undefined") ? uiController.sessionPin : "4821"
            clientDevices: (typeof uiController !== "undefined") ? uiController.connectedClients : []
            masterVolume: (typeof uiController !== "undefined") ? uiController.masterVolume : 1.0
            targetLatencyMs: (typeof uiController !== "undefined") ? uiController.targetLatencyMs : 300

            onStopSharing: {
                if (typeof uiController !== "undefined") {
                    uiController.stopHost();
                }
                stackView.pop();
            }
            onSetClientVolume: (id, vol) => {
                if (typeof uiController !== "undefined") uiController.setClientVolume(id, vol);
            }
            onSetClientMute: (id, muted) => {
                if (typeof uiController !== "undefined") uiController.setClientMute(id, muted);
            }
            onSetClientOffset: (id, off) => {
                if (typeof uiController !== "undefined") uiController.setClientOffset(id, off);
            }
            onSetMasterVolume: (vol) => {
                if (typeof uiController !== "undefined") uiController.setMasterVolume(vol);
            }
            onSetTargetLatency: (lat) => {
                if (typeof uiController !== "undefined") uiController.setTargetLatency(lat);
            }
        }
    }

    Component {
        id: clientFindView
        ClientFindView {
            discoveredHosts: (typeof uiController !== "undefined") ? uiController.discoveredHosts : []

            onJoinHost: (ip, port, pin) => {
                if (typeof uiController !== "undefined") {
                    uiController.joinHost(ip, port, pin);
                }
                stackView.push(clientListeningView);
            }
            onCancel: stackView.pop()
        }
    }

    Component {
        id: clientListeningView
        ClientListeningView {
            hostName: (typeof uiController !== "undefined") ? uiController.connectedHostName : "Sharing Device"
            syncErrorMs: (typeof uiController !== "undefined") ? uiController.syncErrorMs : 0.0
            syncState: (typeof uiController !== "undefined") ? uiController.syncState : "tight"
            volume: (typeof uiController !== "undefined") ? uiController.clientVolume : 1.0
            isMuted: (typeof uiController !== "undefined") ? uiController.clientMuted : false
            offsetMs: (typeof uiController !== "undefined") ? uiController.clientOffsetMs : 0

            onSetVolume: (vol) => {
                if (typeof uiController !== "undefined") uiController.setLocalVolume(vol);
            }
            onSetMute: (muted) => {
                if (typeof uiController !== "undefined") uiController.setLocalMute(muted);
            }
            onSetOffset: (off) => {
                if (typeof uiController !== "undefined") uiController.setLocalOffset(off);
            }
            onCalibrate: stackView.push(calibrateView)
            onLeave: {
                if (typeof uiController !== "undefined") {
                    uiController.leaveHost();
                }
                stackView.pop();
            }
        }
    }

    Component {
        id: calibrateView
        CalibrateView {
            offsetMs: (typeof uiController !== "undefined") ? uiController.clientOffsetMs : 0
            onSaveOffset: (off) => {
                if (typeof uiController !== "undefined") uiController.setLocalOffset(off);
            }
            onClose: stackView.pop()
        }
    }

    Component {
        id: settingsView
        SettingsView {
            onClose: stackView.pop()
        }
    }
}
