	IFND LIBRARIES_REQTOOLS_I
LIBRARIES_REQTOOLS_I SET 1
**
**	$Filename: libraries/reqtools.i $
**	$Release: 2.2 $
**	$Revision: 38.11 $
**
**	reqtools.library definitions
**
**	(C) Copyright 1991-1994 Nico François
**	All Rights Reserved
**

   IFND EXEC_LISTS_I
   include "exec/lists.i"
   ENDC

   IFND EXEC_LIBRARIES_I
   include "exec/libraries.i"
   ENDC

   IFND EXEC_SEMAPHORES_I
   include "exec/semaphores.i"
   ENDC

   IFND LIBRARIES_DOS_I
   include "libraries/dos.i"
   ENDC

   IFND LIBRARIES_DOSEXTENS_I
   include "libraries/dosextens.i"
   ENDC

   IFND GRAPHICS_TEXT_I
   include "graphics/text.i"
   ENDC

   IFND UTILITY_TAGITEM_I
   include "utility/tagitem.i"
   ENDC

REQTOOLSNAME   MACRO
   dc.b "reqtools.library",0
   ENDM

REQTOOLSVERSION		equ	 38

************************
*                      *
*     Preferences      *
*                      *
************************

RTPREF_FILEREQ		equ	 0
RTPREF_FONTREQ		equ	 1
RTPREF_PALETTEREQ	equ	 2
RTPREF_SCREENMODEREQ	equ	 3
RTPREF_VOLUMEREQ	equ	 4
RTPREF_OTHERREQ		equ	 5
RTPREF_NR_OF_REQ	equ	 6

   STRUCTURE ReqDefaults,0
      ULONG rtrd_Size
      ULONG rtrd_ReqPos
      UWORD rtrd_LeftOffset
      UWORD rtrd_TopOffset
      UWORD rtrd_MinEntries
      UWORD rtrd_MaxEntries
      LABEL ReqDefaults_SIZE

   STRUCTURE ReqToolsPrefs,0
      * Size of preferences (_without_ this field and the semaphore)
      ULONG  rtpr_PrefsSize
      STRUCT rtpr_PrefsSemaphore,SS_SIZE
      * Start of real preferences
      ULONG  rtpr_Flags
      STRUCT rtpr_ReqDefaults,RTPREF_NR_OF_REQ*ReqDefaults_SIZE
      LABEL ReqToolsPrefs_SIZE

RTPREFS_SIZE	equ 	 (ReqToolsPrefs_SIZE-SS_SIZE-4)

* Flags

   BITDEF RTPR,DIRSFIRST,0
   BITDEF RTPR,DIRSMIXED,1
   BITDEF RTPR,IMMSORT,2
   BITDEF RTPR,NOSCRTOFRONT,3
   BITDEF RTPR,NOLED,4

************************
*                      *
*     Library Base     *
*                      *
************************

   STRUCTURE ReqToolsBase,LIB_SIZE
      UBYTE  rt_RTFlags
      STRUCT rt_pad,3
      ULONG  rt_SegList

      * PUBLIC FIELDS *

      * The following library bases may be read and used by your program
      APTR   rt_IntuitionBase
      APTR   rt_GfxBase
      APTR   rt_DOSBase
      * Next two library bases are only (and always) valid on Kickstart 2.0!
      * (1.3 version of reqtools also initializes these when run on 2.0)
      APTR   rt_GadToolsBase
      APTR   rt_UtilityBase

      * PRIVATE FIELDS, THESE WILL CHANGE FROM RELEASE TO RELEASE!

      * The RealOpenCnt is for the buffered AvailFonts feature.  Since
      * Kickstart 3.0 offers low memory handlers a release of ReqTools for
      * 3.0 will not use this field and start using the normal OpenCnt again.
      UWORD  rt_RealOpenCnt
      UWORD  rt_AvailFontsLock
      APTR   rt_AvailFontsHeader
      ULONG  rt_FontsAssignType
      BPTR   rt_FontsAssignLock
      APTR   rt_FontsAssignList
      STRUCT rt_ReqToolsPrefs,ReqToolsPrefs_SIZE
      UWORD  rt_prefspad
      LABEL  ReqToolsBase_SIZE

