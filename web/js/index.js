import { LedPreview, PreviewSocket } from "./preview.js";
import { getLayout, wakeup } from "./api.js";

const dot = document.getElementById("ws-dot");
const readout = document.getElementById("readout");
const calibrateButton = document.getElementById("calibrate-button");
const clock = document.getElementById("preview");

let readoutTimer = null;

// The bottom line is normally empty. It carries the calibration position while
// calibrating, and an error if something fails — nothing else, so the page
// stays quiet.
function say(text, { error = false, sticky = false } = {}) {
  clearTimeout(readoutTimer);
  if (!text) {
    readout.hidden = true;
    return;
  }
  readout.textContent = text;
  readout.classList.toggle("error", error);
  readout.hidden = false;
  if (!sticky) {
    readoutTimer = setTimeout(() => {
      readout.hidden = true;
    }, 3000);
  }
}

// Which segment owns a strip index, straight from the device's own mapping —
// no second copy of segmentLedStart() living in the browser.
function locateLed(layout, index) {
  for (let seg = 0; seg < layout.segments.length; seg++) {
    const { start, count } = layout.segments[seg];
    if (index >= start && index < start + count) {
      return { segment: seg, offset: index - start };
    }
  }
  return null;
}

async function start() {
  const layout = await getLayout();
  const preview = new LedPreview(clock, layout);

  const socket = new PreviewSocket({
    onFrame: (data) => preview.update(data),
    onStatus: (state) => dot.classList.toggle("connected", state === "live"),
  });

  // The clock is the button. Its own response — lighting up with the time — is
  // the feedback, so nothing is shown unless the request fails.
  clock.addEventListener("click", async () => {
    try {
      await wakeup();
    } catch (error) {
      say(error.message, { error: true });
    }
  });

  // Calibration walks one lit LED along the strip. Watch the clock and the
  // preview together: if a bar fills in the opposite direction on screen,
  // adjust segmentIsReversed() in src/config.h and reflash.
  let calibrating = false;
  calibrateButton.addEventListener("click", () => {
    calibrating = !calibrating;
    socket.send(calibrating ? "calibrate on" : "calibrate off");
    calibrateButton.setAttribute("aria-pressed", String(calibrating));
    say(calibrating ? "calibrating…" : "", { sticky: true });
  });

  preview.onLitLed = (index) => {
    if (!calibrating || index < 0) return;
    const found = locateLed(layout, index);
    say(
      found
        ? `LED ${index} · segment ${found.segment} · position ${found.offset}`
        : `LED ${index}`,
      { sticky: true },
    );
  };
}

start().catch((error) => say(error.message, { error: true, sticky: true }));
