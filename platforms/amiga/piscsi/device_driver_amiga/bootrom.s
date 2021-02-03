**
** Sample autoboot code fragment
**
** These are the calling conventions for the Diag routine
**
** A7 -- points to at least 2K of stack
** A6 -- ExecBase
** A5 -- ExpansionBase
** A3 -- your board's ConfigDev structure
** A2 -- Base of diag/init area that was copied
** A0 -- Base of your board
**
** Your Diag routine should return a non-zero value in D0 for success.
** If this value is NULL, then the diag/init area that was copied
** will be returned to the free memory pool.
**

    INCLUDE "exec/types.i"
    INCLUDE "exec/nodes.i"
    INCLUDE "exec/resident.i"
    INCLUDE "libraries/configvars.i"

    ; LVO's resolved by linking with library amiga.lib
    XREF   _LVOFindResident

ROMINFO     EQU      0
ROMOFFS     EQU     $4000

* ROMINFO defines whether you want the AUTOCONFIG information in
* the beginning of your ROM (set to 0 if you instead have PALS
* providing the AUTOCONFIG information instead)
*
* ROMOFFS is the offset from your board base where your ROMs appear.
* Your ROMs might appear at offset 0 and contain your AUTOCONFIG
* information in the high nibbles of the first $40 words ($80 bytes).
* Or, your autoconfig ID information may be in a PAL, with your
* ROMs possibly being addressed at some offset (for example $2000)
* from your board base.  This ROMOFFS constant will be used as an
* additional offset from your configured board address when patching
* structures which require absolute pointers to ROM code or data.

*----- We'll store Version and Revision in serial number
VERSION 	EQU	37		; also the high word of serial number
REVISION	EQU	1		; also the low word of serial number

* See the Addison-Wesley Amiga Hardware Manual for more info.

MANUF_ID	EQU	2011		; CBM assigned (2011 for hackers only)
PRODUCT_ID	EQU	1		; Manufacturer picks product ID

BOARDSIZE	EQU	$10000          ; How much address space board decodes
SIZE_FLAG	EQU	3		; Autoconfig 3-bit flag for BOARDSIZE
					;   0=$800000(8meg)  4=$80000(512K)
					;   1=$10000(64K)    5=$100000(1meg)
					;   2=$20000(128K)   6=$200000(2meg)
					;   3=$40000(256K)   7=$400000(4meg)
            CODE

AllocMem    EQU -198

*******  RomStart  ***************************************************
**********************************************************************

RomStart:

*******  DiagStart  **************************************************
DiagStart:  ; This is the DiagArea structure whose relative offset from
            ; your board base appears as the Init Diag vector in your
            ; autoconfig ID information.  This structure is designed
            ; to use all relative pointers (no patching needed).
            dc.b    DAC_WORDWIDE+DAC_CONFIGTIME    ; da_Config
            dc.b    0                              ; da_Flags
            dc.w    $4000              ; da_Size
            dc.w    DiagEntry-DiagStart            ; da_DiagPoint
            dc.w    BootEntry-DiagStart            ; da_BootPoint
            dc.w    DevName-DiagStart              ; da_Name
            dc.w    0                              ; da_Reserved01
            dc.w    0                              ; da_Reserved02

*******  Resident Structure  *****************************************
Romtag:
            dc.w    RTC_MATCHWORD      ; UWORD RT_MATCHWORD
rt_Match:   dc.l    Romtag-DiagStart   ; APTR  RT_MATCHTAG
rt_End:     dc.l    EndCopy-DiagStart  ; APTR  RT_ENDSKIP
            dc.b    RTW_COLDSTART      ; UBYTE RT_FLAGS
            dc.b    VERSION            ; UBYTE RT_VERSION
            dc.b    NT_DEVICE          ; UBYTE RT_TYPE
            dc.b    20                 ; BYTE  RT_PRI
rt_Name:    dc.l    DevName-DiagStart  ; APTR  RT_NAME
rt_Id:      dc.l    IdString-DiagStart ; APTR  RT_IDSTRING
rt_Init:    dc.l    Init-RomStart      ; APTR  RT_INIT


******* Strings referenced in Diag Copy area  ************************
DevName:    dc.b    'PiSCSI Snake Controller',0                      ; Name string
IdString    dc.b    'PISCSI v0.1',0   ; Id string

