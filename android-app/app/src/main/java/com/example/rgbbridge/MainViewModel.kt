package com.example.rgbbridge

import android.app.Application
import android.net.nsd.NsdServiceInfo
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

data class ColorUiState(
  val red: Int = 0,
  val green: Int = 0,
  val blue: Int = 0,
  val maxTargets: Int = 4,
  val allTargets: Boolean = true,
  val selectedTargets: Set<Int> = emptySet(),
  val effect: String = "static",
  val isDiscovering: Boolean = false,
  val deviceState: DeviceState = DeviceState(),
)

class MainViewModel(application: Application) : AndroidViewModel(application) {
  private val store = DeviceStore(application)
  private val discovery = NsdDiscovery(application)
  private val publisher = MqttPublisher()

  private val _uiState = MutableStateFlow(ColorUiState())
  val uiState: StateFlow<ColorUiState> = _uiState.asStateFlow()

  private var publishJob: Job? = null

  init {
    viewModelScope.launch {
      store.state.collectLatest { state ->
        _uiState.value = _uiState.value.copy(deviceState = state)
        if (state.devices.isEmpty()) {
          startDiscovery()
        }
      }
    }
  }

  fun startDiscovery() {
    if (_uiState.value.isDiscovering) return
    _uiState.value = _uiState.value.copy(isDiscovering = true)
    viewModelScope.launch {
      discovery.discoverEsp32().collectLatest { serviceInfo ->
        val device = serviceToDevice(serviceInfo)
        store.upsertDevice(device)
        _uiState.value = _uiState.value.copy(isDiscovering = false)
      }
    }
  }

  fun selectDevice(deviceId: String?) {
    viewModelScope.launch {
      store.selectDevice(deviceId)
    }
  }

  fun saveDevice(device: Esp32Device) {
    viewModelScope.launch {
      store.upsertDevice(device)
    }
  }

  fun updateColor(red: Int, green: Int, blue: Int) {
    _uiState.value = _uiState.value.copy(
      red = red,
      green = green,
      blue = blue,
      effect = "static",
    )
    schedulePublish()
  }

  fun updateMaxTargets(value: Int) {
    _uiState.value = _uiState.value.copy(maxTargets = value.coerceAtLeast(1))
    schedulePublish()
  }

  fun toggleAllTargets(enabled: Boolean) {
    _uiState.value = _uiState.value.copy(allTargets = enabled)
    schedulePublish()
  }

  fun toggleTarget(index: Int, enabled: Boolean) {
    val state = _uiState.value
    val current = if (state.allTargets) {
      (1..state.maxTargets).toMutableSet()
    } else {
      state.selectedTargets.toMutableSet()
    }
    if (enabled) {
      current.add(index)
    } else {
      current.remove(index)
    }
    _uiState.value = _uiState.value.copy(selectedTargets = current, allTargets = false)
    schedulePublish()
  }

  fun updateEffect(effect: String) {
    _uiState.value = _uiState.value.copy(effect = effect)
    publishEffect()
  }

  private fun schedulePublish() {
    publishJob?.cancel()
    publishJob = viewModelScope.launch {
      val state = _uiState.value
      val device = resolveSelectedDevice(state.deviceState)
      if (device != null) {
        val payload = "${state.red},${state.green},${state.blue}"
        if (state.effect == "static") {
          val targets = resolveTargets(state)
          targets.forEach { index ->
            val topic = "${device.topicBase}/$index"
            publisher.publish(device, topic, payload)
          }
        }
      }
    }
  }

  private fun publishEffect() {
    publishJob?.cancel()
    publishJob = viewModelScope.launch {
      val state = _uiState.value
      val device = resolveSelectedDevice(state.deviceState)
      if (device != null) {
        val targets = resolveTargets(state)
        val effectName = if (state.effect == "static") "none" else state.effect
        val targetPayload = if (targets.isEmpty()) {
          "effect=${effectName}"
        } else {
          "effect=${effectName};targets=${targets.joinToString(\",\")}"
        }
        publisher.publish(device, "${device.topicBase}/effect", targetPayload)
      }
    }
  }

  private fun resolveTargets(state: ColorUiState): List<Int> {
    return if (state.allTargets) {
      (1..state.maxTargets).toList()
    } else {
      state.selectedTargets.sorted()
    }
  }

  private fun resolveSelectedDevice(state: DeviceState): Esp32Device? {
    val selected = state.selectedId
    return if (selected == null) {
      state.devices.firstOrNull()
    } else {
      state.devices.firstOrNull { it.id == selected }
    }
  }

  private fun serviceToDevice(info: NsdServiceInfo): Esp32Device {
    val host = info.host.hostAddress ?: "192.168.4.1"
    return Esp32Device(
      name = "ESP32 ${host}",
      host = host,
      port = 1883,
      topicBase = "rgbled",
    )
  }
}
