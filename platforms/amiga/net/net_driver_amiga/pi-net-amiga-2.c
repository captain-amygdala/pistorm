// SPDX-License-Identifier: MIT

#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/alerts.h>
#include <exec/tasks.h>
#include <exec/io.h>
#include <exec/execbase.h>
#include <exec/interrupts.h>
#include <hardware/intbits.h>

#include <libraries/expansion.h>

#include <devices/trackdisk.h>
#include <devices/timer.h>
#include <devices/scsidisk.h>

#include <dos/filehandler.h>
#include <dos/dostags.h>

#include <proto/exec.h>
#include <proto/disk.h>
#include <proto/expansion.h>
#include <proto/utility.h>
#include <proto/dos.h>

#include <utility/tagitem.h>

//#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
//#include <clib/debug_protos.h>
#include "sana2.h"
#include "../pi-net-enums.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define STR(s) #s
#define XSTR(s) STR(s)

#define DEVICE_NAME "pi-net.device"
#define DEVICE_DATE "(14 May 2021)"
#define DEVICE_ID_STRING "Pi-NET " XSTR(DEVICE_VERSION) "." XSTR(DEVICE_REVISION) " " DEVICE_DATE
#define DEVICE_VERSION 43
#define DEVICE_REVISION 10
#define DEVICE_PRIORITY 0

#pragma pack(4)
struct pinet_base {
    struct Device* pi_dev;
    struct Unit unit;
    uint8_t MAC[6];
    uint8_t IP[4];

	struct List read_list;
	struct SignalSemaphore read_list_sem;
};

struct ExecBase* SysBase = NULL;
uint8_t *saved_seg_list;
uint8_t is_open;

#define WRITESHORT(cmd, val) *(unsigned short *)((unsigned long)(PINET_OFFSET + cmd)) = val;
#define WRITELONG(cmd, val) *(unsigned long *)((unsigned long)(PINET_OFFSET + cmd)) = val;
#define WRITEBYTE(cmd, val) *(unsigned char *)((unsigned long)(PINET_OFFSET + cmd)) = val;

#define READBYTE(cmd, var) var = *(volatile unsigned char *)(PINET_OFFSET + cmd);
#define READSHORT(cmd, var) var = *(volatile unsigned short *)(PINET_OFFSET + cmd);
#define READLONG(cmd, var) var = *(volatile unsigned long *)(PINET_OFFSET + cmd);

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

typedef struct BufferManagement
{
  struct MinNode   bm_Node;
  BOOL           (*bm_CopyFromBuffer)(void* a __asm("a0"), void* b __asm("a1"), long c __asm("d0"));
  BOOL           (*bm_CopyToBuffer)(void* a __asm("a0"), void* b __asm("a1"), long c __asm("d0"));
} BufferManagement;

struct pinet_base *dev_base = NULL;

//#define exit(...)
//#define debug(...)
#define kprintf(...)

static struct Library __attribute__((used)) *init_device(struct Device* dev) {
	uint8_t *p;
	uint32_t i;
	int32_t  ok;

    // Unused variables
    if (ok || i || p) { }

    SysBase = *(struct ExecBase **)4L;

    kprintf("Initializing net device.\n");

    dev_base = AllocMem(sizeof(struct pinet_base), MEMF_PUBLIC | MEMF_CLEAR);
    dev_base->pi_dev = dev;

    kprintf("Grabbing MAC.\n");
    for (int i = 0; i < 6; i++) {
        READBYTE((PINET_CMD_MAC + i), dev_base->MAC[i]);
    }
    kprintf("Grabbing IP.\n");
    for (int i = 0; i < 4; i++) {
        READBYTE((PINET_CMD_IP + i), dev_base->IP[i]);
    }

	return (struct Library *)dev;
}

static uint8_t* __attribute__((used)) expunge(void) {
    kprintf("Cleaning up.\n");
    FreeMem(dev_base, sizeof(struct pinet_base));
    return 0;
}

static void __attribute__((used)) open(struct IORequest *io, uint32_t num, uint32_t flags) {
    struct IOSana2Req *ioreq = (struct IOSana2Req *)io;
	int32_t ok = 0, ret = IOERR_OPENFAIL;
    struct BufferManagement *bm;

    kprintf("Opening net device %ld.\n", num);
    dev_base->unit.unit_OpenCnt++;

    // Unused variables
    if (bm || ok) { }

    if (num == 0 && dev_base->unit.unit_OpenCnt == 1) {
        //kprintf("Trying to alloc buffer management.\n");
        //if ((bm = (struct BufferManagement*)AllocVec(sizeof(struct BufferManagement), MEMF_CLEAR | MEMF_PUBLIC))) {
            //kprintf("Setting up buffer copy funcs (1).\n");
            //bm->bm_CopyToBuffer = (BOOL *)GetTagData(S2_CopyToBuff, 0, (struct TagItem *)ioreq->ios2_BufferManagement);
            //kprintf("Setting up buffer copy funcs (2).\n");
            //bm->bm_CopyFromBuffer = (BOOL *)GetTagData(S2_CopyFromBuff, 0, (struct TagItem *)ioreq->ios2_BufferManagement);

            kprintf("Doing more things.\n");
            ioreq->ios2_BufferManagement = NULL;//(VOID *)bm;
            ioreq->ios2_Req.io_Error = 0;
            ioreq->ios2_Req.io_Unit = (struct Unit *)&dev_base->unit;
            ioreq->ios2_Req.io_Device = (struct Device *)dev_base->pi_dev;

            kprintf("New list.\n");

            NewList(&dev_base->read_list);
            InitSemaphore(&dev_base->read_list_sem);

            ret = 0;
            ok = 1;
        //}
    }

    if (ret == IOERR_OPENFAIL) {
        kprintf("Failed to open device. Already open?\n");
    }
    else {
        kprintf("Device opened, yay.\n");
    }
    ioreq->ios2_Req.io_Error = ret;
    ioreq->ios2_Req.io_Message.mn_Node.ln_Type = NT_REPLYMSG;

    kprintf("Opened device, return code: %ld\n", ret);
}

