/*
  Hatari - configuration.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_CONFIGURATION_H
#define HATARI_CONFIGURATION_H


/* Configuration Dialog */
typedef struct
{
  bool bShowConfigDialogAtStartup;
} CNF_CONFIGDLG;

/* Logging and tracing */
typedef struct
{
  char sLogFileName[FILENAME_MAX];
  char sTraceFileName[FILENAME_MAX];
  int nTextLogLevel;
  int nAlertDlgLogLevel;
  bool bConfirmQuit;
} CNF_LOG;


/* Debugger */
typedef struct
{
  int nNumberBase;
  int nDisasmLines;
  int nMemdumpLines;
} CNF_DEBUGGER;


/* ROM configuration */
typedef struct
{
    char szRom030FileName[FILENAME_MAX];
    char szRom040FileName[FILENAME_MAX];
    char szRomTurboFileName[FILENAME_MAX];
} CNF_ROM;


/* Sound configuration */
typedef struct
{
  bool bEnableMicrophone;
  bool bEnableSound;
  int nPlaybackFreq;
  int SdlAudioBufferSize;
  char szYMCaptureFileName[FILENAME_MAX];
  int YmVolumeMixing;
} CNF_SOUND;



/* RS232 configuration */
typedef struct
{
  bool bEnableRS232;
  char szOutFileName[FILENAME_MAX];
  char szInFileName[FILENAME_MAX];
} CNF_RS232;


/* Dialog Keyboard */
typedef enum
{
  KEYMAP_SYMBOLIC,  /* Use keymapping with symbolic (ASCII) key codes */
  KEYMAP_SCANCODE,  /* Use keymapping with PC keyboard scancodes */
  KEYMAP_LOADED     /* Use keymapping with a map configuration file */
} KEYMAPTYPE;

typedef struct
{
  bool bDisableKeyRepeat;
  KEYMAPTYPE nKeymapType;
  char szMappingFileName[FILENAME_MAX];
} CNF_KEYBOARD;


typedef enum {
  SHORTCUT_OPTIONS,
  SHORTCUT_FULLSCREEN,
  SHORTCUT_MOUSEGRAB,
  SHORTCUT_COLDRESET,
  SHORTCUT_WARMRESET,
  SHORTCUT_SCREENSHOT,
  SHORTCUT_BOSSKEY,
  SHORTCUT_CURSOREMU,
  SHORTCUT_FASTFORWARD,
  SHORTCUT_RECANIM,
  SHORTCUT_RECSOUND,
  SHORTCUT_SOUND,
  SHORTCUT_DEBUG,
  SHORTCUT_PAUSE,
  SHORTCUT_QUIT,
  SHORTCUT_LOADMEM,
  SHORTCUT_SAVEMEM,
  SHORTCUT_INSERTDISKA,
  SHORTCUT_KEYS,  /* number of shortcuts */
  SHORTCUT_NONE
} SHORTCUTKEYIDX;

typedef struct
{
  int withModifier[SHORTCUT_KEYS];
  int withoutModifier[SHORTCUT_KEYS];
} CNF_SHORTCUT;


/* Dialog Mouse */
#define MOUSE_LIN_MIN   0.1
#define MOUSE_LIN_MAX   10.0
#define MOUSE_EXP_MIN   0.5
#define MOUSE_EXP_MAX   1.0
typedef struct
{
    bool bEnableAutoGrab;
    float fLinSpeedNormal;
    float fLinSpeedLocked;
    float fExpSpeedNormal;
    float fExpSpeedLocked;
} CNF_MOUSE;


/* Memory configuration */

typedef enum
{
  MEMORY_120NS,
  MEMORY_100NS,
  MEMORY_80NS,
  MEMORY_60NS
} MEMORY_SPEED;

typedef struct
{
  int nMemoryBankSize[4];
  MEMORY_SPEED nMemorySpeed;
  bool bAutoSave;
  char szMemoryCaptureFileName[FILENAME_MAX];
  char szAutoSaveFileName[FILENAME_MAX];
} CNF_MEMORY;


/* Dialog Boot options */

typedef enum
{
    BOOT_ROM,
    BOOT_SCSI,
    BOOT_ETHERNET,
    BOOT_MO,
    BOOT_FLOPPY
} BOOT_DEVICE;

typedef struct
{
    BOOT_DEVICE nBootDevice;
    bool bEnableDRAMTest;
    bool bEnablePot;
    bool bExtendedPot;
    bool bEnableSoundTest;
    bool bEnableSCSITest;
    bool bLoopPot;
    bool bVerbose;
} CNF_BOOT;


/* Disk image configuration */

typedef enum
{
  WRITEPROT_OFF,
  WRITEPROT_ON,
  WRITEPROT_AUTO
} WRITEPROTECTION;

#define MAX_FLOPPYDRIVES 2

typedef struct
{
  bool bAutoInsertDiskB;
  bool bSlowFloppy;                  /* true to slow down FDC emulation */
  WRITEPROTECTION nWriteProtection;
  char szDiskZipPath[MAX_FLOPPYDRIVES][FILENAME_MAX];
  char szDiskFileName[MAX_FLOPPYDRIVES][FILENAME_MAX];
  char szDiskImageDirectory[FILENAME_MAX];
} CNF_DISKIMAGE;


/* Hard drives configuration */
#define ESP_MAX_DEVS 7
typedef struct {
  char szImageName[FILENAME_MAX];
  bool bAttached;
  bool bCDROM;
} SCSIDISK;

