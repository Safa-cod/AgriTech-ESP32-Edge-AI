#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// --- Include Edge Impulse AI Library ---
#include <mohammadfahim-project-1_inferencing.h> 

// --- Hardware Pin Definitions ---
#define TFT_CS      5
#define TFT_RST     4
#define TFT_DC      2
#define TFT_MOSI    23
#define TFT_SCLK    18

#define VRX_PIN     34  // Joystick X Axis
#define VRY_PIN     39  // Joystick Y Axis
#define SW_PIN      27  // Joystick Button

#define PUMP_PIN    16  // Relay 1
#define FAN_PIN     17  // Relay 2

#define DHT_PIN     32  
#define DHTTYPE     DHT22
#define SOIL_PIN    33  
#define LDR_DO_PIN  35  // Digital LDR Module

#define LED_PIN     19  // WS2812B Data Pin
#define BUZZER_PIN  14  

#define NUM_LEDS    7

// --- Color Definitions (16-bit RGB565) ---
#define COLOR_BG_TOP       0x0169  // Deep Vibrant Blue
#define COLOR_BG_BOTTOM    0x00A5  // Dark Navy
#define COLOR_HEADER_BG    0x194A  // Royal Navy Panel Header
#define COLOR_WARN_RED     0xB800  // Bright Alert Red
#define COLOR_CARD_BG      0x18E7  // Slate Blue Card Panel
#define COLOR_CARD_BORDER  0x3B78  // Soft Blue Border

// --- Wi-Fi & Telegram Credentials ---
const char* WIFI_SSID = "NothingPhone";
const char* WIFI_PASS = "88888888";

#define BOTtoken "8608873198:AAEHPKgwLyRJOSD09U_cutZSEFEwvMCdIPA"
#define CHAT_ID "1492372397"

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
Preferences preferences;

// FreeRTOS Task Handles & Mutex
TaskHandle_t NetworkTaskHandle = NULL;
SemaphoreHandle_t telemetryMutex = NULL;

// Hardware Object Instances
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
DHT dht(DHT_PIN, DHTTYPE);
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- Navigation & Pages ---
enum ScreenPage { 
  PAGE_MAIN, 
  PAGE_SETTINGS, 
  PAGE_CROP_SELECT, 
  PAGE_THRESHOLD_EDIT 
};
ScreenPage currentPage = PAGE_MAIN;

// --- Automation Threshold Structure ---
struct CropProfile {
  char name[12];
  float targetPh;
  float tempMin;
  float tempMax;
  int soilMin;
  int soilMax;
};

// Default Profiles
CropProfile crops[4] = {
  {"Tomato",     6.2, 18.0, 28.0, 40, 70},
  {"Lettuce",    6.0, 15.0, 22.0, 50, 80},
  {"Strawberry", 5.8, 16.0, 26.0, 45, 75},
  {"Custom",     6.5, 20.0, 30.0, 30, 60}
};

int activeCropIndex = 0;
bool autoControlEnabled = false;

// System Settings
bool buzzerEnabled = true;
bool indicatorLedEnabled = true;
bool autoTelegramAlertsEnabled = true;

// Alert gap options in minutes: 30m, 60m (1h), 120m (2h), 240m (4h), 480m (8h)
const int alertIntervalOptions[] = {30, 60, 120, 240, 480};
int alertIntervalIndex = 1; // Default: 60 mins (1 hr)

// Dynamic Threshold Variables
float cfg_targetPh = 6.2;
float cfg_tempMin = 18.0;
float cfg_tempMax = 28.0;
int cfg_soilMin = 40;
int cfg_soilMax = 70;

// --- State Variables ---
float simulated_pH = 6.2; 
bool isPhEditing = false;
bool isThresholdEditing = false;

float temperature = 0.0;
float humidity = 0.0;
int soilMoisturePct = 0;
bool isDark = false;

bool pumpState = false;
bool fanState = false;
bool growLightState = false;
bool isWarningActive = false;
bool isHighPhActive = false;
bool isLowPhActive = false;
int aiDiseaseRiskLevel = 0; // 0: Normal, 1: Moderate Risk, 2: High Disease Alert

// Network Flags for Cross-Core Communication
volatile bool triggerTelegramInstantFlag = false;
volatile bool sendWarningNotificationFlag = false;

// Navigation Indices
int selectedIndex = 0;     
int settingsIndex = 0;     
int cropSelectIndex = 0;    
int thresholdIndex = 0;     
int prevSelectedIndex = -1;

// LED Animation
int spinIndex = 0;
unsigned long lastAnimationTime = 0;

// Joystick Calibration
int xCenter = 2048;
int yCenter = 2048;
const int DEADZONE = 600; 

// Timers
unsigned long lastNavTime = 0;
unsigned long lastValueStepTime = 0;
unsigned long lastSensorReadTime = 0;
unsigned long lastWarningBeepTime = 0;

// Function Declarations
void drawStaticUI();
void drawCard(int index, bool isSelected);
void drawSettingsUI();
void drawSettingsCard(int index, bool isSelected);
void drawCropSelectUI();
void drawCropCard(int index, bool isSelected);
void drawThresholdUI();
void drawThresholdCard(int index, bool isSelected);

void drawWifiIcon(int x, int y, bool isConnected);
void drawAIIcon(int x, int y, bool isAuto);
void readSensors();
void runAIInference();
void processAutoControl();
void checkAIWarnings();
void stopAllActuators();
void updateTelemetryDisplay();
void updateLEDs();
void triggerBuzzer(int frequency, int duration);
void startWiFiAsync();

