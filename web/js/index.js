import { LedPreview, PreviewSocket } from "./preview.js";
import { getLayout, wakeup } from "./api.js";
import { initSettingsPanel, refreshSettings } from "./settings.js";

const dot = document.getElementById("ws-dot");
const readout = document.getElementById("readout");
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

  // --- calibration ---------------------------------------------------------
  // Lives in the panel's developer section. The reading goes next to the button
  // rather than in the bottom line, so it is beside the control that produced
  // it — and it stays visible on a phone, where the panel covers the bottom.
  const calibrateButton = document.getElementById("calibrate-button");
  const calibrateStatus = document.getElementById("calibrate-status");
  let calibrating = false;

  calibrateButton.addEventListener("click", () => {
    calibrating = !calibrating;
    socket.send(calibrating ? "calibrate on" : "calibrate off");
    calibrateButton.setAttribute("aria-pressed", String(calibrating));
    calibrateButton.textContent = calibrating ? "Stop calibrating" : "Calibrate LED order";
    calibrateStatus.textContent = calibrating ? "starting…" : "";
  });

  preview.onLitLed = (index) => {
    if (!calibrating || index < 0) return;
    const found = locateLed(layout, index);
    calibrateStatus.textContent = found
      ? `LED ${index} · segment ${found.segment} · position ${found.offset}`
      : `LED ${index}`;
  };

  // Leaving the panel while calibrating would strand the clock in it.
  function stopCalibrating() {
    if (!calibrating) return;
    calibrating = false;
    socket.send("calibrate off");
    calibrateButton.setAttribute("aria-pressed", "false");
    calibrateButton.textContent = "Calibrate LED order";
    calibrateStatus.textContent = "";
  }

  // --- settings panel ------------------------------------------------------
  // Opening it narrows the clock rather than covering it, so a brightness or
  // mode change is visible on the preview while it is being made. The clock's
  // ResizeObserver re-fits the canvases as the panel slides.
  const panel = document.getElementById("panel");
  const settingsToggle = document.getElementById("settings-toggle");
  let panelReady = false;

  function setPanel(open) {
    document.body.dataset.panel = open ? "open" : "closed";
    settingsToggle.setAttribute("aria-expanded", String(open));
    panel.inert = !open;
    if (!open) {
      stopCalibrating();
      return;
    }
    if (!panelReady) {
      panelReady = true;
      initSettingsPanel();
    } else {
      refreshSettings().catch(() => {});
    }
  }

  setPanel(false);
  settingsToggle.addEventListener("click", () =>
    setPanel(document.body.dataset.panel !== "open"),
  );
  document.getElementById("panel-close").addEventListener("click", () => setPanel(false));
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && document.body.dataset.panel === "open") setPanel(false);
  });

}

start().catch((error) => say(error.message, { error: true, sticky: true }));
