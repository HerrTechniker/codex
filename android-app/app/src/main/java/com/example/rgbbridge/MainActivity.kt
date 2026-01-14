package com.example.rgbbridge

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.DrawerValue
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ModalNavigationDrawer
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.rememberDrawerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import kotlinx.coroutines.launch
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

class MainActivity : ComponentActivity() {
  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    setContent { RgbBridgeApp() }
  }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RgbBridgeApp(viewModel: MainViewModel = viewModel()) {
  val state by viewModel.uiState.collectAsStateWithLifecycle()
  val drawerState = rememberDrawerState(DrawerValue.Closed)
  val scope = rememberCoroutineScope()
  var showAddDialog by remember { mutableStateOf(false) }
  var editingDevice by remember { mutableStateOf<Esp32Device?>(null) }

  if (showAddDialog) {
    DeviceDialog(
      title = "ESP32 hinzufügen",
      initial = Esp32Device(name = "ESP32", host = "192.168.4.1"),
      onDismiss = { showAddDialog = false },
      onSave = {
        viewModel.saveDevice(it)
        showAddDialog = false
      },
    )
  }

  editingDevice?.let { device ->
    DeviceDialog(
      title = "ESP32 bearbeiten",
      initial = device,
      onDismiss = { editingDevice = null },
      onSave = {
        viewModel.saveDevice(it)
        editingDevice = null
      },
    )
  }

  ModalNavigationDrawer(
    drawerState = drawerState,
    drawerContent = {
      DrawerContent(
        state = state.deviceState,
        onAddClick = { showAddDialog = true },
        onDeviceSelected = { viewModel.selectDevice(it) },
        onEditDevice = { editingDevice = it },
      )
    },
  ) {
    Scaffold(
      topBar = {
        TopAppBar(
          title = { Text("RGB Bridge") },
          navigationIcon = {
            IconButton(onClick = { scope.launch { drawerState.open() } }) {
              Icon(Icons.Default.Menu, contentDescription = "Menü")
            }
          },
          actions = {
            if (state.deviceState.devices.isEmpty()) {
              IconButton(onClick = { viewModel.startDiscovery() }) {
                Icon(Icons.Default.Add, contentDescription = "ESP32 suchen")
              }
            }
          },
        )
      },
    ) { padding ->
      ColorControlScreen(
        modifier = Modifier.padding(padding),
        state = state,
        onColorChange = { r, g, b -> viewModel.updateColor(r, g, b) },
        onLedIndexChange = { viewModel.updateLedIndex(it) },
      )
    }
  }
}

@Composable
fun DrawerContent(
  state: DeviceState,
  onAddClick: () -> Unit,
  onDeviceSelected: (String) -> Unit,
  onEditDevice: (Esp32Device) -> Unit,
) {
  Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
    Text("Geräte", modifier = Modifier.padding(bottom = 8.dp))
    state.devices.forEach { device ->
      Row(
        modifier = Modifier
          .fillMaxWidth()
          .clickable { onDeviceSelected(device.id) }
          .padding(vertical = 8.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
      ) {
        Column {
          Text(device.name)
          Text(device.host, color = Color.Gray)
        }
        IconButton(onClick = { onEditDevice(device) }) {
          Icon(Icons.Default.Edit, contentDescription = "Bearbeiten")
        }
      }
    }
    Spacer(modifier = Modifier.height(16.dp))
    Button(onClick = onAddClick, modifier = Modifier.fillMaxWidth()) {
      Icon(Icons.Default.Add, contentDescription = null)
      Spacer(modifier = Modifier.width(8.dp))
      Text("ESP32 hinzufügen")
    }
  }
}

@Composable
fun ColorControlScreen(
  modifier: Modifier = Modifier,
  state: ColorUiState,
  onColorChange: (Int, Int, Int) -> Unit,
  onLedIndexChange: (Int) -> Unit,
) {
  Column(
    modifier = modifier
      .fillMaxSize()
      .padding(16.dp),
    verticalArrangement = Arrangement.spacedBy(16.dp),
  ) {
    if (state.deviceState.devices.isEmpty()) {
      Text("Kein ESP32 gefunden. Öffne das Menü und füge einen hinzu.")
    }

    ColorWheel(
      modifier = Modifier.size(220.dp),
      red = state.red,
      green = state.green,
      blue = state.blue,
      onColorChange = onColorChange,
    )

    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
      RgbField("R", state.red) { onColorChange(it, state.green, state.blue) }
      RgbField("G", state.green) { onColorChange(state.red, it, state.blue) }
      RgbField("B", state.blue) { onColorChange(state.red, state.green, it) }
    }

    LedIndexField(state.ledIndex, onLedIndexChange)
  }
}

@Composable
fun RgbField(label: String, value: Int, onValueChange: (Int) -> Unit) {
  OutlinedTextField(
    value = value.toString(),
    onValueChange = { text ->
      val parsed = text.toIntOrNull()?.coerceIn(0, 255) ?: 0
      onValueChange(parsed)
    },
    label = { Text(label) },
    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
    modifier = Modifier.weight(1f),
  )
}

@Composable
fun LedIndexField(value: Int, onValueChange: (Int) -> Unit) {
  OutlinedTextField(
    value = value.toString(),
    onValueChange = { text ->
      val parsed = text.toIntOrNull()?.coerceAtLeast(1) ?: 1
      onValueChange(parsed)
    },
    label = { Text("LED") },
    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
    modifier = Modifier.fillMaxWidth(),
  )
}

