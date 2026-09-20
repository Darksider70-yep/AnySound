package dev.chorus.app

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.net.Uri
import android.os.Bundle
import android.os.IBinder
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import dev.chorus.app.net.NsdDiscoveryManager
import dev.chorus.app.service.AppRole
import dev.chorus.app.service.AppSnapshot
import dev.chorus.app.service.ChorusPlaybackService
import dev.chorus.app.ui.screens.CalibrateScreen
import dev.chorus.app.ui.screens.ClientFindScreen
import dev.chorus.app.ui.screens.ClientListeningScreen
import dev.chorus.app.ui.screens.HomeScreen
import dev.chorus.app.ui.screens.HostScreen
import dev.chorus.app.ui.screens.SettingsScreen
import dev.chorus.app.ui.theme.ChorusAppTheme

class MainActivity : ComponentActivity() {

    private var playbackService: ChorusPlaybackService? = null
    private var isBound = false
    private val nsdManager by lazy { NsdDiscoveryManager(this) }

    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            val binder = service as ChorusPlaybackService.LocalBinder
            playbackService = binder.getService()
            isBound = true
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            playbackService = null
            isBound = false
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        // Bind playback foreground service
        val serviceIntent = Intent(this, ChorusPlaybackService::class.java)
        startService(serviceIntent)
        bindService(serviceIntent, serviceConnection, Context.BIND_AUTO_CREATE)

        setContent {
            ChorusAppTheme {
                val navController = rememberNavController()
                val snapshotState = playbackService?.snapshot?.collectAsState(initial = AppSnapshot())
                val snapshot = snapshotState?.value ?: AppSnapshot()
                val nsdHosts by nsdManager.discoveredHosts.collectAsState()

                DisposableEffect(Unit) {
                    nsdManager.startDiscovery()
                    onDispose {
                        nsdManager.stopDiscovery()
                    }
                }

                // Handle incoming deep link intent: chorus://<ip>:<port>?pin=<pin>
                LaunchedEffect(intent) {
                    handleIntent(intent)
                }

                // Handle automatic navigation upon active session
                LaunchedEffect(snapshot.role, snapshot.isActive) {
                    if (snapshot.role == AppRole.Client && snapshot.isActive) {
                        if (navController.currentDestination?.route != "listening") {
                            navController.navigate("listening") {
                                popUpTo("home")
                            }
                        }
                    } else if (snapshot.role == AppRole.Hosting && snapshot.isActive) {
                        if (navController.currentDestination?.route != "host") {
                            navController.navigate("host") {
                                popUpTo("home")
                            }
                        }
                    }
                }

                NavHost(navController = navController, startDestination = "home") {
                    composable("home") {
                        HomeScreen(
                            onNavigateToFind = { navController.navigate("find") },
                            onNavigateToHost = {
                                playbackService?.startHost("", 300, true)
                                navController.navigate("host")
                            },
                            onNavigateToSettings = { navController.navigate("settings") }
                        )
                    }

                    composable("find") {
                        ClientFindScreen(
                            discoveredHosts = snapshot.discoveredHosts,
                            nsdHosts = nsdHosts,
                            rejectionReason = snapshot.rejectionReason,
                            onBack = { navController.popBackStack() },
                            onJoin = { hostIp, port, pin ->
                                playbackService?.joinHost(hostIp, port, pin)
                            }
                        )
                    }

                    composable("listening") {
                        ClientListeningScreen(
                            snapshot = snapshot,
                            onVolumeChange = { playbackService?.setVolume(it) },
                            onMuteToggle = { playbackService?.setMute(!snapshot.isMuted) },
                            onNavigateToCalibrate = { navController.navigate("calibrate") },
                            onDisconnect = {
                                playbackService?.leaveSession()
                                navController.navigate("home") {
                                    popUpTo("home") { inclusive = true }
                                }
                            }
                        )
                    }

                    composable("host") {
                        HostScreen(
                            snapshot = snapshot,
                            onBack = { navController.popBackStack() },
                            onStopHosting = {
                                playbackService?.leaveSession()
                                navController.navigate("home") {
                                    popUpTo("home") { inclusive = true }
                                }
                            }
                        )
                    }

                    composable("calibrate") {
                        CalibrateScreen(
                            offsetMs = snapshot.offsetMs,
                            onOffsetChange = { playbackService?.setOffsetMs(it) },
                            onBack = { navController.popBackStack() }
                        )
                    }

                    composable("settings") {
                        SettingsScreen(
                            onBack = { navController.popBackStack() }
                        )
                    }
                }
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleIntent(intent)
    }

    private fun handleIntent(intent: Intent?) {
        val uri: Uri? = intent?.data
        if (uri != null && uri.scheme == "chorus") {
            val host = uri.host ?: return
            val port = if (uri.port != -1) uri.port else 47800
            val pin = uri.getQueryParameter("pin") ?: ""
            playbackService?.joinHost(host, port, pin)
        }
    }

    override fun onDestroy() {
        if (isBound) {
            unbindService(serviceConnection)
            isBound = false
        }
        super.onDestroy()
    }
}
