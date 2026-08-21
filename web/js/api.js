// Thin wrapper over the device API. Everything goes through /api/state, which
// is the same canonical document the firmware serialises for every transport.

export async function getState() {
  const response = await fetch("/api/state");
  if (!response.ok) throw new Error(`HTTP ${response.status}`);
  return response.json();
}

export async function patchState(patch) {
  const response = await fetch("/api/state", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(patch),
  });
  const body = await response.json().catch(() => ({}));
  if (!response.ok || !body.success) {
    throw new Error(body.message || `HTTP ${response.status}`);
  }
  return body;
}

export async function wakeup() {
  const response = await fetch("/api/wakeup", { method: "POST" });
  if (!response.ok) throw new Error(`HTTP ${response.status}`);
  return response.json();
}

// The physical LED layout: how many LEDs and which belong to which segment.
// Comes from the device rather than the bundle, so changing LEDS_PER_SEGMENT in
// the firmware needs no frontend rebuild.
export async function getLayout() {
  const response = await fetch("/api/layout");
  if (!response.ok) throw new Error(`HTTP ${response.status}`);
  return response.json();
}

// Messages are an action rather than a setting, so they have their own
// endpoint instead of going through /api/state.
export async function sendMessage(message) {
  const response = await fetch("/api/message", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(message),
  });
  const body = await response.json().catch(() => ({}));
  if (!response.ok || !body.success) {
    throw new Error(body.message || `HTTP ${response.status}`);
  }
  return body;
}

export async function clearMessages() {
  const response = await fetch("/api/message", { method: "DELETE" });
  if (!response.ok) throw new Error(`HTTP ${response.status}`);
  return response.json();
}

export async function getTimezones() {
  const response = await fetch("/api/timezones");
  if (!response.ok) throw new Error(`HTTP ${response.status}`);
  return response.json();
}

// Shows a transient result next to a control.
export function showStatus(element, ok, message) {
  if (typeof element === "string") element = document.getElementById(element);
  if (!element) return;
  element.textContent = message;
  element.className = `status ${ok ? "success" : "error"}`;
  clearTimeout(element._timer);
  element._timer = setTimeout(() => {
    element.textContent = "";
    element.className = "status";
  }, 3000);
}

// Wraps a save handler so every button reports success or the server's reason
// for refusing, instead of a generic failure.
export function saveHandler(statusId, fn) {
  return async () => {
    try {
      const result = await fn();
      showStatus(statusId, true, result?.message || "✓ Saved");
    } catch (error) {
      showStatus(statusId, false, `✗ ${error.message}`);
    }
  };
}