DosName:    dc.b    'dos.library',0                ; DOS library name

DosDevName: dc.b    'ABC',0        ; dos device name for MakeDosNode()
                                   ;   (dos device will be ABC:)

            ds.w    0              ; word align

*******  DiagEntry  **************************************************
**********************************************************************
*
*   success = DiagEntry(BoardBase,DiagCopy, configDev)
*   d0                  a0         a2                  a3
*
*   Called by expansion architecture to relocate any pointers
*   in the copied diagnostic area.   We will patch the romtag.
*   If you have pre-coded your MakeDosNode packet, BootNode,
*   or device initialization structures, they would also need
*   to be within this copy area, and patched by this routine.
*
**********************************************************************

DiagEntry:
            nop
            nop
            nop
            move.l  #1,$80000020
            nop
            nop
            nop
            nop
            nop
            nop

            lea      patchTable-RomStart(a0),a1   ; find patch table
            adda.l   #ROMOFFS,a1                  ; adjusting for ROMOFFS

* Patch relative pointers to labels within DiagCopy area
* by adding Diag RAM copy address.  These pointers were coded as
* long relative offsets from base of the DiagArea structure.
*
dpatches:
            move.l   a2,d1           ;d1=base of ram Diag copy
dloop:
            move.w   (a1)+,d0        ;d0=word offs. into Diag needing patch
            bmi.s    bpatches        ;-1 is end of word patch offset table
            add.l    d1,0(a2,d0.w)   ;add DiagCopy addr to coded rel. offset
            bra.s    dloop

* Patches relative pointers to labels within the ROM by adding
* the board base address + ROMOFFS.  These pointers were coded as
* long relative offsets from RomStart.
*
bpatches:
            move.l   a0,d1           ;d1 = board base address
            add.l    #ROMOFFS,d1     ;add offset to where your ROMs are
rloop:
            move.w   (a1)+,d0        ;d0=word offs. into Diag needing patch
            bmi.s   endpatches       ;-1 is end of patch offset table
            add.l   d1,0(a2,d0.w)    ;add ROM address to coded relative offset
            bra.s   rloop

endpatches:
            moveq.l #1,d0           ; indicate "success"
            rts


*******  BootEntry  **************************************************
**********************************************************************

BootEntry:
            move.l  #2,$80000020
            nop
            nop
            nop
            nop
            nop

            lea     DosName(PC),a1          ; 'dos.library',0
            jsr     _LVOFindResident(a6)    ; find the DOS resident tag
            move.l  d0,a0                   ; in order to bootstrap
            move.l  RT_INIT(A0),a0          ; set vector to DOS INIT
            jsr     (a0)                    ; and initialize DOS
            rts

*
* End of the Diag copy area which is copied to RAM
*
EndCopy:
*************************************************************************

*************************************************************************
*
*   Beginning of ROM driver code and data that is accessed only in
*   the ROM space.  This must all be position-independent.
*

patchTable:
* Word offsets into Diag area where pointers need Diag copy address added
            dc.w   rt_Match-DiagStart
            dc.w   rt_End-DiagStart
            dc.w   rt_Name-DiagStart
            dc.w   rt_Id-DiagStart
            dc.w   -1

* Word offsets into Diag area where pointers need boardbase+ROMOFFS added
            dc.w   rt_Init-DiagStart
            dc.w   -1

*******  Romtag InitEntry  **********************************************
*************************************************************************

Init:       ; After Diag patching, our romtag will point to this
            ; routine in ROM so that it can be called at Resident
            ; initialization time.
            ; This routine will be similar to a normal expansion device
            ; initialization routine, but will MakeDosNode then set up a
            ; BootNode, and Enqueue() on eb_MountList.
            ;
            move.l  #3,$80000020
            nop
            nop
            nop
            nop

            move.l  #4,$80000020
            movea.l 4,a6
            move.l #$40000,d0
            moveq #0,d1
            jsr AllocMem(a6)

            move.l  d0,$80000040
            nop
            nop

            move.l  d0,a1
            move.l  #0,d1
            movea.l  4,a6
            add.l #$16e,a1
            move.l  #5,$80000020
            nop
            nop
            nop
            jsr     -102(a6)
            nop
            nop

            moveq.l #1,d0           ; indicate "success"

            rts
            END
