import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';
import express from 'express';
import { WebSocketServer } from 'ws';

import { JsonlLogger } from './logger.js';
import { SerialLink, listPorts } from './serial.js';
import { Telemetry } from './telemetry.js';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const RAW_HISTORY = 200;

const { values: cli } = parseArgs({
  options: {
    port: { type: 'string', short: 'p' },
    host: { type: 'string' },
    'http-port': { type: 'string' },
    list: { type: 'boolean' },
  },
});
const args = {
  port: cli.port ?? process.env.SERIAL_PORT,
  host: cli.host ?? process.env.HOST ?? '127.0.0.1',
  httpPort: Number(cli['http-port'] ?? process.env.HTTP_PORT ?? 8080),
  list: cli.list ?? false,
};

if (args.list) {
  const ports = await listPorts();
  for (const p of ports) {
    console.log(`${p.path}\t${p.manufacturer ?? ''}\t${p.friendlyName ?? ''}`);
  }
  process.exit(0);
}

if (!args.port) {
  console.error('Serial port is required: npm start -- --port COM5');
  console.error('List ports: npm run ports');
  process.exit(1);
}

const telemetry = new Telemetry();
const logger = new JsonlLogger(path.join(ROOT, 'logs'));
const rawHistory = [];
const serial = new SerialLink(args.port);

const app = express();
app.disable('x-powered-by');
app.use(express.static(path.join(ROOT, 'public')));
app.use('/vendor/leaflet', express.static(path.join(ROOT, 'node_modules/leaflet/dist')));
app.use('/vendor/uplot', express.static(path.join(ROOT, 'node_modules/uplot/dist')));

const server = http.createServer(app);
const wss = new WebSocketServer({
  server,
  path: '/ws',
  verifyClient: ({ origin, req }) => origin === `http://${req.headers.host}`,
});

function broadcast(message) {
  const data = JSON.stringify(message);
  for (const client of wss.clients) {
    if (client.readyState === client.OPEN) {
      client.send(data);
    }
  }
}

function pushRaw(entry) {
  rawHistory.push(entry);
  if (rawHistory.length > RAW_HISTORY) {
    rawHistory.shift();
  }
  broadcast({ type: 'raw', entry });
}

function describe(line, text) {
  switch (line.type) {
    case 'rx':
      return `${text ?? `hex=${line.hex}`}  rssi=${line.rssi} snr=${line.snr}`;
    case 'rx_fail':
      return String(line.reason ?? '');
    default:
      return String(line.msg ?? JSON.stringify(line));
  }
}

serial.on('line', (line) => {
  const receivedAt = new Date().toISOString();
  logger.write({ receivedAt, ...line });

  let text = null;
  let kind = line.type;
  if (line.type === 'rx') {
    const result = telemetry.addRx(line, receivedAt);
    text = result.text;
    if (result.point) {
      broadcast({ type: 'point', point: result.point });
    } else {
      kind = 'parse_error';
    }
  } else if (line.type === 'rx_fail') {
    telemetry.addRxFail();
  }

  pushRaw({ receivedAt, kind, text: describe(line, text) });
  broadcast({ type: 'stats', stats: telemetry.stats });
});

serial.on('status', (status) => {
  logger.write({ receivedAt: new Date().toISOString(), type: 'serial', ...status });
  broadcast({ type: 'serial', serial: status });
});

wss.on('connection', (ws) => {
  ws.send(
    JSON.stringify({
      type: 'snapshot',
      points: telemetry.points,
      raw: rawHistory,
      stats: telemetry.stats,
      serial: serial.status,
    }),
  );
});

server.listen(args.httpPort, args.host, () => {
  console.log(`Ground station UI: http://${args.host}:${args.httpPort}/`);
  console.log(`Serial: ${args.port}  Log: ${logger.filePath}`);
  serial.start();
});

async function shutdown() {
  serial.stop();
  wss.close();
  server.close();
  await logger.close();
  process.exit(0);
}

process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
