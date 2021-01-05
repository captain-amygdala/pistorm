// PiStorm RTG driver, VBCC edition.
// Based in part on the ZZ9000 RTG driver.

#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/debug_protos.h>
#include <string.h>
#include <stdio.h>
#include "boardinfo.h"

#define WRITESHORT(cmd, val) *(unsigned short *)((unsigned int)(b->RegisterBase)+cmd) = val;
#define WRITELONG(cmd, val) *(unsigned int *)((unsigned int)(b->RegisterBase)+cmd) = val;
#define WRITEBYTE(cmd, val) *(unsigned char *)((unsigned int)(b->RegisterBase)+cmd) = val;

#define CARD_OFFSET  0x70000000
#define CARD_REGSIZE 0x00010000
// 32MB "VRAM"
#define CARD_MEMSIZE 0x02000000

// "Register" offsets for sending data to the RTG.
enum pi_regs {
  RTG_COMMAND = 0x00,
  RTG_X1      = 0x02,
  RTG_X2      = 0x04,
  RTG_X3      = 0x06,
  RTG_Y1      = 0x08,
  RTG_Y2      = 0x0A,
  RTG_Y3      = 0x0C,
  RTG_FORMAT  = 0x0E,
  RTG_RGB1    = 0x10,
  RTG_RGB2    = 0x14,
  RTG_ADDR1   = 0x18,
  RTG_ADDR2   = 0x1C,
  RTG_U81     = 0x20,
  RTG_U82     = 0x21,
  RTG_U83     = 0x22,
  RTG_U84     = 0x23,
};

enum rtg_cmds {
  RTGCMD_SETGC,
  RTGCMD_SETPAN,
  RTGCMD_SETCLUT,
  RTGCMD_ENABLE,
  RTGCMD_SETDISPLAY,
  RTGCMD_SETSWITCH,
  RTGCMD_FILLRECT,
};

enum rtg_formats {
  RTGFMT_8BIT,
  RTGFMT_RBG565,
  RTGFMT_RGB32,
  RTGFMT_RGB555,
};

const unsigned short rgbf_to_rtg[16] = {
  RTGFMT_8BIT,      // 0x00
  RTGFMT_8BIT,      // 0x01
  0,                // 0x02
  0,                // 0x03
  0,                // 0x04
  RTGFMT_RGB555,    // 0x05
  0,                // 0x06
  0,                // 0x07
  RTGFMT_RGB32,     // 0x08
  RTGFMT_RGB32,     // 0x09
  RTGFMT_RBG565,    // 0x0A
  RTGFMT_RGB555,    // 0x0B
  0,                // 0x0C
  RTGFMT_RGB555,    // 0x0D
  0,                // 0x0E
  0,                // 0x0F
};

struct GFXBase {
  struct Library libNode;
  BPTR segList;
  struct ExecBase* sysBase;
  struct ExpansionBase* expansionBase;
};

int FindCard(__REGA0(struct BoardInfo* b));
int InitCard(__REGA0(struct BoardInfo* b));

void SetDAC (__REGA0(struct BoardInfo *b), __REGD7(RGBFTYPE format));
void SetGC (__REGA0(struct BoardInfo *b), __REGA1(struct ModeInfo *mode_info), __REGD0(BOOL border));
void SetColorArray (__REGA0(struct BoardInfo *b), __REGD0(UWORD start), __REGD1(UWORD num));
void SetPanning (__REGA0(struct BoardInfo *b), __REGA1(UBYTE *addr), __REGD0(UWORD width), __REGD1(WORD x_offset), __REGD2(WORD y_offset), __REGD7(RGBFTYPE format));
BOOL SetSwitch (__REGA0(struct BoardInfo *b), __REGD0(BOOL enabled));
BOOL SetDisplay (__REGA0(struct BoardInfo *b), __REGD0(BOOL enabled));

UWORD CalculateBytesPerRow (__REGA0(struct BoardInfo *b), __REGD0(UWORD width), __REGD7(RGBFTYPE format));
APTR CalculateMemory (__REGA0(struct BoardInfo *b), __REGA1(unsigned int addr), __REGD7(RGBFTYPE format));
ULONG GetCompatibleFormats (__REGA0(struct BoardInfo *b), __REGD7(RGBFTYPE format));

