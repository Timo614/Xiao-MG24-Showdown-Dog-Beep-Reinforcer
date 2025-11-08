#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_AHTX0.h>
#include <Matter.h>
#include <MatterTemperature.h>
#include <MatterOnOffPluginUnit.h>
#include "mazel.h"
#include "mensch.h"
#include <stdint.h>

// ---------------- RFM69 pins (XIAO MG24) ----------------
#define PIN_RF69_INT    D0
#define PIN_RF69_CS     D1
#define PIN_RF69_RST    D2
#define PIN_RF69_DIO2   D3

// ---------------- RFM69 regs ----------------
#define REG_OPMODE        0x01
#define REG_DATAMODUL     0x02
#define REG_FRFMSB        0x07
#define REG_FRFMID        0x08
#define REG_FRFLSB        0x09
#define REG_PALEVEL       0x11
#define REG_DIOMAPPING1   0x25
#define REG_VERSION       0x10

// ~433.92 MHz
#define FRF_MSB  0x6C
#define FRF_MID  0x7A
#define FRF_LSB  0xE1

static SPISettings rfspi(4000000, MSBFIRST, SPI_MODE0);

// ---------------- Matter endpoints ----------------
MatterTemperature     matter_temp_sensor;  // read-only temp
MatterOnOffPluginUnit matter_mazel;        // controllable "switch" -> Mazel
MatterOnOffPluginUnit matter_mensch;       // controllable "switch" -> Mensch

// ---------------- AHT21 ----------------
Adafruit_AHTX0 aht;
bool aht_ok = false;
static const uint32_t AHT_PERIOD_MS = 2000;
uint32_t aht_last_ms = 0;

// ===================================================
// RFM69
// ===================================================
void rfWrite(uint8_t reg, uint8_t val) {
  SPI.beginTransaction(rfspi);
  digitalWrite(PIN_RF69_CS, LOW);
  SPI.transfer(reg | 0x80);
  SPI.transfer(val);
  digitalWrite(PIN_RF69_CS, HIGH);
  SPI.endTransaction();
}

uint8_t rfRead(uint8_t reg) {
  SPI.beginTransaction(rfspi);
  digitalWrite(PIN_RF69_CS, LOW);
  SPI.transfer(reg & 0x7F);
  uint8_t v = SPI.transfer(0x00);
  digitalWrite(PIN_RF69_CS, HIGH);
  SPI.endTransaction();
  return v;
}

void rfReset() {
  pinMode(PIN_RF69_RST, OUTPUT);
  digitalWrite(PIN_RF69_RST, HIGH);
  delay(5);
  digitalWrite(PIN_RF69_RST, LOW);
  delay(5);
  pinMode(PIN_RF69_RST, INPUT_PULLUP);
  delay(5);
}

static inline void delayAbsMicros(uint32_t us) {
  if (us >= 1000UL) { delay(us / 1000UL); us %= 1000UL; }
  if (us) delayMicroseconds((unsigned int)us);
}

static inline int32_t read_i32_p(const int32_t* base, size_t idx) {
#if defined(__AVR__)
  return (int32_t)pgm_read_dword(&base[idx]);
#else
  return base[idx];
#endif
}

void replayToDIO2(const int32_t* timings, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    int32_t t = read_i32_p(timings, i);
    if (t > 0) {
      digitalWrite(PIN_RF69_DIO2, HIGH);
      delayAbsMicros((uint32_t)t);
    } else {
      digitalWrite(PIN_RF69_DIO2, LOW);
      uint32_t us = (t == INT32_MIN) ? 2147483648UL : (uint32_t)(-t);
      delayAbsMicros(us);
    }
  }
  digitalWrite(PIN_RF69_DIO2, LOW);
}

