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
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Hearing
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.SpatialAudio
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import dev.chorus.app.ui.theme.ChorusTheme

@Composable
fun HomeScreen(
    onNavigateToFind: () -> Unit,
    onNavigateToHost: () -> Unit,
    onNavigateToSettings: () -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxSize()
            .background(ChorusTheme.colors.harbor)
            .padding(24.dp),
        verticalArrangement = Arrangement.SpaceBetween
    ) {
        // Header
        Column(modifier = Modifier.padding(top = 24.dp)) {
            Text(
                text = "Chorus",
                style = ChorusTheme.typography.titleLarge,
                color = ChorusTheme.colors.fog
            )
            Spacer(modifier = Modifier.height(4.dp))
            Text(
                text = "Make every device a speaker.",
                style = ChorusTheme.typography.bodyMedium,
                color = ChorusTheme.colors.mist
            )
        }

        // Action Cards
        Column(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // Join / Client Card
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(12.dp))
                    .background(ChorusTheme.colors.deck)
                    .border(1.dp, ChorusTheme.colors.line, RoundedCornerShape(12.dp))
                    .clickable { onNavigateToFind() }
                    .padding(20.dp)
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(16.dp)
                ) {
                    Box(
                        modifier = Modifier
                            .size(48.dp)
                            .clip(RoundedCornerShape(8.dp))
                            .background(ChorusTheme.colors.line),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = Icons.Default.Hearing,
                            contentDescription = null,
                            tint = ChorusTheme.colors.sonar,
                            modifier = Modifier.size(28.dp)
                        )
                    }
                    Column {
                        Text(
                            text = "Play sound from another device",
                            style = ChorusTheme.typography.bodyLarge,
                            color = ChorusTheme.colors.fog
                        )
                        Spacer(modifier = Modifier.height(2.dp))
                        Text(
                            text = "Join as an extra speaker in the room",
                            style = ChorusTheme.typography.bodyMedium,
                            color = ChorusTheme.colors.mist
                        )
                    }
                }
            }

            // Host Card
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(12.dp))
                    .background(ChorusTheme.colors.deck)
                    .border(1.dp, ChorusTheme.colors.line, RoundedCornerShape(12.dp))
                    .clickable { onNavigateToHost() }
                    .padding(20.dp)
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(16.dp)
                ) {
                    Box(
                        modifier = Modifier
                            .size(48.dp)
                            .clip(RoundedCornerShape(8.dp))
                            .background(ChorusTheme.colors.line),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = Icons.Default.SpatialAudio,
                            contentDescription = null,
                            tint = ChorusTheme.colors.sonar,
                            modifier = Modifier.size(28.dp)
                        )
                    }
                    Column {
                        Text(
                            text = "Share this device's sound",
                            style = ChorusTheme.typography.bodyLarge,
                            color = ChorusTheme.colors.fog
                        )
                        Spacer(modifier = Modifier.height(2.dp))
                        Text(
                            text = "Broadcast synchronized test audio to clients",
                            style = ChorusTheme.typography.bodyMedium,
                            color = ChorusTheme.colors.mist
                        )
                    }
                }
            }
        }

        // Footer / Settings
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 16.dp),
            horizontalArrangement = Arrangement.End
        ) {
            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(8.dp))
                    .clickable { onNavigateToSettings() }
                    .padding(8.dp)
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Settings,
                        contentDescription = "Settings",
                        tint = ChorusTheme.colors.mist,
                        modifier = Modifier.size(20.dp)
                    )
                    Text(
                        text = "Settings",
                        style = ChorusTheme.typography.bodyMedium,
                        color = ChorusTheme.colors.mist
                    )
                }
            }
        }
    }
}
