/*
  Hatari - hatari-glue.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  This file contains some code to glue the UAE CPU core to the rest of the
  emulator and Hatari's "illegal" opcodes.
*/
const char HatariGlue_fileid[] = "Hatari hatari-glue.c : " __DATE__ " " __TIME__;


#include <stdio.h>

#include "main.h"
#include "configuration.h"
//#include "video.h"
//#include "sysReg.h"

#include "sysdeps.h"
#include "maccess.h"
#include "memory.h"
#include "newcpu.h"
#include "hatari-glue.h"
#include "m68krash.h"


struct uae_prefs currprefs, changed_prefs;

int pendingInterrupts = 0;

uae_u32 get_longi(uaecptr addr) {
	printf("ACHTUNG!\n");
	exit(1);
	return 0x4E71;
}
uae_u32 get_wordi(uaecptr addr) {
	printf("ACHTUNG!\n");
	exit(1);
	return 0x4E71;
}


/**
 * Reset custom chips
 */
void customreset(void)
{
	pendingInterrupts = 0;

	/* In case the 6301 was executing a custom program from its RAM */
	/* we must turn it back to the 'normal' mode. */
//	IKBD_Reset_ExeMode ();

	/* Reseting the GLUE video chip should also set freq/res register to 0 */
	//Video_Reset_Glue ();
}


/**
 * Return interrupt number (1 - 7), 0 means no interrupt.
 * Note that the interrupt stays pending if it can't be executed yet
 * due to the interrupt level field in the SR.
 */
int intlev(void)
{
    /* Poll interrupt level from interrupt status and mask registers
     * --> see sysReg.c
     */
    return get_interrupt_level();
}

/**
 * Initialize 680x0 emulation
 */
int Init680x0(M68kCpu cpu)
{
	switch(cpu) {
		case M68K_CPU_030:
			currprefs.cpu_level = changed_prefs.cpu_level = 3;
			currprefs.cpu_model = 68030;
			
			currprefs.fpu_model = changed_prefs.fpu_model = FPU_68882;
			currprefs.fpu_revision = 0x20;
			
			currprefs.cpu_compatible = changed_prefs.cpu_compatible = 1;
			currprefs.address_space_24 = changed_prefs.address_space_24 = 1;
			currprefs.cpu_cycle_exact = changed_prefs.cpu_cycle_exact = 0;
			currprefs.fpu_strict = changed_prefs.fpu_strict = 0;
			currprefs.mmu_model = changed_prefs.mmu_model = 68030;
			break;
		case M68K_CPU_040:
			currprefs.cpu_level = changed_prefs.cpu_level = 4;
			currprefs.cpu_model = 68040;
			
			currprefs.fpu_model = changed_prefs.fpu_model = FPU_CPU;
			currprefs.fpu_revision = 0x41;
			
			currprefs.cpu_compatible = changed_prefs.cpu_compatible = 0;
			currprefs.address_space_24 = changed_prefs.address_space_24 = 0;
			currprefs.cpu_cycle_exact = changed_prefs.cpu_cycle_exact = 0;
			currprefs.fpu_strict = changed_prefs.fpu_strict = 0;
			currprefs.mmu_model = changed_prefs.mmu_model = 68040;
			break;
		default:
			fprintf(stderr, "Invalid cpu version\n");
			return false;
	}

   	write_log("Init680x0() called\n");

	init_m68k();

	return true;
}


/**
 * Deinitialize 680x0 emulation
 */
void Exit680x0(void)
{
	memory_uninit();

	free(table68k);
	table68k = NULL;
}
