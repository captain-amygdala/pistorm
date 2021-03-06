void configure_rtc_emulation_amiga(uint8_t enabled);
void set_hard_drive_image_file_amiga(uint8_t index, char *filename);

/* GARY ADDRESSES */
#define GARY_REG0 0xDE0000
#define GARY_REG1 0xDE0001
#define GARY_REG2 0xDE0002
#define GARY_REG3 0xDE1000
#define GARY_REG4 0xDE1001
#define GARY_REG5 0xDE1002

/* RAMSEY ADDRESSES */
#define RAMSEY_REG 0xDE0003 /* just a nibble, it should return 0x08 for defaults with 16MB */
#define RAMSEY_ID 0xDE0043  /* Either 0x0D or 0x0F (most recent version) */
/* RAMSEY TYPES */
#define RAMSEY_REV4 0x0D
#define RAMSEY_REV7 0x0F