@Composable
fun ColorWheel(
  modifier: Modifier,
  red: Int,
  green: Int,
  blue: Int,
  onColorChange: (Int, Int, Int) -> Unit,
) {
  val radius = remember { mutableStateOf(0f) }
  val center = remember { mutableStateOf(Offset.Zero) }
  val hueSaturation = remember(red, green, blue) {
    rgbToHsv(red, green, blue)
  }
  val hueState = remember { mutableStateOf(hueSaturation.first) }
  val saturationState = remember { mutableStateOf(hueSaturation.second) }

  Canvas(
    modifier = modifier
      .pointerInput(Unit) {
        detectDragGestures { change, _ ->
          val position = change.position
          val dx = position.x - center.value.x
          val dy = position.y - center.value.y
          val distance = sqrt(dx * dx + dy * dy)
          val limited = min(distance, radius.value)
          val sat = (limited / radius.value).coerceIn(0f, 1f)
          val angle = atan2(dy, dx)
          val hue = ((angle / (2 * Math.PI)) * 360 + 360).toFloat() % 360f
          hueState.value = hue
          saturationState.value = sat
          val rgb = hsvToRgb(hue, sat, 1f)
          onColorChange(rgb.first, rgb.second, rgb.third)
        }
      },
  ) {
    radius.value = size.minDimension / 2f
    center.value = Offset(size.width / 2f, size.height / 2f)

    val hueColors = listOf(
      Color.Red,
      Color.Yellow,
      Color.Green,
      Color.Cyan,
      Color.Blue,
      Color.Magenta,
      Color.Red,
    )
    drawCircle(
      brush = Brush.sweepGradient(hueColors, center.value),
      radius = radius.value,
    )
    drawCircle(
      brush = Brush.radialGradient(
        colors = listOf(Color.White, Color.Transparent),
        center = center.value,
        radius = radius.value,
      ),
      radius = radius.value,
    )

    val indicatorRadius = radius.value * saturationState.value
    val angleRad = Math.toRadians(hueState.value.toDouble())
    val indicator = Offset(
      x = center.value.x + indicatorRadius * cos(angleRad).toFloat(),
      y = center.value.y + indicatorRadius * sin(angleRad).toFloat(),
    )
    drawCircle(
      color = Color.Black,
      radius = 10f,
      center = indicator,
      style = Stroke(width = 3f),
    )
  }
}

@Composable
fun DeviceDialog(
  title: String,
  initial: Esp32Device,
  onDismiss: () -> Unit,
  onSave: (Esp32Device) -> Unit,
) {
  var name by remember { mutableStateOf(initial.name) }
  var host by remember { mutableStateOf(initial.host) }
  var port by remember { mutableStateOf(initial.port.toString()) }
  var topicBase by remember { mutableStateOf(initial.topicBase) }
  var user by remember { mutableStateOf(initial.mqttUser) }
  var password by remember { mutableStateOf(initial.mqttPassword) }

  AlertDialog(
    onDismissRequest = onDismiss,
    title = { Text(title) },
    text = {
      Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedTextField(value = name, onValueChange = { name = it }, label = { Text("Name") })
        OutlinedTextField(value = host, onValueChange = { host = it }, label = { Text("Broker/Host") })
        OutlinedTextField(
          value = port,
          onValueChange = { port = it },
          label = { Text("Port") },
          keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
        )
        OutlinedTextField(
          value = topicBase,
          onValueChange = { topicBase = it },
          label = { Text("Topic-Basis") },
        )
        OutlinedTextField(value = user, onValueChange = { user = it }, label = { Text("MQTT User") })
        OutlinedTextField(
          value = password,
          onValueChange = { password = it },
          label = { Text("MQTT Passwort") },
        )
      }
    },
    confirmButton = {
      Button(onClick = {
        val updated = initial.copy(
          name = name.ifBlank { "ESP32" },
          host = host.ifBlank { "192.168.4.1" },
          port = port.toIntOrNull() ?: 1883,
          topicBase = topicBase.ifBlank { "rgbled" },
          mqttUser = user,
          mqttPassword = password,
        )
        onSave(updated)
      }) {
        Text("Speichern")
      }
    },
    dismissButton = {
      Button(onClick = onDismiss) {
        Text("Abbrechen")
      }
    },
  )
}

fun hsvToRgb(hue: Float, saturation: Float, value: Float): Triple<Int, Int, Int> {
  val c = value * saturation
  val x = c * (1 - kotlin.math.abs((hue / 60f) % 2 - 1))
  val m = value - c
  val (r1, g1, b1) = when {
    hue < 60 -> Triple(c, x, 0f)
    hue < 120 -> Triple(x, c, 0f)
    hue < 180 -> Triple(0f, c, x)
    hue < 240 -> Triple(0f, x, c)
    hue < 300 -> Triple(x, 0f, c)
    else -> Triple(c, 0f, x)
  }
  val r = ((r1 + m) * 255).toInt().coerceIn(0, 255)
  val g = ((g1 + m) * 255).toInt().coerceIn(0, 255)
  val b = ((b1 + m) * 255).toInt().coerceIn(0, 255)
  return Triple(r, g, b)
}

fun rgbToHsv(red: Int, green: Int, blue: Int): Pair<Float, Float> {
  val r = red / 255f
  val g = green / 255f
  val b = blue / 255f
  val max = maxOf(r, g, b)
  val min = minOf(r, g, b)
  val delta = max - min
  val hue = when {
    delta == 0f -> 0f
    max == r -> (60 * (((g - b) / delta) % 6))
    max == g -> (60 * (((b - r) / delta) + 2))
    else -> (60 * (((r - g) / delta) + 4))
  }
  val saturation = if (max == 0f) 0f else delta / max
  return Pair((hue + 360) % 360, saturation)
}
