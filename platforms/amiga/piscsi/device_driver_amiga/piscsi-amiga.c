/*
 * Based on:
 * Amiga ZZ9000 USB Storage Driver (ZZ9000USBStorage.device)
 * Copyright (C) 2016-2020, Lukas F. Hartmann <lukas@mntre.com>
 * Based on code Copyright (C) 2016, Jason S. McMullan <jason.mcmullan@gmail.com>
 * All rights reserved.
 *
 * Licensed under the MIT License:
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/alerts.h>
#include <exec/tasks.h>
#include <exec/io.h>
#include <exec/execbase.h>

#include <libraries/expansion.h>

#include <devices/trackdisk.h>
#include <devices/timer.h>
#include <devices/scsidisk.h>

#include <dos/filehandler.h>

#include <proto/exec.h>
#include <proto/disk.h>
#include <proto/expansion.h>

#include <clib/debug_protos.h>
#include <stdint.h>
#include <stdlib.h>
#include "../piscsi-enums.h"

#define WRITESHORT(cmd, val) *(unsigned short *)((unsigned long)(PISCSI_OFFSET+cmd)) = val;
#define WRITELONG(cmd, val) *(unsigned long *)((unsigned long)(PISCSI_OFFSET+cmd)) = val;
#define WRITEBYTE(cmd, val) *(unsigned char *)((unsigned long)(PISCSI_OFFSET+cmd)) = val;

#define READSHORT(cmd, var) var = *(volatile unsigned short *)(PISCSI_OFFSET + cmd);
#define READLONG(cmd, var) var = *(volatile unsigned long *)(PISCSI_OFFSET + cmd);

#pragma pack(4)
struct piscsi_base {
    struct Device* pi_dev;
    struct piscsi_unit {
        struct Unit unit;
        uint32_t regs_ptr;

        uint8_t enabled;
        uint8_t present;
        uint8_t valid;
        uint8_t read_only;
        uint8_t motor;
        uint8_t unit_num;
        uint16_t h, s;
        uint32_t c;

        uint32_t change_num;
    } units[NUM_UNITS];
};

struct ExecBase* SysBase = NULL;

#ifdef _FSCSIDEV
const char DevName[] = "scsi.device";
#elif _FSCSI2ND
const char DevName[] = "2nd.scsi.device";
#else
const char DevName[] = "pi-scsi.device";
#endif
const char DevIdString[] = "Pi-SCSI 0.1";

const UWORD DevVersion = 43;
const UWORD DevRevision = 10;

#include "stabs.h"

struct piscsi_base *dev_base = NULL;

struct WBStartup *_WBenchMsg = NULL;

//#define exit(...)
//#define debug(...)
#define KPrintF(...)

//#define bug(x,args...) KPrintF(x ,##args);
//#define debug(x,args...) bug("%s:%ld " x "\n", __func__, (unsigned long)__LINE__ ,##args)

uint8_t piscsi_perform_io(struct piscsi_unit *u, struct IORequest *io);
uint8_t piscsi_rw(struct piscsi_unit *u, struct IORequest *io, uint32_t offset, uint8_t write);
uint8_t piscsi_scsi(struct piscsi_unit *u, struct IORequest *io);

extern void* DOSBase[2];

uint32_t __UserDevInit(struct Device* dev) {
    //uint8_t* registers = NULL;
    SysBase = *(struct ExecBase **)4L;

    KPrintF("Initializing devices.\n");

    dev_base = AllocMem(sizeof(struct piscsi_base), MEMF_PUBLIC | MEMF_CLEAR);
    dev_base->pi_dev = dev;

    for (int i = 0; i < NUM_UNITS; i++) {
        uint16_t r = 0;
        WRITESHORT(PISCSI_CMD_DRVNUM, i);
        dev_base->units[i].regs_ptr = PISCSI_OFFSET;
        READSHORT(PISCSI_CMD_DRVTYPE, r);
        dev_base->units[i].enabled = r;
        dev_base->units[i].present = r;
        dev_base->units[i].valid = r;
        dev_base->units[i].unit_num = i;
        if (dev_base->units[i].present) {
            READLONG(PISCSI_CMD_CYLS, dev_base->units[i].c);
            READSHORT(PISCSI_CMD_HEADS, dev_base->units[i].h);
            READSHORT(PISCSI_CMD_SECS, dev_base->units[i].s);
            KPrintF("C/H/S: %ld / %ld / %ld\n", dev_base->units[i].c, dev_base->units[i].h, dev_base->units[i].s);
        }
        dev_base->units[i].change_num++;
        // Send any reset signal to the "SCSI" device here.
    }

    return 1;
}

uint32_t __UserDevCleanup(void) {
    KPrintF("Cleaning up.\n");
    FreeMem(dev_base, sizeof(struct piscsi_base));
    return 0;
}

uint32_t __UserDevOpen(struct IOExtTD *iotd, uint32_t num, uint32_t flags) {
    struct Node* node = (struct Node*)iotd;
    int io_err = IOERR_OPENFAIL;

    //WRITESHORT(PISCSI_CMD_DEBUGME, 1);

    int unit_num = 0;
    WRITELONG(PISCSI_CMD_DRVNUM, num);
    READLONG(PISCSI_CMD_DRVNUM, unit_num);

    KPrintF("Opening device %ld Flags: %ld (%lx)\n", unit_num, flags, flags);

    if (iotd && unit_num < NUM_UNITS) {
        if (dev_base->units[unit_num].enabled && dev_base->units[unit_num].present) {
            io_err = 0;
            iotd->iotd_Req.io_Unit = (struct Unit*)&dev_base->units[unit_num].unit;
            iotd->iotd_Req.io_Unit->unit_flags = UNITF_ACTIVE;
            iotd->iotd_Req.io_Unit->unit_OpenCnt = 1;
        }
    }

skip:;
    iotd->iotd_Req.io_Error = io_err;

    return io_err;
}

uint32_t __UserDevClose(struct IOExtTD *iotd) {
  return 0;
}

void exit(int status) { }

ADDTABL_1(__BeginIO,a1);
void __BeginIO(struct IORequest *io) {
    if (dev_base == NULL || io == NULL)
        return;
    
    struct piscsi_unit *u;
    struct Node* node = (struct Node*)io;
    u = (struct piscsi_unit *)io->io_Unit;

    if (node == NULL || u == NULL)
        return;

    KPrintF("io_Command = %ld, io_Flags = 0x%lx quick = %lx\n", io->io_Command, io->io_Flags, (io->io_Flags & IOF_QUICK));
    io->io_Error = piscsi_perform_io(u, io);

    if (!(io->io_Flags & IOF_QUICK)) {
        ReplyMsg(&io->io_Message);
    }
}

ADDTABL_1(__AbortIO,a1);
void __AbortIO(struct IORequest* io) {
    KPrintF("AbortIO!\n");
    if (!io) return;
    io->io_Error = IOERR_ABORTED;
}

uint8_t piscsi_rw(struct piscsi_unit *u, struct IORequest *io, uint32_t offset, uint8_t write) {
    struct IOStdReq *iostd = (struct IOStdReq *)io;
    struct IOExtTD *iotd = (struct IOExtTD *)io;

    uint8_t* data;
    uint32_t len, num_blocks;
    uint32_t block, max_addr;
    uint8_t sderr;

    data = iotd->iotd_Req.io_Data;
    len = iotd->iotd_Req.io_Length;
    //uint32_t offset2 = iostd->io_Offset;

    max_addr = 0xffffffff;

    // well... if we had 64 bits this would make sense
    if ((offset > max_addr) || (offset+len > max_addr))
        return IOERR_BADADDRESS;
    if (data == 0)
        return IOERR_BADADDRESS;
    if (len < PISCSI_BLOCK_SIZE) {
        iostd->io_Actual = 0;
        return IOERR_BADLENGTH;
    }

    //block = offset;// >> SD_SECTOR_SHIFT;
    //num_blocks = len;// >> SD_SECTOR_SHIFT;
    sderr = 0;

    if (write) {
        //uint32_t retries = 10;
        //KPrintF("Write %lx -> %lx %lx\n", (uint32_t)data, offset, len);
        WRITELONG(PISCSI_CMD_ADDR1, (offset >> 9));
        WRITELONG(PISCSI_CMD_ADDR2, len);
        WRITELONG(PISCSI_CMD_ADDR3, (uint32_t)data);
        WRITESHORT(PISCSI_CMD_WRITE, 1);
    } else {
        //KPrintF("read %lx %lx -> %lx\n", offset, len, (uint32_t)data);
        WRITELONG(PISCSI_CMD_ADDR1, (offset >> 9));
        WRITELONG(PISCSI_CMD_ADDR2, len);
        WRITELONG(PISCSI_CMD_ADDR3, (uint32_t)data);
        WRITESHORT(PISCSI_CMD_READ, 1);
    }

    if (sderr) {
        iostd->io_Actual = 0;

        if (sderr & SCSIERR_TIMEOUT)
            return TDERR_DiskChanged;
        if (sderr & SCSIERR_PARAM)
            return TDERR_SeekError;
        if (sderr & SCSIERR_ADDRESS)
            return TDERR_SeekError;
        if (sderr & (SCSIERR_ERASESEQ | SCSIERR_ERASERES))
            return TDERR_BadSecPreamble;
        if (sderr & SCSIERR_CRC)
            return TDERR_BadSecSum;
        if (sderr & SCSIERR_ILLEGAL)
            return TDERR_TooFewSecs;
        if (sderr & SCSIERR_IDLE)
            return TDERR_PostReset;

        return TDERR_SeekError;
    } else {
        iostd->io_Actual = len;
    }

    return 0;
}

#define PISCSI_ID_STRING "PISTORM Fake SCSI Disk  0.1 1111111111111111"

uint8_t piscsi_scsi(struct piscsi_unit *u, struct IORequest *io)
{
    struct IOStdReq *iostd = (struct IOStdReq *)io;
    struct SCSICmd *scsi = iostd->io_Data;
    //uint8_t* registers = sdu->sdu_Registers;
    uint8_t *data = (uint8_t *)scsi->scsi_Data;
    uint32_t i, block, blocks, maxblocks;
    uint8_t err = 0;

    KPrintF("SCSI len=%ld, cmd = %02lx %02lx %02lx ... (%ld)\n",
        iostd->io_Length, scsi->scsi_Command[0],
        scsi->scsi_Command[1], scsi->scsi_Command[2],
        scsi->scsi_CmdLength);

    //maxblocks = u->s * u->c * u->h;

    if (scsi->scsi_CmdLength < 6) {
        //KPrintF("SCSICMD BADLENGTH2");
        return IOERR_BADLENGTH;
    }

    if (scsi->scsi_Command == NULL) {
        //KPrintF("SCSICMD IOERR_BADADDRESS1");
        return IOERR_BADADDRESS;
    }

    scsi->scsi_Actual = 0;
    //iostd->io_Actual = sizeof(*scsi);

    switch (scsi->scsi_Command[0]) {
        case 0x00:      // TEST_UNIT_READY
            KPrintF("SCSI command: Test Unit Ready.\n");
            err = 0;
            break;
        
        case 0x12:      // INQUIRY
            KPrintF("SCSI command: Inquiry.\n");
            for (i = 0; i < scsi->scsi_Length; i++) {
                uint8_t val = 0;

                switch (i) {
                    case 0: // SCSI device type: direct-access device
                        val = (0 << 5) | 0;
                        break;
                    case 1: // RMB = 1
                        val = (1 << 7);
                        break;
                    case 2: // VERSION = 0
                        val = 0;
                        break;
                    case 3: // NORMACA=0, HISUP = 0, RESPONSE_DATA_FORMAT = 2
                        val = (0 << 5) | (0 << 4) | 2;
                        break;
                    case 4: // ADDITIONAL_LENGTH = 44 - 4
                        val = 44 - 4;
                        break;
                    default:
                        if (i >= 8 && i < 44)
                            val = PISCSI_ID_STRING[i - 8];
                        else
                            val = 0;
                        break;
                }
                data[i] = val;
            }
            scsi->scsi_Actual = i;
            err = 0;
            break;
        
        case 0x08: // READ (6)
        case 0x0a: // WRITE (6)
            block = scsi->scsi_Command[1] & 0x1f;
            block = (block << 8) | scsi->scsi_Command[2];
            block = (block << 8) | scsi->scsi_Command[3];
            blocks = scsi->scsi_Command[4];

            READLONG(PISCSI_CMD_BLOCKS, maxblocks);
            if (block + blocks > maxblocks) {
                err = IOERR_BADADDRESS;
                break;
            }
            /*if (scsi->scsi_Length < (blocks << SD_SECTOR_SHIFT)) {
                err = IOERR_BADLENGTH;
                break;
            }*/
            if (data == NULL) {
                err = IOERR_BADADDRESS;
                break;
            }

            if (scsi->scsi_Command[0] == 0x08) {
                //KPrintF("scsi_read %lx %lx\n",block,blocks);
                KPrintF("SCSI read %lx %lx -> %lx\n", block, blocks, (uint32_t)data);
                WRITELONG(PISCSI_CMD_ADDR2, block);
                WRITELONG(PISCSI_CMD_ADDR2, (blocks << 9));
                WRITELONG(PISCSI_CMD_ADDR3, (uint32_t)data);
                WRITESHORT(PISCSI_CMD_READ, 1);
            }
            else {
                //KPrintF("scsi_write %lx %lx\n",block,blocks);
                KPrintF("SCSI write %lx -> %lx %lx\n", (uint32_t)data, block, blocks);
                WRITELONG(PISCSI_CMD_ADDR2, block);
                WRITELONG(PISCSI_CMD_ADDR2, (blocks << 9));
                WRITELONG(PISCSI_CMD_ADDR3, (uint32_t)data);
                WRITESHORT(PISCSI_CMD_WRITE, 1);
            }

            scsi->scsi_Actual = scsi->scsi_Length;
            err = 0;
            break;
        
        case 0x25: // READ CAPACITY (10)
            KPrintF("SCSI command: Read Capacity.\n");
            if (scsi->scsi_CmdLength < 10) {
                err = HFERR_BadStatus;
                break;
            }

            block = *((uint32_t*)&scsi->scsi_Command[2]);

            /*if ((scsi->scsi_Command[8] & 1) || block != 0) {
                // PMI Not supported
                KPrintF("PMI not supported.\n");
                err = HFERR_BadStatus;
                break;
            }*/

            if (scsi->scsi_Length < 8) {
                err = IOERR_BADLENGTH;
                break;
            }

            READLONG(PISCSI_CMD_BLOCKS, blocks);
            ((uint32_t*)data)[0] = blocks - 1;
            ((uint32_t*)data)[1] = PISCSI_BLOCK_SIZE;

            scsi->scsi_Actual = 8;
            err = 0;

            break;
        case 0x1a: // MODE SENSE (6)    
            KPrintF("SCSI command: Mode Sense.\n");
            data[0] = 3 + 8 + 0x16;
            data[1] = 0; // MEDIUM TYPE
            data[2] = 0;
            data[3] = 8;

            READLONG(PISCSI_CMD_BLOCKS, maxblocks);
            (blocks = (maxblocks - 1) & 0xFFFFFF);

            *((uint32_t *)&data[4]) = blocks;
            *((uint32_t *)&data[8]) = PISCSI_BLOCK_SIZE;

            switch (((UWORD)scsi->scsi_Command[2] << 8) | scsi->scsi_Command[3]) {
                case 0x0300: { // Format Device Mode
                    KPrintF("Grabbing SCSI format device mode data.\n");
                    uint8_t *datext = data + 12;
                    datext[0] = 0x03;
                    datext[1] = 0x16;
                    datext[2] = 0x00;
                    datext[3] = 0x01;
                    *((uint32_t *)&datext[4]) = 0;
                    *((uint32_t *)&datext[8]) = 0;
                    *((uint16_t *)&datext[10]) = u->s;
                    *((uint16_t *)&datext[12]) = PISCSI_BLOCK_SIZE;
                    datext[14] = 0x00;
                    datext[15] = 0x01;
                    *((uint32_t *)&datext[16]) = 0;
                    datext[20] = 0x80;

                    scsi->scsi_Actual = data[0] + 1;
                    err = 0;
                    break;
                }
                case 0x0400: // Rigid Drive Geometry
                    KPrintF("Grabbing SCSI rigid drive geometry.\n");
                    uint8_t *datext = data + 12;
                    datext[0] = 0x04;
                    *((uint32_t *)&datext[1]) = u->c;
                    datext[1] = 0x16;
                    datext[5] = u->h;
                    datext[6] = 0x00;
                    *((uint32_t *)&datext[6]) = 0;
                    *((uint32_t *)&datext[10]) = 0;
                    *((uint32_t *)&datext[13]) = u->c;
                    datext[17] = 0;
                    *((uint32_t *)&datext[18]) = 0;
                    *((uint16_t *)&datext[20]) = 5400;

                    scsi->scsi_Actual = data[0] + 1;
                    err = 0;
                    break;
                
                default:
                    KPrintF("[WARN] Unhandled mode sense thing: %lx\n", ((UWORD)scsi->scsi_Command[2] << 8) | scsi->scsi_Command[3]);
                    err = HFERR_BadStatus;
                    break;
            }
            break;
        
        case 0x37: // READ DEFECT DATA (10)
            
            break;

        default:
            KPrintF("Unknown/unhandled SCSI command %lx.\n", scsi->scsi_Command[0]);
            err = HFERR_BadStatus;
            break;
    }

    if (err != 0) {
        KPrintF("Some SCSI error occured: %ld\n", err);
        scsi->scsi_Actual = 0;
    }

    return err;
}