LONG ResolvePixelClock (__REGA0(struct BoardInfo *b), __REGA1(struct ModeInfo *mode_info), __REGD0(ULONG pixel_clock), __REGD7(RGBFTYPE format));
ULONG GetPixelClock (__REGA0(struct BoardInfo *b), __REGA1(struct ModeInfo *mode_info), __REGD0(ULONG index), __REGD7(RGBFTYPE format));
void SetClock (__REGA0(struct BoardInfo *b));

void SetMemoryMode (__REGA0(struct BoardInfo *b), __REGD7(RGBFTYPE format));
void SetWriteMask (__REGA0(struct BoardInfo *b), __REGD0(UBYTE mask));
void SetClearMask (__REGA0(struct BoardInfo *b), __REGD0(UBYTE mask));
void SetReadPlane (__REGA0(struct BoardInfo *b), __REGD0(UBYTE plane));

void WaitVerticalSync (__REGA0(struct BoardInfo *b), __REGD0(BOOL toggle));

void FillRect (__REGA0(struct BoardInfo *b), __REGA1(struct RenderInfo *r), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD w), __REGD3(WORD h), __REGD4(ULONG color), __REGD5(UBYTE mask), __REGD7(RGBFTYPE format));

static ULONG LibStart(void) {
  return(-1);
}

static const char LibraryName[] = "PiRTG.card";
static const char LibraryID[]   = "$VER: PiRTG.card 0.01\r\n";

__saveds struct GFXBase* OpenLib(__REGA6(struct GFXBase *gfxbase));
BPTR __saveds CloseLib(__REGA6(struct GFXBase *gfxbase));
BPTR __saveds ExpungeLib(__REGA6(struct GFXBase *exb));
ULONG ExtFuncLib(void);
__saveds struct GFXBase* InitLib(__REGA6(struct ExecBase *sysbase),
                                 __REGA0(BPTR seglist),
                                 __REGD0(struct GFXBase *exb));

static const APTR FuncTab[] = {
  (APTR)OpenLib,
  (APTR)CloseLib,
  (APTR)ExpungeLib,
  (APTR)ExtFuncLib,

  (APTR)FindCard,
  (APTR)InitCard,
  (APTR)((LONG)-1)
};

struct InitTable
{
  ULONG LibBaseSize;
  APTR  FunctionTable;
  APTR  DataTable;
  APTR  InitLibTable;
};

static struct InitTable InitTab = {
  (ULONG) sizeof(struct GFXBase),
  (APTR) FuncTab,
  (APTR) NULL,
  (APTR) InitLib
};

static const struct Resident ROMTag = {
	RTC_MATCHWORD,
  &ROMTag,
  &ROMTag + 1,
  RTF_AUTOINIT,
	83,
  NT_LIBRARY,
  0,
  (char *)LibraryName,
  (char *)LibraryID,
  (APTR)&InitTab
};

#define CLOCK_HZ 100000000

static struct GFXBase *_gfxbase;
const char *gfxname = "PiStorm RTG";
char dummies[128];

__saveds struct GFXBase* InitLib(__REGA6(struct ExecBase *sysbase),
                                          __REGA0(BPTR seglist),
                                          __REGD0(struct GFXBase *exb))
{
  _gfxbase = exb;
  return _gfxbase;
}

__saveds struct GFXBase* OpenLib(__REGA6(struct GFXBase *gfxbase))
{
  gfxbase->libNode.lib_OpenCnt++;
  gfxbase->libNode.lib_Flags &= ~LIBF_DELEXP;

  return gfxbase;
}

BPTR __saveds CloseLib(__REGA6(struct GFXBase *gfxbase))
{
  gfxbase->libNode.lib_OpenCnt--;

  if (!gfxbase->libNode.lib_OpenCnt) {
    if (gfxbase->libNode.lib_Flags & LIBF_DELEXP) {
      return (ExpungeLib(gfxbase));
    }
  }
  return 0;
}

