# Amiga-specific config file setvar options

Here's a list of all(?) config file `setvar` options available for the Amiga platform.  
*Deprecated* after a variable name means that the option is legacy and should no longer be used. There is probably a replacement available for it.

# a314

Example: `setvar a314`  
Enables A314 emulation. There's a lot that can be said about this, but you're better off checking out the readme in the `a314` folder in the GitHub repo.

# a314_conf

Example: `setvar a314_conf ../a314.conf`  
Sets a custom config filename path for the A314 emulation. This has nothing to do with PiStorm config files, and the `setvar` option **must** be specified ahead of the `setvar a314` line.

# cdtv

Example: `setvar cdtv`  
Enables "CDTV mode". This is not meant to be used on an actual CDTV, but rather on an Amiga 500/2000 to emulate the computer being a CDTV. It is currently missing DMAC/CD-ROM drive emulation, and cannot be used for much other than watching the pretty CDTV Kickstart screen on your Amiga.

# enable_rtc_emulation

Example: `setvar enable_rtc_emulation VALUE`  
Mostly used to disable the RTC emulation by uncommenting the line `setvar enable_rtc_emulation 0`.  
**Note:** There is not much point to disabling the PiStorm RTC emulation, unless you are using an actual A314 in your Amiga, or if you absolutely want to use a physical RTC in your Amiga instead of the emulated PiStorm one.

# hdd0 / hdd1 - *Deprecated*

Example: `setvar hdd0 ../hd0.img`  
Old options to set the HDD image files used by the (now removed) Gayle emulation. There were too many issues with it, and it didn't work with Kickstart 1.3 either way, only very specific Kickstart 2.0+ ROMs.  
**There are currently no plans to bring back the Gayle emulation, and you should consider using PiSCSI instead.**

# kick13

Example: `setvar kick13`  
Enables Kickstart 1.3 mode, completely disabling any Zorro 3 PIC enumeration.  
**This is not a very useful option, and you should definitely not be enabling it unless you know you need it.**

# move-slow-to-chip
Example: `setvar move-slow-to-chip`  
"Promotes" Slow RAM to the Chip RAM, giving 1MB of Chip on compatible Amiga 500(?) computers.
**Note:** Requires a 1MB ECS Agnus (8372) and a trapdoor memory expansion!

# no-pistorm-dev

Example: `no-pistorm-dev`  
Disables the PiStorm Interaction AutoConfig device. Disabling this can be useful if you're running out of Zorro II address space somehow, or if you just really hate the PiStorm Interaction Device and wish that it would burn in **Hell**.

# rtc_type
Example: `rtc_type msm`  
Enables you to set the emulated RTC chip type to `msm` if you for some reason need that for something. Typically, the Ricoh RTC emulation ends up working fine.  
**This is not a very useful option, and you should definitely not be enabling it unless you know you need it.**

# pi-net

Example: `setvar pi-net`  
Enables the **(currently non-working)** Pi-Net interface. This is currently just a skeleton driver and interface for a SANA-II compatible virtual ethernet card thing.  
**This is not a very useful option, and you should definitely not be enabling it unless you know you need it.**

# piscsi

Example: `setvar piscsi`  
Enables the PiSCSI high performance virtual SCSI host device. More detailed information is available in the `platforms/amiga/piscsi` readme file.  
**Note:** PiSCSI **requires** 32-bit CPU emulation to work properly. It will not function with a 68000, 68010 or 68EC020 CPU selected in the config file.

# piscsi0-piscsi6

Example: `setvar piscsi0 PI0.hdf`  
The `piscsi0` through `piscsi6` variables allow you to mount an RDB hard drive image or a physical drive for use with the PiSCSI interface.  
More detailed information is available in the `platforms/amiga/piscsi` readme file.

# rtg

Example: `setvar rtg`  
Enables the PiGFX RTG (ReTargetable Graphics) emulation, giving your Amiga a reasonably powerful Picasso96/CyberGraphics (CGX through Picasso96/P96 compatibility layer only, it is not possible to create a native CGX driver) graphics card that can be used for software and games that support RTG functionality.  
Combined with the PiStorm Interaction Device, this also offers some additional acceleration that can be used to speed up large screen buffer blits considerably.  
More detailed RTG information can be found in the `platforms/amiga/rtg` readme file.  
**Note:** The PiGFX RTG **requires** 32-bit CPU emulation to work properly. It will not function with a 68000, 68010 or 68EC020 CPU selected in the config file. On the other hand, neither will Picasso96, so that's probably okay.

# swap-df0-df

Example: `setvar swap-df0-df 1`  
Swaps DF0 with DF1/2/3. Useful for Kickstart 1.x and/or Trackloader games that will only boot from DF0.
