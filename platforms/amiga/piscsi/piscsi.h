// SPDX-License-Identifier: MIT

#include <stdint.h>

#include "platforms/amiga/hunk-reloc.h"

#define	TDF_EXTCOM (1<<15)

#define CMD_INVALID	0
#define CMD_RESET	1
#define CMD_READ	2
#define CMD_WRITE	3
#define CMD_UPDATE	4
#define CMD_CLEAR	5
#define CMD_STOP	6
#define CMD_START	7
#define CMD_FLUSH	8
#define CMD_NONSTD	9

#define	TD_MOTOR	(CMD_NONSTD+0)      // 9
#define	TD_SEEK		(CMD_NONSTD+1)      // 10
#define	TD_FORMAT	(CMD_NONSTD+2)      // 11
#define	TD_REMOVE	(CMD_NONSTD+3)      // 12
#define	TD_CHANGENUM	(CMD_NONSTD+4)  // 13
#define	TD_CHANGESTATE	(CMD_NONSTD+5)  // 15
#define	TD_PROTSTATUS	(CMD_NONSTD+6)  // 16
#define	TD_RAWREAD	(CMD_NONSTD+7)      // 17
#define	TD_RAWWRITE	(CMD_NONSTD+8)      // 18
#define	TD_GETDRIVETYPE	(CMD_NONSTD+9)  // 19
#define	TD_GETNUMTRACKS	(CMD_NONSTD+10) // 20
#define	TD_ADDCHANGEINT	(CMD_NONSTD+11) // 21
#define	TD_REMCHANGEINT	(CMD_NONSTD+12) // 22
#define TD_GETGEOMETRY	(CMD_NONSTD+13) // 23
#define TD_EJECT	(CMD_NONSTD+14)     // 24
#define	TD_LASTCOMM	(CMD_NONSTD+15)     // 25

#define	ETD_WRITE	(CMD_WRITE|TDF_EXTCOM)
#define	ETD_READ	(CMD_READ|TDF_EXTCOM)
#define	ETD_MOTOR	(TD_MOTOR|TDF_EXTCOM)
#define	ETD_SEEK	(TD_SEEK|TDF_EXTCOM)
#define	ETD_FORMAT	(TD_FORMAT|TDF_EXTCOM)
#define	ETD_UPDATE	(CMD_UPDATE|TDF_EXTCOM)
#define	ETD_CLEAR	(CMD_CLEAR|TDF_EXTCOM)
#define	ETD_RAWREAD	(TD_RAWREAD|TDF_EXTCOM)
#define	ETD_RAWWRITE	(TD_RAWWRITE|TDF_EXTCOM)

#define HD_SCSICMD 28

#define NSCMD_DEVICEQUERY 0x4000
#define NSCMD_TD_READ64   0xC000
#define NSCMD_TD_WRITE64  0xC001
#define NSCMD_TD_SEEK64   0xC002
#define NSCMD_TD_FORMAT64 0xC003

#define RDB_BLOCK_LIMIT 16

// RDSK
#define RDB_IDENTIFIER 0x5244534B
// PART
#define PART_IDENTIFIER 0x50415254
// FSHD
#define	FS_IDENTIFIER 0x46534844

#define PISCSI_DRIVER_OFFSET 0x1000
#define NUM_FILESYSTEMS 32

struct piscsi_dev {
    uint32_t c;
    uint16_t h, s;
    uint64_t fs;
    int32_t fd;
    uint32_t lba;
    uint32_t num_partitions;
    uint32_t fshd_offs;
    uint32_t block_size;
    struct PartitionBlock *pb[16];
    struct RigidDiskBlock *rdb;
};

struct piscsi_fs {
   struct FileSysHeaderBlock * fhb;
   uint32_t FS_ID;
   uint32_t handler;
   struct hunk_reloc relocs[4096];
   struct hunk_info h_info;
   uint8_t *binary_data;
};

//  .long 0 /* dos disk name */
//  .long 0 /* device file name */
//  .long 0 /* unit */
//  .long 0 /* flags */
struct DosEnvec {
    uint32_t de_TableSize;	     /* Size of Environment vector */
    uint32_t de_SizeBlock;	     /* in longwords: standard value is 128 */
    uint32_t de_SecOrg;	     /* not used; must be 0 */
    uint32_t de_Surfaces;	     /* # of heads (surfaces). drive specific */
    uint32_t de_SectorPerBlock; /* not used; must be 1 */
    uint32_t de_BlocksPerTrack; /* blocks per track. drive specific */
    uint32_t de_Reserved;	     /* DOS reserved blocks at start of partition. */
    uint32_t de_PreAlloc;	     /* DOS reserved blocks at end of partition */
    uint32_t de_Interleave;     /* usually 0 */
    uint32_t de_LowCyl;	     /* starting cylinder. typically 0 */
    uint32_t de_HighCyl;	     /* max cylinder. drive specific */
    uint32_t de_NumBuffers;     /* Initial # DOS of buffers.  */
    uint32_t de_BufMemType;     /* type of mem to allocate for buffers */
    uint32_t de_MaxTransfer;    /* Max number of bytes to transfer at a time */
    uint32_t de_Mask;	     /* Address Mask to block out certain memory */
    int32_t  de_BootPri;	     /* Boot priority for autoboot */
    uint32_t de_DosType;	     /* ASCII (HEX) string showing filesystem type;
			      * 0X444F5300 is old filesystem,
			      * 0X444F5301 is fast file system */
    uint32_t de_Baud;	     /* Baud rate for serial handler */
    uint32_t de_Control;	     /* Control word for handler/filesystem */
    uint32_t de_BootBlocks;     /* Number of blocks containing boot code */

};

