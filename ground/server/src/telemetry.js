const PAYLOAD_RE =
  /^(\d{4})(\d{2})(\d{2})T(\d{2})(\d{2})(\d{2})Z,(-?\d+(?:\.\d+)?),(-?\d+(?:\.\d+)?),(-?\d+(?:\.\d+)?)$/;

const MAX_POINTS = 50000;

export function hexToText(hex) {
  if (typeof hex !== 'string' || !/^(?:[0-9a-fA-F]{2})*$/.test(hex)) {
    return null;
  }
  return Buffer.from(hex, 'hex').toString('latin1');
}

export function parsePayload(text) {
  const m = PAYLOAD_RE.exec(text.trim());
  if (!m) {
    return null;
  }
  const [, y, mo, d, h, mi, s, latStr, lonStr, altStr] = m;
  const timeMs = Date.UTC(+y, +mo - 1, +d, +h, +mi, +s);
  const lat = Number(latStr);
  const lon = Number(lonStr);
  const alt = Number(altStr);
  const valid =
    Number.isFinite(timeMs) &&
    Math.abs(lat) <= 90 &&
    Math.abs(lon) <= 180 &&
    Number.isFinite(alt);
  return valid ? { time: new Date(timeMs).toISOString(), lat, lon, alt } : null;
}

export class Telemetry {
  constructor() {
    this.points = [];
    this.stats = { rx: 0, rxFail: 0, parseError: 0, lastRxAt: null };
  }

  addRx(line, receivedAt) {
    this.stats.rx += 1;
    this.stats.lastRxAt = receivedAt;

    const text = hexToText(line.hex);
    const fix = text === null ? null : parsePayload(text);
    if (!fix) {
      this.stats.parseError += 1;
      return { text, point: null };
    }

    const point = {
      receivedAt,
      ...fix,
      climb: this.#climbFrom(fix),
      rssi: Number.isFinite(line.rssi) ? line.rssi : null,
      snr: Number.isFinite(line.snr) ? line.snr : null,
    };
    this.points.push(point);
    if (this.points.length > MAX_POINTS) {
      this.points.shift();
    }
    return { text, point };
  }

  addRxFail() {
    this.stats.rxFail += 1;
  }

  #climbFrom(fix) {
    const prev = this.points.at(-1);
    if (!prev) {
      return null;
    }
    const dt = (Date.parse(fix.time) - Date.parse(prev.time)) / 1000;
    return dt > 0 ? (fix.alt - prev.alt) / dt : null;
  }
}
