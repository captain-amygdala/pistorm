# PiStorm config file

The PiStorm config file provides a way for you to set up things like mapped RAM, ROM and register regions that can be processed in software by the PiStorm emulator.

This is useful for instance when wanting to map a region of RAM that is accessoble to the CPU only (such as Fast RAM on the Amiga) or when processing requests from the OS to expansion hardware (such as AutoConfig on the Amiga).

To select a config file to use when launching the emulator, simply append `--config` and a path to a config file when launching the emulator, an example of usage would be something like this: `sudo ./emulator --config my_config_file.cfg`

# Generic config file commands

There are some generic config file commands available that work on any platform, and can be used for basic system setup or to "blackhole" certain memory ranges from communicating over the CPU bus.

Comments in the config files are lines that start with `#`.  
These can be things like information about the config file option below, or simply a commented out config file command that might not be suitable to have enabled by default. An example of a comment would for instance be `# Map 512KB kickstart ROM to default offset.`

Commands in the config file look a little something like this: `cpu 68020`  
In this case, the command is `cpu` and the argument for the command is `68020`, this is the command to set which CPU type the emulator should be running.  
Optional command arguments are enclosed in curly brackets (`{}`).  
Address ranges and sizes can be specified in standard integer format (`12345`), hexadecimal format (`0x12345`) or KB/MB/GB format (`64K, 2M, 4G`).

Changing random options for no reason should be avoided. If you're not sure what a certain option does or if you should change it, ask someone for help first.  
Here's a list of config file commands that can be used with any platform:

# cpu

SYNTAX: `cpu CPU_TYPE`  
Example: `cpu 68020`  
This command takes a single argument, which is the CPU type to emulate. Valid CPU types are `68000`, `68010`, `68EC020`, `68020`, `68EC030`, `68030`, `68EC040`, `68LC040`, `68040` and `SCC68070`.

# map

SYNTAX: `map type=MAP_TYPE address=BASE_ADDRESS size=MAP_SIZE {range=MAP_START-MAP_END} {file=MAP_DATA_FILE} {ovl=MIRROR_ADDRESS} {id=MAP_ID} {autodump_mem} {autodump_file}`  
Example: `map type=rom address=0xF80000 size=0x80000 file=kick.rom ovl=0 id=kickstart autodump_mem`  
This command maps a range of memory to be handled in some way by the PiStorm emulator itself. These mapped ranges can then be handled per platform in the `platform_read/write_check` functions in `emulator.c`.  
A short description of each argument:
* `type` - This sets the type of memory range to map, the following are available:
  * `rom` - A "Read Only Memory" range, any writes to a `rom` type memory region will be silently ignored and passed on either to the bus or a different mapped memory range.
  * `ram` - A "Random Access Memory" range, appears to the host system as a standard big endian memory range, but can only be used by the CPU itself. These memory ranges can never be available for DMA from various peripherals.
  * `register` - A "Register" range, for when you want to take over the operation of an entire range of register addresses, such as the RTC or Gayle range on an Amiga.
  * `ram_noalloc` - A RAM range which doesn't get memory allocated to itself by default. If you are unsure what this is, you should not use it.
  * `wtcram` - A "Write Through Cache" RAM map. This is a RAM range where only writes get passed on over the physical CPU bus, useful for when there's a section of memory that is **only ever read** by physical system peripherals, but **never written to**.
* `address` - Which address that the memory range should start at.
* `size` - The size of the mapped range. This argument can be optional, but only when mapping a data file to a `rom` type range, in which case it defaults the size of the mapped range to the size of the file it loads.
* `range` - This can be used instead of `address` and `size` by simply specifying a full range to map to, for instance `0xF800-0xFFFF` to map the entire range between `0xF800-0xFFFF`.
* `file` - The name of a file to load data from for a `rom` type map. If this file is not found, the `rom` mapping is not enabled, for ease of troubleshooting reasons.
* `ovl` - An address that a `rom` type mapping should be redirected to while ROM overlay functionality is enabled. For instance the `ovl=0` option on the Amiga Kickstart means that it will be mirrored at the address `0x0` (or `$0`) while OVL is enabled.
* `id` - A name/ID of a certain mapping. These can be used for identifying mapped ranges in the emulator, and are used for instance with various RAM and ROM types in the Amiga platform code.
* `autodump_mem` / `autodump_file` - When a `rom` type mapping has one of these options set, it will automatically "dump" the ROM contents if the `file` specified cannot be loaded. `autodump_mem` simply copies the ROM data to Pi memory and keeps it there temporarily, while `autodump_file` will dump it to the `file` filename specified to be automatically loaded on next launch.

