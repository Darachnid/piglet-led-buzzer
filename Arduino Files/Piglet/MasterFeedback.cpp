#include "MasterFeedback.h"
#include "Config.h"
#include "Globals.h"
#include <Adafruit_NeoPixel.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define MASTER_FEEDBACK_HW 1
#else
  #define MASTER_FEEDBACK_HW 0
#endif

static Adafruit_NeoPixel* rgb = nullptr;

static bool gpsFixHyst      = false;
static bool gpsAlertPending = false;
static uint32_t gpsLostAtMs = 0;
static uint32_t gpsFixSinceMs = 0;
static uint32_t gpsLostSinceMs = 0;
static const uint32_t GPS_HYST_MS = 4000;

static bool criticalError = false;
static bool scanActive    = false;
static uint32_t nodeDiscUntilMs = 0;
static uint32_t bootPulseUntilMs = 0;

static bool passiveBuzzer = false;
static uint8_t buzzerDuty = 128;

// Non-blocking buzzer sequencer
static uint8_t  beepRemaining = 0;
static uint16_t beepFreq = 880;
static uint16_t beepOnMs = 90;
static uint16_t beepGapMs = 70;
static uint32_t beepPhaseMs = 0;
static bool     beepOnPhase = true;
static bool     buzzerPinAttached = false;

enum LedMode : uint8_t {
  LED_BOOT_PULSE,
  LED_GPS_SEARCH,
  LED_GPS_FIX,
  LED_GPS_LOST_FLASH,
  LED_SCAN_PULSE,
  LED_NODE_DISC,
  LED_CRITICAL,
};

static LedMode currentLedMode = LED_BOOT_PULSE;
static uint32_t ledPhaseMs = 0;
static bool ledPhaseState = false;

static bool masterHardwareEnabled() {
#if !MASTER_FEEDBACK_HW
  return false;
#endif
  if (cfg.deviceRole == "node") return false;
  return cfg.ws2811Enable || cfg.buzzerEnable;
}

static bool rgbEnabled() {
  return MASTER_FEEDBACK_HW && cfg.ws2811Enable && cfg.ws2811Pin >= 0 && cfg.deviceRole != "node";
}

static bool buzzerEnabled() {
  return MASTER_FEEDBACK_HW && cfg.buzzerEnable && cfg.buzzerPin >= 0 && cfg.deviceRole != "node";
}

static void buzzerStop() {
  if (!buzzerEnabled()) return;
  if (passiveBuzzer && buzzerPinAttached) {
    ledcWrite(cfg.buzzerPin, 0);
    ledcDetach(cfg.buzzerPin);
    buzzerPinAttached = false;
  } else {
    digitalWrite(cfg.buzzerPin, LOW);
  }
}

static void buzzerStartTone() {
  if (!buzzerEnabled()) return;
  if (passiveBuzzer) {
    if (!buzzerPinAttached) {
      ledcAttach(cfg.buzzerPin, beepFreq, 8);
      buzzerPinAttached = true;
    }
    ledcWrite(cfg.buzzerPin, buzzerDuty);
  } else {
    pinMode(cfg.buzzerPin, OUTPUT);
    digitalWrite(cfg.buzzerPin, HIGH);
  }
}

static void queueBeeps(uint8_t count, uint16_t freq, uint16_t onMs, uint16_t gapMs) {
  if (!buzzerEnabled() || count == 0) return;
  beepRemaining = count;
  beepFreq = freq;
  beepOnMs = onMs;
  beepGapMs = gapMs;
  beepOnPhase = true;
  beepPhaseMs = millis();
  buzzerStartTone();
}

static void buzzerTick(uint32_t now) {
  if (beepRemaining == 0) return;
  uint16_t phaseLen = beepOnPhase ? beepOnMs : beepGapMs;
  if (now - beepPhaseMs < phaseLen) return;
  beepPhaseMs = now;
  if (beepOnPhase) {
    buzzerStop();
    beepOnPhase = false;
  } else {
    beepRemaining--;
    if (beepRemaining == 0) return;
    beepOnPhase = true;
    buzzerStartTone();
  }
}

static void rgbSet(uint8_t r, uint8_t g, uint8_t b) {
  if (!rgb || !rgbEnabled()) return;
  uint8_t br = cfg.ws2811Brightness;
  if (br > 255) br = 255;
  rgb->setPixelColor(0, rgb->Color((r * br) / 255, (g * br) / 255, (b * br) / 255));
  rgb->show();
}

