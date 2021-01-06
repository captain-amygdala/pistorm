//#define BCM2708_PERI_BASE        0x20000000  //pi0-1
//#define BCM2708_PERI_BASE	0xFE000000     //pi4
#define BCM2708_PERI_BASE 0x3F000000  // pi3
#define BCM2708_PERI_SIZE 0x01000000
#define GPIO_BASE (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define GPCLK_BASE (BCM2708_PERI_BASE + 0x101000)
#define GPIO_ADDR 0x200000 /* GPIO controller */
#define GPCLK_ADDR 0x101000
#define CLK_PASSWD 0x5a000000
#define CLK_GP0_CTL 0x070
#define CLK_GP0_DIV 0x074

#define SA0 5
#define SA1 3
#define SA2 2

#define STATUSREGADDR  \
  GPIO_CLR = 1 << SA0; \
  GPIO_CLR = 1 << SA1; \
  GPIO_SET = 1 << SA2;
#define W16            \
  GPIO_CLR = 1 << SA0; \
  GPIO_CLR = 1 << SA1; \
  GPIO_CLR = 1 << SA2;
#define R16            \
  GPIO_SET = 1 << SA0; \
  GPIO_CLR = 1 << SA1; \
  GPIO_CLR = 1 << SA2;
#define W8             \
  GPIO_CLR = 1 << SA0; \
  GPIO_SET = 1 << SA1; \
  GPIO_CLR = 1 << SA2;
#define R8             \
  GPIO_SET = 1 << SA0; \
  GPIO_SET = 1 << SA1; \
  GPIO_CLR = 1 << SA2;

#define PAGE_SIZE (4 * 1024)
#define BLOCK_SIZE (4 * 1024)

#define GPIOSET(no, ishigh) \
  do {                      \
    if (ishigh)             \
      set |= (1 << (no));   \
    else                    \
      reset |= (1 << (no)); \
  } while (0)

#define JOY0DAT 0xDFF00A
#define JOY1DAT 0xDFF00C
#define CIAAPRA 0xBFE001
#define POTGOR  0xDFF016

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or
// SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio + ((g) / 10)) &= ~(7 << (((g) % 10) * 3))
#define OUT_GPIO(g) *(gpio + ((g) / 10)) |= (1 << (((g) % 10) * 3))
#define SET_GPIO_ALT(g, a)  \
  *(gpio + (((g) / 10))) |= \
      (((a) <= 3 ? (a) + 4 : (a) == 4 ? 3 : 2) << (((g) % 10) * 3))

#define GPIO_SET \
  *(gpio + 7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR \
  *(gpio + 10)  // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(g) (*(gpio + 13) & (1 << g))  // 0 if LOW, (1<<g) if HIGH

#define GPIO_PULL *(gpio + 37)      // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio + 38)  // Pull up/pull down clock

#define GPIO_HANDLE_IRQ \
  if (GET_GPIO(1) == 0) { \
    srdata = read_reg(); \
    m68k_set_irq((srdata >> 13) & 0xff); \
  } else { \
    if ((gayle_int & 0x80) && get_ide(0)->drive->intrq) { \
      write16(0xdff09c, 0x8008); \
      m68k_set_irq(2); \
    } \
    else \
        m68k_set_irq(0); \
  }; \

extern uint8_t gayle_int;

void setup_io();
void gpio_enable_200mhz();
void gpio_handle_irq();

int gpio_get_irq();

uint32_t read8(uint32_t address);
void write8(uint32_t address, uint32_t data);

uint32_t read16(uint32_t address);
void write16(uint32_t address, uint32_t data);

void write32(uint32_t address, uint32_t data);
uint32_t read32(uint32_t address);

uint16_t read_reg(void);
void write_reg(unsigned int value);
