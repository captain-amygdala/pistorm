#ifndef LIBRARIES_REQTOOLS_H
#define LIBRARIES_REQTOOLS_H
/*
**	$Filename: libraries/reqtools.h $
**	$Release: 2.2 $
**	$Revision: 38.11 $
**
**	reqtools.library definitions
**
**	(C) Copyright 1991-1994 Nico François
**	All Rights Reserved
*/

#ifndef	EXEC_TYPES_H
#include <exec/types.h>
#endif	/* EXEC_TYPES_H */

#ifndef	EXEC_LISTS_H
#include <exec/lists.h>
#endif	/* EXEC_LISTS_H */

#ifndef	EXEC_LIBRARIES_H
#include <exec/libraries.h>
#endif	/* EXEC_LIBRARIES_H */

#ifndef	EXEC_SEMAPHORES_H
#include <exec/semaphores.h>
#endif	/* EXEC_SEMAPHORES_H */

#ifndef LIBRARIES_DOS_H
#include <libraries/dos.h>
#endif  /* LIBRARIES_DOS_H */

#ifndef LIBRARIES_DOSEXTENS_H
#include <libraries/dosextens.h>
#endif  /* LIBRARIES_DOSEXTENS_H */

#ifndef LIBRARIES_DISKFONT_H
#include <libraries/diskfont.h>
#endif  /* LIBRARIES_DISKFONT_H */

#ifndef	GRAPHICS_TEXT_H
#include <graphics/text.h>
#endif	/* GRAPHICS_TEXT_H */

#ifndef UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif	/* UTILITY_TAGITEM_H */

#define	REQTOOLSNAME		 "reqtools.library"
#define	REQTOOLSVERSION		 38L

/***********************
*                      *
*     Preferences      *
*                      *
***********************/

#define RTPREF_FILEREQ		 0L
#define RTPREF_FONTREQ		 1L
#define RTPREF_PALETTEREQ	 2L
#define RTPREF_SCREENMODEREQ	 3L
#define RTPREF_VOLUMEREQ	 4L
#define RTPREF_OTHERREQ		 5L
#define RTPREF_NR_OF_REQ	 6L

struct ReqDefaults {
   ULONG Size;
   ULONG ReqPos;
   UWORD LeftOffset;
   UWORD TopOffset;
	UWORD MinEntries;
	UWORD MaxEntries;
   };

struct ReqToolsPrefs {
   /* Size of preferences (_without_ this field and the semaphore) */
   ULONG PrefsSize;
   struct SignalSemaphore PrefsSemaphore;
   /* Start of real preferences */
   ULONG Flags;
   struct ReqDefaults ReqDefaults[RTPREF_NR_OF_REQ];
   };

#define RTPREFS_SIZE \
   (sizeof (struct ReqToolsPrefs) - sizeof (struct SignalSemaphore) - 4)

/* Flags */

#define RTPRB_DIRSFIRST		 0L
#define RTPRF_DIRSFIRST		 (1L<<RTPRB_DIRSFIRST)
#define RTPRB_DIRSMIXED		 1L
#define RTPRF_DIRSMIXED		 (1L<<RTPRB_DIRSMIXED)
#define RTPRB_IMMSORT		 2L
#define RTPRF_IMMSORT		 (1L<<RTPRB_IMMSORT)
#define RTPRB_NOSCRTOFRONT	 3L
#define RTPRF_NOSCRTOFRONT	 (1L<<RTPRB_NOSCRTOFRONT)
#define RTPRB_NOLED		 4L
#define RTPRF_NOLED		 (1L<<RTPRB_NOLED)

/***********************
*                      *
*     Library Base     *
*                      *
***********************/

struct ReqToolsBase {
   struct Library LibNode;
   UBYTE RTFlags;
   UBYTE pad[3];
   BPTR SegList;

   /* PUBLIC FIELDS */

   /* NOTE: Some versions of the Manx C compiler contain a bug so it gets
            confused by the library bases below.  Add the rt_ prefix to the
            library names to fix the problem (e.g. rt_IntuitionBase). */

   /* The following library bases may be read and used by your program */
   struct IntuitionBase *IntuitionBase;
   struct GfxBase *GfxBase;
   struct DosLibrary *DOSBase;
   /* Next two library bases are only (and always) valid on Kickstart 2.0!
      (1.3 version of reqtools also initializes these when run on 2.0) */
   struct Library *GadToolsBase;
   struct Library *UtilityBase;

