/*
 * Copyright (c) 2020-2021 Niklas Ekström
 *
 * Thanks to Christian Vogelgsang and Mike Sterling for inspiration gained from their SANA-II drivers:
 * - https://github.com/cnvogelg/plipbox
 * - https://github.com/mikestir/k1208-drivers
 */

#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/alerts.h>
#include <exec/devices.h>
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/io.h>
#include <exec/execbase.h>
#include <exec/ports.h>

#include <libraries/expansion.h>
#include <libraries/dos.h>

#include <devices/trackdisk.h>
#include <devices/timer.h>
#include <devices/scsidisk.h>

#include <dos/filehandler.h>

#include <proto/disk.h>
#include <proto/expansion.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <string.h>

#include <clib/alib_protos.h>

#include "../../a314device/a314.h"
#include "../../a314device/proto_a314.h"
#include "sana2.h"

#include <stdint.h>

// Defines.

#define kprintf(...)

#define STR(s) #s
#define XSTR(s) STR(s)

#define DEVICE_NAME "a314eth.device"
#define DEVICE_DATE "(19 May 2021)"
#define SERVICE_NAME "ethernet"
#define DEVICE_ID_STRING "A314Eth " XSTR(DEVICE_VERSION) "." XSTR(DEVICE_REVISION) " " DEVICE_DATE
#define DEVICE_VERSION 42
#define DEVICE_REVISION 2
#define DEVICE_PRIORITY 0


#define DEVICE_NAME		"a314eth.device"
#define SERVICE_NAME		"ethernet"

#define TASK_PRIO		10

#define MACADDR_SIZE		6
#define NIC_BPS			10000000

#define ETH_MTU			1500
#define RAW_MTU			1518

#define READ_FRAME_REQ		1
#define WRITE_FRAME_REQ		2
#define READ_FRAME_RES		3
#define WRITE_FRAME_RES		4

#define ET_RBUF_CNT		2
#define ET_WBUF_CNT		2
#define ET_BUF_CNT		(ET_RBUF_CNT + ET_WBUF_CNT)

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
    "endcode:                               \n"
	".align 4\n"
	"	dc.l	16\n"
	"_device_process_seglist:\n"
	"	dc.l	0\n"
	"	jmp	_device_process_run\n");

char device_name[] = DEVICE_NAME;
char device_id_string[] = DEVICE_ID_STRING;

// Typedefs.

typedef BOOL (*buf_copy_func_t)(void *dst asm("a0"), void *src asm("a1"), LONG size asm("d0"));

// Structs.

#pragma pack(push, 1)
struct EthHdr
{
	unsigned char eh_Dst[MACADDR_SIZE];
	unsigned char eh_Src[MACADDR_SIZE];
	unsigned short eh_Type;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ServiceMsg
{
	ULONG sm_Address;
	UWORD sm_Length;
	UWORD sm_Kind;
};
#pragma pack(pop)

struct BufDesc
{
	struct MinNode bd_Node;
	void *bd_Buffer;
	int bd_Length;
};

// Constants.

static const char service_name[] = SERVICE_NAME;
static const char a314_device_name[] = A314_NAME;

static const unsigned char macaddr[MACADDR_SIZE] = { 0x40, 0x61, 0x33, 0x31, 0x34, 0x65 };

// Global variables.

BPTR saved_seg_list;
struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Library *A314Base;

buf_copy_func_t copyfrom;
buf_copy_func_t copyto;

volatile struct List ut_rbuf_list;
volatile struct List ut_wbuf_list;

struct BufDesc et_bufs[ET_BUF_CNT];

struct List et_rbuf_free_list;
struct List et_rbuf_pending_list;
struct List et_rbuf_has_data_list;

struct List et_wbuf_free_list;
struct List et_wbuf_pending_list;

LONG a314_socket;
short last_req_kind;

struct MsgPort a314_mp;

struct A314_IORequest read_ior;
struct A314_IORequest write_ior;
struct A314_IORequest reset_ior;

BOOL pending_a314_read;
BOOL pending_a314_write;
BOOL pending_a314_reset;

struct ServiceMsg a314_read_buf;
struct ServiceMsg a314_write_buf;

volatile ULONG sana2_sigmask;
volatile ULONG shutdown_sigmask;

volatile struct Task *init_task;

struct Process *device_process;
volatile int device_start_error;
void device_process_run();

// External declarations.

extern void device_process_seglist();

// Procedures.

static struct Library __attribute__((used)) *init_device(uint8_t *seg_list asm("a0"), struct Library *dev asm("d0"))
{
	saved_seg_list = (BPTR)seg_list;

