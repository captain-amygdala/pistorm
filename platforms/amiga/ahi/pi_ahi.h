// SPDX-License-Identifier: MIT

#define AHIST_M8S		0
#define AHIST_M16S		1
#define AHIST_S8S		2
#define AHIST_S16S		3
#define AHIST_M32S		8
#define AHIST_S32S		10

#define AHIST_SAMPLE		  0
#define AHIST_DYNAMICSAMPLE	  1
#define AHIST_INPUT		(1<<29)

uint32_t pi_ahi_init(char *dev);
void pi_ahi_shutdown();
void handle_pi_ahi_write(uint32_t addr_, uint32_t val, uint8_t type);
uint32_t handle_pi_ahi_read(uint32_t addr_, uint8_t type);
int get_ahi_sample_size(uint16_t type);
int get_ahi_channels(uint16_t type);
void pi_ahi_set_playback_rate(uint32_t rate);
