import time

import serial


def _escape(text: str) -> str:
    parts = []
    for char in text:
        code = ord(char)
        if char == "\n":
            parts.append("\\n")
        elif char == "\r":
            parts.append("\\r")
        elif code < 32 or code > 126:
            parts.append(f"\\x{code:02x}")
        else:
            parts.append(char)
    return "".join(parts)


class Tlm922s:
    def __init__(self, port: str, baud: int = 115200):
        self.port = port
        self.baud = baud
        self.last_error = ""
        self._ser = None

    def begin(self) -> bool:
        self.end()
        time.sleep(0.02)
        try:
            self._ser = serial.Serial(
                self.port, self.baud, timeout=0, exclusive=True
            )
        except serial.SerialException as exc:
            self.last_error = str(exc)
            return False
        time.sleep(0.5)
        self._drain()
        return True

    def end(self) -> None:
        if self._ser is not None:
            self._ser.close()
            self._ser = None

    def configure_p2p(self, freq_hz: int, sf: int, bw_khz: int, pwr_dbm: int) -> bool:
        commands = [
            f"p2p set_freq {freq_hz}",
            f"p2p set_sf {sf}",
            f"p2p set_bw {bw_khz}",
            f"p2p set_pwr {pwr_dbm}",
            "p2p set_sync 12",
            "p2p set_crc on",
        ]
        for cmd in commands:
            print(f"CMD: {cmd} ... ", end="", flush=True)
            ok, captured = self._exec(cmd, "Ok", 1.5)
            if not ok:
                shown = _escape(captured)
                self.last_error = shown
                print(f'FAIL resp="{shown}"', flush=True)
                if not captured:
                    print(
                        "  (no UART response: check TX/RX cross, 3V3, GND, MD0=LOW)",
                        flush=True,
                    )
                return False
            print("OK", flush=True)
        self.last_error = ""
        return True

    def send_text(self, text: str) -> bool:
        hex_payload = text.encode("ascii").hex()
        ok, captured = self._exec(f"p2p tx {hex_payload}", "Ok", 1.5)
        if not ok:
            self.last_error = _escape(captured) or "no Ok"
            return False
        ok, captured = self._wait_for_token("radio_tx_ok", 5.0)
        if not ok:
            self.last_error = _escape(captured) or "no radio_tx_ok"
            return False
        self.last_error = ""
        return True

    def _exec(self, cmd: str, token: str, timeout_s: float) -> tuple:
        self._drain()
        self._send(cmd)
        return self._wait_for_token(token, timeout_s)

    def _send(self, cmd: str) -> None:
        self._ser.write(cmd.encode("ascii") + b"\r")
        self._ser.flush()

    def _drain(self) -> None:
        while self._ser.in_waiting:
            self._ser.read(self._ser.in_waiting)

    def _wait_for_token(self, token: str, timeout_s: float) -> tuple:
        buf = ""
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            if not self._ser.in_waiting:
                time.sleep(0.001)
                continue
            raw = self._ser.read(1)
            if not raw:
                continue
            char = raw.decode("latin-1")
            if char == "\r":
                continue
            if char != "\n":
                buf += char
                if len(buf) > 256:
                    buf = buf[-128:]
                continue
            if token in buf:
                return True, buf
            if "Invalid" in buf or "radio_err" in buf:
                return False, buf
            buf = ""
        return False, buf
