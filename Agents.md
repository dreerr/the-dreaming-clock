# The Dreaming Clock — Project Structure

An ESP32-C3 7-segment LED clock: 282 APA102 LEDs arranged as four digits and a
colon, driven by a probability-based animation engine, configured over a web
interface, with an optional DS1307 RTC.

## Layout

```
dreamy-clock-esp32/
├── platformio.ini        # Build config; every dependency pinned
├── partitions.csv        # 1.625 MB app slots + 640 KB LittleFS
├── src/                  # Firmware (one .h/.cpp pair per module)
├── web/                  # Frontend source (bundled into data/)
├── data/                 # Build output, flashed to LittleFS — committed
├── test/test_logic/      # Host-side unit tests
└── scripts/build_web.py  # PlatformIO pre-hook: rebuilds web/ -> data/
```

## Hardware

| Component | Description |
|-----------|-------------|
| MCU | ESP32-C3-DevKitM-1 (4 MB flash) |
| LEDs | 282× APA102 (DotStar) |
| RTC | DS1307 (optional — the clock runs on NTP without it) |

| Function | GPIO |
|----------|------|
| LED data | 6 |
| LED clock | 7 |
| I²C SDA | 4 |
| I²C SCL | 5 |

All of this lives in `src/config.h`, including `segmentLedStart()` — the single
definition of how the 29 segments map onto the strip. The colon sits physically
in the middle, so digit segments after it are offset by two LEDs. Both the
renderer and the web preview derive their mapping from that one function.

---

## Modules

Each module is a `.h` declaring the interface and a `.cpp` defining it. No
module defines globals in its header, so include order does not matter.

### `config.h`
Compile-time constants: pins, LED counts, hostname, OTA password (from the
`CLOCK_OTA_PASSWORD` build flag). `static_assert`s check that the segment→LED
mapping covers the strip exactly.

### `patterns` — 7-segment glyphs
`glyphFor(char)` returns the bit pattern for a character; `isRenderable(char)`
reports whether it has one. Arduino-free, so it is unit-tested on the host.

```
    ┌───5───┐
    │       │
    4       6
    │       │
    ├───3───┤
    │       │
    0       2
    │       │
    └───1───┘        bit order [6][5][4][3][2][1][0]
```

**A 7-segment cell has one glyph per letter, so case is not distinguishable.**
`'b'` and `'B'` return the same pattern. Mixed case in the word list is a note
about which shape is drawn, nothing more.

### `schedule` — active hours and wakeup slots
`isDisplayActiveTime()` (supports overnight windows) and
`minutesToNextWakeupSlot()`, which aligns wakeups to midnight. Aligning to
midnight rather than to the current minute is what makes intervals longer than
an hour work — `minute % 120` can never align, because `minute` never exceeds
59. Arduino-free and unit-tested.

### `segment` — the animation engine

The heart of the project. A segment is **driven by probability, not by
brightness**: the display layer sets a probability, and once per animation cycle
the segment rolls against it to decide whether it lights up for the next cycle.

| Input | Meaning |
|-------|---------|
| `probability` | 0 = never lit, 255 = always lit, in between = flickers in and out |
| `cycleMs` | length of one animation cycle (1–15 s while dreaming) |
| `fadeMs` | crossfade into the new cycle's target |
| `mode` | `CONSTANT`, `RANDOM_GRADIENT`, `PULSE`, `BLINK` |
| `color`, `brightness` | for the non-gradient modes |
| `hueBase`, `hueSpread` | hue range for `RANDOM_GRADIENT` |

Two methods, and the split between them matters:

- `tick(now)` is the **only** thing that touches scheduling state. Once per
  cycle it snapshots what is on screen, rolls the probability, and builds the
  new target.
- `render(now)` is a **pure function of the elapsed time** since the cycle
  began. It cannot reset the animation clock, which is what used to make
  crossfades freeze when a caller re-issued a colour every frame.

`probability == 255` is special-cased to mean *always*, because `random8() < 255`
still fails one cycle in 256 and the clock display has to be steady.

### `display` — glyphs onto segments
`setChar`, `setWord`, `setNumber`, and `setWordOverNoise` — the last of which
keeps a background probability on the segments *outside* the glyph, so a word
condenses out of the dream noise instead of being pasted over it.

### `dreams` — the word list
87 four-character words. `nextDreamWord()` picks one, avoiding an immediate
repeat. **This function is the seam for external data sources**: an HTTP- or
MQTT-fed word list replaces it without the mode layer changing. Arduino-free;
the tests assert every word is exactly four renderable characters (a missing
comma once concatenated two words into one).

### `modes` — the state machine

| Mode | Behaviour |
|------|-----------|
| `OFF` | Outside active hours; the strip is blanked |
| `TIME_NOT_SET` | 00:00 blinking, via the `BLINK` segment mode |
| `DREAM` | Drifting noise with words condensing out of it |
| `PATTERN` | Drifting noise, no words |
| `WAKEUP` | The time, shown solidly for 15 s |

Timers are three `Deadline` structs — a rollover-safe one-shot each for the
sleep timer, the auto-wakeup and the dream-word cycle. They replaced an external
`Timer` library that was unpinned, GPL-licensed, and handed out slot indices
that went stale after firing. `updateMode()` services them **in every mode**,
including `OFF`, so a wakeup scheduled during off-hours is not left pending.

