#include "tlm922s.h"

Tlm922s::Tlm922s(HardwareSerial& serial) : serial_(serial) {}

bool Tlm922s::begin(int rxPin, int txPin, uint32_t baud) {
  serial_.end();
  delay(20);
  serial_.begin(baud, SERIAL_8N1, rxPin, txPin);
  delay(500);
  drainInput();
  return true;
}

void Tlm922s::end() {
  serial_.end();
}

bool Tlm922s::query(const char* cmd, String& response, uint32_t timeoutMs) {
  response = "";
  drainInput();
  sendCommand(cmd);

  String buf;
  const uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (serial_.available()) {
      const char c = static_cast<char>(serial_.read());
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        const int mark = buf.indexOf(">>");
        if (mark >= 0) {
          response = buf.substring(mark + 2);
          response.trim();
          const bool rejected = response.indexOf("Unknown") >= 0 ||
                                response.indexOf("Invalid") >= 0;
          return !rejected && response.length() > 0;
        }
        buf = "";
      } else {
        buf += c;
        if (buf.length() > 160) {
          buf.remove(0, buf.length() - 80);
        }
      }
    }
    delay(1);
  }

  response = buf;
  response.trim();
  return false;
}

bool Tlm922s::uartLoopbackTest(int rxPin, int txPin) {
  begin(rxPin, txPin);
  delay(50);
  drainInput();

  const char* msg = "LOOP";
  for (size_t i = 0; msg[i] != '\0'; ++i) {
    serial_.write(static_cast<uint8_t>(msg[i]));
  }
  serial_.flush();

  String got;
  const uint32_t start = millis();
  while (millis() - start < 300) {
    while (serial_.available()) {
      got += static_cast<char>(serial_.read());
    }
    delay(1);
  }

  Serial.printf("uart loopback RX=%d TX=%d got=\"%s\"\n", rxPin, txPin,
                escapeForLog(got).c_str());
  return got.indexOf(msg) >= 0;
}

bool Tlm922s::probe(uint32_t timeoutMs) {
  drainInput();
  if (!waitReady(timeoutMs)) {
    const String raw = dumpInput(200);
    Serial.printf("probe: no ready prompt. raw=\"%s\"\n",
                  escapeForLog(raw).c_str());
    return false;
  }

  String captured;
  if (!execCommand("p2p get_freq", "", 1000, &captured)) {
    // get_freq returns a number line, not Ok
  }

  // Accept either a frequency digit line or any non-empty response.
  if (captured.length() == 0) {
    captured = dumpInput(200);
  }

  Serial.printf("probe: response=\"%s\"\n", escapeForLog(captured).c_str());
  for (unsigned i = 0; i < captured.length(); ++i) {
    if (isDigit(captured.charAt(i))) {
      return true;
    }
  }
  return captured.indexOf("Ok") >= 0 || captured.indexOf(">>") >= 0;
}

bool Tlm922s::configureP2p(uint32_t freqHz, int sf, int bwKhz, int pwrDbm) {
  const String commands[] = {
      "p2p set_freq " + String(freqHz),
      "p2p set_sf " + String(sf),
      "p2p set_bw " + String(bwKhz),
      "p2p set_pwr " + String(pwrDbm),
      "p2p set_sync 12",
      "p2p set_crc on",
  };

  for (const String& cmd : commands) {
    String captured;
    Serial.printf("CMD: %s ... ", cmd.c_str());
    if (!execCommand(cmd, "Ok", 1500, &captured)) {
      Serial.printf("FAIL resp=\"%s\"\n", escapeForLog(captured).c_str());
      if (captured.isEmpty()) {
        Serial.println("  (no UART response: check TX/RX cross, 3V3, GND, MD0=LOW)");
      }
      return false;
    }
    Serial.println("OK");
  }

  return true;
}

bool Tlm922s::sendText(const char* text) {
  const String hex = toHex(text);
  if (!execCommand("p2p tx " + hex, "Ok", 1500)) {
    return false;
  }
  return waitForToken("radio_tx_ok", 5000);
}

bool Tlm922s::receive(uint16_t windowMs, TlmRxPacket& out, String& failure) {
  out = {};
  failure = "";

  sendCommand("p2p rx " + String(windowMs));

  String buf;
  bool sawOk = false;
  bool sawBusy = false;
  const uint32_t timeoutMs = static_cast<uint32_t>(windowMs) + 2000;
  const uint32_t start = millis();

  while (millis() - start < timeoutMs) {
    while (serial_.available()) {
      const char c = static_cast<char>(serial_.read());
      if (c == '\r') {
        continue;
      }
      if (c != '\n') {
        buf += c;
        if (buf.length() > 256) {
          buf.remove(0, buf.length() - 128);
        }
        continue;
      }

      String line = buf;
      buf = "";
      line.trim();
      if (line.isEmpty()) {
        continue;
      }

      if (line.indexOf("radio_rx") >= 0) {
        if (!parseRxLine(line, out)) {
          failure = line;
          return false;
        }
        return true;
      }
      if (line.indexOf("radio_err_timeout") >= 0) {
        failure = "radio_err_timeout";
        return false;
      }
      if (line.indexOf("radio_err") >= 0 || line.indexOf("Invalid") >= 0) {
        failure = line;
        return false;
      }
      if (line.indexOf("busy") >= 0) {
        sawBusy = true;
        continue;
      }
      if (line.indexOf("Ok") >= 0) {
        sawOk = true;
      }
    }
    delay(1);
  }

  if (sawBusy) {
    failure = "busy";
  } else if (!sawOk) {
    failure = buf.isEmpty() ? "no response" : buf;
  } else {
    failure = "no radio_rx";
  }
  return false;
}

