#if defined(GU256X64D7000_HOST_TEST)

#include <GU256X64D7000.h>

#include <cassert>
#include <initializer_list>
#include <vector>

namespace {
std::vector<uint8_t> writes;
std::vector<bool> resetLevels;
uint8_t bus;
bool busy;

struct TestWiring {
  struct Data {
    struct Latches {};
    static void begin() {}
    static Latches capture() { return {}; }
    static void write(uint8_t value, Latches) { bus = value; }
  };
  struct Busy {
    static void inputPullup() {}
    static bool read() { return busy; }
  };
  struct Write {
    static void outputHigh() {}
    static void low() {
      assert(!busy);
      writes.push_back(bus);
    }
    static void high() {}
  };
  struct Reset {
    static void outputHigh() {}
    static void low() { resetLevels.push_back(false); }
    static void high() { resetLevels.push_back(true); }
  };
};

void expect(std::initializer_list<uint8_t> expected) {
  assert(writes == std::vector<uint8_t>(expected));
  writes.clear();
}

void testInitializeAndControls(const gu256x64d7000::Display<TestWiring>& vfd) {
  assert(vfd.initialize());
  assert((resetLevels == std::vector<bool>{false, true}));
  expect({0x1b, 0x40, 0x0c});
  assert(vfd.restoreDefaults() && vfd.clear() && vfd.home());
  expect({0x1b, 0x40, 0x0c, 0x0b});
  assert(vfd.setPower(false) && vfd.setBrightness(7));
  expect({0x1f, 0x28, 0x61, 0x40, 0, 0x1f, 0x58, 7});
  assert(vfd.setCursorVisible(true) && vfd.setReverse(true) &&
         vfd.setFontScale(4, 2));
  expect({0x1f, 0x43, 1, 0x1f, 0x72, 1, 0x1f, 0x28, 0x67, 0x40, 4, 2});

  assert(!vfd.setBrightness(0) && !vfd.setBrightness(9));
  assert(!vfd.setFontScale(5, 1) && !vfd.setFontScale(1, 3));
  busy = true;
  assert(!vfd.clear());
  busy = false;
  expect({});
}

void testTextAndBitmaps(const gu256x64d7000::Display<TestWiring>& vfd) {
  assert(vfd.writeText("Hi"));
  expect({'H', 'i'});
  assert(vfd.drawTextAt(260, 2, "Hi"));
  expect({0x1f, 0x24, 4, 1, 2, 0, 'H', 'i'});

  const uint8_t pixels[] = {0x80, 0x01, 0xff, 0x00};
  assert(vfd.drawBitmap(120, 3, 2, 2, pixels));
  expect({0x1f, 0x24, 120, 0, 3, 0, 0x1f, 0x28, 0x66, 0x11, 2, 0, 2, 0, 1, 0x80,
          0x01, 0xff, 0});

  assert(!vfd.drawTextAt(0, 0, "A\x1b") && !vfd.writeText(nullptr));
  assert(!vfd.drawTextAt(512, 0, "A"));
  assert(!vfd.drawBitmap(511, 3, 2, 2, pixels));
  assert(!vfd.drawBitmap(120, 7, 2, 2, pixels));
  assert(!vfd.drawBitmap(120, 3, 2, 2, pixels, 3));
  expect({});
}
}  // namespace

int main() {
  const gu256x64d7000::Display<TestWiring> vfd{};
  vfd.begin();
  testInitializeAndControls(vfd);
  testTextAndBitmaps(vfd);
}

#endif  // GU256X64D7000_HOST_TEST
