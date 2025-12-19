# The Dreaming Clock - Projektstruktur

Eine ESP32-C3-basierte 7-Segment LED-Uhr mit Web-Interface, RTC-Modul und persistierten Einstellungen.

## Übersicht

```
dreamy-clock-esp32/
├── platformio.ini          # PlatformIO Konfiguration
├── src/                    # Quellcode
│   ├── main.cpp            # Hauptprogramm (Setup & Loop)
│   ├── definitions.h       # Globale Konstanten und Variablen
│   ├── settings.h          # Persistierte Einstellungen (NVS)
│   ├── rtc.h               # RTC-Modul (DS1307) Steuerung
│   ├── leds.h              # LED-Anzeige & 7-Segment Logik
│   ├── segment.h           # Segment-Klasse für Animationen
│   ├── network.h           # WiFi & Captive Portal
│   ├── ota.h               # Over-The-Air Updates
│   └── web.h               # REST API Webserver
├── data/                   # LittleFS Dateisystem (Web-Frontend)
│   ├── index.html          # Startseite mit Wakeup-Button
│   ├── settings.html       # Settings-Seite (Zeit, Active Hours, Wakeup Interval)
│   ├── style.css           # Styling
│   └── script.js           # (leer, JS ist inline in HTML)
└── lib/                    # Private Libraries (leer)
```

---

## Hardware

| Komponente | Beschreibung |
|------------|--------------|
| **MCU** | ESP32-C3-DevKitM-1 |
| **LEDs** | 282x APA102 (Dotstar) in 7-Segment Anordnung |
| **RTC** | DS1307 Echtzeituhr-Modul |

### Pin-Belegung

| Funktion | GPIO |
|----------|------|
| LED Data (APA102) | 6 |
| LED Clock (APA102) | 7 |
| I2C SDA (RTC) | 4 |
| I2C SCL (RTC) | 5 |

---

## Module

### main.cpp
**Einstiegspunkt** - Initialisiert alle Subsysteme und führt die Hauptschleife aus.

```
setup() → RTC → Settings → Network → OTA → Web → LEDs
loop()  → Network → OTA → LEDs
```

### definitions.h
**Globale Konfiguration** - Enthält alle konfigurierbaren Konstanten:

| Konstante | Wert | Beschreibung |
|-----------|------|--------------|
| `AP_SSID` | "the dreaming clock" | WiFi Access Point Name |
| `HOSTNAME` | "the-dreaming-clock" | mDNS Hostname |
| `USE_CAPTIVE` | true | Captive Portal aktivieren |

**Globale Variablen:**
- `wakeup` - Flag zum "Aufwecken" der Anzeige
- `timeWasSet` - Ob die RTC-Zeit gesetzt wurde

### settings.h
**Persistierte Einstellungen** - Verwaltet Einstellungen mit ESP32 Preferences (NVS).

**Datenstrukturen:**
```cpp
struct DaySchedule {
  bool enabled;      // Tag aktiv?
  uint8_t startHour; // Startzeit (0-23)
  uint8_t endHour;   // Endzeit (0-23)
};

struct ClockSettings {
  bool useActiveHours;    // Active Hours Feature aktiviert?
  DaySchedule days[7];    // Zeitplan pro Wochentag (0=So, 1=Mo, ...)
  int wakeupInterval;     // Auto-Wakeup in Minuten (0=aus)
};
```

| Funktion | Beschreibung |
|----------|--------------|
| `setupSettings()` | Lädt Einstellungen aus NVS |
| `saveSettings()` | Speichert alle Einstellungen |
| `saveActiveHours()` | Speichert nur Active Hours |
| `saveWakeupInterval()` | Speichert nur Wakeup Interval |
| `isDisplayActiveTime(hour, weekday)` | Prüft ob Display zur Zeit aktiv sein soll |

**Wakeup Intervalle:**
- 0 = Aus
- 5, 15, 30 = Minuten
- 60, 120, 180, 240, 360 = Stunden (1-6h)

### rtc.h
**Echtzeituhr** - Verwaltet das DS1307 RTC-Modul via I2C.

| Funktion | Beschreibung |
|----------|--------------|
| `setupRTC()` | Initialisiert I2C und prüft ob RTC läuft |
| `setRTCTime(h, m, s, d, mo, y)` | Setzt Datum und Uhrzeit |
| `rtc.now()` | Gibt aktuelles DateTime-Objekt zurück |

### leds.h
**LED-Steuerung** - Kernlogik für die 7-Segment Anzeige.

**Struktur:**
- 4 Ziffern × 7 Segmente = 28 Segmente
- 1 Doppelpunkt = 1 Segment (Index 28)
- Jedes Segment = 10 LEDs (Doppelpunkt = 2 LEDs)

| Funktion | Beschreibung |
|----------|--------------|
| `setupLEDs()` | Initialisiert FastLED und Segmente, startet Auto-Wakeup |
| `loopLEDs()` | Hauptschleife (60 FPS begrenzt) |
| `setDigit(pos, value, opacity)` | Setzt eine Ziffer (0-9) |
| `setNumber(value, opacity)` | Setzt 4-stellige Zahl |
| `showCurrentTime()` | Zeigt aktuelle Uhrzeit an |
| `goSleep()` | Wechselt in Random-Modus |
| `scheduleAutoWakeup()` | Plant nächstes automatisches Aufwachen |

