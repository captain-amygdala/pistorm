// SPDX-License-Identifier: MIT

#include "emulator.h"
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include "config_file/config_file.h"
#include <pthread.h>
#include "gpio/ps_protocol.h"
#include "platforms/amiga/amiga-interrupts.h"
#include "pi_ahi.h"
#include "pi-ahi-enums.h"

#include <alsa/asoundlib.h>

//#define AHI_DEBUG

uint32_t ahi_u32[4];
uint32_t ahi_addr[4];
uint16_t ahi_u16[8];
uint16_t ahi_u8[8];

#ifdef AHI_DEBUG
void print_ahi_debugmsg(int val, int type);
void print_ahi_sample_type(uint16_t type);
static const char *op_type_names[4] = {
    "BYTE",
    "WORD",
    "LONGWORD",
    "MEM",
};
#define DEBUG printf
#define PRINT_AHI_DEBUGMSG print_ahi_debugmsg
#define PRINT_AHI_SAMPLE_TYPE print_ahi_sample_type
#else
#define PRINT_AHI_DEBUGMSG(...)
#define PRINT_AHI_SAMPLE_TYPE(...)
#define DEBUG(...)
void print_ahi_sample_type(uint16_t type);
#endif

uint32_t tmp, dir, buff_size;
uint32_t playback_rate = 48000;
int32_t channels = 2, seconds;

snd_pcm_t *pcm_handle;
snd_pcm_hw_params_t *params;
snd_pcm_uframes_t frames;

char dbgbuf[255] = "blep";
char dbgout[255] = "blep";

int loops;

extern struct emulator_config *cfg;
char *shitbuf;

uint32_t sndbuf_offset = 0, old_sndbuf_offset = 0, ahi_shutdown = 0, timing_enabled = 0, ahi_interrupt_triggered = 0, irq_disabled = 1;
uint32_t ahi_ints_triggered = 0, ahi_ints_handled = 0, ahi_ints_spurious = 0;

static struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

extern uint16_t emulated_irqs;
extern uint8_t emulated_ipl;
static const uint8_t IPL[14] = {1, 1, 1, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 6 };

static inline void emulate_irq(unsigned int irq) {
  emulated_irqs |= 1 << irq;
  uint8_t ipl = IPL[irq];

  if (emulated_ipl < ipl) {
    emulated_ipl = ipl;
  }
}

void *ahi_timing_task(void *args) {
    printf("[PI-AHI] AHI timing thread running.\n");

    struct timespec f1, f2;
    struct timespec c1, c2;
    int32_t timer_interval[2] = { 18000000, 20000000 };

    for (;;) {
        if (timing_enabled) {
            clock_gettime(CLOCK_REALTIME, &f1);
            do {
                usleep(0);
                clock_gettime(CLOCK_REALTIME, &f2);
            } while (diff(f1, f2).tv_nsec <= timer_interval[0]);

            while (diff(f1, f2).tv_nsec <= timer_interval[1]) {
                clock_gettime(CLOCK_REALTIME, &f2);
            }

            if (!irq_disabled) {
                emulate_irq(13);
                ahi_interrupt_triggered = 1;
                ahi_ints_triggered++;
            }

            clock_gettime(CLOCK_REALTIME, &c2);
            if (diff(c1, c2).tv_sec >= 1) {
                //printf("Triggered %d times in one second. (Handled: %d Spurious: %d)\n", ahi_ints_triggered, ahi_ints_handled, ahi_ints_spurious);
                ahi_ints_triggered = 0;
                ahi_ints_handled = 0;
                ahi_ints_spurious = 0;
                clock_gettime(CLOCK_REALTIME, &c1);
            }
        } else {
            clock_gettime(CLOCK_REALTIME, &c1);
            usleep(1);
        }

        if (ahi_shutdown) {
            ahi_shutdown = 0;
            break;
        }
  }
  printf("[PI-AHI] AHI timing thread shutting down.\n");
  return args;
}

void pi_ahi_set_playback_rate(uint32_t rate) {
    playback_rate = rate;
    printf("[PI-AHI] Playback sample rate set to %dHz.\n", playback_rate);
}

