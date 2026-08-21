import { LedPreview, PreviewSocket } from "./preview.js";
import { wakeup, showStatus } from "./api.js";

const preview = new LedPreview(document.getElementById("preview"));
const statusEl = document.getElementById("ws-status");
const calibrationEl = document.getElementById("calibration-readout");

const socket = new PreviewSocket({
  onFrame: (data) => preview.update(data),
  onStatus: (state) => {
    statusEl.textContent = state === "live" ? "🟢 Live" : "🔴 Reconnecting…";
    statusEl.className = `ws-status ${state === "live" ? "connected" : "disconnected"}`;
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

// --- calibration -----------------------------------------------------------
// Walks one lit LED along the strip. Watch the clock and the preview together:
// if a bar fills in the opposite direction on screen, add its segment number to
// REVERSED_SEGMENTS in preview.js.
let calibrating = false;
const calibrateButton = document.getElementById("calibrate-button");

calibrateButton.addEventListener("click", () => {
  calibrating = !calibrating;
  socket.send(calibrating ? "calibrate on" : "calibrate off");
  calibrateButton.textContent = calibrating ? "Stop calibration" : "Calibrate LED order";
  calibrateButton.classList.toggle("active", calibrating);
  calibrationEl.hidden = !calibrating;
});

preview.onLitLed = (index) => {
  if (!calibrating || index < 0) return;
  const segment = index >= 140 && index < 142 ? 28 : Math.floor((index >= 142 ? index - 2 : index) / 10);
  const offset = index >= 142 ? (index - 2) % 10 : index >= 140 ? index - 140 : index % 10;
  calibrationEl.textContent = `LED ${index} — segment ${segment}, position ${offset}`;
};
