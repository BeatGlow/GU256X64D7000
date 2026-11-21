#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__AVR_ATmega4809__)
#include "detail/Atmega4809.h"
#endif

namespace gu256x64d7000 {

namespace detail {

template <uint32_t Number>
struct ArduinoPin {
  static void high() { digitalWrite(Number, HIGH); }
  static void low() { digitalWrite(Number, LOW); }
  static bool read() { return digitalRead(Number) == HIGH; }

  static void outputHigh() {
    high();
    pinMode(Number, OUTPUT);
    high();
  }

  static void inputPullup() { pinMode(Number, INPUT_PULLUP); }
};

template <uint32_t... Pins>
struct DistinctPins;

template <uint32_t Pin, uint32_t... Rest>
struct DistinctPins<Pin, Rest...> {
  static constexpr bool excludes(uint32_t other) {
    return Pin != other && DistinctPins<Rest...>::excludes(other);
  }

  static constexpr bool value =
      DistinctPins<Rest...>::excludes(Pin) && DistinctPins<Rest...>::value;
};

template <>
struct DistinctPins<> {
  static constexpr bool excludes(uint32_t) { return true; }
  static constexpr bool value = true;
};

template <uint32_t... Pins>
struct ArduinoData {
  struct Latches {};
  static Latches capture() { return {}; }

  static void begin() {
    const uint32_t pins[] = {Pins...};
    for (uint32_t pin : pins) {
      pinMode(pin, OUTPUT);
    }
  }

  static void write(uint8_t value, Latches) {
    const uint32_t pins[] = {Pins...};
    for (uint32_t pin : pins) {
      digitalWrite(pin, (value & 1) ? HIGH : LOW);
      value >>= 1;
    }
  }
};

struct ArduinoWriteTiming {
  static void afterData() { delayMicroseconds(2); }
  static void afterFallingEdge() { delayMicroseconds(2); }
  static void afterRisingEdge() { delayMicroseconds(2); }
};

}  // namespace detail

// D0..D7 in order, then PBUSY, /WR and /RESET. Use native 5V GPIO.
template <uint32_t D0, uint32_t D1, uint32_t D2, uint32_t D3, uint32_t D4,
          uint32_t D5, uint32_t D6, uint32_t D7, uint32_t BusyPin,
          uint32_t WritePin, uint32_t ResetPin>
struct ArduinoWiring {
  static_assert(detail::DistinctPins<D0, D1, D2, D3, D4, D5, D6, D7, BusyPin,
                                     WritePin, ResetPin>::value,
                "VFD pins must be distinct");

  using Data = detail::ArduinoData<D0, D1, D2, D3, D4, D5, D6, D7>;
  using Busy = detail::ArduinoPin<BusyPin>;
  using Write = detail::ArduinoPin<WritePin>;
  using Reset = detail::ArduinoPin<ResetPin>;
};

namespace detail {

#if defined(__AVR_ATmega4809__)
// Preserve the legacy timing for AVR wiring, including custom wiring types.
template <typename BoardWiring>
struct WriteTiming {
  __attribute__((always_inline)) static void afterData() {}
  __attribute__((always_inline)) static void afterFallingEdge() {
    asm volatile("nop\n\tnop" ::: "memory");
  }
  __attribute__((always_inline)) static void afterRisingEdge() {
    asm volatile("nop\n\tnop" ::: "memory");
  }
};

template <uint32_t... Pins>
struct WriteTiming<ArduinoWiring<Pins...>> : ArduinoWriteTiming {};
#else
template <typename BoardWiring>
struct WriteTiming : ArduinoWriteTiming {};
#endif

}  // namespace detail

#if defined(__AVR_ATmega4809__)
template <typename BoardWiring = NanoEveryWiring>
#else
template <typename BoardWiring>
#endif
class Display {
  using Data = typename BoardWiring::Data;
  using Busy = typename BoardWiring::Busy;
  using Write = typename BoardWiring::Write;
  using Reset = typename BoardWiring::Reset;
  using Timing = detail::WriteTiming<BoardWiring>;

 public:
  using Latches = typename Data::Latches;
  constexpr explicit Display(BoardWiring = {}) {}

  void begin() const {
    Write::outputHigh();
    Reset::outputHigh();
    Data::begin();
    Busy::inputPullup();
  }

  void reset() const {
    Reset::low();
    delay(2);
    Reset::high();
    delay(1);
  }

  __attribute__((always_inline)) bool busy() const { return Busy::read(); }

  // Call after observing busy. Short waits avoid calling millis().
  bool waitWhileBusy() const {
    uint8_t spin = 0;
    while (busy()) {
      if (++spin == 0) {
        const uint32_t started = millis();
        while (busy()) {
          if (millis() - started >= 120) {
            return false;
          }
        }
        break;
      }
    }
    return true;
  }

