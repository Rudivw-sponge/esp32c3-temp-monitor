#include <SPI.h>
#include <TFT_eSPI.h>
#include <Preferences.h>

#define TFT_BACKLIGHT 4 

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

Preferences prefs;

int cpuVal = 0;
int gpuVal = -1; // -1 means "not provided"
bool showCPU = true;

unsigned long lastSwitch = 0;
unsigned long switchInterval = 3000; // default 3s

// Interval update message
bool showIntervalMsg = false;
unsigned long intervalMsgUntil = 0;

// Connection message when python app is connected
bool showConnectMsg = false;
unsigned long connectMsgUntil = 0;

// Waiting state tracking and screen sleep
unsigned long waitingStart = 0;
bool screenSleeping = false;

// Format interval to display it properly
String formatInterval(unsigned long ms) {
  if (ms % 60000 == 0) {
    return String(ms / 60000) + " min";
  } else if (ms % 1000 == 0) {
    return String(ms / 1000) + " s";
  } else {
    return String(ms) + " ms";
  }
}

void setup() {
  tft.init();
  Serial.begin(115200);

  // Load saved interval
  prefs.begin("settings", false); // namespace "settings"
  switchInterval = prefs.getUInt("interval", 3000); // default 3000 if not set

  spr.createSprite(tft.width(), tft.height());
  spr.setTextDatum(MC_DATUM);
  spr.setFreeFont(&FreeSansBold12pt7b);

  // Show connection message on startup if serial is connected
  if (Serial) {
    showConnectMsg = true;
    connectMsgUntil = millis() + 3000;
    Serial.println("esp_connected");
  }
}