// Network Task & Telegram Helpers
void NetworkTask(void *pvParameters);
void sendTelegramNotification(String msg);
void handleTelegramMessages(int numNewMessages);
String buildTelemetryReport();
String getActiveRiskCauses();

// NVS Storage Helpers
void loadSettingsFromNVS();
void saveSettingsToNVS();

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 

  Serial.begin(115200);

  // Initialize FreeRTOS Mutex for safely accessing shared sensor telemetry
  telemetryMutex = xSemaphoreCreateMutex();

  analogSetAttenuation(ADC_11db);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(LDR_DO_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(PUMP_PIN, HIGH); 
  digitalWrite(FAN_PIN, HIGH);

  // Load saved settings from NVS Flash
  loadSettingsFromNVS();

  dht.begin();
  
  SPI.setFrequency(16000000);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3);

  strip.begin();
  strip.setBrightness(200);
  strip.clear();
  strip.show();

  long xSum = 0, ySum = 0;
  for (int i = 0; i < 20; i++) {
    xSum += analogRead(VRX_PIN);
    ySum += analogRead(VRY_PIN);
    delay(10);
  }
  xCenter = xSum / 20;
  yCenter = ySum / 20;

  client.setInsecure(); // Skip TLS verification

  triggerBuzzer(2000, 100);
  readSensors();
  drawStaticUI();
  updateLEDs();

  startWiFiAsync();

  // --- PIN NETWORK TASK TO CORE 0 ---
  xTaskCreatePinnedToCore(
    NetworkTask,        // Task function
    "NetworkTask",      // Name of task
    8192,               // Stack size in words
    NULL,               // Parameter
    1,                  // Task Priority
    &NetworkTaskHandle, // Task handle
    0                   // Core ID (0)
  );
}

