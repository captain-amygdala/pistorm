#ifndef _M68KRASH_H_
#define _M68KRASH_H_

#define m68krash_init M68000_Init
#define m68krash_reset M68000_Reset
#define m68krash_start M68000_Start
#define m68krash_bus_error M68000_BusError

typedef enum M68kCpu M68kCpu;
enum M68kCpu {
	M68K_CPU_000,
	M68K_CPU_010,
	M68K_CPU_020,
	M68K_CPU_030,
	M68K_CPU_040,
	M68K_CPU_060,
};

extern void M68000_Init(M68kCpu cpu);
extern void M68000_Reset(bool bCold);
extern void M68000_Stop(void);
extern void M68000_Start(void);

#endif