   /* PRIVATE FIELDS, THESE WILL CHANGE FROM RELEASE TO RELEASE! */

   /* The RealOpenCnt is for the buffered AvailFonts feature.  Since
      Kickstart 3.0 offers low memory handlers a release of ReqTools for 3.0
      will not use this field and start using the normal OpenCnt again. */
   UWORD RealOpenCnt;
   UWORD AvailFontsLock;
   struct AvailFontsHeader *AvailFontsHeader;
   ULONG FontsAssignType;
   BPTR FontsAssignLock;
   struct AssignList *FontsAssignList;
   struct ReqToolsPrefs ReqToolsPrefs;
   UWORD prefspad;
   };

/* types of requesters, for rtAllocRequestA() */
#define RT_FILEREQ		 0L
#define RT_REQINFO		 1L
#define RT_FONTREQ		 2L
/* (V38) */
#define RT_SCREENMODEREQ	 3L

/***********************
*                      *
*    File requester    *
*                      *
***********************/

/* structure _MUST_ be allocated with rtAllocRequest() */

struct rtFileRequester {
   ULONG ReqPos;
   UWORD LeftOffset;
   UWORD TopOffset;
   ULONG Flags;
   /* OBSOLETE IN V38! DON'T USE! */ struct Hook *Hook;
   /* */
   char  *Dir;		     /* READ ONLY! Change with rtChangeReqAttrA()! */
   char  *MatchPat;	     /* READ ONLY! Change with rtChangeReqAttrA()! */
   /* */
   struct TextFont *DefaultFont;
   ULONG WaitPointer;
   /* (V38) */
   ULONG LockWindow;
   ULONG ShareIDCMP;
   struct Hook *IntuiMsgFunc;
   UWORD reserved1;
   UWORD reserved2;
   UWORD reserved3;
   UWORD ReqHeight;	     /* READ ONLY!  Use RTFI_Height tag! */
   /* Private data follows! HANDS OFF :-) */
   };

/* returned by rtFileRequestA() if multiselect is enabled,
   free list with rtFreeFileList() */

struct rtFileList {
   struct rtFileList *Next;
   ULONG StrLen;	     /* -1 for directories */
   char *Name;
   };

/* structure passed to RTFI_FilterFunc callback hook by
   volume requester (see RTFI_VolumeRequest tag) */

struct rtVolumeEntry {
   ULONG Type;		     /* DLT_DEVICE or DLT_DIRECTORY */
   char *Name;
   };

/***********************
*                      *
*    Font requester    *
*                      *
***********************/

/* structure _MUST_ be allocated with rtAllocRequest() */

struct rtFontRequester {
   ULONG ReqPos;
   UWORD LeftOffset;
   UWORD TopOffset;
   ULONG Flags;
   /* OBSOLETE IN V38! DON'T USE! */ struct Hook *Hook;
   /* */
   struct TextAttr Attr;	 /* READ ONLY! */
   /* */
   struct TextFont *DefaultFont;
   ULONG WaitPointer;
   /* (V38) */
   ULONG LockWindow;
   ULONG ShareIDCMP;
   struct Hook *IntuiMsgFunc;
   UWORD reserved1;
   UWORD reserved2;
   UWORD reserved3;
   UWORD ReqHeight;		 /* READ ONLY!  Use RTFO_Height tag! */
   /* Private data follows! HANDS OFF :-) */
   };

/*************************
*                        *
*  ScreenMode requester  *
*                        *
*************************/

/* structure _MUST_ be allocated with rtAllocRequest() */

struct rtScreenModeRequester {
   ULONG ReqPos;
   UWORD LeftOffset;
   UWORD TopOffset;
   ULONG Flags;
   ULONG private1;
   /* */
   ULONG DisplayID;		 /* READ ONLY! */
   UWORD DisplayWidth;		 /* READ ONLY! */
   UWORD DisplayHeight;		 /* READ ONLY! */
   /* */
   struct TextFont *DefaultFont;
   ULONG WaitPointer;
   ULONG LockWindow;
   ULONG ShareIDCMP;
   struct Hook *IntuiMsgFunc;
   UWORD reserved1;
   UWORD reserved2;
   UWORD reserved3;
   UWORD ReqHeight;		 /* READ ONLY!  Use RTSC_Height tag! */
   /* */
   UWORD DisplayDepth;		 /* READ ONLY! */
   UWORD OverscanType;		 /* READ ONLY! */
   ULONG AutoScroll;		 /* READ ONLY! */
   /* Private data follows! HANDS OFF :-) */
   };

