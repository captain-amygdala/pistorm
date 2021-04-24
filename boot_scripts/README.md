# PiStorm on Pi Boot

## Bootup script

To start PiStorm on automatically on boot, copy this file into /etc/systemd/system/ in your Pi's filesystem. Then run:

```bash
sudo systemctl enable pistorm
```

This script will start PiStorm *before* the network connections have started.

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
