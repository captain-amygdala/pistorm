# PiNET Ethernet Interface/Device driver for Amiga

A SANA2-compatible Ethernet driver for use with AmiTCP-compatible stacks such as (well...) AmiTCP, Genesis, Roadshow, etc.

`pi-net.device` from the `net_driver_amiga` directory goes in the same drawer as other ethernet device drivers. The NetInterface file is currently missing, so one needs to hack another available one a bit.

# Instructions

PiNET is enabled by uncommenting the `setvar pi-net` line in default.cfg, or adding it to the config file you're currently using.

The device driver currently has no ETH frame send/receive functionality on the ARM side of things, so it can't do all that much except spew out a bunch of debug information.

# Making changes to the driver

The driver needs to be refactored a bit to work more like the RTG driver, but it can be built by using `build.sh` in the `net_driver_amiga` directory using bebbo's GCC for AmigaOS.