uint32_t pi_ahi_init(char *dev) {
    pthread_t ahi_tid = 0;
    int err;

    err = pthread_create(&ahi_tid, NULL, &ahi_timing_task, NULL);
    if (err != 0) {
        printf("[!!!PI-AHI] Could not start AHI timing thread: [%s]", strerror(err));
        goto pcm_init_fail;
    } else {
        pthread_setname_np(ahi_tid, "pistorm: ahi");
        printf("[PI-AHI] AHI timing thread started successfully.\n");
    }

    if (dev) {
        int32_t res = 0;

        res = snd_pcm_open(&pcm_handle, dev, SND_PCM_STREAM_PLAYBACK, 0);
        if (res < 0) {
            printf("[PI-AHI] Failed to open sound device %s for playback: %s\n", dev, snd_strerror(res));
            return 1;
        }

        snd_pcm_hw_params_malloc(&params);
        snd_pcm_hw_params_any(pcm_handle, params);

        res = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
        if (res  < 0) {
            printf("[PI-AHI] Failed to set interleaved mode: %s\n", snd_strerror(res));
            goto pcm_init_fail;
        }

        res = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_BE);
        if (res  < 0) {
            printf("[PI-AHI] Failed to set sound format: %s\n", snd_strerror(res));
            goto pcm_init_fail;
        }

        res = snd_pcm_hw_params_set_channels(pcm_handle, params, channels);
        if (res < 0) {
            printf("[PI-AHI] Failed to set 16 channels: %s\n", snd_strerror(res));
            goto pcm_init_fail;
        }

        res = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &playback_rate, 0);
        if (res  < 0) {
            printf("[PI-AHI] Failed to set sample rate: %s\n", snd_strerror(res));
            goto pcm_init_fail;
        }

        res = snd_pcm_hw_params(pcm_handle, params);
        if (res < 0) {
            printf("[PI-AHI] Failed to set PCM parameters: %s\n", snd_strerror(res));
            goto pcm_init_fail;
        }

        snd_pcm_hw_params_get_period_size(params, &frames, 0);
        buff_size = frames * channels * 2;
        shitbuf = malloc(256 * SIZE_KILO);

        snd_pcm_prepare(pcm_handle);

        printf("[PI-AHI] PCM name: %s\n", snd_pcm_name(pcm_handle));
        printf("[PI-AHI] PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(pcm_handle)));
        snd_pcm_hw_params_get_channels(params, &tmp);
        printf("[PI-AHI] Channels: %i\n", tmp);
        printf("[PI-AHI] Buffer size: %i\n", buff_size);

        goto pcm_init_done;

pcm_init_fail:
        snd_pcm_close(pcm_handle); return 1;
pcm_init_done:

        printf("[PI-AHI] Some Pi-AHI enabled: %s\n", dev);
        return 0;
    }
    else
        return 1;
}

void pi_ahi_shutdown() {
    printf("[PI-AHI] Shutting down Pi-AHI.\n");
    if (pcm_handle) {
        snd_pcm_drop(pcm_handle);
        snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
        if (shitbuf) {
            free(shitbuf);
            shitbuf = NULL;
        }
    }
    ahi_shutdown = 1;
    printf("[PI-AHI] Pi-AHI shut down.\n");
}

uint32_t dbg_repeat;