BPTR __saveds ExpungeLib(__REGA6(struct GFXBase *exb))
{
  BPTR seglist;
  struct ExecBase *SysBase = *(struct ExecBase **)4L;

  if(!exb->libNode.lib_OpenCnt) {
    ULONG negsize, possize, fullsize;
    UBYTE *negptr = (UBYTE *)exb;

    seglist = exb->segList;

    Remove((struct Node *)exb);

    negsize  = exb->libNode.lib_NegSize;
    possize  = exb->libNode.lib_PosSize;
    fullsize = negsize + possize;
    negptr  -= negsize;

    FreeMem(negptr, fullsize);
    return(seglist);
  }

  exb->libNode.lib_Flags |= LIBF_DELEXP;
  return 0;
}

ULONG ExtFuncLib(void)
{
  return 0;
}

static LONG zorro_version = 0;

static struct GFXData *gfxdata;
//MNTZZ9KRegs* registers;

#define LOADLIB(a, b) if ((a = (struct a*)OpenLibrary(b,0L))==NULL) { \
    KPrintF("Failed to load %s.\n", b); \
    return 0; \
  } \

static BYTE card_already_found;
static BYTE card_initialized;

int FindCard(__REGA0(struct BoardInfo* b)) {
  //if (card_already_found)
//    return 1;
  struct ConfigDev* cd = NULL;
  struct ExpansionBase *ExpansionBase = NULL;
  struct DOSBase *DOSBase = NULL;
  struct IntuitionBase *IntuitionBase = NULL;
  struct ExecBase *SysBase = *(struct ExecBase **)4L;

  LOADLIB(ExpansionBase, "expansion.library");
  LOADLIB(DOSBase, "dos.library");
  LOADLIB(IntuitionBase, "intuition.library");

  b->MemorySize = CARD_MEMSIZE;
  b->RegisterBase = (void *)CARD_OFFSET;
  b->MemoryBase = (void *)(CARD_OFFSET + CARD_REGSIZE);

  return 1;
}

#define HWSPRITE 1
#define VGASPLIT (1 << 6)
#define FLICKERFIXER (1 << 12)
#define INDISPLAYCHAIN (1 << 20)
#define DIRECTACCESS (1 << 26)

int InitCard(__REGA0(struct BoardInfo* b)) {
  //if (!card_initialized)
//    card_initialized = 1;
//  else
    //return 1;

  int max, i;
  struct ExecBase *SysBase = *(struct ExecBase **)4L;

  b->CardBase = (struct CardBase *)_gfxbase;
  b->ExecBase = SysBase;
  b->BoardName = "PiStorm RTG";
  b->BoardType = BT_MNT_ZZ9000;
  b->PaletteChipType = PCT_MNT_ZZ9000;
  b->GraphicsControllerType = GCT_MNT_ZZ9000;

  b->Flags = BIF_INDISPLAYCHAIN | BIF_GRANTDIRECTACCESS;
  b->RGBFormats = 1 | 2 | 512 | 1024 | 2048;
  b->SoftSpriteFlags = 0;
  b->BitsPerCannon = 8;

  for(i = 0; i < MAXMODES; i++) {
    b->MaxHorValue[i] = 1920;
    b->MaxVerValue[i] = 1080;
    b->MaxHorResolution[i] = 1920;
    b->MaxVerResolution[i] = 1080;
    b->PixelClockCount[i] = 1;
  }

  b->MemoryClock = CLOCK_HZ;

  //b->AllocCardMem = (void *)NULL;
  //b->FreeCardMem = (void *)NULL;
  b->SetSwitch = (void *)SetSwitch;
  b->SetColorArray = (void *)SetColorArray;
  b->SetDAC = (void *)SetDAC;
  b->SetGC = (void *)SetGC;
  b->SetPanning = (void *)SetPanning;
  b->CalculateBytesPerRow = (void *)CalculateBytesPerRow;
  b->CalculateMemory = (void *)CalculateMemory;
  b->GetCompatibleFormats = (void *)GetCompatibleFormats;
  b->SetDisplay = (void *)SetDisplay;

  b->ResolvePixelClock = (void *)ResolvePixelClock;
  b->GetPixelClock = (void *)GetPixelClock;
  b->SetClock = (void *)SetClock;

  b->SetMemoryMode = (void *)SetMemoryMode;
  b->SetWriteMask = (void *)SetWriteMask;
  b->SetClearMask = (void *)SetClearMask;
  b->SetReadPlane = (void *)SetReadPlane;

  b->WaitVerticalSync = (void *)WaitVerticalSync;
  //b->SetInterrupt = (void *)NULL;

  //b->WaitBlitter = (void *)NULL;

  //b->ScrollPlanar = (void *)NULL;
  //b->UpdatePlanar = (void *)NULL;

  //b->BlitPlanar2Chunky = (void *)NULL;
  //b->BlitPlanar2Direct = (void *)NULL;

  b->FillRect = (void *)FillRect;
  //b->InvertRect = (void *)NULL;
  //b->BlitRect = (void *)NULL;
  //b->BlitTemplate = (void *)NULL;
  //b->BlitPattern = (void *)NULL;
  //b->DrawLine = (void *)NULL;
  //b->BlitRectNoMaskComplete = (void *)NULL;
  //b->EnableSoftSprite = (void *)NULL;

  //b->AllocCardMemAbs = (void *)NULL;
  //b->SetSplitPosition = (void *)NULL;
  //b->ReInitMemory = (void *)NULL;
  //b->WriteYUVRect = (void *)NULL;
  //b->GetVSyncState = (void *)NULL;
  //b->GetVBeamPos = (void *)NULL;
  //b->SetDPMSLevel = (void *)NULL;
  //b->ResetChip = (void *)NULL;
  //b->GetFeatureAttrs = (void *)NULL;
  //b->AllocBitMap = (void *)NULL;
  //b->FreeBitMap = (void *)NULL;
  //b->GetBitMapAttr = (void *)NULL;

  //b->SetSprite = (void *)NULL;
  //b->SetSpritePosition = (void *)NULL;
  //b->SetSpriteImage = (void *)NULL;
  //b->SetSpriteColor = (void *)NULL;

  //b->CreateFeature = (void *)NULL;
  //b->SetFeatureAttrs = (void *)NULL;
  //b->DeleteFeature = (void *)NULL;

  return 1;
}

