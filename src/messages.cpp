#include "messages.h"

#include <string.h>


namespace {

const char *const kEffectNames[] = {"scroll", "appear", "blink"};
constexpr uint8_t kEffectCount = sizeof(kEffectNames) / sizeof(kEffectNames[0]);

int textLength(const Message &message) {
  int len = 0;
  while (len < MESSAGE_MAX_CHARS && message.text[len] != '\0') {
    len++;
  }
  return len;
}

int pageCount(int len) {
  if (len <= 0) {
    return 1;
  }
  return (len + NUM_DIGITS - 1) / NUM_DIGITS;
}

// --- queue ---------------------------------------------------------------

Message queue[MESSAGE_QUEUE_DEPTH];
int queueCount = 0;
int current = 0;
int step = 0;
int repeat = 0;
uint32_t stepStartedAt = 0;
bool playing = false;

} // namespace

const char *messageEffectName(MessageEffect effect) {
  const uint8_t i = static_cast<uint8_t>(effect);
  return i < kEffectCount ? kEffectNames[i] : kEffectNames[0];
}

bool messageEffectFromName(const char *name, MessageEffect &out) {
  if (name == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < kEffectCount; i++) {
    if (strcmp(name, kEffectNames[i]) == 0) {
      out = static_cast<MessageEffect>(i);
      return true;
    }
  }
  return false;
}

// ===========================================================================
// Step maths
// ===========================================================================

int messageStepCount(const Message &message) {
  const int len = textLength(message);
  if (len == 0) {
    return 0;
  }
  if (message.effect == MessageEffect::SCROLL) {
    // The text enters at the right edge on step 0 and has fully left by the
    // last step, which is one blank beat after the final character exits.
    return len + NUM_DIGITS;
  }
  return pageCount(len);
}

void messageWindowAt(const Message &message, int step, char out[NUM_DIGITS]) {
  const int len = textLength(message);

  for (int i = 0; i < NUM_DIGITS; i++) {
    int index;
    if (message.effect == MessageEffect::SCROLL) {
      // The text sits NUM_DIGITS characters into a blank-padded strip, so at
      // step 0 only its first character has reached the rightmost cell.
      index = step + 1 + i - NUM_DIGITS;
    } else {
      index = step * NUM_DIGITS + i;
    }
    out[i] = (index >= 0 && index < len) ? message.text[index] : ' ';
  }
}

uint8_t messageLevelAt(const Message &message, uint32_t intoStepMs) {
  const uint32_t stepMs = message.stepMs > 0 ? message.stepMs : 1;
  const uint32_t t = intoStepMs >= stepMs ? stepMs - 1 : intoStepMs;

  switch (message.effect) {
  case MessageEffect::BLINK:
    // On for the first half of the step, off for the second — the same shape
    // as the BLINK segment mode, so the two read alike.
    return (t * 2 < stepMs) ? 255 : 0;

  case MessageEffect::APPEAR: {
    // Fade in over the first quarter, hold, fade out over the last quarter.
    const uint32_t ramp = stepMs / 4;
    if (ramp == 0) {
      return 255;
    }
    if (t < ramp) {
      return static_cast<uint8_t>((t * 255) / ramp);
    }
    if (t > stepMs - ramp) {
      return static_cast<uint8_t>(((stepMs - t) * 255) / ramp);
    }
    return 255;
  }

  case MessageEffect::SCROLL:
  default:
    // Full brightness throughout; the glide between glyphs is the segments'
    // own cross-fade, which is what `crossfade` turns on and off.
    return 255;
  }
}

// ===========================================================================
// Queue and playback
// ===========================================================================

bool messageEnqueue(const Message &message) {
  if (queueCount >= MESSAGE_QUEUE_DEPTH) {
    return false;
  }
  // Text of nothing but blanks is allowed: in a chain it is a deliberate beat
  // between two messages. Empty text is refused where the message is parsed.
  queue[queueCount++] = message;
  return true;
}

void messageClearAll() {
  queueCount = 0;
  current = 0;
  step = 0;
  repeat = 0;
  playing = false;
}

int messageQueued() { return playing ? queueCount - current : queueCount; }

bool messagePlaying() { return playing; }

const Message *messageCurrent() {
  return (playing && current < queueCount) ? &queue[current] : nullptr;
}

const char *messageCurrentText() {
  const Message *m = messageCurrent();
  return m != nullptr ? m->text : "";
}

void messageBegin(uint32_t now) {
  if (playing || queueCount == 0) {
    return;
  }
  current = 0;
  step = 0;
  repeat = 0;
  stepStartedAt = now;
  playing = true;
}

bool messageUpdate(uint32_t now) {
  if (!playing) {
    return false;
  }
  if (current >= queueCount) {
    messageClearAll();
    return false;
  }

  const Message &message = queue[current];
  const uint32_t stepMs = message.stepMs > 0 ? message.stepMs : 1;
  if (now - stepStartedAt < stepMs) {
    return true;
  }

  stepStartedAt = now;
  step++;
  if (step < messageStepCount(message)) {
    return true;
  }

  // Finished a pass through this message.
  step = 0;
  repeat++;
  const uint8_t repeats = message.repeats > 0 ? message.repeats : 1;
  if (repeat < repeats) {
    return true;
  }

  repeat = 0;
  current++;
  if (current >= queueCount) {
    messageClearAll();
    return false;
  }
  return true;
}

void messageWindow(char out[NUM_DIGITS]) {
  const Message *m = messageCurrent();
  if (m == nullptr) {
    for (int i = 0; i < NUM_DIGITS; i++) {
      out[i] = ' ';
    }
    return;
  }
  messageWindowAt(*m, step, out);
}

int messageStepIndex() { return step; }

uint8_t messageLevel(uint32_t now) {
  const Message *m = messageCurrent();
  return m == nullptr ? 0 : messageLevelAt(*m, now - stepStartedAt);
}
