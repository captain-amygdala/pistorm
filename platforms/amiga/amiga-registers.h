void configure_rtc_emulation_amiga(uint8_t enabled);
void set_hard_drive_image_file_amiga(uint8_t index, char *filename);
int custom_read_amiga(struct emulator_config *cfg, unsigned int addr, unsigned int *val, unsigned char type);
int custom_write_amiga(struct emulator_config *cfg, unsigned int addr, unsigned int val, unsigned char type);

void put_rtc_byte(uint32_t address_, uint8_t value, uint8_t rtc_type);
uint8_t get_rtc_byte(uint32_t address_, uint8_t rtc_type);

#define GAYLEBASE 0xD80000
#define GAYLESIZE 0x070000
#define GAYLEMASK 0xDF0000

#define CLOCKBASE 0xDC0000
#define CLOCKSIZE 0x010000
#define CLOCKMASK 0x00FFFF

enum rtc_types {
    RTC_TYPE_MSM,
    RTC_TYPE_RICOH,
    RTC_TYPE_NONE,
};