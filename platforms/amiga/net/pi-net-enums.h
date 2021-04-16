// SPDX-License-Identifier: MIT

#define PINET_OFFSET  0x80010000
#define PINET_REGSIZE 0x00010000
#define PINET_UPPER   0x80020000

/*enum piscsi_stuff {
    PISCSI_BLOCK_SIZE = 512,
    PISCSI_TRACK_SECTORS = 2048,
};*/

#define ADDRFIELDSIZE 6
#define ETH_HDR_SIZE  14

enum pinet_cmds {
    PINET_CMD_WRITE     = 0x00,
    PINET_CMD_READ      = 0x02,
    PINET_CMD_MAC       = 0x04,
    PINET_CMD_IP        = 0x0A,
    PINET_CMD_BEEF      = 0x0E,
    PINET_CMD_ADDR1     = 0x10,
    PINET_CMD_ADDR2     = 0x14,
    PINET_CMD_ADDR3     = 0x18,
    PINET_CMD_ADDR4     = 0x1C,
    PINET_CMD_FRAME     = 0x1000,
};
