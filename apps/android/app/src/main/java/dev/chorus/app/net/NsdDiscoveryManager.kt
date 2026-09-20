package dev.chorus.app.net

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.util.Log
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.net.InetAddress

data class NsdDiscoveredHost(
    val name: String,
    val host: InetAddress?,
    val port: Int
)

class NsdDiscoveryManager(context: Context) {
    private val nsdManager = context.getSystemService(Context.NSD_SERVICE) as NsdManager
    private val _discoveredHosts = MutableStateFlow<List<NsdDiscoveredHost>>(emptyList())
    val discoveredHosts: StateFlow<List<NsdDiscoveredHost>> = _discoveredHosts.asStateFlow()

    private var discoveryListener: NsdManager.DiscoveryListener? = null
    private var isScanning = false

    private val activeHostsMap = mutableMapOf<String, NsdDiscoveredHost>()

    fun startDiscovery() {
        if (isScanning) return

        discoveryListener = object : NsdManager.DiscoveryListener {
            override fun onDiscoveryStarted(regType: String) {
                isScanning = true
                Log.d(TAG, "NSD discovery started for $regType")
            }

            override fun onServiceFound(service: NsdServiceInfo) {
                Log.d(TAG, "NSD service found: ${service.serviceName} type: ${service.serviceType}")
                if (service.serviceType.contains(SERVICE_TYPE)) {
                    nsdManager.resolveService(service, object : NsdManager.ResolveListener {
                        override fun onResolveFailed(serviceInfo: NsdServiceInfo, errorCode: Int) {
                            Log.e(TAG, "NSD resolve failed for ${serviceInfo.serviceName}: $errorCode")
                        }

                        override fun onServiceResolved(serviceInfo: NsdServiceInfo) {
                            Log.d(TAG, "NSD resolved: ${serviceInfo.serviceName} -> ${serviceInfo.host}:${serviceInfo.port}")
                            val item = NsdDiscoveredHost(
                                name = serviceInfo.serviceName,
                                host = serviceInfo.host,
                                port = serviceInfo.port
                            )
                            synchronized(activeHostsMap) {
                                activeHostsMap[serviceInfo.serviceName] = item
                                _discoveredHosts.value = activeHostsMap.values.toList()
                            }
                        }
                    })
                }
            }

            override fun onServiceLost(service: NsdServiceInfo) {
                Log.d(TAG, "NSD service lost: ${service.serviceName}")
                synchronized(activeHostsMap) {
                    activeHostsMap.remove(service.serviceName)
                    _discoveredHosts.value = activeHostsMap.values.toList()
                }
            }

            override fun onDiscoveryStopped(serviceType: String) {
                isScanning = false
                Log.d(TAG, "NSD discovery stopped")
            }

            override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
                Log.e(TAG, "NSD start discovery failed: $errorCode")
                isScanning = false
            }

            override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {
                Log.e(TAG, "NSD stop discovery failed: $errorCode")
                isScanning = false
            }
        }

        try {
            nsdManager.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, discoveryListener)
        } catch (e: Exception) {
            Log.e(TAG, "Exception starting NSD discovery", e)
        }
    }

    fun stopDiscovery() {
        if (!isScanning) return
        discoveryListener?.let {
            try {
                nsdManager.stopServiceDiscovery(it)
            } catch (e: Exception) {
                Log.e(TAG, "Exception stopping NSD discovery", e)
            }
        }
        discoveryListener = null
        isScanning = false
        synchronized(activeHostsMap) {
            activeHostsMap.clear()
            _discoveredHosts.value = emptyList()
        }
    }

    companion object {
        private const val TAG = "NsdDiscoveryManager"
        private const val SERVICE_TYPE = "_chorus._tcp."
    }
}
