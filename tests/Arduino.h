#pragma once

#include <stdint.h>

struct VPORT_t {
  uint8_t DIR, OUT, IN;
};

extern VPORT_t VPORTA, VPORTB, VPORTC, VPORTD, VPORTE, VPORTF;
extern uint8_t pinControl[48];

constexpr uint8_t PORT_PULLUPEN_bm = 8;
constexpr unsigned PORTA_PIN0CTRL = 0, PORTB_PIN0CTRL = 8, PORTC_PIN0CTRL = 16;
constexpr unsigned PORTD_PIN0CTRL = 24, PORTE_PIN0CTRL = 32,
                   PORTF_PIN0CTRL = 40;

#define _SFR_MEM_ADDR(reg) (reg)
#define _SFR_MEM8(address) (pinControl[(address)])

inline uint32_t millis() {
  static uint32_t tick;
  return tick++;
}

inline void delay(uint32_t) {}
