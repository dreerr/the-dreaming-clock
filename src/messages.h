#pragma once
#include <stdint.h>

#include "config.h"
#include "segmentmode.h"

// ===========================================================================
// Messages
// ===========================================================================
//
// Text that interrupts whatever the clock is doing, plays, and hands the
// display back. Several can be chained, each with its own look.
//
// The display is four seven-segment cells with no sub-character resolution, so
// a scroll steps the text through those four cells one character at a time.
// Whether a step cross-fades or snaps is per-message.
//
// The step maths below is Arduino-free so it can be tested on the host: "what
// do the four cells show at step k" is where the bugs live.

constexpr int MESSAGE_MAX_CHARS = 32;
constexpr int MESSAGE_QUEUE_DEPTH = 8;

enum class MessageEffect : uint8_t {
  SCROLL, // steps right to left, on at one edge and off at the other
  APPEAR, // fades a page in, holds it, fades it out
  BLINK,  // flashes a page on and off
};

const char *messageEffectName(MessageEffect effect);
bool messageEffectFromName(const char *name, MessageEffect &out);

struct Message {
  char text[MESSAGE_MAX_CHARS + 1] = {0};
  MessageEffect effect = MessageEffect::SCROLL;
  SegmentMode fill = SegmentMode::CONSTANT;
  uint8_t hue = 0;
  uint16_t stepMs = 350;   // scroll step, page hold, or blink period
  uint8_t repeats = 1;
  bool crossfade = true;   // glide between steps rather than snapping
};

// --- pure step maths -------------------------------------------------------

// Steps in one repeat. Scroll runs the text fully on and fully off; appear and
// blink page through it four characters at a time.
int messageStepCount(const Message &message);

// What the four cells show at `step`. Always writes NUM_DIGITS characters,
// padding with spaces.
void messageWindowAt(const Message &message, int step, char out[NUM_DIGITS]);

// Brightness envelope within a step, 0..255 — what makes appear fade and blink
// blink. Scroll holds full brightness and lets the segments glide between
// glyphs instead.
uint8_t messageLevelAt(const Message &message, uint32_t intoStepMs);

// --- queue and playback ----------------------------------------------------

// False if the queue is full or the text has no renderable characters.
bool messageEnqueue(const Message &message);
void messageClearAll();

int messageQueued();     // messages waiting, including the one playing
bool messagePlaying();
const char *messageCurrentText();

// Begin playing the queue. Safe to call when already playing.
void messageBegin(uint32_t now);

// Advance playback. Returns false once the whole queue has finished.
bool messageUpdate(uint32_t now);

// The current window and level, valid while messagePlaying().
void messageWindow(char out[NUM_DIGITS]);
uint8_t messageLevel(uint32_t now);
const Message *messageCurrent();
