// SPDX-License-Identifier: MIT

#include <stdint.h>
#include "config_file/config_file.h"
#include "amiga-registers.h"
#include "amiga-interrupts.h"
#include "gpio/ps_protocol.h"

static const uint8_t IPL[14] = {1, 1, 1, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 6 };

uint16_t emulated_irqs = 0x0000;
uint8_t emulated_ipl;

void amiga_emulate_irq(AMIGA_IRQ irq) {
  emulated_irqs |= 1 << irq;
  uint8_t ipl = IPL[irq];

  if (emulated_ipl < ipl) {
    emulated_ipl = ipl;
  }
}

inline uint8_t amiga_emulated_ipl() {
  return emulated_ipl;
}

inline int amiga_emulating_irq(AMIGA_IRQ irq) {
  return emulated_irqs & (1 << irq);
}

void amiga_clear_emulating_irq() {
  emulated_irqs = 0;
  emulated_ipl = 0;
}

inline int amiga_handle_intrqr_read(uint32_t *res) {
  if (emulated_irqs) {
    *res = ps_read_16(INTREQR) | emulated_irqs;
    return 1;
  }
  return 0;
}

int amiga_handle_intrq_write(uint32_t val) {
  if (emulated_irqs && !(val & 0x8000)) {
    uint16_t previous_emulated_irqs = emulated_irqs;
    uint16_t hardware_irqs_to_clear = val & ~emulated_irqs;
    emulated_irqs &= ~val;
    if (previous_emulated_irqs != emulated_irqs) {
      emulated_ipl = 0;
      for (int irq = 13; irq >= 0; irq--) {
        if (emulated_irqs & (1 << irq)) {
          emulated_ipl = IPL[irq];
        }
      }
    }
    if (hardware_irqs_to_clear) {
      ps_write_16(INTREQ, hardware_irqs_to_clear);
    }
    return 1;
  }
  return 0;
}