// --- CORE 1: MAIN APPLICATION, AI INFERENCE, SENSORS, & DISPLAY ---
void loop() {
  unsigned long currentMillis = millis();

  // Read Joystick Analog Values
  int xVal = analogRead(VRX_PIN);
  int yVal = analogRead(VRY_PIN);
  bool btnPressed = (digitalRead(SW_PIN) == LOW);

  // --- 1. SENSOR & AI LOGIC (CORE 1) ---
  if (currentMillis - lastSensorReadTime > 1000) {
    if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      readSensors();
      runAIInference();       
      processAutoControl();   
      checkAIWarnings();
      xSemaphoreGive(telemetryMutex);
    }
    
    if (currentPage == PAGE_MAIN) {
      updateTelemetryDisplay();
    }
    lastSensorReadTime = currentMillis;
  }

  // --- 2. LED ANIMATION (CORE 1) ---
  if (currentMillis - lastAnimationTime > 200) {
    updateLEDs();
    lastAnimationTime = currentMillis;
  }

  // --- 3. JOYSTICK NAVIGATION (CORE 1) ---
  if (currentPage == PAGE_MAIN) {
    if (!isPhEditing && (currentMillis - lastNavTime > 200)) {
      if (xVal < (xCenter - DEADZONE)) { 
        prevSelectedIndex = selectedIndex;
        selectedIndex = (selectedIndex - 1 + 5) % 5;
        drawCard(prevSelectedIndex, false);
        drawCard(selectedIndex, true);
        triggerBuzzer(1500, 30);
        lastNavTime = currentMillis;
      } 
      else if (xVal > (xCenter + DEADZONE)) { 
        prevSelectedIndex = selectedIndex;
        selectedIndex = (selectedIndex + 1) % 5;
        drawCard(prevSelectedIndex, false);
        drawCard(selectedIndex, true);
        triggerBuzzer(1500, 30);
        lastNavTime = currentMillis;
      }
    }

    if (isPhEditing && (selectedIndex == 0) && (currentMillis - lastValueStepTime > 100)) {
      bool inc = (yVal > (yCenter + DEADZONE)) || (xVal > (xCenter + DEADZONE));
      bool dec = (yVal < (yCenter - DEADZONE)) || (xVal < (xCenter - DEADZONE));

      if (inc && simulated_pH < 14.0) {
        simulated_pH += 0.1;
        drawCard(0, true);
        triggerBuzzer(1800, 15);
        lastValueStepTime = currentMillis;
      } else if (dec && simulated_pH > 0.1) {
        simulated_pH -= 0.1;
        drawCard(0, true);
        triggerBuzzer(1200, 15);
        lastValueStepTime = currentMillis;
      }
    }
  } 
  else if (currentPage == PAGE_SETTINGS) {
    if (currentMillis - lastNavTime > 200) {
      if (xVal < (xCenter - DEADZONE)) {
        int prev = settingsIndex;
        settingsIndex = (settingsIndex - 1 + 8) % 8;
        drawSettingsCard(prev, false);
        drawSettingsCard(settingsIndex, true);
        triggerBuzzer(1500, 30);
        lastNavTime = currentMillis;
      } else if (xVal > (xCenter + DEADZONE)) {
        int prev = settingsIndex;
        settingsIndex = (settingsIndex + 1) % 8;
        drawSettingsCard(prev, false);
        drawSettingsCard(settingsIndex, true);
        triggerBuzzer(1500, 30);
        lastNavTime = currentMillis;
      }
    }
  }
  else if (currentPage == PAGE_CROP_SELECT) {
    if (currentMillis - lastNavTime > 200) {
      if (xVal < (xCenter - DEADZONE)) {
        int prev = cropSelectIndex;
        cropSelectIndex = (cropSelectIndex - 1 + 5) % 5;
        drawCropCard(prev, false);
        drawCropCard(cropSelectIndex, true);
        triggerBuzzer(1500, 30);
        lastNavTime = currentMillis;
      } else if (xVal > (xCenter + DEADZONE)) {
        int prev = cropSelectIndex;
        cropSelectIndex = (cropSelectIndex + 1) % 5;
        drawCropCard(prev, false);
        drawCropCard(cropSelectIndex, true);
        triggerBuzzer(1500, 30);
        lastNavTime = currentMillis;
      }
    }
  }
  else if (currentPage == PAGE_THRESHOLD_EDIT) {
    if (!isThresholdEditing && (currentMillis - lastNavTime > 200)) {
      if (xVal < (xCenter - DEADZONE)) {
        int prev = thresholdIndex;
        thresholdIndex = (thresholdIndex - 1 + 7) % 7;
        drawThresholdCard(prev, false);
        drawThresholdCard(thresholdIndex, true);
        triggerBuzzer(1500, 30);
        lastNavTime = currentMillis;
      } else if (xVal > (xCenter + DEADZONE)) {
        int prev = thresholdIndex;
        thresholdIndex = (thresholdIndex + 1) % 7;
        drawThresholdCard(prev, false);
        drawThresholdCard(thresholdIndex, true);
        triggerBuzzer(1500, 30);
        lastNavTime = currentMillis;
      }
    }

    if (isThresholdEditing && (thresholdIndex < 5) && (currentMillis - lastValueStepTime > 100)) {
      bool inc = (yVal > (yCenter + DEADZONE)) || (xVal > (xCenter + DEADZONE));
      bool dec = (yVal < (yCenter - DEADZONE)) || (xVal < (xCenter - DEADZONE));

      if (inc || dec) {
        float dir = inc ? 1.0 : -1.0;
        if (thresholdIndex == 0) cfg_targetPh = constrain(cfg_targetPh + (dir * 0.1), 0.0, 14.0);
        else if (thresholdIndex == 1) cfg_tempMin = constrain(cfg_tempMin + (dir * 0.5), 0.0, 50.0);
        else if (thresholdIndex == 2) cfg_tempMax = constrain(cfg_tempMax + (dir * 0.5), 0.0, 50.0);
        else if (thresholdIndex == 3) cfg_soilMin = constrain(cfg_soilMin + (int)dir, 0, 100);
        else if (thresholdIndex == 4) cfg_soilMax = constrain(cfg_soilMax + (int)dir, 0, 100);

        drawThresholdCard(thresholdIndex, true);
        triggerBuzzer(1600, 15);
        lastValueStepTime = currentMillis;
      }
    }
  }

  // --- 4. BUTTON PRESS HANDLER (CORE 1) ---
  static unsigned long lastBtnTime = 0;
  if (btnPressed && (currentMillis - lastBtnTime > 500)) { // Increased debounce gap
    lastBtnTime = currentMillis; // Immediately latch timestamp
    triggerBuzzer(2500, 60);

    if (currentPage == PAGE_MAIN) {
      if (selectedIndex == 0) {
        isPhEditing = !isPhEditing;
        drawCard(0, true);
      }
      else if (selectedIndex == 1 && !autoControlEnabled) {
        pumpState = !pumpState;
        digitalWrite(PUMP_PIN, pumpState ? LOW : HIGH);
        drawCard(1, true);
      } 
      else if (selectedIndex == 2 && !autoControlEnabled) {
        fanState = !fanState;
        digitalWrite(FAN_PIN, fanState ? LOW : HIGH);
        drawCard(2, true);
      }
      else if (selectedIndex == 3 && !autoControlEnabled) {
        growLightState = !growLightState;
        drawCard(3, true);
      }
      else if (selectedIndex == 4) {
        currentPage = PAGE_SETTINGS;
        settingsIndex = 0;
        drawSettingsUI();
      }
    } 
    else if (currentPage == PAGE_SETTINGS) {
      if (settingsIndex == 0) {
        startWiFiAsync();
        drawSettingsUI();
      } else if (settingsIndex == 1) { 
        currentPage = PAGE_CROP_SELECT;
        cropSelectIndex = 0;
        drawCropSelectUI();
      } else if (settingsIndex == 2) { 
        buzzerEnabled = !buzzerEnabled;
        saveSettingsToNVS();
        drawSettingsCard(2, true);
      } else if (settingsIndex == 3) { 
        indicatorLedEnabled = !indicatorLedEnabled;
        saveSettingsToNVS();
        drawSettingsCard(3, true);
      } else if (settingsIndex == 4) { 
        autoTelegramAlertsEnabled = !autoTelegramAlertsEnabled;
        saveSettingsToNVS();
        drawSettingsCard(4, true);
      } else if (settingsIndex == 5) { 
        alertIntervalIndex = (alertIntervalIndex + 1) % 5;
        saveSettingsToNVS();
        drawSettingsCard(5, true);
      } else if (settingsIndex == 6) { 
        if (!triggerTelegramInstantFlag) { // Check flag isn't active already
          triggerTelegramInstantFlag = true; // Set flag
          tft.fillRect(10, 100, 140, 20, ST77XX_BLUE);
          tft.setCursor(15, 106);
          tft.setTextColor(ST77XX_WHITE);
          tft.print("Sending Alert...");
          delay(1200);
          drawSettingsUI();
        }
      } else if (settingsIndex == 7) {
        currentPage = PAGE_MAIN;
        drawStaticUI();
      }
    }
    else if (currentPage == PAGE_CROP_SELECT) {
      if (cropSelectIndex >= 0 && cropSelectIndex <= 3) { 
        activeCropIndex = cropSelectIndex;
        cfg_targetPh = crops[activeCropIndex].targetPh;
        cfg_tempMin  = crops[activeCropIndex].tempMin;
        cfg_tempMax  = crops[activeCropIndex].tempMax;
        cfg_soilMin  = crops[activeCropIndex].soilMin;
        cfg_soilMax  = crops[activeCropIndex].soilMax;

        currentPage = PAGE_THRESHOLD_EDIT;
        thresholdIndex = 0;
        drawThresholdUI();
      } else if (cropSelectIndex == 4) { 
        currentPage = PAGE_SETTINGS;
        drawSettingsUI();
      }
    }
    else if (currentPage == PAGE_THRESHOLD_EDIT) {
      if (thresholdIndex >= 0 && thresholdIndex <= 4) {
        isThresholdEditing = !isThresholdEditing;
        drawThresholdCard(thresholdIndex, true);
      } 
      else if (thresholdIndex == 5) { 
        autoControlEnabled = !autoControlEnabled;
        if (!autoControlEnabled) {
          stopAllActuators();
        }
        drawThresholdCard(5, true);
      } 
      else if (thresholdIndex == 6) { 
        crops[activeCropIndex].targetPh = cfg_targetPh;
        crops[activeCropIndex].tempMin  = cfg_tempMin;
        crops[activeCropIndex].tempMax  = cfg_tempMax;
        crops[activeCropIndex].soilMin  = cfg_soilMin;
        crops[activeCropIndex].soilMax  = cfg_soilMax;

        isThresholdEditing = false;
        saveSettingsToNVS();

        if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          readSensors();
          if (autoControlEnabled) {
            processAutoControl();
          } else {
            stopAllActuators();
          }
          xSemaphoreGive(telemetryMutex);
        }

        triggerBuzzer(3000, 150); 
        currentPage = PAGE_MAIN;
        drawStaticUI();
      }
    }
  }
}

