package com.example.rgbbridge

import org.eclipse.paho.client.mqttv3.MqttAsyncClient
import org.eclipse.paho.client.mqttv3.MqttConnectOptions
import org.eclipse.paho.client.mqttv3.MqttException
import org.eclipse.paho.client.mqttv3.MqttMessage
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence

class MqttPublisher {
  private var client: MqttAsyncClient? = null
  private var currentConfig: Esp32Device? = null

  @Synchronized
  private fun ensureClient(device: Esp32Device) {
    if (client != null && currentConfig == device && client?.isConnected == true) {
      return
    }
    try {
      client?.disconnect()
    } catch (_: MqttException) {
      // Ignore disconnect errors and recreate the client.
    }
    client = null
    currentConfig = device

    val serverUri = "tcp://${device.host}:${device.port}"
    client = MqttAsyncClient(serverUri, "android-${device.id}", MemoryPersistence())
    val options = MqttConnectOptions().apply {
      isAutomaticReconnect = true
      isCleanSession = true
      if (device.mqttUser.isNotBlank()) {
        userName = device.mqttUser
        password = device.mqttPassword.toCharArray()
      }
    }
    try {
      client?.connect(options)?.waitForCompletion(3000)
    } catch (ex: MqttException) {
      // Ignore; will retry on next publish.
    }
  }

  fun publish(device: Esp32Device, topic: String, payload: String) {
    ensureClient(device)
    val message = MqttMessage(payload.toByteArray()).apply {
      qos = 0
      isRetained = false
    }
    try {
      client?.publish(topic, message)
    } catch (_: MqttException) {
      // Ignore transient errors.
    }
  }
}