/***********************
*                      *
*    Requester Info    *
*                      *
***********************/

/* for rtEZRequestA(), rtGetLongA(), rtGetStringA() and rtPaletteRequestA(),
   _MUST_ be allocated with rtAllocRequest() */

struct rtReqInfo {
   ULONG ReqPos;
   UWORD LeftOffset;
   UWORD TopOffset;
   ULONG Width;			 /* not for rtEZRequestA() */
   char *ReqTitle;		 /* currently only for rtEZRequestA() */
   ULONG Flags;
   struct TextFont *DefaultFont; /* currently only for rtPaletteRequestA() */
   ULONG WaitPointer;
   /* (V38) */
   ULONG LockWindow;
   ULONG ShareIDCMP;
   struct Hook *IntuiMsgFunc;
   /* structure may be extended in future */
   };

/***********************
*                      *
*     Handler Info     *
*                      *
***********************/

/* for rtReqHandlerA(), will be allocated for you when you use
   the RT_ReqHandler tag, never try to allocate this yourself! */

struct rtHandlerInfo {
   ULONG private1;
   ULONG WaitMask;
   ULONG DoNotWait;
   /* Private data follows, HANDS OFF :-) */
   };

/* possible return codes from rtReqHandlerA() */

#define CALL_HANDLER		 (ULONG)0x80000000


/*************************************
*                                    *
*                TAGS                *
*                                    *
*************************************/

#define RT_TagBase		 TAG_USER

/*** tags understood by most requester functions ***
*/
/* optional pointer to window */
#define RT_Window		 (RT_TagBase+1)
/* idcmp flags requester should abort on (useful for IDCMP_DISKINSERTED) */
#define RT_IDCMPFlags		 (RT_TagBase+2)
/* position of requester window (see below) - default REQPOS_POINTER */
#define RT_ReqPos		 (RT_TagBase+3)
/* leftedge offset of requester relative to position specified by RT_ReqPos */
#define RT_LeftOffset		 (RT_TagBase+4)
/* topedge offset of requester relative to position specified by RT_ReqPos */
#define RT_TopOffset		 (RT_TagBase+5)
/* name of public screen to put requester on (Kickstart 2.0 only!) */
#define RT_PubScrName		 (RT_TagBase+6)
/* address of screen to put requester on */
#define RT_Screen		 (RT_TagBase+7)
/* tagdata must hold the address of (!) an APTR variable */
#define RT_ReqHandler		 (RT_TagBase+8)
/* font to use when screen font is rejected, _MUST_ be fixed-width font!
   (struct TextFont *, not struct TextAttr *!)
   - default GfxBase->DefaultFont */
#define RT_DefaultFont		 (RT_TagBase+9)
/* boolean to set the standard wait pointer in window - default FALSE */
#define RT_WaitPointer		 (RT_TagBase+10)
/* (V38) char preceding keyboard shortcut characters (will be underlined) */
#define RT_Underscore		 (RT_TagBase+11)
/* (V38) share IDCMP port with window - default FALSE */
#define RT_ShareIDCMP		 (RT_TagBase+12)
/* (V38) lock window and set standard wait pointer - default FALSE */
#define RT_LockWindow		 (RT_TagBase+13)
/* (V38) boolean to make requester's screen pop to front - default TRUE */
#define RT_ScreenToFront	 (RT_TagBase+14)
/* (V38) Requester should use this font - default: screen font */
#define RT_TextAttr		 (RT_TagBase+15)
/* (V38) call this hook for every IDCMP message not for requester */
#define RT_IntuiMsgFunc		 (RT_TagBase+16)
/* (V38) Locale ReqTools should use for text */
#define RT_Locale		 (RT_TagBase+17)

/*** tags specific to rtEZRequestA ***
*/
/* title of requester window - english default "Request" or "Information" */
#define RTEZ_ReqTitle		 (RT_TagBase+20)
/* (RT_TagBase+21) reserved */
/* various flags (see below) */
#define RTEZ_Flags		 (RT_TagBase+22)
/* default response (activated by pressing RETURN) - default TRUE */
#define RTEZ_DefaultResponse	 (RT_TagBase+23)

