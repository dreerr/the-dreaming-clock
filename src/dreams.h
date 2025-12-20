#pragma once
#include <Arduino.h>

// ============================================================================
// Dream Words - Subliminal messages during random/dream phase
// ============================================================================
// Only using characters that display well on 7-segment:
// A b C c d E F G H h L n o p r S t U u
//
// Note: Some letters look the same (C/c, H/h, U/u) but lowercase
// b, d, n, o, r, t, u are distinctly different from their uppercase versions

const char *dreamWords[] = {
    // Ethereal & Dreamy
    "HALO", "HOPE", "GLOW", "FADE", "SOFT", "BLUR", "HUSH", "REST", "EASE",
    "DEEP", "ECHO", "SOUL", "FREE", "PURE", "LUNA", "OPEN",

    // Calming
    "SAFE", "SANE", "SURE", "HELD", "FEEL", "SELF", "SOLO", "COOL",

    // Abstract
    "LOOP", "SEED", "FLEE", "FUSE", "GONE", "NOON", "SOUS", "EONS", "LINE",

    // Playful lowercase mix
    "bEEp", "bUbS", "duSt", "pUFF", "FLoP", "dooP", "SnoO", "FuSE",

    // Short atmospheric words
    "CALL", "FELL", "FALL", "FULL", "PULL", "HUGE", "ELSE", "Edge",

    // Nature-ish
    "SunS", "LEAF", "FERN", "POOL", "SAND", "LAND", "GLEN", "ALSO",
    "bEAr"

    // Deutsche Wörter (German words)
    "HASE", // Hase (rabbit)
    "EGAL", // egal (doesn't matter)
    "ELSE", // Else (river name)
    "SALZ", // Salz (salt) - Z looks like 2
    "FELD", // Feld (field)
    "GOLD", // Gold
    "LAUF", // Lauf (run)
    "FALZ", // Falz (fold)
    "GLAS", // Glas (glass)
    "FLUG", // Flug (flight)
    "SAFT", // Saft (juice)
    "FEST", // Fest (celebration)
    "FUSS", // Fuß (foot)
    "EULE", // Eule (owl)
    "ERDE", // Erde (earth)
    "PFAD", // Pfad (path)
    "LEER", // leer (empty)
    "SPAT", // spät (late) - without umlaut
    "GrUn", // grün (green)
    "bLAU", // blau (blue)
    "rOSA", // rosa (pink)
    "CHEF", // Chef (boss)
    "RUND", // rund (round)
    "LANG", // lang (long)
    "ALLE", // alle (all)
    "GANZ", // ganz (whole)
    "DANN", // dann (then)
    "NOCH", // noch (still)
    "FANG", // Fang (catch)
    "PULS", // Puls (pulse)
    "GELD", // Geld (money)
    "SAND", // Sand
    "HAUS", // Haus (house)
    "HUND", // Hund (dog)
};

const int dreamWordCount = sizeof(dreamWords) / sizeof(dreamWords[0]);

// Get a random dream word
inline const char *getRandomDreamWord() {
  return dreamWords[random(0, dreamWordCount)];
}

// ============================================================================
// Dream Phase Configuration
// ============================================================================

// How subtle the dream words appear (0-255, lower = more subtle)
#define DREAM_WORD_MAX_OPACITY 80

// Minimum opacity for dream words (creates "barely visible" effect)
#define DREAM_WORD_MIN_OPACITY 15

// Duration a word is visible before fading (milliseconds)
#define DREAM_WORD_DISPLAY_MS 4000

// Pause between words (milliseconds) - random pattern continues
#define DREAM_WORD_PAUSE_MS 8000

// Animation speed for dream words (1-4, lower = slower/smoother)
#define DREAM_WORD_FADE_SPEED 1

// Probability of showing a word during dream phase (0-255)
// 255 = always show, 0 = never show
#define DREAM_WORD_PROBABILITY 180
