#include <Arduino.h>
#include <TinyGPSPlus.h>

// Switch Science MAX-M10S (SSCI-10437) UART wiring
static constexpr int GPS_RX_PIN = 20;  // ESP32 RX <- GPS TX
static constexpr int GPS_TX_PIN = 21;  // ESP32 TX -> GPS RX
static constexpr uint32_t GPS_BAUD = 9600;  // u-blox MAX-M10S default
static constexpr uint32_t PRINT_INTERVAL_MS = 1000;
static constexpr bool DUMP_NMEA = true;  // GGA/RMC を生出力

TinyGPSPlus gps;
HardwareSerial GpsSerial(1);

void dumpInterestingNmea(char c) {
  static char line[120];
  static size_t len = 0;

  if (c == '\r') {
    return;
  }
  if (c == '\n') {
    line[len] = '\0';
    if (len >= 6 &&
        (strncmp(line, "$GNGGA", 6) == 0 || strncmp(line, "$GPGGA", 6) == 0 ||
         strncmp(line, "$GNRMC", 6) == 0 || strncmp(line, "$GPRMC", 6) == 0)) {
      Serial.print(F("NMEA "));
      Serial.println(line);
    }
    len = 0;
    return;
  }
  if (len + 1 < sizeof(line)) {
    line[len++] = c;
  } else {
    len = 0;
  }
}

void printFloat(float value, bool valid, int totalWidth, int decimals) {
  if (!valid) {
    while (totalWidth-- > 1) {
      Serial.print('*');
    }
    Serial.print(' ');
    return;
  }
  Serial.print(value, decimals);
  int written = decimals > 0 ? (decimals + 1) : 0;
  float absValue = fabsf(value);
  do {
    written++;
    absValue /= 10.0f;
  } while (absValue >= 1.0f);
  if (value < 0.0f) {
    written++;
  }
  while (written++ < totalWidth) {
    Serial.print(' ');
  }
}

void printPosition() {
  const bool hasFix = gps.location.isValid() && gps.satellites.isValid() &&
                      gps.satellites.value() > 0;

  if (!hasFix) {
    Serial.print(F("NO FIX  "));
  }

  Serial.print(F("Lat: "));
  printFloat(gps.location.lat(), gps.location.isValid(), 11, 6);
  Serial.print(F(" Lon: "));
  printFloat(gps.location.lng(), gps.location.isValid(), 12, 6);

  Serial.print(F(" Alt: "));
  printFloat(gps.altitude.meters(), gps.altitude.isValid(), 7, 1);
  Serial.print(F(" m"));

  Serial.print(F(" Sats: "));
  if (gps.satellites.isValid()) {
    Serial.print(gps.satellites.value());
  } else {
    Serial.print(F("--"));
  }

  Serial.print(F(" HDOP: "));
  printFloat(gps.hdop.hdop(), gps.hdop.isValid(), 5, 1);

  if (gps.date.isValid() && gps.time.isValid() && gps.date.month() >= 1 &&
      gps.date.day() >= 1) {
    char datetime[24];
    snprintf(datetime, sizeof(datetime), " %04u-%02u-%02u %02u:%02u:%02u",
             gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(),
             gps.time.minute(), gps.time.second());
    Serial.print(datetime);
  }

  Serial.printf("  chk ok=%lu fail=%lu",
                static_cast<unsigned long>(gps.passedChecksum()),
                static_cast<unsigned long>(gps.failedChecksum()));
  Serial.println();

  if (!hasFix) {
    Serial.println(
        F("  -> UART OK. Check antenna (SMA), sky view, PPS LED."));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("ESP32-C3 SuperMini + MAX-M10S (SSCI-10437)"));
  Serial.printf("GPS UART: RX=GPIO%d TX=GPIO%d baud=%lu\n", GPS_RX_PIN,
                GPS_TX_PIN, static_cast<unsigned long>(GPS_BAUD));
  Serial.println(F("Waiting for GNSS fix (needs active/passive antenna + sky)..."));

  GpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

void loop() {
  while (GpsSerial.available() > 0) {
    const char c = static_cast<char>(GpsSerial.read());
    gps.encode(c);
    if (DUMP_NMEA) {
      dumpInterestingNmea(c);
    }
  }

  static uint32_t lastPrintMs = 0;
  const uint32_t now = millis();
  if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = now;

    if (gps.charsProcessed() < 10) {
      Serial.println(F("No GPS data. Check wiring and baud rate."));
      return;
    }

    printPosition();
  }
}
