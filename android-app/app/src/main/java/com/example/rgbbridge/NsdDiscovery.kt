package com.example.rgbbridge

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow

class NsdDiscovery(private val context: Context) {
  private val nsdManager = context.getSystemService(Context.NSD_SERVICE) as NsdManager

  fun discoverEsp32(): Flow<NsdServiceInfo> = callbackFlow {
    val discoveryListener = object : NsdManager.DiscoveryListener {
      override fun onDiscoveryStarted(regType: String) = Unit
      override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
        close()
      }
      override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) = Unit
      override fun onDiscoveryStopped(serviceType: String) = Unit

      override fun onServiceFound(serviceInfo: NsdServiceInfo) {
        if (serviceInfo.serviceName.contains("esp32-rgb-bridge", ignoreCase = true)) {
          nsdManager.resolveService(serviceInfo, object : NsdManager.ResolveListener {
            override fun onResolveFailed(serviceInfo: NsdServiceInfo, errorCode: Int) = Unit
            override fun onServiceResolved(resolvedServiceInfo: NsdServiceInfo) {
              trySend(resolvedServiceInfo)
            }
          })
        }
      }

      override fun onServiceLost(serviceInfo: NsdServiceInfo) = Unit
    }

    nsdManager.discoverServices("_http._tcp.", NsdManager.PROTOCOL_DNS_SD, discoveryListener)
    awaitClose { nsdManager.stopServiceDiscovery(discoveryListener) }
  }
}
