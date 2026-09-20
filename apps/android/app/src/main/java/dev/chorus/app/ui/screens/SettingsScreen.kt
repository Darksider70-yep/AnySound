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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import dev.chorus.app.ui.theme.ChorusTheme

@Composable
fun SettingsScreen(
    onBack: () -> Unit,
    modifier: Modifier = Modifier
) {
    var selectedTheme by remember { mutableStateOf("Dark") }
    var selectedLatency by remember { mutableStateOf("300 ms") }
    val scrollState = rememberScrollState()

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(ChorusTheme.colors.harbor)
            .padding(24.dp)
            .verticalScroll(scrollState)
    ) {
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
                text = "Settings",
                style = ChorusTheme.typography.titleLarge,
                color = ChorusTheme.colors.fog
            )
        }

        // Appearance
        Text(
            text = "Appearance",
            style = ChorusTheme.typography.bodyLarge,
            color = ChorusTheme.colors.fog
        )
        Spacer(modifier = Modifier.height(8.dp))
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            listOf("Dark", "Light", "System").forEach { themeName ->
                val isSelected = selectedTheme == themeName
                Box(
                    modifier = Modifier
                        .weight(1.0f)
                        .height(44.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .background(if (isSelected) ChorusTheme.colors.sonar else ChorusTheme.colors.deck)
                        .border(1.dp, if (isSelected) ChorusTheme.colors.sonar else ChorusTheme.colors.line, RoundedCornerShape(8.dp))
                        .clickable { selectedTheme = themeName },
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = themeName,
                        style = ChorusTheme.typography.bodyMedium,
                        color = if (isSelected) ChorusTheme.colors.harbor else ChorusTheme.colors.fog
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(28.dp))

        // Buffer & Latency Presets
        Text(
            text = "Buffer & latency preset",
            style = ChorusTheme.typography.bodyLarge,
            color = ChorusTheme.colors.fog
        )
        Spacer(modifier = Modifier.height(8.dp))
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            listOf("100 ms", "300 ms", "500 ms").forEach { lat ->
                val isSelected = selectedLatency == lat
                Box(
                    modifier = Modifier
                        .weight(1.0f)
                        .height(44.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .background(if (isSelected) ChorusTheme.colors.sonar else ChorusTheme.colors.deck)
                        .border(1.dp, if (isSelected) ChorusTheme.colors.sonar else ChorusTheme.colors.line, RoundedCornerShape(8.dp))
                        .clickable { selectedLatency = lat },
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = lat,
                        style = ChorusTheme.typography.bodyMedium,
                        color = if (isSelected) ChorusTheme.colors.harbor else ChorusTheme.colors.fog
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(28.dp))

        // System Info & Diagnostics
        Text(
            text = "Diagnostics",
            style = ChorusTheme.typography.bodyLarge,
            color = ChorusTheme.colors.fog
        )
        Spacer(modifier = Modifier.height(8.dp))
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(12.dp))
                .background(ChorusTheme.colors.deck)
                .border(1.dp, ChorusTheme.colors.line, RoundedCornerShape(12.dp))
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            DiagnosticRow("Audio engine", "AAudio / miniaudio 48 kHz stereo")
            DiagnosticRow("Opus codec", "libopus 1.5.2 (20 ms frames, PLC enabled)")
            DiagnosticRow("Protocol version", "Chorus v1")
            DiagnosticRow("Wi-Fi Lock", "Low-latency mode active")
            DiagnosticRow("Wake Lock", "Partial wake lock held")
        }
    }
}

@Composable
private fun DiagnosticRow(label: String, value: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = label,
            style = ChorusTheme.typography.bodyMedium,
            color = ChorusTheme.colors.mist
        )
        Text(
            text = value,
            style = ChorusTheme.typography.labelSmall,
            color = ChorusTheme.colors.fog
        )
    }
}
