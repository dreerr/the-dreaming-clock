# The Dreaming Clock — TODO

## Segment-Engine (Kern)

Ein Segment wird über **Wahrscheinlichkeit** gesteuert, nicht über Opacity.
Die Display-Schicht schreibt `probability`; das Segment würfelt nach jedem
Zyklus neu, ob es weiterleuchtet oder ausgeht.

- [x] Dauer der Animation in ms (`cycleMs`)
- [x] `probability` 0–255 — Neuwurf nach jedem Zyklus
- [x] `cycleMs` 1–15 s pro Segment einstellbar
      ("wie schnell die Wahrscheinlichkeit angezeigt wird")
- [x] Leuchtmodi:
  - [x] konstant in einer Farbe (`CONSTANT`)
  - [x] zufälliger Verlauf (`RANDOM_GRADIENT`)
  - [x] pulsierend, auf- und abdimmen (`PULSE`)
  - [x] blinkend, an/aus im gleichen Intervall (`BLINK`)
- [x] `probability` 255/0 → konstante Anzeige (Uhrzeit steht ruhig)
- [x] Wörter über unterschiedlich lange Animationen pro Segment einblenden
- [ ] Leuchtmodi pro Uhr-Modus über die Weboberfläche wählbar
      (die Engine kann es, das UI noch nicht)

## Uhr-Modi

- [x] normale Uhrzeit (`wakeup`)
- [x] Traumwörter (`dream`) — kondensieren aus dem Rauschen heraus
- [x] randomisierte Muster als eigener Modus (`pattern`)
- [ ] Übergänge zwischen den Modi gestalten (aktuell harter Schnitt)

## Fundament

- [x] `Timer`-Library ersetzt (war ungepinnt und GPL vs. MIT)
- [x] Header in `.h`/`.cpp` aufgeteilt
- [x] Kanonischer State + `applyCommand()` (`src/state.cpp`)
- [x] Partitionstabelle: 1.625 MB App + 640 KB LittleFS
- [x] OTA-Passwort aus dem Quelltext (`$CLOCK_OTA_PASSWORD`)
- [ ] **OTA-Passwort rotieren** — das alte steht in der Git-Historie
- [x] Zeitzone wird tatsächlich angewendet; NTP läuft auch mit vorhandener RTC
- [x] Netzwerk-Reconnect ereignisgesteuert (blockierte vorher 15 s)
- [x] Tests (`pio test -e native`) + CI

## Web

- [x] Vorschau mit voller LED-Auflösung (282 statt 29 Farben, Canvas)
- [x] Kalibrier-Modus für die LED-Richtung pro Segment
- [ ] **Kalibrierung einmal durchführen** und `REVERSED_SEGMENTS`
      in `web/js/preview.js` eintragen
- [x] Build-Step (esbuild + gzip), bewusst ohne Framework
- [x] Globale Helligkeit (getrennt für Uhrzeit und Traum)

## Später (bewusst noch nicht implementiert)

- [ ] Home-Assistant-Integration über MQTT Discovery
      — `serializeState()` / `applyCommand()` sind die vorbereitete Naht
- [ ] Externe Datenquellen für Traumwörter
      — `nextDreamWord()` ist die vorbereitete Naht
- [ ] Authentifizierung für die API (heute offen im LAN)
