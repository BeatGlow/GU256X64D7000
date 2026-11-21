#pragma once

namespace gu256x64d7000 {

enum class Port { A, B, C, D, E, F };

namespace detail {
template <Port P>
struct Registers {
  __attribute__((always_inline)) static VPORT_t& vport() {
    switch (P) {
      case Port::A:
        return VPORTA;
      case Port::B:
        return VPORTB;
      case Port::C:
        return VPORTC;
      case Port::D:
        return VPORTD;
      case Port::E:
        return VPORTE;
      case Port::F:
        return VPORTF;
    }
    __builtin_unreachable();
  }

  template <uint8_t Bit>
  static volatile uint8_t& pinControl() {
    // The Arduino emulation layer can redefine PORTB, PORTC, and PORTD.
    switch (P) {
      case Port::A:
        return _SFR_MEM8(_SFR_MEM_ADDR(PORTA_PIN0CTRL) + Bit);
      case Port::B:
        return _SFR_MEM8(_SFR_MEM_ADDR(PORTB_PIN0CTRL) + Bit);
      case Port::C:
        return _SFR_MEM8(_SFR_MEM_ADDR(PORTC_PIN0CTRL) + Bit);
      case Port::D:
        return _SFR_MEM8(_SFR_MEM_ADDR(PORTD_PIN0CTRL) + Bit);
      case Port::E:
        return _SFR_MEM8(_SFR_MEM_ADDR(PORTE_PIN0CTRL) + Bit);
      case Port::F:
        return _SFR_MEM8(_SFR_MEM_ADDR(PORTF_PIN0CTRL) + Bit);
    }
    __builtin_unreachable();
  }
};
}  // namespace detail

template <Port P, uint8_t Bit>
struct Pin {
  static_assert(Bit < 8, "GPIO bit must be in the range 0..7");
  static constexpr Port port = P;
  static constexpr uint8_t mask = 1u << Bit;
  static constexpr uint8_t bit = Bit;

  __attribute__((always_inline)) static void high() {
    detail::Registers<P>::vport().OUT |= mask;
  }
  __attribute__((always_inline)) static void low() {
    detail::Registers<P>::vport().OUT &= ~mask;
  }
  __attribute__((always_inline)) static bool read() {
    return detail::Registers<P>::vport().IN & mask;
  }

  static void outputHigh() {
    high();
    detail::Registers<P>::vport().DIR |= mask;
  }

  static void inputPullup() {
    detail::Registers<P>::vport().DIR &= ~mask;
    detail::Registers<P>::template pinControl<Bit>() = PORT_PULLUPEN_bm;
  }
};

// D0..D7 on the matching bits of a dedicated AVR port.
template <Port P>
struct DataPort {
  struct Latches {};
  template <Port Q>
  static constexpr uint8_t mask() {
    return P == Q ? 0xff : 0;
  }
  static void begin() { detail::Registers<P>::vport().DIR = 0xff; }
  __attribute__((always_inline)) static Latches capture() { return {}; }
  __attribute__((always_inline)) static void write(uint8_t value, Latches) {
    detail::Registers<P>::vport().OUT = value;
  }
};

// LowBits consecutive data lines on LowPort, followed by the rest on HighPort.
template <Port LowPort, uint8_t LowStart, uint8_t LowBits, Port HighPort,
          uint8_t HighStart>
struct SplitData {
  static_assert(LowPort != HighPort, "Use two distinct ports for SplitData");
  static_assert(LowBits > 0 && LowBits < 8,
                "SplitData requires one to seven low bits");
  static_assert(LowStart + LowBits <= 8 && HighStart + 8 - LowBits <= 8,
                "A data group exceeds its port");

  struct Latches {
    uint8_t low;
    uint8_t high;
  };
  static constexpr uint8_t lowMask = ((1u << LowBits) - 1) << LowStart;
  static constexpr uint8_t highMask = ((1u << (8 - LowBits)) - 1) << HighStart;

  template <Port P>
  static constexpr uint8_t mask() {
    return P == LowPort ? lowMask : P == HighPort ? highMask : 0;
  }

  static void begin() {
    detail::Registers<LowPort>::vport().DIR |= lowMask;
    detail::Registers<HighPort>::vport().DIR |= highMask;
  }

  __attribute__((always_inline)) static Latches capture() {
    return {uint8_t(detail::Registers<LowPort>::vport().OUT & ~lowMask),
            uint8_t(detail::Registers<HighPort>::vport().OUT & ~highMask)};
  }

