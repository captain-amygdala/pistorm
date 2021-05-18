#include <exec/types.h>
#include <exec/memory.h>

#include <libraries/dos.h>

#include <devices/audio.h>

#include <proto/exec.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../../a314device/a314.h"
#include "../../a314device/proto_a314.h"

#include <clib/alib_protos.h>

#define SERVICE_NAME "piaudio"

#define FREQ 18000
#define BUFFER_LEN_MS 50
#define SAMPLES (FREQ * BUFFER_LEN_MS / 1000)

#define R0		1
#define L0		2
#define L1		4
#define R1		8

#define LEFT_CHAN_MASK	(L0 | L1)
#define RIGHT_CHAN_MASK	(R0 | R1)

#define LEFT 0
#define RIGHT 1

struct MsgPort *sync_mp = NULL;
struct MsgPort *async_mp = NULL;

struct A314_IORequest *sync_a314_req = NULL;
struct A314_IORequest *write_a314_req = NULL;

struct Library *A314Base;

char *audio_buffers[4] = { NULL, NULL, NULL, NULL };

struct IOAudio *sync_audio_req = NULL;
struct IOAudio *async_audio_req[4] = { NULL, NULL, NULL, NULL };

ULONG allocated_channels;

BOOL a314_device_open = FALSE;
BOOL audio_device_open = FALSE;
BOOL stream_open = FALSE;
BOOL pending_a314_write = FALSE;

ULONG socket;
int back_index = 0;
char awbuf[8];

void start_a314_cmd(struct MsgPort *reply_port, struct A314_IORequest *ior, UWORD cmd, char *buffer, int length)
{
	ior->a314_Request.io_Message.mn_ReplyPort = reply_port;
	ior->a314_Request.io_Command = cmd;
	ior->a314_Request.io_Error = 0;
	ior->a314_Socket = socket;
	ior->a314_Buffer = buffer;
	ior->a314_Length = length;
	SendIO((struct IORequest *)ior);
}

BYTE a314_connect(char *name)
{
	socket = time(NULL);
	start_a314_cmd(sync_mp, sync_a314_req, A314_CONNECT, name, strlen(name));
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return sync_a314_req->a314_Request.io_Error;
}

BYTE a314_write(char *buffer, int length)
{
	start_a314_cmd(sync_mp, sync_a314_req, A314_WRITE, buffer, length);
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return sync_a314_req->a314_Request.io_Error;
}

BYTE a314_eos()
{
	start_a314_cmd(sync_mp, sync_a314_req, A314_EOS, NULL, 0);
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return sync_a314_req->a314_Request.io_Error;
}

BYTE a314_reset()
{
	start_a314_cmd(sync_mp, sync_a314_req, A314_RESET, NULL, 0);
	Wait(1L << sync_mp->mp_SigBit);
	GetMsg(sync_mp);
	return sync_a314_req->a314_Request.io_Error;
}

void start_a314_write(char *buffer, int length)
{
	start_a314_cmd(async_mp, write_a314_req, A314_WRITE, buffer, length);
	pending_a314_write = TRUE;
}

void submit_async_audio_req(int index)
{
	ULONG mask = ((index & 1) == LEFT) ? LEFT_CHAN_MASK : RIGHT_CHAN_MASK;
	ULONG unit = allocated_channels & mask;

	async_audio_req[index]->ioa_Request.io_Message.mn_ReplyPort = async_mp;
	async_audio_req[index]->ioa_Request.io_Command = CMD_WRITE;
	async_audio_req[index]->ioa_Request.io_Flags = ADIOF_PERVOL;
	async_audio_req[index]->ioa_Request.io_Unit = (void*)unit;
	async_audio_req[index]->ioa_Data = audio_buffers[index];
	async_audio_req[index]->ioa_Length = SAMPLES;
	async_audio_req[index]->ioa_Period = 197;
	async_audio_req[index]->ioa_Volume = 64;
	async_audio_req[index]->ioa_Cycles = 1;
	BeginIO((struct IORequest *)async_audio_req[index]);
}