struct pihd_dosnode_data {
    uint32_t name_ptr;
    uint32_t dev_name_ptr;
    uint32_t unit;
    uint32_t flags;
    uint32_t node_len;
    uint32_t block_len;
    uint32_t pad1;
    uint32_t surf;
    uint32_t secs_per_block;
    uint32_t blocks_per_track;
    uint32_t reserved_blocks;
    uint32_t pad2;
    uint32_t interleave;
    uint32_t lowcyl;
    uint32_t highcyl;
    uint32_t buffers;
    uint32_t mem_type;
    uint32_t maxtransfer;
    uint32_t transfer_mask;
    uint32_t priority;
    uint32_t dostype;
};

struct RigidDiskBlock {
    uint32_t   rdb_ID;
    uint32_t   rdb_SummedLongs;
    int32_t    rdb_ChkSum;
    uint32_t   rdb_HostID;
    uint32_t   rdb_BlockBytes;
    uint32_t   rdb_Flags;
    /* block list heads */
    uint32_t   rdb_BadBlockList;
    uint32_t   rdb_PartitionList;
    uint32_t   rdb_FileSysHeaderList;
    uint32_t   rdb_DriveInit;
    uint32_t   rdb_Reserved1[6];
    /* physical drive characteristics */
    uint32_t   rdb_Cylinders;
    uint32_t   rdb_Sectors;
    uint32_t   rdb_Heads;
    uint32_t   rdb_Interleave;
    uint32_t   rdb_Park;
    uint32_t   rdb_Reserved2[3];
    uint32_t   rdb_WritePreComp;
    uint32_t   rdb_ReducedWrite;
    uint32_t   rdb_StepRate;
    uint32_t   rdb_Reserved3[5];
    /* logical drive characteristics */
    uint32_t   rdb_RDBBlocksLo;
    uint32_t   rdb_RDBBlocksHi;
    uint32_t   rdb_LoCylinder;
    uint32_t   rdb_HiCylinder;
    uint32_t   rdb_CylBlocks;
    uint32_t   rdb_AutoParkSeconds;
    uint32_t   rdb_HighRDSKBlock;
    uint32_t   rdb_Reserved4;
    /* drive identification */
    char    rdb_DiskVendor[8];
    char    rdb_DiskProduct[16];
    char    rdb_DiskRevision[4];
    char    rdb_ControllerVendor[8];
    char    rdb_ControllerProduct[16];
    char    rdb_ControllerRevision[4];
    char    rdb_DriveInitName[40];
};

struct DeviceNode {
    uint32_t    dn_Next;
    uint32_t    dn_Type;
    uint32_t    dn_Task;
    uint32_t    dn_Lock;
    uint8_t	    *dn_Handler;
    uint32_t    dn_StackSize;
    int32_t     dn_Priority;
    uint32_t    dn_Startup;
    uint32_t    dn_SegList;
    uint32_t    dn_GlobalVec;
    uint8_t     *dn_Name;
};

struct PartitionBlock {
    uint32_t   pb_ID;
    uint32_t   pb_SummedLongs;
    int32_t    pb_ChkSum;
    uint32_t   pb_HostID;
    uint32_t   pb_Next;
    uint32_t   pb_Flags;
    uint32_t   pb_Reserved1[2];
    uint32_t   pb_DevFlags;
    uint8_t    pb_DriveName[32];
    uint32_t   pb_Reserved2[15];
    uint32_t   pb_Environment[20];
    uint32_t   pb_EReserved[12];
};

struct SCSICmd_ModeSense6 {
    uint8_t opcode;
    uint8_t reserved_dbd;
    uint8_t pc_pagecode;
    uint8_t subpage_code;
    uint8_t alloc_len;
    uint8_t control;
};

struct SCSICmd_RW10 {
    uint8_t opcode;
    uint8_t rdprotect_flags;
    uint32_t block;
    uint8_t res_groupnum;
    uint16_t len;
    uint8_t control;
};

struct FileSysHeaderBlock {
    uint32_t   fhb_ID;
    uint32_t   fhb_SummedLongs;
    int32_t    fhb_ChkSum;
    uint32_t   fhb_HostID;
    uint32_t   fhb_Next;
    uint32_t   fhb_Flags;
    uint32_t   fhb_Reserved1[2];
    uint32_t   fhb_DosType;
    uint32_t   fhb_Version;
    uint32_t   fhb_PatchFlags;
    uint32_t   fhb_Type;
    uint32_t   fhb_Task;
    uint32_t   fhb_Lock;
    uint32_t   fhb_Handler;
    uint32_t   fhb_StackSize;
    int32_t    fhb_Priority;
    int32_t    fhb_Startup;
    int32_t    fhb_SegListBlocks;
    int32_t    fhb_GlobalVec;
    uint32_t   fhb_Reserved2[23];
    uint8_t    fhb_FileSysName[84];
};

void piscsi_init();
void piscsi_shutdown();
void piscsi_map_drive(char *filename, uint8_t index);
void piscsi_unmap_drive(uint8_t index);
struct piscsi_dev *piscsi_get_dev(uint8_t index);

void handle_piscsi_write(uint32_t addr, uint32_t val, uint8_t type);
uint32_t handle_piscsi_read(uint32_t addr, uint8_t type);

void piscsi_find_filesystems(struct piscsi_dev *d);
void piscsi_refresh_drives();
