import geometry from "../geometry.json";

// ---------------------------------------------------------------------------
// Live LED preview.
//
// The firmware streams the raw FastLED buffer: NUM_LEDS * 3 bytes, one RGB
// triple per LED in strip order.
//
// Each segment is drawn as its exact shape from the original clock artwork,
// filled with a linear gradient whose stops are that segment's ten LED colours
// laid along its long axis. So the form is the real segment, while the colour
// still carries per-LED detail — which is what an APA102 bar diffused behind a
// panel actually looks like.
//
// Two stacked canvases: a sharp one, and a CSS-blurred one underneath for the
// bloom. The blur is a compositor filter, so it costs nothing per frame. The
// glow layer uses each segment's average colour rather than its gradient —
// bloom is low-frequency, and it halves the gradient work per frame.
// ---------------------------------------------------------------------------

const COLON_INDEX = 28;

// Segments whose LED order runs opposite to the artwork's geometry. The SVG
// cannot say which end of a bar is LED 0 — run the calibration walk (the button
// under the preview) and add any segment that fills backwards. Reversing a
// segment simply flips its gradient direction.
const REVERSED_SEGMENTS = new Set([]);

export class LedPreview {
  constructor(container) {
    this.glow = document.createElement("canvas");
    this.core = document.createElement("canvas");
    this.glow.className = "led-layer led-glow";
    this.core.className = "led-layer led-core";
    container.append(this.glow, this.core);

    this.glowCtx = this.glow.getContext("2d");
    this.coreCtx = this.core.getContext("2d");

    // Paths are built once in artwork coordinates; the canvas transform scales
    // them, so a resize costs nothing.
    this.paths = geometry.segments.map((seg) => {
      const path = new Path2D();
      seg.points.forEach(([x, y], i) => (i ? path.lineTo(x, y) : path.moveTo(x, y)));
      path.closePath();
      return path;
    });

    this.colonPaths = geometry.colon.dots.map(([x, y]) => {
      const path = new Path2D();
      path.arc(x, y, geometry.colon.r, 0, Math.PI * 2);
      return path;
    });

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

  // `data` is a Uint8Array of NUM_LEDS * 3 bytes.
  update(data) {
    this.frame = data;
    this.dirty = true;
  }

  draw() {
    requestAnimationFrame(() => this.draw());
    if (!this.dirty) return;
    this.dirty = false;

    const { glowCtx, coreCtx, scale, frame } = this;

    for (const [ctx, canvas] of [[glowCtx, this.glow], [coreCtx, this.core]]) {
      ctx.setTransform(1, 0, 0, 1, 0, 0);
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      ctx.setTransform(scale, 0, 0, scale, 0, 0);
    }
    if (!frame) return;

    let brightest = -1;
    let brightestValue = 0;

    geometry.segments.forEach((seg, index) => {
      let sumR = 0;
      let sumG = 0;
      let sumB = 0;
      for (let i = 0; i < seg.leds; i++) {
        const o = (seg.start + i) * 3;
        sumR += frame[o];
        sumG += frame[o + 1];
        sumB += frame[o + 2];
        const total = frame[o] + frame[o + 1] + frame[o + 2];
        if (total > brightestValue) {
          brightestValue = total;
          brightest = seg.start + i;
        }
      }
      if (sumR + sumG + sumB === 0) return; // dark segments are most of them

      const path = this.paths[index];
      const [from, to] = REVERSED_SEGMENTS.has(index)
        ? [seg.axis[1], seg.axis[0]]
        : seg.axis;

      // LED i sits at (i + 0.5) / leds along the bar. Canvas clamps to the end
      // stops beyond that range, so the tapered tips take the end LEDs' colour.
      const gradient = coreCtx.createLinearGradient(from[0], from[1], to[0], to[1]);
      for (let i = 0; i < seg.leds; i++) {
        const o = (seg.start + i) * 3;
        gradient.addColorStop(
          (i + 0.5) / seg.leds,
          `rgb(${frame[o]},${frame[o + 1]},${frame[o + 2]})`,
        );
      }
      coreCtx.fillStyle = gradient;
      coreCtx.fill(path);

      const n = seg.leds;
      glowCtx.fillStyle = `rgb(${(sumR / n) | 0},${(sumG / n) | 0},${(sumB / n) | 0})`;
      glowCtx.fill(path);
    });

    // Colon: one LED per dot, so a flat fill each.
    geometry.colon.dots.forEach((_, dot) => {
      const o = (geometry.colon.start + dot) * 3;
      const r = frame[o];
      const g = frame[o + 1];
      const b = frame[o + 2];
      if (r + g + b === 0) return;
      if (r + g + b > brightestValue) {
        brightestValue = r + g + b;
        brightest = geometry.colon.start + dot;
      }
      const fill = `rgb(${r},${g},${b})`;
      coreCtx.fillStyle = fill;
      coreCtx.fill(this.colonPaths[dot]);
      glowCtx.fillStyle = fill;
      glowCtx.fill(this.colonPaths[dot]);
    });

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
