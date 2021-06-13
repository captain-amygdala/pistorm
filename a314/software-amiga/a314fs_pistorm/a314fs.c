/*
 * Copyright (c) 2018-2021 Niklas Ekström
 */

#include <exec/types.h>
#include <exec/ports.h>
#include <exec/nodes.h>
#include <exec/libraries.h>

#include <devices/timer.h>

#include <libraries/dos.h>
#include <libraries/dosextens.h>
#include <libraries/filehandler.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include <string.h>
#include <stdarg.h>

#include "../../a314device/a314.h"
#include "../../a314device/proto_a314.h"

#include "messages.h"

#define MKBADDRU(x) (((ULONG)x) >> 2)

#define ID_314_DISK (('3' << 24) | ('1' << 16) | ('4' << 8))

#define REQ_RES_BUF_SIZE 256
#define BUFFER_SIZE 4096

// Not defined if using NDK13
#ifndef ACTION_EXAMINE_FH
#define ACTION_EXAMINE_FH 1034
#endif

#ifndef ACTION_SAME_LOCK
#define ACTION_SAME_LOCK 40
#endif

// Grab a reserved action type which seems to be unused
#define ACTION_UNSUPPORTED 65535

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct MsgPort *mp;

struct DeviceList *my_volume;
char default_volume_name[] = "\006PiDisk";

char device_name[32]; // "\004PI0:"

struct MsgPort *timer_mp;
struct timerequest *tr;

struct MsgPort *a314_mp;
struct A314_IORequest *a314_ior;

struct Library *A314Base;

long socket;

// These are allocated in A314 memory.
char *request_buffer = NULL;
char *data_buffer = NULL;

void MyNewList(struct List *l)
{
    l->lh_Head = (struct Node *)&(l->lh_Tail);
    l->lh_Tail = NULL;
    l->lh_TailPred = (struct Node *)&(l->lh_Head);
}

struct MsgPort *MyCreatePort(char *name, long pri)
{
	int sigbit = AllocSignal(-1);
	if (sigbit == -1)
		return NULL;

	struct MsgPort *port = (struct MsgPort *)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
	if (!port)
	{
		FreeSignal(sigbit);
		return NULL;
	}

	port->mp_Node.ln_Name = name;
	port->mp_Node.ln_Pri = pri;
	port->mp_Node.ln_Type = NT_MSGPORT;
	port->mp_Flags = PA_SIGNAL;
	port->mp_SigBit = sigbit;
	port->mp_SigTask = FindTask(NULL);

	if (name)
		AddPort(port);
	else
		MyNewList(&(port->mp_MsgList));

	return port;
}

struct IORequest *MyCreateExtIO(struct MsgPort *ioReplyPort, ULONG size)
{
	if (!ioReplyPort)
		return NULL;

	struct IORequest *ioReq = (struct IORequest *)AllocMem(size, MEMF_PUBLIC | MEMF_CLEAR);
	if (!ioReq)
		return NULL;

	ioReq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
	ioReq->io_Message.mn_Length = size;
	ioReq->io_Message.mn_ReplyPort = ioReplyPort;

	return ioReq;
}

#define DEBUG 0

#if !DEBUG
void dbg_init()
{
}

void dbg(const char* fmt, ...)
{
}
#else
struct FileHandle *dbg_log;
struct MsgPort *dbg_replyport;
struct StandardPacket *dbg_sp;
char dbg_buf[256];

void dbg_init()
{
	dbg_log = (struct FileHandle *)BADDR(Open("CON:200/0/440/256/a314fs", MODE_NEWFILE));
	dbg_replyport = MyCreatePort(NULL, 0);
	dbg_sp = (struct StandardPacket *)AllocMem(sizeof(struct StandardPacket), MEMF_PUBLIC | MEMF_CLEAR);
}

void dbg_out(int l)
{
	dbg_sp->sp_Msg.mn_Node.ln_Name = (char *)&(dbg_sp->sp_Pkt);
	dbg_sp->sp_Pkt.dp_Link = &(dbg_sp->sp_Msg);
	dbg_sp->sp_Pkt.dp_Port = dbg_replyport;
	dbg_sp->sp_Pkt.dp_Type = ACTION_WRITE;
	dbg_sp->sp_Pkt.dp_Arg1 = (long)dbg_log->fh_Arg1;
	dbg_sp->sp_Pkt.dp_Arg2 = (long)dbg_buf;
	dbg_sp->sp_Pkt.dp_Arg3 = (long)l - 1;

	PutMsg((struct MsgPort *)dbg_log->fh_Type, (struct Message *)dbg_sp);
	WaitPort(dbg_replyport);
	GetMsg(dbg_replyport);
}

