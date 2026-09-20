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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Smartphone
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
import dev.chorus.app.ui.theme.ChorusTheme

@Composable
fun HostScreen(
    snapshot: AppSnapshot,
    onBack: () -> Unit,
    onStopHosting: () -> Unit,
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
                modifier = Modifier.padding(bottom = 16.dp)
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
                    text = "Hosting audio",
                    style = ChorusTheme.typography.titleLarge,
                    color = ChorusTheme.colors.fog
                )
            }

            // PIN badge
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(12.dp))
                    .background(ChorusTheme.colors.deck)
                    .border(1.dp, ChorusTheme.colors.line, RoundedCornerShape(12.dp))
                    .padding(16.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text(
                            text = "Room PIN",
                            style = ChorusTheme.typography.bodyMedium,
                            color = ChorusTheme.colors.mist
                        )
                        Text(
                            text = if (snapshot.sessionPin.isNotEmpty()) snapshot.sessionPin else "No PIN required",
                            style = ChorusTheme.typography.titleLarge,
                            color = ChorusTheme.colors.sonar
                        )
                    }
                    Text(
                        text = "${snapshot.connectedClients.size} speaker(s)",
                        style = ChorusTheme.typography.bodyLarge,
                        color = ChorusTheme.colors.fog
                    )
                }
            }

            Spacer(modifier = Modifier.height(20.dp))

            // Connected Clients List
            Text(
                text = "Connected speakers",
                style = ChorusTheme.typography.bodyMedium,
                color = ChorusTheme.colors.mist
            )
            Spacer(modifier = Modifier.height(8.dp))

            if (snapshot.connectedClients.isEmpty()) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(100.dp)
                        .clip(RoundedCornerShape(12.dp))
                        .background(ChorusTheme.colors.deck)
                        .border(1.dp, ChorusTheme.colors.line, RoundedCornerShape(12.dp)),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = "Waiting for other devices to join...",
                        style = ChorusTheme.typography.bodyMedium,
                        color = ChorusTheme.colors.mist
                    )
                }
            } else {
                LazyColumn(
                    modifier = Modifier.fillMaxWidth(),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    items(snapshot.connectedClients) { client ->
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(12.dp))
                                .background(ChorusTheme.colors.deck)
                                .border(1.dp, ChorusTheme.colors.line, RoundedCornerShape(12.dp))
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
                                        imageVector = Icons.Default.Smartphone,
                                        contentDescription = null,
                                        tint = ChorusTheme.colors.sonar
                                    )
                                    Column {
                                        Text(
                                            text = client.name.ifEmpty { "Client ${client.clientId}" },
                                            style = ChorusTheme.typography.bodyLarge,
                                            color = ChorusTheme.colors.fog
                                        )
                                        Text(
                                            text = client.endpoint,
                                            style = ChorusTheme.typography.labelSmall,
                                            color = ChorusTheme.colors.mist
                                        )
                                    }
                                }

                                SyncBadge(
                                    syncErrorMs = client.syncErrorUs / 1000.0f,
                                    isConnected = true
                                )
                            }
                        }
                    }
                }
            }
        }

        // Stop Hosting Button
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(48.dp)
                .clip(RoundedCornerShape(8.dp))
                .background(ChorusTheme.colors.lost.copy(alpha = 0.15f))
                .border(1.dp, ChorusTheme.colors.lost.copy(alpha = 0.5f), RoundedCornerShape(8.dp))
                .clickable { onStopHosting() },
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = "Stop broadcasting",
                style = ChorusTheme.typography.bodyLarge,
                color = ChorusTheme.colors.lost
            )
        }
    }
}
