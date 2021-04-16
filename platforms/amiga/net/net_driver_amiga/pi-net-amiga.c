// SPDX-License-Identifier: MIT

#include <exec/resident.h>
#include <exec/memory.h>
#include <exec/alerts.h>
#include <exec/io.h>
#include <exec/execbase.h>
#include <libraries/expansion.h>
#include <dos/filehandler.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/expansion.h>
#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
#include <exec/lists.h>
#include <exec/errors.h>
#include <exec/interrupts.h>
#include <exec/tasks.h>
#include <hardware/intbits.h>
#include <string.h>
#include "sana2.h"
#include "../pi-net-enums.h"

#include <clib/debug_protos.h>
#include <stdint.h>
#include <stdlib.h>

#define WRITESHORT(cmd, val) *(unsigned short *)((unsigned long)(PINET_OFFSET + cmd)) = val;
#define WRITELONG(cmd, val) *(unsigned long *)((unsigned long)(PINET_OFFSET + cmd)) = val;
#define WRITEBYTE(cmd, val) *(unsigned char *)((unsigned long)(PINET_OFFSET + cmd)) = val;

#define READBYTE(cmd, var) var = *(volatile unsigned char *)(PINET_OFFSET + cmd);
#define READSHORT(cmd, var) var = *(volatile unsigned short *)(PINET_OFFSET + cmd);
#define READLONG(cmd, var) var = *(volatile unsigned long *)(PINET_OFFSET + cmd);

//typedef BOOL (*BMFunc)(void* a __asm("a0"), void* b __asm("a1"), long c __asm("d0"));

typedef struct BufferManagement
{
  struct MinNode   bm_Node;
  BOOL           (*bm_CopyFromBuffer)(void* a __asm("a0"), void* b __asm("a1"), long c __asm("d0"));
  BOOL           (*bm_CopyToBuffer)(void* a __asm("a0"), void* b __asm("a1"), long c __asm("d0"));
} BufferManagement;

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

const char DevName[] = "pi-net.device";
const char DevIdString[] = "Pi-NET 0.1";

const UWORD DevVersion = 1;
const UWORD DevRevision = 0;

#include "stabs.h"

struct pinet_base *dev_base = NULL;

struct WBStartup *_WBenchMsg = NULL;

//#define exit(...)
//#define debug(...)
//#define KPrintF(...)

uint32_t __UserDevInit(struct Device* dev) {
	uint8_t *p;
	uint32_t i;
	int32_t  ok;

    SysBase = *(struct ExecBase **)4L;

    KPrintF("Initializing net device.\n");

    dev_base = AllocMem(sizeof(struct pinet_base), MEMF_PUBLIC | MEMF_CLEAR);
    dev_base->pi_dev = dev;

    KPrintF("Grabbing MAC.\n");
    for (int i = 0; i < 6; i++) {
        READBYTE((PINET_CMD_MAC + i), dev_base->MAC[i]);
    }
    KPrintF("Grabbing IP.\n");
    for (int i = 0; i < 4; i++) {
        READBYTE((PINET_CMD_IP + i), dev_base->IP[i]);
    }

	return (uint32_t)dev;
}

uint32_t __UserDevCleanup(void) {
    KPrintF("Cleaning up.\n");
    FreeMem(dev_base, sizeof(struct pinet_base));
    return 0;
}