void dbg(const char* fmt, ...)
{
	char numbuf[16];

	const char *p = fmt;
	char *q = dbg_buf;

	va_list args;
	va_start(args, fmt);
	while (*p != 0)
	{
		char c = *p++;
		if (c == '$')
		{
			c = *p++;
			if (c == 'b')
			{
				UBYTE x = va_arg(args, UBYTE);
				*q++ = '$';
				for (int i = 0; i < 2; i++)
				{
					int ni = (x >> ((1 - i) * 4)) & 0xf;
					*q++ = (ni >= 10) ? ('a' + (ni - 10)) : ('0' + ni);
				}
			}
			else if (c == 'w')
			{
				UWORD x = va_arg(args, UWORD);
				*q++ = '$';
				for (int i = 0; i < 4; i++)
				{
					int ni = (x >> ((3 - i) * 4)) & 0xf;
					*q++ = (ni >= 10) ? ('a' + (ni - 10)) : ('0' + ni);
				}
			}
			else if (c == 'l')
			{
				ULONG x = va_arg(args, ULONG);
				*q++ = '$';
				for (int i = 0; i < 8; i++)
				{
					int ni = (x >> ((7 - i) * 4)) & 0xf;
					*q++ = (ni >= 10) ? ('a' + (ni - 10)) : ('0' + ni);
				}
			}
			else if (c == 'S')
			{
				unsigned char *s = (unsigned char *)va_arg(args, ULONG);
				int l = *s++;
				for (int i = 0; i < l; i++)
					*q++ = *s++;
			}
			else if (c == 's')
			{
				unsigned char *s = (unsigned char *)va_arg(args, ULONG);
				while (*s)
					*q++ = *s++;
				*q++ = 0;
			}
		}
		else
		{
			*q++ = c;
		}
	}
	*q++ = 0;

	va_end(args);

	dbg_out(q - dbg_buf);
}
#endif

char *DosAllocMem(int len)
{
	long *p = (long *)AllocMem(len + 4, MEMF_PUBLIC | MEMF_CLEAR);
	*p++ = len;
	return((char *)p);
}

void DosFreeMem(char *p)
{
	long *lp = (long *)p;
	long len = *--lp;
	FreeMem((char *)lp, len);
}

void reply_packet(struct DosPacket *dp)
{
	struct MsgPort *reply_port = dp->dp_Port;
	dp->dp_Port = mp;
	PutMsg(reply_port, dp->dp_Link);
}

// Routines for talking to the A314 driver.
LONG a314_cmd_wait(UWORD command, char *buffer, int length)
{
	a314_ior->a314_Request.io_Command = command;
	a314_ior->a314_Request.io_Error = 0;
	a314_ior->a314_Socket = socket;
	a314_ior->a314_Buffer = buffer;
	a314_ior->a314_Length = length;
	return DoIO((struct IORequest *)a314_ior);
}

LONG a314_connect(char *name)
{
	struct DateStamp ds;
	DateStamp(&ds);
	socket = ds.ds_Tick;
	return a314_cmd_wait(A314_CONNECT, name, strlen(name));
}

LONG a314_read(char *buf, int length)
{
	return a314_cmd_wait(A314_READ, buf, length);
}

LONG a314_write(char *buf, int length)
{
	return a314_cmd_wait(A314_WRITE, buf, length);
}

LONG a314_eos()
{
	return a314_cmd_wait(A314_EOS, NULL, 0);
}

LONG a314_reset()
{
	return a314_cmd_wait(A314_RESET, NULL, 0);
}

void create_and_add_volume()
{
	my_volume = (struct DeviceList *)DosAllocMem(sizeof(struct DeviceList));
	my_volume->dl_Name = MKBADDRU(default_volume_name);
	my_volume->dl_Type = DLT_VOLUME;
	my_volume->dl_Task = mp;
	my_volume->dl_DiskType = ID_314_DISK;
	DateStamp(&my_volume->dl_VolumeDate);

	struct RootNode *root = (struct RootNode *)DOSBase->dl_Root;
	struct DosInfo *info = (struct DosInfo *)BADDR(root->rn_Info);

	Forbid();
	my_volume->dl_Next = info->di_DevInfo;
	info->di_DevInfo = MKBADDRU(my_volume);
	Permit();
}

void startup_fs_handler(struct DosPacket *dp)
{
	unsigned char *name = (unsigned char *)BADDR(dp->dp_Arg1);
	struct FileSysStartupMsg *fssm = (struct FileSysStartupMsg *)BADDR(dp->dp_Arg2);
	struct DeviceNode *node = (struct DeviceNode *)BADDR(dp->dp_Arg3);

	memcpy(device_name, name, *name + 1);
	device_name[*name + 1] = 0;

	node->dn_Task = mp;

	dp->dp_Res1 = DOSTRUE;
	dp->dp_Res2 = 0;
	reply_packet(dp);

	dbg_init();

	dbg("ACTION_STARTUP\n");
	dbg("  device_name = $S\n", (ULONG)device_name);
	dbg("  fssm = $l\n", (ULONG)fssm);
	dbg("  node = $l\n", (ULONG)node);

	timer_mp = MyCreatePort(NULL, 0);
	tr = (struct timerequest *)MyCreateExtIO(timer_mp, sizeof(struct timerequest));
	if (OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tr, 0) != 0)
	{
		// If this happens, there's nothing we can do.
		// For now, assume this does not happen.
		dbg("Fatal error: unable to open timer.device\n");
		return;
	}

	a314_mp = MyCreatePort(NULL, 0);
	a314_ior = (struct A314_IORequest *)MyCreateExtIO(a314_mp, sizeof(struct A314_IORequest));
	if (OpenDevice(A314_NAME, 0, (struct IORequest *)a314_ior, 0) != 0)
	{
		// If this fails, there's nothing we can do.
		// For now, assume this does not happen.
		dbg("Fatal error: unable to open a314.device\n");
		return;
	}

	A314Base = &(a314_ior->a314_Request.io_Device->dd_Library);

	if (a314_connect("a314fs") != A314_CONNECT_OK)
	{
		dbg("Fatal error: unable to connect to a314fs on rasp\n");
		// This COULD happen.
		// If it DOES happen, we just wait for a bit and try again.

		// TODO: Have to use timer.device to set a timer for ~2 seconds and then try connecting again.
		return;
	}

	request_buffer = AllocMem(REQ_RES_BUF_SIZE, MEMF_FAST);
	data_buffer = AllocMem(BUFFER_SIZE, MEMF_FAST);

	// We can assume that we arrive here, and have a stream to the Pi side, to where we can transfer data.
	create_and_add_volume();

	dbg("Startup successful\n");

	// If we end up having problems with the connections, treat the disc as ejected, and try inserting it again in two seconds.
	// TODO: This is not currently handled.
}

