// SPDX-License-Identifier: MIT

struct pi_ahi {
	struct Task *t_master;
	struct Library *ahi_base;
	struct Interrupt *play_soft_int;
	struct Interrupt *mix_soft_int;
	struct Task *t_slave;
	struct Process *slave_process;
	int8_t master_signal;
	int8_t slave_signal;
	int8_t play_signal;
	int8_t mix_signal;
	int8_t fart_signal;
	int8_t snake_signal;
	int8_t pad81;
	int8_t pad82;
	uint8_t *samp_buf[8];
	uint8_t *mix_buf[8];
	uint32_t mix_freq;
	uint32_t flags;
	int32_t monitor_volume, input_gain, output_volume;
	uint16_t buffer_type;
	uint16_t pad;
	uint32_t buffer_size;
	uint16_t disable_cnt;
	uint16_t quitting;
	uint16_t playing;
	struct AHIAudioCtrlDrv *audioctrl;
};
