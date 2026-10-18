const GSI_ATTRIBUTION =
  '<a href="https://maps.gsi.go.jp/development/ichiran.html" target="_blank" rel="noopener">地理院タイル</a>';

const TILE_LAYERS = {
  標準地図: {
    url: 'https://cyberjapandata.gsi.go.jp/xyz/std/{z}/{x}/{y}.png',
    maxZoom: 18,
  },
  淡色地図: {
    url: 'https://cyberjapandata.gsi.go.jp/xyz/pale/{z}/{x}/{y}.png',
    maxZoom: 18,
  },
  写真: {
    url: 'https://cyberjapandata.gsi.go.jp/xyz/seamlessphoto/{z}/{x}/{y}.jpg',
    maxZoom: 18,
  },
};

const RAW_LIMIT = 200;
const STALE_WARN_S = 15;
const STALE_ALERT_S = 60;
const WS_RETRY_MS = 2000;

const $ = (id) => document.getElementById(id);

const state = {
  points: [],
  stats: null,
  follow: true,
};

const map = L.map('map', { zoomControl: true }).setView([36.0, 138.0], 5);
const baseLayers = Object.fromEntries(
  Object.entries(TILE_LAYERS).map(([name, cfg]) => [
    name,
    L.tileLayer(cfg.url, { maxZoom: cfg.maxZoom, attribution: GSI_ATTRIBUTION }),
  ]),
);
Object.values(baseLayers)[0].addTo(map);
L.control.layers(baseLayers).addTo(map);
L.control.scale({ imperial: false }).addTo(map);

const track = L.polyline([], { color: '#dc2626', weight: 3 }).addTo(map);
const marker = L.circleMarker([0, 0], {
  radius: 7,
  color: '#fff',
  weight: 2,
  fillColor: '#dc2626',
  fillOpacity: 1,
});

const FollowControl = L.Control.extend({
  options: { position: 'topleft' },
  onAdd() {
    const div = L.DomUtil.create('div', 'leaflet-bar leaflet-control-follow');
    const label = L.DomUtil.create('label', '', div);
    const input = L.DomUtil.create('input', '', label);
    input.type = 'checkbox';
    input.checked = state.follow;
    label.append('追従');
    L.DomEvent.disableClickPropagation(div);
    input.addEventListener('change', () => {
      state.follow = input.checked;
      const last = state.points.at(-1);
      if (state.follow && last) {
        map.panTo([last.lat, last.lon]);
      }
    });
    return div;
  },
});
new FollowControl().addTo(map);

const chartEl = $('alt-chart');
const chart = new uPlot(
  {
    width: chartEl.clientWidth,
    height: chartEl.clientHeight,
    legend: { show: false },
    scales: { x: { time: true } },
    series: [{}, { label: '高度', stroke: '#2563eb', width: 2, points: { show: false } }],
    axes: [
      {
        size: 30,
        values: (_u, ticks) =>
          ticks.map((t) =>
            new Date(t * 1000).toLocaleTimeString('ja-JP', { hour: '2-digit', minute: '2-digit' }),
          ),
      },
      { size: 64, values: (_u, ticks) => ticks.map((v) => `${v} m`) },
    ],
  },
  [[], []],
  chartEl,
);
new ResizeObserver(() => {
  chart.setSize({ width: chartEl.clientWidth, height: chartEl.clientHeight });
}).observe(chartEl);

function updateChart() {
  chart.setData([
    state.points.map((p) => Date.parse(p.time) / 1000),
    state.points.map((p) => p.alt),
  ]);
}

function fmt(value, digits, unit = '') {
  return Number.isFinite(value) ? `${value.toFixed(digits)}${unit}` : '-';
}

function formatTime(iso) {
  const d = new Date(iso);
  const utc = d.toISOString().slice(11, 19);
  const local = d.toLocaleTimeString('ja-JP', { hour12: false });
  return `${utc}Z (${local})`;
}

function renderLatest() {
  const p = state.points.at(-1);
  $('copy-coords').disabled = !p;
  if (!p) {
    return;
  }
  $('v-time').textContent = formatTime(p.time);
  $('v-lat').textContent = fmt(p.lat, 6);
  $('v-lon').textContent = fmt(p.lon, 6);
  $('v-alt').textContent = fmt(p.alt, 1, ' m');
  $('v-climb').textContent = fmt(p.climb, 1, ' m/s');
  $('v-signal').textContent = `${p.rssi ?? '-'} dBm / ${p.snr ?? '-'} dB`;
}

