#define PIGFX_RTG_BASE 0x70000000
#define PIGFX_RTG_SIZE 0x04000000

#define PIGFX_REG_SIZE 0x00010000

#define CARD_OFFSET 0

enum pi_regs {
  RTG_COMMAND = 0x00,
  RTG_X1      = 0x02,
  RTG_X2      = 0x04,
  RTG_X3      = 0x06,
  RTG_Y1      = 0x08,
  RTG_Y2      = 0x0A,
  RTG_Y3      = 0x0C,
  RTG_FORMAT  = 0x0E,
  RTG_RGB1    = 0x10,
  RTG_RGB2    = 0x14,
  RTG_ADDR1   = 0x18,
  RTG_ADDR2   = 0x1C,
  RTG_U81     = 0x20,
  RTG_U82     = 0x21,
  RTG_U83     = 0x22,
  RTG_U84     = 0x23,
  RTG_X4      = 0x24,
  RTG_X5      = 0x26,
  RTG_Y4      = 0x28,
  RTG_Y5      = 0x2A,
  RTG_U1      = 0x2C,
  RTG_U2      = 0x2E,
};

enum rtg_cmds {
  RTGCMD_SETGC,
  RTGCMD_SETPAN,
  RTGCMD_SETCLUT,
  RTGCMD_ENABLE,
  RTGCMD_SETDISPLAY,
  RTGCMD_SETSWITCH,
  RTGCMD_FILLRECT,
  RTGCMD_BLITRECT,
};

enum rtg_formats {
  RTGFMT_8BIT,
  RTGFMT_RBG565,
  RTGFMT_RGB32,
  RTGFMT_RGB555,
  RTGFMT_NUM,
};

void rtg_write(uint32_t address, uint32_t value, uint8_t mode);
unsigned int rtg_read(uint32_t address, uint8_t mode);
void rtg_set_clut_entry(uint8_t index, uint32_t xrgb);
void rtg_init_display();
void rtg_shutdown_display();

void rtg_fillrect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color, uint16_t pitch, uint16_t format, uint8_t mask);
void rtg_blitrect(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t w, uint16_t h, uint16_t pitch, uint16_t format, uint8_t mask);