	dev->lib_Node.ln_Type = NT_DEVICE;
	dev->lib_Node.ln_Name = (char *)device_name;
	dev->lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
	dev->lib_Version = 1;
	dev->lib_Revision = 0;
	dev->lib_IdString = (APTR)device_id_string;

	SysBase = *(struct ExecBase **)4;
	DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)DOSNAME, 0);

	return dev;
}

static uint8_t* __attribute__((used)) expunge(struct Library *dev asm("a6"))
{
	if (dev->lib_OpenCnt)
	{
		dev->lib_Flags |= LIBF_DELEXP;
		return 0;
	}

	// Shady way of waiting for device process to terminate before unloading.
	Delay(10);

	CloseLibrary((struct Library *)DOSBase);

	Remove(&dev->lib_Node);
	FreeMem((char *)dev - dev->lib_NegSize, dev->lib_NegSize + dev->lib_PosSize);
	return (uint8_t *)saved_seg_list;
}

static void send_a314_cmd(struct A314_IORequest *ior, UWORD cmd, char *buffer, int length)
{
	ior->a314_Request.io_Command = cmd;
	ior->a314_Request.io_Error = 0;
	ior->a314_Socket = a314_socket;
	ior->a314_Buffer = (STRPTR)buffer;
	ior->a314_Length = length;
	SendIO((struct IORequest *)ior);
}

static void do_a314_cmd(struct A314_IORequest *ior, UWORD cmd, char *buffer, int length)
{
	ior->a314_Request.io_Command = cmd;
	ior->a314_Request.io_Error = 0;
	ior->a314_Socket = a314_socket;
	ior->a314_Buffer = (STRPTR)buffer;
	ior->a314_Length = length;
	DoIO((struct IORequest *)ior);
}

static void copy_from_bd_and_reply(struct IOSana2Req *ios2, struct BufDesc *bd)
{
	struct EthHdr *eh = bd->bd_Buffer;

	if (ios2->ios2_Req.io_Flags & SANA2IOF_RAW)
	{
		ios2->ios2_DataLength = bd->bd_Length;
		copyto(ios2->ios2_Data, bd->bd_Buffer, ios2->ios2_DataLength);
		ios2->ios2_Req.io_Flags = SANA2IOF_RAW;
	}
	else
	{
		ios2->ios2_DataLength = bd->bd_Length - sizeof(struct EthHdr);
		copyto(ios2->ios2_Data, &eh[1], ios2->ios2_DataLength);
		ios2->ios2_Req.io_Flags = 0;
	}

	memcpy(ios2->ios2_SrcAddr, eh->eh_Src, MACADDR_SIZE);
	memcpy(ios2->ios2_DstAddr, eh->eh_Dst, MACADDR_SIZE);

	BOOL bcast = TRUE;
	for (int i = 0; i < MACADDR_SIZE; i++)
	{
		if (eh->eh_Dst[i] != 0xff)
		{
			bcast = FALSE;
			break;
		}
	}

	if (bcast)
		ios2->ios2_Req.io_Flags |= SANA2IOF_BCAST;

	ios2->ios2_PacketType = eh->eh_Type;

	ios2->ios2_Req.io_Error = 0;
	ReplyMsg(&ios2->ios2_Req.io_Message);
}

