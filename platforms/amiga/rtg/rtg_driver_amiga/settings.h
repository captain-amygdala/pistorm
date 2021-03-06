#ifndef settings_H
#define settings_H

/************************************************************************/

enum{
	PLANAR,
	CHUNKY,
	HICOLOR,
	TRUECOLOR,
	TRUEALPHA,
	MAXMODES
};

/************************************************************************/

#define	SETTINGSNAMEMAXCHARS		30
#define	BOARDNAMEMAXCHARS			30

struct P96MonitorInfo
{
	UBYTE	Name[32];	// Name des Monitortyps, z.B. "NEC P750"

	ULONG	HSyncMin;	// Minimal unterstützte Horizontalfrequenz in Hz
	ULONG	HSyncMax;	// Maximal unterstützte Horizontalfrequenz in Hz

	UWORD	VSyncMin;	// Minimal unterstützte Vertikalfrequenz in Hz
	UWORD	VSyncMax;	// Maximal unterstützte Vertikalfrequenz in Hz

	ULONG	Flags;		// Siehe unten
};

#define MIB_DPMS_StandBy	(0)	// Monitor unterstützt DPMS-Level "stand-by".
					// Dieses Feature ist optional, nicht jeder
					// DPMS-fähige Monitor muß es unterstützen.
					// Aktivierung: hsync aus, vsync an

#define MIB_DPMS_Suspend	(1)	// Monitor unterstützt DPMS-Level "suspend".
					// Dieses Feature ist Pflicht, jeder
					// DPMS-fähige Monitor muß es unterstützen.
					// Aktivierung: hsync an, vsync aus

#define MIB_DPMS_ActiveOff	(2)	// Monitor unterstützt DPMS-Level "active off".
					// Dieses Feature ist Pflicht, jeder
					// DPMS-fähige Monitor muß es unterstützen.
					// Aktivierung: hsync aus, vsync aus

#define MIF_DPMS_StandBy	(1UL << MIB_DPMS_StandBy)
#define MIF_DPMS_Suspend	(1UL << MIB_DPMS_Suspend)
#define MIF_DPMS_ActiveOff	(1UL << MIB_DPMS_ActiveOff)

struct Settings{
	struct Node			Node;
	struct MinList		Resolutions;
	ULONG					BoardType;
// a value discribing assignment to nth board local to boardtype
// to be used for reassignment when boards are added or removed.
	UWORD					LocalOrdering;
	WORD					LastSelected;
	char					NameField[SETTINGSNAMEMAXCHARS];
	char					*BoardName;
	struct P96MonitorInfo *MonitorInfo;
};

#define MAXRESOLUTIONNAMELENGTH 22

/********************************
 * only used within rtg.library *
 ********************************/
struct LibResolution{
	struct Node				Node;
	char						P96ID[6];
	char						Name[MAXRESOLUTIONNAMELENGTH];
	ULONG						DisplayID;
	UWORD						Width;
	UWORD						Height;
	UWORD						Flags;
	struct ModeInfo		*Modes[MAXMODES];
	struct BoardInfo		*BoardInfo;
	struct LibResolution	*HashChain;
};

/*****************************
 * only used within MoniTool *
 *****************************/
struct Resolution{
	struct Node		Node;
	struct MinList	ModeInfos;
	ULONG				DisplayID;
	UWORD				Width;
	UWORD				Height;
	BOOL				Active;
	WORD				LastSelected;
	UWORD				Flags;
	char				Name[MAXRESOLUTIONNAMELENGTH];
};

#define	P96B_FAMILY			0						// obsolete (Resolution is an entire family)
#define	P96B_PUBLIC			1						// Resolution should be added to the public
#define	P96B_MONITOOL		2

#define	P96B_CHECKME		15						// Resolution has been attached to another board
															// by AttachSettings without being checked against
															// hardware limits

#define	P96F_FAMILY			(1<<P96B_FAMILY)	// obsolete
#define	P96F_PUBLIC			(1<<P96B_PUBLIC)
#define	P96F_MONITOOL		(1<<P96B_MONITOOL)
#define	P96F_CHECKME		(1<<P96B_CHECKME)

/*
enum {
	DBLLORES_FLAGS,	// 0000		 320x200
	DBLHIRES_FLAGS,	// 8000		 640x200
	DBLSHIRES_FLAGS,	// 8020		1280x200
	LORES_FLAGS,		// 0004		 320x400
	HIRES_FLAGS,		// 8004		 640x400
	SHIRES_FLAGS,		// 8024		1280x400
	LORESLACE_FLAGS,	// 0005		 320x800
	HIRESLACE_FLAGS,	//	8005		 640x800
	SHIRESLACE_FLAGS,	// 8025		1280x800
	MAXFAMILYFLAGS
};
*/

/*****************************
 * this one describes a mode *
 *****************************/
struct ModeInfo{
	struct Node	Node;						// used for linking ModeInfos e.g. within MoniTool
	WORD			OpenCount;
	BOOL			Active;
	UWORD			Width;
	UWORD			Height;
	UBYTE			Depth;
	UBYTE			Flags;

	UWORD			HorTotal;				// wichtig für aufziehen (beeinflußt Timings)
	UWORD			HorBlankSize;			// Rahmengröße
	UWORD			HorSyncStart;			// bestimmt Bildlage
	UWORD			HorSyncSize;			// muß Spezifikation für Sync-Lücke erfüllen

	UBYTE			HorSyncSkew;			// im Moment obsolet
	UBYTE			HorEnableSkew;			//

	UWORD			VerTotal;				// analog zu horizontalen Werten
	UWORD			VerBlankSize;
	UWORD			VerSyncStart;
	UWORD			VerSyncSize;

	union{
		UBYTE		Clock;					// Tseng: Nummer der Hardwareclock
		UBYTE		Numerator;				// Cirrus: Mumerator für PLL
	} pll1;
	union{
		UBYTE		ClockDivide;			// Tseng: Clockteiler
		UBYTE		Denominator;			// Cirrus: Denominator für PLL
	} pll2;
	ULONG			PixelClock;				// PixelClock in Hz
};

/***********************************
* Flags: */

#define GMB_DOUBLECLOCK		0
#define GMB_INTERLACE		1
#define GMB_DOUBLESCAN		2
#define GMB_HPOLARITY		3
#define GMB_VPOLARITY		4
#define GMB_COMPATVIDEO		5
#define GMB_DOUBLEVERTICAL	6
#define GMB_ALWAYSBORDER	7			// only used by MoniTool-EditScreen

#define GMF_DOUBLECLOCK		(1L<<GMB_DOUBLECLOCK)
#define GMF_INTERLACE		(1L<<GMB_INTERLACE)
#define GMF_DOUBLESCAN		(1L<<GMB_DOUBLESCAN)
#define GMF_HPOLARITY		(1L<<GMB_HPOLARITY)
#define GMF_VPOLARITY		(1L<<GMB_VPOLARITY)
#define GMF_COMPATVIDEO		(1L<<GMB_COMPATVIDEO)
#define GMF_DOUBLEVERTICAL	(1L<<GMB_DOUBLEVERTICAL)
#define GMF_ALWAYSBORDER	(1L<<GMB_ALWAYSBORDER)

/************************************************************************/
#endif