void SetDAC (__REGA0(struct BoardInfo *b), __REGD7(RGBFTYPE format)) {
  // Used to set the color format of the video card's RAMDAC.
  // This needs no handling, since the PiStorm doesn't really have a RAMDAC or a video card chipset.
}

void SetGC (__REGA0(struct BoardInfo *b), __REGA1(struct ModeInfo *mode_info), __REGD0(BOOL border)) {
  b->ModeInfo = mode_info;
  // Send width, height and format to the RaspberryPi Targetable Graphics.
  WRITESHORT(RTG_X1, mode_info->Width);
  WRITESHORT(RTG_Y1, mode_info->Height);
  WRITESHORT(RTG_FORMAT, rgbf_to_rtg[b->RGBFormat]);
  WRITESHORT(RTG_COMMAND, RTGCMD_SETGC);
}

int setswitch = 0;
BOOL SetSwitch (__REGA0(struct BoardInfo *b), __REGD0(BOOL enabled)) {
  WRITEBYTE(RTG_U81, (unsigned char)enabled);
  WRITESHORT(RTG_COMMAND, RTGCMD_SETSWITCH);
  if (setswitch != enabled) {
    b->MoniSwitch = setswitch;
    setswitch = enabled;
  }

  return b->MoniSwitch;
}

void SetPanning (__REGA0(struct BoardInfo *b), __REGA1(UBYTE *addr), __REGD0(UWORD width), __REGD1(WORD x_offset), __REGD2(WORD y_offset), __REGD7(RGBFTYPE format)) {
  // Set the panning offset, or the offset used for the current display area on the Pi.
  // The address needs to have CARD_BASE subtracted from it to be used as an offset on the Pi side.
  if (!b)
    return;

  b->XOffset = x_offset;
  b->YOffset = y_offset;

  WRITELONG(RTG_ADDR1, (unsigned int)addr);
  WRITESHORT(RTG_X1, width);
  WRITESHORT(RTG_X2, b->XOffset);
  WRITESHORT(RTG_Y2, b->YOffset);
  WRITESHORT(RTG_COMMAND, RTGCMD_SETPAN);
}

