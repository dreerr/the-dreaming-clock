# The Dreaming Clock

![The Dreaming Clock Video](./the-dreaming-clock-video.gif)

A beautiful ESP32-C3-based 7-segment LED clock with web interface, real-time clock module, and customizable display schedules.

![ESP32-C3](https://img.shields.io/badge/ESP32--C3-DevKitM--1-blue)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Framework-orange)
![License](https://img.shields.io/badge/License-MIT-green)

## ✨ Features

- **282 APA102 (Dotstar) LEDs** arranged as a 4-digit 7-segment display with
  colon — the per-segment count is one constant in `src/config.h`
- **Probability-driven animation** — segments decide after each cycle whether to
  keep glowing, so words condense out of the noise instead of fading in
- **Four light modes per segment** — constant, random gradient, pulse, blink
- **Live per-LED web preview** — all 282 LEDs, not an averaged approximation
- **Automatic time** via NTP with real timezone and DST handling; the DS1307 RTC
  is optional
- **Web-based configuration** via Captive Portal (no app required!)
- **Active Hours scheduling** - set when the display should be on/off per weekday
- **Auto-wakeup intervals** - display wakes up periodically to show the time
- **Over-The-Air (OTA) updates** - update firmware wirelessly
- **Persistent settings** stored in NVS (Non-Volatile Storage)

## 🎯 Display Modes

| Mode | Description |
|------|-------------|
| **Dream** | Drifting colour gradients with words condensing out of the noise |
| **Pattern** | The same drifting gradients, without words |
| **Wakeup** | Shows the current time for 15 seconds with a blinking colon |
| **Time Not Set** | Blinking "00:00" until the time is known |
| **Off** | Display off during configured inactive hours |

### 💭 Dream Words

During dream mode, the clock subtly displays words from a curated list. Words fade in and out with low opacity, creating an ethereal, barely-perceptible effect:

- **Ethereal**: HALO, HOPE, GLOW, FADE, SOFT, BLUR, HUSH, REST...
- **Calming**: SAFE, SANE, SURE, HELD, FEEL, SELF, SOLO, COOL...
- **Nature**: LEAF, FERN, POOL, SAND, LAND, GLEN...
- **German**: HASE, EGAL, FELD, GOLD, EULE, ERDE, PFAD...
- **Playful**: bEEp, bUbS, duSt, pUFF, FLoP...

## 🔧 Hardware Requirements

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-C3-DevKitM-1 |
| LEDs | 282x APA102 (Dotstar) |
| RTC | DS1307 module (optional — NTP works without it) |

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
- ESP32-C3 development board (4 MB flash)
- APA102 LED strip (282 LEDs)
- DS1307 RTC module (optional)
- Node.js 20+ — only if you want to rebuild the web frontend; the built output
  is committed

### Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/dreerr/the-dreaming-clock.git
   cd the-dreaming-clock
   ```

2. **Set an OTA password** (it is not stored in the repository)
   ```bash
   export CLOCK_OTA_PASSWORD='choose-something-long'
   ```

3. **Build and upload firmware over USB**
   ```bash
   pio run -t upload
   ```

4. **Upload the web interface**
   ```bash
   pio run -t uploadfs
   ```

5. **Connect to the clock**
   - Connect to WiFi network: `the dreaming clock`
   - Open `http://192.168.4.1` or `http://the-dreaming-clock.local`
   - Point it at your WiFi under Settings → Network, and the time syncs itself

> **⚠️ Upgrading from an earlier version?** The partition table changed
> (1.625 MB app slots, 640 KB LittleFS). That first update **must go over USB** —
> OTA cannot rewrite the partition table. Flash the firmware *and* the
> filesystem. OTA works normally afterwards. Your saved settings survive; the
> RTC now stores UTC rather than local time, so the display may be off by your
> UTC offset until NTP syncs or you set the time once.

### Development

```bash
pio test -e native          # host-side logic tests
cd web && npm install       # frontend toolchain (optional)
node build.mjs              # rebuild web/ -> data/
```

## 📱 Web Interface

### Main Page
Just the clock: a live preview of every LED, full-bleed on black. Tap it to show
the time. A live indicator and the settings toggle sit in the top-right corner.

### Settings
Slides in from the right; the clock shrinks beside it rather than being covered,
so a brightness or mode change is visible on the preview while it is being made.
- **Time** - Timezone (with DST), plus manual entry for offline use
- **Display** - Mode, and brightness for time and dream separately
- **Active Hours** - Schedule per weekday
- **Auto Wakeup** - How often the clock shows the time by itself
- **Network** - Access point or joining a network

## 🔌 REST API

One endpoint carries the whole device state, so there is a single document to
read and a single one to patch.

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/state` | GET | Complete device state |
| `/api/state` | POST | Apply settings (JSON body) |
| `/api/layout` | GET | Physical LED layout (counts and per-segment ranges) |
| `/api/timezones` | GET | Supported timezone names |
| `/api/wakeup` | POST | Show the time now |
| `/ws/leds` | WS | Live LED stream, 846 bytes/frame at 25 FPS |

```bash
curl -X POST http://the-dreaming-clock.local/api/state \
  -H 'Content-Type: application/json' \
  -d '{"brightness": 200, "wakeupInterval": 30, "mode": "dream"}'
```

Accepted keys: `brightness`, `dreamBrightness`, `timezone`, `wakeupInterval`,
`useActiveHours`, `days`, `time`, `mode`, `network`. Invalid values are refused
with a reason rather than silently ignored.

## 📦 Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| [FastLED](https://github.com/FastLED/FastLED) | ^3.10.3 | APA102 LED control |
| [ESPAsyncWebServer](https://github.com/mathieucarbou/ESPAsyncWebServer) | ^3.6.0 | Async HTTP server |
| [AsyncTCP](https://github.com/mathieucarbou/AsyncTCP) | ^3.3.2 | TCP for ESP32 |
| [RTClib](https://github.com/adafruit/RTClib) | ^2.1.4 | DS1307 RTC driver |
| [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO) | ^1.17.4 | I²C helper for RTClib |
| [ArduinoJson](https://arduinojson.org/) | ^7.4.2 | JSON serialization |

All dependencies are pinned; nothing resolves from PlatformIO's global library
store, so a fresh clone builds identically.

## 🔄 OTA Updates

After the first USB flash, firmware can be updated wirelessly:

```bash
export CLOCK_OTA_PASSWORD='your-password'
pio run -t upload --upload-port the-dreaming-clock.local
```

The password is a build flag, not a value in the repository. If you used an
earlier version, **rotate it** — the old one is in this repository's git
history.

## 📁 Project Structure

```
dreamy-clock-esp32/
├── platformio.ini      # Build config; every dependency pinned
├── partitions.csv      # 1.625 MB app slots + 640 KB LittleFS
├── src/
│   ├── main.cpp        # Entry point
│   ├── config.h        # Pins, LED counts, segment→LED mapping
│   ├── segment.*       # Probability-driven animation engine
│   ├── display.*       # Glyphs onto segments
│   ├── modes.*         # Mode state machine and timers
│   ├── leds.*          # FastLED setup and frame loop
│   ├── patterns.*      # 7-segment glyphs (host-testable)
│   ├── schedule.*      # Active hours, wakeup slots (host-testable)
│   ├── dreams.*        # Word list (host-testable)
│   ├── clock_time.*    # System clock, NTP, RTC
│   ├── timezones.*     # IANA → POSIX TZ mapping
│   ├── settings.*      # NVS persistence
│   ├── state.*         # Canonical state + command validation
│   ├── net_wifi.*      # WiFi and captive portal
│   ├── web.*           # HTTP API
│   ├── ws_preview.*    # WebSocket LED stream
│   └── ota.*           # OTA updates
├── web/                # Frontend source (esbuild → data/)
├── data/               # Build output, flashed to LittleFS
├── test/test_logic/    # Host-side unit tests
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

MIT. All dependencies are permissively licensed.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

---

Made with ❤️ and lots of colorful LEDs