static void copy_to_bd_and_reply(struct BufDesc *bd, struct IOSana2Req *ios2)
{
	struct EthHdr *eh = bd->bd_Buffer;

	if (ios2->ios2_Req.io_Flags & SANA2IOF_RAW)
	{
		copyfrom(bd->bd_Buffer, ios2->ios2_Data, ios2->ios2_DataLength);
		bd->bd_Length = ios2->ios2_DataLength;
	}
	else
	{
		eh->eh_Type = ios2->ios2_PacketType;
		memcpy(eh->eh_Src, macaddr, sizeof(macaddr));
		memcpy(eh->eh_Dst, ios2->ios2_DstAddr, MACADDR_SIZE);
		copyfrom(&eh[1], ios2->ios2_Data, ios2->ios2_DataLength);
		bd->bd_Length = ios2->ios2_DataLength + sizeof(struct EthHdr);
	}

	ios2->ios2_Req.io_Error = 0;
	ReplyMsg(&ios2->ios2_Req.io_Message);
}

static void handle_a314_reply(struct A314_IORequest *ior)
{
	if (ior == &write_ior)
	{
		pending_a314_write = FALSE;

		if (ior->a314_Request.io_Error == A314_WRITE_OK)
		{
			// Start new write later.
		}
		else // A314_WRITE_RESET
		{
			// TODO: Handle. What if pi-side is shutting down.
		}
	}
	else if (ior == &read_ior)
	{
		pending_a314_read = FALSE;

		if (ior->a314_Request.io_Error == A314_READ_OK)
		{
			if (a314_read_buf.sm_Kind == WRITE_FRAME_RES)
			{
				struct BufDesc *bd = (struct BufDesc *)RemHead(&et_wbuf_pending_list);
				AddTail(&et_wbuf_free_list, (struct Node *)bd);
			}
			else // READ_FRAME_RES
			{
				struct BufDesc *bd = (struct BufDesc *)RemHead(&et_rbuf_pending_list);
				bd->bd_Length = a314_read_buf.sm_Length;
				AddTail(&et_rbuf_has_data_list, (struct Node *)bd);
			}

			send_a314_cmd(&read_ior, A314_READ, (void *)&a314_read_buf, sizeof(a314_read_buf));
			pending_a314_read = TRUE;
		}
		else // A314_READ_RESET
		{
			// TODO: Handle. What if pi-side is shutting down.
		}
	}
	else if (ior == &reset_ior)
	{
		pending_a314_reset = FALSE;
	}
}

static struct IOSana2Req *remove_matching_rbuf(ULONG type)
{
	struct Node *node = ut_rbuf_list.lh_Head;
	while (node->ln_Succ)
	{
		struct IOSana2Req *ios2 = (struct IOSana2Req *)node;
		if (ios2->ios2_PacketType == type)
		{
			Remove(node);
			return ios2;
		}
		node = node->ln_Succ;
	}
	return NULL;
}

static void complete_read_reqs()
{
	struct Node *node = et_rbuf_has_data_list.lh_Head;
	if (!node->ln_Succ)
		return;

	Forbid();
	while (node->ln_Succ)
	{
		struct BufDesc *bd = (struct BufDesc *)node;
		struct EthHdr *eh = (struct EthHdr *)bd->bd_Buffer;

		node = node->ln_Succ;

		struct IOSana2Req *ios2 = remove_matching_rbuf(eh->eh_Type);
		if (ios2)
		{
			copy_from_bd_and_reply(ios2, bd);

			Remove((struct Node *)bd);
			AddTail(&et_rbuf_free_list, (struct Node *)bd);
		}
	}
	Permit();
}

