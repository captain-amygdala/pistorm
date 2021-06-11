#ifndef CLIB_REQTOOLS_PROTOS_H
#define CLIB_REQTOOLS_PROTOS_H
/*
**	$Filename: clib/reqtools_protos.h $
**	$Release: 2.2 $
**	$Revision: 38.11 $
**
**	C prototypes. For use with 32 bit integers only.
**
**	(C) Copyright 1991-1994 Nico François
**	    All Rights Reserved
*/

#ifndef UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif	/* UTILITY_TAGITEM_H */

APTR  rtAllocRequestA (ULONG, struct TagItem *);
void  rtFreeRequest (APTR);
void  rtFreeReqBuffer (APTR);
LONG  rtChangeReqAttrA (APTR, struct TagItem *);
APTR  rtFileRequestA(struct rtFileRequester *,char *,char *,struct TagItem *);
void  rtFreeFileList (struct rtFileList *);
ULONG rtEZRequestA (char *,char *,struct rtReqInfo *,APTR,struct TagItem *);
ULONG rtGetStringA (UBYTE *,ULONG,char *,struct rtReqInfo *,struct TagItem *);
ULONG rtGetLongA (ULONG *, char *, struct rtReqInfo *, struct TagItem *);
ULONG rtFontRequestA (struct rtFontRequester *, char *, struct TagItem *);
LONG  rtPaletteRequestA (char *, struct rtReqInfo *, struct TagItem *);
ULONG rtReqHandlerA (struct rtHandlerInfo *, ULONG, struct TagItem *);
void  rtSetWaitPointer (struct Window *);
ULONG rtGetVScreenSize (struct Screen *, ULONG *, ULONG *);
void  rtSetReqPosition (ULONG, struct NewWindow *,
                        struct Screen *, struct Window *);
void  rtSpread (ULONG *, ULONG *, ULONG, ULONG, ULONG, ULONG);
void  rtScreenToFrontSafely (struct Screen *);
ULONG rtScreenModeRequestA (struct rtScreenModeRequester *,
                            char *, struct TagItem *);
void  rtCloseWindowSafely (struct Window *);
APTR  rtLockWindow (struct Window *);
void  rtUnlockWindow (struct Window *, APTR);

/* private functions */

struct ReqToolsPrefs *rtLockPrefs (void);
void rtUnlockPrefs (void);

/* functions with varargs in reqtools.lib and reqtoolsnb.lib */

APTR  rtAllocRequest (ULONG, Tag,...);
LONG  rtChangeReqAttr (APTR, Tag,...);
APTR  rtFileRequest (struct rtFileRequester *, char *, char *, Tag,...);
ULONG rtEZRequest (char *, char *, struct rtReqInfo *, struct TagItem *,...);
ULONG rtEZRequestTags (char *, char *, struct rtReqInfo *, APTR, Tag,...);
ULONG rtGetString (UBYTE *, ULONG, char *, struct rtReqInfo *, Tag,...);
ULONG rtGetLong (ULONG *, char *, struct rtReqInfo *, Tag,...);
ULONG rtFontRequest (struct rtFontRequester *, char *, Tag,...);
LONG  rtPaletteRequest (char *, struct rtReqInfo *, Tag,...);
ULONG rtReqHandler (struct rtHandlerInfo *, ULONG, Tag,...);
ULONG rtScreenModeRequest (struct rtScreenModeRequester *, char *, Tag,...);

#endif /* CLIB_REQTOOLS_PROTOS_H */