uint32_t __UserDevOpen(struct IORequest *io, uint32_t num, uint32_t flags) {
    struct IOSana2Req *ioreq = (struct IOSana2Req *)io;
	uint32_t ok = 0, ret = IOERR_OPENFAIL;
    struct BufferManagement *bm;

    KPrintF("Opening net device %ld.\n", num);
    dev_base->unit.unit_OpenCnt++;

    if (num == 0 && dev_base->unit.unit_OpenCnt == 1) {
        //KPrintF("Trying to alloc buffer management.\n");
        //if ((bm = (struct BufferManagement*)AllocVec(sizeof(struct BufferManagement), MEMF_CLEAR | MEMF_PUBLIC))) {
            //KPrintF("Setting up buffer copy funcs (1).\n");
            //bm->bm_CopyToBuffer = (BOOL *)GetTagData(S2_CopyToBuff, 0, (struct TagItem *)ioreq->ios2_BufferManagement);
            //KPrintF("Setting up buffer copy funcs (2).\n");
            //bm->bm_CopyFromBuffer = (BOOL *)GetTagData(S2_CopyFromBuff, 0, (struct TagItem *)ioreq->ios2_BufferManagement);

            KPrintF("Doing more things.\n");
            ioreq->ios2_BufferManagement = NULL;//(VOID *)bm;
            ioreq->ios2_Req.io_Error = 0;
            ioreq->ios2_Req.io_Unit = (struct Unit *)&dev_base->unit;
            ioreq->ios2_Req.io_Device = (struct Device *)dev_base->pi_dev;

            KPrintF("New list.\n");

            NewList(&dev_base->read_list);
            InitSemaphore(&dev_base->read_list_sem);

            ret = 0;
            ok = 1;
        //}
    }

    if (ret == IOERR_OPENFAIL) {
        KPrintF("Failed to open device. Already open?\n");
    }
    else {
        KPrintF("Device opened, yay.\n");
    }
    ioreq->ios2_Req.io_Message.mn_Node.ln_Type = NT_REPLYMSG;

    KPrintF("Opened device, return code: %ld\n", ret);

    return ret;
}

uint32_t __UserDevClose(struct IORequest *io) {
  return 0;
}

uint32_t pinet_read_frame(struct IOSana2Req *ioreq) {
    uint32_t datasize;
    uint8_t *frame_ptr;
    uint8_t broadcast;
    uint32_t err = 0;
    struct BufferManagement *bm;

    /*uint8_t* frm = (uint8_t *)(PINET_OFFSET + PINET_CMD_FRAME);
    uint32_t sz   = ((uint32_t)frm[0] << 8) | ((uint32_t)frm[1]);
    uint32_t ser  = ((uint32_t)frm[2] << 8) | ((uint32_t)frm[3]);
    uint16_t tp   = ((uint16_t)frm[16] << 8) | ((uint16_t)frm[17]);

    if (req->ios2_Req.io_Flags & SANA2IOF_RAW) {
        frame_ptr = frm + 4;
        datasize = sz;
        req->ios2_Req.io_Flags = SANA2IOF_RAW;
    }
    else {
        frame_ptr = frm + 4 + ETH_HDR_SIZE;
        datasize = sz - ETH_HDR_SIZE;
        req->ios2_Req.io_Flags = 0;
    }

    req->ios2_DataLength = datasize;

    //D(("datasize: %lx\n",datasize));
    //D(("frame_ptr: %lx\n",frame_ptr));
    //D(("ios2_Data: %lx\n",req->ios2_Data));
    //D(("bufmgmt: %lx\n",req->ios2_BufferManagement));

    // copy frame to device user (probably tcp/ip system)
    bm = (struct BufferManagement *)req->ios2_BufferManagement;
    if (!(*bm->bm_CopyToBuffer)(req->ios2_Data, frame_ptr, datasize)) {
        //D(("rx copybuf error\n"));
        req->ios2_Req.io_Error = S2ERR_SOFTWARE;
        req->ios2_WireError = S2WERR_BUFF_ERROR;
        err = 1;
    }
    else {
        req->ios2_Req.io_Error = req->ios2_WireError = 0;
        err = 0;
    }

    memcpy(req->ios2_SrcAddr, frame+4+6, HW_ADDRFIELDSIZE);
    memcpy(req->ios2_DstAddr, frame+4, HW_ADDRFIELDSIZE);

    //D(("RXSZ %ld\n",(LONG)sz));
    //D(("RXPT %ld\n",(LONG)tp));

    //D(("RXSER %ld\n",(LONG)ser));
    //D(("RXDST %lx...\n",*((ULONG*)(req->ios2_DstAddr))));
    //D(("RXSRC %lx\n",*((ULONG*)(req->ios2_SrcAddr))));
    //D(("RXSRC %lx\n",*((ULONG*)(frame_ptr))));

    broadcast = TRUE;
    for (int i=0; i<HW_ADDRFIELDSIZE; i++) {
        if (frame[i+4] != 0xff) {
        broadcast = FALSE;
        break;
        }
    }
    if (broadcast) {
        req->ios2_Req.io_Flags |= SANA2IOF_BCAST;
    }

    req->ios2_PacketType = tp;*/

    return err;
}