static void maybe_write_req()
{
	if (pending_a314_write)
		return;

	BOOL free_et_wbuf = et_wbuf_free_list.lh_Head->ln_Succ != NULL;
	BOOL idle_et_rbuf = et_rbuf_free_list.lh_Head->ln_Succ != NULL;

	Forbid();

	BOOL waiting_ut_wbuf = ut_wbuf_list.lh_Head->ln_Succ != NULL;

	BOOL want_wbuf = free_et_wbuf && waiting_ut_wbuf;
	BOOL want_rbuf = idle_et_rbuf;

	if (!want_rbuf && !want_wbuf)
	{
		Permit();
		return;
	}

	short next_req_kind = 0;

	if (last_req_kind == WRITE_FRAME_REQ)
		next_req_kind = want_rbuf ? READ_FRAME_REQ : WRITE_FRAME_REQ;
	else
		next_req_kind = want_wbuf ? WRITE_FRAME_REQ : READ_FRAME_REQ;

	struct IOSana2Req *ios2 = NULL;
	if (next_req_kind == WRITE_FRAME_REQ)
		ios2 = (struct IOSana2Req*)RemHead((struct List *)&ut_wbuf_list);

	Permit();

	struct BufDesc *bd;

	if (next_req_kind == READ_FRAME_REQ)
	{
		bd = (struct BufDesc *)RemHead(&et_rbuf_free_list);
		bd->bd_Length = RAW_MTU;
		AddTail(&et_rbuf_pending_list, (struct Node *)&bd->bd_Node);
	}
	else // WRITE_FRAME_REQ
	{
		bd = (struct BufDesc *)RemHead(&et_wbuf_free_list);
		copy_to_bd_and_reply(bd, ios2);
		AddTail(&et_wbuf_pending_list, (struct Node *)bd);
	}

	a314_write_buf.sm_Address = TranslateAddressA314(bd->bd_Buffer);
	a314_write_buf.sm_Length = bd->bd_Length;
	a314_write_buf.sm_Kind = next_req_kind;

	send_a314_cmd(&write_ior, A314_WRITE, (void *)&a314_write_buf, sizeof(a314_write_buf));
	pending_a314_write = TRUE;

	last_req_kind = next_req_kind;
}

void device_process_run()
{
	ULONG sana2_signal = AllocSignal(-1);
	sana2_sigmask = 1UL << sana2_signal;

	ULONG shutdown_signal = AllocSignal(-1);
	shutdown_sigmask = 1UL << shutdown_signal;

	a314_mp.mp_SigBit = AllocSignal(-1);
	a314_mp.mp_SigTask = FindTask(NULL);

	do_a314_cmd(&reset_ior, A314_CONNECT, (char *)service_name, strlen(service_name));
	device_start_error = reset_ior.a314_Request.io_Error == A314_CONNECT_OK ? 0 : -1;

	Signal((struct Task *)init_task, SIGF_SINGLE);

	if (device_start_error)
		return;

	ULONG a314_sigmask = 1UL << a314_mp.mp_SigBit;

	send_a314_cmd(&read_ior, A314_READ, (void *)&a314_read_buf, sizeof(a314_read_buf));
	pending_a314_read = TRUE;

	BOOL shutting_down = FALSE;

	while (TRUE)
	{
		complete_read_reqs();
		maybe_write_req();

		if (shutting_down && !pending_a314_read && !pending_a314_write && !pending_a314_reset)
			break;

		ULONG sigs = Wait(a314_sigmask | sana2_sigmask | shutdown_sigmask);

		if ((sigs & shutdown_sigmask) && !shutting_down)
		{
			send_a314_cmd(&reset_ior, A314_RESET, NULL, 0);
			pending_a314_reset = TRUE;
			shutting_down = TRUE;
		}

		if (sigs & a314_sigmask)
		{
			struct A314_IORequest *ior;
			while ((ior = (struct A314_IORequest *)GetMsg(&a314_mp)))
				handle_a314_reply(ior);
		}
	}

	Signal((struct Task *)init_task, SIGF_SINGLE);
}

