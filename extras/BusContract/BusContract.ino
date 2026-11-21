#include <GU256X64D7000.h>

using namespace gu256x64d7000;

#if defined(ARDUINO_AVR_PROMICRO)
using BoardWiring = ArduinoWiring<2, 3, 4, 5, 6, 7, 8, 9, 10, A0, A1>;
#else
using BoardWiring = ArduinoWiring<2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12>;
#endif

constexpr auto passed = A2;
constexpr auto holdBusy = A3;
Display<BoardWiring> vfd;

void setup() {
  pinMode(passed, OUTPUT);
  pinMode(holdBusy, OUTPUT);
  vfd.begin();
  const uint8_t pixels[] = {0x80, 0x01, 0xff, 0x00};
  if (!vfd.initialize() || !vfd.drawTextAt(260, 2, "Hi") ||
      !vfd.drawBitmap(120, 3, 2, 2, pixels)) {
    return;
  }

  digitalWrite(holdBusy, HIGH);
  const uint32_t started = millis();
  if (!vfd.write(0x55) && uint32_t(millis() - started) >= 120) {
    digitalWrite(passed, HIGH);
  }
}

void loop() {}
