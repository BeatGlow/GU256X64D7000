#include <GU256X64D7000.h>

using namespace gu256x64d7000;

#if defined(ARDUINO_AVR_UNO_WIFI_REV2)
// Uno WiFi Rev2: A0..A5 and D9..D10 for data, D3 for PBUSY,
// D5 for /WR, and D7 for /RESET.
using BoardWiring = Wiring<SplitData<Port::D, 0, 6, Port::B, 0>,
                           Pin<Port::F, 5>, Pin<Port::B, 2>, Pin<Port::A, 1>>;
#elif defined(ARDUINO_AVR_NANO_EVERY)
using BoardWiring = NanoEveryWiring;
#elif defined(ARDUINO_AVR_PROMICRO)
// Pro Micro: D2..D9 for data, D10 for PBUSY, A0 for /WR, A1 for /RESET.
using BoardWiring = ArduinoWiring<2, 3, 4, 5, 6, 7, 8, 9, 10, A0, A1>;
#else
// Uno R3, Mega 2560 and Uno R4 Minima: D2..D9 for data, D10 for
// PBUSY, D11 for /WR, and D12 for /RESET.
using BoardWiring = ArduinoWiring<2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12>;
#endif

Display<BoardWiring> vfd{};

void setup() {
  vfd.begin();
  if (vfd.initialize()) {
    vfd.drawTextAt(0, 0, "Hello");
  }
}

void loop() {}