static struct TagItem *FindTagItem(Tag tagVal, struct TagItem *tagList)
{
	struct TagItem *ti = tagList;
	while (ti && ti->ti_Tag != tagVal)
	{
		switch (ti->ti_Tag)
		{
		case TAG_DONE:
			return NULL;
		case TAG_MORE:
			ti = (struct TagItem *)ti->ti_Data;
			break;
		case TAG_SKIP:
			ti += ti->ti_Data + 1;
			break;
		case TAG_IGNORE:
		default:
			ti++;
			break;
		}
	}
	return ti;
}

static ULONG GetTagData(Tag tagVal, ULONG defaultData, struct TagItem *tagList)
{
	struct TagItem *ti = FindTagItem(tagVal, tagList);
	return ti ? ti->ti_Data : defaultData;
}

static void __attribute__((used)) open(struct Library *dev asm("a6"), struct IOSana2Req *ios2 asm("a1"), uint32_t unitnum asm("d0"), uint32_t flags asm("d1"))
{
	kprintf("We opening this shit.\n");
	ios2->ios2_Req.io_Error = IOERR_OPENFAIL;
	ios2->ios2_Req.io_Message.mn_Node.ln_Type = NT_REPLYMSG;

	if (unitnum != 0 || dev->lib_OpenCnt)
		return;

	dev->lib_OpenCnt++;

	kprintf("Try the copyfrom crap.\n");
	copyfrom = (buf_copy_func_t)GetTagData(S2_CopyFromBuff, 0, (struct TagItem *)ios2->ios2_BufferManagement);
	copyto = (buf_copy_func_t)GetTagData(S2_CopyToBuff, 0, (struct TagItem *)ios2->ios2_BufferManagement);
	ios2->ios2_BufferManagement = (void *)0xdeadbeefUL;

	kprintf("Memsetting some shit.\n");
	memset(&a314_mp, 0, sizeof(a314_mp));
	a314_mp.mp_Node.ln_Pri = 0;
	a314_mp.mp_Node.ln_Type = NT_MSGPORT;
	a314_mp.mp_Node.ln_Name = (char *)device_name;
	a314_mp.mp_Flags = PA_SIGNAL;
	NewList(&a314_mp.mp_MsgList);

	kprintf("Memsetting more shit.\n");
	memset(&write_ior, 0, sizeof(write_ior));
	write_ior.a314_Request.io_Message.mn_ReplyPort = &a314_mp;
	write_ior.a314_Request.io_Message.mn_Length = sizeof(write_ior);
	write_ior.a314_Request.io_Message.mn_Node.ln_Type = NT_REPLYMSG;

	kprintf("Opendevice.\n");
	A314Base = NULL;
	if (OpenDevice((STRPTR)a314_device_name, 0, (struct IORequest *)&write_ior, 0))
		goto error;

	A314Base = &(write_ior.a314_Request.io_Device->dd_Library);

	kprintf("Copy memory.\n");
	memcpy(&read_ior, &write_ior, sizeof(read_ior));
	memcpy(&reset_ior, &write_ior, sizeof(reset_ior));

	kprintf("Making datestamps.\n");
	struct DateStamp ds;
	DateStamp(&ds);
	a314_socket = (ds.ds_Minute * 60 * TICKS_PER_SECOND) + ds.ds_Tick;

	last_req_kind = WRITE_FRAME_REQ;

	kprintf("Making lists.\n");
	NewList((struct List *)&ut_rbuf_list);
	NewList((struct List *)&ut_wbuf_list);

	NewList(&et_rbuf_free_list);
	NewList(&et_rbuf_pending_list);
	NewList(&et_rbuf_has_data_list);

	NewList(&et_wbuf_free_list);
	NewList(&et_wbuf_pending_list);

	kprintf("Memzero buffers.\n");
	for (int i = 0; i < ET_BUF_CNT; i++)
		memset(&et_bufs[i], 0, sizeof(struct BufDesc));

	kprintf("Add tails.\n");
	for (int i = 0; i < ET_BUF_CNT; i++)
	{
		struct BufDesc *bd = &et_bufs[i];

		bd->bd_Buffer = AllocMem(RAW_MTU, MEMF_FAST);
		if (!bd->bd_Buffer)
			goto error;

		if (i < ET_RBUF_CNT)
			AddTail(&et_rbuf_free_list, (struct Node*)&bd->bd_Node);
		else
			AddTail(&et_wbuf_free_list, (struct Node*)&bd->bd_Node);
	}

	kprintf("Find task.\n");
	init_task = FindTask(NULL);

	kprintf("Do msgport.\n");
	struct MsgPort *device_mp = CreateProc((STRPTR)device_name, TASK_PRIO, ((ULONG)&device_process_seglist) >> 2, 2048);
	if (!device_mp)
		goto error;

	kprintf("Process thing.\n");
	device_process = (struct Process *)((char *)device_mp - sizeof(struct Task));

	kprintf("Waitf.\n");
	Wait(SIGF_SINGLE);
	kprintf("Waitedf.\n");

	if (device_start_error) {
		kprintf("Device start error.\n");
		goto error;
	}

	kprintf("Everything ok?\n");
	ios2->ios2_Req.io_Error = 0;
	return;

error:
	kprintf("Error small farts.\n");
	for (int i = ET_BUF_CNT - 1; i >= 0; i--)
		if (et_bufs[i].bd_Buffer)
			FreeMem(et_bufs[i].bd_Buffer, RAW_MTU);

	if (A314Base)
	{
		CloseDevice((struct IORequest *)&write_ior);
		A314Base = NULL;
	}

	dev->lib_OpenCnt--;
}