void wait_for_response()
{
	for (unsigned long i = 0; 1; i++)
	{
		if (*request_buffer)
		{
			dbg("--Got response after $l sleeps\n", i);
			return;
		}

		tr->tr_node.io_Command = TR_ADDREQUEST;
		tr->tr_node.io_Message.mn_ReplyPort = timer_mp;
		tr->tr_time.tv_secs = 0;
		tr->tr_time.tv_micro = 1000;
		DoIO((struct IORequest *)tr);
	}
}

void write_req_and_wait_for_res(int len)
{
	ULONG buf[2] = {TranslateAddressA314(request_buffer), len};
	a314_write((char *)&buf[0], 8);
	//wait_for_response();
	a314_read((char *)&buf[0], 8);
}

struct FileLock *create_and_add_file_lock(long key, long mode)
{
	struct FileLock *lock = (struct FileLock *)DosAllocMem(sizeof(struct FileLock));

	lock->fl_Key = key;
	lock->fl_Access = mode;
	lock->fl_Task = mp;
	lock->fl_Volume = MKBADDRU(my_volume);

	Forbid();
	lock->fl_Link = my_volume->dl_Lock;
	my_volume->dl_Lock = MKBADDRU(lock);
	Permit();

	return lock;
}

void action_locate_object(struct DosPacket *dp)
{
	struct FileLock *parent = (struct FileLock *)BADDR(dp->dp_Arg1);
	unsigned char *name = (unsigned char *)BADDR(dp->dp_Arg2);
	long mode = dp->dp_Arg3;

	dbg("ACTION_LOCATE_OBJECT\n");
	dbg("  parent lock = $l\n", parent);
	dbg("  name = $S\n", name);
	dbg("  mode = $s\n", mode == SHARED_LOCK ? "SHARED_LOCK" : "EXCLUSIVE_LOCK");

	struct LocateObjectRequest *req = (struct LocateObjectRequest *)request_buffer;
	req->has_response = 0;
	req->type = dp->dp_Type;
	req->key = parent == NULL ? 0 : parent->fl_Key;
	req->mode = mode;

	int nlen = *name;
	memcpy(req->name, name, nlen + 1);

	write_req_and_wait_for_res(sizeof(struct LocateObjectRequest) + nlen);

	struct LocateObjectResponse *res = (struct LocateObjectResponse *)request_buffer;
	if (!res->success)
	{
		dbg("  Failed, error code $l\n", (LONG)res->error_code);
		dp->dp_Res1 = DOSFALSE;
		dp->dp_Res2 = res->error_code;
	}
	else
	{
		struct FileLock *lock = create_and_add_file_lock(res->key, mode);

		dbg("  Returning lock $l\n", lock);
		dp->dp_Res1 = MKBADDRU(lock);
		dp->dp_Res2 = 0;
	}

	reply_packet(dp);
}

void action_free_lock(struct DosPacket *dp)
{
	ULONG arg1 = dp->dp_Arg1;
	struct FileLock *lock = (struct FileLock *)BADDR(dp->dp_Arg1);

	dbg("ACTION_FREE_LOCK\n");
	dbg("  lock = $l\n", lock);

	if (lock != NULL)
	{
		struct FreeLockRequest *req = (struct FreeLockRequest *)request_buffer;
		req->has_response = 0;
		req->type = dp->dp_Type;
		req->key = lock->fl_Key;

		write_req_and_wait_for_res(sizeof(struct FreeLockRequest));

		// Ignore the response. Must succeed.
		//struct FreeLockResponse *res = (struct FreeLockResponse *)request_buffer;

		Forbid();
		if (my_volume->dl_Lock == arg1)
			my_volume->dl_Lock = lock->fl_Link;
		else
		{
			struct FileLock *prev = (struct FileLock *)BADDR(my_volume->dl_Lock);
			while (prev->fl_Link != arg1)
				prev = (struct FileLock *)BADDR(prev->fl_Link);
			prev->fl_Link = lock->fl_Link;
		}
		Permit();
		DosFreeMem((char *)lock);
	}

	dp->dp_Res1 = DOSTRUE;
	dp->dp_Res2 = 0;
	reply_packet(dp);
}

void action_copy_dir(struct DosPacket *dp)
{
	struct FileLock *parent = (struct FileLock *)BADDR(dp->dp_Arg1);

	dbg("ACTION_COPY_DIR\n");
	dbg("  lock to duplicate = $l\n", parent);

	if (parent == NULL)
	{
		dp->dp_Res1 = 0;
		dp->dp_Res2 = 0;
	}
	else
	{
		struct CopyDirRequest *req = (struct CopyDirRequest *)request_buffer;
		req->has_response = 0;
		req->type = dp->dp_Type;
		req->key = parent->fl_Key;

		write_req_and_wait_for_res(sizeof(struct CopyDirRequest));

		struct CopyDirResponse *res = (struct CopyDirResponse *)request_buffer;
		if (!res->success)
		{
			dbg("  Failed, error code $l\n", (LONG)res->error_code);
			dp->dp_Res1 = DOSFALSE;
			dp->dp_Res2 = res->error_code;
		}
		else
		{
			struct FileLock *lock = create_and_add_file_lock(res->key, parent->fl_Access);

			dbg("  Returning lock $l\n", lock);
			dp->dp_Res1 = MKBADDRU(lock);
			dp->dp_Res2 = 0;
		}
	}

	reply_packet(dp);
}

