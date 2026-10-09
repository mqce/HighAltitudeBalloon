import time
from dataclasses import dataclass

from pyubx2 import POLL, UBXMessage, UBXParseError, UBXReader
from smbus2 import SMBus, i2c_msg

_ADDR_BYTES_AVAILABLE = 0xFD
_ADDR_DATA = 0xFF
_RAM_LAYER = 1
_AIRBORNE_1G = 6
_CFG_VALSET_CLASS = 0x06
_CFG_VALSET_ID = 0x8A
_MAX_PAYLOAD = 4096
_READ_CHUNK = 256


@dataclass
class GnssFix:
    have: bool = False
    gnss_fix_ok: bool = False
    invalid_llh: bool = False
    time_valid: bool = False
    fix_type: int = 0
    siv: int = 0
    lat: float = 0.0
    lon: float = 0.0
    alt_m: float = 0.0
    year: int = 0
    month: int = 0
    day: int = 0
    hour: int = 0
    minute: int = 0
    second: int = 0
    updated_at: float = 0.0


def pop_ubx_messages(buf: bytearray) -> list:
    messages = []
    while True:
        start = buf.find(b"\xb5\x62")
        if start < 0:
            if len(buf) > 1:
                del buf[:-1]
            return messages
        if start:
            del buf[:start]
        if len(buf) < 8:
            return messages
        length = buf[4] | (buf[5] << 8)
        if length > _MAX_PAYLOAD:
            del buf[:2]
            continue
        total = 8 + length
        if len(buf) < total:
            return messages
        frame = bytes(buf[:total])
        try:
            messages.append(UBXReader.parse(frame))
        except UBXParseError:
            del buf[:2]
            continue
        del buf[:total]


class MaxM10s:
    def __init__(self, bus_id: int = 1, address: int = 0x42):
        self.bus_id = bus_id
        self.address = address
        self.fix = GnssFix()
        self._bus = None
        self._buf = bytearray()

    def close(self) -> None:
        self.fix = GnssFix()
        self._buf.clear()
        if self._bus is not None:
            self._bus.close()
            self._bus = None

    def start(self) -> bool:
        self.close()
        try:
            self._bus = SMBus(self.bus_id)
            if not self._probe():
                print(
                    "MAX-M10S not found. Check SDA/SCL, 3.3V, GND.",
                    flush=True,
                )
                self.close()
                return False
            ready = self._configure()
            if not ready:
                self.close()
            return ready
        except OSError:
            print("MAX-M10S not found. Check SDA/SCL, 3.3V, GND.", flush=True)
            self.close()
            return False

    def poll(self) -> None:
        self._buf.extend(self._read_pending())
        for msg in pop_ubx_messages(self._buf):
            if msg.identity == "NAV-PVT":
                self._capture(msg)

    def _probe(self) -> bool:
        poll = UBXMessage("MON", "MON-VER", POLL)
        self._write(poll.serialize())
        return self._wait(lambda msg: msg.identity == "MON-VER", 1.0)

    def _configure(self) -> bool:
        ubx_only = self._config(
            [
                ("CFG_I2COUTPROT_UBX", 1),
                ("CFG_I2COUTPROT_NMEA", 0),
            ]
        )
        model = self._config([("CFG_NAVSPG_DYNMODEL", _AIRBORNE_1G)])
        rate = self._config(
            [
                ("CFG_RATE_MEAS", 1000),
                ("CFG_RATE_NAV", 1),
            ]
        )
        auto_pvt = self._config([("CFG_MSGOUT_UBX_NAV_PVT_I2C", 1)])

        if not ubx_only:
            print("Warning: I2C UBX-only setting failed", flush=True)
        if not model:
            print("Warning: airborne <1g setting failed", flush=True)
        if not rate:
            print("Warning: 1 Hz navigation rate failed", flush=True)
        if not auto_pvt:
            print("Warning: automatic PVT failed", flush=True)
        if auto_pvt:
            print("MAX-M10S ready (UBX, airborne <1g, RAM only)", flush=True)
        return auto_pvt

    def _config(self, cfg: list) -> bool:
        self._read_pending_into_buf()
        msg = UBXMessage.config_set(_RAM_LAYER, 0, cfg)
        self._write(msg.serialize())
        result = {"ok": False}

        def on_ack(message) -> bool:
            if message.identity not in ("ACK-ACK", "ACK-NAK"):
                return False
            if message.clsID != _CFG_VALSET_CLASS or message.msgID != _CFG_VALSET_ID:
                return False
            result["ok"] = message.identity == "ACK-ACK"
            return True

        if not self._wait(on_ack, 1.0):
            return False
        return result["ok"]

    def _wait(self, predicate, timeout_s: float) -> bool:
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            self._buf.extend(self._read_pending())
            matched = False
            for msg in pop_ubx_messages(self._buf):
                if msg.identity == "NAV-PVT":
                    self._capture(msg)
                if predicate(msg):
                    matched = True
            if matched:
                return True
            time.sleep(0.02)
        return False

    def _read_pending_into_buf(self) -> None:
        chunk = self._read_pending()
        if not chunk:
            return
        self._buf.extend(chunk)
        for msg in pop_ubx_messages(self._buf):
            if msg.identity == "NAV-PVT":
                self._capture(msg)

    def _write(self, data: bytes) -> None:
        self._bus.i2c_rdwr(i2c_msg.write(self.address, data))

    def _bytes_available(self) -> int:
        write = i2c_msg.write(self.address, [_ADDR_BYTES_AVAILABLE])
        read = i2c_msg.read(self.address, 2)
        self._bus.i2c_rdwr(write, read)
        raw = bytes(read)
        count = (raw[0] << 8) | raw[1]
        if count == 0xFFFF:
            return 0
        return count

    def _read_pending(self) -> bytes:
        chunks = []
        total = 0
        while total < _MAX_PAYLOAD:
            count = self._bytes_available()
            if count <= 0:
                break
            count = min(count, _READ_CHUNK)
            write = i2c_msg.write(self.address, [_ADDR_DATA])
            read = i2c_msg.read(self.address, count)
            self._bus.i2c_rdwr(write, read)
            chunk = bytes(read)
            chunks.append(chunk)
            total += len(chunk)
        return b"".join(chunks)

    def _capture(self, msg) -> None:
        fix = self.fix
        fix.have = True
        fix.updated_at = time.monotonic()
        fix.lat = float(msg.lat)
        fix.lon = float(msg.lon)
        fix.alt_m = float(msg.hMSL) / 1000.0
        fix.siv = int(msg.numSV)
        fix.fix_type = int(msg.fixType)
        fix.gnss_fix_ok = bool(msg.gnssFixOk)
        fix.invalid_llh = bool(getattr(msg, "invalidLlh", 0))
        fix.time_valid = bool(msg.confirmedDate) and bool(msg.confirmedTime)
        fix.year = int(msg.year)
        fix.month = int(msg.month)
        fix.day = int(msg.day)
        fix.hour = int(msg.hour)
        fix.minute = int(msg.min)
        fix.second = int(msg.second)
