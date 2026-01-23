package com.example.rgbbridge

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import kotlinx.serialization.Serializable
import kotlinx.serialization.encodeToString
import kotlinx.serialization.decodeFromString
import kotlinx.serialization.json.Json
import java.util.UUID

private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "rgb_bridge")

private val DEVICES_KEY = stringPreferencesKey("devices_json")
private val SELECTED_KEY = stringPreferencesKey("selected_device")

@Serializable
data class Esp32Device(
  val id: String = UUID.randomUUID().toString(),
  val name: String,
  val host: String,
  val port: Int = 1883,
  val topicBase: String = "rgbled",
  val mqttUser: String = "",
  val mqttPassword: String = "",
)

@Serializable
data class DeviceState(
  val devices: List<Esp32Device> = emptyList(),
  val selectedId: String? = null,
)

class DeviceStore(private val context: Context) {
  private val json = Json { ignoreUnknownKeys = true }

  val state: Flow<DeviceState> = context.dataStore.data.map { prefs ->
    val payload = prefs[DEVICES_KEY]
    val devices = if (payload.isNullOrBlank()) {
      emptyList()
    } else {
      runCatching { json.decodeFromString<List<Esp32Device>>(payload) }
        .getOrDefault(emptyList())
    }
    val selected = prefs[SELECTED_KEY]
    DeviceState(devices, selected)
  }

  suspend fun saveState(state: DeviceState) {
    context.dataStore.edit { prefs ->
      prefs[DEVICES_KEY] = json.encodeToString(state.devices)
      state.selectedId?.let { prefs[SELECTED_KEY] = it }
    }
  }

  suspend fun selectDevice(deviceId: String?) {
    context.dataStore.edit { prefs ->
      if (deviceId == null) {
        prefs.remove(SELECTED_KEY)
      } else {
        prefs[SELECTED_KEY] = deviceId
      }
    }
  }

  suspend fun upsertDevice(device: Esp32Device) {
    context.dataStore.edit { prefs ->
      val payload = prefs[DEVICES_KEY]
      val devices = if (payload.isNullOrBlank()) {
        emptyList()
      } else {
        runCatching { json.decodeFromString<List<Esp32Device>>(payload) }
          .getOrDefault(emptyList())
      }
      val updated = devices.filterNot { it.id == device.id } + device
      prefs[DEVICES_KEY] = json.encodeToString(updated)
      prefs[SELECTED_KEY] = device.id
    }
  }
}
