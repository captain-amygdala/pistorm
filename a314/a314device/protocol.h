#include <exec/types.h>

// Packet types that are sent across the physical channel.
#define PKT_DRIVER_STARTED		1
#define PKT_DRIVER_SHUTTING_DOWN	2
#define PKT_SETTINGS			3
#define PKT_CONNECT			4
#define PKT_CONNECT_RESPONSE		5
#define PKT_DATA			6
#define PKT_EOS				7
#define PKT_RESET			8

// Events that are communicated via IRQ from Amiga to Raspberry.
#define R_EVENT_A2R_TAIL	1
#define R_EVENT_R2A_HEAD	2
#define R_EVENT_STARTED 	4

// Events that are communicated from Raspberry to Amiga.
#define A_EVENT_R2A_TAIL	1
#define A_EVENT_A2R_HEAD	2

#define COM_AREA_BASE		0xe90000

#define PISTORM_BASE		0xc00000
#define PISTORM_SIZE		(3*512*1024)

// The communication area, used to create the physical channel.
struct ComArea
{
	volatile UBYTE a_events;
	volatile UBYTE a_enable;
	volatile UBYTE r_events;
	volatile UBYTE r_enable;

	volatile UBYTE a2r_tail;
	volatile UBYTE r2a_head;
	volatile UBYTE r2a_tail;
	volatile UBYTE a2r_head;

	UBYTE a2r_buffer[256];
	UBYTE r2a_buffer[256];
};

extern struct ComArea *ca;