void SetColorArray (__REGA0(struct BoardInfo *b), __REGD0(UWORD start), __REGD1(UWORD num)) {
  // Sets the color components of X color components for 8-bit paletted display modes.
  if (!b->CLUT)
    return;
  for(int i = start; i < num; i++) {
    WRITEBYTE(RTG_U81, (unsigned char)i);
    WRITEBYTE(RTG_U82, (unsigned char)b->CLUT[i].Red);
    WRITEBYTE(RTG_U83, (unsigned char)b->CLUT[i].Green);
    WRITEBYTE(RTG_U84, (unsigned char)b->CLUT[i].Blue);
    WRITESHORT(RTG_COMMAND, RTGCMD_SETCLUT);
  }
}

UWORD CalculateBytesPerRow (__REGA0(struct BoardInfo *b), __REGD0(UWORD width), __REGD7(RGBFTYPE format)) {
  if (!b)
    return 0;

  UWORD pitch = width;

  switch(format) {
    default:
      return pitch;
    case 0x05: case 0x0A: case 0x0B: case 0x0D:
      return (width * 2);
    case 0x08: case 0x09:
      return (width * 4);
  }
}

APTR CalculateMemory (__REGA0(struct BoardInfo *b), __REGA1(unsigned int addr), __REGD7(RGBFTYPE format)) {
  if (!b)
    return (APTR)addr;

  if (addr > (unsigned int)b->MemoryBase && addr < (((unsigned int)b->MemoryBase) + b->MemorySize)) {
    addr = ((addr + 0x1000) & 0xFFFFF000);
  }

  return (APTR)addr;
}

ULONG GetCompatibleFormats (__REGA0(struct BoardInfo *b), __REGD7(RGBFTYPE format)) {
  // It is of course compatible with all the formats ever.
  return 0xFFFFFFFF;
}

static int display_enabled = 0;
BOOL SetDisplay (__REGA0(struct BoardInfo *b), __REGD0(BOOL enabled)) {
  if (!b)
    return 0;

  // Enables or disables the display.
  WRITEBYTE(RTG_U82, (unsigned char)enabled);
  WRITESHORT(RTG_COMMAND, RTGCMD_SETDISPLAY);

  return 1;
}

LONG ResolvePixelClock (__REGA0(struct BoardInfo *b), __REGA1(struct ModeInfo *mode_info), __REGD0(ULONG pixel_clock), __REGD7(RGBFTYPE format)) {
  mode_info->PixelClock = CLOCK_HZ;
  mode_info->pll1.Clock = 0;
  mode_info->pll2.ClockDivide = 1;

  return 0;
}

ULONG GetPixelClock (__REGA0(struct BoardInfo *b), __REGA1(struct ModeInfo *mode_info), __REGD0(ULONG index), __REGD7(RGBFTYPE format)) {
  // Just return 100MHz.
  return CLOCK_HZ;
}

// None of these five really have to do anything.
void SetClock (__REGA0(struct BoardInfo *b)) {
}

void SetMemoryMode (__REGA0(struct BoardInfo *b), __REGD7(RGBFTYPE format)) {
}

void SetWriteMask (__REGA0(struct BoardInfo *b), __REGD0(UBYTE mask)) {
}

void SetClearMask (__REGA0(struct BoardInfo *b), __REGD0(UBYTE mask)) {
}

void SetReadPlane (__REGA0(struct BoardInfo *b), __REGD0(UBYTE plane)) {
}

void WaitVerticalSync (__REGA0(struct BoardInfo *b), __REGD0(BOOL toggle)) {
  // I don't know why this one has a bool in D0, but it isn't used for anything.
}

void FillRect (__REGA0(struct BoardInfo *b), __REGA1(struct RenderInfo *r), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD w), __REGD3(WORD h), __REGD4(ULONG color), __REGD5(UBYTE mask), __REGD7(RGBFTYPE format)) {
  if (!r)
    return;
  if (mask != 0xFF)
    b->FillRectDefault(b, r, x, y, w, h, color, mask, format);
  
  WRITESHORT(RTG_FORMAT, rgbf_to_rtg[format]);
  WRITESHORT(RTG_X1, x);
  WRITESHORT(RTG_X2, w);
  WRITESHORT(RTG_Y1, y);
  WRITESHORT(RTG_Y2, h);
  WRITELONG(RTG_RGB1, color);
  WRITESHORT(RTG_X3, r->BytesPerRow);
  WRITESHORT(RTG_COMMAND, RTGCMD_FILLRECT);
}