typedef struct
{
  SCSIDISK target[ESP_MAX_DEVS];    
  
  WRITEPROTECTION nWriteProtection;
  bool bBootFromHardDisk;
} CNF_SCSI;


/* Magneto-optical drives configuration */
#define MO_MAX_DRIVES   2
typedef struct {
    char szImageName[FILENAME_MAX];
    bool bDriveConnected;
    bool bDiskInserted;
    bool bWriteProtected;
} MODISK;

typedef struct {
    MODISK drive[MO_MAX_DRIVES];
} CNF_MO;


/* Falcon register $FFFF8006 bits 6 & 7 (mirrored in $FFFF82C0 bits 0 & 1):
 * 00 Monochrome
 * 01 RGB - Colormonitor
 * 10 VGA - Colormonitor
 * 11 TV
 */
#define FALCON_MONITOR_MONO 0x00  /* SM124 */
#define FALCON_MONITOR_RGB  0x40
#define FALCON_MONITOR_VGA  0x80
#define FALCON_MONITOR_TV   0xC0

typedef enum
{
  MONITOR_TYPE_MONO,
  MONITOR_TYPE_RGB,
  MONITOR_TYPE_VGA,
  MONITOR_TYPE_TV
} MONITORTYPE;

/* Screen configuration */
typedef struct
{
  MONITORTYPE nMonitorType;
  int nFrameSkips;
  bool bFullScreen;
  bool bKeepResolution;
  bool bAllowOverscan;
  bool bAspectCorrect;
  bool bUseExtVdiResolutions;
  int nSpec512Threshold;
  int nForceBpp;
  int nVdiColors;
  int nVdiWidth;
  int nVdiHeight;
  bool bShowStatusbar;
  bool bShowDriveLed;
  bool bCrop;
  int nMaxWidth;
  int nMaxHeight;
} CNF_SCREEN;


/* Printer configuration */
typedef struct
{
  bool bEnablePrinting;
  bool bPrintToFile;
  char szPrintToFileName[FILENAME_MAX];
} CNF_PRINTER;


/* Midi configuration */
typedef struct
{
  bool bEnableMidi;
  char sMidiInFileName[FILENAME_MAX];
  char sMidiOutFileName[FILENAME_MAX];
} CNF_MIDI;


/* Dialog System */
typedef enum
{
  NEXT_CUBE030,
  NEXT_CUBE040,
  NEXT_STATION
} MACHINETYPE;

typedef enum
{
  NCR53C90,
  NCR53C90A
} SCSICHIP;

typedef enum
{
  MC68HC68T1,
  MCCS1850
} RTCCHIP;

typedef enum
{
  DSP_TYPE_NONE,
  DSP_TYPE_DUMMY,
  DSP_TYPE_EMU
} DSPTYPE;

typedef enum
{
  FPU_NONE = 0,
  FPU_68881 = 68881,
  FPU_68882 = 68882,
  FPU_CPU = 68040
} FPUTYPE;

typedef struct
{
  bool bColor;
  bool bTurbo;
  bool bADB;
  SCSICHIP nSCSI;
  RTCCHIP nRTC;
  int nCpuLevel;
  int nCpuFreq;
  bool bCompatibleCpu;            /* Prefetch mode */
  MACHINETYPE nMachineType;
  bool bBlitter;                  /* TRUE if Blitter is enabled */
  DSPTYPE nDSPType;               /* how to "emulate" DSP */
  bool bRealTimeClock;
  bool bPatchTimerD;
  bool bFastForward;
  bool bAddressSpace24;
  bool bCycleExactCpu;
  FPUTYPE n_FPUType;
  bool bCompatibleFPU;            /* More compatible FPU */
  bool bMMU;                      /* TRUE if MMU is enabled */
} CNF_SYSTEM;

typedef struct
{
  int AviRecordVcodec;
  int AviRecordFps;
  char AviRecordFile[FILENAME_MAX];
} CNF_VIDEO;



/* State of system is stored in this structure */
/* On reset, variables are copied into system globals and used. */
typedef struct
{
  /* Configure */
  CNF_CONFIGDLG ConfigDialog;
  CNF_LOG Log;
  CNF_DEBUGGER Debugger;
  CNF_SCREEN Screen;
  CNF_KEYBOARD Keyboard;
  CNF_SHORTCUT Shortcut;
  CNF_MOUSE Mouse;
  CNF_SOUND Sound;
  CNF_MEMORY Memory;
  CNF_DISKIMAGE DiskImage;
  CNF_BOOT Boot;
  CNF_SCSI SCSI;
  CNF_MO MO;
  CNF_ROM Rom;
  CNF_RS232 RS232;
  CNF_PRINTER Printer;
  CNF_MIDI Midi;
  CNF_SYSTEM System;
  CNF_VIDEO Video;
} CNF_PARAMS;


extern CNF_PARAMS ConfigureParams;
extern char sConfigFileName[FILENAME_MAX];

extern void Configuration_SetDefault(void);
extern void Configuration_SetSystemDefaults(void);
extern void Configuration_Apply(bool bReset);
extern int Configuration_CheckMemory(int *banksize);
extern void Configuration_Load(const char *psFileName);
extern void Configuration_Save(void);
extern void Configuration_MemorySnapShot_Capture(bool bSave);

#endif