void action_parent(struct DosPacket *dp)
{
	struct FileLock *prev_lock = (struct FileLock *)BADDR(dp->dp_Arg1);

	dbg("ACTION_PARENT\n");
	dbg("  lock = $l\n", prev_lock);

	if (prev_lock == NULL)
	{
		dp->dp_Res1 = DOSFALSE;
		dp->dp_Res2 = ERROR_INVALID_LOCK;
	}
	else
	{
		struct ParentRequest *req = (struct ParentRequest *)request_buffer;
		req->has_response = 0;
		req->type = dp->dp_Type;
		req->key = prev_lock->fl_Key;

		write_req_and_wait_for_res(sizeof(struct ParentRequest));

		struct ParentResponse *res = (struct ParentResponse *)request_buffer;
		if (!res->success)
		{
			dbg("  Failed, error code $l\n", (LONG)res->error_code);
			dp->dp_Res1 = DOSFALSE;
			dp->dp_Res2 = res->error_code;
		}
		else if (res->key == 0)
		{
			dp->dp_Res1 = 0;
			dp->dp_Res2 = 0;
		}
		else
		{
			struct FileLock *lock = create_and_add_file_lock(res->key, SHARED_LOCK);

			dbg("  Returning lock $l\n", lock);
			dp->dp_Res1 = MKBADDRU(lock);
			dp->dp_Res2 = 0;
		}
	}

	reply_packet(dp);
}

void action_examine_object(struct DosPacket *dp)
{
	struct FileLock *lock = (struct FileLock *)BADDR(dp->dp_Arg1);
	struct FileInfoBlock *fib = (struct FileInfoBlock *)BADDR(dp->dp_Arg2);

	dbg("ACTION_EXAMINE_OBJECT\n");
	dbg("  lock = $l\n", lock);
	dbg("  fib = $l\n", fib);

	struct ExamineObjectRequest *req = (struct ExamineObjectRequest *)request_buffer;
	req->has_response = 0;
	req->type = dp->dp_Type;
	req->key = lock == NULL ? 0 : lock->fl_Key;

	write_req_and_wait_for_res(sizeof(struct ExamineObjectRequest));

	struct ExamineObjectResponse *res = (struct ExamineObjectResponse *)request_buffer;
	if (!res->success)
	{
		dbg("  Failed, error code $l\n", (LONG)res->error_code);
		dp->dp_Res1 = DOSFALSE;
		dp->dp_Res2 = res->error_code;
	}
	else
	{
		int nlen = (unsigned char)(res->data[0]);
		memcpy(fib->fib_FileName, res->data, nlen + 1);
		fib->fib_FileName[nlen + 1] = 0;

		int clen = (unsigned char)(res->data[nlen + 1]);
		memcpy(fib->fib_Comment, &(res->data[nlen + 1]), clen + 1);
		fib->fib_Comment[clen + 1] = 0;

		fib->fib_DiskKey = res->disk_key;
		fib->fib_DirEntryType = res->entry_type;
		fib->fib_EntryType = res->entry_type;
		fib->fib_Protection = res->protection;
		fib->fib_Size = res->size;
		fib->fib_NumBlocks = (res->size + 511) >> 9;
		fib->fib_Date.ds_Days = res->date[0];
		fib->fib_Date.ds_Minute = res->date[1];
		fib->fib_Date.ds_Tick = res->date[2];

		dp->dp_Res1 = DOSTRUE;
		dp->dp_Res2 = 0;
	}

	reply_packet(dp);
}

void action_examine_next(struct DosPacket *dp)
{
	struct FileLock *lock = (struct FileLock *)BADDR(dp->dp_Arg1);
	struct FileInfoBlock *fib = (struct FileInfoBlock *)BADDR(dp->dp_Arg2);

	dbg("ACTION_EXAMINE_NEXT\n");
	dbg("  lock = $l\n", lock);
	dbg("  fib = $l\n", fib);

	struct ExamineNextRequest *req = (struct ExamineNextRequest *)request_buffer;
	req->has_response = 0;
	req->type = dp->dp_Type;
	req->key = lock == NULL ? 0 : lock->fl_Key;
	req->disk_key = fib->fib_DiskKey;

	write_req_and_wait_for_res(sizeof(struct ExamineNextRequest));

	struct ExamineNextResponse *res = (struct ExamineNextResponse *)request_buffer;
	if (!res->success)
	{
		dbg("  Failed, error code $l\n", (LONG)res->error_code);
		dp->dp_Res1 = DOSFALSE;
		dp->dp_Res2 = res->error_code;
	}
	else
	{
		int nlen = (unsigned char)(res->data[0]);
		memcpy(fib->fib_FileName, res->data, nlen + 1);
		fib->fib_FileName[nlen + 1] = 0;

		int clen = (unsigned char)(res->data[nlen + 1]);
		memcpy(fib->fib_Comment, &(res->data[nlen + 1]), clen + 1);
		fib->fib_Comment[clen + 1] = 0;

		fib->fib_DiskKey = res->disk_key;
		fib->fib_DirEntryType = res->entry_type;
		fib->fib_EntryType = res->entry_type;
		fib->fib_Protection = res->protection;
		fib->fib_Size = res->size;
		fib->fib_NumBlocks = (res->size + 511) >> 9;
		fib->fib_Date.ds_Days = res->date[0];
		fib->fib_Date.ds_Minute = res->date[1];
		fib->fib_Date.ds_Tick = res->date[2];

		dp->dp_Res1 = DOSTRUE;
		dp->dp_Res2 = 0;
	}

	reply_packet(dp);
}

