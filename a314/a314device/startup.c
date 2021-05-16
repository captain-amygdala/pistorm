#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <hardware/intbits.h>
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>

#include <proto/exec.h>
#include <proto/expansion.h>

#include "a314.h"
#include "device.h"
#include "protocol.h"
#include "startup.h"
#include "fix_mem_region.h"
#include "debug.h"

#define A314_MANUFACTURER 0x07db
#define A314_PRODUCT 0xa3

#define TASK_PRIORITY 80
#define TASK_STACK_SIZE 1024

struct ExpansionBase *ExpansionBase;
struct MsgPort task_mp;
struct Task *task;
struct ComArea *ca;

struct InterruptData
{
	struct Task *task;
	struct ComArea *ca;
};

struct InterruptData interrupt_data;
struct Interrupt ports_interrupt;

extern void task_main();
extern void init_sockets();
extern void IntServer();

void NewList(struct List *l)
{
	l->lh_Head = (struct Node *)&(l->lh_Tail);
	l->lh_Tail = NULL;
	l->lh_TailPred = (struct Node *)&(l->lh_Head);
}

static struct Task *create_task(char *name, long priority, char *initialPC, unsigned long stacksize)
{
	char *stack = AllocMem(stacksize, MEMF_CLEAR);
	if (stack == NULL)
		return NULL;

	struct Task *tc = AllocMem(sizeof(struct Task), MEMF_CLEAR | MEMF_PUBLIC);
	if (tc == NULL)
	{
		FreeMem(stack, stacksize);
		return NULL;
	}

	tc->tc_Node.ln_Type = NT_TASK;
	tc->tc_Node.ln_Pri = priority;
	tc->tc_Node.ln_Name = name;
	tc->tc_SPLower = (APTR)stack;
	tc->tc_SPUpper = (APTR)(stack + stacksize);
	tc->tc_SPReg = (APTR)(stack + stacksize);

	AddTask(tc, initialPC, 0);
	return tc;
}

static void init_message_port()
{
	task_mp.mp_Node.ln_Name = device_name;
	task_mp.mp_Node.ln_Pri = 0;
	task_mp.mp_Node.ln_Type = NT_MSGPORT;
	task_mp.mp_Flags = PA_SIGNAL;
	task_mp.mp_SigBit = SIGB_MSGPORT;
	task_mp.mp_SigTask = task;
	NewList(&(task_mp.mp_MsgList));
}

static void add_interrupt_handler()
{
	interrupt_data.task = task;
	interrupt_data.ca = ca;

	ports_interrupt.is_Node.ln_Type = NT_INTERRUPT;
	ports_interrupt.is_Node.ln_Pri = 0;
	ports_interrupt.is_Node.ln_Name = device_name;
	ports_interrupt.is_Data = (APTR)&interrupt_data;
	ports_interrupt.is_Code = IntServer;

	AddIntServer(INTB_PORTS, &ports_interrupt);
}

BOOL task_start()
{
	ExpansionBase = (struct ExpansionBase *)OpenLibrary(EXPANSIONNAME, 0);
	if (!ExpansionBase)
		return FALSE;

	struct ConfigDev *cd = FindConfigDev(NULL, A314_MANUFACTURER, A314_PRODUCT);
	if (!cd)
	{
		CloseLibrary((struct Library *)ExpansionBase);
		return FALSE;
	}

	ca = (struct ComArea *)cd->cd_BoardAddr;

	if (!fix_memory())
		return FALSE;

	task = create_task(device_name, TASK_PRIORITY, (void *)task_main, TASK_STACK_SIZE);
	if (task == NULL)
	{
		debug_printf("Unable to create task\n");
		return FALSE;
	}

	init_message_port();
	init_sockets();

	ca->a_enable = 0;
	unsigned char discard_value = ca->a_events;

	ca->r_events = R_EVENT_STARTED;

	add_interrupt_handler();

	ca->a_enable = A_EVENT_R2A_TAIL;

	return TRUE;
}
