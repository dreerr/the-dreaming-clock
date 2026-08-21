import {
  getState,
  patchState,
  getTimezones,
  sendMessage,
  clearMessages,
  saveHandler,
  showStatus,
} from "./api.js";

const $ = (id) => document.getElementById(id);
const DAY_NAMES = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
const DAY_ORDER = [1, 2, 3, 4, 5, 6, 0]; // display Monday-first, index stays Sunday-first

let state = null;
const chain = []; // messages staged but not yet sent

// --- rendering -------------------------------------------------------------

function renderDayTable() {
  const tbody = $("day-rows");
  tbody.innerHTML = "";
  for (const index of DAY_ORDER) {
    const day = state.schedule.days[index];
    const row = document.createElement("tr");
    row.dataset.day = index;
    row.innerHTML = `
      <td>${DAY_NAMES[index]}</td>
      <td><input type="checkbox" class="day-enabled" aria-label="${DAY_NAMES[index]} active" ${
        day.enabled ? "checked" : ""
      } /></td>
      <td><input type="number" class="day-start" min="0" max="23" value="${day.start}" aria-label="${
        DAY_NAMES[index]
      } start hour" /></td>
      <td><input type="number" class="day-end" min="0" max="23" value="${day.end}" aria-label="${
        DAY_NAMES[index]
      } end hour" /></td>`;
    tbody.append(row);
  }
}

function collectDays() {
  const days = state.schedule.days.map((d) => ({ ...d }));
  for (const row of document.querySelectorAll("tr[data-day]")) {
    const index = Number(row.dataset.day);
    days[index] = {
      enabled: row.querySelector(".day-enabled").checked,
      start: Number(row.querySelector(".day-start").value),
      end: Number(row.querySelector(".day-end").value),
    };
  }
  return days;
}

function renderNetworkInfo() {
  const { network } = state;
  const parts = [network.activeMode === 0 ? "Access point" : "Joined a network"];
  if (network.ip) parts.push(network.ip);
  if (network.activeMode === 1) parts.push(network.connected ? "connected" : "disconnected");
  $("network-info").innerHTML = `<strong>${parts[0]}</strong>${
    parts.length > 1 ? " · " + parts.slice(1).join(" · ") : ""
  }`;
}

function renderTimeInfo() {
  const { time } = state;
  if (!time.valid) {
    $("time-info").innerHTML = "<strong>Not set</strong>";
    return;
  }
  const source = time.ntpSynced ? "NTP" : time.rtcPresent ? "RTC" : "internal, lost on power cycle";
  const clock = `${String(time.hour).padStart(2, "0")}:${String(time.minute).padStart(2, "0")}`;
  $("time-info").innerHTML = `<strong>${clock}</strong> · ${time.timezone} · ${source}`;
}

// While something is playing, offer to stop it.
function renderMessageState() {
  const playing = state.message?.playing;
  $("msg-clear").hidden = !playing;
  if (playing && state.message.text) {
    showStatus("msg-status", true, `playing "${state.message.text}"`);
  }
}

function selectedEffect() {
  const pressed = document.querySelector('#msg-effect [aria-pressed="true"]');
  return pressed ? pressed.dataset.effect : "scroll";
}

// The segmented control shows which mode the clock is actually in, so it is
// state rather than three buttons that look identical.
function renderMode() {
  for (const button of document.querySelectorAll("[data-mode]")) {
    button.setAttribute(
      "aria-pressed",
      String(button.dataset.mode === state.display.mode),
    );
  }
}

function toggleNetworkMode() {
  $("clientConfig").hidden = $("networkMode").value !== "1";
}

function toggleActiveHours() {
  const enabled = $("activeHoursEnabled").checked;
  const config = $("activeHoursConfig");
  config.dataset.enabled = String(enabled);
  config.inert = !enabled;
}

function apply() {
  $("timezone").value = state.time.timezone;
  $("wakeupInterval").value = state.schedule.wakeupInterval;
  $("activeHoursEnabled").checked = state.schedule.useActiveHours;
  $("brightness").value = state.display.brightness;
  $("brightness-value").textContent = state.display.brightness;
  $("dreamBrightness").value = state.display.dreamBrightness;
  $("dreamBrightness-value").textContent = state.display.dreamBrightness;
  $("networkMode").value = String(state.network.mode);
  $("wifiSSID").value = state.network.ssid || "";
  $("fallbackEnabled").checked = state.network.fallback;

  renderMode();
  renderMessageState();
  renderDayTable();
  renderNetworkInfo();
  renderTimeInfo();
  toggleNetworkMode();
  toggleActiveHours();
}

async function reload() {
  state = await getState();
  apply();
}

// --- wiring ----------------------------------------------------------------

async function init() {
  const zones = await getTimezones();
  $("timezone").innerHTML = zones
    .map((z) => `<option value="${z}">${z.replace(/_/g, " ")}</option>`)
    .join("");

  await reload();
  fillBrowserTime();
}

function fillBrowserTime() {
  const now = new Date();
  $("hours").value = now.getHours();
  $("minutes").value = now.getMinutes();
  $("day").value = now.getDate();
  $("month").value = now.getMonth() + 1;
  $("year").value = now.getFullYear();
}