void action_examine_fh(struct DosPacket *dp)
{
	ULONG arg1 = dp->dp_Arg1;
	struct FileInfoBlock *fib = (struct FileInfoBlock *)BADDR(dp->dp_Arg2);

	dbg("ACTION_EXAMINE_FH\n");
	dbg("  arg1 = $l\n", arg1);
	dbg("  fib = $l\n", fib);

	struct ExamineFhRequest *req = (struct ExamineFhRequest *)request_buffer;
	req->has_response = 0;
	req->type = dp->dp_Type;
	req->arg1 = arg1;

	write_req_and_wait_for_res(sizeof(struct ExamineFhRequest));

	struct ExamineFhResponse *res = (struct ExamineFhResponse *)request_buffer;
	if (!res->success)
	{
		dbg("  Failed, error code $l\n", (LONG)res->error_code);
		dp->dp_Res1 = DOSFALSE;
		dp->dp_Res2 = res->error_code;
	}
	else
	{
		int nlen = (unsigned char)(res->data[0]);
		memcpy(fib->fib_FileName, res->data, nlen + 1);
		fib->fib_FileName[nlen + 1] = 0;

		int clen = (unsigned char)(res->data[nlen + 1]);
		memcpy(fib->fib_Comment, &(res->data[nlen + 1]), clen + 1);
		fib->fib_Comment[clen + 1] = 0;

		fib->fib_DiskKey = res->disk_key;
		fib->fib_DirEntryType = res->entry_type;
		fib->fib_EntryType = res->entry_type;
		fib->fib_Protection = res->protection;
		fib->fib_Size = res->size;
		fib->fib_NumBlocks = (res->size + 511) >> 9;
		fib->fib_Date.ds_Days = res->date[0];
		fib->fib_Date.ds_Minute = res->date[1];
		fib->fib_Date.ds_Tick = res->date[2];

		dp->dp_Res1 = DOSTRUE;
		dp->dp_Res2 = 0;
	}

	reply_packet(dp);
}

void action_findxxx(struct DosPacket *dp)
{
	struct FileHandle *fh = (struct FileHandle *)BADDR(dp->dp_Arg1);
	struct FileLock *lock = (struct FileLock *)BADDR(dp->dp_Arg2);
	unsigned char *name = (unsigned char *)BADDR(dp->dp_Arg3);

	if (dp->dp_Type == ACTION_FINDUPDATE)
		dbg("ACTION_FINDUPDATE\n");
	else if (dp->dp_Type == ACTION_FINDINPUT)
		dbg("ACTION_FINDINPUT\n");
	else if (dp->dp_Type == ACTION_FINDOUTPUT)
		dbg("ACTION_FINDOUTPUT\n");

	dbg("  file handle = $l\n", fh);
	dbg("  lock = $l\n", lock);
	dbg("  name = $S\n", name);

	struct FindXxxRequest *req = (struct FindXxxRequest *)request_buffer;
	req->has_response = 0;
	req->type = dp->dp_Type;
	req->key = lock == NULL ? 0 : lock->fl_Key;

	int nlen = *name;
	memcpy(req->name, name, nlen + 1);

	write_req_and_wait_for_res(sizeof(struct FindXxxRequest) + nlen);

	struct FindXxxResponse *res = (struct FindXxxResponse *)request_buffer;
	if (!res->success)
	{
		dbg("  Failed, error code $l\n", (LONG)res->error_code);
		dp->dp_Res1 = DOSFALSE;
		dp->dp_Res2 = res->error_code;
	}
	else
	{
		fh->fh_Arg1 = res->arg1;
		fh->fh_Type = mp;
		fh->fh_Port = DOSFALSE;  // Not an interactive file.

		dp->dp_Res1 = DOSTRUE;
		dp->dp_Res2 = 0;
	}

	reply_packet(dp);
}

void action_read(struct DosPacket *dp)
{
	ULONG arg1 = dp->dp_Arg1;
	UBYTE *dst = (UBYTE *)dp->dp_Arg2;
	int length = dp->dp_Arg3;

	dbg("ACTION_READ\n");
	dbg("  arg1 = $l\n", arg1);
	dbg("  length = $l\n", length);

	if (length == 0)
	{
		dp->dp_Res1 = -1;
		dp->dp_Res2 = ERROR_INVALID_LOCK; // This is not the correct error.
		reply_packet(dp);
		return;
	}

	int total_read = 0;
	while (length)
	{
		int to_read = length;
		if (to_read > BUFFER_SIZE)
			to_read = BUFFER_SIZE;

		struct ReadRequest *req = (struct ReadRequest *)request_buffer;
		req->has_response = 0;
		req->type = dp->dp_Type;
		req->arg1 = arg1;
		req->address = TranslateAddressA314(data_buffer);
		req->length = to_read;

		write_req_and_wait_for_res(sizeof(struct ReadRequest));

		struct ReadResponse *res = (struct ReadResponse *)request_buffer;
		if (!res->success)
		{
			dbg("  Failed, error code $l\n", (LONG)res->error_code);
			dp->dp_Res1 = -1;
			dp->dp_Res2 = res->error_code;
			reply_packet(dp);
			return;
		}

		if (res->actual)
		{
			memcpy(dst, data_buffer, res->actual);
			dst += res->actual;
			total_read += res->actual;
			length -= res->actual;
		}

		if (res->actual < to_read)
			break;
	}

	dp->dp_Res1 = total_read;
	dp->dp_Res2 = 0;
	reply_packet(dp);
}

