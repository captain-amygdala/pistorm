struct P96Line {
    int16_t     X, Y;
    uint16_t    Length;
    int16_t     dX, dY;
    int16_t     sDelta, lDelta, twoSDminusLD;
    uint16_t    LinePtrn;
    uint16_t    PatternShift;
    uint32_t    FgPen, BgPen;
    uint16_t    Horizontal;
    uint8_t     DrawMode;
    int8_t      pad;
    uint16_t    Xorigin, Yorigin;
};

#pragma pack(2)
struct P96Template {
    uint32_t _p_Memory;
    uint16_t BytesPerRow;
    uint8_t XOffset;
    uint8_t DrawMode;
    uint32_t FgPen;
    uint32_t BgPen;
};

#pragma pack(2)
struct P96Pattern {
    uint32_t _p_Memory;
    uint16_t XOffset, YOffset;
    uint32_t FgPen, BgPen;
    uint8_t Size; // Width: 16, Height: (1<<pat_Size)
    uint8_t DrawMode;
};

struct MinNode_placeholder {
    uint32_t _p_mln_Succ;
    uint32_t _p_mln_Pred;
};

#pragma pack(2)
struct Node_placeholder {
    uint32_t _p_ln_Succ;
    uint32_t _p_ln_Pred;
    uint8_t shit[2];
    uint32_t _p_ln_Name;
};

struct BitMap {
    uint16_t BytesPerRow;
    uint16_t Rows;
    uint8_t Flags;
    uint8_t Depth;
    uint16_t pad;
    uint32_t _p_Planes[8];
};

struct MinList_placeholder {
   uint32_t _p_mlh_Head;
   uint32_t _p_mlh_Tail;
   uint32_t _p_mlh_TailPred;
};

struct List_placeholder {
   uint32_t _p_lh_Head;
   uint32_t _p_lh_Tail;
   uint32_t _p_lh_TailPred;
   uint8_t lh_Type;
   uint8_t l_pad;
};

struct SemaphoreRequest_placeholder {
    struct MinNode_placeholder sr_Link;
    uint32_t _p_sr_Waiter;
};

struct SignalSemaphore_placeholder {
    struct Node_placeholder ss_Link;
    int16_t ss_NestCount;
    struct MinList_placeholder ss_WaitQueue;
    struct SemaphoreRequest_placeholder ss_MultipleLink;
    uint32_t _p_ss_Owner;
    int16_t ss_QueueCount;
};

struct Interrupt_placeholder {
    struct Node_placeholder is_Node;
    uint32_t _p_is_Data;
    uint32_t _p_is_Code;
};

struct MsgPort_placeholder {
    struct Node_placeholder mp_Node;
    uint8_t mp_Flags;
    uint8_t mp_SigBit;
    uint32_t _p_mp_SigTask;
    struct List_placeholder mp_MsgList;
};

struct P96Rectangle {
    int16_t MinX,MinY;
    int16_t MaxX,MaxY;
};

struct CLUTEntry {
    uint8_t Red;
    uint8_t Green;
    uint8_t Blue;
};

struct timeval_placeholder {
    uint32_t tv_secs;
    uint32_t tv_micro;
};

#define MAXMODES 5

struct ModeInfo_placeholder {
    struct Node_placeholder Node;
    int16_t OpenCount;
    uint32_t Active;
    uint16_t Width;
    uint16_t Height;
    uint8_t Depth;
    uint8_t Flags;

    uint16_t HorTotal;
    uint16_t HorBlankSize;
    uint16_t HorSyncStart;
    uint16_t HorSyncSize;

    uint8_t HorSyncSkew;
    uint8_t HorEnableSkew;

    uint16_t VerTotal;
    uint16_t VerBlankSize;
    uint16_t VerSyncStart;
    uint16_t VerSyncSize;

    union {
        uint8_t Clock;
        uint8_t Numerator;
    } pll1;
    union {
        uint8_t ClockDivide;
        uint8_t Denominator;
    } pll2;
    uint32_t PixelClock;
};

struct P96RenderInfo {
	uint32_t _p_Memory;
	int16_t BytesPerRow;
	int16_t pad;
	uint32_t RGBFormat;
};

struct P96BoardInfo{
    uint32_t _p_RegisterBase, _p_MemoryBase, _p_MemoryIOBase;
    uint32_t MemorySize;
    uint32_t _p_BoardName;
    int8_t VBIName[32];
    uint32_t _p_CardBase;
    uint32_t _p_ChipBase;
    uint32_t _p_ExecBase;
    uint32_t _p_UtilBase;
    struct Interrupt_placeholder HardInterrupt;
    struct Interrupt_placeholder SoftInterrupt;
    struct SignalSemaphore_placeholder BoardLock;
    struct MinList_placeholder ResolutionsList;
    uint32_t BoardType;
    uint32_t PaletteChipType;
    uint32_t GraphicsControllerType;
    uint16_t MoniSwitch;
    uint16_t BitsPerCannon;
    uint32_t Flags;
    uint16_t SoftSpriteFlags;
    uint16_t ChipFlags;
    uint32_t CardFlags;

    uint16_t BoardNum;
    uint16_t RGBFormats;

