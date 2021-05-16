// SPDX-License-Identifier: MIT

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

#include "newstyle.h"

#include "../piscsi-enums.h"
#include <stdint.h>

#define STR(s) #s
#define XSTR(s) STR(s)

#define DEVICE_NAME "pi-scsi.device"
#define DEVICE_DATE "(3 Feb 2021)"
#define DEVICE_ID_STRING "PiSCSI " XSTR(DEVICE_VERSION) "." XSTR(DEVICE_REVISION) " " DEVICE_DATE
#define DEVICE_VERSION 43
#define DEVICE_REVISION 20
#define DEVICE_PRIORITY 0

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
        uint16_t scsi_num;
        uint16_t h, s;
        uint32_t c;

        uint32_t change_num;
    } units[NUM_UNITS];
};

struct ExecBase *SysBase;
uint8_t *saved_seg_list;
uint8_t is_open;

#define WRITESHORT(cmd, val) *(unsigned short *)((unsigned long)(PISCSI_OFFSET+cmd)) = val;
#define WRITELONG(cmd, val) *(unsigned long *)((unsigned long)(PISCSI_OFFSET+cmd)) = val;
#define WRITEBYTE(cmd, val) *(unsigned char *)((unsigned long)(PISCSI_OFFSET+cmd)) = val;

#define READSHORT(cmd, var) var = *(volatile unsigned short *)(PISCSI_OFFSET + cmd);
#define READLONG(cmd, var) var = *(volatile unsigned long *)(PISCSI_OFFSET + cmd);

int __attribute__((no_reorder)) _start()
{
    return -1;
}

asm("romtag:                                \n"
    "       dc.w    "XSTR(RTC_MATCHWORD)"   \n"
    "       dc.l    romtag                  \n"
    "       dc.l    endcode                 \n"
    "       dc.b    "XSTR(RTF_AUTOINIT)"    \n"
    "       dc.b    "XSTR(DEVICE_VERSION)"  \n"
    "       dc.b    "XSTR(NT_DEVICE)"       \n"
    "       dc.b    "XSTR(DEVICE_PRIORITY)" \n"
    "       dc.l    _device_name            \n"
    "       dc.l    _device_id_string       \n"
    "       dc.l    _auto_init_tables       \n"
    "endcode:                               \n");

char device_name[] = DEVICE_NAME;
char device_id_string[] = DEVICE_ID_STRING;

uint8_t piscsi_perform_io(struct piscsi_unit *u, struct IORequest *io);
uint8_t piscsi_rw(struct piscsi_unit *u, struct IORequest *io);
uint8_t piscsi_scsi(struct piscsi_unit *u, struct IORequest *io);

#define debug(...)
#define debugval(...)
//#define debug(c, v) WRITESHORT(c, v)
//#define debugval(c, v) WRITELONG(c, v)

struct piscsi_base *dev_base = NULL;

static struct Library __attribute__((used)) *init_device(uint8_t *seg_list asm("a0"), struct Library *dev asm("d0"))
{
    SysBase = *(struct ExecBase **)4L;

    debug(PISCSI_DBG_MSG, DBG_INIT);

    dev_base = AllocMem(sizeof(struct piscsi_base), MEMF_PUBLIC | MEMF_CLEAR);
    dev_base->pi_dev = (struct Device *)dev;

    for (int i = 0; i < NUM_UNITS; i++) {
        uint16_t r = 0;
        WRITESHORT(PISCSI_CMD_DRVNUM, (i * 10));
        dev_base->units[i].regs_ptr = PISCSI_OFFSET;
        READSHORT(PISCSI_CMD_DRVTYPE, r);
        dev_base->units[i].enabled = r;
        dev_base->units[i].present = r;
        dev_base->units[i].valid = r;
        dev_base->units[i].unit_num = i;
        dev_base->units[i].scsi_num = i * 10;
        if (dev_base->units[i].present) {
            READLONG(PISCSI_CMD_CYLS, dev_base->units[i].c);
            READSHORT(PISCSI_CMD_HEADS, dev_base->units[i].h);
            READSHORT(PISCSI_CMD_SECS, dev_base->units[i].s);

            debugval(PISCSI_DBG_VAL1, dev_base->units[i].c);
            debugval(PISCSI_DBG_VAL2, dev_base->units[i].h);
            debugval(PISCSI_DBG_VAL3, dev_base->units[i].s);
            debug(PISCSI_DBG_MSG, DBG_CHS);
        }
        dev_base->units[i].change_num++;
    }