/*** tags specific to rtGetLongA ***
*/
/* minimum allowed value - default MININT */
#define RTGL_Min		 (RT_TagBase+30)
/* maximum allowed value - default MAXINT */
#define RTGL_Max		 (RT_TagBase+31)
/* suggested width of requester window (in pixels) */
#define RTGL_Width		 (RT_TagBase+32)
/* boolean to show the default value - default TRUE */
#define RTGL_ShowDefault	 (RT_TagBase+33)
/* (V38) string with possible responses - english default " _Ok |_Cancel" */
#define RTGL_GadFmt 		 (RT_TagBase+34)
/* (V38) optional arguments for RTGL_GadFmt */
#define RTGL_GadFmtArgs		 (RT_TagBase+35)
/* (V38) invisible typing - default FALSE */
#define RTGL_Invisible		 (RT_TagBase+36)
/* (V38) window backfill - default TRUE */
#define RTGL_BackFill		 (RT_TagBase+37)
/* (V38) optional text above gadget */
#define RTGL_TextFmt		 (RT_TagBase+38)
/* (V38) optional arguments for RTGS_TextFmt */
#define RTGL_TextFmtArgs	 (RT_TagBase+39)
/* (V38) various flags (see below) */
#define RTGL_Flags		 RTEZ_Flags

/*** tags specific to rtGetStringA ***
*/
/* suggested width of requester window (in pixels) */
#define RTGS_Width		 RTGL_Width
/* allow empty string to be accepted - default FALSE */
#define RTGS_AllowEmpty		 (RT_TagBase+80)
/* (V38) string with possible responses - english default " _Ok |_Cancel" */
#define RTGS_GadFmt 		 RTGL_GadFmt
/* (V38) optional arguments for RTGS_GadFmt */
#define RTGS_GadFmtArgs		 RTGL_GadFmtArgs
/* (V38) invisible typing - default FALSE */
#define RTGS_Invisible		 RTGL_Invisible
/* (V38) window backfill - default TRUE */
#define RTGS_BackFill		 RTGL_BackFill
/* (V38) optional text above gadget */
#define RTGS_TextFmt		 RTGL_TextFmt
/* (V38) optional arguments for RTGS_TextFmt */
#define RTGS_TextFmtArgs	 RTGL_TextFmtArgs
/* (V38) various flags (see below) */
#define RTGS_Flags		 RTEZ_Flags

/*** tags specific to rtFileRequestA ***
*/
/* various flags (see below) */
#define RTFI_Flags		 (RT_TagBase+40)
/* suggested height of file requester */
#define RTFI_Height		 (RT_TagBase+41)
/* replacement text for 'Ok' gadget (max 6 chars) */
#define RTFI_OkText		 (RT_TagBase+42)
/* (V38) bring up volume requester, tag data holds flags (see below) */
#define RTFI_VolumeRequest	 (RT_TagBase+43)
/* (V38) call this hook for every file in the directory */
#define RTFI_FilterFunc		 (RT_TagBase+44)
/* (V38) allow empty file to be accepted - default FALSE */
#define RTFI_AllowEmpty		 (RT_TagBase+45)

/*** tags specific to rtFontRequestA ***
*/
/* various flags (see below) */
#define RTFO_Flags		 RTFI_Flags
/* suggested height of font requester */
#define RTFO_Height		 RTFI_Height
/* replacement text for 'Ok' gadget (max 6 chars) */
#define RTFO_OkText		 RTFI_OkText
/* suggested height of font sample display - default 24 */
#define RTFO_SampleHeight	 (RT_TagBase+60)
/* minimum height of font displayed */
#define RTFO_MinHeight		 (RT_TagBase+61)
/* maximum height of font displayed */
#define RTFO_MaxHeight		 (RT_TagBase+62)
/* [(RT_TagBase+63) to (RT_TagBase+66) used below] */
/* (V38) call this hook for every font */
#define RTFO_FilterFunc		 RTFI_FilterFunc