* types of requesters, for rtAllocRequestA()
RT_FILEREQ		equ	 0
RT_REQINFO		equ	 1
RT_FONTREQ		equ	 2
* (V38) *
RT_SCREENMODEREQ	equ	 3

************************
*                      *
*    File requester    *
*                      *
************************

* structure _MUST_ be allocated with rtAllocRequest()

   STRUCTURE rtFileRequester,0
      ULONG rtfi_ReqPos
      UWORD rtfi_LeftOffset
      UWORD rtfi_TopOffset
      ULONG rtfi_Flags
      ULONG rtfi_private1
      APTR  rtfi_Dir		* READ ONLY! Change with rtChangeReqAttrA()!
      APTR  rtfi_MatchPat	* READ ONLY! Change with rtChangeReqAttrA()!
      APTR  rtfi_DefaultFont
      ULONG rtfi_WaitPointer
      * (V38) *
      ULONG rtfi_LockWindow
      ULONG rtfi_ShareIDCMP
      APTR  rtfi_IntuiMsgFunc
      UWORD rtfi_reserved1
      UWORD rtfi_reserved2
      UWORD rtfi_reserved3
      UWORD rtfi_ReqHeight	* READ ONLY!  Use RTFI_Height tag!
      * Lots of private data follows! HANDS OFF :-)

* returned by rtFileRequestA() if multiselect is enabled,
* free list with rtFreeFileList()

   STRUCTURE rtFileList,0
      APTR  rtfl_Next
      ULONG rtfl_StrLen
      APTR  rtfl_Name
      LABEL rtFileList_SIZE

* structure passed to RTFI_FilterFunc callback hook by
* volume requester (see RTFI_VolumeRequest tag)

   STRUCTURE rtVolumeEntry,0
      ULONG rtve_Type		* DLT_DEVICE or DLT_DIRECTORY
      APTR  rtve_Name
      LABEL rtVolumeEntry_SIZE

************************
*                      *
*    Font requester    *
*                      *
************************

* structure _MUST_ be allocated with rtAllocRequest()

   STRUCTURE rtFontRequester,0
      ULONG  rtfo_ReqPos
      UWORD  rtfo_LeftOffset
      UWORD  rtfo_TopOffset
      ULONG  rtfo_Flags
      APTR   rtfo_private1
      STRUCT rtfo_Attr,ta_SIZEOF * READ ONLY!
      APTR   rtfo_DefaultFont
      ULONG  rtfo_WaitPointer
      * (V38) *
      ULONG  rtfo_LockWindow
      ULONG  rtfo_ShareIDCMP
      APTR   rtfo_IntuiMsgFunc
      UWORD  rtfo_reserved1
      UWORD  rtfo_reserved2
      UWORD  rtfo_reserved3
      UWORD  rtfo_ReqHeight	* READ ONLY!  Use RTFO_Height tag!
      * Lots of private data follows! HANDS OFF :-)

**************************
*                        *
*  ScreenMode requester  *
*                        *
**************************

