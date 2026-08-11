const state = { distance: 2.5, feed: 300, busy: false, microsteps: 1, captureMs: 100 };
const jogButtons = [...document.querySelectorAll(".jog")];
const connection = document.querySelector("#connection");
const modeToggle = document.querySelector("#active-only");
const driveMode = document.querySelector("#drive-mode");
const penButtons = [document.querySelector("#pen-up"), document.querySelector("#pen-down")];
function setBusy(busy) {
  state.busy = busy;
  jogButtons.forEach((button) => { button.disabled = busy; });
  modeToggle.disabled = busy;
  driveMode.disabled = busy;
  penButtons.forEach((button) => { button.disabled = busy; });
}
async function movePen(penState) {
  if (state.busy) return;
  setBusy(true);
  try {
    await request(`/api/pen.cgi?state=${penState}`);
    await refreshStatus();
  } catch (error) {
    connection.textContent = "切断";
    connection.classList.add("offline");
    setBusy(false);
  }
}
async function request(path) {
  const separator = path.includes("?") ? "&" : "?";
  const response = await fetch(`${path}${separator}_=${Date.now()}`, { cache: "no-store" });
  if (!response.ok) throw new Error(`HTTP ${response.status}`);
  return response;
}
async function jog(axis, direction) {
  if (state.busy) return;
  setBusy(true);
  const query = new URLSearchParams({ axis, direction, distance: state.distance, feed: state.feed });
  try { await request(`/api/jog.cgi?${query}`); await refreshStatus(); }
  catch (error) { connection.textContent = "切断"; connection.classList.add("offline"); setBusy(false); }
}
async function stop() {
  try { await request("/api/stop.cgi"); await refreshStatus(); }
  catch (error) { connection.textContent = "切断"; connection.classList.add("offline"); }
}
function selectDistance(distance) {
  state.distance = distance;
  document.querySelectorAll("[data-distance]").forEach((item) => {
    item.classList.toggle("selected", Number(item.dataset.distance) === distance);
  });
}
async function setDriveConfig(activeOnly, microsteps) {
  if (state.busy) return;
  state.microsteps = microsteps;
  state.captureMs = 100;
  selectDistance(microsteps === 1 ? 2.5 : 0.625);
  setBusy(true);
  try {
    const query = new URLSearchParams({
      activeOnly: activeOnly ? 1 : 0,
      microsteps,
      captureMs: state.captureMs,
    });
    await request(`/api/mode.cgi?${query}`);
    await refreshStatus();
  } catch (error) {
    connection.textContent = "切断";
    connection.classList.add("offline");
    setBusy(false);
  }
}
async function refreshStatus() {
  try {
    const response = await request("/api/status.shtml");
    const status = await response.json();
    connection.textContent = "接続中"; connection.classList.remove("offline");
    document.querySelector("#x-position").textContent = Number(status.x).toFixed(4);
    document.querySelector("#y-position").textContent = Number(status.y).toFixed(4);
    document.querySelector("#z-position").textContent = Number(status.z).toFixed(4);
    document.querySelector("#motion-state").textContent = status.busy ? "実行中" : "待機";
    document.querySelector("#output-state").textContent = status.outputs ? "出力 ON" : "出力 Hi-Z";
    document.querySelector("#last-result").textContent = status.lastResult;
    modeToggle.checked = Boolean(status.activeOnly);
    state.microsteps = Number(status.microsteps) || 1;
    state.captureMs = Number(status.captureMs) || 0;
    driveMode.value = String(state.microsteps);
    document.querySelector("#drive-state").textContent = `${state.microsteps === 1 ? "確実動作" : "滑らか動作"} U${state.microsteps} / 捕捉${state.captureMs}ms`;
    const activeAxis = status.activeMask === 1 ? "X" : status.activeMask === 2 ? "Y" : status.activeMask === 3 ? "XY" : "なし";
    document.querySelector("#phase-state").textContent = `相: X ${status.xPhase} / Y ${status.yPhase} / 対象 ${activeAxis}`;
    document.querySelector("#pen-state").textContent =
      `${status.penUp ? "上" : "下"} / ${Number(status.penPulseUs)}µs / GP12`;
    setBusy(status.busy);
  } catch (error) { connection.textContent = "切断"; connection.classList.add("offline"); setBusy(false); }
}
jogButtons.forEach((button) => button.addEventListener("click", () => jog(button.dataset.axis, button.dataset.direction)));
document.querySelector("#stop").addEventListener("click", stop);
document.querySelector("#pen-up").addEventListener("click", () => movePen("up"));
document.querySelector("#pen-down").addEventListener("click", () => movePen("down"));
modeToggle.addEventListener("change", () => setDriveConfig(modeToggle.checked, state.microsteps));
driveMode.addEventListener("change", () => setDriveConfig(modeToggle.checked, Number(driveMode.value)));
document.querySelectorAll("[data-distance]").forEach((button) => button.addEventListener("click", () => {
  selectDistance(Number(button.dataset.distance));
}));
document.querySelector("#feed").addEventListener("input", (event) => {
  state.feed = Number(event.target.value); document.querySelector("#feed-value").textContent = state.feed;
});
refreshStatus(); setInterval(refreshStatus, 250);
