/*
  Project: ESP32-S3 BASE Unit Peripheral Interactive Tester
  Authors: David Steeman & ChatGPT
  Version: 1.0.6
  Date:    2026-02-08

  BASE Unit IO:
  - ADS1256 ADC (SPI):
      MOSI:35, SCLK:36, MISO:37, CS:39, RST:38, DRDY:40
  - SD Card (SPI):
      MOSI:11, CLK:12, MISO:13, CS:10 (FAT32, MBR)
  - DS1307 RTC (I2C): SDA:8, SCL:9  (CR2032 backup)
  - Igniter FET:
      CTRL: GPIO41
      POWER: GPIO4
  - WS2812: GPIO48 (single LED)
  - Buzzer: GPIO42 (ACTIVE LOW)
*/

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"
#include "esp_system.h"

// ---------------- Pins ----------------
static const int PIN_I2C_SDA = 8;
static const int PIN_I2C_SCL = 9;

// ADS1256 SPI
static const int PIN_ADC_MOSI = 35;
static const int PIN_ADC_SCLK = 36;
static const int PIN_ADC_MISO = 37;
static const int PIN_ADC_CS   = 39;
static const int PIN_ADC_RST  = 38;
static const int PIN_ADC_DRDY = 40;

// SD SPI
static const int PIN_SD_MOSI = 11;
static const int PIN_SD_SCLK = 12;
static const int PIN_SD_MISO = 13;
static const int PIN_SD_CS   = 10;

// Igniter
static const int PIN_IGN_CTRL = 41;
static const int PIN_IGN_PWR  = 4;

// WS2812 + Buzzer
static const int PIN_WS2812 = 48;
static const int PIN_BUZZER = 42;

// ---------------- Buzzer polarity (ACTIVE LOW) ----------------
static const uint8_t BUZZER_ON_LEVEL  = LOW;
static const uint8_t BUZZER_OFF_LEVEL = HIGH;

static const uint16_t BUZZ_TONE_MS  = 250;
static const uint16_t BUZZ_GAP_MS   = 180;

// ---------------- RTC ----------------
RTC_DS1307 rtc;

// ---------------- SPI bus instances ----------------
#if defined(SPI2_HOST)
  SPIClass spiADC(SPI2_HOST);
#elif defined(HSPI)
  SPIClass spiADC(HSPI);
#else
  SPIClass spiADC(1);
#endif
SPIClass &spiSD = SPI;

// ADS1256 settings (often MODE1). Start slow for bring-up.
static const SPISettings ADC_SPI_SETTINGS(1000000, MSBFIRST, SPI_MODE1);
static const float ADC_VREF_V = 2.500f; // adjust if your module uses a different Vref

// ---------------- NeoPixel via ESP32 core (robust) ----------------
// Provided by ESP32 Arduino core
extern "C" void neopixelWrite(uint8_t pin, uint8_t red, uint8_t green, uint8_t blue);

static void wsShow(uint8_t r, uint8_t g, uint8_t b) {
  // neopixelWrite expects RGB argument order.
  neopixelWrite((uint8_t)PIN_WS2812, r, g, b);
}
static void wsOff() { wsShow(0, 0, 0); }

static void wsPreflight() {
  // Unmissable boot pattern even if Serial isn't open yet
  pinMode(PIN_WS2812, OUTPUT);
  wsShow(0, 0, 255); delay(300); // Blue
  wsOff();           delay(150);
  wsShow(255, 0, 0); delay(300); // Red
  wsOff();           delay(150);
  wsShow(0, 255, 0); delay(300); // Green
  wsOff();           delay(150);
}

// ---------------- Serial helpers ----------------
static void flushSerialInput() {
  while (Serial.available()) Serial.read();
}

static String readLineBlocking() {
  String s;
  while (true) {
    while (Serial.available()) {
      char c = (char)Serial.read();
      if (c == '\r') continue;
      if (c == '\n') return s;
      s += c;
    }
    delay(5);
  }
}

static void waitEnter(const char* prompt) {
  Serial.print(prompt);
  Serial.println(" (press ENTER)");
  (void)readLineBlocking();
}

static bool askYesNo(const char* prompt) {
  while (true) {
    Serial.print(prompt);
    Serial.print(" [y/n]: ");
    String s = readLineBlocking();
    s.trim();
    s.toLowerCase();
    if (s == "y" || s == "yes") return true;
    if (s == "n" || s == "no") return false;
    Serial.println("Please answer 'y' or 'n'.");
  }
}

// ---------------- Reset reason ----------------
static const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "UNKNOWN";
  }
}

// ---------------- Buzzer ----------------
static void buzzerOffHard() {
  noTone(PIN_BUZZER);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, BUZZER_OFF_LEVEL);
}

static void beepTone(uint32_t freq, uint16_t ms) {
  buzzerOffHard();
  delay(10);
  tone(PIN_BUZZER, (unsigned int)freq);
  delay(ms);
  buzzerOffHard();
}

