#include "ConsoleTap.h"
#include "MasterFeedback.h"

ConsoleTap CueConsole;

static char cueLine[200];
static uint8_t cueLen = 0;

static void cueFeed(uint8_t c) {
  if (c == '\r') return;
  if (c == '\n') {
    cueLine[cueLen] = '\0';
    if (cueLen > 0) masterFeedbackOnConsoleLine(cueLine);
    cueLen = 0;
    return;
  }
  if (cueLen < sizeof(cueLine) - 1) cueLine[cueLen++] = (char)c;
}

void ConsoleTap::begin(unsigned long baud) { Serial.begin(baud); }

size_t ConsoleTap::write(uint8_t c) {
  size_t n = Serial.write(c);
  if (n) cueFeed(c);
  return n;
}

size_t ConsoleTap::write(const uint8_t* buffer, size_t size) {
  size_t n = 0;
  for (size_t i = 0; i < size; i++) n += write(buffer[i]);
  return n;
}

int ConsoleTap::available() { return Serial.available(); }
int ConsoleTap::read() { return Serial.read(); }
int ConsoleTap::peek() { return Serial.peek(); }
void ConsoleTap::flush() { Serial.flush(); }