* structure _MUST_ be allocated with rtAllocRequest()

   STRUCTURE rtScreenModeRequester,0
      ULONG rtsc_ReqPos
      UWORD rtsc_LeftOffset
      UWORD rtsc_TopOffset
      ULONG rtsc_Flags
      APTR  rtsc_private1
      *
      ULONG rtsc_DisplayID	* READ ONLY!
      UWORD rtsc_DisplayWidth	* READ ONLY!
      UWORD rtsc_DisplayHeight	* READ ONLY!
      *
      APTR  rtsc_DefaultFont
      ULONG rtsc_WaitPointer
      ULONG rtsc_LockWindow
      ULONG rtsc_ShareIDCMP
      APTR  rtsc_IntuiMsgFunc
      UWORD rtsc_reserved1
      UWORD rtsc_reserved2
      UWORD rtsc_reserved3
      UWORD rtsc_ReqHeight	* READ ONLY!  Use RTSC_Height tag!
      *
      UWORD rtsc_DisplayDepth	* READ ONLY!
      UWORD rtsc_OverscanType	* READ ONLY!
      ULONG rtsc_AutoScroll	* READ ONLY!
      * Lots of private data follows! HANDS OFF :-)

************************
*                      *
*    Requester Info    *
*                      *
************************

* for rtEZRequestA(), rtGetLongA(), rtGetStringA() and rtPaletteRequestA(),
* _MUST_ be allocated with rtAllocRequest()

   STRUCTURE rtReqInfo,0
      ULONG rtri_ReqPos
      UWORD rtri_LeftOffset
      UWORD rtri_TopOffset
      ULONG rtri_Width		 * not for rtEZRequestA()
      APTR  rtri_ReqTitle	 * currently only for rtEZRequestA()
      ULONG rtri_Flags
      APTR  rtri_DefaultFont	 * currently only for rtPaletteRequestA()
      ULONG rtri_WaitPointer
      * (V38) *
      ULONG rtri_LockWindow
      ULONG rtri_ShareIDCMP
      ULONG rtri_IntuiMsgFunc
      * structure may be extended in future

************************
*                      *
*     Handler Info     *
*                      *
************************

* for rtReqHandlerA(), will be allocated for you when you use
* the RT_ReqHandler tag, never try to allocate this yourself!

   STRUCTURE rtHandlerInfo,4	* first longword is private!
      ULONG rthi_WaitMask
      ULONG rthi_DoNotWait
      * Private data follows, HANDS OFF :-)

* possible return codes from rtReqHandlerA()

CALL_HANDLER		equ	 $80000000


**************************************
*                                    *
*                TAGS                *
*                                    *
**************************************

RT_TagBase		equ	 TAG_USER

*** tags understood by most requester functions ***
*
* optional pointer to window
RT_Window		equ	 (RT_TagBase+1)
* idcmp flags requester should abort on (useful for IDCMP_DISKINSERTED)
RT_IDCMPFlags		equ	 (RT_TagBase+2)
* position of requester window (see below) - default REQPOS_POINTER
RT_ReqPos		equ	 (RT_TagBase+3)
* leftedge offset of requester relative to position specified by RT_ReqPos
RT_LeftOffset		equ	 (RT_TagBase+4)
* topedge offset of requester relative to position specified by RT_ReqPos
RT_TopOffset		equ	 (RT_TagBase+5)
* name of public screen to put requester on (use on Kickstart 2.0 only!)
RT_PubScrName		equ	 (RT_TagBase+6)
* address of screen to put requester on
RT_Screen		equ	 (RT_TagBase+7)
* additional signal mask to wait on
RT_ReqHandler		equ	 (RT_TagBase+8)
* font to use when screen font is rejected, _MUST_ be fixed-width font!
* (struct TextFont *, not struct TextAttr *!)
* - default GfxBase->DefaultFont
RT_DefaultFont		equ	 (RT_TagBase+9)
* boolean to set the standard wait pointer in window - default FALSE
RT_WaitPointer		equ	 (RT_TagBase+10)
* (V38) char preceding keyboard shortcut characters (will be underlined)
RT_Underscore		equ	 (RT_TagBase+11)
* (V38) share IDCMP port with window - default FALSE
RT_ShareIDCMP		equ	 (RT_TagBase+12)
* (V38) lock window and set standard wait pointer - default FALSE
RT_LockWindow		equ	 (RT_TagBase+13)
* (V38) boolean to make requester's screen pop to front - default TRUE
RT_ScreenToFront	equ	 (RT_TagBase+14)
* (V38) Requester should use this font - default: screen font
RT_TextAttr		equ	 (RT_TagBase+15)
* (V38) call this hook for every IDCMP message not for requester
RT_IntuiMsgFunc		equ	 (RT_TagBase+16)
* (V38) Locale ReqTools should use for text
RT_Locale		equ	 (RT_TagBase+17)

