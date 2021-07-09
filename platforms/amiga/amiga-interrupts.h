// SPDX-License-Identifier: MIT

#ifndef PISTORM_AMIGA_INTERRUPTS_H
#define PISTORM_AMIGA_INTERRUPTS_H

typedef enum {
  TBE,    //Serial port transmit buffer empty
  DSKBLK, //Disk block finished
  SOFT,   //Reserved for software initiated interrupt
  PORTS,  //I/O Ports and timers
  COPER,  //Coprocessor
  VERTB,  //Start of vertical blank
  BLIT,   //Blitter has finished
  AUD0,   //Audio channel 0 block finished
  AUD1,   //Audio channel 1 block finished
  AUD2,   //Audio channel 2 block finished
  AUD3,   //Audio channel 3 block finished
  RBF,    //Serial port receive buffer full
  DSKSYN, //Disk sync register (DSKSYNC) matches disk
  EXTER,  //External interrupt
} AMIGA_IRQ;

void amiga_emulate_irq(AMIGA_IRQ irq);
uint8_t amiga_emulated_ipl();
int amiga_emulating_irq(AMIGA_IRQ irq);
void amiga_clear_emulating_irq();
int amiga_handle_intrqr_read(uint32_t *res);
int amiga_handle_intrq_write(uint32_t val);

#endif //PISTORM_AMIGA_INTERRUPTS_H
