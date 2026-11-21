#include <avr_ioport.h>
#include <sim_avr.h>
#include <sim_cycle_timers.h>
#include <sim_hex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { Busy = 8, Write, Reset, Passed, HoldBusy, PinCount };
enum { Clock = 16000000 };

struct board {
  const char* name;
  const char* mcu;
  const char* pins[PinCount];
};

static const struct board boards[] = {
    {"uno",
     "atmega328p",
     {"D2", "D3", "D4", "D5", "D6", "D7", "B0", "B1", "B2", "B3", "B4", "C2",
      "C3"}},
    {"mega",
     "atmega2560",
     {"E4", "E5", "G5", "E3", "H3", "H4", "H5", "H6", "B4", "B5", "B6", "F2",
      "F3"}},
    {"promicro",
     "atmega32u4",
     {"D1", "D0", "D4", "C6", "D7", "E6", "B4", "B5", "B6", "F7", "F6", "F5",
      "F4"}},
};

static const uint8_t expected[] = {
    0x1b, 0x40, 0x0c,                           // initialize
    0x1f, 0x24, 4,    1,    2, 0, 'H', 'i',     // text
    0x1f, 0x24, 120,  0,    3, 0,               // bitmap cursor
    0x1f, 0x28, 0x66, 0x11, 2, 0, 2,   0,   1,  // bitmap header
    0x80, 0x01, 0xff, 0x00};                    // bitmap bytes

struct fixture;
struct signal {
  struct fixture* fixture;
  unsigned index;
  bool high;
};

struct fixture {
  avr_t* avr;
  avr_irq_t* busy_pin;
  struct signal signals[PinCount];
  uint64_t data_changed, wr_fell, last_write, reset_fell, timeout_started;
  unsigned count;
  bool saw_reset, ready, busy, held, passed, failed;
};

static void require(struct fixture* f, bool condition, const char* message) {
  if (!condition) {
    fprintf(stderr, "cycle %llu: %s\n", (unsigned long long)f->avr->cycle,
            message);
    f->failed = true;
  }
}

static void drive_busy(struct fixture* f, bool high) {
  f->busy = high;
  avr_raise_irq(f->busy_pin, high);
}

static avr_cycle_count_t release_busy(avr_t* avr, avr_cycle_count_t cycle,
                                      void* parameter) {
  struct fixture* f = parameter;
  (void)avr;
  (void)cycle;
  if (!f->held) {
    drive_busy(f, false);
  }
  return 0;
}

static avr_cycle_count_t assert_busy(avr_t* avr, avr_cycle_count_t cycle,
                                     void* parameter) {
  struct fixture* f = parameter;
  (void)cycle;
  drive_busy(f, true);
  avr_cycle_timer_register(avr, 25 * (Clock / 1000000), release_busy, f);
  return 0;
}

static void capture_byte(struct fixture* f, uint64_t cycle) {
  uint8_t value = 0;
  for (unsigned bit = 0; bit < 8; ++bit) {
    value |= f->signals[bit].high << bit;
  }
  require(f, !f->busy && !f->held, "write while PBUSY is high");
  require(f, f->wr_fell && cycle - f->wr_fell >= 2, "/WR pulse below 100 ns");
  require(f, cycle - f->data_changed >= 1, "data setup below 50 ns");
  printf("cycle %llu: /WR rising, byte %02x\n", (unsigned long long)cycle,
         value);
  require(f, f->count < sizeof expected && value == expected[f->count],
          "unexpected byte");
  ++f->count;
  f->last_write = cycle;
  // Assert after one 16 MHz clock (62.5 ns) to model propagation.
  avr_cycle_timer_register(f->avr, 1, assert_busy, f);
}

static void gpio_changed(struct avr_irq_t* irq, uint32_t value,
                         void* parameter) {
  struct signal* signal = parameter;
  struct fixture* f = signal->fixture;
  const uint64_t cycle = f->avr->cycle;
  const bool high = (value & 1u) != 0;
  (void)irq;
  if (signal->high == high) {
    return;
  }
  signal->high = high;

  if (signal->index < 8) {
    require(f, !f->last_write || cycle > f->last_write,
            "data hold below 10 ns");
    f->data_changed = cycle;
  } else if (signal->index == Reset) {
    if (!high) {
      f->saw_reset = true;
      f->reset_fell = cycle;
    } else if (f->saw_reset) {
      require(f, cycle - f->reset_fell >= Clock / 1000, "short reset pulse");
      f->ready = true;
    }
  } else if (signal->index == Write && f->ready) {
    if (high) {
      capture_byte(f, cycle);
    } else {
      require(f, !f->busy, "/WR fell while PBUSY was high");
      f->wr_fell = cycle;
    }
  } else if (signal->index == HoldBusy && high) {
    require(f, !f->busy, "sketch did not wait for final PBUSY release");
    f->timeout_started = cycle;
    f->held = true;
    drive_busy(f, true);
  } else if (signal->index == Passed && high) {
    require(f, f->ready && f->count == sizeof expected && f->held,
            "incomplete transaction");
    const uint64_t elapsed = cycle - f->timeout_started;
    require(f,
            elapsed >= 119 * (Clock / 1000) && elapsed <= 125 * (Clock / 1000),
            "timeout outside 119–125 ms");
    f->passed = true;
  }
}

static const struct board* find_board(const char* name) {
  for (size_t i = 0; i < sizeof boards / sizeof *boards; ++i) {
    if (strcmp(boards[i].name, name) == 0) {
      return &boards[i];
    }
  }
  return NULL;
}

int main(int argc, char** argv) {
  const struct board* board = argc == 3 ? find_board(argv[1]) : NULL;
  if (!board) {
    fprintf(stderr, "Usage: %s uno|mega|promicro firmware.hex\n", argv[0]);
    return 2;
  }

  uint32_t size, offset;
  uint8_t* firmware = read_ihex_file(argv[2], &size, &offset);
  avr_t* avr = avr_make_mcu_by_name(board->mcu);
  if (!firmware || !avr || avr_init(avr) != 0 || offset != 0) {
    fprintf(stderr, "Could not initialize %s from %s\n", board->mcu, argv[2]);
    free(firmware);
    return 2;
  }
  avr->frequency = Clock;
  avr_loadcode(avr, firmware, size, offset);
  free(firmware);

  struct fixture f = {.avr = avr};
  for (unsigned i = 0; i < PinCount; ++i) {
    const char* pin = board->pins[i];
    avr_irq_t* irq =
        avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(pin[0]), pin[1] - '0');
    if (!irq) {
      fprintf(stderr, "Emulator missing %s\n", pin);
      return 2;
    }
    f.signals[i] = (struct signal){&f, i, false};
    if (i == Busy) {
      f.busy_pin = irq;
      const avr_ioport_external_t external = {
          .name = pin[0], .mask = 1u << (pin[1] - '0'), .value = 0};
      avr_ioctl(avr, AVR_IOCTL_IOPORT_SET_EXTERNAL(pin[0]), (void*)&external);
    } else {
      avr_irq_register_notify(irq, gpio_changed, &f.signals[i]);
    }
  }
  drive_busy(&f, false);

  while (!f.failed && !f.passed && avr->cycle < Clock / 2) {
    const int state = avr_run(avr);
    if (state == cpu_Done || state == cpu_Crashed) {
      break;
    }
  }
  require(&f, f.passed, "sketch did not complete");
  avr_terminate(avr);
  return f.failed ? 1 : 0;
}