// --- CORE 0: DEDICATED NETWORK & TELEGRAM TASK ---
void NetworkTask(void *pvParameters) {
  unsigned long lastTelegramPollTime = 0;
  unsigned long lastPeriodicReportTime = millis(); // Start timer at current millis

  for (;;) {
    unsigned long currentMillis = millis();

    if (WiFi.status() == WL_CONNECTED) {
      // 1. Poll Incoming Messages
      if (currentMillis - lastTelegramPollTime > 2500) {
        int numNewMessages = bot.getUpdates(bot.messages[0].update_id + 1);
        while (numNewMessages) {
          handleTelegramMessages(numNewMessages);
          numNewMessages = bot.getUpdates(bot.messages[0].update_id + 1);
        }
        lastTelegramPollTime = currentMillis;
      }

      // 2. Periodic Custom-Interval Report (If Auto Alerts Enabled)
      unsigned long reportIntervalMs = (unsigned long)alertIntervalOptions[alertIntervalIndex] * 60UL * 1000UL;
      if (autoTelegramAlertsEnabled && (currentMillis - lastPeriodicReportTime >= reportIntervalMs)) {
        String report = "";
        if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          report = buildTelemetryReport();
          xSemaphoreGive(telemetryMutex);
        }
        sendTelegramNotification("⏰ *[SCHEDULED SYSTEM REPORT]*\n\n" + report);
        lastPeriodicReportTime = currentMillis;
      }

      // 3. Handle Flags Raised by Core 1
      if (triggerTelegramInstantFlag) {
        String report = "";
        if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          report = buildTelemetryReport();
          xSemaphoreGive(telemetryMutex);
        }
        sendTelegramNotification("⚡ *[INSTANT TELEGRAM ALERT]*\n\n" + report);
        triggerTelegramInstantFlag = false; // Reset flag after sending
      }

      if (sendWarningNotificationFlag) {
        if (autoTelegramAlertsEnabled) {
          String report = "";
          if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            report = buildTelemetryReport();
            xSemaphoreGive(telemetryMutex);
          }
          sendTelegramNotification("🚨 *[SYSTEM ALERT DETECTED]*\n\n" + report);
        }
        sendWarningNotificationFlag = false; // Reset flag after evaluation
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Non-blocking delay yielding to CPU Core 0
  }
}

// --- NVS PERSISTENCE FUNCTIONS ---
void loadSettingsFromNVS() {
  preferences.begin("farm_cfg", true);

  activeCropIndex           = preferences.getInt("cropIdx", 0);
  autoControlEnabled        = preferences.getBool("autoCtrl", false);
  buzzerEnabled             = preferences.getBool("buzzer", true);
  indicatorLedEnabled       = preferences.getBool("indLed", true);
  autoTelegramAlertsEnabled = preferences.getBool("autoTgAlert", true);
  alertIntervalIndex        = preferences.getInt("alertGapIdx", 1);

  cfg_targetPh = preferences.getFloat("targetPh", crops[activeCropIndex].targetPh);
  cfg_tempMin  = preferences.getFloat("tempMin", crops[activeCropIndex].tempMin);
  cfg_tempMax  = preferences.getFloat("tempMax", crops[activeCropIndex].tempMax);
  cfg_soilMin  = preferences.getInt("soilMin", crops[activeCropIndex].soilMin);
  cfg_soilMax  = preferences.getInt("soilMax", crops[activeCropIndex].soilMax);

  crops[activeCropIndex].targetPh = cfg_targetPh;
  crops[activeCropIndex].tempMin  = cfg_tempMin;
  crops[activeCropIndex].tempMax  = cfg_tempMax;
  crops[activeCropIndex].soilMin  = cfg_soilMin;
  crops[activeCropIndex].soilMax  = cfg_soilMax;

  preferences.end();
}