void loop() {
  static bool wasConnected = false;
  static bool dataReceived = false;

  // Detect serial connection transition
  if (Serial && !wasConnected) {
    wasConnected = true;
    showConnectMsg = true;
    connectMsgUntil = millis() + 3000;
    Serial.println("esp_connected");

    // Wake screen if sleeping
    if (screenSleeping) {
      tft.writecommand(0x11); // Wake up screen
      delay(120);
      screenSleeping = false;
    }
  }
  if (!Serial && wasConnected) {
    wasConnected = false;
  }

  // Read serial message
  if (Serial.available() > 0) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();

    // Wake screen if sleeping
    if (screenSleeping) {
      tft.writecommand(0x11); // Wake up screen
      delay(120);
      screenSleeping = false;
    }

    // Setting interval parsing
    if (msg.startsWith("interval=") || msg.toInt() > 999) {
      unsigned long newInterval;
      if (msg.startsWith("interval=")) {
        newInterval = msg.substring(9).toInt();
      } else {
        newInterval = msg.toInt();
      }

      if (newInterval >= 1000 && newInterval <= 600000) { // allow up to 10 min
        switchInterval = newInterval;
        prefs.putUInt("interval", switchInterval);

        // Trigger 3s message
        showIntervalMsg = true;
        intervalMsgUntil = millis() + 3000;

        Serial.println("Interval updated to " + formatInterval(switchInterval));
      }
    }
    else {
      // Temperature parsing
      int spaceIndex = msg.indexOf(' ');
      if (spaceIndex == -1) {
        cpuVal = msg.toInt();
        gpuVal = -1;
      } else {
        cpuVal = msg.substring(0, spaceIndex).toInt();
        gpuVal = msg.substring(spaceIndex + 1).toInt();
      }
      // mark that we have valid data
      dataReceived = true;
    }
  }

  // Show connection message when connected to python app
  if (showConnectMsg) {
    spr.fillSprite(TFT_BLACK);
    spr.setTextColor(TFT_GREEN, TFT_BLACK);
    spr.drawString("Connected", tft.width()/2, tft.height()/2, 4);
    spr.pushSprite(0, 0);

    if (millis() > connectMsgUntil) {
      showConnectMsg = false;
    }
    return;
  }

  // Show interval update message when interval is updated
  if (showIntervalMsg) {
    spr.fillSprite(TFT_BLACK);
    spr.setTextColor(TFT_YELLOW, TFT_BLACK);
    spr.drawString("Interval set to", tft.width()/2, tft.height()/3, 4);
    spr.drawString(formatInterval(switchInterval), tft.width()/2, (tft.height()*2)/3, 4);
    spr.pushSprite(0, 0);

    if (millis() > intervalMsgUntil) {
      showIntervalMsg = false;
    }
    return;
  }

  // WAITING STATE
  if (!dataReceived) {
    if (waitingStart == 0) waitingStart = millis();
    unsigned long now = millis();

    // If waiting > 60s, turn off screen
    if (!screenSleeping && (now - waitingStart > 60000)) {
      // Display Sleep In
      tft.writecommand(0x10);
      screenSleeping = true;
    }
    // Draw waiting text with blue gauge
    if (!screenSleeping) {
      float phase = (now % 2000) / 2000.0 * TWO_PI;
      int brightness = (sin(phase) * 0.5 + 0.5) * 255;

      spr.fillSprite(TFT_BLACK);
      uint16_t baseColor = tft.color565(0, 0, 255);
      uint16_t ringColor = adjustBrightness(baseColor, brightness);

      int outerRadius = min(tft.width(), tft.height()) / 2 - 2;
      int thickness   = 15;
      int innerRadius = outerRadius - thickness;

      spr.fillCircle(tft.width()/2, tft.height()/2, outerRadius, ringColor);
      spr.fillCircle(tft.width()/2, tft.height()/2, innerRadius, TFT_BLACK);

      spr.setTextDatum(MC_DATUM);
      spr.setTextColor(TFT_WHITE, TFT_BLACK);
      spr.drawString("Waiting...", tft.width()/2, tft.height()/2, 4);

      spr.pushSprite(0, 0);
    }
    return;
  } else {
    // reset when data arrives
    waitingStart = 0;
  }

  // Normal CPU/GPU gauge
  if (gpuVal != -1) {
    unsigned long now = millis();
    if (now - lastSwitch > switchInterval) {
      showCPU = !showCPU;
      lastSwitch = now;
    }
  } else {
    showCPU = true;
  }

  int currentVal = showCPU ? cpuVal : gpuVal;

  // Gauge Pulse timing
  unsigned long now = millis();
  int pulseSpeed = 2000;
  // Adjust pulse speed based on temperature
  if (currentVal > 75) {
    pulseSpeed = map(currentVal, 75, 100, 2000, 600);
    pulseSpeed = constrain(pulseSpeed, 600, 2000);
  }
  // Calculate pulse brightness
  float phase = (now % pulseSpeed) / (float)pulseSpeed * TWO_PI;
  int rawBrightness = (sin(phase) * 0.5 + 0.5) * 255;
  // Adjust brightness based on temperature
  int minBrightness = 80;
  // Scale brightness between 20-75C
  int brightness = rawBrightness;
  if (currentVal >= 20 && currentVal <= 75) {
    brightness = map(rawBrightness, 0, 255, minBrightness, 255);
  }

  // Draw gauge
  spr.fillSprite(TFT_BLACK);
  // Get base color from gradient
  uint16_t baseColor = gradientColor(currentVal);
  uint16_t ringColor = adjustBrightness(baseColor, brightness);
  // Glow effect parameters
  int outerRadius = min(tft.width(), tft.height()) / 2 - 2;
  int thickness   = 15;
  int innerRadius = outerRadius - thickness;

  int glowLayers = 6;
  int glowStep   = 4;
  // Draw glow layers
  for (int i = glowLayers; i >= 1; i--) {
    int radius = outerRadius + i * glowStep;
    int fade = map(i, 1, glowLayers, brightness, 20);
    uint16_t glowColor = adjustBrightness(baseColor, fade);
   
    spr.fillCircle(tft.width()/2, tft.height()/2, radius, glowColor);
    spr.fillCircle(tft.width()/2, tft.height()/2, outerRadius, TFT_BLACK);
  }

  spr.fillCircle(tft.width()/2, tft.height()/2, outerRadius, ringColor);
  spr.fillCircle(tft.width()/2, tft.height()/2, innerRadius, TFT_BLACK);

  // Label CPU/GPU
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(showCPU ? "CPU" : "GPU", tft.width()/2, tft.height()/3, 4);

  // Draw temperature value
  String tempStr = String(currentVal) + "  C";
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(tempStr, tft.width()/2, (tft.height()*2)/3, 4);
  // Draw degree symbol
  int textWidth = spr.textWidth(tempStr, 4);
  int baseX = tft.width()/2 - textWidth/2;
  int degX  = baseX + spr.textWidth(String(currentVal) + " ", 4);
  int degY  = (tft.height()*2)/3 - 10;
  spr.drawCircle(degX, degY, 3, TFT_WHITE);

  spr.pushSprite(0, 0);
}

// Gradient function that maps 0-100 to green-yellow-red
uint16_t gradientColor(int value) {
  value = constrain(value, 0, 100);
  uint8_t r, g, b;
  // Green to Red gradient
  if (value <= 20) {
    r = 0; g = 255; b = 0;
  // Red
  } else if (value >= 75) {
    r = 255; g = 0; b = 0;
  // Green to Yellow
  } else if (value <= 47) {
    int ratio = map(value, 20, 47, 0, 255);
    r = ratio; g = 255; b = 0;
  }
  else // Yellow to Red
  }
    int ratio = map(value, 47, 75, 0, 255);
    r = 255; g = 255 - ratio; b = 0;
  }
  return tft.color565(r, g, b);
}

// Brightness adjustment
uint16_t adjustBrightness(uint16_t color, int brightness) {
  // Clamp brightness to 0–255
  if (brightness < 0) brightness = 0;
  if (brightness > 255) brightness = 255;

  // Extract RGB565 channels to 8-bit
  uint8_t r = ((color >> 11) & 0x1F) << 3; // 5-bit -> 8-bit
  uint8_t g = ((color >> 5)  & 0x3F) << 2; // 6-bit -> 8-bit
  uint8_t b = ( color        & 0x1F) << 3; // 5-bit -> 8-bit

  // Linear scale by brightness (0–255)
  r = (uint16_t)r * brightness / 255;
  g = (uint16_t)g * brightness / 255;
  b = (uint16_t)b * brightness / 255;

  // Back to RGB565
  return tft.color565(r, g, b);
}