int main()
{
	SetTaskPri(FindTask(NULL), 50);

	sync_mp = CreatePort(NULL, 0);
	if (!sync_mp)
	{
		printf("Unable to create sync reply message port\n");
		goto cleanup;
	}

	async_mp = CreatePort(NULL, 0);
	if (!async_mp)
	{
		printf("Unable to create async reply message port\n");
		goto cleanup;
	}

	sync_a314_req = (struct A314_IORequest *)CreateExtIO(sync_mp, sizeof(struct A314_IORequest));
	write_a314_req = (struct A314_IORequest *)CreateExtIO(sync_mp, sizeof(struct A314_IORequest));
	if (!sync_a314_req || !write_a314_req)
	{
		printf("Unable to create A314_IORequest\n");
		goto cleanup;
	}

	if (OpenDevice(A314_NAME, 0, (struct IORequest *)sync_a314_req, 0))
	{
		printf("Unable to open a314.device\n");
		goto cleanup;
	}

	a314_device_open = TRUE;

	A314Base = &(sync_a314_req->a314_Request.io_Device->dd_Library);

	memcpy(write_a314_req, sync_a314_req, sizeof(struct A314_IORequest));

	audio_buffers[0] = AllocMem(SAMPLES * 2, MEMF_FAST | MEMF_CHIP | MEMF_CLEAR);
	audio_buffers[2] = AllocMem(SAMPLES * 2, MEMF_FAST | MEMF_CHIP | MEMF_CLEAR);
	if (!audio_buffers[0] || !audio_buffers[2])
	{
		printf("Unable to allocate audio buffers in A314 chip memory\n");
		goto cleanup;
	}

	audio_buffers[1] = audio_buffers[0] + SAMPLES;
	audio_buffers[3] = audio_buffers[2] + SAMPLES;

	sync_audio_req = (struct IOAudio *)CreateExtIO(sync_mp, sizeof(struct IOAudio));
	if (!sync_audio_req)
	{
		printf("Unable to allocate sync audio request\n");
		goto cleanup;
	}

	int i;
	for (i = 0; i < 4; i++)
	{
		async_audio_req[i] = AllocMem(sizeof(struct IOAudio), MEMF_PUBLIC);
		if (!async_audio_req[i])
		{
			printf("Unable to allocate async audio request\n");
			goto cleanup;
		}
	}

	UBYTE which_channels[] = { L0 | R0, L0 | R1, L1 | R0, L1 | R1 };

	sync_audio_req->ioa_Request.io_Message.mn_ReplyPort = sync_mp;
	sync_audio_req->ioa_Request.io_Message.mn_Node.ln_Pri = 127;
	sync_audio_req->ioa_Request.io_Command = ADCMD_ALLOCATE;
	sync_audio_req->ioa_Request.io_Flags = ADIOF_NOWAIT;
	sync_audio_req->ioa_AllocKey = 0;
	sync_audio_req->ioa_Data = which_channels;
	sync_audio_req->ioa_Length = sizeof(which_channels);

	if (OpenDevice(AUDIONAME, 0, (struct IORequest *)sync_audio_req, 0))
	{
		printf("Unable to open audio.device\n");
		goto cleanup;
	}

	audio_device_open = TRUE;

	allocated_channels = (ULONG)sync_audio_req->ioa_Request.io_Unit;

	for (i = 0; i < 4; i++)
		memcpy(async_audio_req[i], sync_audio_req, sizeof(struct IOAudio));

	if (a314_connect(SERVICE_NAME) != A314_CONNECT_OK)
	{
		printf("Unable to connect to piaudio service\n");
		goto cleanup;
	}

	stream_open = TRUE;

	ULONG *buf_ptrs = (ULONG *)awbuf;
	buf_ptrs[0] = TranslateAddressA314(audio_buffers[0]);
	buf_ptrs[1] = TranslateAddressA314(audio_buffers[2]);
	if (a314_write(awbuf, 8) != A314_WRITE_OK)
	{
		printf("Unable to write buffer pointers\n");
		goto cleanup;
	}

	printf("PiAudio started, allocated channels: L%d, R%d\n",
		(allocated_channels & LEFT_CHAN_MASK) == L0 ? 0 : 1,
		(allocated_channels & RIGHT_CHAN_MASK) == R0 ? 0 : 1);

	sync_audio_req->ioa_Request.io_Command = CMD_STOP;
	DoIO((struct IORequest *)sync_audio_req);

	submit_async_audio_req(back_index + LEFT);
	submit_async_audio_req(back_index + RIGHT);

	sync_audio_req->ioa_Request.io_Command = CMD_START;
	DoIO((struct IORequest *)sync_audio_req);

	int pending_audio_reqs = 2;

	ULONG portsig = 1L << async_mp->mp_SigBit;

	printf("Press ctrl-c to exit...\n");

	while (TRUE)
	{
		if (pending_audio_reqs <= 2)
		{
			back_index ^= 2;

			submit_async_audio_req(back_index + LEFT);
			submit_async_audio_req(back_index + RIGHT);

			pending_audio_reqs += 2;

			if (!pending_a314_write)
			{
				awbuf[0] = back_index == 0 ? 0 : 1;
				start_a314_write(awbuf, 1);
			}
		}

		ULONG signal = Wait(SIGBREAKF_CTRL_C | portsig);

		if (signal & SIGBREAKF_CTRL_C)
			break;
		else if (signal & portsig)
		{
			struct Message *msg;
			while (msg = GetMsg(async_mp))
			{
				if (msg == (struct Message *)write_a314_req)
				{
					if (write_a314_req->a314_Request.io_Error == A314_WRITE_OK)
						pending_a314_write = FALSE;
					else
						goto cleanup;
				}
				else
					pending_audio_reqs--;
			}
		}
	}

cleanup:
	if (stream_open)
		a314_reset();
	if (audio_device_open)
		CloseDevice((struct IORequest *)sync_audio_req);
	for (i = 3; i >= 0; i--)
		if (async_audio_req[i])
			FreeMem(async_audio_req[i], sizeof(struct IOAudio));
	if (sync_audio_req)
		DeleteExtIO((struct IORequest *)sync_audio_req);
	if (audio_buffers[2])
		FreeMem(audio_buffers[2], SAMPLES * 2);
	if (audio_buffers[0])
		FreeMem(audio_buffers[0], SAMPLES * 2);
	if (a314_device_open)
		CloseDevice((struct IORequest *)sync_a314_req);
	if (write_a314_req)
		DeleteExtIO((struct IORequest *)write_a314_req);
	if (sync_a314_req)
		DeleteExtIO((struct IORequest *)sync_a314_req);
	if (async_mp)
		DeletePort(async_mp);
	if (sync_mp)
		DeletePort(sync_mp);

	return 0;
}
