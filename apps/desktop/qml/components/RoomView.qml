import QtQuick 2.15
import ".."

Item {
    id: root

    // devices: list of objects { id, name, syncErrorMs, syncState, isHost }
    property var devices: []
    property real masterPhase: 0.0

    clip: true

    Timer {
        id: animTimer
        interval: 33 // ~30 fps
        running: root.visible && !Theme.reducedMotion
        repeat: true
        onTriggered: {
            root.masterPhase = (root.masterPhase + (33 / 2400.0)) % 1.0;
            canvas.requestPaint();
        }
    }

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();

            const cx = width / 2;
            const cy = height / 2;

            // Background subtle grid/rings
            ctx.strokeStyle = Theme.line;
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.arc(cx, cy, Math.min(cx, cy) * 0.75, 0, 2 * Math.PI);
            ctx.stroke();

            // Host Node at Center
            drawNode(ctx, cx, cy, "This computer (Host)", Theme.sonar, 0.0, true);

            // Orbiting devices
            const clientList = root.devices || [];
            const count = clientList.length;
            if (count > 0) {
                const radius = Math.min(width, height) * 0.35;
                for (let i = 0; i < count; ++i) {
                    const dev = clientList[i];
                    const angle = (2 * Math.PI * i / count) - (Math.PI / 2);
                    const dx = cx + Math.cos(angle) * radius;
                    const dy = cy + Math.sin(angle) * radius;

                    // Compute phase offset: phase = clamp(syncErrorMs / 20, -1, 1) * 0.5
                    const syncErr = dev.syncErrorMs || 0.0;
                    const phaseOffset = Math.max(-1.0, Math.min(1.0, syncErr / 20.0)) * 0.5;

                    let nodeColor = Theme.sonar;
                    if (dev.syncState === "drifting") nodeColor = Theme.drift;
                    else if (dev.syncState === "out" || dev.syncState === "lost") nodeColor = Theme.lost;

                    drawNode(ctx, dx, dy, dev.name || "Client", nodeColor, phaseOffset, false);
                }
            }
        }

        function drawNode(ctx, x, y, name, color, phaseOffset, isHost) {
            // Draw expanding sound rings (2 rings staggered by 0.5)
            if (!Theme.reducedMotion) {
                for (let ringIdx = 0; ringIdx < 2; ++ringIdx) {
                    const ringProg = ((root.masterPhase + phaseOffset + (ringIdx * 0.5)) % 1.0 + 1.0) % 1.0;
                    const maxRadius = isHost ? 60 : 45;
                    const currentR = 8 + (ringProg * maxRadius);
                    const alpha = (1.0 - ringProg) * 0.6;

                    ctx.strokeStyle = Qt.rgba(color.r, color.g, color.b, alpha);
                    ctx.lineWidth = 1.5;
                    ctx.beginPath();
                    ctx.arc(x, y, currentR, 0, 2 * Math.PI);
                    ctx.stroke();
                }
            } else {
                // Static concentric ring for reduced-motion mode
                ctx.strokeStyle = Qt.rgba(color.r, color.g, color.b, 0.4);
                ctx.lineWidth = 1.5;
                ctx.beginPath();
                ctx.arc(x, y, isHost ? 32 : 24, 0, 2 * Math.PI);
                ctx.stroke();
            }

            // Node core dot
            ctx.fillStyle = color;
            ctx.beginPath();
            ctx.arc(x, y, isHost ? 8 : 6, 0, 2 * Math.PI);
            ctx.fill();

            // Label beneath node
            ctx.font = "500 12px " + Theme.sansFont;
            ctx.fillStyle = Theme.fog;
            ctx.textAlign = "center";
            ctx.fillText(name, x, y + 20);
        }
    }

    Connections {
        target: root
        function onDevicesChanged() { canvas.requestPaint(); }
    }
}
