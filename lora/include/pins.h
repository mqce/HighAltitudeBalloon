#pragma once

// ESP32-C3 SuperMini UART to TLM922S (crossed)
#ifndef LORA_RX_PIN
#define LORA_RX_PIN 0
#endif

#ifndef LORA_TX_PIN
#define LORA_TX_PIN 1
#endif

#ifndef P2P_FREQ_HZ
#define P2P_FREQ_HZ 922500000UL
#endif

#ifndef P2P_SF
#define P2P_SF 7
#endif

#ifndef P2P_BW
#define P2P_BW 125
#endif

#ifndef P2P_PWR_DBM
#define P2P_PWR_DBM 14
#endif
