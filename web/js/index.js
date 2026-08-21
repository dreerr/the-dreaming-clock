import { LedPreview, PreviewSocket } from "./preview.js";
import { getLayout, wakeup, showStatus } from "./api.js";

const statusEl = document.getElementById("ws-status");
const calibrationEl = document.getElementById("calibration-readout");
const calibrateButton = document.getElementById("calibrate-button");

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
  const preview = new LedPreview(document.getElementById("preview"), layout);

  const socket = new PreviewSocket({
    onFrame: (data) => preview.update(data),
    onStatus: (state) => {
      const live = state === "live";
      statusEl.textContent = live ? "🟢 Live" : "🔴 Reconnecting…";
      statusEl.className = `ws-status ${live ? "connected" : "disconnected"}`;
    },
  });

  document.getElementById("wake-button").addEventListener("click", async () => {
    try {
      await wakeup();
      showStatus("status", true, "✓ Clock woken");
    } catch (error) {
      showStatus("status", false, `✗ ${error.message}`);
    }
  });

  // Calibration walks one lit LED along the strip. Watch the clock and the
  // preview together: if a bar fills in the opposite direction on screen,
  // add its segment number to REVERSED_SEGMENTS in preview.js.
  let calibrating = false;
  calibrateButton.addEventListener("click", () => {
    calibrating = !calibrating;
    socket.send(calibrating ? "calibrate on" : "calibrate off");
    calibrateButton.textContent = calibrating
      ? "Stop calibration"
      : "Calibrate LED order";
    calibrateButton.classList.toggle("active", calibrating);
    calibrationEl.hidden = !calibrating;
  });

  preview.onLitLed = (index) => {
    if (!calibrating || index < 0) return;
    const found = locateLed(layout, index);
    calibrationEl.textContent = found
      ? `LED ${index} — segment ${found.segment}, position ${found.offset}`
      : `LED ${index}`;
  };
}

start().catch((error) => {
  statusEl.textContent = `🔴 ${error.message}`;
  statusEl.className = "ws-status disconnected";
});