*** tags specific to rtEZRequestA ***
*
* title of requester window - english default "Request" or "Information"
RTEZ_ReqTitle		equ	 (RT_TagBase+20)
* (RT_TagBase+21) reserved
* various flags (see below)
RTEZ_Flags		equ	 (RT_TagBase+22)
* default response (activated by pressing RETURN) - default TRUE
RTEZ_DefaultResponse	equ	 (RT_TagBase+23)

*** tags specific to rtGetLongA ***
*
* minimum allowed value - default MININT
RTGL_Min		equ	 (RT_TagBase+30)
* maximum allowed value - default MAXINT
RTGL_Max		equ	 (RT_TagBase+31)
* suggested width of requester window (in pixels)
RTGL_Width		equ	 (RT_TagBase+32)
* boolean to show the default value - default TRUE
RTGL_ShowDefault	equ	 (RT_TagBase+33)
* (V38) string with possible responses - english default " _Ok |_Cancel"
RTGL_GadFmt 		equ	 (RT_TagBase+34)
* (V38) optional arguments for RTGL_GadFmt
RTGL_GadFmtArgs		equ	 (RT_TagBase+35)
* (V38) invisible typing - default FALSE
RTGL_Invisible		equ	 (RT_TagBase+36)
* (V38) window backfill - default TRUE
RTGL_BackFill		equ	 (RT_TagBase+37)
* (V38) optional text above gadget
RTGL_TextFmt		equ	 (RT_TagBase+38)
* (V38) optional arguments for RTGS_TextFmt
RTGL_TextFmtArgs	equ	 (RT_TagBase+39)
* (V38) Center text - default FALSE
RTGL_CenterText		equ	 (RT_TagBase+100)
* (V38) various flags (see below)
RTGL_Flags		equ	 RTEZ_Flags

*** tags specific to rtGetStringA ***
*
* suggested width of requester window (in pixels)
RTGS_Width		equ	 RTGL_Width
* allow empty string to be accepted - default FALSE
RTGS_AllowEmpty		equ	 (RT_TagBase+80)
* (V38) string with possible responses - english default " _Ok |_Cancel"
RTGS_GadFmt 		equ	 RTGL_GadFmt
* (V38) optional arguments for RTGS_GadFmt
RTGS_GadFmtArgs		equ	 RTGL_GadFmtArgs
* (V38) invisible typing - default FALSE
RTGS_Invisible		equ	 RTGL_Invisible
* (V38) window backfill - default TRUE
RTGS_BackFill		equ	 RTGL_BackFill
* (V38) optional text above gadget
RTGS_TextFmt		equ	 RTGL_TextFmt
* (V38) optional arguments for RTGS_TextFmt
RTGS_TextFmtArgs	equ	 RTGL_TextFmtArgs
* (V38) Center text - default FALSE
RTGS_CenterText		equ	 RTGL_CenterText
* (V38) various flags (see below)
RTGS_Flags		equ	 RTEZ_Flags

*** tags specific to rtFileRequestA ***
*
* various flags (see below)
RTFI_Flags		equ	 (RT_TagBase+40)
* suggested height of file requester
RTFI_Height		equ	 (RT_TagBase+41)
* replacement text for 'Ok' gadget (max 6 chars)
RTFI_OkText		equ	 (RT_TagBase+42)
* (V38) bring up volume requester, tag data holds flags (see below)
RTFI_VolumeRequest	equ	 (RT_TagBase+43)
* (V38) call this hook for every file in the directory
RTFI_FilterFunc		equ	 (RT_TagBase+44)
* (V38) allow empty file to be accepted - default FALSE
RTFI_AllowEmpty		equ	 (RT_TagBase+45)

