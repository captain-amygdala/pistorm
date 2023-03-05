#ifndef _PS_PROTOCOL_H
#define _PS_PROTOCOL_H

// REG_STATUS
#define STATUS_IS_BM        (1 << 0)
#define STATUS_RESET        (1 << 1)
#define STATUS_HALT         (1 << 2)
#define STATUS_IPL_MASK     (7 << 3)
#define STATUS_IPL_SHIFT    3
#define STATUS_TERM_NORMAL  (1 << 6)
#define STATUS_REQ_ACTIVE   (1 << 7)

// REG_CONTROL
#define CONTROL_REQ_BM      (1 << 0)
#define CONTROL_DRIVE_RESET (1 << 1)
#define CONTROL_DRIVE_HALT  (1 << 2)
#define CONTROL_DRIVE_INT2  (1 << 3)
#define CONTROL_DRIVE_INT6  (1 << 4)


#define PIN_IPL0        0
#define PIN_IPL1        1
#define PIN_IPL2        2
#define PIN_TXN         3
#define PIN_KBRESET     4
#define SER_OUT_BIT     5 //debug EMU68
#define PIN_RD          6
#define PIN_WR          7
#define PIN_D(x)        (8 + x)
#define PIN_A(x)        (24 + x)
#define SER_OUT_CLK     27 //debug EMU68


#define PIN_CRESET1 6
#define PIN_CRESET2 7
#define PIN_TESTN 17
#define PIN_CCK 22
#define PIN_SS 24
#define PIN_CBUS0 14
#define PIN_CBUS1 15
#define PIN_CBUS2 18
#define PIN_CDI0 10
#define PIN_CDI1 25
#define PIN_CDI2 9
#define PIN_CDI3 8
#define PIN_CDI4 11
#define PIN_CDI5 1
#define PIN_CDI6 16
#define PIN_CDI7 13


unsigned int ps_read_8(unsigned int address);
unsigned int ps_read_16(unsigned int address);
unsigned int ps_read_32(unsigned int address);

void ps_write_8(unsigned int address, unsigned int data);
void ps_write_16(unsigned int address, unsigned int data);
void ps_write_32(unsigned int address, unsigned int data);

unsigned int ps_read_status();
void ps_set_control(unsigned int value);
void ps_clr_control(unsigned int value);

void ps_setup_protocol();
void ps_reset_state_machine();
void ps_pulse_reset();

void ps_efinix_setup();
void ps_efinix_write(unsigned char data_out);
void ps_efinix_load(char* buffer, long length);

#define read8 ps_read_8
#define read16 ps_read_16
#define read32 ps_read_32

#define write8 ps_write_8
#define write16 ps_write_16
#define write32 ps_write_32


#endif /* _PS_PROTOCOL_H */
