# PiStorm Interaction Tools

## Cross-Compiling

Compiling the tool requires VBCC setup with the kickstart 1.3 libraries. For information on how to set this up in Linux please see https://linuxjedi.co.uk/2021/02/27/using-vbcc-as-an-amiga-cross-compiler-in-linux/

Once you have the tooling setup, just run `make`.

## Installation

You will need `reqtools.library` in the `libs:` drive. Some Workbench builds come with this. If you don't have it use the version in libs13 for Workbench 1.3 and libs20 for Workbench 2.0 onwards.

## Tools

### PiSimple

PiSimple is a command line tool to interact with PiStorm. Running it without any parameters will give you detailed usage information for the tool.

### PiStorm

The PiStorm tool is a GUI tool that implements the same interaction API as the command line tool. It is compatible with Workbench 1.3 onwards.

#### RTG

You can enable / disable RTG on-the-fly with the "Enable/Disable RTG" button. This will have a status next to it which indicates the current state of the RTG.

### Config file

It is possible to switch the configuration file PiStorm is using. You can either type a name for the config file relative to the PiStorm's execution directory and hit "Commit" or hit "Load Default". If the config file is valid the PiStorm will load it in and the Amiga will immediately reboot.

### Get file

You can copy a file from the PiStorm to the Amiga. First of all, type in the filename and path relative to the pistorm directory. Then optionally set the destination directory (can be left blank for the same directory PiStorm was executed in) and hit "Retrieve".

### Reboot

Reboots the Amiga (not PiStorm).

### Kickstart file

This will let you choose a Kickstart ROM file on the Pi to boot from. As with Config gile, you can type the name of the Kickstart ROM file relative to the PiStorm's execution directory and hit "Commit". If the Kickstart ROM file is valie the PiStorm will load it in and the Amiga will immediately reboot.

**NOTE:** Instead of rebooting the Amiga may crash here. This would be due to an interrupt trying to access an old ROM position in the new ROM file before the reboot could execute. You can resolve this by manually doing a Ctrl-A-A reboot.

### Shutdown Pi

When pressing this button and confirming, the Pi will safely shutdown the underlying Linux OS. When this happens the Amiga will essentially hang as it will be as if the CPU has been removed. There will be no indication that the Pi shutdown has completed so it is probably wise to wait 10-15 seconds before removing power if this option is used.
