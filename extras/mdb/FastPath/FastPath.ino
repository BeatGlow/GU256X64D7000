#include <GU256X64D7000.h>

using namespace gu256x64d7000;

#if defined(ARDUINO_AVR_UNO_WIFI_REV2)
using BoardWiring = Wiring<SplitData<Port::D, 0, 6, Port::B, 0>,
                           Pin<Port::F, 5>, Pin<Port::B, 2>, Pin<Port::A, 1>>;
#else
using BoardWiring = NanoEveryWiring;
#endif

Display<BoardWiring> vfd;

volatile uint8_t vfd_result;

void setup() {
  uint8_t bytes[64];
  for (uint8_t i = 0; i < sizeof(bytes); ++i) {
    bytes[i] = 0x9a + i;
  }
  bytes[sizeof(bytes) - 1] = 0xa5;

  vfd.begin();
  asm volatile(".global vfd_busy_probe\nvfd_busy_probe:" ::: "memory");
  vfd_result = vfd.busy() ? 1 : 0;
  noInterrupts();
  asm volatile(".global vfd_burst_begin\nvfd_burst_begin:" ::: "memory");
  const bool sent = vfd.writeBytes(bytes, sizeof(bytes));
  asm volatile(".global vfd_burst_end\nvfd_burst_end:" ::: "memory");
  interrupts();
  vfd_result = vfd_result == 1 && sent ? 2 : 0;
  asm volatile(".global vfd_test_end\nvfd_test_end:" ::: "memory");
}

void loop() {}