void saveSettingsToNVS() {
  preferences.begin("farm_cfg", false);

  preferences.putInt("cropIdx", activeCropIndex);
  preferences.putBool("autoCtrl", autoControlEnabled);
  preferences.putBool("buzzer", buzzerEnabled);
  preferences.putBool("indLed", indicatorLedEnabled);
  preferences.putBool("autoTgAlert", autoTelegramAlertsEnabled);
  preferences.putInt("alertGapIdx", alertIntervalIndex);

  preferences.putFloat("targetPh", cfg_targetPh);
  preferences.putFloat("tempMin", cfg_tempMin);
  preferences.putFloat("tempMax", cfg_tempMax);
  preferences.putInt("soilMin", cfg_soilMin);
  preferences.putInt("soilMax", cfg_soilMax);

  preferences.end();
}

// --- TELEGRAM UTILITY FUNCTIONS ---
String getActiveRiskCauses() {
  String causes = "";

  if (isHighPhActive) {
    causes += "  • ⚠️ High pH Warning (" + String(simulated_pH, 1) + " > Target " + String(cfg_targetPh, 1) + ")\n";
  }
  if (isLowPhActive) {
    causes += "  • ⚠️ Low pH Warning (" + String(simulated_pH, 1) + " < Target " + String(cfg_targetPh, 1) + ")\n";
  }

  if (aiDiseaseRiskLevel == 2) {
    causes += "  • 🚨 High AI Crop Disease Risk Identified\n";
  } else if (aiDiseaseRiskLevel == 1) {
    causes += "  • ⚠️ Moderate AI Crop Disease Risk Warning\n";
  }

  if (temperature > cfg_tempMax) {
    causes += "  • 🔥 Over-Temperature Alert (" + String(temperature, 1) + " °C > Max " + String(cfg_tempMax, 1) + " °C)\n";
  } else if (temperature < cfg_tempMin) {
    causes += "  • ❄️ Under-Temperature Alert (" + String(temperature, 1) + " °C < Min " + String(cfg_tempMin, 1) + " °C)\n";
  }

  if (soilMoisturePct < cfg_soilMin) {
    causes += "  • 🌵 Dry Soil Warning (" + String(soilMoisturePct) + " % < Min " + String(cfg_soilMin) + " %)\n";
  } else if (soilMoisturePct > cfg_soilMax) {
    causes += "  • 🌊 Oversaturated Soil Warning (" + String(soilMoisturePct) + " % > Max " + String(cfg_soilMax) + " %)\n";
  }

  if (causes == "") {
    causes = "  • ✅ None (All Parameters Within Normal Range)\n";
  }

  return causes;
}

String buildTelemetryReport() {
  String msg = "📊 *Current System Metrics:*\n";
  msg += "• *Temperature:* " + String(temperature, 1) + " °C\n";
  msg += "• *Humidity:* " + String((int)humidity) + " %\n";
  msg += "• *Soil Moisture:* " + String(soilMoisturePct) + " %\n";
  msg += "• *Digital pH:* " + String(simulated_pH, 1) + " (Target: " + String(cfg_targetPh, 1) + ")\n";
  msg += "• *Light Condition:* " + String(isDark ? "Dark 🌙" : "Daylight ☀️") + "\n";
  msg += "• *Selected Crop:* " + String(crops[activeCropIndex].name) + "\n\n";

  msg += "⚙️ *Actuator & Mode States:*\n";
  msg += "• *Auto Mode:* " + String(autoControlEnabled ? "ENABLED 🟢" : "OFF 🔴") + "\n";
  msg += "• *Water Pump:* " + String(pumpState ? "ACTIVE 💧" : "OFF ⏹") + "\n";
  msg += "• *Cooling Fan:* " + String(fanState ? "ACTIVE 🌀" : "OFF ⏹") + "\n";
  msg += "• *Grow Light:* " + String(growLightState ? "ON 💡" : "OFF ⏹") + "\n\n";

  msg += "⚠️ *Risk & Alert Causes:*\n";
  msg += getActiveRiskCauses();

  return msg;
}

void sendTelegramNotification(String msg) {
  if (WiFi.status() == WL_CONNECTED) {
    bot.sendMessage(CHAT_ID, msg, "Markdown");
  }
}

void handleTelegramMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    if (text == "/start" || text == "/help") {
      String welcome = "Welcome " + from_name + " to your Smart Farm Control Bot!\n\n";
      welcome += "Available Commands:\n";
      welcome += "/status - Get current telemetry & risk causes\n";
      welcome += "/pump_on - Turn Water Pump ON\n";
      welcome += "/pump_off - Turn Water Pump OFF\n";
      welcome += "/fan_on - Turn Fan ON\n";
      welcome += "/fan_off - Turn Fan OFF\n";
      welcome += "/auto_on - Enable Auto Mode\n";
      welcome += "/auto_off - Disable Auto Mode\n";
      bot.sendMessage(CHAT_ID, welcome, "");
    }
    else if (text == "/status") {
      String report = "";
      if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        report = buildTelemetryReport();
        xSemaphoreGive(telemetryMutex);
      }
      sendTelegramNotification("📡 *[TELEGRAM STATUS QUERY]*\n\n" + report);
    }
    else if (text == "/pump_on") {
      if (!autoControlEnabled) {
        pumpState = true;
        digitalWrite(PUMP_PIN, LOW);
        bot.sendMessage(CHAT_ID, "💧 Water Pump turned ON manually.", "");
      } else {
        bot.sendMessage(CHAT_ID, "⚠️ Disable Auto Mode first to control actuators manually.", "");
      }
    }
    else if (text == "/pump_off") {
      if (!autoControlEnabled) {
        pumpState = false;
        digitalWrite(PUMP_PIN, HIGH);
        bot.sendMessage(CHAT_ID, "⏹ Water Pump turned OFF manually.", "");
      } else {
        bot.sendMessage(CHAT_ID, "⚠️ Disable Auto Mode first to control actuators manually.", "");
      }
    }
    else if (text == "/fan_on") {
      if (!autoControlEnabled) {
        fanState = true;
        digitalWrite(FAN_PIN, LOW);
        bot.sendMessage(CHAT_ID, "🌀 Cooling Fan turned ON manually.", "");
      } else {
        bot.sendMessage(CHAT_ID, "⚠️ Disable Auto Mode first to control actuators manually.", "");
      }
    }
    else if (text == "/fan_off") {
      if (!autoControlEnabled) {
        fanState = false;
        digitalWrite(FAN_PIN, HIGH);
        bot.sendMessage(CHAT_ID, "⏹ Cooling Fan turned OFF manually.", "");
      } else {
        bot.sendMessage(CHAT_ID, "⚠️ Disable Auto Mode first to control actuators manually.", "");
      }
    }
    else if (text == "/auto_on") {
      autoControlEnabled = true;
      saveSettingsToNVS();
      if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        processAutoControl();
        xSemaphoreGive(telemetryMutex);
      }
      bot.sendMessage(CHAT_ID, "🟢 Auto Mode is now ENABLED.", "");
    }
    else if (text == "/auto_off") {
      autoControlEnabled = false;
      saveSettingsToNVS();
      stopAllActuators();
      bot.sendMessage(CHAT_ID, "🔴 Auto Mode is now OFF.", "");
    }
  }
}