bool Tlm922s::parseRxLine(const String& line, TlmRxPacket& out) {
  const int rxPos = line.indexOf("radio_rx");
  if (rxPos < 0) {
    return false;
  }

  String rest = line.substring(rxPos + 8);
  rest.trim();

  const int sp1 = rest.indexOf(' ');
  if (sp1 < 0) {
    out.hexPayload = rest;
    out.textPayload = hexToText(rest);
    out.rssi = 0;
    out.snr = 0;
    return !out.hexPayload.isEmpty();
  }

  out.hexPayload = rest.substring(0, sp1);
  String afterHex = rest.substring(sp1 + 1);
  afterHex.trim();

  const int sp2 = afterHex.indexOf(' ');
  if (sp2 < 0) {
    out.rssi = afterHex.toInt();
    out.snr = 0;
  } else {
    out.rssi = afterHex.substring(0, sp2).toInt();
    out.snr = afterHex.substring(sp2 + 1).toInt();
  }

  out.textPayload = hexToText(out.hexPayload);
  return !out.hexPayload.isEmpty();
}

void Tlm922s::drainInput() {
  while (serial_.available()) {
    serial_.read();
  }
}

bool Tlm922s::waitReady(uint32_t timeoutMs) {
  drainInput();
  serial_.print('\r');
  serial_.flush();

  // Module may answer with ">>" prompt / ready-like traffic.
  String buf;
  const uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (serial_.available()) {
      const char c = static_cast<char>(serial_.read());
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        if (buf.indexOf(">>") >= 0 || buf.indexOf("Ok") >= 0 ||
            buf.indexOf("invalid") >= 0) {
          return true;
        }
        buf = "";
      } else {
        buf += c;
        if (buf.indexOf(">>") >= 0) {
          return true;
        }
        if (buf.length() > 64) {
          buf.remove(0, buf.length() - 32);
        }
      }
    }
    delay(1);
  }
  return false;
}

void Tlm922s::sendCommand(const String& cmd) {
  serial_.print(cmd);
  serial_.print('\r');
  serial_.flush();
}

bool Tlm922s::execCommand(const String& cmd, const char* token, uint32_t timeoutMs,
                          String* captured) {
  drainInput();
  sendCommand(cmd);
  if (token == nullptr || token[0] == '\0') {
    // Capture one response line / any traffic.
    String buf;
    const uint32_t start = millis();
    while (millis() - start < timeoutMs) {
      while (serial_.available()) {
        const char c = static_cast<char>(serial_.read());
        if (c == '\r') {
          continue;
        }
        if (c == '\n') {
          if (captured) {
            *captured = buf;
          }
          return !buf.isEmpty();
        }
        buf += c;
      }
      delay(1);
    }
    if (captured) {
      *captured = buf;
    }
    return !buf.isEmpty();
  }
  return waitForToken(token, timeoutMs, captured);
}

bool Tlm922s::waitForToken(const char* token, uint32_t timeoutMs, String* captured) {
  String buf;
  const uint32_t start = millis();

  while (millis() - start < timeoutMs) {
    while (serial_.available()) {
      const char c = static_cast<char>(serial_.read());
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        if (buf.indexOf(token) >= 0) {
          if (captured) {
            *captured = buf;
          }
          return true;
        }
        if (buf.indexOf("Invalid") >= 0 || buf.indexOf("radio_err") >= 0) {
          if (captured) {
            *captured = buf;
          }
          return false;
        }
        buf = "";
      } else {
        buf += c;
        if (buf.length() > 256) {
          buf.remove(0, buf.length() - 128);
        }
      }
    }
    delay(1);
  }

  if (captured) {
    *captured = buf;
  }
  return false;
}

String Tlm922s::dumpInput(uint32_t windowMs) {
  String out;
  const uint32_t start = millis();
  while (millis() - start < windowMs) {
    while (serial_.available()) {
      out += static_cast<char>(serial_.read());
      if (out.length() > 200) {
        return out;
      }
    }
    delay(1);
  }
  return out;
}

String Tlm922s::toHex(const char* text) {
  String hex;
  for (size_t i = 0; text[i] != '\0'; ++i) {
    char tmp[3];
    snprintf(tmp, sizeof(tmp), "%02x", static_cast<uint8_t>(text[i]));
    hex += tmp;
  }
  return hex;
}

String Tlm922s::hexToText(const String& hex) {
  String text;
  for (int i = 0; i + 1 < hex.length(); i += 2) {
    const char high = hex.charAt(i);
    const char low = hex.charAt(i + 1);
    auto nibble = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    const int hi = nibble(high);
    const int lo = nibble(low);
    if (hi < 0 || lo < 0) {
      break;
    }
    text += static_cast<char>((hi << 4) | lo);
  }
  return text;
}

String Tlm922s::escapeForLog(const String& s) {
  String out;
  for (unsigned i = 0; i < s.length(); ++i) {
    const char c = s.charAt(i);
    if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c < 32 || c > 126) {
      char tmp[5];
      snprintf(tmp, sizeof(tmp), "\\x%02x", static_cast<uint8_t>(c));
      out += tmp;
    } else {
      out += c;
    }
  }
  return out;
}
