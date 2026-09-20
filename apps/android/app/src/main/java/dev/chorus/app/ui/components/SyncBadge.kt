package dev.chorus.app.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import dev.chorus.app.ui.theme.ChorusTheme
import kotlin.math.abs

enum class SyncState {
    Tight,
    Drifting,
    OutOfSync,
    Connecting,
    Lost
}

@Composable
fun SyncBadge(
    syncErrorMs: Float,
    isConnected: Boolean,
    isConnecting: Boolean = false,
    modifier: Modifier = Modifier
) {
    val state = when {
        !isConnected -> SyncState.Lost
        isConnecting -> SyncState.Connecting
        abs(syncErrorMs) <= 5.0f -> SyncState.Tight
        abs(syncErrorMs) <= 20.0f -> SyncState.Drifting
        else -> SyncState.OutOfSync
    }

    val color = when (state) {
        SyncState.Tight -> ChorusTheme.colors.sonar
        SyncState.Drifting -> ChorusTheme.colors.drift
        SyncState.OutOfSync -> ChorusTheme.colors.lost
        SyncState.Connecting -> ChorusTheme.colors.mist
        SyncState.Lost -> ChorusTheme.colors.lost
    }

    val label = when (state) {
        SyncState.Tight -> "In sync"
        SyncState.Drifting -> "Drifting"
        SyncState.OutOfSync -> "Out of sync"
        SyncState.Connecting -> "Connecting"
        SyncState.Lost -> "Lost connection"
    }

    Row(
        modifier = modifier
            .clip(RoundedCornerShape(fullPillCornerRadius))
            .background(ChorusTheme.colors.deck)
            .border(1.dp, ChorusTheme.colors.line, RoundedCornerShape(fullPillCornerRadius))
            .padding(horizontal = 12.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Status indicator shape
        Box(
            modifier = Modifier
                .size(8.dp)
                .clip(CircleShape)
                .background(color)
        )

        Spacer(modifier = Modifier.width(8.dp))

        Text(
            text = if (isConnected && !isConnecting) {
                "$label  \u00B1${String.format("%.1f", abs(syncErrorMs))} ms"
            } else {
                label
            },
            style = ChorusTheme.typography.bodyMedium,
            color = ChorusTheme.colors.fog
        )
    }
}

private val fullPillCornerRadius = 999.dp