void action_write(struct DosPacket *dp)
{
	ULONG arg1 = dp->dp_Arg1;
	UBYTE *src = (UBYTE *)dp->dp_Arg2;
	int length = dp->dp_Arg3;

	dbg("ACTION_WRITE\n");
	dbg("  arg1 = $l\n", arg1);
	dbg("  length = $l\n", length);

	int total_written = 0;
	while (length)
	{
		int to_write = length;
		if (to_write > BUFFER_SIZE)
			to_write = BUFFER_SIZE;

		memcpy(data_buffer, src, to_write);

		struct WriteRequest *req = (struct WriteRequest *)request_buffer;
		req->has_response = 0;
		req->type = dp->dp_Type;
		req->arg1 = arg1;
		req->address = TranslateAddressA314(data_buffer);
		req->length = to_write;

		write_req_and_wait_for_res(sizeof(struct WriteRequest));

		struct WriteResponse *res = (struct WriteResponse *)request_buffer;
		if (!res->success)
		{
			dbg("  Failed, error code $l\n", (LONG)res->error_code);
			dp->dp_Res1 = total_written;
			dp->dp_Res2 = res->error_code;
			reply_packet(dp);
			return;
		}

		if (res->actual)
		{
			src += res->actual;
			total_written += res->actual;
			length -= res->actual;
		}

		if (res->actual < to_write)
			break;
	}

	dp->dp_Res1 = total_written;
	dp->dp_Res2 = 0;
	reply_packet(dp);
}

void action_seek(struct DosPacket *dp)
{
	ULONG arg1 = dp->dp_Arg1;
	LONG new_pos = dp->dp_Arg2;
	LONG mode = dp->dp_Arg3;

	dbg("ACTION_SEEK\n");
	dbg("  arg1 = $l\n", arg1);
	dbg("  new_pos = $l\n", new_pos);
	dbg("  mode = $l\n", mode);

	struct SeekRequest *req = (struct SeekRequest *)request_buffer;
	req->has_response = 0;
	req->type = dp->dp_Type;
	req->arg1 = arg1;
	req->new_pos = new_pos;
	req->mode = mode;

	write_req_and_wait_for_res(sizeof(struct SeekRequest));

	struct SeekResponse *res = (struct SeekResponse *)request_buffer;
	if (!res->success)
	{
		dbg("  Failed, error code $l\n", (LONG)res->error_code);
		dp->dp_Res1 = -1;
		dp->dp_Res2 = res->error_code;
	}
	else
	{
		dp->dp_Res1 = res->old_pos;
		dp->dp_Res2 = 0;
	}

	reply_packet(dp);
}

void action_end(struct DosPacket *dp)
{
	ULONG arg1 = dp->dp_Arg1;

	dbg("ACTION_END\n");
	dbg("  arg1 = $l\n", arg1);

	struct EndRequest *req = (struct EndRequest *)request_buffer;
	req->has_response = 0;
	req->type = dp->dp_Type;
	req->arg1 = arg1;

	write_req_and_wait_for_res(sizeof(struct EndRequest));

	//struct EndResponse *res = (struct EndResponse *)request_buffer;

	dp->dp_Res1 = DOSTRUE;
	dp->dp_Res2 = 0;
	reply_packet(dp);
}

void action_delete_object(struct DosPacket *dp)
{
	struct FileLock *lock = (struct FileLock *)BADDR(dp->dp_Arg1);
	unsigned char *name = (unsigned char *)BADDR(dp->dp_Arg2);

	dbg("ACTION_DELETE_OBJECT\n");
	dbg("  lock = $l\n", lock);
	dbg("  name = $S\n", name);

	struct DeleteObjectRequest *req = (struct DeleteObjectRequest *)request_buffer;
	req->has_response = 0;
	req->type = dp->dp_Type;
	req->key = lock == NULL ? 0 : lock->fl_Key;

	int nlen = *name;
	memcpy(req->name, name, nlen + 1);

	write_req_and_wait_for_res(sizeof(struct DeleteObjectRequest) + nlen);

	struct DeleteObjectResponse *res = (struct DeleteObjectResponse *)request_buffer;
	if (!res->success)
	{
		dbg("  Failed, error code $l\n", (LONG)res->error_code);
		dp->dp_Res1 = DOSFALSE;
		dp->dp_Res2 = res->error_code;
	}
	else
	{
		dp->dp_Res1 = DOSTRUE;
		dp->dp_Res2 = 0;
	}

	reply_packet(dp);
}

