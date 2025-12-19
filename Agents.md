# The Dreaming Clock - Projektstruktur

Eine ESP32-C3-basierte 7-Segment LED-Uhr mit Web-Interface und RTC-Modul.

## Übersicht

```
dreamy-clock-esp32/
├── platformio.ini          # PlatformIO Konfiguration
├── src/                    # Quellcode
│   ├── main.cpp            # Hauptprogramm (Setup & Loop)
│   ├── definitions.h       # Globale Konstanten und Variablen
│   ├── rtc.h               # RTC-Modul (DS1307) Steuerung
│   ├── leds.h              # LED-Anzeige & 7-Segment Logik
│   ├── segment.h           # Segment-Klasse für Animationen
│   ├── network.h           # WiFi & Captive Portal
│   ├── ota.h               # Over-The-Air Updates
│   └── web.h               # Webserver & API
├── data/                   # LittleFS Dateisystem (Web-Frontend)
│   ├── index.html          # Hauptseite mit Wakeup-Button
│   ├── adjust.html         # Formular zum Zeit einstellen
│   ├── adjusted.html       # Bestätigungsseite
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
setup() → RTC → Network → OTA → Web → LEDs
loop()  → Network → OTA → LEDs
```

### definitions.h
**Globale Konfiguration** - Enthält alle konfigurierbaren Konstanten:

| Konstante | Wert | Beschreibung |
|-----------|------|--------------|
| `AP_SSID` | "the dreaming clock" | WiFi Access Point Name |
| `HOSTNAME` | "the-dreaming-clock" | mDNS Hostname |
| `USE_CAPTIVE` | true | Captive Portal aktivieren |
| `ONLY_OFFICE_HOURS` | true | Display nur Mo-Fr 9-17 Uhr |
| `HOUR_START` / `HOUR_END` | 8 / 18 | Bürozeiten |

**Globale Variablen:**
- `wakeup` - Flag zum "Aufwecken" der Anzeige
- `timeWasSet` - Ob die RTC-Zeit gesetzt wurde

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
| `setupLEDs()` | Initialisiert FastLED und Segmente |
| `loopLEDs()` | Hauptschleife (60 FPS begrenzt) |
| `setDigit(pos, value, opacity)` | Setzt eine Ziffer (0-9) |
| `setNumber(value, opacity)` | Setzt 4-stellige Zahl |
| `showCurrentTime()` | Zeigt aktuelle Uhrzeit an |
| `goSleep()` | Wechselt in Random-Modus |

**Verhalten:**
1. **Zeit nicht gesetzt**: Blinkendes "00:00"
2. **Außerhalb Bürozeiten**: Display aus (wenn `ONLY_OFFICE_HOURS`)
3. **Wakeup-Modus**: Zeigt Zeit für 15 Sekunden, dann zurück zu Random
4. **Random-Modus**: Zufällige Farbgradienten auf allen Segmenten

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
**Webserver** - Async HTTP Server auf Port 80.

| Route | Methode | Beschreibung |
|-------|---------|--------------|
| `/` | GET | index.html (wenn Zeit gesetzt) oder adjust.html |
| `/adjust` | GET | Zeit-Einstellungs-Formular |
| `/adjust` | POST | Zeit setzen (hours, minutes, day, month, yr) |
| `/wakeup` | POST | Weckt die Anzeige auf (wakeup=1) |
| `/*` | GET | Statische Dateien aus LittleFS |

---

## Web-Frontend

### index.html
Hauptseite mit großem SVG-Button (Wecker-Symbol) zum "Aufwecken" der Uhr.
- Klick sendet POST an `/wakeup?wakeup=1`

### adjust.html
Formular zum Einstellen der Uhrzeit:
- Stunden (0-23)
- Minuten (0-59)
- Tag (1-31)
- Monat (1-12)
- Jahr (1970-9999)

### adjusted.html
Bestätigungsseite nach erfolgreichem Setzen der Zeit.
- Redirect nach 2 Sekunden zu `/`

---

## Ablaufdiagramm

```
                    ┌─────────────┐
                    │   BOOT      │
                    └──────┬──────┘
                           │
        ┌──────────────────┼──────────────────┐
        ▼                  ▼                  ▼
   ┌────────┐        ┌──────────┐       ┌──────────┐
   │  RTC   │        │ Network  │       │   OTA    │
   │ DS1307 │        │  WiFi AP │       │  Update  │
   └────┬───┘        └────┬─────┘       └────┬─────┘
        │                 │                  │
        └────────────┬────┴──────────────────┘
                     ▼
              ┌─────────────┐
              │  Webserver  │
              │   Port 80   │
              └──────┬──────┘
                     │
                     ▼
              ┌─────────────┐
              │    LEDs     │
              │  282x APA102│
              └──────┬──────┘
                     │
        ┌────────────┼────────────┐
        ▼            ▼            ▼
   ┌─────────┐  ┌─────────┐  ┌─────────┐
   │ Zeit    │  │ Random  │  │ Wakeup  │
   │ nicht   │  │ Modus   │  │ Modus   │
   │ gesetzt │  │         │  │ (15s)   │
   └─────────┘  └─────────┘  └─────────┘
```

---

## Dependencies (platformio.ini)

| Library | Version | Verwendung |
|---------|---------|------------|
| FastLED | ^3.9.0 | APA102 LED-Steuerung |
| ESPAsyncWebServer | GitHub | Async HTTP Server |
| AsyncTCP | GitHub | TCP für ESP32 |
| RTClib | ^2.1.4 | DS1307 RTC Treiber |

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
