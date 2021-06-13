#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/utility_protos.h>
#include <clib/graphics_protos.h>

#include <exec/types.h>
#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/alerts.h>
#include <exec/tasks.h>
#include <exec/io.h>
#include <exec/execbase.h>

#include <intuition/intuition.h>
#include <intuition/screens.h>

#include <graphics/displayinfo.h>
#include "pistorm_dev.h"
#include "../pistorm-dev-enums.h"

#include <dos/dos.h>
#include <stdio.h>

struct ExecBase *SysBase;

char *oldtaskname;
struct Task *task;

void (*oldCopyMem)(unsigned char *asm("a0"), unsigned char *asm("a1"), unsigned int asm("d0"));
void (*oldCopyMemQuick)(unsigned char *asm("a0"), unsigned char *asm("a1"), unsigned int asm("d0"));
APTR oldCopyMemPtr;
APTR oldCopyMemQuickPtr;

struct Screen *(*oldOpenScreenTagList)(struct NewScreen *n asm("a0"), struct TagItem *asm("a1"));
struct Screen *(*oldOpenScreen)(struct NewScreen *asm("a0"));

#define NORM_MONITOR_ID ((options.bits.norm_mon==1)?(PAL_MONITOR_ID):((options.bits.norm_mon==2)?(NTSC_MONITOR_ID):(DEFAULT_MONITOR_ID)))
#define AUTO_MONITOR_ID ((options.bits.auto_mon==0)?(PAL_MONITOR_ID):((options.bits.auto_mon==1)?(NTSC_MONITOR_ID):(DEFAULT_MONITOR_ID)))

extern unsigned int pistorm_base_addr;
#define WRITELONG(cmd, val) *(volatile unsigned int *)((unsigned int)(pistorm_base_addr+cmd)) = val;

void pi_CopyMem(unsigned char *src asm("a0"), unsigned char *dst asm("a1"), unsigned int size asm("d0")) {
	WRITELONG(PI_PTR1, (unsigned int)src);
	WRITELONG(PI_PTR2, (unsigned int)dst);
	WRITELONG(PI_CMD_MEMCPY, size);
}

void pi_CopyMemQuick(unsigned char *src asm("a0"), unsigned char *dst asm("a1"), unsigned int size asm("d0")) {
	WRITELONG(PI_PTR1, (unsigned int)src);
	WRITELONG(PI_PTR2, (unsigned int)dst);
	WRITELONG(PI_CMD_MEMCPY_Q, size);
}

int leave(int x)
{
    Forbid();
    task->tc_Node.ln_Name = oldtaskname;
    Permit();
    return(x);
}

ULONG pistorm_addr = 0xFFFFFFFF;

int main(int argc,char *argv[])
{
    char *task_name = "PiMems  ";
    SysBase = *(struct ExecBase **)4L;

    unsigned char no_quit = 1;

    task = (struct Task *)FindTask((STRPTR)&task_name);

    if(task)
        return(0);

    task = (struct Task *)FindTask(NULL);
    oldtaskname = task->tc_Node.ln_Name;

    Forbid();
    task->tc_Node.ln_Name = (char *)&task_name;
    Permit();

    pistorm_addr = pi_find_pistorm();
    if (pistorm_addr == 0xFFFFFFFF) {
        printf("We dead!\n");
        return(leave(50));
    } else {
        pistorm_base_addr = pistorm_addr;
    }

    oldCopyMemPtr = (APTR)SetFunction((struct Library *)SysBase, -0x270, pi_CopyMem);
    
    oldCopyMemQuickPtr = (APTR)SetFunction((struct Library *)SysBase, -0x276, pi_CopyMemQuick);
    //oldCopyMemQuickPtr = (APTR)SetFunction((struct Library *)SysBase, -0x276, pi_CopyMem);

    do
    {
        Wait(SIGBREAKF_CTRL_C);
    }
    while(no_quit);

    return(leave(0));
}
