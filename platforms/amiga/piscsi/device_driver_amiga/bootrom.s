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
    INCLUDE "libraries/expansionbase.i"

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
VERSION 	    EQU	37		; also the high word of serial number
REVISION	    EQU	1		; also the low word of serial number

* See the Addison-Wesley Amiga Hardware Manual for more info.
    
MANUF_ID	    EQU	2011		; CBM assigned (2011 for hackers only)
PRODUCT_ID	    EQU	1		; Manufacturer picks product ID

BOARDSIZE	    EQU	$10000          ; How much address space board decodes
SIZE_FLAG	    EQU	3		; Autoconfig 3-bit flag for BOARDSIZE
			    		;   0=$800000(8meg)  4=$80000(512K)
			    		;   1=$10000(64K)    5=$100000(1meg)
			    		;   2=$20000(128K)   6=$200000(2meg)
			    		;   3=$40000(256K)   7=$400000(4meg)
                CODE

; Exec stuff
AllocMem        EQU -198
InitResident    EQU -102
FindResident    EQU -96
OpenLibrary     EQU -552
CloseLibrary    EQU -414
OpenResource    EQU -$1F2
AddResource     EQU -$1E6
Enqueue         EQU -$10E
AddMemList      EQU -$26A

; Expansion stuff
MakeDosNode     EQU -144
AddDosNode      EQU -150
AddBootNode     EQU -36

; PiSCSI stuff
PiSCSIAddr1     EQU $80000010
PiSCSIAddr2     EQU $80000014
PiSCSIAddr3     EQU $80000018
PiSCSIAddr4     EQU $8000001C
PiSCSIDebugMe   EQU $80000020
PiSCSIDriver    EQU $80000040
PiSCSINextPart  EQU $80000044
PiSCSIGetPart   EQU $80000048
PiSCSIGetPrio   EQU $8000004C
PiSCSIGetFS     EQU $80000060
PiSCSINextFS    EQU $80000064
PiSCSICopyFS    EQU $80000068
PiSCSIFSSize    EQU $8000006C
PiSCSISetFSH    EQU $80000070
PiSCSILoadFS    EQU $80000084
PiSCSIGetFSInfo EQU $80000088
PiSCSIDbg1      EQU $80001010
PiSCSIDbg2      EQU $80001014
PiSCSIDbg3      EQU $80001018
PiSCSIDbg4      EQU $8000101C
PiSCSIDbg5      EQU $80001020
PiSCSIDbg6      EQU $80001024
PiSCSIDbg7      EQU $80001028
PiSCSIDbg8      EQU $8000102C
PiSCSIDbgMsg    EQU $80001000

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
DevName:    dc.b    'pi-scsi.device',0,0                      ; Name string
IdString    dc.b    'PISCSI v0.8',0   ; Id string

DosName:        dc.b    'dos.library',0                ; DOS library name
ExpansionName:  dc.b    "expansion.library",0
LibName:        dc.b    "pi-scsi.device",0,0

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
            align 2
            nop
            nop
            nop
            move.l #1,PiSCSIDebugMe
            move.l a3,PiSCSIAddr1
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
            align 2
            move.l #2,PiSCSIDebugMe
            lea DosName(pc),a1
            jsr FindResident(a6)
            tst.l d0
            beq.b .End
            move.l d0,a0
            move.l RT_INIT(a0),a0
            jmp (a0)
.End
            moveq.l #1,d0           ; indicate "success"
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
            align 2
            move.l a6,-(a7)             ; Push A6 to stack
            ;move.w #$00B8,$dff09a       ; Disable interrupts during init
            move.l  #3,PiSCSIDebugMe
            move.l a3,PiSCSIAddr4

            movea.l 4,a6

            move.l $10000040,d1
            move.l #$feffeeff,$10000040
            move.l $10000040,d0
            cmp.l #$feffeeff,d0
            bne.s NoZ3
            move.l d1,$10000040

            move.l #$8000000,d0         ; Add some Z3 fast RAM if it hasn't been moved (Kick 1.3)
            move.l #$405,d1
            move.l #10,d2
            move.l #$10000000,a0
            move.l #0,a1
            jsr AddMemList(a6)