void action_rename_object(struct DosPacket *dp)
{
	struct FileLock *lock = (struct FileLock *)BADDR(dp->dp_Arg1);
	unsigned char *name = (unsigned char *)BADDR(dp->dp_Arg2);
	struct FileLock *target_dir = (struct FileLock *)BADDR(dp->dp_Arg3);
	unsigned char *new_name = (unsigned char *)BADDR(dp->dp_Arg4);

	dbg("ACTION_RENAME_OBJECT\n");
	dbg("  lock = $l\n", lock);
	dbg("  name = $S\n", name);
	dbg("  target directory = $l\n", lock);
	dbg("  new name = $S\n", new_name);

	struct RenameObjectRequest *req = (struct RenameObjectRequest *)request_buffer;
	req->has_response = 0;
	req->type = dp->dp_Type;
	req->key = lock == NULL ? 0 : lock->fl_Key;
	req->target_dir = target_dir == NULL ? 0 : target_dir->fl_Key;

	int nlen = *name;
	int nnlen = *new_name;

	req->name_len = nlen;
	req->new_name_len = nnlen;

	unsigned char *p = &(req->new_name_len) + 1;
	memcpy(p, name + 1, nlen);
	p += nlen;
	memcpy(p, new_name + 1, nnlen);

	write_req_and_wait_for_res(sizeof(struct RenameObjectRequest) + nlen + nnlen);

	struct RenameObjectResponse *res = (struct RenameObjectResponse *)request_buffer;
	if (!res->success)
	{
		dbg("  Failed, error code $l\n", (LONG)res->error_code);
		dp->dp_Res1 = DOSFALSE;
		dp->dp_Res2 = res->error_code;
	}
	else
	{
		dp->dp_Res1 = DOSTRUE;
		dp->dp_Res2 = 0;
	}

	reply_packet(dp);
}

void action_create_dir(struct DosPacket *dp)
{
	struct FileLock *lock = (struct FileLock *)BADDR(dp->dp_Arg1);
	unsigned char *name = (unsigned char *)BADDR(dp->dp_Arg2);

	dbg("ACTION_CREATE_DIR\n");
	dbg("  lock = $l\n", lock);
	dbg("  name = $S\n", name);

	struct CreateDirRequest *req = (struct CreateDirRequest *)request_buffer;
	req->has_response = 0;
	req->type = dp->dp_Type;
	req->key = lock == NULL ? 0 : lock->fl_Key;

	int nlen = *name;
	memcpy(req->name, name, nlen + 1);

	write_req_and_wait_for_res(sizeof(struct CreateDirRequest) + nlen);

	struct CreateDirResponse *res = (struct CreateDirResponse *)request_buffer;
	if (!res->success)
	{
		dbg("  Failed, error code $l\n", (LONG)res->error_code);
		dp->dp_Res1 = DOSFALSE;
		dp->dp_Res2 = res->error_code;
	}
	else
	{
		struct FileLock *lock = create_and_add_file_lock(res->key, SHARED_LOCK);

		dbg("  Returning lock $l\n", lock);
		dp->dp_Res1 = MKBADDRU(lock);
		dp->dp_Res2 = 0;
	}

	reply_packet(dp);
}

void action_set_protect(struct DosPacket *dp)
{
	struct FileLock *lock = (struct FileLock *)BADDR(dp->dp_Arg2);
	unsigned char *name = (unsigned char *)BADDR(dp->dp_Arg3);
	long mask = dp->dp_Arg4;

	dbg("ACTION_SET_PROTECT\n");
	dbg("  lock = $l\n", lock);
	dbg("  name = $S\n", name);
	dbg("  mask = $l\n", mask);

	struct SetProtectRequest *req = (struct SetProtectRequest *)request_buffer;
	req->has_response = 0;
	req->type = dp->dp_Type;
	req->key = lock == NULL ? 0 : lock->fl_Key;
	req->mask = mask;

	int nlen = *name;
	memcpy(req->name, name, nlen + 1);

	write_req_and_wait_for_res(sizeof(struct SetProtectRequest) + nlen);

	struct SetProtectResponse *res = (struct SetProtectResponse *)request_buffer;
	if (!res->success)
	{
		dbg("  Failed, error code $l\n", (LONG)res->error_code);
		dp->dp_Res1 = DOSFALSE;
		dp->dp_Res2 = res->error_code;
	}
	else
	{
		dp->dp_Res1 = DOSTRUE;
		dp->dp_Res2 = 0;
	}

	reply_packet(dp);
}

void action_set_comment(struct DosPacket *dp)
{
	struct FileLock *lock = (struct FileLock *)BADDR(dp->dp_Arg2);
	unsigned char *name = (unsigned char *)BADDR(dp->dp_Arg3);
	unsigned char *comment = (unsigned char *)BADDR(dp->dp_Arg4);

	dbg("ACTION_SET_COMMENT\n");
	dbg("  lock = $l\n", lock);
	dbg("  name = $S\n", name);
	dbg("  comment = $S\n", comment);

	struct SetCommentRequest *req = (struct SetCommentRequest *)request_buffer;
	req->has_response = 0;
	req->type = dp->dp_Type;
	req->key = lock == NULL ? 0 : lock->fl_Key;

	int nlen = *name;
	int clen = *comment;

	req->name_len = nlen;
	req->comment_len = clen;

	unsigned char *p = &(req->comment_len) + 1;
	memcpy(p, name + 1, nlen);
	p += nlen;
	memcpy(p, comment + 1, clen);

	write_req_and_wait_for_res(sizeof(struct SetCommentRequest) + nlen + clen);

	struct SetCommentResponse *res = (struct SetCommentResponse *)request_buffer;
	if (!res->success)
	{
		dbg("  Failed, error code $l\n", (LONG)res->error_code);
		dp->dp_Res1 = DOSFALSE;
		dp->dp_Res2 = res->error_code;
	}
	else
	{
		dp->dp_Res1 = DOSTRUE;
		dp->dp_Res2 = 0;
	}

	reply_packet(dp);
}

