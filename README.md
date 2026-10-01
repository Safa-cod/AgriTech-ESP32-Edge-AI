# AgriTech-ESP32-Edge-AI
ESP32-powered Smart Agriculture System featuring FreeRTOS multi-threading, Edge Impulse AI crop health classification, bidirectional Telegram control, and local ST7735 TFT display navigation.
# 🌾 AgriTech ESP32 Edge AI

### Smart Automated Agricultural Control & Telemetry System

[![ESP32](https://img.shields.io/badge/ESP32-Dual--Core-red.svg)](https://www.espressif.com/)
[![Edge Impulse](https://img.shields.io/badge/Edge%20Impulse-Edge%20AI-blue.svg)](https://edgeimpulse.com/)
[![FreeRTOS](https://img.shields.io/badge/FreeRTOS-Multi--Tasking-green.svg)](https://www.freertos.org/)
[![Telegram](https://img.shields.io/badge/Telegram-Bot%20Control-0088cc.svg)](https://telegram.org/)
[![Arduino](https://img.shields.io/badge/Arduino-IDE-00979D.svg)](https://www.arduino.cc/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## 📌 Project Overview

**AgriTech ESP32 Edge AI** is a smart agricultural monitoring and automation system developed as an **Embedded Systems project**.

The system uses an **ESP32 dual-core microcontroller**, environmental sensors, **FreeRTOS**, and **Edge AI** to monitor agricultural conditions and automate important tasks such as irrigation and cooling.

The system collects real-time data from temperature, humidity, soil moisture, and light sensors. The collected data is processed locally by the ESP32, and the Edge AI model classifies the current crop/environment condition.

The project also provides two ways to interact with the system:

* 📱 **Online:** Telegram Bot for remote monitoring and control
* 📟 **Offline:** ST7735 TFT display with joystick navigation

This combination of embedded automation, Edge AI, and remote communication makes the system suitable as a low-cost smart farming prototype.

---

## 🎯 Objectives

The main objectives of this project are:

* Monitor agricultural environmental conditions in real time.
* Measure temperature and humidity using DHT22.
* Monitor soil moisture for irrigation decisions.
* Measure surrounding light intensity using an LDR.
* Use Edge AI for crop condition/stress classification.
* Automatically control a water pump based on soil conditions.
* Automatically control a cooling fan based on temperature/conditions.
* Provide remote monitoring through Telegram.
* Provide local offline monitoring through a TFT display.
* Use FreeRTOS for efficient multi-tasking.
* Store configurable parameters using ESP32 NVS.
* Demonstrate a practical application of Embedded Systems and AI in agriculture.

---

## 🌱 Main Features

### 🌡️ Environmental Monitoring

The system continuously monitors:

* Temperature
* Relative Humidity
* Soil Moisture
* Light Intensity

### 🤖 Edge AI

An Edge Impulse / TensorFlow Lite Micro model runs directly on the ESP32 for local crop/environment condition classification.

### 💧 Automatic Irrigation

The soil moisture level is monitored continuously. When the moisture level goes below the configured threshold, the system can activate the water pump.

### 🌬️ Automatic Cooling

The system monitors temperature conditions and can control a cooling fan through a relay.

### 📱 Telegram Remote Control

Users can remotely check and control the system using a Telegram Bot.

Example commands:

```text
/status
/pump_on
/pump_off
/auto_on
```

### 📟 Offline TFT Interface

A 1.44-inch ST7735 TFT display provides local system information without requiring Telegram.

The joystick allows the user to navigate through the local interface.

### 🔊 Alerts

The system provides alerts using:

* Active buzzer
* WS2812B NeoPixel

Different visual/audio indications can be used for different system conditions.

---

# ⚙️ System Architecture

```text
                         ┌───────────────────────┐
                         │      ESP32 Board      │
                         │     Dual-Core MCU     │
                         └───────────┬───────────┘
                                     │
                  ┌──────────────────┴──────────────────┐
                  │                                     │
            ┌─────▼─────┐                         ┌─────▼─────┐
            │   Core 0  │                         │   Core 1  │
            │  Network  │                         │ Sensors + │
            │  & Wi-Fi  │                         │ AI + UI   │
            └─────┬─────┘                         └─────┬─────┘
                  │                                     │
          ┌───────▼────────┐                    ┌──────▼─────────┐
          │ Telegram Bot   │                    │    Sensors     │
          │ Remote Control │                    │ DHT22 / Soil   │
          └────────────────┘                    │ Moisture / LDR │
                                                └──────┬─────────┘
                                                       │
                                                ┌──────▼─────────┐
                                                │    Edge AI     │
                                                │ Classification │
                                                └──────┬─────────┘
                                                       │
                              ┌────────────────────────┼────────────────────┐
                              │                        │                    │
                        ┌─────▼─────┐            ┌────▼────┐         ┌─────▼─────┐
                        │   Water   │            │ Cooling │         │   Alerts  │
                        │   Pump    │            │   Fan    │         │ LED/Buzzer│
                        └───────────┘            └─────────┘         └───────────┘

                              ┌─────────────────────────┐
                              │   Local Offline UI      │
                              │ ST7735 TFT + Joystick   │
                              └─────────────────────────┘
```

---

# ⚡ ESP32 Dual-Core & FreeRTOS

The project uses the ESP32's dual-core architecture with FreeRTOS tasks.

## Core 0 — Network & Communication

Core 0 mainly handles:

* Wi-Fi connection
* Telegram Bot communication
* Receiving commands
* Sending telemetry
* Sending remote alerts

## Core 1 — Sensor, AI & Local Control

Core 1 mainly handles:

* DHT22 sensor reading
* Soil moisture reading
* LDR reading
* Edge AI inference
* Automatic actuator control
* TFT display
* Joystick input
* Local configuration

### 🔐 Task Synchronization

Shared system data is protected using a mutex such as:

```text
telemetryMutex
```

This helps prevent multiple FreeRTOS tasks from accessing shared data incorrectly at the same time.

---

# 🤖 Edge AI

## AI Framework

The project uses:

* **Edge Impulse**
* **TensorFlow Lite Micro**
* ESP32 on-device inference

The AI model is deployed directly to the ESP32 instead of requiring a separate server for inference.

## AI Inputs

The model uses environmental parameters such as:

| Input           | Source               |
| --------------- | -------------------- |
| Temperature     | DHT22                |
| Humidity        | DHT22                |
| Soil Moisture   | Soil Moisture Sensor |
| Light Intensity | LDR                  |

## AI Processing Flow

```text
Sensor Readings
       ↓
Data Processing
       ↓
Feature Preparation
       ↓
Edge AI Model
       ↓
Inference
       ↓
Crop/Environmental Condition
       ↓
System Response
```

Possible condition classes used by the project may include:

* **Optimal**
* **Heat Stress**
* **Water Stress**
* **High Disease Risk**

The exact class names depend on the final trained Edge Impulse model.

---

# 📱 Telegram Bot

The Telegram Bot provides remote access to the system.

## Commands

| Command     | Description                                 |
| ----------- | ------------------------------------------- |
| `/status`   | Shows current sensor and system information |
| `/pump_on`  | Manually turns the pump ON                  |
| `/pump_off` | Manually turns the pump OFF                 |
| `/auto_on`  | Enables automatic control                   |

The system can also send notifications when important conditions are detected.

---

# 📟 Local TFT & Joystick Interface

The project includes a:

**1.44-inch ST7735 SPI TFT Display**

and:

**2-Axis Joystick Module**

The local interface can display information such as:

```text
Temperature
Humidity
Soil Moisture
Light Level
AI Condition
Pump Status
Fan Status
System Mode
```

The joystick is used to navigate menus and interact with the system.

---

# 🔌 Hardware Components

| Component                       | Qty | Purpose                       |
| ------------------------------- | --: | ----------------------------- |
| ESP32 Development Board         |   1 | Main controller               |
| ESP32 Expansion Board           |   1 | Easier GPIO/power connections |
| ST7735 1.44" TFT                |   1 | Local display                 |
| DHT22 Sensor                    |   1 | Temperature & humidity        |
| Capacitive Soil Moisture Sensor |   1 | Soil moisture monitoring      |
| LDR Sensor Module               |   1 | Light measurement             |
| 4-Channel 5V Relay              |   1 | Actuator switching            |
| 5V Water Pump                   |   1 | Irrigation                    |
| 5V DC Fan                       |   1 | Cooling                       |
| WS2812B NeoPixel                |   1 | Visual indication             |
| 2-Axis Joystick                 |   1 | Local navigation              |
| Active Buzzer                   |   1 | Audio alerts                  |

---

# 💻 Software & Technologies

| Technology                | Use                         |
| ------------------------- | --------------------------- |
| **ESP32**                 | Main microcontroller        |
| **C/C++**                 | Firmware development        |
| **Arduino IDE**           | Development and programming |
| **FreeRTOS**              | Multi-tasking               |
| **Edge Impulse**          | AI model development        |
| **TensorFlow Lite Micro** | On-device AI inference      |
| **Wi-Fi**                 | Internet communication      |
| **Telegram Bot API**      | Remote monitoring/control   |
| **ST7735**                | TFT display                 |
| **Preferences / NVS**     | Parameter storage           |
| **GitHub**                | Version control             |

---

# 📁 Repository Structure

```text
AgriTech-ESP32-Edge-AI/
│
├── README.md
├── LICENSE
│
├── firmware/
│   ├── Final_copy.ino
│   ├── FarmModel.h
│   ├── esp32/
│   └── static_buffer/
│
├── edge-impulse/
│   ├── *_inferencing.h
│   ├── edge-impulse-sdk/
│   ├── model-parameters/
│   ├── tflite-model/
│   └── dataset.csv
│
└── documentation/
    ├── Project Proposal.pdf
    └── AgriTech_Presentation.pptx
```

> The exact file/folder structure may be updated according to the final repository contents.

---

# 🛠️ Installation & Setup

## 1. Clone the Repository

```bash
git clone https://github.com/<your-username>/AgriTech-ESP32-Edge-AI.git
cd AgriTech-ESP32-Edge-AI
```

Replace `<your-username>` with the GitHub username that owns the repository.

## 2. Install Arduino IDE

Install Arduino IDE and add ESP32 support through:

```text
Tools → Board → Boards Manager
```

Search for **ESP32** and install the ESP32 board package.

## 3. Install Required Libraries

Install the libraries required by the project, such as:

```text
Adafruit GFX Library
Adafruit ST7735
DHT Sensor Library
Preferences
```

Other libraries included in the final firmware should also be installed if required.

## 4. Add Edge AI Files

The Edge Impulse deployment files should be available in the project structure.

Important files/folders include:

```text
edge-impulse-sdk/
model-parameters/
tflite-model/
*_inferencing.h
```

These files contain the required Edge AI inference components.

## 5. Configure Wi-Fi

Open the main firmware file and enter your Wi-Fi credentials.

```cpp
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
```

## 6. Configure Telegram

Add your Telegram Bot Token in the appropriate configuration section.

**Do not upload real Wi-Fi passwords or Telegram Bot Tokens to a public GitHub repository.**

## 7. Connect ESP32

Connect the ESP32 to your computer using a USB cable.

Select:

```text
Tools → Board → ESP32 Dev Module
```

Select the correct COM port.

## 8. Upload Firmware

Open the main `.ino` file and click:

```text
Upload
```

After uploading, open the Serial Monitor if required for debugging and system information.

---

# 💰 Estimated Cost

The prototype hardware cost is approximately:

## **৳2,950 BDT**

The actual price may vary depending on component availability and local market prices.

---

# 👥 Team Members

### Team: AgriTech Innovators

### Course: Introduction to Embedded Systems

| Name                              | Student ID      | Batch | Role                                |
| --------------------------------- | --------------- | ----: | ----------------------------------- |
| **Mohammad Fahim**                | `2023200000399` |    65 | Team Leader & Lead Architect        |
| **Maria Iffat**                   | `2023200000366` |    65 | Hardware & Sensor Engineer          |
| **Safayet Hossain Bhuiyan Munna** | `2023200000648` |    65 | UI/UX & Graphics Engineer           |
| **Fahad Ahmed**                   | `2023200000371` |    65 | Integration & Verification Engineer |

---

# 🌍 Social Impact

Agriculture often requires continuous monitoring of environmental conditions and efficient use of resources.

The AgriTech system demonstrates how embedded technology can help address these challenges through:

* Automated irrigation
* Environmental monitoring
* Local AI-based decision making
* Reduced manual monitoring
* Remote system access
* Real-time alerts
* Resource-efficient operation

The project is designed as a low-cost prototype demonstrating the potential of **Smart Agriculture + Embedded Systems + Edge AI**.

---

# 🔮 Future Improvements

Future versions of the system could include:

* ☀️ Solar-powered operation
* 📊 Cloud-based historical data
* 📱 Dedicated mobile application
* 🌦️ Weather API integration
* 🌱 More crop-specific AI models
* 🏞️ Multi-zone irrigation
* 📈 Long-term crop health analytics
* 🔋 Battery monitoring
* 📡 LoRa-based long-range communication
* 🧪 Additional soil parameter sensors

---

# 📚 Project Documentation

The repository may contain:

* Project Proposal
* Presentation Slides
* ESP32 Firmware
* Edge AI Model
* Training Dataset
* Hardware-related files
* Project documentation

---

# 📜 License

This project is released under the **MIT License**.

See the `LICENSE` file for more information.

---

# ⭐ Acknowledgement

This project was developed as part of the **Introduction to Embedded Systems** course.

The project demonstrates the integration of **ESP32, sensors, FreeRTOS, Edge AI, automation, TFT UI, and Telegram-based remote control** into a practical smart agriculture system.

---

## 🌾 AgriTech ESP32 Edge AI

**Smart Monitoring • Edge AI • Automation • Remote Control**