void pi_ahi_do_cmd(uint32_t val) {
    switch (val) {
        case AHI_CMD_START:
            //timing_enabled = 1;
            //irq_disabled = 0;
            printf("[PI-AHI] Driver sent START command.\n");
            break;
        case AHI_CMD_STOP:
            printf("[PI-AHI] Driver sent STOP command.\n");
            // fallthrough
        case AHI_CMD_FREEAUDIO:
            //irq_disabled = 1;
            //timing_enabled = 0;
            snd_pcm_drop(pcm_handle);
            snd_pcm_prepare(pcm_handle);
            break;
        case AHI_CMD_RATE: {
            //printf("[PI-AHI] Set rate to %d.\n", ahi_u32[0]);
            break;
        }
        case AHI_CMD_PLAY: {
            int32_t res = 0;
            uint8_t *bufptr = get_mapped_data_pointer_by_address(cfg, ahi_addr[0]);
            uint16_t *bepptr = (uint16_t *)get_mapped_data_pointer_by_address(cfg, ahi_addr[0]);
            //printf("[PI-AHI] Driver sent PLAY command: %d samples @$%.8X ($%.8X).\n", ahi_u32[0], ahi_addr[0], (uint32_t)bufptr);
            if (ahi_u32[0] != 0 && ahi_addr[0] != 0) {
                uint32_t bsize = ahi_u32[0] * get_ahi_sample_size(ahi_u32[3]) * get_ahi_channels(ahi_u32[3]);
                uint32_t hz = (ahi_u32[0] * (ahi_addr[1] >> 16));
                sprintf(dbgbuf, "Samples: %d Rate(?): %dHz MixFreq: %d PlayFreq: %d Channels: %d Bsize: %d Type: %d", ahi_u32[0], hz, ahi_u32[1], (ahi_addr[1] >> 16), ahi_u32[2], bsize, ahi_u32[3]);
                if (strcmp(dbgbuf, dbgout) == 0) {
                    dbg_repeat++;
                } else {
                    printf("%s", dbgbuf);
                    if (dbg_repeat) {
                        //printf (" [line repeated %d times]", dbg_repeat);
                        dbg_repeat = 0;
                    }
                    //printf("\n");
                    strcpy(dbgout, dbgbuf);
                }
                //printf("Buffer: %.4X %.4X %.4X %.4X\n", bepptr[0], bepptr[1], bepptr[2], bepptr[3]);
                if (ahi_u32[1] == playback_rate) {
                    memcpy(shitbuf + sndbuf_offset, bufptr, bsize);
                }                    
                else {
                    uint32_t dst_bsize = 0;
                    if ((ahi_addr[1] >> 16) == 0) {
                        dst_bsize = playback_rate / 50;
                    } else {
                        dst_bsize = playback_rate / (ahi_addr[1] >> 16);
                    }
                    dst_bsize *= get_ahi_sample_size(ahi_u32[3]) * get_ahi_channels(ahi_u32[3]);
                    //printf("Resampling from %d to %d (%d bytes to %d bytes).\n", ahi_u32[1], playback_rate, bsize, dst_bsize);
                    if (get_ahi_channels(ahi_u32[3]) == 2) {
                        uint32_t *u32ptr = (uint32_t *)bufptr;
                        uint32_t *dstptr = (uint32_t *)&shitbuf[sndbuf_offset];
                        float step = (float)bsize / (float)dst_bsize;
                        float index_f = 0.0f;
                        for (uint32_t i = 0; i < (dst_bsize / 4); i++) {
                            uint32_t index = (uint32_t)index_f;
                            index_f += step;
                            dstptr[i] = u32ptr[index];
                        }
                        bsize = dst_bsize;
                    } else {
                        uint16_t *u16ptr = (uint16_t *)bufptr;
                    }
                }
                sndbuf_offset += bsize;
                while (sndbuf_offset >= old_sndbuf_offset + buff_size) {
                    //printf("Writing %d bytes to the PCM...\n", buff_size);
                    res = snd_pcm_writei(pcm_handle, shitbuf + old_sndbuf_offset, frames);
                    old_sndbuf_offset = old_sndbuf_offset + buff_size;
                    if (old_sndbuf_offset >= 127 * SIZE_KILO) {
                        //printf("Buffer wrap.\n");
                        memcpy(shitbuf, bufptr + (sndbuf_offset % buff_size), bsize - (sndbuf_offset % buff_size));
                        sndbuf_offset = (sndbuf_offset % buff_size);
                        old_sndbuf_offset = (sndbuf_offset - (sndbuf_offset % buff_size));
                    }
                    if (res == -EPIPE) {
                        //printf("PCM epipe.\n");
                        snd_pcm_prepare(pcm_handle);
                    } else if (res < 0) {
                        printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(res));
                        snd_pcm_prepare(pcm_handle);
                    }
                    //snd_pcm_hw_params(pcm_handle, params);
                    //printf("[PI-AHI] PCM name: %s\n", snd_pcm_name(pcm_handle));
                    //printf("[PI-AHI] PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(pcm_handle)));
                }
            }
            break;
        }
        case AHI_CMD_ALLOCAUDIO: {
            printf("[PI-AHI] AllocAudio:\n");
            printf("MixFreq: %d PlayFreq: %d Channels: %d ", ahi_u32[1], (ahi_u32[0] >> 16), ahi_u32[2]);
            printf("MinPlayFreq: %d MaxPlayFreq: %d\n", (ahi_addr[0] >> 16), (ahi_addr[1] >> 16));
            sprintf(dbgout, "blep");
            dbg_repeat = 0;
            break;
        }
        default:
            printf("[!!!PI_AHI] Unknown command %d.\n", val);
            break;
    }
}

