#include <Arduino.h>
#include <HardwareSerial.h>

#include "pins.h"
#include "tlm922s.h"

// ESP32-C3 UART1. USB CDC (Serial) carries JSON Lines to the PC.
Tlm922s lora(Serial1);

namespace {

String jsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (unsigned i = 0; i < s.length(); ++i) {
    const char c = s.charAt(i);
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (static_cast<uint8_t>(c) < 0x20 || static_cast<uint8_t>(c) > 0x7e) {
      char tmp[7];
      snprintf(tmp, sizeof(tmp), "\\u%04x", static_cast<uint8_t>(c));
      out += tmp;
    } else {
      out += c;
    }
  }
  return out;
}

void emitLog(const String& msg) {
  Serial.printf("{\"type\":\"log\",\"msg\":\"%s\"}\n", jsonEscape(msg).c_str());
}

void emitRx(const TlmRxPacket& pkt) {
  Serial.printf("{\"type\":\"rx\",\"hex\":\"%s\",\"rssi\":%d,\"snr\":%d}\n",
                jsonEscape(pkt.hexPayload).c_str(), pkt.rssi, pkt.snr);
}

void emitRxFail(const String& reason) {
  Serial.printf("{\"type\":\"rx_fail\",\"reason\":\"%s\"}\n",
                jsonEscape(reason).c_str());
}

[[noreturn]] void halt(const char* msg) {
  while (true) {
    emitLog(msg);
    delay(5000);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);

  emitLog("=== ground (RX) ===");
  emitLog("UART RX=" + String(LORA_RX_PIN) + " TX=" + String(LORA_TX_PIN));

  if (!lora.begin(LORA_RX_PIN, LORA_TX_PIN)) {
    halt("LoRa UART begin failed");
  }

  const char* commands[] = {
      "mod get_ver", "mod get_hw_model", "p2p get_freq", "p2p get_sf",
      "p2p get_bw",  "p2p get_pwr",      "p2p get_crc",  "p2p get_sync",
  };
  for (const char* cmd : commands) {
    String response;
    const bool ok = lora.query(cmd, response);
    emitLog(String(cmd) + " => \"" + response + "\" " + (ok ? "OK" : "FAIL"));
  }

  if (!lora.configureP2p(P2P_FREQ_HZ, P2P_SF, P2P_BW, P2P_PWR_DBM)) {
    halt("P2P configure failed");
  }

  emitLog("P2P configure OK");
  emitLog("freq=" + String(static_cast<unsigned long>(P2P_FREQ_HZ)) +
          " sf=" + String(P2P_SF) + " bw=" + String(P2P_BW) +
          " pwr=" + String(P2P_PWR_DBM));
  emitLog("Waiting for packets...");
}

void loop() {
  TlmRxPacket pkt;
  String failure;
  constexpr uint16_t kRxWindowMs = 10000;

  if (lora.receive(kRxWindowMs, pkt, failure)) {
    emitRx(pkt);
  } else {
    emitRxFail(failure);
  }
}