    return dev;
}

static uint8_t* __attribute__((used)) expunge(struct Library *dev asm("a6"))
{
    debug(PISCSI_DBG_MSG, DBG_CLEANUP);
    /*if (dev_base->open_count)
        return 0;
    FreeMem(dev_base, sizeof(struct piscsi_base));*/
    return 0;
}

static void __attribute__((used)) open(struct Library *dev asm("a6"), struct IOExtTD *iotd asm("a1"), uint32_t num asm("d0"), uint32_t flags asm("d1"))
{
    //struct Node* node = (struct Node*)iotd;
    int io_err = IOERR_OPENFAIL;

    //WRITESHORT(PISCSI_CMD_DEBUGME, 1);

    int unit_num = 0;
    WRITELONG(PISCSI_CMD_DRVNUM, num);
    READLONG(PISCSI_CMD_DRVNUM, unit_num);

    debugval(PISCSI_DBG_VAL1, unit_num);
    debugval(PISCSI_DBG_VAL2, flags);
    debugval(PISCSI_DBG_VAL3, num);
    debug(PISCSI_DBG_MSG, DBG_OPENDEV);

    if (iotd && unit_num < NUM_UNITS) {
        if (dev_base->units[unit_num].enabled && dev_base->units[unit_num].present) {
            io_err = 0;
            iotd->iotd_Req.io_Unit = (struct Unit*)&dev_base->units[unit_num].unit;
            iotd->iotd_Req.io_Unit->unit_flags = UNITF_ACTIVE;
            iotd->iotd_Req.io_Unit->unit_OpenCnt = 1;
        }
    }

    iotd->iotd_Req.io_Error = io_err;
    ((struct Library *)dev_base->pi_dev)->lib_OpenCnt++;
}

static uint8_t* __attribute__((used)) close(struct Library *dev asm("a6"), struct IOExtTD *iotd asm("a1"))
{
    ((struct Library *)dev_base->pi_dev)->lib_OpenCnt--;
    return 0;
}

static void __attribute__((used)) begin_io(struct Library *dev asm("a6"), struct IORequest *io asm("a1"))
{
    if (dev_base == NULL || io == NULL)
        return;
    
    struct piscsi_unit *u;
    struct Node* node = (struct Node*)io;
    u = (struct piscsi_unit *)io->io_Unit;

    if (node == NULL || u == NULL)
        return;

    debugval(PISCSI_DBG_VAL1, io->io_Command);
    debugval(PISCSI_DBG_VAL2, io->io_Flags);
    debugval(PISCSI_DBG_VAL3, (io->io_Flags & IOF_QUICK));
    debug(PISCSI_DBG_MSG, DBG_BEGINIO);
    io->io_Error = piscsi_perform_io(u, io);

    if (!(io->io_Flags & IOF_QUICK)) {
        ReplyMsg(&io->io_Message);
    }
}

static uint32_t __attribute__((used)) abort_io(struct Library *dev asm("a6"), struct IORequest *io asm("a1"))
{
    debug(PISCSI_DBG_MSG, DBG_ABORTIO);
    if (!io) return IOERR_NOCMD;
    io->io_Error = IOERR_ABORTED;

    return IOERR_ABORTED;
}

