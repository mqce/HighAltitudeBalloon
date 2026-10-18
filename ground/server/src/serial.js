import { EventEmitter } from 'node:events';
import { SerialPort } from 'serialport';
import { ReadlineParser } from '@serialport/parser-readline';

const RECONNECT_MS = 3000;

export function parseLine(raw) {
  const line = raw.replace(/\r$/, '');
  if (line.startsWith('{')) {
    try {
      const obj = JSON.parse(line);
      if (obj && typeof obj.type === 'string') {
        return obj;
      }
    } catch {
      // fall through: treat as plain log
    }
  }
  return { type: 'log', msg: line };
}

export class SerialLink extends EventEmitter {
  constructor(path, baudRate = 115200) {
    super();
    this.path = path;
    this.baudRate = baudRate;
    this.port = null;
    this.timer = null;
    this.stopped = false;
    this.status = { connected: false, path, error: null };
  }

  start() {
    this.stopped = false;
    this.#open();
  }

  stop() {
    this.stopped = true;
    clearTimeout(this.timer);
    if (this.port?.isOpen) {
      this.port.close();
    }
  }

  #setStatus(connected, error = null) {
    this.status = { connected, path: this.path, error };
    this.emit('status', this.status);
  }

  #scheduleReconnect() {
    if (this.stopped) {
      return;
    }
    clearTimeout(this.timer);
    this.timer = setTimeout(() => this.#open(), RECONNECT_MS);
  }

  #open() {
    const port = new SerialPort({
      path: this.path,
      baudRate: this.baudRate,
      autoOpen: false,
    });
    const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));
    parser.on('data', (raw) => {
      if (raw.trim()) {
        this.emit('line', parseLine(raw));
      }
    });

    port.on('close', () => {
      this.#setStatus(false, this.stopped ? null : 'disconnected');
      this.#scheduleReconnect();
    });
    port.on('error', (err) => {
      this.#setStatus(false, err.message);
    });

    port.open((err) => {
      if (err) {
        this.#setStatus(false, err.message);
        this.#scheduleReconnect();
        return;
      }
      this.port = port;
      this.#setStatus(true);
    });
  }
}

export async function listPorts() {
  return SerialPort.list();
}
