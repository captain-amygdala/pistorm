void configure_rtc_emulation_amiga(uint8_t enabled);
void set_hard_drive_image_file_amiga(uint8_t index, char *filename);
int custom_read_amiga(struct emulator_config *cfg, unsigned int addr, unsigned int *val, unsigned char type);
int custom_write_amiga(struct emulator_config *cfg, unsigned int addr, unsigned int val, unsigned char type);

#define GAYLEBASE 0xD80000
#define GAYLESIZE 0x70000

#define CLOCKBASE 0xDC0000
#define CLOCKSIZE 0x010000
