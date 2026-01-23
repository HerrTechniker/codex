plugins {
  id("com.android.application")
  id("org.jetbrains.kotlin.android")
  id("org.jetbrains.kotlin.plugin.serialization")
}

android {
  namespace = "com.example.rgbbridge"
  compileSdk = 34

  defaultConfig {
    applicationId = "com.example.rgbbridge"
    minSdk = 33
    targetSdk = 34
    versionCode = 1
    versionName = "1.0"
  }

  buildFeatures {
    compose = true
  }

  composeOptions {
    kotlinCompilerExtensionVersion = "1.5.8"
  }

  compileOptions {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
  }

  kotlinOptions {
    jvmTarget = "17"
  }
}

kotlin {
  jvmToolchain(17)
}

dependencies {
  implementation("androidx.core:core-ktx:1.12.0")
  implementation("androidx.activity:activity-compose:1.8.2")
  implementation("androidx.compose.foundation:foundation-layout:1.5.4")
  implementation("androidx.compose.ui:ui:1.5.4")
  implementation("androidx.compose.ui:ui-tooling-preview:1.5.4")
  implementation("androidx.compose.material3:material3:1.2.0")
  implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.7.0")
  implementation("androidx.lifecycle:lifecycle-runtime-compose:2.7.0")
  implementation("androidx.compose.material:material-icons-extended:1.5.4")
  implementation("androidx.datastore:datastore:1.0.0")
  implementation("androidx.datastore:datastore-preferences:1.0.0")
  implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.6.3")
  implementation("org.eclipse.paho:org.eclipse.paho.client.mqttv3:1.2.5")

  debugImplementation("androidx.compose.ui:ui-tooling:1.5.4")
}
