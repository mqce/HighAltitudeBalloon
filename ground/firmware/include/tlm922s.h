#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>

struct TlmRxPacket {
  String hexPayload;
  String textPayload;
  int rssi;
  int snr;
};

class Tlm922s {
 public:
  explicit Tlm922s(HardwareSerial& serial);

  bool begin(int rxPin, int txPin, uint32_t baud = 115200);
  void end();
  bool query(const char* cmd, String& response, uint32_t timeoutMs = 1000);
  bool probe(uint32_t timeoutMs = 800);
  bool configureP2p(uint32_t freqHz, int sf, int bwKhz, int pwrDbm);
  bool uartLoopbackTest(int rxPin, int txPin);
  bool sendText(const char* text);
  bool receive(uint16_t windowMs, TlmRxPacket& out, String& failure);

 private:
  HardwareSerial& serial_;

  void drainInput();
  bool waitReady(uint32_t timeoutMs = 300);
  void sendCommand(const String& cmd);
  bool parseRxLine(const String& line, TlmRxPacket& out);
  bool execCommand(const String& cmd, const char* token, uint32_t timeoutMs,
                   String* captured = nullptr);
  bool waitForToken(const char* token, uint32_t timeoutMs, String* captured = nullptr);
  String dumpInput(uint32_t windowMs);
  static String toHex(const char* text);
  static String hexToText(const String& hex);
  static String escapeForLog(const String& s);
};
