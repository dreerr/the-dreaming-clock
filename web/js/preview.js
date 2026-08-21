import geometry from "../geometry.json";

// ---------------------------------------------------------------------------
// Live LED preview.
//
// The firmware streams the raw FastLED buffer: NUM_LEDS * 3 bytes, one RGB
// triple per LED in strip order. Drawing all 282 individually is what makes it
// a preview of the clock rather than a diagram of it.
//
// Two stacked canvases: a sharp one for the LED dies and a CSS-blurred one
// underneath for the bloom. The blur is a compositor filter, so it costs
// nothing per frame — far cheaper than shadowBlur or per-LED gradients.
// ---------------------------------------------------------------------------

const LED_COUNT = geometry.positions.length;
const CORE_RADIUS = geometry.ledRadius * 0.55;
const GLOW_RADIUS = geometry.ledRadius * 1.5;

// Which segments have their LED order reversed relative to the SVG geometry.
// The SVG cannot say which end of a bar is LED 0 — run the calibration walk
// (the button under the preview) and flip any segment that runs backwards.
const REVERSED_SEGMENTS = new Set([]);

const LEDS_PER_SEGMENT = 10;
const COLON_INDEX = 28;
const COLON_LED_START = 140;

function segmentLedStart(seg) {
  if (seg === COLON_INDEX) return COLON_LED_START;
  const base = seg * LEDS_PER_SEGMENT;
  return base >= COLON_LED_START ? base + 2 : base;
}

// Maps a strip index to the geometry slot it should be drawn at, honouring
// REVERSED_SEGMENTS. Built once.
function buildIndexMap() {
  const map = new Uint16Array(LED_COUNT);
  for (let i = 0; i < LED_COUNT; i++) map[i] = i;
  for (const seg of REVERSED_SEGMENTS) {
    const start = segmentLedStart(seg);
    const count = seg === COLON_INDEX ? 2 : LEDS_PER_SEGMENT;
    for (let i = 0; i < count; i++) {
      map[start + i] = start + count - 1 - i;
    }
  }
  return map;
}

export class LedPreview {
  constructor(container) {
    this.indexMap = buildIndexMap();
    this.positions = geometry.positions;

    this.glow = document.createElement("canvas");
    this.core = document.createElement("canvas");
    this.glow.className = "led-layer led-glow";
    this.core.className = "led-layer led-core";
    container.append(this.glow, this.core);

    this.glowCtx = this.glow.getContext("2d");
    this.coreCtx = this.core.getContext("2d");

    this.frame = null;
    this.dirty = false;
    this.onLitLed = null;

    this.resize();
    window.addEventListener("resize", () => this.resize());
    requestAnimationFrame(() => this.draw());
  }

  resize() {
    const rect = this.glow.parentElement.getBoundingClientRect();
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const width = rect.width;
    const height = (width * geometry.height) / geometry.width;

    for (const canvas of [this.glow, this.core]) {
      canvas.width = Math.round(width * dpr);
      canvas.height = Math.round(height * dpr);
      canvas.style.width = `${width}px`;
      canvas.style.height = `${height}px`;
    }
    this.scale = (width * dpr) / geometry.width;
    this.dirty = true;
  }

  // `data` is a Uint8Array of LED_COUNT * 3 bytes.
  update(data) {
    this.frame = data;
    this.dirty = true;
  }

  draw() {
    requestAnimationFrame(() => this.draw());
    if (!this.dirty) return;
    this.dirty = false;

    const { glowCtx, coreCtx, scale, positions, indexMap, frame } = this;
    glowCtx.clearRect(0, 0, this.glow.width, this.glow.height);
    coreCtx.clearRect(0, 0, this.core.width, this.core.height);
    if (!frame) return;

    const coreR = CORE_RADIUS * scale;
    const glowR = GLOW_RADIUS * scale;
    let brightest = -1;
    let brightestValue = 0;

    for (let i = 0; i < LED_COUNT; i++) {
      const r = frame[i * 3];
      const g = frame[i * 3 + 1];
      const b = frame[i * 3 + 2];
      const sum = r + g + b;
      if (sum === 0) continue; // most LEDs are dark most of the time

      if (sum > brightestValue) {
        brightestValue = sum;
        brightest = i;
      }

      const p = positions[indexMap[i]];
      const x = p[0] * scale;
      const y = p[1] * scale;
      const fill = `rgb(${r},${g},${b})`;

      glowCtx.fillStyle = fill;
      glowCtx.beginPath();
      glowCtx.arc(x, y, glowR, 0, Math.PI * 2);
      glowCtx.fill();

      coreCtx.fillStyle = fill;
      coreCtx.beginPath();
      coreCtx.arc(x, y, coreR, 0, Math.PI * 2);
      coreCtx.fill();
    }

    if (this.onLitLed) this.onLitLed(brightest);
  }
}

// ---------------------------------------------------------------------------
// WebSocket transport, with automatic reconnect.
// ---------------------------------------------------------------------------
export class PreviewSocket {
  constructor({ onFrame, onStatus }) {
    this.onFrame = onFrame;
    this.onStatus = onStatus;
    this.ws = null;
    this.retryMs = 1000;
    this.connect();
  }

  connect() {
    const protocol = location.protocol === "https:" ? "wss:" : "ws:";
    this.ws = new WebSocket(`${protocol}//${location.host}/ws/leds`);
    this.ws.binaryType = "arraybuffer";

    this.ws.onopen = () => {
      this.retryMs = 1000;
      this.onStatus("live");
    };
    this.ws.onmessage = (event) => {
      if (event.data instanceof ArrayBuffer) {
        this.onFrame(new Uint8Array(event.data));
      }
    };
    this.ws.onclose = () => {
      this.onStatus("offline");
      setTimeout(() => this.connect(), this.retryMs);
      this.retryMs = Math.min(this.retryMs * 2, 10000);
    };
    this.ws.onerror = () => this.ws.close();
  }

  send(text) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) this.ws.send(text);
  }
}