void handle_pi_ahi_write(uint32_t addr_, uint32_t val, uint8_t type) {
    uint32_t addr = addr_ & 0xFFFF;
#ifndef DEBUG_AHI
    if (type) {};
#endif

    switch(addr) {
        case AHI_COMMAND:
            pi_ahi_do_cmd(val);
            break;
        case AHI_U81: case AHI_U82: case AHI_U83: case AHI_U84: {
            int i = (addr - AHI_U81);
            ahi_u8[i] = val;
            break;
        }
        case AHI_U1: case AHI_U2: case AHI_U3: case AHI_U4: case AHI_U5: case AHI_U6: {
            int i = (addr - AHI_U1) / 2;
            ahi_u16[i] = val;
            break;
        }
        case AHI_ADDR1: case AHI_ADDR2: case AHI_ADDR3: case AHI_ADDR4: {
            int i = (addr - AHI_ADDR1) / 4;
            ahi_addr[i] = val;
            break;
        }
        case AHI_U321: case AHI_U322: case AHI_U323: case AHI_U324: {
            int i = (addr - AHI_U321) / 4;
            ahi_u32[i] = val;
            break;
        }
        case AHI_DEBUGMSG:
            PRINT_AHI_DEBUGMSG(val, type);
            break;
        case AHI_DISABLE:
            if (val == 1) {
                irq_disabled++;
            } else {
                if(irq_disabled > 0) {
                    irq_disabled--;
                }
            }
            break;
        case AHI_INTCHK:
            switch(val) {
                case 1:
                    amiga_clear_emulating_irq();
                    DEBUG("Interrupt handler triggered. IRQ enabled: %d\n", irq_enabled);
                    break;
                case 2:
                    ahi_ints_handled++;
                    DEBUG("Sending play signal.\n");
                    break;
                case 3:
                    ahi_ints_spurious++;
                    break;
            }
            break;
        default:
            DEBUG("[PI-AHI] %s write to %.4X (%.8X): %.8X (%d)\n", op_type_names[type], addr, addr_, val, val);
            break;
    }
}

uint32_t handle_pi_ahi_read(uint32_t addr_, uint8_t type) {
    uint32_t addr = addr_ & 0xFFFF;
#ifndef DEBUG_AHI
    if (type) {};
#endif

    switch(addr) {
        case AHI_U81: case AHI_U82: case AHI_U83: case AHI_U84: {
            int i = (addr - AHI_U81);
            return ahi_u8[i];
            break;
        }
        case AHI_U1: case AHI_U2: case AHI_U3: case AHI_U4: case AHI_U5: case AHI_U6: {
            int i = (addr - AHI_U1) / 2;
            return ahi_u16[i];
            break;
        }
        case AHI_ADDR1: case AHI_ADDR2: case AHI_ADDR3: case AHI_ADDR4: {
            int i = (addr - AHI_ADDR1) / 4;
            return ahi_addr[i];
            break;
        }
        case AHI_U321: case AHI_U322: case AHI_U323: case AHI_U324: {
            int i = (addr - AHI_U321) / 4;
            return ahi_u32[i];
            break;
        }
        case AHI_INTCHK:
            if (ahi_interrupt_triggered) {
                ahi_interrupt_triggered = 0;
                return 1;
            }
            break;
        default:
            DEBUG("[PI-AHI] %s read from %.4X (%.8X)?!\n", op_type_names[type], addr, addr_);
            break;
    }

    return 0;
}

