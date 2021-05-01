# PiStorm on Pi Boot

## Bootup script

For your convience a startup script for systemd in Linux is located in this directory. This script will start PiStorm *before* the network connections have started.

### Installation

To start PiStorm on automatically on boot, copy `pistorm.service` into /etc/systemd/system/ in your Pi's filesystem. Then run:

```bash
sudo systemctl enable pistorm
```
### Customisation

There are some things you may want to change in the systemd script. These instructions will help with that.

#### Custom config

If you wish to boot using a custom configuration file change `ExecStart` to:

```ini
ExecStart=/home/pi/pistorm/emulator --config-file myconfig.cfg
```

Where `myconfig.cfg` is your config file in the `pistorm` directory. If your config file is somewhere else you will need to put the full path for it there.

**NOTE:** do not put an '=' between `--config-file` and the file name, this will not work.

#### Different location

You may want to run your PiStorm from a different location than `/home/pi/pistorm` this is fine, but it is important that the files that come with the emulator stay together in the same directory structure. For example, if you moved pistorm to `/opt/pistorm` you will need to change the following two lines:

```ini
ExecStart=/opt/pistorm/emulator
WorkingDirectory=/opt/pistorm
```

It is important both lines are changed otherwise this can cause issues, particularly crashes.

#### Startup order

If, for example, your PiStorm configuration depends on something like Samba running for PiScsi you will want to change the startup order so the PiStorm waits for that to run. In the `[Unit]` second add something like the following example:

```ini
After=network.target samba.service
```

Please see the systemd documentation for more informatino on this.

### Applying changes

If you have made any changes to the `pistorm.service` file *after* it has been copied over you will need to reload the systemd configuration for the changes to be seen. This can be done with:

```bash
sudo systemctl daemon-reload
```

## Faster boot

The Pi does several things that aren't needed for PiStorm at bootup, the following steps will accelerate the boot time for you.

### config.txt

Edit `/boot/config.txt` and add the following lines:

```ini
# Disable the rainbow splash screen
disable_splash=1

# Set the bootloader delay to 0 seconds. The default is 1s if not specified.
boot_delay=0

# Disable Bluetooth
dtoverlay=disable-bt
```

By default there is a 1 second boot delay and initialising bluetooth takes a couple of seconds.

### cmdline.txt

Edit the `/boot/cmdline.txt` and add `quiet` near the end, as an example (do **NOT** copy/paste this line):

```
console=serial0,115200 console=tty1 root=PARTUUID=5f1219ae-02 rootfstype=ext4 elevator=deadline fsck.repair=yes quiet rootwait
```

This shaves a little time off spitting out boot logs to the screen.

### fstab

Disable `/boot` remount on boot, this will mean you will need to do `sudo mount /boot` when you want to change files in that partition, but it shaves off half a second from boot. To do this add `noauto` to the options second for the `/boot` line, for example (do **NOT** copy/paste this line):

```
PARTUUID=5f1219ae-01  /boot           vfat    defaults,noauto          0       2
```

### Disable swap

If we need swap something went wrong. This shaves about another second off the boot time:

```bash
sudo systemctl disable dphys-swapfile.service
```