**Verhalten:**
1. **Zeit nicht gesetzt**: Blinkendes "00:00"
2. **Außerhalb Active Hours**: Display aus (basierend auf Settings)
3. **Wakeup-Modus**: Zeigt Zeit für 15 Sekunden, dann zurück zu Random
4. **Random-Modus**: Zufällige Farbgradienten auf allen Segmenten
5. **Auto-Wakeup**: Automatisches Aufwachen basierend auf Intervall-Setting

### segment.h
**Segment-Klasse** - Animationslogik für einzelne LED-Segmente.

| Eigenschaft | Beschreibung |
|-------------|--------------|
| `opacity` | Helligkeit (0-255) |
| `speed` | Animationsgeschwindigkeit (1-4) |
| `mode` | `RANDOM` oder `COLOR` |

| Methode | Beschreibung |
|---------|--------------|
| `fillColor(color, speed)` | Füllt Segment mit Farbe |
| `fillRandomGradient()` | Generiert zufälligen Farbverlauf |
| `draw()` | Zeichnet aktuellen Frame (Blend-Animation) |

**Animation:** Sanftes Überblenden zwischen `current` und `target` Farben mittels `quadwave8()`.

### network.h
**Netzwerk** - WiFi Access Point mit Captive Portal.

| Modus | Beschreibung |
|-------|--------------|
| **Captive** | Erstellt AP "the dreaming clock", leitet alle Anfragen um |
| **Client** | Verbindet zu konfigurierten WiFi-Netzwerken |

- IP: `192.168.4.1`
- DNS-Server für Captive Portal
- mDNS: `the-dreaming-clock.local`

### ota.h
**Over-The-Air Updates** - Firmware-Updates via WiFi.

- Port: 3232
- Passwort: `kei6yahghohngooS`
- Upload via: `the-dreaming-clock.local`

### web.h
**REST API Webserver** - Async HTTP Server auf Port 80 mit JSON Responses.

| Route | Methode | Beschreibung |
|-------|---------|--------------|
| `/` | GET | index.html |
| `/settings` | GET | Settings-Seite |
| `/api/time` | GET | Aktuelle Zeit als JSON |
| `/api/time` | POST | Zeit setzen (hours, minutes, day, month, year) |
| `/api/active-hours` | GET | Active Hours Einstellungen als JSON |
| `/api/active-hours` | POST | Active Hours speichern |
| `/api/wakeup-interval` | GET | Wakeup Interval als JSON |
| `/api/wakeup-interval` | POST | Wakeup Interval speichern |
| `/wakeup` | POST | Manuelles Aufwecken |
| `/*` | GET | Statische Dateien aus LittleFS |

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

## Web-Frontend

### index.html
Startseite mit großem SVG-Button (Wecker-Symbol) zum "Aufwecken" der Uhr.
- Klick sendet POST an `/wakeup`
- Link zu Settings-Seite (`/adjust`)

### settings.html
Vollständige Settings-Seite mit AJAX-Speicherung:

**Sektionen:**
1. **Set Time** - Stunden, Minuten, Tag, Monat, Jahr
2. **Active Hours** - Pro Wochentag (Mo-So) aktivierbar mit Start/End Zeit
3. **Auto Wakeup Interval** - Dropdown mit 5/15/30 Min, 1-6 Stunden, Aus
4. **Manual Wakeup** - Button zum sofortigen Aufwecken

**Features:**
- Jede Sektion lädt beim Seitenaufruf die aktuellen Werte
- Jede Sektion hat eigenen Save-Button
- Visuelles Feedback bei Erfolg (✓) oder Fehler (✗)
- Alle Anfragen via `fetch()` API

### style.css
Modernes Dark-Theme mit:
- Gradient Background
- Card-basiertes Layout
- Responsive Design
- Animierte Buttons
- Tabellen-Layout für Wochenplan

---

## Ablaufdiagramm

```
                    ┌─────────────┐
                    │   BOOT      │
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
   │ Zeit    │      │ Random  │       │ Wakeup  │
   │ nicht   │      │ Modus   │       │ Modus   │
   │ gesetzt │      │         │       │ (15s)   │
   └─────────┘      └─────────┘       └─────────┘
                          │
                    ┌─────┴─────┐
                    │Auto-Wakeup│
                    │ (Interval)│
                    └───────────┘
```

---

## Dependencies (platformio.ini)

| Library | Version | Verwendung |
|---------|---------|------------|
| FastLED | ^3.9.0 | APA102 LED-Steuerung |
| ESPAsyncWebServer | GitHub | Async HTTP Server |
| AsyncTCP | GitHub | TCP für ESP32 |
| RTClib | ^2.1.4 | DS1307 RTC Treiber |
| ArduinoJson | ^7.0.0 | JSON Serialisierung für REST API |

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