void action_same_lock(struct DosPacket *dp)
{
	struct FileLock *lock1 = (struct FileLock *)BADDR(dp->dp_Arg1);
	struct FileLock *lock2 = (struct FileLock *)BADDR(dp->dp_Arg2);

	dbg("ACTION_SAME_LOCK\n");
	dbg("  locks to compare = $l $l\n", lock1, lock2);

	struct SameLockRequest *req = (struct SameLockRequest *)request_buffer;
	req->has_response = 0;
	req->type = dp->dp_Type;
	req->key1 = lock1->fl_Key;
	req->key2 = lock2->fl_Key;

	write_req_and_wait_for_res(sizeof(struct SameLockRequest));

	struct SameLockResponse *res = (struct SameLockResponse *)request_buffer;
	dp->dp_Res1 = res->success ? DOSTRUE : DOSFALSE;
	dp->dp_Res2 = res->error_code;

	reply_packet(dp);
}

void fill_info_data(struct InfoData *id)
{
	memset(id, 0, sizeof(struct InfoData));
	id->id_DiskState = ID_VALIDATED;
	id->id_NumBlocks = 512 * 1024;
	id->id_NumBlocksUsed = 10;
	id->id_BytesPerBlock = 512;
	id->id_DiskType = my_volume->dl_DiskType;
	id->id_VolumeNode = MKBADDRU(my_volume);
	id->id_InUse = DOSTRUE;
}

void action_info(struct DosPacket *dp)
{
	struct FileLock *lock = (struct FileLock *)BADDR(dp->dp_Arg1);
	struct InfoData *id = (struct InfoData *)BADDR(dp->dp_Arg2);

	dbg("ACTION_INFO\n");
	dbg("  lock = $l\n", lock);

	fill_info_data(id);

	dp->dp_Res1 = DOSTRUE;
	dp->dp_Res2 = 0;
	reply_packet(dp);
}

void action_disk_info(struct DosPacket *dp)
{
	struct InfoData *id = (struct InfoData *)BADDR(dp->dp_Arg1);

	dbg("ACTION_DISK_INFO\n");

	fill_info_data(id);

	dp->dp_Res1 = DOSTRUE;
	dp->dp_Res2 = 0;
	reply_packet(dp);
}

void action_unsupported(struct DosPacket *dp)
{
	dbg("ACTION_UNSUPPORTED\n");
	dbg("  Unsupported action: $l\n", (ULONG)dp->dp_Type);

	struct UnsupportedRequest *req = (struct UnsupportedRequest *)request_buffer;
	req->has_response = 0;
	req->type = ACTION_UNSUPPORTED;
	req->dp_Type = dp->dp_Type;

	write_req_and_wait_for_res(sizeof(struct UnsupportedRequest));

	struct UnsupportedResponse *res = (struct UnsupportedResponse *)request_buffer;
	dp->dp_Res1 = res->success ? DOSTRUE : DOSFALSE;
	dp->dp_Res2 = res->error_code;
	reply_packet(dp);
}

void start(__reg("a0") struct DosPacket *startup_packet)
{
	SysBase = *(struct ExecBase **)4;
	DOSBase = (struct DosLibrary *)OpenLibrary(DOSNAME, 0);

	mp = MyCreatePort(NULL, 0);

	startup_fs_handler(startup_packet);

	while (1)
	{
		WaitPort(mp);
		struct StandardPacket *sp = (struct StandardPacket *)GetMsg(mp);
		struct DosPacket *dp = (struct DosPacket *)(sp->sp_Msg.mn_Node.ln_Name);

		switch (dp->dp_Type)
		{
		case ACTION_LOCATE_OBJECT: action_locate_object(dp); break;
		case ACTION_FREE_LOCK: action_free_lock(dp); break;
		case ACTION_COPY_DIR: action_copy_dir(dp); break;
		case ACTION_PARENT: action_parent(dp); break;
		case ACTION_EXAMINE_OBJECT: action_examine_object(dp); break;
		case ACTION_EXAMINE_NEXT: action_examine_next(dp); break;
		case ACTION_EXAMINE_FH: action_examine_fh(dp); break;

		case ACTION_FINDUPDATE: action_findxxx(dp); break;
		case ACTION_FINDINPUT: action_findxxx(dp); break;
		case ACTION_FINDOUTPUT: action_findxxx(dp); break;
		case ACTION_READ: action_read(dp); break;
		case ACTION_WRITE: action_write(dp); break;
		case ACTION_SEEK: action_seek(dp); break;
		case ACTION_END: action_end(dp); break;
		//case ACTION_TRUNCATE: action_truncate(dp); break;

		case ACTION_DELETE_OBJECT: action_delete_object(dp); break;
		case ACTION_RENAME_OBJECT: action_rename_object(dp); break;
		case ACTION_CREATE_DIR: action_create_dir(dp); break;

		case ACTION_SET_PROTECT: action_set_protect(dp); break;
		case ACTION_SET_COMMENT: action_set_comment(dp); break;
		//case ACTION_SET_DATE: action_set_date(dp); break;

		case ACTION_SAME_LOCK: action_same_lock(dp); break;
		case ACTION_DISK_INFO: action_disk_info(dp); break;
		case ACTION_INFO: action_info(dp); break;

		/*
		case ACTION_CURRENT_VOLUME: action_current_volume(dp); break;
		case ACTION_RENAME_DISK: action_rename_disk(dp); break;

		case ACTION_DIE: //action_die(dp); break;
		case ACTION_MORE_CACHE: //action_more_cache(dp); break;
		case ACTION_FLUSH: //action_flush(dp); break;
		case ACTION_INHIBIT: //action_inhibit(dp); break;
		case ACTION_WRITE_PROTECT: //action_write_protect(dp); break;
		*/

		default: action_unsupported(dp); break;
		}
	}

	dbg("Shutting down\n");

	// More cleaning up is necessary if the handler is to exit.
	CloseLibrary((struct Library *)DOSBase);
}
