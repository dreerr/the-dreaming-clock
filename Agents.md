# The Dreaming Clock - Project Structure

An ESP32-C3-based 7-segment LED clock with web interface, RTC module, and persistent settings.

## Overview

```
dreamy-clock-esp32/
├── platformio.ini          # PlatformIO configuration
├── src/                    # Source code
│   ├── main.cpp            # Main program (Setup & Loop)
│   ├── settings.h          # Constants, global variables & persistent settings
│   ├── rtc.h               # RTC module (DS1307) control
│   ├── leds.h              # LED display & 7-segment logic
│   ├── segment.h           # Segment class for animations
│   ├── network.h           # WiFi & Captive Portal
│   ├── ota.h               # Over-The-Air Updates
│   └── web.h               # REST API Webserver
├── data/                   # LittleFS filesystem (Web frontend)
│   ├── index.html          # Homepage with wakeup button
│   ├── settings.html       # Settings page (time, active hours, wakeup interval)
│   ├── style.css           # Styling
│   └── script.js           # (empty, JS is inline in HTML)
└── lib/                    # Private libraries (empty)
```

---

## Hardware

| Component | Description |
|-----------|-------------|
| **MCU** | ESP32-C3-DevKitM-1 |
| **LEDs** | 282x APA102 (Dotstar) in 7-segment arrangement |
| **RTC** | DS1307 Real-Time Clock module |

### Pin Assignment

| Function | GPIO |
|----------|------|
| LED Data (APA102) | 6 |
| LED Clock (APA102) | 7 |
| I2C SDA (RTC) | 4 |
| I2C SCL (RTC) | 5 |

---

## Modules

### main.cpp
**Entry point** - Initializes all subsystems and runs the main loop.

```
setup() → RTC → Settings → Network → OTA → Web → LEDs
loop()  → Network → OTA → LEDs
```

**Defines global variables:**
- `wakeup` - Flag to "wake up" the display
- `timeWasSet` - Whether the RTC time has been set

### settings.h
**Constants, global variables & persistent settings** - Central configuration file.

**Global Constants:**

| Constant | Value | Description |
|----------|-------|-------------|
| `AP_SSID` | "the dreaming clock" | WiFi Access Point name |
| `HOSTNAME` | "the-dreaming-clock" | mDNS hostname |
| `HTTPHOST` | "http://the-dreaming-clock.local" | HTTP URL |
| `USE_CAPTIVE` | true | Enable Captive Portal |

**Global Variables (extern):**
- `wakeup` - Flag to "wake up" the display
- `timeWasSet` - Whether the RTC time has been set

**Data Structures:**
```cpp
struct DaySchedule {
  bool enabled;      // Day active?
  uint8_t startHour; // Start time (0-23)
  uint8_t endHour;   // End time (0-23)
};

struct ClockSettings {
  bool useActiveHours;    // Active Hours feature enabled?
  DaySchedule days[7];    // Schedule per weekday (0=Sun, 1=Mon, ...)
  int wakeupInterval;     // Auto-wakeup in minutes (0=off)
};
```

| Function | Description |
|----------|-------------|
| `setupSettings()` | Loads settings from NVS |
| `saveSettings()` | Saves all settings |
| `saveActiveHours()` | Saves only Active Hours |
| `saveWakeupInterval()` | Saves only Wakeup Interval |
| `isDisplayActiveTime(hour, weekday)` | Checks if display should be active at this time |

**Wakeup Intervals:**
- 0 = Off
- 5, 15, 30 = Minutes
- 60, 120, 180, 240, 360 = Hours (1-6h)

### rtc.h
**Real-Time Clock** - Manages the DS1307 RTC module via I2C.

| Function | Description |
|----------|-------------|
| `setupRTC()` | Initializes I2C and checks if RTC is running |
| `setRTCTime(h, m, s, d, mo, y)` | Sets date and time |
| `rtc.now()` | Returns current DateTime object |

### leds.h
**LED Control** - Core logic for the 7-segment display.

**Structure:**
- 4 digits × 7 segments = 28 segments
- 1 colon = 1 segment (index 28)
- Each segment = 10 LEDs (colon = 2 LEDs)

| Function | Description |
|----------|-------------|
| `setupLEDs()` | Initializes FastLED and segments, starts auto-wakeup |
| `loopLEDs()` | Main loop (limited to 60 FPS) |
| `setDigit(pos, value, opacity)` | Sets a digit (0-9) |
| `setNumber(value, opacity)` | Sets 4-digit number |
| `showCurrentTime()` | Displays current time |
| `goSleep()` | Switches to random mode |
| `scheduleAutoWakeup()` | Schedules next automatic wakeup |

**Behavior:**
1. **Time not set**: Blinking "00:00"
2. **Outside Active Hours**: Display off (based on settings)
3. **Wakeup mode**: Shows time for 15 seconds, then back to random
4. **Random mode**: Random color gradients on all segments
5. **Auto-wakeup**: Automatic wakeup based on interval setting

### segment.h
**Segment Class** - Animation logic for individual LED segments.

| Property | Description |
|----------|-------------|
| `opacity` | Brightness (0-255) |
| `speed` | Animation speed (1-4) |
| `mode` | `RANDOM` or `COLOR` |