#define DUMMYCMD iostd->io_Actual = 0; break;
uint8_t piscsi_perform_io(struct piscsi_unit *u, struct IORequest *io) {
    struct IOStdReq *iostd = (struct IOStdReq *)io;
    struct IOExtTD *iotd = (struct IOExtTD *)io;

    uint8_t *data;
    uint32_t len;
    uint32_t offset;
    //struct DriveGeometry *geom;
    uint8_t err = 0;

    if (!u->enabled) {
        return IOERR_OPENFAIL;
    }

    data = iotd->iotd_Req.io_Data;
    len = iotd->iotd_Req.io_Length;

    if (io->io_Error == IOERR_ABORTED) {
        return io->io_Error;
    }

    //KPrintF("cmd: %s\n",cmd_name(io->io_Command));
    //KPrintF("IO %lx Start, io_Flags = %ld, io_Command = %ld\n", io, io->io_Flags, io->io_Command);

    switch (io->io_Command) {
        case CMD_CLEAR:
            /* Invalidate read buffer */
            DUMMYCMD;
        case CMD_UPDATE:
            /* Flush write buffer */
            DUMMYCMD;
        case TD_PROTSTATUS:
            DUMMYCMD;
        case TD_CHANGENUM:
            iostd->io_Actual = u->change_num;
            break;
        case TD_REMOVE:
            DUMMYCMD;
        case TD_CHANGESTATE:
            DUMMYCMD;
        case TD_GETDRIVETYPE:
            iostd->io_Actual = DG_DIRECT_ACCESS;
            break;
        case TD_MOTOR:
            iostd->io_Actual = u->motor;
            u->motor = iostd->io_Length ? 1 : 0;
            break;

        case TD_FORMAT:
            offset = iotd->iotd_Req.io_Offset;
            //err = 0;
            err = piscsi_rw(u, io, offset, 1);
            break;
        case CMD_WRITE:
            offset = iotd->iotd_Req.io_Offset;
            //err = 0;
            err = piscsi_rw(u, io, offset, 1);
            break;
        case CMD_READ:
            offset = iotd->iotd_Req.io_Offset;
            //err = 0;
            err = piscsi_rw(u, io, offset, 0);
            break;
        case HD_SCSICMD:
            //err = 0;
            err = piscsi_scsi(u, io);
            break;
        default: {
            int cmd = io->io_Command;
            KPrintF("Unknown IO command: %ld\n", cmd);
            err = IOERR_NOCMD;
            break;
        }
    }

    return err;
}
#undef DUMMYCMD

ADDTABL_END();