// ---------------- ADS1256 minimal driver ----------------
static const uint8_t CMD_WAKEUP = 0x00;
static const uint8_t CMD_RDATA  = 0x01;
static const uint8_t CMD_SDATAC = 0x0F;
static const uint8_t CMD_RREG   = 0x10;
static const uint8_t CMD_WREG   = 0x50;
static const uint8_t CMD_SYNC   = 0xFC;
static const uint8_t CMD_RESET  = 0xFE;

static const uint8_t REG_STATUS = 0x00;
static const uint8_t REG_MUX    = 0x01;
static const uint8_t REG_ADCON  = 0x02;
static const uint8_t REG_DRATE  = 0x03;

static void adcCsLow()  { digitalWrite(PIN_ADC_CS, LOW); }
static void adcCsHigh() { digitalWrite(PIN_ADC_CS, HIGH); }

static void adcResetPulse() {
  pinMode(PIN_ADC_RST, OUTPUT);
  digitalWrite(PIN_ADC_RST, HIGH);
  delay(2);
  digitalWrite(PIN_ADC_RST, LOW);
  delay(10);
  digitalWrite(PIN_ADC_RST, HIGH);
  delay(10);
}

static void adcSendCommand(uint8_t cmd) {
  spiADC.beginTransaction(ADC_SPI_SETTINGS);
  adcCsLow();
  spiADC.transfer(cmd);
  adcCsHigh();
  spiADC.endTransaction();
  delayMicroseconds(10);
}

static uint8_t adcReadReg(uint8_t reg) {
  spiADC.beginTransaction(ADC_SPI_SETTINGS);
  adcCsLow();
  spiADC.transfer((uint8_t)(CMD_RREG | (reg & 0x0F)));
  spiADC.transfer(0x00);  // read 1 register
  delayMicroseconds(10);
  uint8_t val = spiADC.transfer(0xFF);
  adcCsHigh();
  spiADC.endTransaction();
  delayMicroseconds(10);
  return val;
}

static void adcWriteReg(uint8_t reg, uint8_t val) {
  spiADC.beginTransaction(ADC_SPI_SETTINGS);
  adcCsLow();
  spiADC.transfer((uint8_t)(CMD_WREG | (reg & 0x0F)));
  spiADC.transfer(0x00);  // write 1 register
  spiADC.transfer(val);
  adcCsHigh();
  spiADC.endTransaction();
  delayMicroseconds(10);
}

static bool adcWaitDrdyLow(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (digitalRead(PIN_ADC_DRDY) == LOW) return true;
    delay(1);
  }
  return false;
}

static bool adcReadRaw24(int32_t &outRaw) {
  if (!adcWaitDrdyLow(1000)) return false;

  spiADC.beginTransaction(ADC_SPI_SETTINGS);
  adcCsLow();
  spiADC.transfer(CMD_RDATA);
  delayMicroseconds(10);
  uint8_t b0 = spiADC.transfer(0xFF);
  uint8_t b1 = spiADC.transfer(0xFF);
  uint8_t b2 = spiADC.transfer(0xFF);
  adcCsHigh();
  spiADC.endTransaction();

  int32_t v = ((int32_t)b0 << 16) | ((int32_t)b1 << 8) | (int32_t)b2;
  if (v & 0x800000) v |= 0xFF000000; // sign extend
  outRaw = v;
  return true;
}

static float adcRawToVolts(int32_t raw, uint8_t gain) {
  const float fullscale = 8388607.0f; // 2^23 - 1
  return ((float)raw / fullscale) * (ADC_VREF_V / (float)gain);
}

// ---------------- Tests ----------------
static bool testWS2812_step() {
  Serial.println("\n[WS2812] STEP test (core neopixelWrite) - press ENTER to advance.");

  struct Step { uint8_t r,g,b; const char* name; } steps[] = {
    {255,0,0,"RED"},
    {0,255,0,"GREEN"},
    {0,0,255,"BLUE"},
    {255,255,255,"WHITE"},
    {0,0,0,"OFF"}
  };

  for (auto &s : steps) {
    Serial.print("  Showing: "); Serial.println(s.name);
    wsShow(s.r, s.g, s.b);
    waitEnter("  Observe the LED");
  }

  wsOff();
  return askYesNo("  Did the NeoPixel show each color correctly?");
}

static bool testBuzzer() {
  Serial.println("\n[BUZZER] 3 beeps test...");
  beepTone(800, BUZZ_TONE_MS);  delay(BUZZ_GAP_MS);
  beepTone(1200, BUZZ_TONE_MS); delay(BUZZ_GAP_MS);
  beepTone(1600, BUZZ_TONE_MS); delay(BUZZ_GAP_MS);
  return askYesNo("  Did you hear the 3 beeps and then silence?");
}