| Method | Description |
|--------|-------------|
| `fillColor(color, speed)` | Fills segment with color |
| `fillRandomGradient()` | Generates random color gradient |
| `draw()` | Draws current frame (blend animation) |

**Animation:** Smooth blending between `current` and `target` colors using `quadwave8()`.

### network.h
**Network** - WiFi Access Point with Captive Portal.

| Mode | Description |
|------|-------------|
| **Captive** | Creates AP "the dreaming clock", redirects all requests |
| **Client** | Connects to configured WiFi networks |

- IP: `192.168.4.1`
- DNS server for Captive Portal
- mDNS: `the-dreaming-clock.local`

### ota.h
**Over-The-Air Updates** - Firmware updates via WiFi.

- Port: 3232
- Password: `kei6yahghohngooS`
- Upload via: `the-dreaming-clock.local`

### web.h
**REST API Webserver** - Async HTTP server on port 80 with JSON responses.

| Route | Method | Description |
|-------|--------|-------------|
| `/` | GET | index.html |
| `/settings` | GET | Settings page |
| `/api/time` | GET | Current time as JSON |
| `/api/time` | POST | Set time (hours, minutes, day, month, year) |
| `/api/active-hours` | GET | Active Hours settings as JSON |
| `/api/active-hours` | POST | Save Active Hours |
| `/api/wakeup-interval` | GET | Wakeup Interval as JSON |
| `/api/wakeup-interval` | POST | Save Wakeup Interval |
| `/wakeup` | POST | Manual wakeup |
| `/*` | GET | Static files from LittleFS |

**API Response Format:**
```json
{
  "success": true,
  "message": "Optional message"
}
```

**GET /api/time Response:**
```json
{
  "success": true,
  "hours": 14,
  "minutes": 30,
  "day": 15,
  "month": 6,
  "year": 2024,
  "weekday": 6
}
```

**GET /api/active-hours Response:**
```json
{
  "success": true,
  "enabled": true,
  "days": [
    {"enabled": false, "start": 8, "end": 18},
    {"enabled": true, "start": 8, "end": 18},
    ...
  ]
}
```

**GET /api/wakeup-interval Response:**
```json
{
  "success": true,
  "interval": 30
}
```

---

## Web Frontend

### index.html
Homepage with large SVG button (alarm clock symbol) to "wake up" the clock.
- Click sends POST to `/wakeup`
- Link to settings page (`/adjust`)

### settings.html
Complete settings page with AJAX saving:

**Sections:**
1. **Set Time** - Hours, minutes, day, month, year
2. **Active Hours** - Per weekday (Mon-Sun) with start/end time
3. **Auto Wakeup Interval** - Dropdown with 5/15/30 min, 1-6 hours, off
4. **Manual Wakeup** - Button for immediate wakeup

**Features:**
- Each section loads current values on page load
- Each section has its own save button
- Visual feedback on success (✓) or error (✗)
- All requests via `fetch()` API

### style.css
Modern dark theme with:
- Gradient background
- Card-based layout
- Responsive design
- Animated buttons
- Table layout for weekly schedule

---

## Flow Diagram

```
                    ┌─────────────┐
                    │    BOOT     │
                    └──────┬──────┘
                           │
   ┌───────────────────────┼───────────────────────┐
   ▼                       ▼                       ▼
┌────────┐          ┌───────────┐           ┌──────────┐
│  RTC   │          │ Settings  │           │ Network  │
│ DS1307 │          │   NVS     │           │  WiFi AP │
└────┬───┘          └─────┬─────┘           └────┬─────┘
     │                    │                      │
     └────────────────────┼──────────────────────┘
                          ▼
                   ┌─────────────┐
                   │  REST API   │
                   │  Webserver  │
                   └──────┬──────┘
                          │
                          ▼
                   ┌─────────────┐
                   │    LEDs     │
                   │  282x APA102│
                   └──────┬──────┘
                          │
        ┌─────────────────┼─────────────────┐
        ▼                 ▼                 ▼
   ┌─────────┐      ┌─────────┐       ┌─────────┐
   │  Time   │      │ Random  │       │ Wakeup  │
   │  not    │      │  Mode   │       │  Mode   │
   │  set    │      │         │       │ (15s)   │
   └─────────┘      └─────────┘       └─────────┘
                          │
                    ┌─────┴─────┐
                    │Auto-Wakeup│
                    │ (Interval)│
                    └───────────┘
```

---

## Dependencies (platformio.ini)

| Library | Version | Usage |
|---------|---------|-------|
| FastLED | ^3.9.0 | APA102 LED control |
| ESPAsyncWebServer | GitHub | Async HTTP Server |
| AsyncTCP | GitHub | TCP for ESP32 |
| RTClib | ^2.1.4 | DS1307 RTC driver |
| ArduinoJson | ^7.0.0 | JSON serialization for REST API |

---

## Build & Upload

```bash
# Build
pio run

# Upload via USB
pio run -t upload

# Upload via OTA
pio run -t upload --upload-port the-dreaming-clock.local

# Filesystem Upload
pio run -t uploadfs
```