uint8_t piscsi_rw(struct piscsi_unit *u, struct IORequest *io) {
    struct IOStdReq *iostd = (struct IOStdReq *)io;
    struct IOExtTD *iotd = (struct IOExtTD *)io;

    uint8_t* data;
    uint32_t len;
    //uint32_t block, num_blocks;
    uint8_t sderr = 0;
    uint32_t block_size = 512;

    data = iotd->iotd_Req.io_Data;
    len = iotd->iotd_Req.io_Length;

    WRITESHORT(PISCSI_CMD_DRVNUMX, u->unit_num);
    READLONG(PISCSI_CMD_BLOCKSIZE, block_size);

    if (data == 0) {
        return IOERR_BADADDRESS;
    }
    if (len < block_size) {
        iostd->io_Actual = 0;
        return IOERR_BADLENGTH;
    }

    switch (io->io_Command) {
        case TD_WRITE64:
        case NSCMD_TD_WRITE64:
        case TD_FORMAT64:
        case NSCMD_TD_FORMAT64:
            WRITELONG(PISCSI_CMD_ADDR1, iostd->io_Offset);
            WRITELONG(PISCSI_CMD_ADDR2, len);
            WRITELONG(PISCSI_CMD_ADDR3, (uint32_t)data);
            WRITELONG(PISCSI_CMD_ADDR4, iostd->io_Actual);
            WRITESHORT(PISCSI_CMD_WRITE64, u->unit_num);
            break;
        case TD_READ64:
        case NSCMD_TD_READ64:
            WRITELONG(PISCSI_CMD_ADDR1, iostd->io_Offset);
            WRITELONG(PISCSI_CMD_ADDR2, len);
            WRITELONG(PISCSI_CMD_ADDR3, (uint32_t)data);
            WRITELONG(PISCSI_CMD_ADDR4, iostd->io_Actual);
            WRITESHORT(PISCSI_CMD_READ64, u->unit_num);
            break;
        case TD_FORMAT:
        case CMD_WRITE:
            WRITELONG(PISCSI_CMD_ADDR1, iostd->io_Offset);
            WRITELONG(PISCSI_CMD_ADDR2, len);
            WRITELONG(PISCSI_CMD_ADDR3, (uint32_t)data);
            WRITESHORT(PISCSI_CMD_WRITEBYTES, u->unit_num);
            break;
        case CMD_READ:
            WRITELONG(PISCSI_CMD_ADDR1, iostd->io_Offset);
            WRITELONG(PISCSI_CMD_ADDR2, len);
            WRITELONG(PISCSI_CMD_ADDR3, (uint32_t)data);
            WRITESHORT(PISCSI_CMD_READBYTES, u->unit_num);
            break;
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
        iostd->io_Actual = iotd->iotd_Req.io_Length;
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
    uint32_t i, block = 0, blocks = 0, maxblocks = 0;
    uint8_t err = 0;
    uint8_t write = 0;
    uint32_t block_size = 512;

    WRITESHORT(PISCSI_CMD_DRVNUMX, u->unit_num);
    READLONG(PISCSI_CMD_BLOCKSIZE, block_size);

    debugval(PISCSI_DBG_VAL1, iostd->io_Length);
    debugval(PISCSI_DBG_VAL2, scsi->scsi_Command[0]);
    debugval(PISCSI_DBG_VAL3, scsi->scsi_Command[1]);
    debugval(PISCSI_DBG_VAL4, scsi->scsi_Command[2]);
    debugval(PISCSI_DBG_VAL5, scsi->scsi_CmdLength);
    debug(PISCSI_DBG_MSG, DBG_SCSICMD);

    //maxblocks = u->s * u->c * u->h;

    if (scsi->scsi_CmdLength < 6) {
        return IOERR_BADLENGTH;
    }

    if (scsi->scsi_Command == NULL) {
        return IOERR_BADADDRESS;
    }

    scsi->scsi_Actual = 0;
    //iostd->io_Actual = sizeof(*scsi);

    switch (scsi->scsi_Command[0]) {
        case SCSICMD_TEST_UNIT_READY:
            err = 0;
            break;
        
        case SCSICMD_INQUIRY:
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
        
        case SCSICMD_WRITE_6:
            write = 1;
        case SCSICMD_READ_6:
            //block = *(uint32_t *)(&scsi->scsi_Command[0]) & 0x001FFFFF;
            block = scsi->scsi_Command[1] & 0x1f;
            block = (block << 8) | scsi->scsi_Command[2];
            block = (block << 8) | scsi->scsi_Command[3];
            blocks = scsi->scsi_Command[4];
            debugval(PISCSI_DBG_VAL1, (uint32_t)scsi->scsi_Command);
            debug(PISCSI_DBG_MSG, DBG_SCSICMD_RW6);
            goto scsireadwrite;
        case SCSICMD_WRITE_10:
            write = 1;
        case SCSICMD_READ_10:
            debugval(PISCSI_DBG_VAL1, (uint32_t)scsi->scsi_Command);
            debug(PISCSI_DBG_MSG, DBG_SCSICMD_RW10);
            //block = *(uint32_t *)(&scsi->scsi_Command[2]);
            block = scsi->scsi_Command[2];
            block = (block << 8) | scsi->scsi_Command[3];
            block = (block << 8) | scsi->scsi_Command[4];
            block = (block << 8) | scsi->scsi_Command[5];

            //blocks = *(uint16_t *)(&scsi->scsi_Command[7]);
            blocks = scsi->scsi_Command[7];
            blocks = (blocks << 8) | scsi->scsi_Command[8];

scsireadwrite:;
            WRITESHORT(PISCSI_CMD_DRVNUM, (u->scsi_num));
            READLONG(PISCSI_CMD_BLOCKS, maxblocks);
            if (block + blocks > maxblocks || blocks == 0) {
                err = IOERR_BADADDRESS;
                break;
            }
            if (data == NULL) {
                err = IOERR_BADADDRESS;
                break;
            }

            if (write == 0) {
                WRITELONG(PISCSI_CMD_ADDR1, block);
                WRITELONG(PISCSI_CMD_ADDR2, (blocks << 9));
                WRITELONG(PISCSI_CMD_ADDR3, (uint32_t)data);
                WRITESHORT(PISCSI_CMD_READ, u->unit_num);
            }
            else {
                WRITELONG(PISCSI_CMD_ADDR1, block);
                WRITELONG(PISCSI_CMD_ADDR2, (blocks << 9));
                WRITELONG(PISCSI_CMD_ADDR3, (uint32_t)data);
                WRITESHORT(PISCSI_CMD_WRITE, u->unit_num);
            }

            scsi->scsi_Actual = scsi->scsi_Length;
            err = 0;
            break;
        
        case SCSICMD_READ_CAPACITY_10:
            if (scsi->scsi_CmdLength < 10) {
                err = HFERR_BadStatus;
                break;
            }

            if (scsi->scsi_Length < 8) {
                err = IOERR_BADLENGTH;
                break;
            }
            
            WRITESHORT(PISCSI_CMD_DRVNUM, (u->scsi_num));
            READLONG(PISCSI_CMD_BLOCKS, blocks);
            ((uint32_t*)data)[0] = blocks - 1;
            ((uint32_t*)data)[1] = block_size;

            scsi->scsi_Actual = 8;
            err = 0;

            break;
        case SCSICMD_MODE_SENSE_6:
            data[0] = 3 + 8 + 0x16;
            data[1] = 0; // MEDIUM TYPE
            data[2] = 0;
            data[3] = 8;

            debugval(PISCSI_DBG_VAL1, ((uint32_t)scsi->scsi_Command));
            debug(PISCSI_DBG_MSG, DBG_SCSI_DEBUG_MODESENSE_6);

            WRITESHORT(PISCSI_CMD_DRVNUM, (u->scsi_num));
            READLONG(PISCSI_CMD_BLOCKS, maxblocks);
            (blocks = (maxblocks - 1) & 0xFFFFFF);

            *((uint32_t *)&data[4]) = blocks;
            *((uint32_t *)&data[8]) = block_size;

            switch (((UWORD)scsi->scsi_Command[2] << 8) | scsi->scsi_Command[3]) {
                case 0x0300: { // Format Device Mode
                    debug(PISCSI_DBG_MSG, DBG_SCSI_FORMATDEVICE);
                    uint8_t *datext = data + 12;
                    datext[0] = 0x03;
                    datext[1] = 0x16;
                    datext[2] = 0x00;
                    datext[3] = 0x01;
                    *((uint32_t *)&datext[4]) = 0;
                    *((uint32_t *)&datext[8]) = 0;
                    *((uint16_t *)&datext[10]) = u->s;
                    *((uint16_t *)&datext[12]) = block_size;
                    datext[14] = 0x00;
                    datext[15] = 0x01;
                    *((uint32_t *)&datext[16]) = 0;
                    datext[20] = 0x80;

                    scsi->scsi_Actual = data[0] + 1;
                    err = 0;
                    break;
                }
                case 0x0400: // Rigid Drive Geometry
                    debug(PISCSI_DBG_MSG, DBG_SCSI_RDG);
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
                    debugval(PISCSI_DBG_VAL1, (((UWORD)scsi->scsi_Command[2] << 8) | scsi->scsi_Command[3]));
                    debug(PISCSI_DBG_MSG, DBG_SCSI_UNKNOWN_MODESENSE);
                    err = HFERR_BadStatus;
                    break;
            }
            break;
        
        case SCSICMD_READ_DEFECT_DATA_10:
            break;
        case SCSICMD_CHANGE_DEFINITION:
            break;

        default:
            debugval(PISCSI_DBG_VAL1, scsi->scsi_Command[0]);
            debug(PISCSI_DBG_MSG, DBG_SCSI_UNKNOWN_COMMAND);
            err = HFERR_BadStatus;
            break;
    }

    if (err != 0) {
        debugval(PISCSI_DBG_VAL1, err);
        debug(PISCSI_DBG_MSG, DBG_SCSIERR);
        scsi->scsi_Actual = 0;
    }

    return err;
}

uint16_t ns_support[] = {
    NSCMD_DEVICEQUERY,
  	CMD_RESET,
	CMD_READ,
	CMD_WRITE,
	CMD_UPDATE,
	CMD_CLEAR,
	CMD_START,
	CMD_STOP,
	CMD_FLUSH,
	TD_MOTOR,
	TD_SEEK,
	TD_FORMAT,
	TD_REMOVE,
	TD_CHANGENUM,
	TD_CHANGESTATE,
	TD_PROTSTATUS,
	TD_GETDRIVETYPE,
	TD_GETGEOMETRY,
	TD_ADDCHANGEINT,
	TD_REMCHANGEINT,
	HD_SCSICMD,
	NSCMD_TD_READ64,
	NSCMD_TD_WRITE64,
	NSCMD_TD_SEEK64,
	NSCMD_TD_FORMAT64,
	0,
};

#define DUMMYCMD iostd->io_Actual = 0; break;
uint8_t piscsi_perform_io(struct piscsi_unit *u, struct IORequest *io) {
    struct IOStdReq *iostd = (struct IOStdReq *)io;
    struct IOExtTD *iotd = (struct IOExtTD *)io;

    //uint8_t *data;
    //uint32_t len;
    //uint32_t offset;
    //struct DriveGeometry *geom;
    uint8_t err = 0;

    if (!u->enabled) {
        return IOERR_OPENFAIL;
    }

    //data = iotd->iotd_Req.io_Data;
    //len = iotd->iotd_Req.io_Length;

    if (io->io_Error == IOERR_ABORTED) {
        return io->io_Error;
    }

    debugval(PISCSI_DBG_VAL1, io->io_Command);
    debugval(PISCSI_DBG_VAL2, io->io_Flags);
    debugval(PISCSI_DBG_VAL3, iostd->io_Length);
    debug(PISCSI_DBG_MSG, DBG_IOCMD);

    switch (io->io_Command) {
        case NSCMD_DEVICEQUERY: {
            struct NSDeviceQueryResult *res = (struct NSDeviceQueryResult *)iotd->iotd_Req.io_Data;
            res->DevQueryFormat = 0;
            res->SizeAvailable = 16;;
            res->DeviceType = NSDEVTYPE_TRACKDISK;
            res->DeviceSubType = 0;
            res->SupportedCommands = ns_support;

            iostd->io_Actual = 16;
            return 0;
            break;
        }
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
        case TD_FORMAT64:
        case NSCMD_TD_FORMAT64:
        case TD_READ64:
        case NSCMD_TD_READ64:
        case TD_WRITE64:
        case NSCMD_TD_WRITE64:
        case CMD_WRITE:
        case CMD_READ:
            err = piscsi_rw(u, io);
            break;
        case HD_SCSICMD:
            //err = 0;
            err = piscsi_scsi(u, io);
            break;
        default: {
            //int cmd = io->io_Command;
            debug(PISCSI_DBG_MSG, DBG_IOCMD_UNHANDLED);
            err = IOERR_NOCMD;
            break;
        }
    }

    return err;
}
#undef DUMMYCMD

static uint32_t device_vectors[] = {
    (uint32_t)open,
    (uint32_t)close,
    (uint32_t)expunge,
    0, //extFunc not used here
    (uint32_t)begin_io,
    (uint32_t)abort_io,
    -1
};

const uint32_t auto_init_tables[4] = {
    sizeof(struct Library),
    (uint32_t)device_vectors,
    0,
    (uint32_t)init_device
};