void pinet_write_frame(struct IOSana2Req *ioreq) {

}

void exit(int status) { }

ADDTABL_1(__BeginIO,a1);
void __BeginIO(struct IORequest *io) {
    struct IOSana2Req *ioreq = (struct IOSana2Req *)io;
    ULONG unit = (ULONG)ioreq->ios2_Req.io_Unit;
    int mtu;

    ioreq->ios2_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq->ios2_Req.io_Error = S2ERR_NO_ERROR;
    ioreq->ios2_WireError = S2WERR_GENERIC_ERROR;

    //D(("BeginIO command %ld unit %ld\n",(LONG)ioreq->ios2_Req.io_Command,unit));

    switch( ioreq->ios2_Req.io_Command ) {
        case CMD_READ:
            KPrintF("Read\n");
            if (pinet_read_frame(ioreq) != 0) {
                ioreq->ios2_Req.io_Error = S2ERR_BAD_ARGUMENT;
                ioreq->ios2_WireError = S2WERR_BUFF_ERROR;
            }
            ioreq = NULL;
            break;
        case S2_BROADCAST:
            KPrintF("Broadcast\n");
            if (ioreq->ios2_DstAddr) {
                for (int i = 0; i < ADDRFIELDSIZE; i++) {
                    ioreq->ios2_DstAddr[i] = 0xFF;
                }
            } else {
                KPrintF("Invalid ios2_DstAddr\n");
            }
            /* Fallthrough */
        case CMD_WRITE: {
            KPrintF("Write\n");
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
            KPrintF("Unknown/unhandled IO command %lx\n", cmd);
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

ADDTABL_1(__AbortIO,a1);
void __AbortIO(struct IORequest* ioreq) {
    struct IOSana2Req* ios2 = (struct IOSana2Req*)ioreq;

    if (!ioreq) return;
	ioreq->io_Error = IOERR_ABORTED;
    ios2->ios2_WireError = 0;
}

ADDTABL_1(__TermIO,a1);
void __TermIO(struct IORequest *ioreq) {
    struct IOSana2Req* ios2 = (struct IOSana2Req*)ioreq;

    if (!(ios2->ios2_Req.io_Flags & SANA2IOF_QUICK)) {
        ReplyMsg((struct Message *)ioreq);
    } else {
        ioreq->io_Message.mn_Node.ln_Type = NT_REPLYMSG;
    }
}

/*ULONG get_frame_serial(UBYTE* frame) {
  UBYTE* frm = (UBYTE*)frame;
  ULONG ser  = ((ULONG)frm[2]<<8)|((ULONG)frm[3]);
  return ser;
}

ULONG read_frame(struct IOSana2Req *req, volatile UBYTE *frame)
{
  ULONG datasize;
  BYTE *frame_ptr;
  BOOL broadcast;
  ULONG err = 0;
  struct BufferManagement *bm;

  UBYTE* frm = (UBYTE*)frame;
  ULONG sz   = ((ULONG)frm[0]<<8)|((ULONG)frm[1]);
  ULONG ser  = ((ULONG)frm[2]<<8)|((ULONG)frm[3]);
  USHORT tp  = ((USHORT)frm[16]<<8)|((USHORT)frm[17]);

  if (req->ios2_Req.io_Flags & SANA2IOF_RAW) {
    frame_ptr = frm+4;
    datasize = sz;
    req->ios2_Req.io_Flags = SANA2IOF_RAW;
  }
  else {
    frame_ptr = frm+4+HW_ETH_HDR_SIZE;
    datasize = sz-HW_ETH_HDR_SIZE;
    req->ios2_Req.io_Flags = 0;
  }

  req->ios2_DataLength = datasize;

  //D(("datasize: %lx\n",datasize));
  //D(("frame_ptr: %lx\n",frame_ptr));
  //D(("ios2_Data: %lx\n",req->ios2_Data));
  //D(("bufmgmt: %lx\n",req->ios2_BufferManagement));

  // copy frame to device user (probably tcp/ip system)
  bm = (struct BufferManagement *)req->ios2_BufferManagement;
  if (!(*bm->bm_CopyToBuffer)(req->ios2_Data, frame_ptr, datasize)) {
    //D(("rx copybuf error\n"));
    req->ios2_Req.io_Error = S2ERR_SOFTWARE;
    req->ios2_WireError = S2WERR_BUFF_ERROR;
    err = 1;
  }
  else {
    req->ios2_Req.io_Error = req->ios2_WireError = 0;
    err = 0;
  }

  memcpy(req->ios2_SrcAddr, frame+4+6, HW_ADDRFIELDSIZE);
  memcpy(req->ios2_DstAddr, frame+4, HW_ADDRFIELDSIZE);

  //D(("RXSZ %ld\n",(LONG)sz));
  //D(("RXPT %ld\n",(LONG)tp));

  //D(("RXSER %ld\n",(LONG)ser));
  //D(("RXDST %lx...\n",*((ULONG*)(req->ios2_DstAddr))));
  //D(("RXSRC %lx\n",*((ULONG*)(req->ios2_SrcAddr))));
  //D(("RXSRC %lx\n",*((ULONG*)(frame_ptr))));

  broadcast = TRUE;
  for (int i=0; i<HW_ADDRFIELDSIZE; i++) {
    if (frame[i+4] != 0xff) {
      broadcast = FALSE;
      break;
    }
  }
  if (broadcast) {
    req->ios2_Req.io_Flags |= SANA2IOF_BCAST;
  }

  req->ios2_PacketType = tp;

  return err;
}

ULONG write_frame(struct IOSana2Req *req, UBYTE *frame)
{
   ULONG rc=0;
   struct BufferManagement *bm;
   USHORT sz=0;

   if (req->ios2_Req.io_Flags & SANA2IOF_RAW) {
      sz = req->ios2_DataLength;
   } else {
      sz = req->ios2_DataLength + HW_ETH_HDR_SIZE;
      *((USHORT*)(frame+6+6)) = (USHORT)req->ios2_PacketType;
      memcpy(frame, req->ios2_DstAddr, HW_ADDRFIELDSIZE);
      memcpy(frame+6, HW_MAC, HW_ADDRFIELDSIZE);
      frame+=HW_ETH_HDR_SIZE;
   }

   if (sz>0) {
     bm = (struct BufferManagement *)req->ios2_BufferManagement;

     if (!(*bm->bm_CopyFromBuffer)(frame, req->ios2_Data, req->ios2_DataLength)) {
       rc = 1; // FIXME error code
       //D(("tx copybuf err\n"));
     }
     else {
       // buffer was copied to zz9000, send it
       volatile USHORT* reg = (volatile USHORT*)(ZZ9K_REGS+0x80); // FIXME send_frame reg
       *reg = sz;

       // get feedback
       rc = *reg;
       if (rc!=0) {
         D(("tx err: %d\n",rc));
       }
     }
   }

   return rc;
}*/

ADDTABL_END();
