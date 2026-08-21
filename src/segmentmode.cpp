#include "segmentmode.h"

#include <string.h>

namespace {
// Order must match the enum.
const char *const kModeNames[] = {"constant", "pulse", "blink",
                                  "gradient", "sweep", "bloom"};
constexpr uint8_t kModeCount = sizeof(kModeNames) / sizeof(kModeNames[0]);
} // namespace

const char *segmentModeName(SegmentMode mode) {
  const uint8_t i = static_cast<uint8_t>(mode);
  return i < kModeCount ? kModeNames[i] : kModeNames[0];
}

bool segmentModeFromName(const char *name, SegmentMode &out) {
  if (name == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < kModeCount; i++) {
    if (strcmp(name, kModeNames[i]) == 0) {
      out = static_cast<SegmentMode>(i);
      return true;
    }
  }
  return false;
}

bool segmentModeUsesGradient(SegmentMode mode) {
  return mode == SegmentMode::RANDOM_GRADIENT || mode == SegmentMode::SWEEP ||
         mode == SegmentMode::BLOOM;
}