### `clock_time` — time

The ESP32 system clock is the single time base and holds **UTC**. The DS1307 is
a seed at boot and a sink for NTP results, not a parallel source of truth. The
configured IANA timezone is mapped to a POSIX TZ string (`timezones.cpp`) and
applied with `setenv`/`tzset`, so changing it actually changes the display.

NTP runs whenever a network is available, regardless of whether the RTC is
present, and writes its result back to the RTC. `nowLocal()` is cached, so
calling it every frame costs nothing.

### `net_wifi` — WiFi

Captive portal (AP `the dreaming clock`, 192.168.4.1) or WiFi client. Fully
event-driven: association is started and the result arrives as a WiFi event, so
nothing blocks the main loop. `requestNetworkRestart()` defers the actual
teardown to `loop()` — it must never run inside the AsyncTCP task.

### `state` — the canonical document

`serializeState()` and `applyCommand()`. **Every transport reads and writes
through these two functions**, so validation lives in exactly one place and the
representations cannot drift. This is the seam a Home Assistant / MQTT
integration plugs into.

### `web` — HTTP API

| Route | Method | Description |
|-------|--------|-------------|
| `/api/state` | GET | The complete device state |
| `/api/state` | POST | Apply settings (JSON body) |
| `/api/timezones` | GET | Supported timezone names |
| `/api/wakeup` | POST | Show the time now |
| `/*` | GET | Static files from LittleFS |

Unknown `/api/*` paths return **404**, not a redirect to the UI — an
integration must be able to tell a typo from a success. Everything else
redirects to `/` for the captive portal.

`serveStatic` transparently serves a pre-compressed `<file>.gz`, which is what
the frontend build produces.

Example patch:

```bash
curl -X POST http://the-dreaming-clock.local/api/state \
  -H 'Content-Type: application/json' \
  -d '{"brightness": 200, "wakeupInterval": 30, "mode": "dream"}'
```

### `ws_preview` — live preview

`/ws/leds`, binary, **846 bytes per frame** (282 LEDs × 3 bytes) at 25 FPS —
the FastLED buffer sent verbatim. That is ~21 KB/s and *less* ESP work than the
old per-segment averaging.

Frames are dropped when `availableForWriteAll()` says no. Without that a stalled
client silently queues up to `WS_MAX_QUEUED_MESSAGES` (32 on ESP32) copies of
the frame — 27 KB of heap per client at this size.

Accepts the text commands `calibrate on` / `calibrate off`, which walk one lit
LED along the strip so the physical LED order inside each segment can be read
off (see below).

---

## Web frontend

Source in `web/`, bundled by esbuild into `data/`, which is **committed** so
`pio run -t uploadfs` works without a JS toolchain.

```bash
cd web && npm install && node build.mjs
```

Deliberately **no framework**. The frontend lives in LittleFS (640 KB, of which
it uses about 9 KB) — asset size is not a constrained resource here, so a
framework would cost flash and a toolchain to buy nothing. The preview is canvas
drawing, which a declarative DOM framework does not help with either.

The build does earn its place: bundling, minifying and pre-gzipping takes the
shipped payload from 34 KB to ~9 KB, and lets both pages share one API module.

| File | Purpose |
|------|---------|
| `web/js/preview.js` | Canvas LED renderer + WebSocket transport |
| `web/js/api.js` | `/api/state` wrapper |
| `web/js/index.js` | Home page: preview, wake button, calibration |
| `web/js/settings.js` | Settings page |
| `web/geometry.json` | Per-LED (x, y), derived from the segment SVG |

### Calibrating LED order

`geometry.json` says where each LED sits, but the SVG cannot say **which end of
a bar is LED 0**. Press *Calibrate LED order* on the home page: one LED walks
the strip while the readout names its segment and position. If a bar fills in
the opposite direction on screen from the physical clock, add that segment
number to `REVERSED_SEGMENTS` in `web/js/preview.js` and rebuild.

---

## Dependencies

| Library | Version |
|---------|---------|
| FastLED | ^3.10.3 |
| ESPAsyncWebServer (mathieucarbou) | ^3.6.0 |
| AsyncTCP (mathieucarbou) | ^3.3.2 |
| RTClib | ^2.1.4 |
| Adafruit BusIO | ^1.17.4 |
| ArduinoJson | ^7.4.2 |

Every one is pinned. Nothing may resolve from the global `~/.platformio/lib`
store — that is exactly how the old `Timer` dependency stayed invisible until
someone tried to build on a second machine.

`AsyncCallbackJsonWebHandler` is deliberately **not** used: ESPAsyncWebServer
does not declare ArduinoJson as a dependency, so its `AsyncJson.cpp` compiles
itself out via `__has_include` and the handler never links. `web.cpp`
accumulates the request body itself instead.

---

## Build and flash

```bash
pio test -e native            # host-side logic tests
pio run                       # build firmware
pio run -t upload             # flash over USB
pio run -t uploadfs           # flash the filesystem
pio run -t upload --upload-port the-dreaming-clock.local   # OTA
```

The OTA password comes from the environment:

```bash
export CLOCK_OTA_PASSWORD='...'
```

> **The partition table changed.** Moving to `partitions.csv` requires one flash
> **over USB**; OTA cannot rewrite the partition table. Flash both the firmware
> and the filesystem that first time. OTA works normally afterwards.
