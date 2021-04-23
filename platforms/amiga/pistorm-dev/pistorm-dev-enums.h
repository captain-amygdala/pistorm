// SPDX-License-Identifier: MIT

// Currently "2011" / 0x07DB - Defined as "Reserved for Hackers Only" in old Commodore documentation
#define PISTORM_AC_MANUF_ID 0x0, 0x7, 0xD, 0xB

// [R], [W] and [RW] indicate read, write or both access modes for register
// Any failure or result code from a write command should be put in PI_CMDRESULT
enum pistorm_dev_cmds {
    PI_CMD_RESET        = 0x00, // [W] Reset the host system.
    PI_CMD_SWITCHCONFIG = 0x02, // [W] Switch config file to string at PI_STR1, if it exists.
                                //     This will reset the Amiga if the config loads successfully.
    PI_CMD_PISCSI_CTRL  = 0x04, // [RW] Write: Control a PiSCSI device. The command written here uses
                                //      values From various data registers around $2000.
                                //      Read: Returns whether PiSCSI is enabled or not.
    PI_CMD_RTGSTATUS    = 0x06, // [RW] Read: Check RTG status Write: Set RTG status (enabled/disabled)
    PI_CMD_NETSTATUS    = 0x08, // [RW] Read: Check ETH status Write: Set ETH status (enabled/disabled)
    PI_CMD_KICKROM      = 0x0A, // [W] Map a different Kickstart ROM to the standard address using
                                //     the string at PI_STR1, if the file exists. Requires some config
                                //     file names to be set in order to find it.
    PI_CMD_EXTROM       = 0x0E, // [W] Same as above, but the extended ROM.

    PI_CMD_HWREV        = 0x10, // [R] Check the PiStorm hardware version/revision
    PI_CMD_SWREV        = 0x12, // [R] Check the PiStorm software version/revision

    PI_CMD_QBASIC       = 0x0FFC, // QBasic
    PI_CMD_NIBBLES      = 0x0FFE, // Nibbles

    PI_DBG_MSG          = 0x1000, // [W] Trigger debug message output to avoid slow serial kprintf.
    PI_DBG_VAL1         = 0x1010, // [RW]
    PI_DBG_VAL2         = 0x1014, // [RW]
    PI_DBG_VAL3         = 0x1018, // [RW]
    PI_DBG_VAL4         = 0x101C, // [RW]
    PI_DBG_VAL5         = 0x1020, // [RW]
    PI_DBG_VAL6         = 0x1024, // [RW]
    PI_DBG_VAL7         = 0x1028, // [RW]
    PI_DBG_VAL8         = 0x102C, // [RW]
    PI_DBG_STR1         = 0x1030, // [RW] Pointers to debug strings (typically in "Amiga RAM")
    PI_DBG_STR2         = 0x1034, // [RW]
    PI_DBG_STR3         = 0x1038, // [RW]
    PI_DBG_STR4         = 0x103C, // [RW]

    PI_BYTE1            = 0x2000, // [RW] // Bytes, words and longwords used as extended arguments.
    PI_BYTE2            = 0x2001, // [RW] // for PiStorm interaction device commands.
    PI_BYTE3            = 0x2002, // [RW]
    PI_BYTE4            = 0x2003, // [RW]
    PI_BYTE5            = 0x2004, // [RW]
    PI_BYTE6            = 0x2005, // [RW]
    PI_BYTE7            = 0x2006, // [RW]
    PI_BYTE8            = 0x2007, // [RW]
    PI_WORD1            = 0x2008, // [RW]
    PI_WORD2            = 0x200A, // [RW]
    PI_WORD3            = 0x200C, // [RW]
    PI_WORD4            = 0x200E, // [RW]
    PI_LONGWORD1        = 0x2010, // [RW]
    PI_LONGWORD2        = 0x2014, // [RW]
    PI_LONGWORD3        = 0x2018, // [RW]
    PI_LONGWORD4        = 0x201C, // [RW]
    PI_STR1             = 0x2020, // [RW] Pointers to strings (typically in "Amiga RAM")
    PI_STR2             = 0x2024, // [RW]
    PI_STR3             = 0x2028, // [RW]
    PI_STR4             = 0x202C, // [RW]

    PI_CMDRESULT        = 0x2100, // [R] Check the result of any command that provides a "return value".
};