*** tags specific to rtFontRequestA ***
*
* various flags (see below)
RTFO_Flags		equ	 RTFI_Flags
* suggested height of font requester
RTFO_Height		equ	 RTFI_Height
* replacement text for 'Ok' gadget (max 6 chars)
RTFO_OkText		equ	 RTFI_OkText
* suggested height of font sample display - default 24
RTFO_SampleHeight	equ	 (RT_TagBase+60)
* minimum height of font displayed
RTFO_MinHeight		equ	 (RT_TagBase+61)
* maximum height of font displayed
RTFO_MaxHeight		equ	 (RT_TagBase+62)
* [(RT_TagBase+63) to (RT_TagBase+66) used below]
* (V38) call this hook for every font
RTFO_FilterFunc		equ	 RTFI_FilterFunc

*** (V38) tags for rtScreenModeRequestA ***
* various flags (see below) 
RTSC_Flags		equ	 RTFI_Flags
* suggested height of screenmode requester
RTSC_Height		equ	 RTFI_Height
* replacement text for 'Ok' gadget (max 6 chars)
RTSC_OkText		equ	 RTFI_OkText
* property flags (see also RTSC_PropertyMask)
RTSC_PropertyFlags	equ	 (RT_TagBase+90)
* property mask - default all bits in RTSC_PropertyFlags considered
RTSC_PropertyMask	equ	 (RT_TagBase+91)
* minimum display width allowed
RTSC_MinWidth		equ	 (RT_TagBase+92)
* maximum display width allowed
RTSC_MaxWidth		equ	 (RT_TagBase+93)
* minimum display height allowed
RTSC_MinHeight		equ	 (RT_TagBase+94)
* maximum display height allowed
RTSC_MaxHeight		equ	 (RT_TagBase+95)
* minimum display depth allowed
RTSC_MinDepth		equ	 (RT_TagBase+96)
* maximum display depth allowed
RTSC_MaxDepth		equ	 (RT_TagBase+97)
* call this hook for every display mode id
RTSC_FilterFunc		equ	 RTFI_FilterFunc

*** tags for rtChangeReqAttrA ***
*
* file requester - set directory
RTFI_Dir		equ	 (RT_TagBase+50)
* file requester - set wildcard pattern
RTFI_MatchPat		equ	 (RT_TagBase+51)
* file requester - add a file or directory to the buffer
RTFI_AddEntry		equ	 (RT_TagBase+52)
* file requester - remove a file or directory from the buffer
RTFI_RemoveEntry	equ	 (RT_TagBase+53)
* font requester - set font name of selected font
RTFO_FontName		equ	 (RT_TagBase+63)
* font requester - set font size
RTFO_FontHeight		equ	 (RT_TagBase+64)
* font requester - set font style
RTFO_FontStyle		equ	 (RT_TagBase+65)
* font requester - set font flags
RTFO_FontFlags		equ	 (RT_TagBase+66)
* (V38) screenmode requester - get display attributes from screen
RTSC_ModeFromScreen	equ	 (RT_TagBase+80)
* (V38) screenmode requester - set display mode id (32-bit extended)
RTSC_DisplayID		equ	 (RT_TagBase+81)
* (V38) screenmode requester - set display width
RTSC_DisplayWidth	equ	 (RT_TagBase+82)
* (V38) screenmode requester - set display height
RTSC_DisplayHeight	equ	 (RT_TagBase+83)
* (V38) screenmode requester - set display depth
RTSC_DisplayDepth	equ	 (RT_TagBase+84)
* (V38) screenmode requester - set overscan type, 0 for regular size
RTSC_OverscanType	equ	 (RT_TagBase+85)
* (V38) screenmode requester - set autoscroll
RTSC_AutoScroll		equ	 (RT_TagBase+86)

