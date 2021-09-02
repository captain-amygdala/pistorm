// SPDX-License-Identifier: MIT

#define PI_AHI_OFFSET   0x88000000
#define PI_AHI_REGSIZE  0x00010000
#define PI_AHI_UPPER    0x8A000000

enum pi_ahi_regs {
    AHI_COMMAND     = 0x00,
    AHI_U1          = 0x02,
    AHI_U2          = 0x04,
    AHI_U3          = 0x06,
    AHI_U4          = 0x08,
    AHI_U5          = 0x0A,
    AHI_U6          = 0x0C,
    AHI_SNAKE       = 0x0E,
    AHI_ADDR1       = 0x10,
    AHI_ADDR2       = 0x14,
    AHI_ADDR3       = 0x18,
    AHI_ADDR4       = 0x1C,
    AHI_U81         = 0x20,
    AHI_U82         = 0x21,
    AHI_U83         = 0x22,
    AHI_U84         = 0x23,

    AHI_U321        = 0x30,
    AHI_U322        = 0x34,
    AHI_U323        = 0x38,
    AHI_U324        = 0x3C,

    AHI_DEBUGMSG    = 0x200,
    AHI_INTCHK      = 0x204,
    AHI_DISABLE     = 0x206,
};

enum pi_ahi_commands {
    AHI_CMD_PLAY,
    AHI_CMD_STOP,
    AHI_CMD_START,
    AHI_CMD_CHANNELS,
    AHI_CMD_RATE,
    AHI_CMD_FORMAT,
    AHI_CMD_ALLOCAUDIO,
    AHI_CMD_FREEAUDIO,
    AHI_CMD_NUM,
};

#define WRITESHORT(cmd, val) *(volatile unsigned short *)((unsigned long)(PI_AHI_OFFSET)+cmd) = val;
#define WRITELONG(cmd, val) *(volatile unsigned long *)((unsigned long)(PI_AHI_OFFSET)+cmd) = val;
#define WRITEBYTE(cmd, val) *(volatile unsigned char *)((unsigned long)(PI_AHI_OFFSET)+cmd) = val;

#define READSHORT(cmd) *(volatile unsigned short *)((unsigned long)(PI_AHI_OFFSET)+cmd)
#define READLONG(cmd) *(volatile unsigned long *)((unsigned long)(PI_AHI_OFFSET)+cmd)
#define READBYTE(cmd) *(volatile unsigned char *)((unsigned long)(PI_AHI_OFFSET)+cmd)
