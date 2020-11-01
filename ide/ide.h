#include <stdint.h>

#define ACME_ROADRUNNER		1	/* 504MB classic IDE drive */
#define ACME_COYOTE		2	/* 20MB early IDE drive */
#define ACME_NEMESIS		3	/* 20MB LBA capable drive */
#define ACME_ULTRASONICUS	4	/* 40MB LBA capable drive */
#define ACME_ACCELLERATTI	5	/* 128MB LBA capable drive */
#define ACME_ZIPPIBUS		6	/* 256MB LBA capable drive */

#define MAX_DRIVE_TYPE		6

#define		ide_data	0
#define		ide_error_r	1
#define		ide_feature_w	1
#define		ide_sec_count	2
#define		ide_sec_num	3
#define		ide_lba_low	3
#define		ide_cyl_low	4
#define		ide_lba_mid	4
#define		ide_cyl_hi	5
#define		ide_lba_hi	5
#define		ide_dev_head	6
#define		ide_lba_top	6
#define		ide_status_r	7
#define		ide_command_w	7
#define		ide_altst_r	8
#define		ide_devctrl_w	8
#define		ide_data_latch	9

struct ide_taskfile {
  uint16_t data;
  uint8_t error;
  uint8_t feature;
  uint8_t count;
  uint8_t lba1;
  uint8_t lba2;
  uint8_t lba3;
  uint8_t lba4;
  uint8_t status;
  uint8_t command;
  uint8_t devctrl;
  struct ide_drive *drive;
};

struct ide_drive {
  struct ide_controller *controller;
  struct ide_taskfile taskfile;
  unsigned int present:1, intrq:1, failed:1, lba:1, eightbit:1;
  uint16_t cylinders;
  uint8_t heads, sectors;
  uint8_t data[512];
  uint16_t identify[256];
  uint8_t *dptr;
  int state;
  int fd;
  off_t offset;
  int length;
};

struct ide_controller {
  struct ide_drive drive[2];
  int selected;
  const char *name;
  uint16_t data_latch;
};

extern const uint8_t ide_magic[8];

void ide_reset_begin(struct ide_controller *c);
uint8_t ide_read8(struct ide_controller *c, uint8_t r);
void ide_write8(struct ide_controller *c, uint8_t r, uint8_t v);
uint16_t ide_read16(struct ide_controller *c, uint8_t r);
void ide_write16(struct ide_controller *c, uint8_t r, uint16_t v);
uint8_t ide_read_latched(struct ide_controller *c, uint8_t r);
void ide_write_latched(struct ide_controller *c, uint8_t r, uint8_t v);

struct ide_controller *ide_allocate(const char *name);
int ide_attach(struct ide_controller *c, int drive, int fd);
void ide_detach(struct ide_drive *d);
void ide_free(struct ide_controller *c);

int ide_make_drive(uint8_t type, int fd);