// Called once by the clock page when the settings panel is first opened.
export function initSettingsPanel() {
  init().catch((error) => showStatus("time-status", false, `✗ ${error.message}`));

  $("activeHoursEnabled").addEventListener("change", toggleActiveHours);
  $("networkMode").addEventListener("change", toggleNetworkMode);
  $("use-browser-time").addEventListener("click", fillBrowserTime);

  for (const id of ["brightness", "dreamBrightness"]) {
    $(id).addEventListener("input", (e) => {
      $(`${id}-value`).textContent = e.target.value;
    });
  }

  $("save-timezone").addEventListener(
    "click",
    saveHandler("tz-status", () => patchState({ timezone: $("timezone").value })),
  );

  $("save-time").addEventListener(
    "click",
    saveHandler("time-status", async () => {
      const result = await patchState({
        time: {
          year: Number($("year").value),
          month: Number($("month").value),
          day: Number($("day").value),
          hour: Number($("hours").value),
          minute: Number($("minutes").value),
          second: 0,
        },
      });
      await reload();
      return result;
    }),
  );

  $("save-active-hours").addEventListener(
    "click",
    saveHandler("active-status", () =>
      patchState({
        useActiveHours: $("activeHoursEnabled").checked,
        days: collectDays(),
      }),
    ),
  );

  $("save-wakeup").addEventListener(
    "click",
    saveHandler("wakeup-status", () =>
      patchState({ wakeupInterval: Number($("wakeupInterval").value) }),
    ),
  );

  $("save-brightness").addEventListener(
    "click",
    saveHandler("brightness-status", () =>
      patchState({
        brightness: Number($("brightness").value),
        dreamBrightness: Number($("dreamBrightness").value),
      }),
    ),
  );

  // --- message -------------------------------------------------------------
  // Each effect wants a very different pace: a scroll steps per character, an
  // appear holds a whole page. One default would be wrong for two of the three.
  const DEFAULT_STEP = { scroll: 300, appear: 2000, blink: 500 };

  function currentDraft() {
    // Deliberately not trimmed: leading and trailing spaces are blanks on the
    // display, and someone typing "   2" means to see them.
    return {
      text: $("msg-text").value,
      effect: selectedEffect(),
      fill: $("msg-fill").value,
      hue: Number($("msg-hue").value),
      stepMs: Number($("msg-step").value),
      crossfade: $("msg-crossfade").checked,
    };
  }

  function hueCss(hue) {
    return `hsl(${Math.round((hue / 255) * 360)}, 100%, 50%)`;
  }

  function renderSwatch() {
    $("msg-swatch").style.background = hueCss(Number($("msg-hue").value));
  }

  function renderChain() {
    const list = $("msg-chain");
    list.hidden = chain.length === 0;
    list.innerHTML = "";
    chain.forEach((m, index) => {
      const li = document.createElement("li");
      li.innerHTML = `
        <span class="step-no">${index + 1}</span>
        <span class="chain-dot" style="background:${hueCss(m.hue)}"></span>
        <span class="chain-text">${m.text.replace(/ /g, "\u00b7")}</span>
        <span>${m.effect} \u00b7 ${m.stepMs}ms</span>
        <button type="button" aria-label="Remove">\u00d7</button>`;
      li.querySelector("button").addEventListener("click", () => {
        chain.splice(index, 1);
        renderChain();
      });
      list.append(li);
    });
    $("msg-send").textContent = chain.length ? `Send ${chain.length}` : "Send";
  }

  for (const button of document.querySelectorAll("#msg-effect [data-effect]")) {
    button.addEventListener("click", () => {
      for (const other of document.querySelectorAll("#msg-effect [data-effect]")) {
        other.setAttribute("aria-pressed", String(other === button));
      }
      const step = DEFAULT_STEP[button.dataset.effect];
      $("msg-step").value = String(step);
      $("msg-step-value").textContent = String(step);
    });
  }

  $("msg-step").addEventListener("input", (e) => {
    $("msg-step-value").textContent = e.target.value;
  });
  $("msg-hue").addEventListener("input", renderSwatch);
  renderSwatch();

  $("msg-add").addEventListener("click", () => {
    const draft = currentDraft();
    if (!draft.text) {
      showStatus("msg-status", false, "nothing to say");
      return;
    }
    chain.push(draft);
    renderChain();
    $("msg-text").value = "";
    $("msg-text").focus();
  });

  $("msg-send").addEventListener(
    "click",
    saveHandler("msg-status", async () => {
      const draft = currentDraft();
      const toSend = chain.slice();
      if (draft.text) toSend.push(draft);
      if (!toSend.length) throw new Error("nothing to say");

      const result = await sendMessage(toSend.length === 1 ? toSend[0] : toSend);
      // The chain is kept so it can be sent again without rebuilding it.
      $("msg-clear").hidden = false;

      const blanks = result.unrenderable || [];
      if (blanks.length) {
        return { message: `sent \u2014 ${blanks.join(" ")} cannot be drawn` };
      }
      return { message: toSend.length > 1 ? `sent ${toSend.length}` : "sent" };
    }),
  );

  $("msg-clear").addEventListener(
    "click",
    saveHandler("msg-status", async () => {
      await clearMessages();
      $("msg-clear").hidden = true;
      return { message: "stopped" };
    }),
  );


  for (const button of document.querySelectorAll("[data-mode]")) {
    button.addEventListener(
      "click",
      saveHandler("mode-status", async () => {
        const result = await patchState({ mode: button.dataset.mode });
        await reload();
        return result;
      }),
    );
  }

  $("save-network").addEventListener(
    "click",
    saveHandler("network-status", async () => {
      const network = {
        mode: Number($("networkMode").value),
        ssid: $("wifiSSID").value,
        fallback: $("fallbackEnabled").checked,
        apply: $("applyNow").checked,
      };
      // An empty password field means "keep the stored one".
      const password = $("wifiPassword").value;
      if (password) network.password = password;

      const result = await patchState({ network });
      if (network.apply) setTimeout(() => location.reload(), 4000);
      return result;
    }),
  );
}

// Re-read the device each time the panel is opened, so it never shows stale
// values after the clock has been changed from somewhere else.
export async function refreshSettings() {
  await reload();
}