static uint8_t* __attribute__((used)) close(struct Library *dev asm("a6"), struct IOSana2Req *ios2 asm("a1"))
{
	kprintf("Close.\n");
	init_task = FindTask(NULL);
	Signal(&device_process->pr_Task, shutdown_sigmask);
	Wait(SIGF_SINGLE);

	for (int i = ET_BUF_CNT - 1; i >= 0; i--)
		FreeMem(et_bufs[i].bd_Buffer, RAW_MTU);

	CloseDevice((struct IORequest *)&write_ior);
	A314Base = NULL;

	ios2->ios2_Req.io_Device = NULL;
	ios2->ios2_Req.io_Unit = NULL;

	dev->lib_OpenCnt--;

	if (dev->lib_OpenCnt == 0 && (dev->lib_Flags & LIBF_DELEXP))
		return expunge(dev);

	return 0;
}

static void device_query(struct IOSana2Req *req)
{
	struct Sana2DeviceQuery *query;

	query = req->ios2_StatData;
	query->DevQueryFormat = 0;
	query->DeviceLevel = 0;

	if (query->SizeAvailable >= 18)
		query->AddrFieldSize = MACADDR_SIZE * 8;

	if (query->SizeAvailable >= 22)
		query->MTU = ETH_MTU;

	if (query->SizeAvailable >= 26)
		query->BPS = NIC_BPS;

	if (query->SizeAvailable >= 30)
		query->HardwareType = S2WireType_Ethernet;

	query->SizeSupplied = query->SizeAvailable < 30 ? query->SizeAvailable : 30;
}