static uint8_t* __attribute__((used)) close(struct IORequest *io) {
    return 0;
}

uint32_t pinet_read_frame(struct IOSana2Req *ioreq) {
    return 0;
}

void pinet_write_frame(struct IOSana2Req *ioreq) {
}


static void __attribute__((used)) begin_io(struct IORequest *io) {
    struct IOSana2Req *ioreq = (struct IOSana2Req *)io;
    ULONG unit = (ULONG)ioreq->ios2_Req.io_Unit;
    int mtu;

    ioreq->ios2_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq->ios2_Req.io_Error = S2ERR_NO_ERROR;
    ioreq->ios2_WireError = S2WERR_GENERIC_ERROR;

    //D(("BeginIO command %ld unit %ld\n",(LONG)ioreq->ios2_Req.io_Command,unit));

    // Unused variables
    if (mtu || unit) {}

    switch( ioreq->ios2_Req.io_Command ) {
        case CMD_READ:
            kprintf("Read\n");
            if (pinet_read_frame(ioreq) != 0) {
                ioreq->ios2_Req.io_Error = S2ERR_BAD_ARGUMENT;
                ioreq->ios2_WireError = S2WERR_BUFF_ERROR;
            }
            ioreq = NULL;
            break;
        case S2_BROADCAST:
            kprintf("Broadcast\n");
            if (ioreq->ios2_DstAddr) {
                for (int i = 0; i < ADDRFIELDSIZE; i++) {
                    ioreq->ios2_DstAddr[i] = 0xFF;
                }
            } else {
                kprintf("Invalid ios2_DstAddr\n");
            }
            /* Fallthrough */
        case CMD_WRITE: {
            kprintf("Write\n");
            pinet_write_frame(ioreq);
            break;
        }

        case S2_READORPHAN:
            ioreq->ios2_Req.io_Flags &= ~SANA2IOF_QUICK;
            ioreq = NULL;
            break;
        case S2_ONLINE:
        case S2_OFFLINE:
        case S2_CONFIGINTERFACE:   /* forward request */
            break;

        case S2_GETSTATIONADDRESS:
            for (int i = 0; i < ADDRFIELDSIZE; i++) {
                ioreq->ios2_SrcAddr[i] = dev_base->MAC[i];
                ioreq->ios2_DstAddr[i] = dev_base->MAC[i];
            }
            break;
        case S2_DEVICEQUERY: {
            struct Sana2DeviceQuery *devquery;

            devquery = ioreq->ios2_StatData;
            devquery->DevQueryFormat = 0;
            devquery->DeviceLevel = 0;

            if (devquery->SizeAvailable >= 18)
                devquery->AddrFieldSize = ADDRFIELDSIZE * 8;
            if (devquery->SizeAvailable >= 22)
                devquery->MTU           = 1500;
            if (devquery->SizeAvailable >= 26)
                devquery->BPS           =  1000 * 1000 * 100;
            if (devquery->SizeAvailable >= 30)
                devquery->HardwareType  = S2WireType_Ethernet;

            devquery->SizeSupplied = (devquery->SizeAvailable < 30) ? devquery->SizeAvailable : 30;
            break;
        }
        case S2_GETSPECIALSTATS: {
            struct Sana2SpecialStatHeader *s2ssh = (struct Sana2SpecialStatHeader *)ioreq->ios2_StatData;
            s2ssh->RecordCountSupplied = 0;
            break;
        }
        default: {
            uint8_t cmd = ioreq->ios2_Req.io_Command;
            if (cmd) {}
            kprintf("Unknown/unhandled IO command %lx\n", cmd);
            ioreq->ios2_Req.io_Error = S2ERR_NOT_SUPPORTED;
            ioreq->ios2_WireError = S2WERR_GENERIC_ERROR;
            break;
        }
    }

    if (ioreq) {
        if (!(ioreq->ios2_Req.io_Flags & SANA2IOF_QUICK)) {
            ReplyMsg((struct Message *)ioreq);
        } else {
            ioreq->ios2_Req.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
        }
    }
}

static uint32_t __attribute__((used)) abort_io(struct IORequest* ioreq) {
    struct IOSana2Req* ios2 = (struct IOSana2Req*)ioreq;

    if (!ioreq) return IOERR_NOCMD;
	ioreq->io_Error = IOERR_ABORTED;
    ios2->ios2_WireError = 0;

    return IOERR_ABORTED;
}

static uint32_t device_vectors[] = {
    (uint32_t)open,
    (uint32_t)close,
    (uint32_t)expunge,
    0,
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