# loopcycles

SYNTAX: `loopcycles LOOP_CYCLES`  
Example: `loopcycles 300`  
This command sets the number of CPU cycles the 680x0 core will attempt to execute before breaking out of the CPU emulation loop to service other emulator software things, like IRQs. Setting this value higher than the default `300` **can** increase performance in some cases where not a lot of IRQs are being triggered.

# platform

SYNTAX: `platform PLATFORM_NAME {SUB_SYSTEM}`  
Example: `platform amiga`  
This commands specifies which platform to target with the current emulator config file, valid options are currently:
* `none` - A generic non-specific platform. Has no custom `setvar` or mapped range handling outside of mapping RAM/ROM ranges.
* `amiga` - The Amiga computer platform, includes a bunch of optional functionality through `setvar` commands.
* `mac68k` - Mac 68k or Mac Classic, at the moment mostly a skeleton platform with some Mac-specific functionality added.
* `x68000` - Skeleton driver for the X68000 line of computers, the sample config file includes some common mapped RAM/ROM ranges, but none of it currently works.
The `SUB_SYSTEM` argument can be used to indicate a specific computer model where mapped register ranges may be different from other systems of the same family, for instance `platform amiga 4000` uses a different address range than other models for the IDE controller.  
**Note:** The `SUB_SYSTEM` argument is currently not used for much of anything, and there is no point in setting it to anything unless you know you really need it.

# subsys

SYNTAX: 

# keyboard

SYNTAX: `keyboard {grab_key} {grab / nograb} {autoconnect / noautoconnect}`  
Example: `keyboard k nograb noautoconnect`  
This command forward keyboard events to host system, defaults to off unless `grab_key` is pressed, toggled off using F12. The default `grab_key` if none is specified is `k`.  
`grab` steals the keyboard from the Pi so Amiga/etc input is not sent to the Pi at all (also helps prevent sending any ctrl-alt-del to the Amiga from resetting the Pi).  
`autoconnect` connects the keyboard to the Amiga/etc on startup.  
**Please note that the keyboard/mouse forwarding is completely platform-specific. There is no generic keyboard/mouse protocol that works with all host systems. At the moment keyboard/mouse forwarding only works on Amiga, and not quite properly. This is not a universal replacement for your host system mouse/keyboard.**

# kbfile

SYNTAX: `kbfile keyboard_event_filename`  
Example: `kbfile /dev/input/event1`  
This command selects a specific filename for the keyboard event source. This is typically `/dev/input/event1` or `/dev/input/event0`, but it may be `/dev/input/event3` with for instance a wireless keyboard.  
Use ls /dev/input/event* to check which event files are available and try until you find the one that works.  
**Please note that the keyboard/mouse forwarding is completely platform-specific. There is no generic keyboard/mouse protocol that works with all host systems. At the moment keyboard/mouse forwarding only works on Amiga, and not quite properly. This is not a universal replacement for your host system mouse/keyboard.**

# mouse

SYNTAX: `mouse mouse_event_filename grab_key {grab / nograb} {autoconnect / noautoconnect}`  
Example: `mouse /dev/input/mice m noautoconnect`  
This command forwards Pi mouse events to host system, defaults to off unless `autoconnect` is specified. Toggled on/off using `grab_key`.  
See the `keyboard` command above for descriptions of the `grab` and `autoconnect` options.  
**Please note that the keyboard/mouse forwarding is completely platform-specific. There is no generic keyboard/mouse protocol that works with all host systems. At the moment keyboard/mouse forwarding only works on Amiga, and not quite properly. This is not a universal replacement for your host system mouse/keyboard.**

# setvar

SYNTAX: `setvar VARIABLE_NAME {argument 1} {argument 2} ...`  
Example: `setvar swap-df0-df 1`  
Last but not least is the `setvar` command. This allows setting variables specific to the `platform` to enable/disable certain optional functionality, such as the RTG on the Amiga. More information about platform-specific `setvar` options can (hopefully) be found in the `platforms/[system_name]` directories on the github repo.
