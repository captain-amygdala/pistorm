# a314eth.device - SANA-II driver for A314

**NOTE: This readme is the default A314 Ethernet driver readme, and some of these things don't apply to the PiStorm A314 emulation adapted binary.**
This SANA-II driver works by copying Ethernet frames back and forth between the Amiga and a virtual ethernet interface (tap0) on the Raspberry Pi. The Pi will, when configured properly, do network address translation (NAT) and route packets from the Amiga to the Internet.

## Configuring the Raspberry Pi

- Install pytun: `sudo pip3 install python-pytun`.
- Copy `ethernet.py` to `/opt/a314/ethernet.py`.
- Update `/etc/opt/a314/a314d.conf` with a line that starts `ethernet.py` on demand.
  - In order for `a314d` to pick up the changes in `a314d.conf` you'll have to restart `a314d`, either by `sudo systemctl restart a314d` or by rebooting the Pi.
- Copy `pi-config/tap0` to `/etc/network/interfaces.d/tap0`. This file creates a tap device with ip address 192.168.2.1 when the Raspberry Pi is booted up.
- Add the lines in `pi-config/rc.local` to the bottom of `/etc/rc.local` just before `exit 0`. This create iptables rules that forwards packets from the `tap0` interface to the `wlan0` interface.
  - Please note that if the Pi is connected using wired ethernet then `wlan0` should be changed to `eth0`.

The first four steps are performed by `sudo make install`. The last step you have to do manually.

## Configuring the Amiga

This has only been tested with the Roadshow TCP/IP stack, and these instructions are written for Roadshow. The instructions won't describe how to install Roadshow.

- Build the `a314eth.device` binary, for example using the `rpi_docker_build.sh` script.
- Copy `bin/a314eth.device` to `DEVS:`.
- Copy `amiga-config/A314Eth` to `DEVS:NetInterfaces/A314Eth`.
- Copy `amiga-config/routes` to `DEVS:Internet/routes`.
- Copy `amiga-config/name_resolution` to `DEVS:Internet/name_resolution`.
  - You should change the nameserver to a DNS server that works on your network.
  - Note that there may be settings in the above two files (`routes` and `name_resolution`) that you wish to keep, so look through the changes you are about to make first.

Reboot the Amiga and with some luck you should be able to access the Internet from your Amiga.

## Important note:

After `ethernet.py` starts it waits for 15 seconds before it starts forwarding Ethernet frames. Without this waiting there is something that doesn't work properly (I don't know why this is). So when the Amiga boots for the first time after power off you'll have to wait up to 15 seconds before the Amiga can reach the Internet.