NoZ3:
            move.l  #11,PiSCSIDebugMe
            lea LibName(pc),a1
            jsr FindResident(a6)
            move.l  #10,PiSCSIDebugMe
            cmp.l #0,d0
            bne.s SkipDriverLoad        ; Library is already loaded, jump straight to partitions

            move.l  #4,PiSCSIDebugMe
            movea.l 4,a6
            move.l #$40000,d0
            moveq #0,d1
            jsr AllocMem(a6)            ; Allocate memory for the PiStorm to copy the driver to

            move.l  d0,PiSCSIDriver     ; Copy the PiSCSI driver to allocated memory and patch offsets

            move.l  #5,PiSCSIDebugMe
            move.l  d0,a1
            move.l  #0,d1
            movea.l  4,a6
            add.l #$028,a1
            jsr InitResident(a6)        ; Initialize the PiSCSI driver

SkipDriverLoad:
            move.l  #9,PiSCSIDebugMe
            jsr LoadFileSystems(pc)

FSLoadExit:
            lea ExpansionName(pc),a1
            moveq #0,d0
            jsr OpenLibrary(a6)         ; Open expansion.library to make this work, somehow
            move.l a6,a4
            move.l d0,a6

            move.l  #7,PiSCSIDebugMe
PartitionLoop:
            move.l PiSCSIGetPart,d0     ; Get the available partition in the current slot
            beq.w EndPartitions         ; If the next partition returns 0, there's no additional partitions
            move.l d0,a0
            jsr MakeDosNode(a6)
            cmp.l #0,PiSCSIGetFSInfo
            beq.s SkipLoadFS

            move.l d0,PiSCSILoadFS        ; Attempt to load the file system driver from data/fs
            cmp.l #$FFFFFFFF,PiSCSIAddr4
            beq SkipLoadFS

            jsr LoadFileSystems(pc)

SkipLoadFS:
            move.l d0,PiSCSISetFSH
            move.l d0,PiSCSIAddr2       ; Put DeviceNode address in PiSCSIAddr2, because I'm useless
            move.l d0,a0
            move.l PiSCSIGetPrio,d0
            move.l #0,d1
            move.l PiSCSIAddr1,a1

* Uncomment these lines to test AddDosNode/Enqueue stuff
* Or comment them out all the way down to and including SkipEnqueue: to use the AddBootNode method instead.
            cmp.l   #-128,d0
            bne.s   EnqueueNode

* BOOL AddDosNode( LONG bootPri, ULONG flags, struct DeviceNode *deviceNode );
* amicall(ExpansionBase, 0x96, AddDosNode(d0,d1,a0))
            move.l #38,PiSCSIDebugMe
            jsr AddDosNode(a6)
            bra.w SkipEnqueue
* VOID Enqueue( struct List *list, struct Node *node );
* amicall(SysBase, 0x10e, Enqueue(a0,a1))

EnqueueNode:
            exg a6,a4
            ;move.l #35,PiSCSIDebugMe
            ;move.l #BootNode_SIZEOF,PiSCSIDebugMe
            ;move.l #NT_BOOTNODE,PiSCSIDebugMe
            ;move.l #LN_TYPE,PiSCSIDebugMe
            ;move.l #LN_PRI,PiSCSIDebugMe
            ;move.l #LN_NAME,PiSCSIDebugMe
            ;move.l #eb_MountList,PiSCSIDebugMe
            ;move.l #35,PiSCSIDebugMe

            move.l #BootNode_SIZEOF,d0
            move.l #$10001,d1
            jsr AllocMem(a6)            ; Allocate memory for the BootNode

            move.l d0,PiSCSIAddr3
            move.l #36,PiSCSIDebugMe

            move.l d0,a1
            move.b #NT_BOOTNODE,LN_TYPE(a1)
            move.l PiSCSIGetPrio,d0
            move.b d0,LN_PRI(a1)
            move.l PiSCSIAddr2,bn_DeviceNode(a1)
            move.l PiSCSIAddr1,LN_NAME(a1)

            lea eb_MountList(a4),a0
            jsr Enqueue(a6)
            exg a6,a4

SkipEnqueue:

* BOOL AddBootNode( LONG bootPri, ULONG flags, struct DeviceNode *deviceNode, struct ConfigDev *configDev );
* amicall(ExpansionBase, 0x24, AddBootNode(d0,d1,a0,a1))
* Comment out the line below to test AddDosNode/Enqueue stuff
*            jsr AddBootNode(a6)
            move.l #1,PiSCSINextPart    ; Switch to the next partition
            bra.w PartitionLoop


