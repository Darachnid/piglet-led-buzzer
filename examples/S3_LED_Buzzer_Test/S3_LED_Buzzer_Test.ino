/*
 * S3 LED + buzzer wiring test (Seeed XIAO ESP32-S3)
 *
 * WS2811 / NeoPixel data -> D0 (GPIO1), 330 ohm series, common GND
 * Active buzzer +        -> D2 (GPIO3)  (change BUZZER_PIN / BUZZER_ACTIVE if needed)
 *
 * Board: XIAO_ESP32S3
 * Library: Adafruit NeoPixel (Library Manager)
 *
 * Success: Serial describes state; LED cycles red/green/blue/white; one beep at boot.
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

static const uint8_t LED_PIN     = 1;   // D0
static const uint8_t BUZZER_PIN  = 3;   // D2
static const bool    BUZZER_ACTIVE_HIGH = true;  // active buzzer: true = HIGH on
static const bool    TEST_BUZZER = true;

static const uint8_t NUM_PIXELS = 1;
static Adafruit_NeoPixel strip(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

static void buzzerBeep(uint16_t ms) {
  if (!TEST_BUZZER) return;
  digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_HIGH ? HIGH : LOW);
  delay(ms);
  digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_HIGH ? LOW : HIGH);
}

static void setRgb(uint8_t r, uint8_t g, uint8_t b) {
  strip.setPixelColor(0, strip.Color(r, g, b));
  strip.show();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== S3 LED / Buzzer test ===");
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  Serial.println("[OK] Built for ESP32-S3");
#else
  Serial.println("[WARN] Not ESP32-S3 — pin map may differ");
#endif
  Serial.printf("LED data pin: GPIO %u (XIAO D0)\n", (unsigned)LED_PIN);
  Serial.printf("Buzzer pin:   GPIO %u (XIAO D2)\n", (unsigned)BUZZER_PIN);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_HIGH ? LOW : HIGH);

  strip.begin();
  strip.setBrightness(128);
  strip.clear();
  strip.show();

  setRgb(255, 255, 255);
  Serial.println("LED: WHITE (boot)");
  buzzerBeep(120);
  Serial.println("Buzzer: one short beep");
  delay(400);
}

void loop() {
  struct { const char* name; uint8_t r, g, b; } steps[] = {
    { "RED",   255, 0,   0   },
    { "GREEN", 0,   200, 0   },
    { "BLUE",  0,   0,   255 },
    { "OFF",   0,   0,   0   },
  };
  for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
    setRgb(steps[i].r, steps[i].g, steps[i].b);
    Serial.printf("LED: %s\n", steps[i].name);
    delay(800);
  }
}
