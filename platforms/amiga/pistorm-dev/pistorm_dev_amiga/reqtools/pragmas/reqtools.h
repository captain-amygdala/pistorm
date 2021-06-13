	
/*
 *   reqtools.library  © 1991-1994 Nico François
 *
 */

#pragma libcall ReqToolsBase rtAllocRequestA 1E 8002
#pragma libcall ReqToolsBase rtFreeRequest 24 901
#pragma libcall ReqToolsBase rtFreeReqBuffer 2A 901
#pragma libcall ReqToolsBase rtChangeReqAttrA 30 8902
#pragma libcall ReqToolsBase rtFileRequestA 36 8BA904
#pragma libcall ReqToolsBase rtFreeFileList 3C 801
#pragma libcall ReqToolsBase rtEZRequestA 42 8CBA905
#pragma libcall ReqToolsBase rtGetStringA 48 8BA0905
#pragma libcall ReqToolsBase rtGetLongA 4E 8BA904
#pragma libcall ReqToolsBase rtFontRequestA 60 8B903
#pragma libcall ReqToolsBase rtPaletteRequestA 66 8BA03
#pragma libcall ReqToolsBase rtReqHandlerA 6C 80903
#pragma libcall ReqToolsBase rtSetWaitPointer 72 801
#pragma libcall ReqToolsBase rtGetVScreenSize 78 A9803
#pragma libcall ReqToolsBase rtSetReqPosition 7E A98004
#pragma libcall ReqToolsBase rtSpread 84 32109806
#pragma libcall ReqToolsBase rtScreenToFrontSafely 8A 801
#pragma libcall ReqToolsBase rtScreenModeRequestA 90 8B903
#pragma libcall ReqToolsBase rtCloseWindowSafely 96 801
#pragma libcall ReqToolsBase rtLockWindow 9C 801
#pragma libcall ReqToolsBase rtUnlockWindow A2 9802
/**/
/* Private function only to be used by the ReqTools Preference editor.*/
/* Only present in library versions _above_ 38.362 [1.3] and 38.810 [2.0]!*/
/**/
#pragma libcall ReqToolsBase rtLockPrefs A8 0
#pragma libcall ReqToolsBase rtUnlockPrefs AE 0
