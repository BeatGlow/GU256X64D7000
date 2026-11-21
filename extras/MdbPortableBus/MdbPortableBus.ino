#include <GU256X64D7000.h>

using namespace gu256x64d7000;

#if defined(ARDUINO_AVR_PROMICRO)
using BoardWiring = ArduinoWiring<2, 3, 4, 5, 6, 7, 8, 9, 10, A0, A1>;
#else
using BoardWiring = ArduinoWiring<2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12>;
#endif

// Breakpoints let MDB observe real GPIO without adding hooks to the library.
#define MARK(name) \
  asm volatile(".global mdb_" #name "\nmdb_" #name ": nop" ::: "memory")
#define SINGLE_ADDRESS __attribute__((noinline, noclone))

static bool reportBusy = true;

struct ObservedWiring : BoardWiring {
  struct Busy : BoardWiring::Busy {
    SINGLE_ADDRESS static bool read() {
      bool high = BoardWiring::Busy::read();
      if (high && reportBusy) {
        MARK(busy_read);
      }
      return high;
    }
  };

  struct Write : BoardWiring::Write {
    SINGLE_ADDRESS static void low() {
      BoardWiring::Write::low();
      MARK(write_low);
    }

    SINGLE_ADDRESS static void high() {
      BoardWiring::Write::high();
      MARK(write_high);
    }
  };

  struct Reset : BoardWiring::Reset {
    SINGLE_ADDRESS static void low() {
      BoardWiring::Reset::low();
      MARK(reset_low);
    }

    SINGLE_ADDRESS static void high() {
      BoardWiring::Reset::high();
      MARK(reset_high);
    }
  };
};

Display<ObservedWiring> vfd;

void setup() {
  vfd.begin();
  MARK(ready);
  const uint8_t pixels[] = {0x80, 0x01, 0xff, 0x00};
  if (vfd.initialize() && vfd.drawTextAt(260, 2, "Hi") &&
      vfd.drawBitmap(120, 3, 2, 2, pixels)) {
    reportBusy = false;
    MARK(timeout);
    const uint32_t started = millis();
    if (!vfd.write(0x55) && uint32_t(millis() - started) >= 120) {
      MARK(passed);
    }
  }
  MARK(finished);
}

void loop() {}

#if defined(ARDUINO_AVR_PROMICRO)
// MDB's ATmega32U4 model never locks the native USB PLL, so bypass
// USBDevice.attach() while retaining Arduino's timer and GPIO initialization.
int main() {
  init();
  setup();
  for (;;) {
    loop();
  }
}
#endif
