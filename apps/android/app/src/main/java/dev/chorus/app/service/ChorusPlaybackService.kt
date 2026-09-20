package dev.chorus.app.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.net.wifi.WifiManager
import android.os.Binder
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import androidx.core.app.NotificationCompat
import dev.chorus.app.MainActivity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

class ChorusPlaybackService : Service() {

    private val binder = LocalBinder()
    private val serviceScope = CoroutineScope(Dispatchers.Default + Job())
    private var updateJob: Job? = null

    private var wakeLock: PowerManager.WakeLock? = null
    private var wifiLock: WifiManager.WifiLock? = null

    private val bridge = NativeChorusBridge()
    private val _snapshot = MutableStateFlow(AppSnapshot())
    val snapshot: StateFlow<AppSnapshot> = _snapshot.asStateFlow()

    inner class LocalBinder : Binder() {
        fun getService(): ChorusPlaybackService = this@ChorusPlaybackService
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        acquireLocks()
        startUpdateLoop()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_LEAVE -> {
                leaveSession()
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
            }
            else -> {
                startForeground(NOTIFICATION_ID, buildNotification("Ready", "Connecting to session..."))
            }
        }
        return START_NOT_STICKY
    }

    fun joinHost(hostIp: String, port: Int = 47800, pin: String = ""): Boolean {
        val success = bridge.joinHost(hostIp, port, pin)
        if (success) {
            startForeground(NOTIFICATION_ID, buildNotification("Chorus Active", "Listening to $hostIp"))
        }
        return success
    }

    fun startHost(pin: String = "", targetLatencyMs: Long = 300, useTestTone: Boolean = false): Boolean {
        val success = bridge.startHost(pin, targetLatencyMs, useTestTone)
        if (success) {
            startForeground(NOTIFICATION_ID, buildNotification("Chorus Host", "Broadcasting audio"))
        }
        return success
    }

    fun leaveSession() {
        bridge.leaveHost()
        bridge.stopHost()
        stopForeground(STOP_FOREGROUND_REMOVE)
    }

    fun setVolume(volume: Float) {
        bridge.setVolume(volume)
    }

    fun setMute(mute: Boolean) {
        bridge.setMute(mute)
    }

    fun setOffsetMs(offsetMs: Int) {
        bridge.setOffsetMs(offsetMs)
    }

    private fun startUpdateLoop() {
        updateJob = serviceScope.launch {
            var tickCount = 0
            while (isActive) {
                bridge.update()
                val snap = bridge.getSnapshot()
                _snapshot.value = snap

                // Update notification text every second (50 ticks)
                tickCount++
                if (tickCount >= 50 && snap.isActive) {
                    tickCount = 0
                    val title = if (snap.role == AppRole.Hosting) "Chorus Host" else "Chorus Listening"
                    val content = if (snap.role == AppRole.Client) {
                        val error = snap.stats.syncErrorMs
                        val syncState = if (Math.abs(error) <= 5.0f) "In sync" else if (Math.abs(error) <= 20.0f) "Drifting" else "Out of sync"
                        "$syncState (\u00B1${String.format("%.1f", Math.abs(error))} ms)"
                    } else {
                        "${snap.connectedClients.size} devices connected"
                    }
                    val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
                    notificationManager.notify(NOTIFICATION_ID, buildNotification(title, content))
                }

                delay(20) // 50 Hz tick
            }
        }
    }

    private fun acquireLocks() {
        val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "Chorus:AudioPlaybackWakeLock").apply {
            setReferenceCounted(false)
            acquire(24 * 60 * 60 * 1000L) // 24 hours max
        }

        val wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        val wifiLockMode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            WifiManager.WIFI_MODE_FULL_LOW_LATENCY
        } else {
            WifiManager.WIFI_MODE_FULL_HIGH_PERF
        }
        wifiLock = wifiManager.createWifiLock(wifiLockMode, "Chorus:LowLatencyWifiLock").apply {
            setReferenceCounted(false)
            acquire()
        }
    }

    private fun releaseLocks() {
        wakeLock?.let {
            if (it.isHeld) it.release()
        }
        wakeLock = null

        wifiLock?.let {
            if (it.isHeld) it.release()
        }
        wifiLock = null
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "Chorus Audio Playback",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Shows active audio synchronization status"
                setShowBadge(false)
            }
            val manager = getSystemService(NotificationManager::class.java)
            manager?.createNotificationChannel(channel)
        }
    }

    private fun buildNotification(title: String, content: String): Notification {
        val launchIntent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_CLEAR_TOP
        }
        val pendingIntent = PendingIntent.getActivity(
            this, 0, launchIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val leaveIntent = Intent(this, ChorusPlaybackService::class.java).apply {
            action = ACTION_LEAVE
        }
        val leavePendingIntent = PendingIntent.getService(
            this, 1, leaveIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(title)
            .setContentText(content)
            .setSmallIcon(android.R.drawable.ic_lock_silent_mode_off)
            .setContentIntent(pendingIntent)
            .addAction(android.R.drawable.ic_menu_close_clear_cancel, "Leave", leavePendingIntent)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()
    }

    override fun onDestroy() {
        updateJob?.cancel()
        bridge.close()
        releaseLocks()
        super.onDestroy()
    }

    companion object {
        const val CHANNEL_ID = "chorus_playback_channel"
        const val NOTIFICATION_ID = 1001
        const val ACTION_LEAVE = "dev.chorus.app.ACTION_LEAVE"
    }
}