static LedMode pickLedMode(uint32_t now) {
  if (criticalError) return LED_CRITICAL;
  if (now < nodeDiscUntilMs) return LED_NODE_DISC;
  if (now < bootPulseUntilMs) return LED_BOOT_PULSE;
  if (gpsAlertPending && now - gpsLostAtMs < 400) return LED_GPS_LOST_FLASH;
  if (scanActive && gpsFixHyst) return LED_SCAN_PULSE;
  if (gpsFixHyst) return LED_GPS_FIX;
  return LED_GPS_SEARCH;
}

static void ledTick(uint32_t now) {
  if (!rgbEnabled()) return;
  LedMode mode = pickLedMode(now);
  if (mode != currentLedMode) {
    currentLedMode = mode;
    ledPhaseMs = now;
    ledPhaseState = false;
  }

  switch (currentLedMode) {
    case LED_BOOT_PULSE:
      rgbSet(255, 255, 255);
      break;
    case LED_GPS_SEARCH:
      if (now - ledPhaseMs >= 900) { ledPhaseMs = now; ledPhaseState = !ledPhaseState; }
      rgbSet(ledPhaseState ? 180 : 0, ledPhaseState ? 90 : 0, 0);
      break;
    case LED_GPS_FIX:
      rgbSet(0, 200, 0);
      break;
    case LED_GPS_LOST_FLASH:
      rgbSet(255, 0, 0);
      break;
    case LED_SCAN_PULSE:
      if (now - ledPhaseMs >= 120) { ledPhaseMs = now; ledPhaseState = !ledPhaseState; }
      if (ledPhaseState) rgbSet(0, 180, 255);
      else rgbSet(0, 200, 0);
      break;
    case LED_NODE_DISC:
      rgbSet(160, 0, 200);
      break;
    case LED_CRITICAL:
      if (now - ledPhaseMs >= 350) { ledPhaseMs = now; ledPhaseState = !ledPhaseState; }
      rgbSet(ledPhaseState ? 255 : 0, 0, 0);
      break;
  }
}

static void gpsSample(bool hasFix, uint32_t now) {
  if (hasFix) {
    gpsLostSinceMs = 0;
    if (gpsFixSinceMs == 0) gpsFixSinceMs = now;
    if (!gpsFixHyst && now - gpsFixSinceMs >= GPS_HYST_MS) {
      gpsFixHyst = true;
      gpsAlertPending = false;
      if (cfg.alertGpsFix) queueBeeps(1, 1200, 100, 0);
    }
  } else {
    gpsFixSinceMs = 0;
    if (gpsLostSinceMs == 0) gpsLostSinceMs = now;
    if (gpsFixHyst && now - gpsLostSinceMs >= GPS_HYST_MS) {
      gpsFixHyst = false;
      gpsAlertPending = true;
      gpsLostAtMs = now;
      if (cfg.alertGpsFix) queueBeeps(2, 900, 90, 80);
    }
  }
}

void masterFeedbackInit() {
  if (!masterHardwareEnabled()) return;

  buzzerDuty = (uint8_t)cfg.buzzerVolume;
  if (buzzerDuty == 0) buzzerDuty = 128;
  passiveBuzzer = (cfg.buzzerType == "passive");
  if (buzzerEnabled()) pinMode(cfg.buzzerPin, OUTPUT);

  if (rgbEnabled()) {
    rgb = new Adafruit_NeoPixel(1, cfg.ws2811Pin, NEO_GRB + NEO_KHZ800);
    rgb->begin();
    rgb->setBrightness(255);
    rgb->clear();
    rgb->show();
  }

  bootPulseUntilMs = millis() + 300;
  if (cfg.buzzerEnable && cfg.buzzerStartupTone) queueBeeps(1, 880, 80, 0);
}

void masterFeedbackNotifyBootComplete() {}

void masterFeedbackOnGpsFixChange(bool hasFix) {
  (void)hasFix;
}

void masterFeedbackOnNodeDisconnect() {
  nodeDiscUntilMs = millis() + 800;
  if (cfg.alertNodeDisconnect) queueBeeps(3, 700, 70, 60);
}

void masterFeedbackSetCriticalError(bool active) {
  criticalError = active;
}

void masterFeedbackSetScanningActive(bool active) {
  scanActive = active;
}

void masterFeedbackTick() {
  if (!masterHardwareEnabled()) return;
  uint32_t now = millis();
  gpsSample(gpsHasFix, now);
  if (gpsAlertPending && now - gpsLostAtMs >= 400) gpsAlertPending = false;
  buzzerTick(now);
  ledTick(now);
}