static void __attribute__((used)) begin_io(struct Library *dev asm("a6"), struct IOSana2Req *ios2 asm("a1"))
{
	kprintf("BeginIO.\n");
	ios2->ios2_Req.io_Error = S2ERR_NO_ERROR;
	ios2->ios2_WireError = S2WERR_GENERIC_ERROR;

	switch (ios2->ios2_Req.io_Command)
	{
	case CMD_READ:
		if (!ios2->ios2_BufferManagement)
		{
			ios2->ios2_Req.io_Error = S2ERR_BAD_ARGUMENT;
			ios2->ios2_WireError = S2WERR_BUFF_ERROR;
			break;
		}

		Forbid();
		AddTail((struct List *)&ut_rbuf_list, &ios2->ios2_Req.io_Message.mn_Node);
		Permit();

		ios2->ios2_Req.io_Flags &= ~SANA2IOF_QUICK;
		ios2 = NULL;

		Signal(&device_process->pr_Task, sana2_sigmask);
		break;

	case S2_BROADCAST:
		memset(ios2->ios2_DstAddr, 0xff, MACADDR_SIZE);
		/* Fall through */

	case CMD_WRITE:
		if (((ios2->ios2_Req.io_Flags & SANA2IOF_RAW) != 0 && ios2->ios2_DataLength > RAW_MTU) ||
			((ios2->ios2_Req.io_Flags & SANA2IOF_RAW) == 0 && ios2->ios2_DataLength > ETH_MTU))
		{
			ios2->ios2_Req.io_Error = S2ERR_MTU_EXCEEDED;
			break;
		}

		if (!ios2->ios2_BufferManagement)
		{
			ios2->ios2_Req.io_Error = S2ERR_BAD_ARGUMENT;
			ios2->ios2_WireError = S2WERR_BUFF_ERROR;
			break;
		}

		Forbid();
		AddTail((struct List *)&ut_wbuf_list, &ios2->ios2_Req.io_Message.mn_Node);
		Permit();

		ios2->ios2_Req.io_Flags &= ~SANA2IOF_QUICK;
		ios2 = NULL;

		Signal(&device_process->pr_Task, sana2_sigmask);
		break;

	case S2_ONLINE:
	case S2_OFFLINE:
	case S2_CONFIGINTERFACE:
		break;
	case S2_GETSTATIONADDRESS:
		memcpy(ios2->ios2_SrcAddr, macaddr, sizeof(macaddr));
		memcpy(ios2->ios2_DstAddr, macaddr, sizeof(macaddr));
		break;
	case S2_DEVICEQUERY:
		device_query(ios2);
		break;

	case S2_ONEVENT:
	case S2_TRACKTYPE:
	case S2_UNTRACKTYPE:
	case S2_GETTYPESTATS:
	case S2_READORPHAN:
	case S2_GETGLOBALSTATS:
	case S2_GETSPECIALSTATS:
		break;

	default:
		ios2->ios2_Req.io_Error = IOERR_NOCMD;
		ios2->ios2_WireError = S2WERR_GENERIC_ERROR;
		break;
	}

	if (ios2)
	{
		if (ios2->ios2_Req.io_Flags & SANA2IOF_QUICK)
			ios2->ios2_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;
		else
			ReplyMsg(&ios2->ios2_Req.io_Message);
	}
}

static void remove_from_list(struct List *list, struct Node *node)
{
	for (struct Node *n = list->lh_Head; n->ln_Succ; n = n->ln_Succ)
	{
		if (n == node)
		{
			Remove(n);
			return;
		}
	}
}

static uint32_t __attribute__((used)) abort_io(struct Library *dev asm("a6"), struct IOSana2Req *ios2 asm("a1"))
{
	kprintf("AbortIO.\n");
	Forbid();
	remove_from_list((struct List *)&ut_rbuf_list, &ios2->ios2_Req.io_Message.mn_Node);
	remove_from_list((struct List *)&ut_wbuf_list, &ios2->ios2_Req.io_Message.mn_Node);
	Permit();

	ios2->ios2_Req.io_Error = IOERR_ABORTED;
	ios2->ios2_WireError = 0;
	ReplyMsg(&ios2->ios2_Req.io_Message);

	return 0;
}

static ULONG device_vectors[] =
{
	(ULONG)open,
	(ULONG)close,
	(ULONG)expunge,
	0,
	(ULONG)begin_io,
	(ULONG)abort_io,
	-1,
};

ULONG auto_init_tables[] =
{
	sizeof(struct Library),
	(ULONG)device_vectors,
	0,
	(ULONG)init_device,
};