void print_ahi_sample_type(uint16_t type) {
    switch(type) {
        case AHIST_M8S: printf("8-bit signed [Mono]"); break;
        case AHIST_S8S: printf("8-bit signed [Stereo]"); break;
        case AHIST_M16S: printf("16-bit signed [Mono]"); break;
        case AHIST_S16S: printf("16-bit signed [Stereo]"); break;
        case AHIST_M32S: printf("32-bit signed [Mono]"); break;
        case AHIST_S32S: printf("32-bit signed [Stereo]"); break;
        default: printf("UNKNOWN FORMAT (%d)", type);
    }
}

int get_ahi_sample_size(uint16_t type) {
    switch(type) {
        case AHIST_M8S:
        case AHIST_S8S: return 1;
        case AHIST_M16S: 
        case AHIST_S16S: return 2;
        case AHIST_M32S: 
        case AHIST_S32S: return 4;
        default: return 2;
    }
}

int get_ahi_channels(uint16_t type) {
    switch(type) {
        case AHIST_M8S:
        case AHIST_M16S: 
        case AHIST_M32S: return 1;
        case AHIST_S8S:
        case AHIST_S16S:
        case AHIST_S32S: return 2;
        default: return 1;
    }
}

#ifdef AHI_DEBUG
void print_ahi_debugmsg(int val, int type) {
    switch (val) {
        case 1: printf("[PI-AHI-amiga] Called INIT. Dev: %.8X\n", ahi_u32[0]); break;
        case 101: printf("[PI-AHI-amiga] !!!INIT: AHISubBase already set.\n"); break;
        case 102: printf("[PI-AHI-amiga] Allocating driver base.\n"); break;
        case 103: printf("[PI-AHI-amiga] Opening libraries.\n"); break;
        case 104: printf("[PI-AHI-amiga] Init done.\n"); break;

        case 2: printf("[PI-AHI-amiga] Called OPEN.\n"); break;
        case 201: printf("[PI-AHI-amiga] OPEN: Set AHISubBase.\n"); break;

        case 3: printf("[PI-AHI-amiga] Called CLOSE.\n"); break;
        case 4: printf("[PI-AHI-amiga] Called BEGINIO (but how!?).\n"); break;
        case 5: printf("[PI-AHI-amiga] Called ABORTIO (but how!?).\n"); break;
        case 6: printf("[PI-AHI-amiga] Called ALLOCAUDIO.\n"); break;
        case 601: printf("[PI-AHI-amiga] Allocating %d bytes for DriverData.\n", ahi_u32[0]); break;
        case 602: printf("[PI-AHI-amiga] Setting mixing frequency to %d.\n", ahi_u16[0]); break;
        case 603: printf("[PI-AHI-amiga] Setting up crap for the slave process.\n"); break;
        case 604: printf("[PI-AHI-amiga] Find tasks and stuff.\n"); break;
        case 605: printf("[PI-AHI-amiga] Forbidding and creating slace process. ahi-data = %.8X\n", ahi_u32[0]); break;
        case 606: printf("[PI-AHI-amiga] Slave process created.\n"); break;
        case 607: printf("[PI-AHI-amiga] Permitting and moving on.\n"); break;
        case 608: printf("[PI-AHI-amiga] Setting some crap for the slave process.\n"); break;
        case 609: printf("[PI-AHI-amiga] Setting owner flags.\n"); break;
        case 610: printf("[PI-AHI-amiga] Done with slave process stuff, resuming.\n"); break;

        case 7: printf("[PI-AHI-amiga] Called FREEAUDIO.\n"); break;
        case 8: printf("[PI-AHI-amiga] Called START.\n"); break;
        case 81: printf("[PI-AHI-amiga] Buffer type: ");
            PRINT_AHI_SAMPLE_TYPE(ahi_u16[0]);
            printf(" Buffer size: %d\n", ahi_u32[0]);
            break;
        case 82: printf("[!!!PI-AHI-amiga] Out of memory allocating soft interrupts!\n"); break;

        case 9: printf("[PI-AHI-amiga] Called STOP.\n"); break;
        case 91: printf("[PI-AHI-amiga] Free Play SoftInt.\n"); break;
        case 92: printf("[PI-AHI-amiga] Free Mix SoftInt.\n"); break;
        case 93: printf("[PI-AHI-amiga] Free buffers.\n"); break;
        case 94: printf("[PI-AHI-amiga] Free loop %d...\n", ahi_u32[0] + 1); break;

        case 10: printf("[PI-AHI-amiga] Called GETATTR.\n"); break;
        case 1001: printf("[PI-AHI-amiga] Attr: %.8X Arg: %.8X Def: %.8X.\n", ahi_u32[0], ahi_u32[1], ahi_u32[2]); break;
        case 1002: printf("[PI-AHI-amiga] Query Bits.\n"); break;
        case 1003: printf("[PI-AHI-amiga] Query NumFrequencies.\n"); break;
        case 1004: printf("[PI-AHI-amiga] Get Frequency (index).\n"); break;
        case 1005: printf("[PI-AHI-amiga] Index Frequency.\n"); break;
        case 1006: printf("[PI-AHI-amiga] Query Author.\n"); break;
        case 1007: printf("[PI-AHI-amiga] Query Copyright.\n"); break;
        case 1008: printf("[PI-AHI-amiga] Query Version.\n"); break;
        case 1009: printf("[PI-AHI-amiga] Query Annotation.\n"); break;
        case 1010: printf("[PI-AHI-amiga] Query Recording capability.\n"); break;
        case 1011: printf("[PI-AHI-amiga] Query Full Duplex.\n"); break;
        case 1012: printf("[PI-AHI-amiga] Query Realtime.\n"); break;
        case 1013: printf("[PI-AHI-amiga] Query MaxChannels.\n"); break;
        case 1014: printf("[PI-AHI-amiga] Query MaxPlaySamples.\n"); break;
        case 1015: printf("[PI-AHI-amiga] Query MaxRecordSamples.\n"); break;
        case 1016: printf("[PI-AHI-amiga] Query MinMonitorVolume.\n"); break;
        case 1017: printf("[PI-AHI-amiga] Query MaxMonitorVolume.\n"); break;
        case 1018: printf("[PI-AHI-amiga] Query MinInputGain.\n"); break;
        case 1019: printf("[PI-AHI-amiga] Query MaxInputGain.\n"); break;
        case 1020: printf("[PI-AHI-amiga] Query MinOutputVolume.\n"); break;
        case 1021: printf("[PI-AHI-amiga] Query MaxOutputVolume.\n"); break;
        case 1022: printf("[PI-AHI-amiga] Query Inputs.\n"); break;
        case 1023: printf("[PI-AHI-amiga] Query Input (index).\n"); break;
        case 1024: printf("[PI-AHI-amiga] Query Outputs.\n"); break;
        case 1025: printf("[PI-AHI-amiga] Query Output (index).\n"); break;
        case 1099: printf("[!!!PI-AHI-amiga] GETATTR: Hit default case!\n"); break;

        case 11: printf("[PI-AHI-amiga] Called HARDWARECONTROL.\n"); break;
        case 1101: printf("[PI-AHI-amiga] Set MonitorVolume.\n"); break;
        case 1102: printf("[PI-AHI-amiga] Query MonitorVolume.\n"); break;
        case 1103: printf("[PI-AHI-amiga] Set InputGain.\n"); break;
        case 1104: printf("[PI-AHI-amiga] Query InputGain.\n"); break;
        case 1105: printf("[PI-AHI-amiga] Set OutputVolume.\n"); break;
        case 1106: printf("[PI-AHI-amiga] Query OutputVolume.\n"); break;
        case 1107: printf("[PI-AHI-amiga] Set Input.\n"); break;
        case 1108: printf("[PI-AHI-amiga] Query Input.\n"); break;
        case 1109: printf("[PI-AHI-amiga] Set Output.\n"); break;
        case 1110: printf("[PI-AHI-amiga] Query Output.\n"); break;
        case 1199: printf("[!!!PI-AHI-amiga] HARDWARECONTROL: Hit default case!\n"); break;

        case 12: printf("[PI-AHI-amiga] Called SETEFFECT.\n"); break;

        case 13: printf("[PI-AHI-amiga] Called LOADSOUND.\n"); break;
        case 131: printf("[PI-AHI-amiga] Sound: %d Type: ", ahi_u16[0]);
            switch (ahi_u32[0]) {
                case AHIST_SAMPLE: printf("Sample"); break;
                case AHIST_DYNAMICSAMPLE: printf("Dynamic Sample"); break;
                case AHIST_INPUT: printf("Input!?"); break;
                default: printf("UNKNOWN TYPE %d", ahi_u32[0]); break;
            }
            printf(" Info: %.8X\n", ahi_u32[1]);
            break;
        case 132: printf("[PI-AHI-amiga] Called UNLOADSOUND.\n"); break;
        case 133: printf("[PI-AHI-amiga] Sound: %d\n", ahi_u16[0]); break;
        case 134: printf("[PI-AHI-amiga] Buffer type: ");
            PRINT_AHI_SAMPLE_TYPE(ahi_u32[0]);
            printf(" Address: %.8X Length: %d\n", ahi_u32[1], ahi_u32[2]);
            break;

        case 14: printf("[PI-AHI-amiga] Called ENABLE.\n"); break;
        case 141: printf("[PI-AHI-amiga] Enable()\n"); break;

        case 15: printf("[PI-AHI-amiga] Called DISABLE.\n"); break;
        case 151: printf("[PI-AHI-amiga] Disable()\n"); break;

        case 16: printf("[PI-AHI-amiga] Called UPDATE.\n"); break;

        case 17: printf("[PI-AHI-amiga] Called SETVOL.\n"); break;
        case 171:
            printf("[PI-AHI-amiga] Channel: %d Volume: %d Pan: %d Flags: %.8X\n", ahi_u16[0], ahi_u32[0], ahi_u32[1], ahi_u32[2]);
            break;

        case 18: printf("[PI-AHI-amiga] Called SETFREQ.\n"); break;
        case 181:
            printf("[PI-AHI-amiga] Channel: %d Freq: %d Flags: %.8X\n", ahi_u16[0], ahi_u32[0], ahi_u32[2]);
            break;

        case 19: printf("[PI-AHI-amiga] Called SETSOUND.\n"); break;
        case 191:
            printf("[PI-AHI-amiga] Channel: %d Sound: %d Offset: %d Length: %d Flags: %.8X\n", ahi_u16[0], ahi_u32[0], ahi_u32[1], ahi_u32[2], ahi_u32[3]);
            break;
        case 192:
            printf("[PI-AHI-amiga] SETSOUND: ahi-data = %.8X\n", ahi_u32[0]);
            printf("[PI-AHI-amiga] PlayerFunc: %.8X MixerFunc: %.8X PreTimer: %.8X\n", ahi_u32[1], ahi_u32[2], ahi_u32[3]);
            break;
        
        case 20:
            printf("[PI-AHI-amiga] We're in the slave process! ahi-data = %.8X\n", ahi_u32[0]);
            printf("[PI-AHI-amiga] PlayerFunc: %.8X MixerFunc: %.8X PreTimer: %.8X\n", ahi_u32[1], ahi_u32[2], ahi_u32[3]);
            break;
        case 2001: printf("[PI-AHI-amiga] SLAVE:PlaySignal. BuffSamples: %d\n", ahi_u32[0]); break;
        case 2002: printf("[PI-AHI-amiga] SLAVE:MixSignal\n"); break;
        case 2004: printf("[PI-AHI-amiga] SLAVE:Waiting for signal\n"); break;
        case 2005: printf("[PI-AHI-amiga] SLAVE:PlaySignal End\n"); break;
        case 2006: printf("[PI-AHI-amiga] SLAVE:Failed to allocate signals?\n"); break;

        case 21: printf("[PI-AHI-amiga] Called PLAYFUNC.\n"); break;
        case 22: printf("[PI-AHI-amiga] Called MIXFUNC.\n"); break;

        // Debug messages of dubious usefulness
        case 998: printf("[!!!PI-AHI-amiga] NULL function called!\n"); break;
        case 999: printf("[!!!PI-AHI-amiga] EXPUNGE called!\n"); break;
        default:
            printf("[PI-AHI] %s write to DEBUGMSG: %.8X (%d)\n", op_type_names[type], val, val);
            break;
    }
}
#endif