  __attribute__((always_inline)) static void write(uint8_t value,
                                                   Latches latches) {
    detail::Registers<LowPort>::vport().OUT =
        latches.low | ((value << LowStart) & lowMask);
    detail::Registers<HighPort>::vport().OUT =
        latches.high | ((value >> LowBits) << HighStart);
  }
};

namespace detail {
template <Port P, uint8_t Index, typename... Pins>
struct PortMap;

template <Port P, uint8_t Index>
struct PortMap<P, Index> {
  static constexpr uint8_t mask = 0;
  __attribute__((always_inline)) static uint8_t encode(uint8_t) { return 0; }
};

template <Port P, uint8_t Index, typename First, typename... Rest>
struct PortMap<P, Index, First, Rest...> {
  using Tail = PortMap<P, Index + 1, Rest...>;
  static constexpr uint8_t pinMask = First::port == P ? First::mask : 0;
  static_assert((pinMask & Tail::mask) == 0, "Data pins must be distinct");
  static constexpr uint8_t mask = pinMask | Tail::mask;

  __attribute__((always_inline)) static uint8_t encode(uint8_t value) {
    return (First::port == P ? ((value >> Index) & 1) << First::bit : 0) |
           Tail::encode(value);
  }
};
}  // namespace detail

// Arbitrary wiring, listed as Pin types in order D0..D7. Only used ports are
// read or written. Scattered pins require more bit operations than aligned
// pins.
template <typename... Pins>
struct DataPins {
  static_assert(sizeof...(Pins) == 8,
                "Specify eight data pins in the order D0..D7");

  struct Latches {
    uint8_t a, b, c, d, e, f;
  };
  template <Port P>
  static constexpr uint8_t mask() {
    return detail::PortMap<P, 0, Pins...>::mask;
  }

  static void begin() {
    beginPort<Port::A>();
    beginPort<Port::B>();
    beginPort<Port::C>();
    beginPort<Port::D>();
    beginPort<Port::E>();
    beginPort<Port::F>();
  }

  __attribute__((always_inline)) static Latches capture() {
    return {capturePort<Port::A>(), capturePort<Port::B>(),
            capturePort<Port::C>(), capturePort<Port::D>(),
            capturePort<Port::E>(), capturePort<Port::F>()};
  }

  __attribute__((always_inline)) static void write(uint8_t value,
                                                   Latches latches) {
    writePort<Port::A>(value, latches.a);
    writePort<Port::B>(value, latches.b);
    writePort<Port::C>(value, latches.c);
    writePort<Port::D>(value, latches.d);
    writePort<Port::E>(value, latches.e);
    writePort<Port::F>(value, latches.f);
  }

 private:
  template <Port P>
  static void beginPort() {
    if (mask<P>()) {
      detail::Registers<P>::vport().DIR |= mask<P>();
    }
  }

  template <Port P>
  __attribute__((always_inline)) static uint8_t capturePort() {
    return mask<P>() ? detail::Registers<P>::vport().OUT & ~mask<P>() : 0;
  }

  template <Port P>
  __attribute__((always_inline)) static void writePort(uint8_t value,
                                                       uint8_t latch) {
    if (mask<P>()) {
      detail::Registers<P>::vport().OUT =
          latch | detail::PortMap<P, 0, Pins...>::encode(value);
    }
  }
};

// Busy is active high. Write and Reset are active low.
template <typename DataBus, typename BusyPin, typename WritePin,
          typename ResetPin>
struct Wiring {
  using Data = DataBus;
  using Busy = BusyPin;
  using Write = WritePin;
  using Reset = ResetPin;

  static_assert((Data::template mask<Busy::port>() & Busy::mask) == 0 &&
                    (Data::template mask<Write::port>() & Write::mask) == 0 &&
                    (Data::template mask<Reset::port>() & Reset::mask) == 0,
                "VFD control pins must not overlap data pins");
  static_assert((Busy::port != Write::port || Busy::bit != Write::bit) &&
                    (Busy::port != Reset::port || Busy::bit != Reset::bit) &&
                    (Write::port != Reset::port || Write::bit != Reset::bit),
                "VFD control pins must be distinct");
};

using NanoEveryWiring =
    Wiring<SplitData<Port::D, 0, 6, Port::B, 0>, Pin<Port::F, 4>,
           Pin<Port::C, 6>, Pin<Port::A, 0>>;

}  // namespace gu256x64d7000
