import fs from 'node:fs';
import path from 'node:path';

function timestampForFile(date) {
  const pad = (n) => String(n).padStart(2, '0');
  return (
    `${date.getFullYear()}${pad(date.getMonth() + 1)}${pad(date.getDate())}-` +
    `${pad(date.getHours())}${pad(date.getMinutes())}${pad(date.getSeconds())}`
  );
}

export class JsonlLogger {
  constructor(dir) {
    fs.mkdirSync(dir, { recursive: true });
    this.filePath = path.join(dir, `${timestampForFile(new Date())}.jsonl`);
    this.stream = fs.createWriteStream(this.filePath, { flags: 'a' });
  }

  write(record) {
    this.stream.write(`${JSON.stringify(record)}\n`);
  }

  close() {
    return new Promise((resolve) => this.stream.end(resolve));
  }
}
