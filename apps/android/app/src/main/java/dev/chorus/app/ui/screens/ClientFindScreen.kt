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
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Cast
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import dev.chorus.app.net.NsdDiscoveredHost
import dev.chorus.app.service.DiscoveredHost
import dev.chorus.app.ui.theme.ChorusTheme

@Composable
fun ClientFindScreen(
    discoveredHosts: List<DiscoveredHost>,
    nsdHosts: List<NsdDiscoveredHost>,
    rejectionReason: String,
    onBack: () -> Unit,
    onJoin: (hostIp: String, port: Int, pin: String) -> Unit,
    modifier: Modifier = Modifier
) {
    var hostIp by remember { mutableStateOf("127.0.0.1") }
    var portStr by remember { mutableStateOf("47800") }
    var pin by remember { mutableStateOf("") }
    var showManual by remember { mutableStateOf(false) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(ChorusTheme.colors.harbor)
            .padding(24.dp)
    ) {
        // Back Header
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.padding(bottom = 20.dp)
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
                text = "Nearby hosts",
                style = ChorusTheme.typography.titleLarge,
                color = ChorusTheme.colors.fog
            )
        }

        if (rejectionReason.isNotEmpty()) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(8.dp))
                    .background(ChorusTheme.colors.lost.copy(alpha = 0.2f))
                    .border(1.dp, ChorusTheme.colors.lost, RoundedCornerShape(8.dp))
                    .padding(12.dp)
            ) {
                Text(
                    text = "Connection rejected: $rejectionReason",
                    style = ChorusTheme.typography.bodyMedium,
                    color = ChorusTheme.colors.lost
                )
            }
            Spacer(modifier = Modifier.height(16.dp))
        }

        // Host list section
        Text(
            text = "Discovered on network",
            style = ChorusTheme.typography.bodyMedium,
            color = ChorusTheme.colors.mist
        )
        Spacer(modifier = Modifier.height(8.dp))

        // Merge hosts
        val allHosts = discoveredHosts.map { it.name to (it.ip to it.controlPort) } +
                nsdHosts.map { it.name to ((it.host?.hostAddress ?: "") to it.port) }

        if (allHosts.isEmpty()) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(120.dp)
                    .clip(RoundedCornerShape(12.dp))
                    .background(ChorusTheme.colors.deck)
                    .border(1.dp, ChorusTheme.colors.line, RoundedCornerShape(12.dp)),
                contentAlignment = Alignment.Center
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    CircularProgressIndicator(
                        color = ChorusTheme.colors.sonar,
                        modifier = Modifier.size(20.dp),
                        strokeWidth = 2.dp
                    )
                    Text(
                        text = "Searching for Chorus hosts...",
                        style = ChorusTheme.typography.bodyMedium,
                        color = ChorusTheme.colors.mist
                    )
                }
            }
        } else {
            LazyColumn(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1.0f),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(allHosts) { (name, address) ->
                    val (ip, port) = address
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(12.dp))
                            .background(ChorusTheme.colors.deck)
                            .border(1.dp, ChorusTheme.colors.line, RoundedCornerShape(12.dp))
                            .clickable {
                                onJoin(ip, port, pin)
                            }
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
                                    imageVector = Icons.Default.Cast,
                                    contentDescription = null,
                                    tint = ChorusTheme.colors.sonar
                                )
                                Column {
                                    Text(
                                        text = name,
                                        style = ChorusTheme.typography.bodyLarge,
                                        color = ChorusTheme.colors.fog
                                    )
                                    Text(
                                        text = "$ip:$port",
                                        style = ChorusTheme.typography.labelSmall,
                                        color = ChorusTheme.colors.mist
                                    )
                                }
                            }
                            Text(
                                text = "Join",
                                style = ChorusTheme.typography.bodyMedium,
                                color = ChorusTheme.colors.sonar
                            )
                        }
                    }
                }
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        // Manual Connection Form
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(12.dp))
                .background(ChorusTheme.colors.deck)
                .border(1.dp, ChorusTheme.colors.line, RoundedCornerShape(12.dp))
                .padding(16.dp)
        ) {
            Column {
                Text(
                    text = "Manual connection",
                    style = ChorusTheme.typography.bodyLarge,
                    color = ChorusTheme.colors.fog
                )
                Spacer(modifier = Modifier.height(12.dp))

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    OutlinedTextField(
                        value = hostIp,
                        onValueChange = { hostIp = it },
                        label = { Text("Host IP") },
                        modifier = Modifier.weight(2.0f),
                        colors = OutlinedTextFieldDefaults.colors(
                            focusedBorderColor = ChorusTheme.colors.sonar,
                            unfocusedBorderColor = ChorusTheme.colors.line,
                            focusedTextColor = ChorusTheme.colors.fog,
                            unfocusedTextColor = ChorusTheme.colors.fog,
                            focusedLabelColor = ChorusTheme.colors.sonar,
                            unfocusedLabelColor = ChorusTheme.colors.mist
                        ),
                        singleLine = true
                    )

                    OutlinedTextField(
                        value = portStr,
                        onValueChange = { portStr = it },
                        label = { Text("Port") },
                        modifier = Modifier.weight(1.0f),
                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        colors = OutlinedTextFieldDefaults.colors(
                            focusedBorderColor = ChorusTheme.colors.sonar,
                            unfocusedBorderColor = ChorusTheme.colors.line,
                            focusedTextColor = ChorusTheme.colors.fog,
                            unfocusedTextColor = ChorusTheme.colors.fog,
                            focusedLabelColor = ChorusTheme.colors.sonar,
                            unfocusedLabelColor = ChorusTheme.colors.mist
                        ),
                        singleLine = true
                    )
                }

                Spacer(modifier = Modifier.height(12.dp))

                OutlinedTextField(
                    value = pin,
                    onValueChange = {
                        if (it.length <= 4) {
                            pin = it
                            if (it.length == 4) {
                                val port = portStr.toIntOrNull() ?: 47800
                                onJoin(hostIp, port, pin)
                            }
                        }
                    },
                    label = { Text("4-digit PIN (if required)") },
                    modifier = Modifier.fillMaxWidth(),
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedBorderColor = ChorusTheme.colors.sonar,
                        unfocusedBorderColor = ChorusTheme.colors.line,
                        focusedTextColor = ChorusTheme.colors.fog,
                        unfocusedTextColor = ChorusTheme.colors.fog,
                        focusedLabelColor = ChorusTheme.colors.sonar,
                        unfocusedLabelColor = ChorusTheme.colors.mist
                    ),
                    singleLine = true
                )

                Spacer(modifier = Modifier.height(16.dp))

                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(48.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .background(ChorusTheme.colors.sonar)
                        .clickable {
                            val port = portStr.toIntOrNull() ?: 47800
                            onJoin(hostIp, port, pin)
                        },
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = "Connect",
                        style = ChorusTheme.typography.bodyLarge,
                        color = ChorusTheme.colors.harbor
                    )
                }
            }
        }
    }
}
