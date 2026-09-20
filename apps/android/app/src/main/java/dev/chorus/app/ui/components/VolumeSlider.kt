package dev.chorus.app.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.VolumeMute
import androidx.compose.material.icons.automirrored.filled.VolumeUp
import androidx.compose.material3.Icon
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import dev.chorus.app.ui.theme.ChorusTheme

@Composable
fun VolumeSlider(
    volume: Float,
    isMuted: Boolean,
    onVolumeChange: (Float) -> Unit,
    onMuteToggle: () -> Unit,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(ChorusTheme.colors.deck)
            .border(1.dp, ChorusTheme.colors.line, RoundedCornerShape(12.dp))
            .padding(horizontal = 16.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Mute Icon button
        Box(
            modifier = Modifier
                .size(40.dp)
                .clip(RoundedCornerShape(8.dp))
                .background(if (isMuted) ChorusTheme.colors.line else ChorusTheme.colors.deck)
                .clickable { onMuteToggle() },
            contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = if (isMuted) Icons.AutoMirrored.Filled.VolumeMute else Icons.AutoMirrored.Filled.VolumeUp,
                contentDescription = if (isMuted) "Unmute" else "Mute",
                tint = if (isMuted) ChorusTheme.colors.mist else ChorusTheme.colors.sonar,
                modifier = Modifier.size(24.dp)
            )
        }

        Spacer(modifier = Modifier.width(12.dp))

        // Custom Slider
        Slider(
            value = if (isMuted) 0.0f else volume,
            onValueChange = { onVolumeChange(it) },
            valueRange = 0.0f..1.0f,
            colors = SliderDefaults.colors(
                thumbColor = ChorusTheme.colors.sonar,
                activeTrackColor = ChorusTheme.colors.sonar,
                inactiveTrackColor = ChorusTheme.colors.line
            ),
            modifier = Modifier.weight(1.0f)
        )

        Spacer(modifier = Modifier.width(12.dp))

        // Percentage text
        Text(
            text = "${(if (isMuted) 0 else (volume * 100).toInt())}%",
            style = ChorusTheme.typography.bodyMedium,
            color = ChorusTheme.colors.mist,
            modifier = Modifier.width(44.dp)
        )
    }
}
