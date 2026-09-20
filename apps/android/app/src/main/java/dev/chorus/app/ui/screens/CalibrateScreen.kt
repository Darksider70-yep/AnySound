package dev.chorus.app.ui.screens

import androidx.compose.foundation.background
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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import dev.chorus.app.ui.components.DelayStepper
import dev.chorus.app.ui.theme.ChorusTheme

@Composable
fun CalibrateScreen(
    offsetMs: Int,
    onOffsetChange: (Int) -> Unit,
    onBack: () -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .background(ChorusTheme.colors.harbor)
            .padding(24.dp),
        verticalArrangement = Arrangement.SpaceBetween
    ) {
        Column {
            // Header
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.padding(bottom = 24.dp)
            ) {
                Box(
                    modifier = Modifier
                        .size(40.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .clickable { onBack() },
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                        contentDescription = "Back",
                        tint = ChorusTheme.colors.fog
                    )
                }
                Spacer(modifier = Modifier.width(12.dp))
                Text(
                    text = "Calibrate speaker",
                    style = ChorusTheme.typography.titleLarge,
                    color = ChorusTheme.colors.fog
                )
            }

            Text(
                text = "If this device is slightly ahead or behind other speakers (e.g. over Bluetooth or high-latency DAC), adjust the delay offset until sound reinforces cleanly.",
                style = ChorusTheme.typography.bodyMedium,
                color = ChorusTheme.colors.mist
            )

            Spacer(modifier = Modifier.height(32.dp))

            DelayStepper(
                offsetMs = offsetMs,
                onOffsetChange = onOffsetChange
            )
        }

        // Done button
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(48.dp)
                .clip(RoundedCornerShape(8.dp))
                .background(ChorusTheme.colors.sonar)
                .clickable { onBack() },
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = "Done",
                style = ChorusTheme.typography.bodyLarge,
                color = ChorusTheme.colors.harbor
            )
        }
    }
}