    uint16_t MaxHorValue[MAXMODES];
    uint16_t MaxVerValue[MAXMODES];
    uint16_t MaxHorResolution[MAXMODES];
    uint16_t MaxVerResolution[MAXMODES];
    uint32_t MaxMemorySize, MaxChunkSize;

    uint32_t MemoryClock;

    uint32_t PixelClockCount[MAXMODES];

    uint32_t _p_AllocCardMem;
    uint32_t _p_FreeCardMem;

    uint32_t _p_SetSwitch;

    uint32_t _p_SetColorArray;

    uint32_t _p_SetDAC;
    uint32_t _p_SetGC;
    uint32_t _p_SetPanning;
    uint32_t _p_CalculateBytesPerRow;
    uint32_t _p_CalculateMemory;
    uint32_t _p_GetCompatibleFormats;
    uint32_t _p_SetDisplay;

    uint32_t _p_ResolvePixelClock;
    uint32_t _p_GetPixelClock;
    uint32_t _p_SetClock;

    uint32_t _p_SetMemoryMode;
    uint32_t _p_SetWriteMask;
    uint32_t _p_SetClearMask;
    uint32_t _p_SetReadPlane;

    uint32_t _p_WaitVerticalSync;
    uint32_t _p_SetInterrupt;

    uint32_t _p_WaitBlitter;

    uint32_t _p_ScrollPlanar;
    uint32_t _p_ScrollPlanarDefault;
    uint32_t _p_UpdatePlanar;
    uint32_t _p_UpdatePlanarDefault;
    uint32_t _p_BlitPlanar2Chunky;
    uint32_t _p_BlitPlanar2ChunkyDefault;

    uint32_t _p_FillRect;
    uint32_t _p_FillRectDefault;
    uint32_t _p_InvertRect;
    uint32_t _p_InvertRectDefault;
    uint32_t _p_BlitRect;
    uint32_t _p_BlitRectDefault;
    uint32_t _p_BlitTemplate;
    uint32_t _p_BlitTemplateDefault;
    uint32_t _p_BlitPattern;
    uint32_t _p_BlitPatternDefault;
    uint32_t _p_DrawLine;
    uint32_t _p_DrawLineDefault;
    uint32_t _p_BlitRectNoMaskComplete;
    uint32_t _p_BlitRectNoMaskCompleteDefault;
    uint32_t _p_BlitPlanar2Direct;
    uint32_t _p_BlitPlanar2DirectDefault;
    uint32_t _p_EnableSoftSprite;
    uint32_t _p_EnableSoftSpriteDefault;
    uint32_t _p_AllocCardMemAbs;
    uint32_t _p_SetSplitPosition;
    uint32_t _p_ReInitMemory;
    uint32_t _p_Reserved2Default;
    uint32_t _p_Reserved3;
    uint32_t _p_Reserved3Default;

    uint32_t _p_WriteYUVRect;
    uint32_t _p_WriteYUVRectDefault;

    uint32_t _p_GetVSyncState;
    uint32_t _p_GetVBeamPos;
    uint32_t _p_SetDPMSLevel;
    uint32_t _p_ResetChip;
    uint32_t _p_GetFeatureAttrs;

    uint32_t _p_AllocBitMap;
    uint32_t _p_FreeBitMap;
    uint32_t _p_GetBitMapAttr;
    uint32_t _p_SetSprite;
    uint32_t _p_SetSpritePosition;
    uint32_t _p_SetSpriteImage;
    uint32_t _p_SetSpriteColor;
    uint32_t _p_CreateFeature;
    uint32_t _p_SetFeatureAttrs;
    uint32_t _p_DeleteFeature;
    struct MinList_placeholder SpecialFeatures;

    uint32_t _p_ModeInfo;
    uint32_t RGBFormat;
    int16_t XOffset;
    int16_t YOffset;
    uint8_t Depth;
    uint8_t ClearMask;
    uint16_t Border;
    uint32_t Mask;
    uint8_t CLUT[256 * 3];

    uint32_t _p_ViewPort;
    uint32_t _p_VisibleBitMap;
    uint32_t _p_BitMapExtra;
    struct MinList_placeholder BitMapList;
    struct MinList_placeholder MemList;

    int16_t MouseX;
    int16_t MouseY;
    uint8_t MouseWidth;
    uint8_t MouseHeight;
    uint8_t MouseXOffset;
    uint8_t MouseYOffset;
    uint32_t _p_MouseImage;
    uint8_t MousePens[4];
    struct P96Rectangle MouseRect;
    uint32_t _p_MouseChunky;
    uint32_t _p_MouseRendered;
    uint32_t _p_MouseSaveBuffer;

    uint32_t ChipData[16];
    uint32_t CardData[16];

    uint32_t _p_MemorySpaceBase;
    uint32_t MemorySpaceSize;

    uint32_t _p_DoubleBufferList;

    struct timeval_placeholder SyncTime;
    uint32_t SyncPeriod;
    struct MsgPort_placeholder SoftVBlankPort;

    struct MinList_placeholder WaitQ;
    
    int32_t EssentialFormats;
    uint32_t _p_MouseImageBuffer;

    uint32_t _p_backViewPort;
    uint32_t _p_backBitMap;
    uint32_t _p_backExtra;
    int16_t YSplit;
    uint32_t MaxPlanarMemory;
    uint32_t MaxBMWidth;
    uint32_t MaxBMHeight;
};
