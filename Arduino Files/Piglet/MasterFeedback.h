#pragma once
#include <Arduino.h>

// External WS2811/NeoPixel + buzzer — ESP32-S3 master/controller only.
void masterFeedbackInit();
void masterFeedbackTick();

// Call once config pins are known. Blue LED + two-note power chirp.
void masterFeedbackPowerOn();

// A full serial line (no newline). Boot tags set the LED color.
void masterFeedbackOnConsoleLine(const char* line);

// First time this BSSID is seen this boot: yellow pulse + a very short tick.
void masterFeedbackNoteNetwork(const char* bssid);

void masterFeedbackNotifyBootComplete();
void masterFeedbackOnGpsFixChange(bool hasFix);
void masterFeedbackOnNodeDisconnect();
void masterFeedbackSetCriticalError(bool active);

// Scanning activity hint (local wardrive or mesh core receiving records).
void masterFeedbackSetScanningActive(bool active);