*** tags for rtPaletteRequestA ***
*
* initially selected color - default 1
RTPA_Color		equ	 (RT_TagBase+70)

*** tags for rtReqHandlerA ***
*
* end requester by software control, set tagdata to REQ_CANCEL, REQ_OK or
* in case of rtEZRequest to the return value
RTRH_EndRequest		equ	 (RT_TagBase+60)

*** tags for rtAllocRequestA ***
* no tags defined yet


*************
* RT_ReqPos *
*************
REQPOS_POINTER		equ	 0
REQPOS_CENTERWIN	equ	 1
REQPOS_CENTERSCR	equ	 2
REQPOS_TOPLEFTWIN	equ	 3
REQPOS_TOPLEFTSCR	equ	 4

*******************
* RTRH_EndRequest *
*******************
REQ_CANCEL		equ	 0
REQ_OK			equ	 1

****************************************
* flags for RTFI_Flags and RTFO_Flags  *
* or filereq->Flags and fontreq->Flags *
****************************************
   BITDEF FREQ,NOBUFFER,2

******************************************
* flags for RTFI_Flags or filereq->Flags *
******************************************
   BITDEF FREQ,MULTISELECT,0
   BITDEF FREQ,SAVE,1
   BITDEF FREQ,NOFILES,3
   BITDEF FREQ,PATGAD,4
   BITDEF FREQ,SELECTDIRS,12

******************************************
* flags for RTFO_Flags or fontreq->Flags *
******************************************
   BITDEF FREQ,FIXEDWIDTH,5
   BITDEF FREQ,COLORFONTS,6
   BITDEF FREQ,CHANGEPALETTE,7
   BITDEF FREQ,LEAVEPALETTE,8
   BITDEF FREQ,SCALE,9
   BITDEF FREQ,STYLE,10

******************************************************
* (V38) flags for RTSC_Flags or screenmodereq->Flags *
******************************************************
   BITDEF SCREQ,SIZEGADS,13
   BITDEF SCREQ,DEPTHGAD,14
   BITDEF SCREQ,NONSTDMODES,15
   BITDEF SCREQ,GUIMODES,16
   BITDEF SCREQ,AUTOSCROLLGAD,18
   BITDEF SCREQ,OVERSCANGAD,19

******************************************
* flags for RTEZ_Flags or reqinfo->Flags *
******************************************
   BITDEF EZREQ,NORETURNKEY,0
   BITDEF EZREQ,LAMIGAQUAL,1
   BITDEF EZREQ,CENTERTEXT,2

************************************************
* (V38) flags for RTGL_Flags or reqinfo->Flags *
************************************************
   BITDEF GLREQ,CENTERTEXT,EZREQB_CENTERTEXT
   BITDEF GLREQ,HIGHLIGHTTEXT,3

************************************************
* (V38) flags for RTGS_Flags or reqinfo->Flags *
************************************************
   BITDEF GSREQ,CENTERTEXT,EZREQB_CENTERTEXT
   BITDEF GSREQ,HIGHLIGHTTEXT,GLREQB_HIGHLIGHTTEXT

******************************************
* (V38) flags for RTFI_VolumeRequest tag *
******************************************
   BITDEF VREQ,NOASSIGNS,0
   BITDEF VREQ,NODISKS,1
   BITDEF VREQ,ALLDISKS,2

*
*  Following things are obsolete in ReqTools V38.
*  DON'T USE THESE IN NEW CODE!
*
 IFND NO_REQTOOLS_OBSOLETE
rtfi_Hook equ rtfi_private1
rtfo_Hook equ rtfo_private1
REQHOOK_WILDFILE equ 0
REQHOOK_WILDFONT equ 1
 BITDEF FREQ,DOWILDFUNC,11
 ENDC

   ENDC ; LIBRARIES_REQTOOLS_I
