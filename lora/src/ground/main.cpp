#include <Arduino.h>
#include <HardwareSerial.h>

#include "pins.h"
#include "tlm922s.h"

// ESP32-C3 UART1. USB CDC (Serial) stays free for the monitor.
Tlm922s lora(Serial1);

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("=== ground (RX) ===");
  Serial.printf("UART RX=%d TX=%d\n", LORA_RX_PIN, LORA_TX_PIN);

  if (!lora.begin(LORA_RX_PIN, LORA_TX_PIN)) {
    Serial.println("LoRa UART begin failed");
    while (true) {
      delay(1000);
    }
  }

  const char* commands[] = {
      "mod get_ver",
      "mod get_hw_model",
      "p2p get_freq",
      "p2p get_sf",
      "p2p get_bw",
      "p2p get_pwr",
      "p2p get_crc",
      "p2p get_sync",
  };

  for (const char* cmd : commands) {
    String response;
    const bool ok = lora.query(cmd, response);
    Serial.printf("%s => \"%s\" %s\n", cmd, response.c_str(), ok ? "OK" : "FAIL");
  }

  Serial.println("query done");

  if (!lora.configureP2p(P2P_FREQ_HZ, P2P_SF, P2P_BW, P2P_PWR_DBM)) {
    Serial.println("P2P configure failed");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("P2P configure OK");
  Serial.printf("freq=%lu sf=%d bw=%d pwr=%d\n",
                static_cast<unsigned long>(P2P_FREQ_HZ), P2P_SF, P2P_BW,
                P2P_PWR_DBM);
  Serial.println("Waiting for packets...");
}

void loop() {
  TlmRxPacket pkt;
  String failure;
  constexpr uint16_t kRxWindowMs = 10000;

  if (!lora.receive(kRxWindowMs, pkt, failure)) {
    Serial.printf("RX fail: %s\n", failure.c_str());
    return;
  }

  Serial.printf("RX text=\"%s\" hex=%s rssi=%d snr=%d\n",
                pkt.textPayload.c_str(), pkt.hexPayload.c_str(), pkt.rssi,
                pkt.snr);
}
