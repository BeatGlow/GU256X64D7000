#pragma once

#include <stdint.h>

enum PinMode { OUTPUT, INPUT_PULLUP };
enum PinStatus { LOW, HIGH };

void pinMode(uint32_t pin, PinMode mode);
void digitalWrite(uint32_t pin, PinStatus value);
PinStatus digitalRead(uint32_t pin);

inline uint32_t millis() {
  static uint32_t tick;
  return tick++;
}

inline void delay(uint32_t) {}

inline void delayMicroseconds(uint32_t) {}
