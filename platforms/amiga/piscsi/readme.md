# PiSCSI Interface/Device driver for Amiga

Intended to be used as a high performance replacement for scsi.device, can currently be used for mounting raw RDB disk images (RDSK) for use in Workbench.

This driver and interface is work in progress, do not use it in conjunction with any critical data that you need to survive.

# Making changes to the driver

If you make changes to the driver, you can always test these on the Amiga as a regular file in `DEVS:`, but the Z2 device has to be disabled for this to work properly.

If you use the boot ROM, the separate `pi-scsi.device` driver file currently has to match the one in `piscsi.rom`. This is due to an oversight, and will probably be addressed at some point...

Steps to create an updated boot ROM, all of these are done in the `device_driver_amiga` directory:

* (Optional) If you've made changes to bootrom.s, first run `./build.sh`.
* (Optional) If you've had build.sh create a new `bootrom` file, you need to chop off the first few bytes of it, since VASM adds a single hunk to the beginning of it. Simply delete all bytes up until you bump into the value `0x90`, this is the first value in the boot ROM identifier.
* Compile the new `pi-scsi.device` using `./build2.sh`.
* (Optional) If you haven't previously compiled the `makerom` binary, or the code for it has been updated since last time, simply run `gcc makerom.c -o makerom`
* Run `./makerom` to assemble the boot ROM file, it's automatically in the correct place for the emulator to find it.

# Instructions

In a perfect world, the PiSCSI boot ROM would automatically detect drives/partitions and add them as boot nodes to be available during early startup, but this is not yet possible.

To enable the PiSCSI interface, uncomment the `setvar piscsi` line in default.cfg, or add it to the config file you're currently using.

Add disk images to the PiSCSI interface by uncommenting the `piscsi0` and `piscsi1` lines and editing them to point at the disk image(s) you want to use. `piscsi0` through `piscsi6` are available for a total of seven mapped drives.

To get a hard drive image mounted when WB starts, you need a few things:
* Copy pi-scsi.device from the `device_driver_amiga` folder to `SYS:Devs` on your Amiga.
  If you're super savvy, the driver is also available in "RAM" at the address `$80004400` because that's where the PiSCSI interface keeps the boot ROM, so you can technically just write it to a file on the Amiga from there.
* Download giggledisk from http://www.geit.de/eng_giggledisk.html or https://aminet.net/package/disk/misc/giggledisk to make MountLists for attached devices.
  Place the giggledisk binary in `C:` or something so that it's available in the search path.
* It might be a good idea to have fat95 installed on your Amiga, in case you want to use FAT32 images or other file systems that fat95 can handle: https://aminet.net/package/disk/misc/fat95

Now open a new CLI, and type something like:
`giggledisk device=pi-scsi.device unit=0 to=RAM:PI0`

This will create a MountList file called `PI0` on the RAM disk, which contains almost all the information needed to mount the drive and its partitions in Workbench.

You'll have to start up your favorite (or least hated) text editor and change the contents of the file a bit.

Above the `FileSystem` line, you'll notice a drive identifier. This line might say something along the lines of `DH0` or `PDH0`, etc, and it must be removed, otherwise the file can't be parsed.

The `FileSystem` line will usually be empty, so you have to fill this out yourself. For instance, you can set it to something like `L:FastFileSystem` to use the standard file system for the drive, or `L:fat95` in case the image is in a format that fat95 can handle.

Thus, an edited line would look something like `FileSystem       = L:FastFileSystem`

If the MountList has several partitions listed in it, it must be split up into separate files for all partitions to be mounted.

Once you've edited a MountList file, simply copy/move it to `SYS:Devs/DOSDrivers`, and the drive will be mounted automatically the next time you boot into Workbench.

If you don't want it to be mounted automatically, simply use the `Mount` command from CLI.

# A big word of caution

While the PiSCSI interface can be used to mount physical drives that are available as block device nodes on the Pi, you should not do this unless you are absolutely sure what you're doing.

Directly mounting a block device connected to the Pi may corrupt or destroy the data on the device, depending on what you or the file system drivers do with this disk on the Amiga side.

