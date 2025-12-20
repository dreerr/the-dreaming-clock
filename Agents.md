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
│   ├── leds.h              # LED setup & main loop (includes display modes)
│   ├── display.h           # Display functions (setChar, setDigit, setNumber)
│   ├── patterns.h          # 7-segment patterns for digits & letters
│   ├── dreams.h            # Dream words & subliminal message system
│   ├── wakeup.h            # Wakeup/sleep logic & auto-wakeup timer
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
**LED Control** - Main LED setup and loop, includes sub-modules.

**Constants:**

| Constant | Value | Description |
|----------|-------|-------------|
| `FRAMES_PER_SECOND` | 60 | Frame rate limit |
| `DATA_PIN` | 6 | APA102 data GPIO |
| `CLOCK_PIN` | 7 | APA102 clock GPIO |
| `NUM_LEDS` | 282 | Total LED count |
| `NUM_SEGMENTS` | 29 | 4 digits × 7 + colon |
| `LEDS_PER_SEGMENT` | 10 | LEDs per digit segment |
| `COLON_LEDS` | 2 | LEDs in colon |
| `COLON_INDEX` | 28 | Segment index for colon |

**Display Modes (enum DisplayMode):**

| Mode | Description |
|------|-------------|
| `MODE_OFF` | Display is off (outside active hours) |
| `MODE_TIME_NOT_SET` | Blinking "00:00" to indicate RTC needs configuration |
| `MODE_DREAM` | Random colors with subliminal dream words |
| `MODE_WAKEUP` | Showing current time clearly for 15 seconds |

| Function | Description |
|----------|-------------|
| `setupLEDs()` | Initializes FastLED and segments, starts in dream mode |
| `loopLEDs()` | Main loop (limited to 60 FPS), handles mode switching |
| `enterDreamMode()` | Transitions to dream mode with random patterns |
| `enterWakeupMode()` | Transitions to wakeup mode showing time |
| `handleTimeNotSet()` | Mode handler: blinking zeros |
| `handleDreamMode()` | Mode handler: random patterns + dream words |
| `handleWakeupMode()` | Mode handler: show current time |

**Dream Word Functions:**

| Function | Description |
|----------|-------------|
| `setDreamWord(word, opacity)` | Sets a word on display with given opacity |
| `startDreamWord()` | Starts showing a random dream word |
| `endDreamWord()` | Ends current word, returns to pure random |
| `updateDreamWord()` | Updates dream word animation (called every frame) |

**Behavior:**
1. **Time not set**: Blinking "00:00"
2. **Outside Active Hours**: Display off (based on settings)
3. **Dream mode**: Random color gradients with subliminal words fading in/out
4. **Wakeup mode**: Shows time for 15 seconds, then back to dream mode
5. **Auto-wakeup**: Automatic wakeup based on interval setting

### patterns.h
**7-Segment Patterns** - Bit patterns for displaying digits and letters.

**Layout:**
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

| Data | Description |
|------|-------------|
| `segmentPatterns[]` | Bit patterns for 0-9 and A-Z |

| Function | Description |
|----------|-------------|
| `getPatternIndex(char)` | Returns pattern index for a character |

### display.h
**Display Functions** - High-level functions to show content on the display.

| Function | Description |
|----------|-------------|
| `setChar(pos, char, opacity)` | Sets a character (0-9, A-Z) at position |
| `setDigit(pos, value, opacity)` | Sets a digit (0-9) at position |
| `setNumber(value, opacity)` | Sets 4-digit number |
| `showCurrentTime()` | Displays current time with blinking colon |

### wakeup.h
**Wakeup/Sleep Logic** - Controls display wakeup and sleep states.

**Constants:**

| Constant | Value | Description |
|----------|-------|-------------|
| `WAKEUP_DURATION_MS` | 15000 | Time display stays on after wakeup |

| Function | Description |
|----------|-------------|
| `goSleep()` | Calls `enterDreamMode()` to return to dream state |
| `triggerAutoWakeup()` | Triggers a wakeup event |
| `scheduleAutoWakeup()` | Schedules next automatic wakeup |
| `startSleepTimer()` | Starts timer to go back to sleep |

### dreams.h
**Dream Words** - Subliminal messages displayed during dream phase.

**Word Selection:**
Only characters that display well on 7-segment displays are used:
- Uppercase: A, C, E, F, G, H, L, O, P, S, U
- Lowercase (distinct shapes): b, c, d, n, o, r, t, u

**Word Categories:**
- Ethereal & Dreamy (HALO, HOPE, GLOW, FADE, etc.)
- Calming (SAFE, SANE, SURE, HELD, etc.)
- Abstract (LOOP, SEED, FLEE, FUSE, etc.)
- Playful lowercase mix (bEEp, bUbS, duSt, etc.)
- Nature-ish (SunS, LEAF, FERN, POOL, etc.)
- German words (HASE, EGAL, FELD, GOLD, etc.)

**Configuration Constants:**

| Constant | Value | Description |
|----------|-------|-------------|
| `DREAM_WORD_MAX_OPACITY` | 80 | Maximum brightness (0-255) |
| `DREAM_WORD_MIN_OPACITY` | 15 | Minimum brightness ("barely visible") |
| `DREAM_WORD_DISPLAY_MS` | 4000 | Duration word is visible (ms) |
| `DREAM_WORD_PAUSE_MS` | 8000 | Pause between words (ms) |
| `DREAM_WORD_FADE_SPEED` | 1 | Animation speed (1-4) |
| `DREAM_WORD_PROBABILITY` | 180 | Chance to show word (0-255) |

| Function | Description |
|----------|-------------|
| `getRandomDreamWord()` | Returns a random word from the list |

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
   │  Time   │      │  Dream  │       │ Wakeup  │
   │  not    │      │  Mode   │       │  Mode   │
   │  set    │      │ +Words  │       │ (15s)   │
   └─────────┘      └─────────┘       └─────────┘
                          │
              ┌───────────┼───────────┐
              ▼           │           ▼
        ┌──────────┐      │     ┌───────────┐
        │  Dream   │      │     │Auto-Wakeup│
        │  Words   │      │     │ (Interval)│
        │ Fade In/ │      │     └───────────┘
        │   Out    │      │
        └──────────┘      │
              │           │
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
