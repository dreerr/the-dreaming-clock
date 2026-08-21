#include "dreams.h"

namespace {

// Curated for the 7-segment glyph set. Duplicates that differ only in case have
// been removed, since case does not survive rendering.
const char *const kWords[] = {
    "HALO", "HOPE", "GLOW", "FADE", "SOFT", "BLUR", "HUSH", "REST", "EASE",
    "DEEP", "ECHO", "SOUL", "FREE", "PURE", "LUNA", "OPEN", "SAFE", "SANE",
    "SURE", "HELD", "FEEL", "SELF", "SOLO", "COOL", "LOOP", "SEED", "FLEE",
    "FUSE", "GONE", "NOON", "SOUS", "EONS", "LINE", "bEEp", "bUbS", "duSt",
    "pUFF", "FLoP", "dooP", "SnoO", "CALL", "FELL", "FALL", "FULL", "PULL",
    "HUGE", "ELSE", "Edge", "SunS", "LEAF", "FERN", "POOL", "SAND", "LAND",
    "GLEN", "ALSO", "bEAr", "HASE", "EGAL", "SALZ", "FELD", "GOLD", "LAUF",
    "FALZ", "GLAS", "FLUG", "SAFT", "FEST", "FUSS", "EULE", "ERDE", "PFAD",
    "LEER", "bLAU", "rOSA", "CHEF", "RUND", "LANG", "ALLE", "GANZ", "DANN",
    "NOCH", "FANG", "PULS", "GELD", "HAUS", "HUND",};

constexpr int kWordCount = sizeof(kWords) / sizeof(kWords[0]);

// Small xorshift so this file stays free of Arduino's random(); seeded from
// hardware entropy at boot.
uint32_t rngState = 0x2545F491u;

uint32_t nextRandom() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return rngState;
}

int lastIndex = -1;

} // namespace

int dreamWordCount() { return kWordCount; }

const char *dreamWordAt(int index) {
  if (index < 0 || index >= kWordCount) {
    return nullptr;
  }
  return kWords[index];
}

void seedDreamWords(uint32_t seed) {
  if (seed != 0) {
    rngState = seed;
  }
}

const char *nextDreamWord() {
  // Avoid repeating the word that was just shown; with 87 entries a repeat
  // reads as a glitch rather than as chance.
  int index = lastIndex;
  for (int attempt = 0; attempt < 8 && index == lastIndex; attempt++) {
    index = static_cast<int>(nextRandom() % kWordCount);
  }
  lastIndex = index;
  return kWords[index];
}
