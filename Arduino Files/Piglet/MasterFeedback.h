#pragma once
#include <Arduino.h>

// External WS2811/NeoPixel + buzzer — ESP32-S3 master/controller only.
void masterFeedbackInit();
void masterFeedbackTick();

void masterFeedbackNotifyBootComplete();
void masterFeedbackOnGpsFixChange(bool hasFix);
void masterFeedbackOnNodeDisconnect();
void masterFeedbackSetCriticalError(bool active);

// Scanning activity hint (local wardrive or mesh core receiving records).
void masterFeedbackSetScanningActive(bool active);