// --- AI MODEL INFERENCE ---
void runAIInference() {
  float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {
    (float)simulated_pH,
    (float)temperature,
    (float)humidity,
    (float)soilMoisturePct,
    (float)(isDark ? 1.0 : 0.0),
    (float)cfg_tempMin,
    (float)cfg_tempMax,
    (float)cfg_soilMin,
    (float)cfg_soilMax
  };

  signal_t signal;
  int err = numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
  if (err != EI_IMPULSE_OK) return;

  ei_impulse_result_t result = { 0 };
  err = run_classifier(&signal, &result, false);
  if (err != EI_IMPULSE_OK) return;

  int predicted_class = 0;
  float highest_score = 0.0;
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    if (result.classification[ix].value > highest_score) {
      highest_score = result.classification[ix].value;
      predicted_class = ix;
    }
  }

  aiDiseaseRiskLevel = predicted_class;
}

void processAutoControl() {
  if (!autoControlEnabled) return;

  pumpState      = (soilMoisturePct < cfg_soilMin);
  fanState       = (temperature > cfg_tempMax || humidity > 85.0);
  growLightState = isDark;

  digitalWrite(PUMP_PIN, pumpState ? LOW : HIGH);
  digitalWrite(FAN_PIN, fanState ? LOW : HIGH);
}

void checkAIWarnings() {
  bool prevWarning = isWarningActive;

  // High & Low pH condition checks
  isHighPhActive = (simulated_pH > (cfg_targetPh + 1.0)) || (simulated_pH >= 8.0);
  isLowPhActive  = (simulated_pH < (cfg_targetPh - 1.0)) || (simulated_pH <= 5.5);

  isWarningActive = isHighPhActive || 
                    isLowPhActive  ||
                    (aiDiseaseRiskLevel > 0) || 
                    (temperature > cfg_tempMax || temperature < cfg_tempMin) || 
                    (soilMoisturePct < cfg_soilMin);

  if (!prevWarning && isWarningActive) {
    sendWarningNotificationFlag = true; // Signal Core 0 to send warning message
  }

  if (isHighPhActive && (millis() - lastWarningBeepTime > 3000)) {
    triggerBuzzer(2200, 150);
    lastWarningBeepTime = millis();
  } else if (isLowPhActive && (millis() - lastWarningBeepTime > 3000)) {
    triggerBuzzer(1400, 150);
    lastWarningBeepTime = millis();
  } else if (aiDiseaseRiskLevel == 2 && (millis() - lastWarningBeepTime > 2000)) {
    triggerBuzzer(1800, 200); 
    lastWarningBeepTime = millis();
  } else if (isWarningActive && (millis() - lastWarningBeepTime > 4000)) {
    triggerBuzzer(1000, 100); 
    lastWarningBeepTime = millis();
  }
}

void stopAllActuators() {
  pumpState = false;
  fanState = false;
  growLightState = false;

  digitalWrite(PUMP_PIN, HIGH); 
  digitalWrite(FAN_PIN, HIGH);  
}

void triggerBuzzer(int frequency, int duration) {
  if (!buzzerEnabled) return;
  tone(BUZZER_PIN, frequency, duration);
}

void readSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidity = h;

  int rawSoil = analogRead(SOIL_PIN);
  soilMoisturePct = map(rawSoil, 4095, 0, 0, 100);
  soilMoisturePct = constrain(soilMoisturePct, 0, 100);

  isDark = (digitalRead(LDR_DO_PIN) == HIGH); 
}

void updateLEDs() {
  strip.clear();

  if (growLightState) {
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, strip.Color(180, 0, 255));
  } 
  else if (indicatorLedEnabled) {
    if (pumpState && fanState) {
      spinIndex = (spinIndex + 1) % NUM_LEDS;
      int oppositeIndex = (spinIndex + (NUM_LEDS / 2)) % NUM_LEDS;
      strip.setPixelColor(spinIndex, strip.Color(0, 100, 255));     
      strip.setPixelColor(oppositeIndex, strip.Color(0, 255, 0));   
    }
    else if (pumpState) {
      spinIndex = (spinIndex + 1) % NUM_LEDS;
      strip.setPixelColor(spinIndex, strip.Color(0, 100, 255)); 
    } 
    else if (fanState) {
      spinIndex = (spinIndex + 1) % NUM_LEDS;
      strip.setPixelColor(spinIndex, strip.Color(0, 255, 0)); 
    } 
    else {
      for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, strip.Color(255, 0, 0));
    }
  }

  strip.show();
}