EndPartitions:
            move.l #8,PiSCSIDebugMe
            move.l a6,a1
            move.l #800,PiSCSIDebugMe
            movea.l 4,a6
            move.l #801,PiSCSIDebugMe
            jsr CloseLibrary(a6)
            move.l #802,PiSCSIDebugMe

            move.l (a7)+,a6             ; Pop A6 from stack
            move.l #803,PiSCSIDebugMe

            ;move.w #$80B8,$dff09a       ; Re-enable interrupts
            move.l #804,PiSCSIDebugMe
            moveq.l #1,d0               ; indicate "success"
            move.l #805,PiSCSIDebugMe
            rts

            align 4
FileSysName     dc.b    'FileSystem.resource',0
FileSysCreator  dc.b    'PiStorm',0

CurFS:          dc.l    $0
FSResource:     dc.l    $0

            align 2
LoadFileSystems:
            movem.l d0-d7/a0-a6,-(sp)       ; Push registers to stack
            move.l #30,PiSCSIDebugMe
            movea.l 4,a6
ReloadResource:
            lea FileSysName(pc),a1
            jsr OpenResource(a6)
            tst.l d0
            bne FSRExists

            move.l #33,PiSCSIDebugMe        ; FileSystem.resource isn't open, create it
                                            ; Code based on WinUAE filesys.asm

            moveq #32,d0                    ; sizeof(FileSysResource)
            move.l #$10001,d1
            jsr AllocMem(a6)
            move.l d0,a2
            move.b #8,8(a2)                 ; NT_RESOURCE
            lea FileSysName(pc),a0
            move.l a0,10(a2)                ; node name
            lea FileSysCreator(pc),a0
            move.l a0,14(a2)                ; fsr_Creator
            lea 18(a2),a0
            move.l a0,(a0)                  ; NewList() fsr_FileSysEntries
            addq.l #4,(a0)
            move.l a0,8(a0)
            lea $150(a6),a0                 ; ResourceList
            move.l a2,a1
            jsr -$f6(a6)                    ; AddTail
            move.l a2,a0
            move.l a0,d0

FSRExists:  
            move.l d0,PiSCSIAddr2             ; PiSCSIAddr2 is now FileSystem.resource
            move.l #31,PiSCSIDebugMe
            move.l PiSCSIAddr2,a0
            move.l PiSCSIGetFS,d0
            cmp.l #0,d0
            beq.w FSDone

FSNext:     
            move.l #45,PiSCSIDebugMe
            lea fsr_FileSysEntries(a0),a0
            move.l a0,d2
            move.l LH_HEAD(a0),d0
            beq.w NoEntries

FSLoop:     
            move.l #34,PiSCSIDebugMe
            move.l d0,a1
            move.l #35,PiSCSIDebugMe
            cmp.l fse_DosType(a1),d7
            move.l #36,PiSCSIDebugMe
            beq.w AlreadyLoaded
            move.l #37,PiSCSIDebugMe
            move.l LN_SUCC(a1),d0
            bne.w FSLoop
            move.l #390,PiSCSIDebugMe
            bra.w NoEntries

            align 2
NoEntries:  
            move.l #39,PiSCSIDebugMe
            move.l PiSCSIFSSize,d0
            move.l #40,PiSCSIDebugMe
            move.l #$10001,d1
            move.l #41,PiSCSIDebugMe
            jsr AllocMem(a6)
            move.l d0,PiSCSIAddr3
            move.l d0,a0
            move.l #1,PiSCSICopyFS
            move.b #NT_RESOURCE,LN_TYPE(a0)

AlreadyLoaded:
            move.l #480,PiSCSIDebugMe
            move.l PiSCSIAddr2,a0
            move.l #1,PiSCSINextFS
            move.l PiSCSIGetFS,d0
            cmp.l #0,d0
            bne.w FSNext

FSDone:     move.l #37,PiSCSIDebugMe
            move.l #32,PiSCSIDebugMe    ; Couldn't open FileSystem.resource, Kick 1.2/1.3?

            movem.l (sp)+,d0-d7/a0-a6   ; Pop registers from stack
            rts

FSRes
    dc.l    0
    dc.l    0
    dc.b    NT_RESOURCE
    dc.b    0
    dc.l    FileSysName
    dc.l    FileSysCreator
.Head
    dc.l    .Tail
.Tail
    dc.l    0
    dc.l    .Head
    dc.b    NT_RESOURCE
    dc.b    0
