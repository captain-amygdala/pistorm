#define PIGFX_RTG_BASE 0x70000000
#define PIGFX_RTG_SIZE 0x04000000

#define PIGFX_REG_SIZE 0x00010000

#define CARD_OFFSET 0

enum pi_regs {
  RTG_COMMAND = CARD_OFFSET + 0x00,
  RTG_X1      = CARD_OFFSET + 0x02,
  RTG_X2      = CARD_OFFSET + 0x04,
  RTG_X3      = CARD_OFFSET + 0x06,
  RTG_Y1      = CARD_OFFSET + 0x08,
  RTG_Y2      = CARD_OFFSET + 0x0A,
  RTG_Y3      = CARD_OFFSET + 0x0C,
  RTG_FORMAT  = CARD_OFFSET + 0x0E,
  RTG_RGB1    = CARD_OFFSET + 0x10,
  RTG_RGB2    = CARD_OFFSET + 0x14,
  RTG_ADDR1   = CARD_OFFSET + 0x18,
  RTG_ADDR2   = CARD_OFFSET + 0x1C,
  RTG_U81     = CARD_OFFSET + 0x20,
  RTG_U82     = CARD_OFFSET + 0x21,
  RTG_U83     = CARD_OFFSET + 0x22,
  RTG_U84     = CARD_OFFSET + 0x23,
};

enum rtg_cmds {
  RTGCMD_SETGC,
  RTGCMD_SETPAN,
  RTGCMD_SETCLUT,
  RTGCMD_ENABLE,
  RTGCMD_SETDISPLAY,
  RTGCMD_SETSWITCH,
  RTGCMD_FILLRECT,
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
void rtg_set_clut_entry(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void rtg_init_display();
void rtg_shutdown_display();

void rtg_fillrect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color, uint16_t pitch, uint16_t format, uint8_t mask);