void startWiFiAsync() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void drawStaticUI() {
  tft.fillRect(0, 0, 160, 60, COLOR_BG_TOP);
  tft.fillRect(0, 60, 160, 68, COLOR_BG_BOTTOM);

  updateTelemetryDisplay();
  for (int i = 0; i < 5; i++) {
    drawCard(i, selectedIndex == i);
  }
}

void drawWifiIcon(int x, int y, bool isConnected) {
  uint16_t color = isConnected ? ST77XX_CYAN : 0x52AA; 

  tft.fillRect(x + 5, y + 8, 2, 2, color);
  tft.drawPixel(x + 3, y + 6, color);
  tft.drawFastHLine(x + 4, y + 5, 4, color);
  tft.drawPixel(x + 8, y + 6, color);
  tft.drawPixel(x + 1, y + 3, color);
  tft.drawFastHLine(x + 2, y + 1, 8, color);
  tft.drawPixel(x + 10, y + 3, color);

  if (!isConnected) {
    tft.drawLine(x, y + 9, x + 10, y, ST77XX_RED);
  }
}

void drawAIIcon(int x, int y, bool isAuto) {
  uint16_t color = isAuto ? ST77XX_GREEN : 0x52AA;
  tft.drawRoundRect(x, y, 16, 9, 2, color);
  tft.setCursor(x + 2, y + 1);
  tft.setTextSize(1);
  tft.setTextColor(color);
  tft.print("AI");
}

void updateTelemetryDisplay() {
  tft.fillRect(2, 2, 156, 11, COLOR_HEADER_BG);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.setTextColor(ST77XX_CYAN);
  tft.print("T:"); tft.print(temperature, 1); tft.print("C  H:"); tft.print((int)humidity); tft.print("%");
  drawWifiIcon(142, 2, WiFi.status() == WL_CONNECTED);

  tft.fillRect(2, 14, 156, 11, COLOR_HEADER_BG);
  tft.setCursor(4, 16);
  tft.setTextColor(ST77XX_GREEN);
  tft.print("Soil:"); tft.print(soilMoisturePct); tft.print("%  LGT:");
  if (isDark) {
    tft.setTextColor(ST77XX_RED);
    tft.print("DRK");
  } else {
    tft.setTextColor(ST77XX_YELLOW);
    tft.print("DAY");
  }
  drawAIIcon(138, 15, autoControlEnabled);

  uint16_t warnBg = (isHighPhActive || isLowPhActive || aiDiseaseRiskLevel == 2 || isWarningActive) ? COLOR_WARN_RED : 0x0320;
  tft.fillRect(2, 26, 156, 11, warnBg);
  tft.setCursor(18, 28);
  tft.setTextColor(ST77XX_WHITE);
  
  if (isHighPhActive) {
    tft.print("[ pH TOO HIGH! ]");
  } else if (isLowPhActive) {
    tft.print("[ pH TOO LOW! ]");
  } else if (aiDiseaseRiskLevel == 2) {
    tft.print("[ DIS.RISK ALERT ]");
  } else if (isWarningActive) {
    tft.print("[ TEMP/SOIL WARN ]");
  } else {
    tft.setTextColor(ST77XX_GREEN);
    tft.print("[ OK - NOMINAL ]");
  }
}

void drawCard(int index, bool isSelected) {
  uint16_t border = isSelected ? (isPhEditing && index == 0 ? ST77XX_ORANGE : ST77XX_YELLOW) : COLOR_CARD_BORDER;
  uint16_t bg = isSelected ? 0x2A15 : COLOR_CARD_BG;

  int yPos = 39 + (index * 17);

  tft.fillRoundRect(2, yPos, 156, 15, 3, bg);
  tft.drawRoundRect(2, yPos, 156, 15, 3, border);

  tft.setTextSize(1);
  tft.setCursor(6, yPos + 4);

  if (index == 0) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Digital pH: ");
    tft.setTextColor((isHighPhActive || isLowPhActive) ? ST77XX_RED : 0x07FF);
    tft.print(simulated_pH, 1);
    if (isPhEditing) {
      tft.setTextColor(ST77XX_ORANGE);
      tft.print(" *");
    }
  } else if (index == 1) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Water Pump: ");
    tft.setTextColor(pumpState ? ST77XX_GREEN : ST77XX_RED);
    tft.print(pumpState ? "[ ACTIVE ]" : "[ OFF ]");
  } else if (index == 2) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Cooling Fan:");
    tft.setTextColor(fanState ? ST77XX_GREEN : ST77XX_RED);
    tft.print(fanState ? "[ ACTIVE ]" : "[ OFF ]");
  } else if (index == 3) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Grow Light: ");
    tft.setTextColor(growLightState ? ST77XX_GREEN : ST77XX_RED);
    tft.print(growLightState ? "[ ON ]" : "[ OFF ]");
  } else if (index == 4) {
    tft.setTextColor(ST77XX_YELLOW);
    tft.print("> WiFi & System Config");
  }
}

void drawSettingsUI() {
  tft.fillRect(0, 0, 160, 128, COLOR_BG_TOP);

  tft.fillRect(0, 0, 160, 13, 0x3186);
  tft.setCursor(20, 2);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("SYSTEM SETTINGS");

  for (int i = 0; i < 8; i++) drawSettingsCard(i, settingsIndex == i);
}