  bool waitReady() const { return !busy() || waitWhileBusy(); }
  __attribute__((always_inline)) Latches captureLatches() const {
    return Data::capture();
  }

  // The caller has already checked PBUSY. Keep other output latches stable
  // while sending a run of bytes with cached latches.
  inline __attribute__((always_inline)) void writeReady(uint8_t value,
                                                        Latches latches) const {
    Data::write(value, latches);
    Timing::afterData();
    Write::low();
    Timing::afterFallingEdge();
    Write::high();
    Timing::afterRisingEdge();
  }

  inline __attribute__((always_inline)) bool write(uint8_t value) const {
    if (!waitReady()) {
      return false;
    }
    writeReady(value, captureLatches());
    return true;
  }

  template <size_t N>
  bool command(const uint8_t (&bytes)[N]) const {
    for (uint8_t value : bytes) {
      if (!write(value)) {
        return false;
      }
    }
    return waitReady();
  }

  // Configure the pins with begin() before initializing the controller.
  bool initialize() const {
    reset();
    return restoreDefaults() && clear();
  }

  bool restoreDefaults() const { return command<2>({0x1b, 0x40}); }
  bool clear() const { return command<1>({0x0c}); }
  bool home() const { return command<1>({0x0b}); }

  bool setPower(bool enabled) const {
    return command<5>({0x1f, 0x28, 0x61, 0x40, uint8_t(enabled)});
  }

  bool setBrightness(uint8_t level) const {
    return level >= 1 && level <= 8 && command<3>({0x1f, 0x58, level});
  }

  bool setCursorVisible(bool visible) const {
    return command<3>({0x1f, 0x43, uint8_t(visible)});
  }

  // Affects subsequent writes to the display memory.
  bool setReverse(bool enabled) const {
    return command<3>({0x1f, 0x72, uint8_t(enabled)});
  }

  bool setFontScale(uint8_t horizontal, uint8_t vertical) const {
    return horizontal >= 1 && horizontal <= 4 && vertical >= 1 &&
           vertical <= 2 &&
           command<6>({0x1f, 0x28, 0x67, 0x40, horizontal, vertical});
  }

  bool setCursor(uint16_t x, uint8_t band) const {
    return x < 512 && band < 8 &&
           command<6>({0x1f, 0x24, uint8_t(x), uint8_t(x >> 8), band, 0});
  }

  bool writeText(const char* text) const {
    size_t count = 0;
    return measureText(text, count) &&
           writeBytes(reinterpret_cast<const uint8_t*>(text), count);
  }

  bool drawTextAt(uint16_t x, uint8_t band, const char* text) const {
    size_t count = 0;
    return measureText(text, count) && setCursor(x, band) &&
           writeBytes(reinterpret_cast<const uint8_t*>(text), count);
  }

  // Follow this header with columns * bands bytes using write or writeBytes.
  bool beginBitmap(uint16_t columns, uint8_t bands) const {
    return columns > 0 && columns <= 512 && bands > 0 && bands <= 8 &&
           command<9>({0x1f, 0x28, 0x66, 0x11, uint8_t(columns),
                       uint8_t(columns >> 8), bands, 0, 1});
  }

  // The data must cover a rectangle entirely within VFD display memory.
  bool drawBitmap(uint16_t x, uint8_t band, uint16_t columns, uint8_t bands,
                  const uint8_t* data, size_t count) const {
    return data && x < 512 && band < 8 && columns > 0 && columns <= 512 - x &&
           bands > 0 && bands <= 8 - band && count == size_t(columns) * bands &&
           setCursor(x, band) && beginBitmap(columns, bands) &&
           writeBytes(data, count);
  }

  template <size_t N>
  bool drawBitmap(uint16_t x, uint8_t band, uint16_t columns, uint8_t bands,
                  const uint8_t (&data)[N]) const {
    return drawBitmap(x, band, columns, bands, data, N);
  }

  bool writeBytes(const uint8_t* bytes, size_t count) const {
    const Latches latches = captureLatches();
    for (size_t i = 0; i < count; ++i) {
      if (!waitReady()) {
        return false;
      }
      writeReady(bytes[i], latches);
    }
    return waitReady();
  }

 private:
  static bool measureText(const char* text, size_t& count) {
    if (!text) {
      return false;
    }
    while (text[count]) {
      const uint8_t character = text[count];
      if (character < 0x20 || character > 0x7e) {
        return false;
      }
      ++count;
    }
    return true;
  }
};

}  // namespace gu256x64d7000
