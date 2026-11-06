#include "MasterFeedback.h"
#include "Config.h"
#include "Globals.h"
#include <Adafruit_NeoPixel.h>
#include <vector>

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
static bool hwReady       = false;
static bool powerChirped  = false;
static bool bootComplete  = false;
static uint32_t nodeDiscUntilMs = 0;
static uint32_t cueUntilMs = 0;
static uint32_t yellowUntilMs = 0;
static uint8_t cueR = 0, cueG = 0, cueB = 0;

static std::vector<uint32_t> seenMacs;
static uint32_t chirpWindowMs = 0;
static uint8_t chirpsThisWindow = 0;

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
  LED_INIT_BLUE,
  LED_CUE,
  LED_YELLOW,
  LED_SCAN_PULSE,
  LED_NODE_DISC,
  LED_CRITICAL,
};

static LedMode currentLedMode = LED_INIT_BLUE;
static uint32_t ledPhaseMs = 0;
static bool ledPhaseState = false;

static bool rgbEnabled() {
  return MASTER_FEEDBACK_HW && cfg.ws2811Enable && cfg.ws2811Pin >= 0 && cfg.deviceRole != "node";
}

static bool buzzerEnabled() {
  return MASTER_FEEDBACK_HW && cfg.buzzerEnable && cfg.buzzerPin >= 0 && cfg.deviceRole != "node";
}

// Power chirp and new-network ticks use the pin even when the mute flag is off.
static bool buzzerPinOk() {
  return MASTER_FEEDBACK_HW && cfg.buzzerPin >= 0 && cfg.deviceRole != "node";
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
  if (now < yellowUntilMs) return LED_YELLOW;
  if (now < nodeDiscUntilMs) return LED_NODE_DISC;
  if (now < cueUntilMs) return LED_CUE;
  if (!bootComplete) return LED_INIT_BLUE;
  return LED_SCAN_PULSE;
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
    case LED_INIT_BLUE:
      rgbSet(0, 40, 255);
      break;
    case LED_CUE:
      rgbSet(cueR, cueG, cueB);
      break;
    case LED_YELLOW:
      rgbSet(255, 200, 0);
      break;
    case LED_SCAN_PULSE:
      if (now - ledPhaseMs >= 380) { ledPhaseMs = now; ledPhaseState = !ledPhaseState; }
      if (ledPhaseState) rgbSet(255, 0, 0);
      else rgbSet(150, 0, 255);
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

static void hwInit() {
  if (hwReady) return;
  if (!MASTER_FEEDBACK_HW || cfg.deviceRole == "node") return;
  if (!cfg.ws2811Enable && !buzzerPinOk()) return;

  buzzerDuty = (uint8_t)cfg.buzzerVolume;
  if (buzzerDuty == 0) buzzerDuty = 128;
  passiveBuzzer = (cfg.buzzerType == "passive");
  if (buzzerPinOk()) pinMode(cfg.buzzerPin, OUTPUT);

  if (rgbEnabled()) {
    rgb = new Adafruit_NeoPixel(1, cfg.ws2811Pin, NEO_GRB + NEO_KHZ800);
    rgb->begin();
    rgb->setBrightness(255);
    rgb->clear();
    rgb->show();
  }
  hwReady = true;
}

static void buzzerPulseBlocking(uint16_t freq, uint16_t onMs, uint8_t duty) {
  if (!buzzerPinOk() || onMs == 0) return;
  if (passiveBuzzer) {
    if (buzzerPinAttached) {
      ledcDetach(cfg.buzzerPin);
      buzzerPinAttached = false;
    }
    ledcAttach(cfg.buzzerPin, freq, 8);
    buzzerPinAttached = true;
    ledcWrite(cfg.buzzerPin, duty);
    delay(onMs);
    ledcWrite(cfg.buzzerPin, 0);
    ledcDetach(cfg.buzzerPin);
    buzzerPinAttached = false;
  } else {
    pinMode(cfg.buzzerPin, OUTPUT);
    digitalWrite(cfg.buzzerPin, HIGH);
    delay(onMs);
    digitalWrite(cfg.buzzerPin, LOW);
  }
}

static void holdCue(uint8_t r, uint8_t g, uint8_t b, uint32_t ms) {
  cueR = r;
  cueG = g;
  cueB = b;
  cueUntilMs = millis() + ms;
  rgbSet(r, g, b);
}

static uint32_t macHash(const char* mac) {
  uint32_t h = 2166136261u;
  for (const char* p = mac; *p; ++p) {
    h ^= (uint8_t)*p;
    h *= 16777619u;
  }
  return h ? h : 1u;
}

void masterFeedbackPowerOn() {
  hwInit();
  if (powerChirped) return;
  powerChirped = true;
  bootComplete = false;
  holdCue(0, 40, 255, 60000);
  // "doo" then "dee" — two notes, played here so they are heard before
  // the rest of setup blocks the main loop.
  if (buzzerPinOk()) {
    buzzerPulseBlocking(392, 170, 160);
    delay(55);
    buzzerPulseBlocking(587, 90, 140);
  }
}

void masterFeedbackOnConsoleLine(const char* line) {
  if (!line || !hwReady) return;
  if (strstr(line, "[GPS]")) {
    holdCue(0, 255, 50, 280);
  } else if (strstr(line, "[WIFI]") || strstr(line, "[NET]") || strstr(line, "[FWUPD]")) {
    holdCue(255, 150, 30, 280);  // light orange
  } else if (strstr(line, "[WDGWars]")) {
    holdCue(160, 0, 255, 280);
  } else if (strstr(line, "[WiGLE]")) {
    holdCue(255, 40, 150, 280);
  }
}

void masterFeedbackNoteNetwork(const char* bssid) {
  if (!bssid || !bssid[0]) return;
  uint32_t h = macHash(bssid);
  for (uint32_t v : seenMacs) {
    if (v == h) return;
  }
  if (seenMacs.size() < 8000) seenMacs.push_back(h);

  yellowUntilMs = millis() + 120;
  rgbSet(255, 200, 0);

  uint32_t now = millis();
  if (now - chirpWindowMs > 1000) {
    chirpWindowMs = now;
    chirpsThisWindow = 0;
  }
  if (chirpsThisWindow >= 150) return;
  if (beepRemaining) return;
  chirpsThisWindow++;
  buzzerPulseBlocking(3300, 6, 16);
}

void masterFeedbackInit() {
  hwInit();
}

void masterFeedbackNotifyBootComplete() {
  bootComplete = true;
  cueUntilMs = 0;
}

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
  if (!hwReady) return;
  (void)scanActive;
  uint32_t now = millis();
  gpsSample(gpsHasFix, now);
  if (gpsAlertPending && now - gpsLostAtMs >= 400) gpsAlertPending = false;
  buzzerTick(now);
  ledTick(now);
}
