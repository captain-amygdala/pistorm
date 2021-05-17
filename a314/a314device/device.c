/*
 * Copyright 2020-2021 Niklas Ekström
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/devices.h>
#include <exec/errors.h>
#include <exec/ports.h>
#include <libraries/dos.h>
#include <proto/exec.h>

#include "device.h"
#include "a314.h"
#include "startup.h"
#include "fix_mem_region.h"

char device_name[] = A314_NAME;
char id_string[] = A314_NAME " 1.1 (28 Nov 2020)";

struct ExecBase *SysBase;
BPTR saved_seg_list;
BOOL running = FALSE;

static struct Library *init_device(__reg("a6") struct ExecBase *sys_base, __reg("a0") BPTR seg_list, __reg("d0") struct Library *dev)
{
	SysBase = *(struct ExecBase **)4;
	saved_seg_list = seg_list;

	// We are being called from InitResident() in initializers.asm.
	// MakeLibrary() was executed before we got here.

	dev->lib_Node.ln_Type = NT_DEVICE;
	dev->lib_Node.ln_Name = device_name;
	dev->lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
	dev->lib_Version = 1;
	dev->lib_Revision = 0;
	dev->lib_IdString = (APTR)id_string;

	// AddDevice() is executed after we return.
	return dev;
}

static BPTR expunge(__reg("a6") struct Library *dev)
{
	// There is currently no support for unloading a314.device.

	if (TRUE) //dev->lib_OpenCnt != 0)
	{
		dev->lib_Flags |= LIBF_DELEXP;
		return 0;
	}

	/*
	BPTR seg_list = saved_seg_list;
	Remove(&dev->lib_Node);
	FreeMem((char *)dev - dev->lib_NegSize, dev->lib_NegSize + dev->lib_PosSize);
	return seg_list;
	*/
}

static void open(__reg("a6") struct Library *dev, __reg("a1") struct A314_IORequest *ior, __reg("d0") ULONG unitnum, __reg("d1") ULONG flags)
{
	dev->lib_OpenCnt++;

	if (dev->lib_OpenCnt == 1 && !running)
	{
		if (!task_start())
		{
			ior->a314_Request.io_Error = IOERR_OPENFAIL;
			ior->a314_Request.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
			dev->lib_OpenCnt--;
			return;
		}
		running = TRUE;
	}

	ior->a314_Request.io_Error = 0;
	ior->a314_Request.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
}

static BPTR close(__reg("a6") struct Library *dev, __reg("a1") struct A314_IORequest *ior)
{
	ior->a314_Request.io_Device = NULL;
	ior->a314_Request.io_Unit = NULL;

	dev->lib_OpenCnt--;

	if (dev->lib_OpenCnt == 0 && (dev->lib_Flags & LIBF_DELEXP))
		return expunge(dev);

	return 0;
}

static void begin_io(__reg("a6") struct Library *dev, __reg("a1") struct A314_IORequest *ior)
{
	PutMsg(&task_mp, (struct Message *)ior);
	ior->a314_Request.io_Flags &= ~IOF_QUICK;
}

static ULONG abort_io(__reg("a6") struct Library *dev, __reg("a1") struct A314_IORequest *ior)
{
	// There is currently no support for aborting an IORequest.
	return IOERR_NOCMD;
}

static ULONG device_vectors[] =
{
	(ULONG)open,
	(ULONG)close,
	(ULONG)expunge,
	0,
	(ULONG)begin_io,
	(ULONG)abort_io,
	(ULONG)translate_address_a314,
	-1,
};

ULONG auto_init_tables[] =
{
	sizeof(struct Library),
	(ULONG)device_vectors,
	0,
	(ULONG)init_device,
};
