#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_AHTX0.h>
#include "mazel.h"
#include "mensch.h"
#include <stdint.h>

// ---------------- RFM69 (XIAO MG24) ----------------
#define PIN_RF69_INT    D0
#define PIN_RF69_CS     D1
#define PIN_RF69_RST    D2
#define PIN_RF69_DIO2   D3

// button on RX
#define BTN_PIN         D7  // active LOW

// RFM69 regs
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

// press logic
static const uint32_t PRESS_WINDOW_MS = 2000;
static const uint32_t DEBOUNCE_MS     = 50;
static const uint32_t STARTUP_IGNORE_MS = 500;

// AHT21
Adafruit_AHTX0 aht;
bool  aht_ok = false;
float last_temp_c = 0.0f;
float last_hum_pc = 0.0f;
static const uint32_t AHT_PERIOD_MS = 2000;
uint32_t aht_last_ms = 0;

// OLED – use HW I2C so it coexists with AHT21
U8G2_SH1107_SEEED_128X128_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
String last_action = "idle";

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
  if (us >= 1000UL) {
    delay(us / 1000UL);
    us %= 1000UL;
  }
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

  rfWrite(REG_DATAMODUL, 0x68);  // OOK continuous
  rfWrite(REG_PALEVEL, 0x5F);

  uint8_t dio = rfRead(REG_DIOMAPPING1);
  dio &= ~(0x0C);               // DIO2 = data
  rfWrite(REG_DIOMAPPING1, dio);

  rfWrite(REG_OPMODE, 0x0C);    // TX
  delay(5);
}

void sendMazelAlert() {
  replayToDIO2(Mazel, MazelCount);
  last_action = "mazel";
}

void sendMenschAlert() {
  replayToDIO2(Mensch, MenschCount);
  last_action = "mensch";
}

// ===================================================
// OLED
// ===================================================
void oledDraw() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 12, "AHT21");
    char buf[32];
    snprintf(buf, sizeof(buf), "T: %.1fC", last_temp_c);
    u8g2.drawStr(0, 28, buf);
    snprintf(buf, sizeof(buf), "H: %.0f%%", last_hum_pc);
    u8g2.drawStr(0, 42, buf);
    u8g2.drawStr(0, 62, "Last:");
    u8g2.drawStr(36, 62, last_action.c_str());
  } while (u8g2.nextPage());
}

// ===================================================
// setup
// ===================================================
void setup() {
  // NO Serial.begin(); RX (D7) stays free

  pinMode(BTN_PIN, INPUT_PULLUP);

  Wire.begin();
  u8g2.begin();

  if (!aht.begin()) {
    aht_ok = false;
  } else {
    aht_ok = true;
    sensors_event_t hum, temp;
    if (aht.getEvent(&hum, &temp)) {
      last_temp_c = temp.temperature;
      last_hum_pc = hum.relative_humidity;
    }
  }

  // RFM69
  pinMode(PIN_RF69_CS, OUTPUT);
  digitalWrite(PIN_RF69_CS, HIGH);
  pinMode(PIN_RF69_DIO2, OUTPUT);
  digitalWrite(PIN_RF69_DIO2, LOW);
  pinMode(PIN_RF69_INT, INPUT);

  SPI.begin();
  rfReset();
  rfConfig_OOK_DIO2();

  oledDraw();
}

// ===================================================
// loop
// ===================================================
void loop() {
  uint32_t now_ms = millis();

  // AHT21 periodic
  if (now_ms - aht_last_ms >= AHT_PERIOD_MS) {
    aht_last_ms = now_ms;
    if (aht_ok) {
      sensors_event_t hum, temp;
      if (aht.getEvent(&hum, &temp)) {
        last_temp_c = temp.temperature;
        last_hum_pc = hum.relative_humidity;
      }
    }
    oledDraw();
  }

  // button -> count presses in 2s window
  static bool     last_btn_level = HIGH;
  static uint32_t last_btn_ms    = 0;
  static uint32_t window_start   = 0;
  static uint8_t  press_count    = 0;
  static bool     window_active  = false;

  bool btn_level = digitalRead(BTN_PIN);  // LOW = pressed

  // ignore weird power-on states
  if (now_ms < STARTUP_IGNORE_MS) {
    last_btn_level = btn_level;
    delay(3);
    return;
  }

  if (btn_level != last_btn_level) {
    if (now_ms - last_btn_ms > DEBOUNCE_MS) {
      last_btn_ms = now_ms;
      last_btn_level = btn_level;
      if (btn_level == LOW) {
        if (!window_active) {
          window_active = true;
          window_start  = now_ms;
          press_count   = 1;
        } else {
          press_count++;
        }
      }
    }
  }

  if (window_active && (now_ms - window_start >= PRESS_WINDOW_MS)) {
    if (press_count == 1) {
      sendMazelAlert();
    } else if (press_count >= 2) {
      sendMenschAlert();
    }
    oledDraw();
    window_active = false;
  }

  delay(3);
}
