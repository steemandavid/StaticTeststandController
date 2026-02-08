/*
  Project: ESP32-S3 Peripheral Interactive Tester
  Authors: David Steeman & ChatGPT
  Version: 1.3.1
  Date:    2026-02-08

  Hardware / pins:
  - SSD1306 OLED (I2C): SDA=8, SCL=9, 128x64
  - Ignition Button: GPIO16 (input), GPIO17 (LED)
  - Arm/Safe Switch: GPIO4 (armed), GPIO5 (safe) - toggle, active low
  - Battery Monitor: GPIO6 (ADC) voltage divider
  - WS2812 RGB LED: GPIO48
  - Buzzer: GPIO42

  Buzzer wiring used here (sink mode):
  - Buzzer + -> 3.3V
  - Buzzer - -> GPIO42
  => GPIO LOW sinks current => buzzer ON
  => GPIO HIGH => buzzer OFF

  Libraries needed (Arduino Library Manager):
  - Adafruit SSD1306
  - Adafruit GFX Library
  - Adafruit NeoPixel
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

// ---------------- Pins ----------------
static const int PIN_OLED_SDA   = 8;
static const int PIN_OLED_SCL   = 9;

static const int PIN_IGN_BTN    = 16;
static const int PIN_IGN_LED    = 17;    // ✅ as requested

static const int PIN_ARMED      = 4;     // active LOW
static const int PIN_SAFE       = 5;     // active LOW

static const int PIN_BATT_ADC   = 6;     // ✅ as requested

static const int PIN_WS2812     = 48;    // ✅ as requested

static const int PIN_BUZZER     = 42;

// ---------------- Timing tweaks ----------------
static const uint16_t BTN_LED_ON_MS  = 600;   // ✅ slower blink
static const uint16_t BTN_LED_OFF_MS = 600;

static const uint16_t WS_STEP_MS     = 1200;  // ✅ slower WS2812 steps
static const uint8_t  WS_BRIGHTNESS  = 40;

// ---------------- Buzzer polarity for sink wiring ----------------
static const uint8_t BUZZER_ON_LEVEL  = LOW;   // ✅ ON when sinking
static const uint8_t BUZZER_OFF_LEVEL = HIGH;  // ✅ OFF when not sinking

// ---------------- OLED ----------------
static const int OLED_W = 128;
static const int OLED_H = 64;
Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);

// ---------------- WS2812 ----------------
static const int WS_COUNT = 1;
Adafruit_NeoPixel pixels(WS_COUNT, PIN_WS2812, NEO_GRB + NEO_KHZ800);

// ---------------- Battery calculation ----------------
// Adjust these to your actual divider.
// Vbatt -> R_TOP -> ADC node -> R_BOTTOM -> GND
static const float R_TOP_OHMS    = 100000.0f; // e.g. 100k
static const float R_BOTTOM_OHMS = 100000.0f; // e.g. 100k
static const float ADC_REF_V     = 3.3f;      // approximate
static const int   ADC_BITS      = 12;        // 0..4095

// ---------------- Test tracking ----------------
struct TestResult {
  bool i2cScanOk = false;
  bool oledOk    = false;
  bool ignBtnOk  = false;
  bool ignLedOk  = false;
  bool armSafeOk = false;
  bool battOk    = false;
  bool wsOk      = false;
  bool buzzerOk  = false;

  uint8_t oledAddr = 0;
  int ignBtnPressedCount = 0;
  float battVadc = 0.0f;
  float battVest = 0.0f;
};

TestResult result;

// ---------------- Helpers: Serial UI ----------------
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

static void waitEnter(const char* prompt) {
  Serial.print(prompt);
  Serial.println(" (press ENTER)");
  (void)readLineBlocking();
}

// ---------------- I2C scan ----------------
static uint8_t i2cScanForOledAddr() {
  Serial.println("\n[I2C] Scanning bus...");
  uint8_t foundAddr = 0;
  int foundCount = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("  Found device at 0x%02X\n", addr);
      foundCount++;
      if ((addr == 0x3C || addr == 0x3D) && foundAddr == 0) foundAddr = addr;
      if (foundAddr == 0) foundAddr = addr;
    }
  }

  if (foundCount == 0) {
    Serial.println("  No I2C devices found.");
    return 0;
  }

  Serial.printf("  I2C scan complete. Using OLED address 0x%02X\n", foundAddr);
  return foundAddr;
}

// ---------------- OLED test ----------------
static bool testOLED(uint8_t addr) {
  Serial.println("\n[OLED] Initializing SSD1306...");

  Wire.end();
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);

  if (addr == 0) return false;

  if (!display.begin(SSD1306_SWITCHCAPVCC, addr)) {
    Serial.println("  display.begin() failed.");
    return false;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP32-S3 Test");
  display.println("OLED OK if you");
  display.println("see this text.");
  display.println("");
  display.println("I2C SDA=8 SCL=9");
  display.display();

  return askYesNo("  Do you see readable text on the OLED?");
}

// ---------------- Ignition button + LED test ----------------
static bool testIgnitionButtonAndLed() {
  Serial.println("\n[IGNITION] Testing button GPIO16 and LED GPIO17...");

  pinMode(PIN_IGN_BTN, INPUT_PULLUP);
  pinMode(PIN_IGN_LED, OUTPUT);
  digitalWrite(PIN_IGN_LED, LOW);

  Serial.println("  Blinking button LED (GPIO17) 6 times (slower)...");
  for (int i = 0; i < 6; i++) {
    digitalWrite(PIN_IGN_LED, HIGH);
    delay(BTN_LED_ON_MS);
    digitalWrite(PIN_IGN_LED, LOW);
    delay(BTN_LED_OFF_MS);
  }
  bool ledOk = askYesNo("  Did the button LED blink?");

  Serial.println("  Now testing button input (GPIO16).");
  Serial.println("  Press and release the ignition button 3 times within 10 seconds.");

  int presses = 0;
  bool prev = digitalRead(PIN_IGN_BTN);
  unsigned long start = millis();

  while (millis() - start < 10000 && presses < 3) {
    bool cur = digitalRead(PIN_IGN_BTN);
    if (prev == HIGH && cur == LOW) {
      presses++;
      Serial.printf("    Detected press %d/3\n", presses);

      // small confirmation blink (still visible)
      digitalWrite(PIN_IGN_LED, HIGH);
      delay(180);
      digitalWrite(PIN_IGN_LED, LOW);
    }
    prev = cur;
    delay(5);
  }

  result.ignBtnPressedCount = presses;
  bool btnOk = (presses >= 3);
  if (!btnOk) {
    Serial.printf("  Only detected %d presses.\n", presses);
    btnOk = askYesNo("  If you DID press 3 times, answer 'y' to accept anyway. Accept?");
  }

  result.ignLedOk = ledOk;
  result.ignBtnOk = btnOk;
  return ledOk && btnOk;
}

// ---------------- Arm/Safe switch test ----------------
static bool testArmSafe() {
  Serial.println("\n[ARM/SAFE] Testing toggle switch inputs:");
  Serial.println("  GPIO4 = ARMED (active LOW)");
  Serial.println("  GPIO5 = SAFE  (active LOW)");

  pinMode(PIN_ARMED, INPUT_PULLUP);
  pinMode(PIN_SAFE, INPUT_PULLUP);

  Serial.println("  Flip switch to SAFE position, then press ENTER.");
  waitEnter("  Ready?");
  int safeRead = digitalRead(PIN_SAFE);
  int armedRead = digitalRead(PIN_ARMED);
  Serial.printf("  Reads: SAFE=%d, ARMED=%d (LOW=active)\n", safeRead, armedRead);

  bool safeOk = (safeRead == LOW && armedRead == HIGH);
  if (!safeOk) safeOk = askYesNo("  Expected SAFE=LOW and ARMED=HIGH. Still accept SAFE as OK?");

  Serial.println("  Now flip switch to ARMED position, then press ENTER.");
  waitEnter("  Ready?");
  safeRead = digitalRead(PIN_SAFE);
  armedRead = digitalRead(PIN_ARMED);
  Serial.printf("  Reads: SAFE=%d, ARMED=%d (LOW=active)\n", safeRead, armedRead);

  bool armedOk = (armedRead == LOW && safeRead == HIGH);
  if (!armedOk) armedOk = askYesNo("  Expected ARMED=LOW and SAFE=HIGH. Still accept ARMED as OK?");

  return safeOk && armedOk;
}

// ---------------- Battery ADC test ----------------
static float adcToVoltage(float adcCounts) {
  float maxCounts = (float)((1 << ADC_BITS) - 1);
  return (adcCounts / maxCounts) * ADC_REF_V;
}

static float adcToBatteryVoltage(float vadc) {
  float scale = (R_TOP_OHMS + R_BOTTOM_OHMS) / R_BOTTOM_OHMS;
  return vadc * scale;
}

static bool testBattery() {
  Serial.println("\n[BATTERY] Testing ADC on GPIO6...");
  Serial.println("  Make sure the battery is connected to the divider, and ADC node is on GPIO6.");

  const int samples = 60;
  uint32_t sum = 0;

  analogReadResolution(ADC_BITS);

  for (int i = 0; i < samples; i++) {
    sum += analogRead(PIN_BATT_ADC);
    delay(5);
  }

  float avg = (float)sum / samples;
  float vadc = adcToVoltage(avg);
  float vbatt = adcToBatteryVoltage(vadc);

  result.battVadc = vadc;
  result.battVest = vbatt;

  Serial.printf("  ADC avg counts: %.1f / %d\n", avg, (1 << ADC_BITS) - 1);
  Serial.printf("  Estimated ADC pin voltage: %.3f V\n", vadc);
  Serial.printf("  Estimated battery voltage (using divider): %.2f V\n", vbatt);
  Serial.println("  (If the estimate is off, adjust R_TOP_OHMS / R_BOTTOM_OHMS in the sketch.)");

  return askYesNo("  Does the battery reading look plausible (non-zero and roughly what you expect)?");
}

// ---------------- WS2812 test ----------------
static bool testWS2812() {
  Serial.println("\n[WS2812] Testing RGB LED on GPIO48...");
  pixels.begin();
  pixels.setBrightness(WS_BRIGHTNESS);

  Serial.println("  Cycling colors slowly: Red -> Green -> Blue -> White -> Off (repeat 2x).");

  auto showColor = [&](uint8_t r, uint8_t g, uint8_t b) {
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
    delay(WS_STEP_MS);
  };

  for (int k = 0; k < 2; k++) {
    showColor(255, 0, 0);
    showColor(0, 255, 0);
    showColor(0, 0, 255);
    showColor(255, 255, 255);
    showColor(0, 0, 0);
  }

  pixels.clear();
  pixels.show();

  return askYesNo("  Did the WS2812 show the expected (slow) color sequence?");
}

// ---------------- Buzzer test ----------------
// We intentionally use tone()/noTone() for maximum compatibility.
// Then we *force* the pin to BUZZER_OFF_LEVEL to prevent “stuck on”.
static void buzzerOffHard() {
  noTone(PIN_BUZZER);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, BUZZER_OFF_LEVEL);
}

static void buzzerToneHz(uint32_t freq, uint32_t ms) {
  // Ensure OFF state first
  buzzerOffHard();
  delay(10);

  // Play tone
  tone(PIN_BUZZER, (unsigned int)freq);
  delay(ms);

  // Stop and force OFF (HIGH for sink wiring)
  buzzerOffHard();
}

static bool testBuzzer() {
  Serial.println("\n[BUZZER] Testing buzzer on GPIO42...");
  Serial.println("  Wiring assumed: Buzzer+ to 3.3V, Buzzer- to GPIO42 (sink mode).");
  Serial.println("  Expect: 3 beeps, then SILENCE.");

  buzzerOffHard();
  delay(100);

  Serial.println("  Playing 3 tones: 800 Hz, 1200 Hz, 1600 Hz.");
  buzzerToneHz(800, 250);  delay(250);
  buzzerToneHz(1200, 250); delay(250);
  buzzerToneHz(1600, 250); delay(250);

  buzzerOffHard();
  delay(300);

  bool ok = askYesNo("  Did you hear the 3 beeps, and did it go silent afterwards?");

  if (!ok) {
    Serial.println("  Trying simple on/off pulsing (should end silent)...");
    buzzerOffHard();
    for (int i = 0; i < 6; i++) {
      pinMode(PIN_BUZZER, OUTPUT);
      digitalWrite(PIN_BUZZER, BUZZER_ON_LEVEL);
      delay(250);
      digitalWrite(PIN_BUZZER, BUZZER_OFF_LEVEL);
      delay(250);
    }
    buzzerOffHard();
    ok = askYesNo("  Did you hear buzzing/clicking during pulsing, and then silence?");
  }

  buzzerOffHard();
  return ok;
}

// ---------------- Summary ----------------
static void printSummary() {
  Serial.println("\n==================== TEST SUMMARY ====================");
  Serial.printf("I2C scan:        %s\n", result.i2cScanOk ? "PASS" : "FAIL");
  if (result.oledAddr) Serial.printf("OLED address:    0x%02X\n", result.oledAddr);
  Serial.printf("OLED display:    %s\n", result.oledOk ? "PASS" : "FAIL");
  Serial.printf("Ignition LED:    %s\n", result.ignLedOk ? "PASS" : "FAIL");
  Serial.printf("Ignition button: %s (detected presses: %d)\n", result.ignBtnOk ? "PASS" : "FAIL", result.ignBtnPressedCount);
  Serial.printf("Arm/Safe switch: %s\n", result.armSafeOk ? "PASS" : "FAIL");
  Serial.printf("Battery ADC:     %s (Vadc=%.3f V, Vbatt~%.2f V)\n", result.battOk ? "PASS" : "FAIL", result.battVadc, result.battVest);
  Serial.printf("WS2812 RGB:      %s\n", result.wsOk ? "PASS" : "FAIL");
  Serial.printf("Buzzer:          %s\n", result.buzzerOk ? "PASS" : "FAIL");
  Serial.println("======================================================");

  bool allOk = result.i2cScanOk && result.oledOk && result.ignLedOk && result.ignBtnOk &&
               result.armSafeOk && result.battOk && result.wsOk && result.buzzerOk;

  Serial.printf("OVERALL: %s\n", allOk ? "PASS ✅" : "FAIL ❌");
}

// ---------------- Arduino entry points ----------------
void setup() {
  Serial.begin(115200);
  delay(200);

  // Helps both UART-serial and native USB CDC
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0 < 2000)) delay(10);
  flushSerialInput();

  Serial.println("\nESP32-S3 Peripheral Interactive Tester");
  Serial.println("Follow prompts in Serial Monitor.");
  Serial.println("Set Serial Monitor to: 115200 baud, Newline (or Both NL/CR).\n");

  // Ensure buzzer starts OFF
  buzzerOffHard();

  waitEnter("Press ENTER to start the tests");

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);

  result.oledAddr = i2cScanForOledAddr();
  result.i2cScanOk = (result.oledAddr != 0);

  result.oledOk = testOLED(result.oledAddr);

  (void)testIgnitionButtonAndLed();

  result.armSafeOk = testArmSafe();

  result.battOk = testBattery();

  result.wsOk = testWS2812();

  result.buzzerOk = testBuzzer();

  printSummary();

  Serial.println("\nDone. Reset board to run again.");
}

void loop() {
  delay(1000);
}
