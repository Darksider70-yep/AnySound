import QtQuick 2.15
import QtQuick.Layouts 1.15
import ".."

Item {
    id: root

    // syncErrorMs: measured error in ms (negative or positive)
    property real syncErrorMs: 0.0
    property string connectionState: "connected" // "idle", "connecting", "connected", "lost"

    // Sync categories: "tight" (<=5ms), "drifting" (5-20ms), "out" (>20ms), "connecting", "lost"
    readonly property string syncCategory: {
        if (connectionState === "connecting") return "connecting";
        if (connectionState === "lost") return "lost";
        const absErr = Math.abs(syncErrorMs);
        if (absErr <= 5.0) return "tight";
        if (absErr <= 20.0) return "drifting";
        return "out";
    }

    readonly property color stateColor: {
        if (syncCategory === "tight") return Theme.sonar;
        if (syncCategory === "drifting") return Theme.drift;
        if (syncCategory === "out" || syncCategory === "lost") return Theme.lost;
        return Theme.mist;
    }

    readonly property string stateLabel: {
        if (syncCategory === "tight") return "In sync";
        if (syncCategory === "drifting") return "Drifting";
        if (syncCategory === "out") return "Out of sync";
        if (syncCategory === "connecting") return "Connecting";
        return "Lost connection";
    }

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight

    RowLayout {
        id: layout
        spacing: Theme.space8
        anchors.verticalCenter: parent.verticalCenter

        // Shape indicator (Circle / Dotted / Ring)
        Canvas {
            id: shapeCanvas
            Layout.preferredWidth: 12
            Layout.preferredHeight: 12
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const cx = width / 2;
                const cy = height / 2;
                const r = 5;

                ctx.strokeStyle = root.stateColor;
                ctx.fillStyle = root.stateColor;
                ctx.lineWidth = 1.5;

                if (root.syncCategory === "tight") {
                    ctx.beginPath();
                    ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                    ctx.fill();
                } else if (root.syncCategory === "drifting") {
                    ctx.beginPath();
                    ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.arc(cx, cy, r, -Math.PI / 2, Math.PI / 2, false);
                    ctx.fill();
                } else if (root.syncCategory === "out") {
                    ctx.beginPath();
                    ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(cx - 3, cy - 3);
                    ctx.lineTo(cx + 3, cy + 3);
                    ctx.stroke();
                } else if (root.syncCategory === "connecting") {
                    ctx.setLineDash([2, 2]);
                    ctx.beginPath();
                    ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                    ctx.stroke();
                } else {
                    ctx.beginPath();
                    ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                    ctx.stroke();
                }
            }

            Connections {
                target: root
                function onSyncErrorMsChanged() { shapeCanvas.requestPaint(); }
                function onConnectionStateChanged() { shapeCanvas.requestPaint(); }
            }
        }

        Text {
            text: root.stateLabel
            font.family: Theme.sansFont
            font.pixelSize: Theme.fontSizeSecondary
            font.weight: Font.Medium
            color: root.stateColor
        }

        Text {
            visible: (root.syncCategory === "tight" || root.syncCategory === "drifting" || root.syncCategory === "out")
            text: "±" + Math.abs(Math.round(root.syncErrorMs)) + " ms"
            font.family: Theme.sansFont
            font.pixelSize: Theme.fontSizeSecondary
            font.weight: Font.Normal
            color: Theme.mist
        }
    }
}