function renderStats() {
  const s = state.stats;
  if (!s) {
    return;
  }
  $('counts').textContent = `${s.rx} / ${s.rxFail}（パース失敗 ${s.parseError}）`;
  renderElapsed();
}

function renderElapsed() {
  const el = $('last-rx');
  const lastRxAt = state.stats?.lastRxAt;
  if (!lastRxAt) {
    el.textContent = '未受信';
    el.className = 'warn';
    return;
  }
  const sec = Math.max(0, Math.round((Date.now() - Date.parse(lastRxAt)) / 1000));
  el.textContent = `${sec} 秒前`;
  el.className = sec >= STALE_ALERT_S ? 'alert' : sec >= STALE_WARN_S ? 'warn' : 'ok';
}

function renderSerial(serial) {
  const el = $('serial-status');
  el.textContent = serial.connected
    ? `${serial.path} 接続`
    : `${serial.path} 切断${serial.error ? `（${serial.error}）` : ''}`;
  el.className = serial.connected ? 'ok' : 'alert';
}

function renderWs(connected) {
  const el = $('ws-status');
  el.textContent = connected ? '接続' : '切断（再接続中）';
  el.className = connected ? 'ok' : 'alert';
}

function addRaw(entry) {
  const list = $('raw-list');
  const li = document.createElement('li');
  li.className = `kind-${entry.kind}`;
  const time = new Date(entry.receivedAt).toLocaleTimeString('ja-JP', { hour12: false });
  li.textContent = `${time} [${entry.kind}] ${entry.text}`;
  list.prepend(li);
  while (list.childElementCount > RAW_LIMIT) {
    list.lastElementChild.remove();
  }
}

function addPoint(point, { redraw = true } = {}) {
  const isFirst = state.points.length === 0;
  state.points.push(point);
  const latlng = [point.lat, point.lon];
  track.addLatLng(latlng);
  marker.setLatLng(latlng);
  if (!map.hasLayer(marker)) {
    marker.addTo(map);
  }
  if (!redraw) {
    return;
  }
  if (isFirst) {
    map.setView(latlng, 12);
  } else if (state.follow) {
    map.panTo(latlng);
  }
  updateChart();
  renderLatest();
}

function applySnapshot(msg) {
  state.points = [];
  track.setLatLngs([]);
  marker.remove();
  $('raw-list').replaceChildren();

  for (const p of msg.points) {
    addPoint(p, { redraw: false });
  }
  const last = state.points.at(-1);
  if (state.points.length > 1) {
    map.fitBounds(track.getBounds(), { padding: [30, 30] });
  } else if (last) {
    map.setView([last.lat, last.lon], 12);
  }
  updateChart();
  renderLatest();

  for (const entry of msg.raw) {
    addRaw(entry);
  }
  state.stats = msg.stats;
  renderStats();
  renderSerial(msg.serial);
}

function handleMessage(msg) {
  switch (msg.type) {
    case 'snapshot':
      applySnapshot(msg);
      break;
    case 'point':
      addPoint(msg.point);
      break;
    case 'raw':
      addRaw(msg.entry);
      break;
    case 'stats':
      state.stats = msg.stats;
      renderStats();
      break;
    case 'serial':
      renderSerial(msg.serial);
      break;
  }
}

function connect() {
  const ws = new WebSocket(`ws://${location.host}/ws`);
  ws.addEventListener('open', () => renderWs(true));
  ws.addEventListener('message', (ev) => handleMessage(JSON.parse(ev.data)));
  ws.addEventListener('close', () => {
    renderWs(false);
    setTimeout(connect, WS_RETRY_MS);
  });
}

$('copy-coords').addEventListener('click', async () => {
  const p = state.points.at(-1);
  if (!p) {
    return;
  }
  const btn = $('copy-coords');
  try {
    await navigator.clipboard.writeText(`${p.lat.toFixed(6)},${p.lon.toFixed(6)}`);
    btn.textContent = 'コピー済み';
  } catch {
    btn.textContent = 'コピー失敗';
  }
  setTimeout(() => {
    btn.textContent = '座標コピー';
  }, 1500);
});

setInterval(renderElapsed, 1000);
connect();