void rfConfig_OOK_DIO2() {
  rfWrite(REG_OPMODE, 0x04);  // standby
  delay(5);
  rfWrite(REG_FRFMSB, FRF_MSB);
  rfWrite(REG_FRFMID, FRF_MID);
  rfWrite(REG_FRFLSB, FRF_LSB);
  rfWrite(REG_DATAMODUL, 0x68); // OOK, continuous
  rfWrite(REG_PALEVEL, 0x5F);
  uint8_t dio = rfRead(REG_DIOMAPPING1);
  dio &= ~(0x0C);              // DIO2=data
  rfWrite(REG_DIOMAPPING1, dio);
  rfWrite(REG_OPMODE, 0x0C);   // TX
  delay(5);
}

inline void sendMazelAlert()  { replayToDIO2(Mazel,  MazelCount); }
inline void sendMenschAlert() { replayToDIO2(Mensch, MenschCount); }

// ===================================================
// setup
// ===================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("Matter: AHT21 + 2x On/Off plugin units + RFM69"));

  // AHT21
  if (!aht.begin()) {
    Serial.println(F("AHT21 not found."));
    aht_ok = false;
  } else {
    aht_ok = true;
  }

  // Matter
  Matter.begin();
  matter_temp_sensor.begin();
  matter_mazel.begin();
  matter_mensch.begin();

  if (!Matter.isDeviceCommissioned()) {
    Serial.println(F("Commission this device:"));
    Serial.printf("Manual code: %s\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR URL    : %s\n", Matter.getOnboardingQRCodeUrl().c_str());
  }
  while (!Matter.isDeviceCommissioned()) { delay(200); }

  Serial.println(F("Waiting for Thread..."));
  while (!Matter.isDeviceThreadConnected()) { delay(200); }
  Serial.println(F("Thread connected."));

  Serial.println(F("Waiting endpoints..."));
  while (!matter_temp_sensor.is_online()
         || !matter_mazel.is_online()
         || !matter_mensch.is_online()) {
    delay(200);
  }
  Serial.println(F("Endpoints online."));

  // RFM69
  pinMode(PIN_RF69_CS, OUTPUT);
  digitalWrite(PIN_RF69_CS, HIGH);
  pinMode(PIN_RF69_DIO2, OUTPUT);
  digitalWrite(PIN_RF69_DIO2, LOW);
  pinMode(PIN_RF69_INT, INPUT);
  SPI.begin();
  rfReset();
  rfConfig_OOK_DIO2();

  // prime temp reading
  if (aht_ok) {
    sensors_event_t hum, temp;
    if (aht.getEvent(&hum, &temp)) {
      matter_temp_sensor.set_measured_value_celsius(temp.temperature);
    }
  }

  // show as OFF in HA initially
  matter_mazel.set_onoff(false);
  matter_mensch.set_onoff(false);

  Serial.println(F("Setup done."));
}

// ===================================================
// loop
// ===================================================
void loop() {
  const uint32_t now_ms = millis();

  // AHT21 → Matter temp
  if (now_ms - aht_last_ms >= AHT_PERIOD_MS) {
    aht_last_ms = now_ms;
    if (aht_ok) {
      sensors_event_t hum, temp;
      if (aht.getEvent(&hum, &temp)) {
        matter_temp_sensor.set_measured_value_celsius(temp.temperature);
        Serial.printf("AHT21: %.2f C, %.1f %%RH\n", temp.temperature, hum.relative_humidity);
      }
    }
  }

  // Edge-detect On/Off to act as "momentary"
  static bool last_mzl = false, last_mns = false;
  bool mzl = matter_mazel.get_onoff();
  bool mns = matter_mensch.get_onoff();

  if (mzl && !last_mzl) {           // Mazel turned ON from HA
    sendMazelAlert();
    matter_mazel.set_onoff(false);  // spring back
  }
  if (mns && !last_mns) {           // Mensch turned ON from HA
    sendMenschAlert();
    matter_mensch.set_onoff(false); // spring back
  }

  last_mzl = mzl;
  last_mns = mns;

  delay(5);
}
