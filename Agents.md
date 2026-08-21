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
in the middle, so digit segments after it are offset by the colon's LEDs.

### Changing the LED count

`LEDS_PER_SEGMENT` and `COLON_LEDS` in `src/config.h` are the only edit. Rebuild
and flash the firmware; nothing else needs touching.

- `NUM_LEDS`, `COLON_LED_START` and `MAX_SEG_LEDS` are derived, and the
  `static_assert`s re-check that the mapping is contiguous and covers the strip
  exactly. A count that does not work will fail the build, not the hardware.
- The web preview reads the layout from **`GET /api/layout`** at page load, so
  it adapts with no rebuild. `web/geometry.json` holds shapes only — a bar's
  outline does not depend on how many LEDs are inside it.
- `pio test -e native` sweeps the mapping across 1-32 LEDs per segment and 1-4
  colon LEDs, which is what makes the knob safe to turn.

Below about three LEDs per segment, `Segment::fillGradient()` falls back to a
two-stop gradient (`src/segment.cpp`) — correct for the colon, and a graceful
degradation for a very short bar.

---

## Modules

Each module is a `.h` declaring the interface and a `.cpp` defining it. No
module defines globals in its header, so include order does not matter.

### `config.h`
Compile-time constants: pins, LED counts, hostname, OTA password (from the
`CLOCK_OTA_PASSWORD` build flag). `static_assert`s check that the segment→LED
mapping covers the strip exactly.

### `layout` — segment to LED mapping
Parameterised `constexpr` maths (`segmentLedStartFor`, `numLedsFor`, …) that
`config.h` binds to the configured counts. Kept separate and Arduino-free so the
host tests can sweep every LED count in a single binary — a `constexpr` in
`config.h` only ever has one value.

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
| `mode` | colour: `CONSTANT`, `PULSE`, `BLINK` · gradient: `RANDOM_GRADIENT`, `SWEEP`, `BLOOM` |
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

The modes come in two families. The **colour** modes paint the flat `color`; the
**gradient** modes paint a fresh random gradient and differ only in the spatial
envelope they move across it — `SWEEP` runs a soft comet end to end and back,
`BLOOM` grows light from the centre to both ends and then wipes it away with
darkness growing the same way. All the enveloped modes cross-fade in from
whatever the previous cycle was showing, so changing animation is a hand-over
rather than a cut.

### `animation` — the maths behind all of it
Waves, the dream palette and the spatial envelopes, kept Arduino-free so it is
host-tested. `dreamHueSpread()` breathes between near-monochrome and the whole
wheel over four minutes; it sets how far a *segment's* hue centre may wander
from the theme. Each segment then picks its own spread and stop count on top of
that, so one bar reads as a single colour while its neighbour is a rainbow.

The envelopes are checked for seams as well as shape: a test walks every phase
of every envelope for every LED and fails on any step over 40/255, including
across the cycle wrap. Bloom originally punched a full-depth notch into the
middle of the bar the instant it turned from growing to erasing — visible on
the strip as a jump of 148/255, invisible to a test that only looked at one
phase at a time.

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
| `MESSAGE` | A queued message, playing over everything else |

Timers are three `Deadline` structs — a rollover-safe one-shot each for the
sleep timer, the auto-wakeup and the dream-word cycle. They replaced an external
`Timer` library that was unpinned, GPL-licensed, and handed out slot indices
that went stale after firing. `updateMode()` services them **in every mode**,
including `OFF`, so a wakeup scheduled during off-hours is not left pending.

### `messages` — text on demand

A queue of up to eight messages, held in RAM: a message is a moment, not a
setting. Each carries its own effect (`scroll`, `appear`, `blink`), fill (any
`SegmentMode`), hue, step length, repeat count and whether steps glide or snap.

`MESSAGE` is tested **first** in `updateMode()`, before the time-not-set and
active-hours branches — a message is an explicit act, so it lights the display
whatever the clock is doing, then hands it back to whatever it interrupted,
including going back to off.

The display is four seven-segment cells with no sub-character resolution, so a
scroll steps the text through those cells one character at a time; the glide is
the segments' own cross-fade. Spaces are content — "   2" means three dark cells
and a 2, and an all-blank message is a beat between two others.

A segment only acts on a new probability when its cycle turns over, which is far
too slow for text. `renderMessage()` therefore re-rolls every digit segment the
moment the glyphs change; without that the first steps show whatever the
previous roll left lit. The step maths lives behind `messageStepCount()`,
`messageWindowAt()` and `messageLevelAt()`, kept Arduino-free and host-tested —
"what do the four cells show at step k" is where the bugs are.

Dimming in and out rides on segment brightness, which is applied at render time,
so the ramp is immediate rather than waiting for a cycle boundary.

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
| `/api/layout` | GET | Physical LED layout (see below) |
| `/api/timezones` | GET | Supported timezone names |
| `/api/wakeup` | POST | Show the time now |
| `/api/message` | POST | Queue one message, or an array played in order |
| `/api/message` | DELETE | Cancel the queue and hand the display back |
| `/*` | GET | Static files from LittleFS |

Unknown `/api/*` paths return **404**, not a redirect to the UI — an
integration must be able to tell a typo from a success. Everything else
redirects to `/` for the captive portal.

`serveStatic` transparently serves a pre-compressed `<file>.gz`, which is what
the frontend build produces.

`/api/layout` publishes the LED mapping — `ledsPerSegment`, `colonLeds`,
`numLeds`, and an explicit `{start, count}` for each of the 29 segments. It
exists so consumers read the results of `segmentLedStart()` rather than
reimplementing it, which is what lets the count change without a frontend
rebuild. Any external consumer of `/ws/leds` needs it too.

Example patch:

```bash
curl -X POST http://the-dreaming-clock.local/api/state \
  -H 'Content-Type: application/json' \
  -d '{"brightness": 200, "wakeupInterval": 30, "mode": "dream"}'
```

### `ws_preview` — live preview

`/ws/leds`, binary, `NUM_LEDS × 3` bytes per frame (846 at the shipped count) at
25 FPS — the FastLED buffer sent verbatim. That is ~21 KB/s and *less* ESP work
than the old per-segment averaging. The frame carries no header; `/api/layout`
describes the mapping, so clients do not bake the count in.

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
| `web/js/index.js` | Page shell: preview, tap-to-wake, panel, calibration |
| `web/js/settings.js` | Settings panel controls (a module, not a page) |
| `web/geometry.json` | Segment shapes and gradient axes (no LED counts) |
| `web/clock.svg` | The artwork; source of truth for the shapes |
| `scripts/make_geometry.py` | Regenerates `geometry.json` from the SVG |

### Calibrating LED order

`geometry.json` says where each bar is and `/api/layout` says which LEDs are in
it, but only the hardware knows **which end of a bar is LED 0**. Press
Settings -> Developer -> Calibrate LED order: one LED walks the strip while the
readout names its segment and position. If a bar fills in the opposite direction
on screen from the physical clock, adjust `segmentIsReversed()` in
`src/config.h` and reflash — the preview reads it from `/api/layout`.

On this build, positions 0 (lower left), 3 (middle) and 6 (upper right) run
backwards on every digit, measured this way.

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
