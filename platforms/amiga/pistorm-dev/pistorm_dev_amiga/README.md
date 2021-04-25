# PiStorm Interaction Tool

## Cross-Compiling

Compiling the tool requires VBCC setup with the kickstart 1.3 libraries. For information on how to set this up in Linux please see https://linuxjedi.co.uk/2021/02/27/using-vbcc-as-an-amiga-cross-compiler-in-linux/

Once you have the tooling setup, just run `make`.

## Tools

### PiSimple

PiSimple is a command line tool to interact with PiStorm. Running it without any parameters will give you detailed usage information for the tool.

### PiStorm

The PiStorm tool is a GUI tool that implements the same interaction API as the command line tool. It is compatible with Workbench 1.3 onwards.

#### RTG

You can enable / disable RTG on-the-fly with the "Enable/Disable RTG" button. This will have a status next to it which indicates the current state of the RTG.

### Config file

It is possible to switch the configuration file PiStorm is using. You can either type a name for the config file relative to the PiStorm's execution directory and hit "Commit" or hit "Load Default". If the config file is valid the PiStorm will load it in and the Amiga will immediately reboot.
