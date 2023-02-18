# PiStorm

![logo](media/pistorm_banner.jpg)

# Join us on Discord or on Libera Chat IRC #PiStorm

* There's a Discord server dedicated to PiStorm discussion and development, which you can join through this handy invite link: https://discord.com/invite/j6rPtzxaNW
* There are also IRC channels on the Libera IRC network (irc.libera.chat):
  * `#PiStorm`, bridged with the `#general` channel on Discord, `#PiStorm-hardware` which is bridged with `#hardware`, `#PiStorm-firmware`, bridged with `#firmware`,
  * `#PiStorm-Amiga` bridged with `#software-amiga`, `#PiStorm-pi`, brigded with `#software-pi` and `#PiStorm-chat`, bridged with `#ot-and-chitchat`.

# Project information

This branch is for the PiStorm32-lite Accelerator only, it wont work on regualar PiStorm
Compatible Raspberry Boards are : Pi4,CM4,Pi3,PiZero2

# Amiga-specific functionality

Since much of the initial work and testing for the PiStorm was done on Amiga computers, a number of extended features are available when the PiStorm is paired with for instance an Amiga 500:
* Kickstart ROM mapping: 1.3, 2.0, 3.1, anything you might own and have dumped in a byteswapped format. Extended ROM mapping as well for instance with the CDTV extended BIOS.
  * An A1200 3.1+ Kickstart ROM is currently recommended, as this one has the most dynamic automatic configuration on boot.
* Fast RAM: Z2, Z3 and CPU local Fast can be mapped for high performance memory available to the CPU only on the PiStorm side of things.
* Virtual SCSI: PiSCSI, a high performance virtual SCSI interface for mapping raw RDB disk images or physical storage devices connected to the Pi for use on the Amiga.
* RTG: PiGFX, a virtual RTG board with almost all P96-supported functionality supported and accelerated.
* Some other things: Most likely I forgot something while writing this, but someone will probably tell me about it.

# Simple quickstart

* Download Raspberry Pi OS from https://www.raspberrypi.com/software/operating-systems/#raspberry-pi-os-32-bit, the Lite version is recommended as the windowing system of the Full version adds a lot of extra system load which may impact performance. **Note: You must use the 32bit version of Pi OS.** 
* Write the Image to a SD Card. 8GB is plenty for the PiStorm binaries and required libraries, but if you wish to use large hard drive images or sometthing with it, go with a bigger card.
* Install the PiStorm adapter in place of the orignal CPU in the system, for instance an Amiga 500.
  Make sure the PiStorm sits flush and correct in the socket.
  When installed in an Amiga 500, The correct orientation on the PiStorm is with the USB port facing toward you and the HDMI port facing to the right.
  If the PiStorm does not stay in place properly (popping out of the CPU socket) then bend the pins of the PiStorm very very very slightly outwards.
  Double check that all is properly in place and no pins are bent.
* Connect an HDMI Display and a USB keyboard to the PiStorm. Using a USB Hub is possible, an externally powered hub is recommended.
  Connect the Amiga to the PSU and PAL Monitor
* Insert the SD into the Raspberry Pi, Power on the Amiga now. You should see a Rainbow colored screen on the HDMI Monitor and the PiStorm booting.

* When the boot process is finished (on the first run it reboots automatically after resizing the filesystems to your SD).
* Setup the Pi base system in the dialog (Keyboard,Username&Password, don't forget to write down the login details....)
* Log in and run `sudo raspi-config`
* Set up your Wi-Fi credentials
* Enable SSH at boot time
* Exit raspi-config

You can now reach the PiStorm over SSH, check your router web/settings page to find the IP of the PiStorm, or run `ifconfig` locally on the PiStorm from the console.

Now the final steps to get things up and running, all of this is done from a command prompt (terminal) either locally on the PiStorm or over ssh:
* `sudo apt-get update`
* `sudo apt-get install git libdrm-dev libegl1-mesa-dev libgles2-mesa-dev libgbm-dev`
* `git clone https://github.com/captain-amygdala/pistorm.git --branch pistorm32-lite`
* `cd pistorm`
* `make`

**Important note:** If you are using **Raspberry Pi OS "Bullseye"**, the main graphics backend for the OS has changed from dispmanx to DRM, and you need to follow these steps instead of just running `make`:
* First run `sudo apt-get install libdrm-dev libegl1-mesa-dev libgles2-mesa-dev libgbm-dev` to install the DRM OpenGL/ES libraries, which are for some reason not included with the distro by default. These are necessary to link the graphics output library (raylib).
* Then finally, run `make` for the emulator to compile successfully.

Next up, follow the steps for installing the FPGA bitstream update below. (Scroll down.)

If you are running the PiStorm in an Amiga computer, you can start the emulator with a basic default Amiga config by typing `sudo ./emulator --config amiga.cfg`.  
In addition, the emulator will attempt to load a file called `default.cfg` if no config file is specified on the command line, so if you wish for the emulator to start up with for instance the basic default Amiga config, you can copy `amiga.cfg` to `default.cfg`.  
**Important note:** Try not to edit the sample config files such as `amiga.cfg`, always save them under a different name, for instance one directory level below the `pistorm` directory.  
One way to do this would be to copy for instance `amiga.cfg` like this: `cp ./amiga.cfg ../amiga.cfg` and then running the emulator using `sudo ./emulator --config ../amiga.cfg`. This way, you will never have any problems using `git pull` to update your PiStorm repo to the latest commit.

To exit the emulator you can press `Ctrl+C` (on the keyboard or over SSH) or press `Q` on the keyboard connected to the Raspberry Pi.

For Amiga, there is currently no Gayle or IDE controller emulation available, but PiSCSI can now autoboot RDB/RDSK hard drive images (and physical drives), with Kickstart 2.0 and up. Check out the readme in `platforms/amiga/piscsi` for more detailed information.

# FPGA bitstream update :

The FPGA gets automatically loaded on start of the emulator. No further programming/flashing is needed
