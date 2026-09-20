package dev.chorus.app.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import dev.chorus.app.service.AppSnapshot
import dev.chorus.app.ui.components.SonarRingView
import dev.chorus.app.ui.components.SyncBadge
import dev.chorus.app.ui.components.VolumeSlider
import dev.chorus.app.ui.theme.ChorusTheme
import kotlin.math.abs

@Composable
fun ClientListeningScreen(
    snapshot: AppSnapshot,
    onVolumeChange: (Float) -> Unit,
    onMuteToggle: () -> Unit,
    onNavigateToCalibrate: () -> Unit,
    onDisconnect: () -> Unit,
    modifier: Modifier = Modifier
) {
    val scrollState = rememberScrollState()
    val syncErrorMs = snapshot.stats.syncErrorMs

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(ChorusTheme.colors.harbor)
            .padding(24.dp)
            .verticalScroll(scrollState),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceBetween
    ) {
        // Top status badge
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            modifier = Modifier.padding(top = 16.dp)
        ) {
            SyncBadge(
                syncErrorMs = syncErrorMs,
                isConnected = snapshot.isActive,
                isConnecting = !snapshot.isActive
            )

            Spacer(modifier = Modifier.height(24.dp))

            // Hero sync readout
            Text(
                text = "\u00B1${String.format("%.1f", abs(syncErrorMs))} ms",
                style = ChorusTheme.typography.displayLarge,
                color = ChorusTheme.colors.fog
            )

            Text(
                text = "Playout sync error",
                style = ChorusTheme.typography.bodyMedium,
                color = ChorusTheme.colors.mist
            )
        }

        // Sonar Animation
        Box(
            modifier = Modifier.padding(vertical = 24.dp),
            contentAlignment = Alignment.Center
        ) {
            SonarRingView(
                syncErrorMs = syncErrorMs,
                isActive = snapshot.isActive,
                modifier = Modifier.size(240.dp)
            )
        }

        // Controls Section
        Column(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // Volume Slider
            VolumeSlider(
                volume = snapshot.volume,
                isMuted = snapshot.isMuted,
                onVolumeChange = onVolumeChange,
                onMuteToggle = onMuteToggle
            )

            // Calibration / Delay Quick Action
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(12.dp))
                    .background(ChorusTheme.colors.deck)
                    .border(1.dp, ChorusTheme.colors.line, RoundedCornerShape(12.dp))
                    .clickable { onNavigateToCalibrate() }
                    .padding(16.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(12.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.Tune,
                            contentDescription = "Calibrate",
                            tint = ChorusTheme.colors.sonar
                        )
                        Column {
                            Text(
                                text = "Calibrate offset",
                                style = ChorusTheme.typography.bodyLarge,
                                color = ChorusTheme.colors.fog
                            )
                            Text(
                                text = "Current offset: ${if (snapshot.offsetMs > 0) "+" else ""}${snapshot.offsetMs} ms",
                                style = ChorusTheme.typography.labelSmall,
                                color = ChorusTheme.colors.mist
                            )
                        }
                    }
                    Text(
                        text = "Adjust",
                        style = ChorusTheme.typography.bodyMedium,
                        color = ChorusTheme.colors.sonar
                    )
                }
            }

            // Disconnect Button
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(48.dp)
                    .clip(RoundedCornerShape(8.dp))
                    .background(ChorusTheme.colors.lost.copy(alpha = 0.15f))
                    .border(1.dp, ChorusTheme.colors.lost.copy(alpha = 0.5f), RoundedCornerShape(8.dp))
                    .clickable { onDisconnect() },
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "Disconnect",
                    style = ChorusTheme.typography.bodyLarge,
                    color = ChorusTheme.colors.lost
                )
            }
        }
    }
}
