import geometry from "../geometry.json";

// ---------------------------------------------------------------------------
// Live LED preview.
//
// The firmware streams the raw FastLED buffer: NUM_LEDS * 3 bytes, one RGB
// triple per LED in strip order.
//
// Each segment is drawn as its exact shape from the original clock artwork,
// filled with a linear gradient whose stops are that segment's LED colours laid
// along its long axis. So the form is the real segment, while the colour still
// carries per-LED detail — which is what an APA102 bar diffused behind a panel
// actually looks like.
//
// Shapes come from geometry.json (the artwork) and the LED ranges come from the
// device's /api/layout. Keeping those apart is what lets LEDS_PER_SEGMENT
// change in the firmware without regenerating anything here.
//
// Two stacked canvases: a sharp one, and a CSS-blurred one underneath for the
// bloom. The blur is a compositor filter, so it costs nothing per frame. The
// glow layer uses each segment's average colour rather than its gradient —
// bloom is low-frequency, and it halves the gradient work per frame.
// ---------------------------------------------------------------------------

export class LedPreview {
  // `layout` is the device's /api/layout document.
  constructor(container, layout) {
    this.layout = layout;
    this.frameBytes = layout.numLeds * 3;
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
    this.stage = container;
    this.pendingResize = false;

    this.resize();

    // A ResizeObserver catches the container changing for any reason — window
    // resize, orientation change, a phone's URL bar sliding away — not just
    // window resize events. Work is coalesced into one frame so dragging a
    // window edge cannot thrash the canvas allocation.
    if (typeof ResizeObserver !== "undefined") {
      new ResizeObserver(() => this.scheduleResize()).observe(container);
    } else {
      window.addEventListener("resize", () => this.scheduleResize());
    }

    // Moving the window between displays changes devicePixelRatio without
    // changing the element's size, so the observer alone would miss it.
    this.watchPixelRatio();

    requestAnimationFrame(() => this.draw());
  }

  watchPixelRatio() {
    if (typeof matchMedia !== "function") return;
    const query = matchMedia(`(resolution: ${window.devicePixelRatio}dppx)`);
    const onChange = () => {
      this.scheduleResize();
      this.watchPixelRatio(); // the query is tied to the old ratio; re-arm it
    };
    if (query.addEventListener) {
      query.addEventListener("change", onChange, { once: true });
    }
  }

  scheduleResize() {
    if (this.pendingResize) return;
    this.pendingResize = true;
    requestAnimationFrame(() => {
      this.pendingResize = false;
      this.resize();
    });
  }

  resize() {
    const availWidth = this.stage.clientWidth;
    const availHeight = this.stage.clientHeight;
    if (availWidth === 0 || availHeight === 0) return;

    // Fill the width, but never overflow the height — a short, wide window
    // would otherwise crop the clock instead of shrinking it.
    const aspect = geometry.width / geometry.height;
    let width = availWidth;
    let height = width / aspect;
    if (height > availHeight) {
      height = availHeight;
      width = height * aspect;
    }

    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const pixelWidth = Math.max(1, Math.round(width * dpr));
    const pixelHeight = Math.max(1, Math.round(height * dpr));

    for (const canvas of [this.glow, this.core]) {
      // Assigning width/height clears the canvas, so only touch it on a real
      // change — otherwise every spurious observer callback drops a frame.
      if (canvas.width !== pixelWidth || canvas.height !== pixelHeight) {
        canvas.width = pixelWidth;
        canvas.height = pixelHeight;
      }
      canvas.style.width = `${width}px`;
      canvas.style.height = `${height}px`;
    }

    this.scale = pixelWidth / geometry.width;
    this.dirty = true;
  }

  // `data` is a Uint8Array of numLeds * 3 bytes.
  update(data) {
    // A page left open across a firmware change with a different LED count
    // would otherwise index past the end and draw nonsense.
    if (data.length !== this.frameBytes) {
      if (!this.warnedAboutSize) {
        this.warnedAboutSize = true;
        console.warn(
          `LED frame is ${data.length} bytes, expected ${this.frameBytes}. ` +
            "Reload to pick up the current layout.",
        );
      }
      return;
    }
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
      const { start, count } = this.layout.segments[index];
      let sumR = 0;
      let sumG = 0;
      let sumB = 0;
      for (let i = 0; i < count; i++) {
        const o = (start + i) * 3;
        sumR += frame[o];
        sumG += frame[o + 1];
        sumB += frame[o + 2];
        const total = frame[o] + frame[o + 1] + frame[o + 2];
        if (total > brightestValue) {
          brightestValue = total;
          brightest = start + i;
        }
      }
      if (sumR + sumG + sumB === 0) return; // dark segments are most of them

      const path = this.paths[index];
      // Which end of a bar holds its first LED is a wiring fact the device
      // reports; reversing simply flips the gradient.
      const [from, to] = this.layout.segments[index].reversed
        ? [seg.axis[1], seg.axis[0]]
        : seg.axis;

      // LED i sits at (i + 0.5) / count along the bar. Canvas clamps to the end
      // stops beyond that range, so the tapered tips take the end LEDs' colour.
      const gradient = coreCtx.createLinearGradient(from[0], from[1], to[0], to[1]);
      for (let i = 0; i < count; i++) {
        const o = (start + i) * 3;
        gradient.addColorStop(
          (i + 0.5) / count,
          `rgb(${frame[o]},${frame[o + 1]},${frame[o + 2]})`,
        );
      }
      coreCtx.fillStyle = gradient;
      coreCtx.fill(path);

      glowCtx.fillStyle =
        `rgb(${(sumR / count) | 0},${(sumG / count) | 0},${(sumB / count) | 0})`;
      glowCtx.fill(path);
    });

    // Colon: the artwork has two dots. If the hardware puts more than one LED
    // in the colon segment, each dot averages its half.
    const colon = this.layout.segments[this.layout.colonIndex];
    const perDot = Math.max(1, Math.floor(colon.count / geometry.colon.dots.length));
    geometry.colon.dots.forEach((_, dot) => {
      const first = colon.start + Math.min(dot * perDot, colon.count - 1);
      const n = Math.min(perDot, colon.start + colon.count - first);
      let r = 0;
      let g = 0;
      let b = 0;
      for (let i = 0; i < n; i++) {
        const o = (first + i) * 3;
        r += frame[o];
        g += frame[o + 1];
        b += frame[o + 2];
        const total = frame[o] + frame[o + 1] + frame[o + 2];
        if (total > brightestValue) {
          brightestValue = total;
          brightest = first + i;
        }
      }
      r = (r / n) | 0;
      g = (g / n) | 0;
      b = (b / n) | 0;
      if (r + g + b === 0) return;
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