/*** (V38) tags for rtScreenModeRequestA ***
*/
/* various flags (see below) */
#define RTSC_Flags		 RTFI_Flags
/* suggested height of screenmode requester */
#define RTSC_Height		 RTFI_Height
/* replacement text for 'Ok' gadget (max 6 chars) */
#define RTSC_OkText		 RTFI_OkText
/* property flags (see also RTSC_PropertyMask) */
#define RTSC_PropertyFlags	 (RT_TagBase+90)
/* property mask - default all bits in RTSC_PropertyFlags considered */
#define RTSC_PropertyMask	 (RT_TagBase+91)
/* minimum display width allowed */
#define RTSC_MinWidth		 (RT_TagBase+92)
/* maximum display width allowed */
#define RTSC_MaxWidth		 (RT_TagBase+93)
/* minimum display height allowed */
#define RTSC_MinHeight		 (RT_TagBase+94)
/* maximum display height allowed */
#define RTSC_MaxHeight		 (RT_TagBase+95)
/* minimum display depth allowed */
#define RTSC_MinDepth		 (RT_TagBase+96)
/* maximum display depth allowed */
#define RTSC_MaxDepth		 (RT_TagBase+97)
/* call this hook for every display mode id */
#define RTSC_FilterFunc		 RTFI_FilterFunc

/*** tags for rtChangeReqAttrA ***
*/
/* file requester - set directory */
#define RTFI_Dir		 (RT_TagBase+50)
/* file requester - set wildcard pattern */
#define RTFI_MatchPat		 (RT_TagBase+51)
/* file requester - add a file or directory to the buffer */
#define RTFI_AddEntry		 (RT_TagBase+52)
/* file requester - remove a file or directory from the buffer */
#define RTFI_RemoveEntry	 (RT_TagBase+53)
/* font requester - set font name of selected font */
#define RTFO_FontName		 (RT_TagBase+63)
/* font requester - set font size */
#define RTFO_FontHeight		 (RT_TagBase+64)
/* font requester - set font style */
#define RTFO_FontStyle		 (RT_TagBase+65)
/* font requester - set font flags */
#define RTFO_FontFlags		 (RT_TagBase+66)
/* (V38) screenmode requester - get display attributes from screen */
#define RTSC_ModeFromScreen	 (RT_TagBase+80)
/* (V38) screenmode requester - set display mode id (32-bit extended) */
#define RTSC_DisplayID		 (RT_TagBase+81)
/* (V38) screenmode requester - set display width */
#define RTSC_DisplayWidth	 (RT_TagBase+82)
/* (V38) screenmode requester - set display height */
#define RTSC_DisplayHeight	 (RT_TagBase+83)
/* (V38) screenmode requester - set display depth */
#define RTSC_DisplayDepth	 (RT_TagBase+84)
/* (V38) screenmode requester - set overscan type, 0 for regular size */
#define RTSC_OverscanType	 (RT_TagBase+85)
/* (V38) screenmode requester - set autoscroll */
#define RTSC_AutoScroll		 (RT_TagBase+86)

/*** tags for rtPaletteRequestA ***
*/
/* initially selected color - default 1 */
#define RTPA_Color		 (RT_TagBase+70)

/*** tags for rtReqHandlerA ***
*/
/* end requester by software control, set tagdata to REQ_CANCEL, REQ_OK or
   in case of rtEZRequest to the return value */
#define RTRH_EndRequest		 (RT_TagBase+60)

/*** tags for rtAllocRequestA ***/
/* no tags defined yet */


/************
* RT_ReqPos *
************/
#define REQPOS_POINTER		 0L
#define REQPOS_CENTERWIN	 1L
#define REQPOS_CENTERSCR	 2L
#define REQPOS_TOPLEFTWIN	 3L
#define REQPOS_TOPLEFTSCR	 4L

/******************
* RTRH_EndRequest *
******************/
#define REQ_CANCEL		 0L
#define REQ_OK			 1L

/***************************************
* flags for RTFI_Flags and RTFO_Flags  *
* or filereq->Flags and fontreq->Flags *
***************************************/
#define FREQB_NOBUFFER		 2L
#define FREQF_NOBUFFER		 (1L<<FREQB_NOBUFFER)

/*****************************************
* flags for RTFI_Flags or filereq->Flags *
*****************************************/
#define FREQB_MULTISELECT	 0L
#define FREQF_MULTISELECT	 (1L<<FREQB_MULTISELECT)
#define FREQB_SAVE		 1L
#define FREQF_SAVE		 (1L<<FREQB_SAVE)
#define FREQB_NOFILES		 3L
#define FREQF_NOFILES		 (1L<<FREQB_NOFILES)
#define FREQB_PATGAD		 4L
#define FREQF_PATGAD		 (1L<<FREQB_PATGAD)
#define FREQB_SELECTDIRS	 12L
#define FREQF_SELECTDIRS	 (1L<<FREQB_SELECTDIRS)

