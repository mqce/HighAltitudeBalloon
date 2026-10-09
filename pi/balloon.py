#!/usr/bin/env python3

import signal
import time

import serial

from max_m10s import GnssFix, MaxM10s
from tlm922s import Tlm922s

# GPIO14 TXD (pin 8) -> TLM922S UART_RX, GPIO15 RXD (pin 10) -> UART_TX
SERIAL_PORT = "/dev/serial0"
SERIAL_BAUD = 115200

# GPIO2 SDA (pin 3), GPIO3 SCL (pin 5)
I2C_BUS = 1
GPS_I2C_ADDR = 0x42

P2P_FREQ_HZ = 922_500_000
P2P_SF = 7
P2P_BW = 125
P2P_PWR_DBM = 14

SEND_INTERVAL_S = 5.0
STALE_S = 2.5
RETRY_INTERVAL_S = 3.0


def format_payload(fix: GnssFix) -> str:
    return (
        f"{fix.year:04d}{fix.month:02d}{fix.day:02d}"
        f"T{fix.hour:02d}{fix.minute:02d}{fix.second:02d}Z,"
        f"{fix.lat:.6f},{fix.lon:.6f},{fix.alt_m:.1f}"
    )


def fix_is_sendable(fix: GnssFix, now: float) -> bool:
    if not fix.have or not fix.time_valid:
        return False
    if now - fix.updated_at >= STALE_S:
        return False
    return fix.gnss_fix_ok and fix.fix_type == 3 and not fix.invalid_llh


def describe_fix(fix: GnssFix, now: float) -> str:
    if not fix.have:
        return "No GNSS solution yet."
    age_ms = int((now - fix.updated_at) * 1000)
    fresh = now - fix.updated_at < STALE_S
    has_fix = fresh and fix.gnss_fix_ok and fix.fix_type == 3 and not fix.invalid_llh
    if not fresh:
        state = "STALE"
    elif not has_fix:
        state = "NO FIX"
    elif not fix.time_valid:
        state = "NO TIME"
    else:
        state = "FIX"
    return (
        f"{state} type={fix.fix_type} ok={int(fix.gnss_fix_ok)} "
        f"sats={fix.siv} age={age_ms} ms"
    )


def _due(last, now: float) -> bool:
    return last is None or now - last >= RETRY_INTERVAL_S


def open_lora(lora: Tlm922s) -> bool:
    print("=== balloon (TX) ===", flush=True)
    print(f"UART {SERIAL_PORT} baud={SERIAL_BAUD}", flush=True)
    try:
        if not lora.begin():
            print(f"LoRa UART begin failed {lora.last_error}", flush=True)
            return False
        if not lora.configure_p2p(P2P_FREQ_HZ, P2P_SF, P2P_BW, P2P_PWR_DBM):
            print("P2P configure failed", flush=True)
            return False
    except serial.SerialException as exc:
        print(f"LoRa UART error: {exc}", flush=True)
        lora.end()
        return False
    print("P2P configure OK", flush=True)
    print(
        f"freq={P2P_FREQ_HZ} sf={P2P_SF} bw={P2P_BW} pwr={P2P_PWR_DBM}",
        flush=True,
    )
    return True


def main() -> None:
    stop = False

    def request_stop(_signum, _frame):
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    lora = Tlm922s(SERIAL_PORT, SERIAL_BAUD)
    gnss = MaxM10s(I2C_BUS, GPS_I2C_ADDR)
    lora_ready = False
    gnss_ready = False
    last_lora_retry = None
    last_gnss_retry = None
    next_tx = time.monotonic()

    try:
        while not stop:
            now = time.monotonic()

            if not gnss_ready and _due(last_gnss_retry, now):
                last_gnss_retry = now
                gnss_ready = gnss.start()

            if not lora_ready and _due(last_lora_retry, now):
                last_lora_retry = now
                lora_ready = open_lora(lora)

            if gnss_ready:
                try:
                    gnss.poll()
                except OSError as exc:
                    print(f"GNSS I2C error: {exc}", flush=True)
                    gnss.close()
                    gnss_ready = False
                    last_gnss_retry = time.monotonic()

            now = time.monotonic()
            if lora_ready and gnss_ready and now >= next_tx:
                next_tx = now + SEND_INTERVAL_S
                fix = gnss.fix
                if fix_is_sendable(fix, now):
                    payload = format_payload(fix)
                    print(f"TX: {payload} ... ", end="", flush=True)
                    try:
                        sent = lora.send_text(payload)
                    except serial.SerialException as exc:
                        print(f"FAIL {exc}", flush=True)
                        lora.end()
                        lora_ready = False
                        last_lora_retry = time.monotonic()
                    else:
                        if sent:
                            print("OK", flush=True)
                        else:
                            detail = f" {lora.last_error}" if lora.last_error else ""
                            print(f"FAIL{detail}", flush=True)
                else:
                    print(describe_fix(fix, now), flush=True)

            time.sleep(0.05)
    finally:
        lora.end()
        gnss.close()


if __name__ == "__main__":
    main()