static bool testRTC() {
  Serial.println("\n[RTC] DS1307 test...");
  Wire.end();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  if (!rtc.begin(&Wire)) {
    Serial.println("  rtc.begin failed (no I2C response).");
    return false;
  }

  DateTime now = rtc.now();
  Serial.printf("  RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());
  return true;
}

static bool testSD() {
  Serial.println("\n[SD] test...");
  waitEnter("  Insert SD and press ENTER");

  spiSD.begin(PIN_SD_SCLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

  if (!SD.begin(PIN_SD_CS, spiSD)) {
    Serial.println("  SD.begin failed. Check wiring, CS, power, formatting.");
    return false;
  }

  const char* path = "/base_hw_test.txt";
  String payload = "BASE HW TEST OK - " + String(millis()) + "\n";

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("  open for write failed.");
    return false;
  }
  f.print(payload);
  f.close();

  Serial.println("  Wrote /base_hw_test.txt");
  return true;
}

static bool testADC_withCh0Samples() {
  Serial.println("\n[ADC] ADS1256: configure CH0 (AIN0-AINCOM) and dump 10 samples.");
  Serial.println("      Pins: MOSI=35 MISO=37 SCLK=36 CS=39 RST=38 DRDY=40");

  pinMode(PIN_ADC_CS, OUTPUT);
  adcCsHigh();
  pinMode(PIN_ADC_DRDY, INPUT_PULLUP);

  Serial.println("  SPI begin...");
  spiADC.begin(PIN_ADC_SCLK, PIN_ADC_MISO, PIN_ADC_MOSI, PIN_ADC_CS);
  delay(10);

  Serial.println("  Reset...");
  adcResetPulse();
  adcSendCommand(CMD_RESET);
  delay(5);
  adcSendCommand(CMD_SDATAC);
  delay(5);

  uint8_t status = adcReadReg(REG_STATUS);
  uint8_t mux    = adcReadReg(REG_MUX);
  uint8_t adcon  = adcReadReg(REG_ADCON);
  uint8_t drate  = adcReadReg(REG_DRATE);

  Serial.printf("  STATUS=0x%02X MUX=0x%02X ADCON=0x%02X DRATE=0x%02X\n",
                status, mux, adcon, drate);

  Serial.println("  Set MUX = AIN0-AINCOM (0x08)...");
  adcWriteReg(REG_MUX, 0x08);
  uint8_t mux2 = adcReadReg(REG_MUX);
  Serial.printf("  MUX readback: 0x%02X\n", mux2);

  uint8_t gainCode = (adcon & 0x07);
  uint8_t gain = 1;
  switch (gainCode) {
    case 0: gain = 1; break;
    case 1: gain = 2; break;
    case 2: gain = 4; break;
    case 3: gain = 8; break;
    case 4: gain = 16; break;
    case 5: gain = 32; break;
    case 6: gain = 64; break;
    default: gain = 1; break;
  }

  Serial.printf("  Gain=%u (code=%u), Vref=%.3f V\n", gain, gainCode, ADC_VREF_V);

  bool drdy = adcWaitDrdyLow(800);
  Serial.printf("  DRDY low within 800ms: %s\n", drdy ? "YES" : "NO");
  if (!drdy) {
    Serial.println("  If NO: check DRDY wiring or ADC not converting.");
  }

  Serial.println("  Reading 10 samples...");
  for (int i = 0; i < 10; i++) {
    adcSendCommand(CMD_SYNC);
    delayMicroseconds(10);
    adcSendCommand(CMD_WAKEUP);
    delayMicroseconds(10);

    int32_t raw;
    if (!adcReadRaw24(raw)) {
      Serial.printf("    %02d: READ FAIL (DRDY timeout)\n", i);
      continue;
    }
    float v = adcRawToVolts(raw, gain);
    Serial.printf("    %02d: RAW=%ld  V~%.6f\n", i, (long)raw, v);
  }

  return askYesNo("  Did samples look reasonable (not all identical nonsense)?");
}

// ---------------- Arduino entry points ----------------
void setup() {
  Serial.begin(115200);
  delay(300);
  flushSerialInput();

  // NeoPixel preflight (no Serial required)
  wsPreflight();

  Serial.println("\nESP32-S3 BASE Unit HW Tester");
  Serial.printf("Reset reason: %s\n", resetReasonStr(esp_reset_reason()));
  Serial.println("Set monitor: 115200 baud + Newline\n");

  // Buzzer off by default
  buzzerOffHard();

  waitEnter("Press ENTER to start");

  bool wsOk   = testWS2812_step();
  bool buzzOk = testBuzzer();
  bool rtcOk  = testRTC();
  bool sdOk   = testSD();
  bool adcOk  = testADC_withCh0Samples();

  Serial.println("\nSUMMARY:");
  Serial.printf("  WS2812: %s\n", wsOk ? "PASS" : "FAIL");
  Serial.printf("  Buzzer: %s\n", buzzOk ? "PASS" : "FAIL");
  Serial.printf("  RTC   : %s\n", rtcOk ? "PASS" : "FAIL");
  Serial.printf("  SD    : %s\n", sdOk ? "PASS" : "FAIL");
  Serial.printf("  ADC   : %s\n", adcOk ? "PASS" : "FAIL");
  Serial.println("Done.");
}

void loop() {
  delay(1000);
}
