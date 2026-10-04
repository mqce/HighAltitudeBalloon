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
  Serial.println("=== ground (module query) ===");
  Serial.printf("UART RX=%d TX=%d\n", LORA_RX_PIN, LORA_TX_PIN);

  if (!lora.begin(LORA_RX_PIN, LORA_TX_PIN)) {
    Serial.println("LoRa UART begin failed");
    return;
  }

  // Read-only. No p2p tx / rx, so a peer is not required.
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
}

void loop() {}
