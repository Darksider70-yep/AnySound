package dev.chorus.app.service

import org.json.JSONArray
import org.json.JSONObject
import java.io.Closeable

enum class AppRole {
    Idle,
    Hosting,
    Client
}

data class ClientStats(
    val syncErrorUs: Long = 0,
    val skewPpm: Double = 0.0,
    val underruns: Long = 0,
    val lateFrames: Long = 0,
    val lossPct: Float = 0.0f,
    val bufferMs: Float = 0.0f
) {
    val syncErrorMs: Float
        get() = syncErrorUs / 1000.0f
}

data class DiscoveredHost(
    val name: String,
    val ip: String,
    val controlPort: Int,
    val audioPort: Int
)

data class ConnectedClient(
    val clientId: Long,
    val name: String,
    val endpoint: String,
    val volume: Float,
    val isMuted: Boolean,
    val offsetMs: Int,
    val syncErrorUs: Long
)

data class AppSnapshot(
    val role: AppRole = AppRole.Idle,
    val isActive: Boolean = false,
    val sessionPin: String = "",
    val rejectionReason: String = "",
    val volume: Float = 1.0f,
    val isMuted: Boolean = false,
    val offsetMs: Int = 0,
    val stats: ClientStats = ClientStats(),
    val connectedClients: List<ConnectedClient> = emptyList(),
    val discoveredHosts: List<DiscoveredHost> = emptyList()
)

class NativeChorusBridge : Closeable {
    private var nativeHandle: Long = 0

    init {
        System.loadLibrary("chorus_jni")
        nativeHandle = nativeInit()
    }

    fun startHost(pin: String = "", targetLatencyMs: Long = 300, useTestTone: Boolean = false): Boolean {
        return if (nativeHandle != 0L) {
            nativeStartHost(nativeHandle, pin, targetLatencyMs, useTestTone)
        } else false
    }

    fun stopHost() {
        if (nativeHandle != 0L) {
            nativeStopHost(nativeHandle)
        }
    }

    fun joinHost(hostIp: String, port: Int = 47800, pin: String = ""): Boolean {
        return if (nativeHandle != 0L) {
            nativeJoinHost(nativeHandle, hostIp, port, pin)
        } else false
    }

    fun leaveHost() {
        if (nativeHandle != 0L) {
            nativeLeaveHost(nativeHandle)
        }
    }

    fun update() {
        if (nativeHandle != 0L) {
            nativeUpdate(nativeHandle)
        }
    }

    fun setVolume(volume: Float) {
        if (nativeHandle != 0L) {
            nativeSetVolume(nativeHandle, volume)
        }
    }

    fun setMute(mute: Boolean) {
        if (nativeHandle != 0L) {
            nativeSetMute(nativeHandle, mute)
        }
    }

    fun setOffsetMs(offsetMs: Int) {
        if (nativeHandle != 0L) {
            nativeSetOffsetMs(nativeHandle, offsetMs)
        }
    }

    fun getSnapshot(): AppSnapshot {
        if (nativeHandle == 0L) return AppSnapshot()
        val jsonStr = nativeGetSnapshotJson(nativeHandle) ?: return AppSnapshot()
        return try {
            val json = JSONObject(jsonStr)
            val roleStr = json.optString("role", "idle")
            val role = when (roleStr) {
                "hosting" -> AppRole.Hosting
                "client" -> AppRole.Client
                else -> AppRole.Idle
            }

            val statsObj = json.optJSONObject("stats")
            val stats = if (statsObj != null) {
                ClientStats(
                    syncErrorUs = statsObj.optLong("sync_error_us", 0),
                    skewPpm = statsObj.optDouble("skew_ppm", 0.0),
                    underruns = statsObj.optLong("underruns", 0),
                    lateFrames = statsObj.optLong("late_frames", 0),
                    lossPct = statsObj.optDouble("loss_pct", 0.0).toFloat(),
                    bufferMs = statsObj.optDouble("buffer_ms", 0.0).toFloat()
                )
            } else ClientStats()

            val hostsArr = json.optJSONArray("discovered_hosts")
            val hosts = mutableListOf<DiscoveredHost>()
            if (hostsArr != null) {
                for (i in 0 until hostsArr.length()) {
                    val h = hostsArr.getJSONObject(i)
                    hosts.add(
                        DiscoveredHost(
                            name = h.optString("name", "Host"),
                            ip = h.optString("ip", ""),
                            controlPort = h.optInt("control_port", 47800),
                            audioPort = h.optInt("audio_port", 47801)
                        )
                    )
                }
            }

            val clientsArr = json.optJSONArray("connected_clients")
            val clients = mutableListOf<ConnectedClient>()
            if (clientsArr != null) {
                for (i in 0 until clientsArr.length()) {
                    val c = clientsArr.getJSONObject(i)
                    clients.add(
                        ConnectedClient(
                            clientId = c.optLong("client_id", 0),
                            name = c.optString("name", ""),
                            endpoint = c.optString("endpoint", ""),
                            volume = c.optDouble("volume", 1.0).toFloat(),
                            isMuted = c.optBoolean("is_muted", false),
                            offsetMs = c.optInt("offset_ms", 0),
                            syncErrorUs = c.optLong("sync_error_us", 0)
                        )
                    )
                }
            }

            AppSnapshot(
                role = role,
                isActive = json.optBoolean("is_active", false),
                sessionPin = json.optString("session_pin", ""),
                rejectionReason = json.optString("rejection_reason", ""),
                volume = json.optDouble("volume", 1.0).toFloat(),
                isMuted = json.optBoolean("is_muted", false),
                offsetMs = json.optInt("offset_ms", 0),
                stats = stats,
                connectedClients = clients,
                discoveredHosts = hosts
            )
        } catch (e: Exception) {
            AppSnapshot()
        }
    }

    override fun close() {
        if (nativeHandle != 0L) {
            nativeCleanup(nativeHandle)
            nativeHandle = 0
        }
    }

    private external fun nativeInit(): Long
    private external fun nativeCleanup(handle: Long)
    private external fun nativeStartHost(handle: Long, pin: String, targetLatencyMs: Long, useTestTone: Boolean): Boolean
    private external fun nativeStopHost(handle: Long)
    private external fun nativeJoinHost(handle: Long, hostIp: String, port: Int, pin: String): Boolean
    private external fun nativeLeaveHost(handle: Long)
    private external fun nativeUpdate(handle: Long)
    private external fun nativeSetVolume(handle: Long, volume: Float)
    private external fun nativeSetMute(handle: Long, mute: Boolean)
    private external fun nativeSetOffsetMs(handle: Long, offsetMs: Int)
    private external fun nativeGetSnapshotJson(handle: Long): String?
}
