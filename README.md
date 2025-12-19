# The Dreaming Clock

A beautiful ESP32-C3-based 7-segment LED clock with web interface, real-time clock module, and customizable display schedules.

![ESP32-C3](https://img.shields.io/badge/ESP32--C3-DevKitM--1-blue)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Framework-orange)
![License](https://img.shields.io/badge/License-MIT-green)

## ✨ Features

- **282 APA102 (Dotstar) LEDs** arranged as a 4-digit 7-segment display with colon
- **Animated color gradients** with smooth transitions between colors
- **DS1307 Real-Time Clock** for accurate timekeeping
- **Web-based configuration** via Captive Portal (no app required!)
- **Active Hours scheduling** - set when the display should be on/off per weekday
- **Auto-wakeup intervals** - display wakes up periodically to show the time
- **Over-The-Air (OTA) updates** - update firmware wirelessly
- **Persistent settings** stored in NVS (Non-Volatile Storage)

## 🎯 Display Modes

| Mode | Description |
|------|-------------|
| **Random** | Mesmerizing random color gradients flowing across all segments |
| **Wakeup** | Shows current time for 15 seconds with pulsing colon |
| **Time Not Set** | Blinking "00:00" to indicate RTC needs configuration |
| **Inactive** | Display off during configured inactive hours |

## 🔧 Hardware Requirements

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-C3-DevKitM-1 |
| LEDs | 282x APA102 (Dotstar) |
| RTC | DS1307 module |

### Pin Configuration

| Function | GPIO |
|----------|------|
| LED Data (APA102) | 6 |
| LED Clock (APA102) | 7 |
| I2C SDA (RTC) | 4 |
| I2C SCL (RTC) | 5 |

## 🚀 Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) IDE or CLI
- ESP32-C3 development board
- APA102 LED strip (282 LEDs)
- DS1307 RTC module

### Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/yourusername/dreamy-clock-esp32.git
   cd dreamy-clock-esp32
   ```

2. **Build and upload firmware**
   ```bash
   pio run -t upload
   ```

3. **Upload web interface files**
   ```bash
   pio run -t uploadfs
   ```

4. **Connect to the clock**
   - Connect to WiFi network: `the dreaming clock`
   - Open browser and navigate to `http://192.168.4.1` or `http://the-dreaming-clock.local`

## 📱 Web Interface

### Main Page
Large wakeup button to trigger the time display. Simply tap to see the current time!

### Settings Page (`/settings`)
- **Set Time** - Configure hours, minutes, and date
- **Active Hours** - Set display schedule per weekday
- **Wakeup Interval** - Configure automatic wakeup (5min to 6 hours)
- **Manual Wakeup** - Trigger immediate time display

## 🔌 REST API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/time` | GET | Get current time |
| `/api/time` | POST | Set time (hours, minutes, day, month, year) |
| `/api/active-hours` | GET | Get active hours configuration |
| `/api/active-hours` | POST | Set active hours per weekday |
| `/api/wakeup-interval` | GET | Get wakeup interval |
| `/api/wakeup-interval` | POST | Set wakeup interval |
| `/wakeup` | POST | Trigger manual wakeup |

### Example API Response

```json
{
  "success": true,
  "hours": 14,
  "minutes": 30,
  "day": 19,
  "month": 12,
  "year": 2024,
  "weekday": 4
}
```

## 📦 Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| [FastLED](https://github.com/FastLED/FastLED) | ^3.9.0 | APA102 LED control |
| [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) | GitHub | Async HTTP server |
| [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) | GitHub | TCP for ESP32 |
| [RTClib](https://github.com/adafruit/RTClib) | ^2.1.4 | DS1307 RTC driver |
| [ArduinoJson](https://arduinojson.org/) | ^7.0.0 | JSON serialization |

## 🔄 OTA Updates

After initial setup, firmware can be updated wirelessly:

```bash
pio run -t upload --upload-port the-dreaming-clock.local
```

**OTA Password:** `kei6yahghohngooS`

## 📁 Project Structure

```
dreamy-clock-esp32/
├── platformio.ini      # PlatformIO configuration
├── src/
│   ├── main.cpp        # Entry point
│   ├── settings.h      # Configuration & NVS persistence
│   ├── rtc.h           # RTC module control
│   ├── leds.h          # LED display logic
│   ├── segment.h       # Segment animation class
│   ├── network.h       # WiFi & Captive Portal
│   ├── ota.h           # OTA update handling
│   └── web.h           # REST API server
├── data/               # Web interface files (LittleFS)
│   ├── index.html
│   ├── settings.html
│   └── style.css
└── Agents.md           # Detailed module documentation
```

## 🎨 LED Segment Layout

Each digit uses a standard 7-segment layout:

```
    ┌───5───┐
    │       │
    4       6
    │       │
    ├───3───┤
    │       │
    0       2
    │       │
    └───1───┘
```

- 4 digits × 7 segments = 28 segments
- 1 colon segment (between digits 2 and 3)
- Each segment = 10 LEDs
- Colon = 2 LEDs
- **Total: 282 LEDs**

## 📝 License

This project is open source. Feel free to use, modify, and distribute.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

---

Made with ❤️ and lots of colorful LEDs
