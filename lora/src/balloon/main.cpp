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
  Serial.println("=== balloon (TX) ===");
  Serial.printf("UART RX=%d TX=%d\n", LORA_RX_PIN, LORA_TX_PIN);

  if (!lora.begin(LORA_RX_PIN, LORA_TX_PIN)) {
    Serial.println("LoRa UART begin failed");
    while (true) {
      delay(1000);
    }
  }

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
}

void loop() {
  const char* msg = "hello";
  Serial.printf("TX: %s ... ", msg);

  if (lora.sendText(msg)) {
    Serial.println("OK");
  } else {
    Serial.println("FAIL");
  }

  delay(3000);
}