/*****************************************
* flags for RTFO_Flags or fontreq->Flags *
*****************************************/
#define FREQB_FIXEDWIDTH	 5L
#define FREQF_FIXEDWIDTH	 (1L<<FREQB_FIXEDWIDTH)
#define FREQB_COLORFONTS	 6L
#define FREQF_COLORFONTS	 (1L<<FREQB_COLORFONTS)
#define FREQB_CHANGEPALETTE	 7L
#define FREQF_CHANGEPALETTE	 (1L<<FREQB_CHANGEPALETTE)
#define FREQB_LEAVEPALETTE	 8L
#define FREQF_LEAVEPALETTE	 (1L<<FREQB_LEAVEPALETTE)
#define FREQB_SCALE		 9L
#define FREQF_SCALE		 (1L<<FREQB_SCALE)
#define FREQB_STYLE		 10L
#define FREQF_STYLE		 (1L<<FREQB_STYLE)

/*****************************************************
* (V38) flags for RTSC_Flags or screenmodereq->Flags *
*****************************************************/
#define SCREQB_SIZEGADS		 13L
#define SCREQF_SIZEGADS		 (1L<<SCREQB_SIZEGADS)
#define SCREQB_DEPTHGAD		 14L
#define SCREQF_DEPTHGAD		 (1L<<SCREQB_DEPTHGAD)
#define SCREQB_NONSTDMODES	 15L
#define SCREQF_NONSTDMODES	 (1L<<SCREQB_NONSTDMODES)
#define SCREQB_GUIMODES		 16L
#define SCREQF_GUIMODES		 (1L<<SCREQB_GUIMODES)
#define SCREQB_AUTOSCROLLGAD	 18L
#define SCREQF_AUTOSCROLLGAD	 (1L<<SCREQB_AUTOSCROLLGAD)
#define SCREQB_OVERSCANGAD	 19L
#define SCREQF_OVERSCANGAD	 (1L<<SCREQB_OVERSCANGAD)

/*****************************************
* flags for RTEZ_Flags or reqinfo->Flags *
*****************************************/
#define EZREQB_NORETURNKEY	 0L
#define EZREQF_NORETURNKEY	 (1L<<EZREQB_NORETURNKEY)
#define EZREQB_LAMIGAQUAL	 1L
#define EZREQF_LAMIGAQUAL	 (1L<<EZREQB_LAMIGAQUAL)
#define EZREQB_CENTERTEXT	 2L
#define EZREQF_CENTERTEXT	 (1L<<EZREQB_CENTERTEXT)

/***********************************************
* (V38) flags for RTGL_Flags or reqinfo->Flags *
***********************************************/
#define GLREQB_CENTERTEXT	 EZREQB_CENTERTEXT
#define GLREQF_CENTERTEXT	 EZREQF_CENTERTEXT
#define GLREQB_HIGHLIGHTTEXT	 3L
#define GLREQF_HIGHLIGHTTEXT	 (1L<<GLREQB_HIGHLIGHTTEXT)

/***********************************************
* (V38) flags for RTGS_Flags or reqinfo->Flags *
***********************************************/
#define GSREQB_CENTERTEXT	 EZREQB_CENTERTEXT
#define GSREQF_CENTERTEXT	 EZREQF_CENTERTEXT
#define GSREQB_HIGHLIGHTTEXT	 GLREQB_HIGHLIGHTTEXT
#define GSREQF_HIGHLIGHTTEXT	 GLREQF_HIGHLIGHTTEXT

/*****************************************
* (V38) flags for RTFI_VolumeRequest tag *
*****************************************/
#define VREQB_NOASSIGNS		 0L
#define VREQF_NOASSIGNS		 (1L<<VREQB_NOASSIGNS)
#define VREQB_NODISKS		 1L
#define VREQF_NODISKS		 (1L<<VREQB_NODISKS)
#define VREQB_ALLDISKS		 2L
#define VREQF_ALLDISKS		 (1L<<VREQB_ALLDISKS)

/*
   Following things are obsolete in ReqTools V38.
   DON'T USE THESE IN NEW CODE!
*/
#ifndef NO_REQTOOLS_OBSOLETE
#define REQHOOK_WILDFILE 0L
#define REQHOOK_WILDFONT 1L
#define FREQB_DOWILDFUNC 11L                   
#define FREQF_DOWILDFUNC (1L<<FREQB_DOWILDFUNC)
#endif

#endif /* LIBRARIES_REQTOOLS_H */
