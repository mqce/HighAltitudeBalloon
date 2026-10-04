#include <Arduino.h>
#include <SparkFun_u-blox_GNSS_v3.h>
#include <Wire.h>

// Switch Science MAX-M10S (SSCI-10437) I2C / Qwiic
static constexpr int GPS_SDA_PIN = 6;
static constexpr int GPS_SCL_PIN = 7;
static constexpr uint8_t GPS_I2C_ADDR = 0x42;
static constexpr uint32_t GPS_I2C_HZ = 100000;
static constexpr uint32_t PRINT_INTERVAL_MS = 1000;
static constexpr uint32_t RETRY_INTERVAL_MS = 3000;
static constexpr uint32_t STALE_MS = 2500;

SFE_UBLOX_GNSS gnss;
static bool gnssReady = false;

struct GnssFix {
  bool have;
  bool gnssFixOk;
  bool invalidLlh;
  bool timeValid;
  uint8_t fixType;
  uint8_t siv;
  int32_t lat;
  int32_t lon;
  int32_t altMslMm;
  int32_t hAccMm;
  int32_t vAccMm;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint32_t updatedMs;
};

static GnssFix fix = {};

bool configureGnss() {
  const bool ubxOnly = gnss.setI2COutput(COM_TYPE_UBX, VAL_LAYER_RAM);
  const bool model = gnss.setDynamicModel(DYN_MODEL_AIRBORNE1g, VAL_LAYER_RAM);
  const bool rate = gnss.setNavigationFrequency(1, VAL_LAYER_RAM);
  const bool autoPvt = gnss.setAutoPVT(true, VAL_LAYER_RAM);

  if (!ubxOnly) {
    Serial.println(F("Warning: I2C UBX-only setting failed"));
  }
  if (!model) {
    Serial.println(F("Warning: airborne <1g setting failed"));
  }
  if (!rate) {
    Serial.println(F("Warning: 1 Hz navigation rate failed"));
  }
  if (!autoPvt) {
    Serial.println(F("Warning: automatic PVT failed"));
  }
  return autoPvt;
}

bool startGnss() {
  if (!gnss.begin(Wire, GPS_I2C_ADDR)) {
    return false;
  }
  if (!configureGnss()) {
    return false;
  }
  Serial.println(F("MAX-M10S ready (UBX, airborne <1g, RAM only)"));
  return true;
}

void capturePvt() {
  fix.have = true;
  fix.updatedMs = millis();
  fix.lat = gnss.getLatitude();
  fix.lon = gnss.getLongitude();
  fix.altMslMm = gnss.getAltitudeMSL();
  fix.hAccMm = gnss.getHorizontalAccEst();
  fix.vAccMm = gnss.getVerticalAccEst();
  fix.siv = gnss.getSIV();
  fix.fixType = gnss.getFixType();
  fix.gnssFixOk = gnss.getGnssFixOk();
  fix.invalidLlh = gnss.getInvalidLlh();
  fix.timeValid = gnss.getConfirmedDate() && gnss.getConfirmedTime();
  fix.year = gnss.getYear();
  fix.month = gnss.getMonth();
  fix.day = gnss.getDay();
  fix.hour = gnss.getHour();
  fix.minute = gnss.getMinute();
  fix.second = gnss.getSecond();
}

void printMeters(int32_t millimeters) {
  Serial.printf("%7.1f m", millimeters / 1000.0);
}

void printFix() {
  if (!fix.have) {
    Serial.println(F("No GNSS solution yet."));
    return;
  }

  const uint32_t age = millis() - fix.updatedMs;
  const bool fresh = age < STALE_MS;
  const bool hasFix = fresh && fix.gnssFixOk && fix.fixType == 3 && !fix.invalidLlh;

  if (!fresh) {
    Serial.print(F("STALE   "));
  } else if (!hasFix) {
    Serial.print(F("NO FIX  "));
  } else {
    Serial.print(F("FIX     "));
  }

  if (hasFix) {
    Serial.printf("Lat: %11.6f Lon: %12.6f Alt: ", fix.lat / 1e7, fix.lon / 1e7);
    printMeters(fix.altMslMm);
  } else {
    Serial.print(F("Lat: ********** Lon: *********** Alt: ******* "));
  }

  Serial.printf("  Sats: %2u  hAcc: ", fix.siv);
  printMeters(fix.hAccMm);
  Serial.print(F("  vAcc: "));
  printMeters(fix.vAccMm);
  Serial.printf("  type:%u ok:%u age:%lu ms", fix.fixType, fix.gnssFixOk ? 1 : 0,
                static_cast<unsigned long>(age));

  if (fix.timeValid) {
    Serial.printf("  %04u-%02u-%02u %02u:%02u:%02u", fix.year, fix.month, fix.day,
                  fix.hour, fix.minute, fix.second);
  }
  Serial.println();

  if (!fresh) {
    Serial.println(F("  -> PVT stopped. Check SDA/SCL and 3.3V."));
  } else if (!hasFix) {
    Serial.println(F("  -> I2C OK. Check antenna (SMA), sky view, PPS LED."));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("ESP32-C3 SuperMini + MAX-M10S (SSCI-10437)"));
  Serial.printf("GPS I2C: SDA=GPIO%d SCL=GPIO%d addr=0x%02X %lu Hz\n",
                GPS_SDA_PIN, GPS_SCL_PIN, GPS_I2C_ADDR,
                static_cast<unsigned long>(GPS_I2C_HZ));
  Serial.println(F("Waiting for GNSS fix (needs active/passive antenna + sky)..."));

  Wire.begin(GPS_SDA_PIN, GPS_SCL_PIN);
  Wire.setClock(GPS_I2C_HZ);
  Wire.setTimeOut(50);

  gnssReady = startGnss();
  if (!gnssReady) {
    Serial.println(F("MAX-M10S not found. Check SDA/SCL, 3.3V, GND."));
  }
}

void loop() {
  const uint32_t now = millis();

  if (!gnssReady) {
    static uint32_t lastRetryMs = millis();
    if (now - lastRetryMs >= RETRY_INTERVAL_MS) {
      lastRetryMs = now;
      gnssReady = startGnss();
      if (!gnssReady) {
        Serial.println(F("MAX-M10S not found. Check SDA/SCL, 3.3V, GND."));
      }
    }
    return;
  }

  if (gnss.getPVT()) {
    capturePvt();
  }

  static uint32_t lastPrintMs = 0;
  if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = now;
    printFix();
  }
}