void drawSettingsCard(int index, bool isSelected) {
  uint16_t border = isSelected ? ST77XX_YELLOW : COLOR_CARD_BORDER;
  uint16_t bg = COLOR_CARD_BG;

  int yPos = 14 + (index * 14);

  tft.fillRoundRect(2, yPos, 156, 13, 2, bg);
  tft.drawRoundRect(2, yPos, 156, 13, 2, border);

  tft.setCursor(6, yPos + 3);
  tft.setTextSize(1);

  if (index == 0) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("WiFi: ");
    tft.setTextColor(WiFi.status() == WL_CONNECTED ? ST77XX_GREEN : ST77XX_RED);
    tft.print(WiFi.status() == WL_CONNECTED ? "Connected" : "Reconnecting");
  } else if (index == 1) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Crop: ");
    tft.setTextColor(ST77XX_GREEN);
    tft.print(crops[activeCropIndex].name);
  } else if (index == 2) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Buzzer: ");
    tft.setTextColor(buzzerEnabled ? ST77XX_GREEN : ST77XX_RED);
    tft.print(buzzerEnabled ? "[ ON ]" : "[ MUTED ]");
  } else if (index == 3) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Indicator: ");
    tft.setTextColor(indicatorLedEnabled ? ST77XX_GREEN : ST77XX_RED);
    tft.print(indicatorLedEnabled ? "[ ON ]" : "[ OFF ]");
  } else if (index == 4) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Auto TG Alert: ");
    tft.setTextColor(autoTelegramAlertsEnabled ? ST77XX_GREEN : ST77XX_RED);
    tft.print(autoTelegramAlertsEnabled ? "[ON]" : "[OFF]");
  } else if (index == 5) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Alert Gap: ");
    tft.setTextColor(ST77XX_CYAN);
    int mins = alertIntervalOptions[alertIntervalIndex];
    if (mins >= 60) {
      tft.print(mins / 60); tft.print(" hr");
    } else {
      tft.print(mins); tft.print(" min");
    }
  } else if (index == 6) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Instant TG: ");
    tft.setTextColor(ST77XX_CYAN);
    tft.print("[ SEND ]");
  } else if (index == 7) {
    tft.setTextColor(ST77XX_CYAN);
    tft.print("< Save & Return");
  }
}

void drawCropSelectUI() {
  tft.fillRect(0, 0, 160, 128, COLOR_BG_TOP);

  tft.fillRect(0, 0, 160, 16, 0x03E0);
  tft.setCursor(20, 4);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLACK);
  tft.print("SELECT CROP PROFILE");

  for (int i = 0; i < 5; i++) drawCropCard(i, cropSelectIndex == i);
}

void drawCropCard(int index, bool isSelected) {
  uint16_t border = isSelected ? ST77XX_YELLOW : COLOR_CARD_BORDER;
  uint16_t bg = COLOR_CARD_BG;

  int yPos = 22 + (index * 19);

  tft.fillRoundRect(6, yPos, 148, 16, 3, bg);
  tft.drawRoundRect(6, yPos, 148, 16, 3, border);

  tft.setCursor(10, yPos + 4);
  tft.setTextSize(1);

  if (index < 4) {
    tft.setTextColor(activeCropIndex == index ? ST77XX_GREEN : ST77XX_WHITE);
    tft.print(crops[index].name);
    if (activeCropIndex == index) tft.print(" [ACTIVE]");
  } else {
    tft.setTextColor(ST77XX_CYAN);
    tft.print("< Back");
  }
}

void drawThresholdUI() {
  tft.fillRect(0, 0, 160, 128, COLOR_BG_TOP);

  tft.fillRect(0, 0, 160, 14, 0x4100);
  tft.setCursor(20, 3);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("CROP THRESHOLDS");

  for (int i = 0; i < 7; i++) drawThresholdCard(i, thresholdIndex == i);
}

void drawThresholdCard(int index, bool isSelected) {
  uint16_t border = isSelected ? (isThresholdEditing ? ST77XX_ORANGE : ST77XX_YELLOW) : COLOR_CARD_BORDER;
  uint16_t bg = COLOR_CARD_BG;

  int yPos = 17 + (index * 15);

  tft.fillRoundRect(4, yPos, 152, 13, 2, bg);
  tft.drawRoundRect(4, yPos, 152, 13, 2, border);

  tft.setCursor(8, yPos + 3);
  tft.setTextSize(1);

  if (index == 0) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Target pH: ");
    tft.setTextColor(ST77XX_CYAN);
    tft.print(cfg_targetPh, 1);
  } else if (index == 1) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Temp Min:  ");
    tft.setTextColor(ST77XX_CYAN);
    tft.print(cfg_tempMin, 1); tft.print(" C");
  } else if (index == 2) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Temp Max:  ");
    tft.setTextColor(ST77XX_CYAN);
    tft.print(cfg_tempMax, 1); tft.print(" C");
  } else if (index == 3) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Soil Min:  ");
    tft.setTextColor(ST77XX_CYAN);
    tft.print(cfg_soilMin); tft.print(" %");
  } else if (index == 4) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Soil Max:  ");
    tft.setTextColor(ST77XX_CYAN);
    tft.print(cfg_soilMax); tft.print(" %");
  } else if (index == 5) {
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Auto Mode: ");
    tft.setTextColor(autoControlEnabled ? ST77XX_GREEN : ST77XX_RED);
    tft.print(autoControlEnabled ? "[ ENABLED ]" : "[ OFF ]");
  } else if (index == 6) {
    tft.setTextColor(ST77XX_GREEN);
    tft.print(">> SAVE & EXIT");
  }

  if (isSelected && isThresholdEditing && index < 5) {
    tft.setTextColor(ST77XX_ORANGE);
    tft.print(" *");
  }
}