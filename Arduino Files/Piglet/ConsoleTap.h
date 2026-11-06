#pragma once
#include <Arduino.h>

// Forwards Serial to the USB console and watches each line for boot tags.
class ConsoleTap : public Stream {
 public:
  void begin(unsigned long baud);
  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buffer, size_t size) override;
  int available() override;
  int read() override;
  int peek() override;
  void flush() override;
};

extern ConsoleTap CueConsole;
