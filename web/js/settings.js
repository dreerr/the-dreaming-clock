import { getState, patchState, getTimezones, saveHandler, showStatus } from "./api.js";

const $ = (id) => document.getElementById(id);
const DAY_NAMES = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
const DAY_ORDER = [1, 2, 3, 4, 5, 6, 0]; // display Monday-first, index stays Sunday-first

let state = null;

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
      <td><input type="checkbox" class="day-enabled" ${day.enabled ? "checked" : ""} /></td>
      <td><input type="number" class="day-start" min="0" max="23" value="${day.start}" /></td>
      <td><input type="number" class="day-end" min="0" max="23" value="${day.end}" /></td>`;
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
  const mode = network.activeMode === 0 ? "Captive Portal" : "WiFi Client";
  const parts = [`<strong>Current:</strong> ${mode}`];
  if (network.ip) parts.push(`<strong>IP:</strong> ${network.ip}`);
  if (network.activeMode === 1) parts.push(network.connected ? "🟢 Connected" : "🔴 Disconnected");
  $("network-info").innerHTML = parts.join(" | ");
}

function renderTimeInfo() {
  const { time } = state;
  const source = time.ntpSynced ? "NTP" : time.rtcPresent ? "RTC" : "internal (lost on power cycle)";
  $("time-info").textContent = time.valid
    ? `${String(time.hour).padStart(2, "0")}:${String(time.minute).padStart(2, "0")} · source: ${source}`
    : "Time not set";
}

function toggleNetworkMode() {
  $("clientConfig").hidden = $("networkMode").value !== "1";
}

function toggleActiveHours() {
  const enabled = $("activeHoursEnabled").checked;
  $("activeHoursConfig").style.opacity = enabled ? "1" : "0.5";
  $("activeHoursConfig").style.pointerEvents = enabled ? "auto" : "none";
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
  $("current-mode").textContent = state.display.mode;

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

document.addEventListener("DOMContentLoaded", () => {
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
});
