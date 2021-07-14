# PiGFX/PiStorm RTG driver for Amiga

A reasonably complete RTG driver for the PiStorm, which should be compatible with any* version of Picasso96/P96.

The driver has support and acceleration for all common P96 features except for screen dragging. Screen dragging support is not really planned, as it would require uploading two full screen size textures every single frame. And no one actually uses it anyway.

(RTG video output is over the Raspberry Pi HDMI.)

**Note**: The RTG has been rewritten to use raylib instead of SDL. Precompiled raylib libraries are included, there is no need to install any additional dependencies when building the emulator.  
For raylib to work in the console on a Pi 3, the OpenGL driver must **not** be enabled. If you have the line `dtoverlay=vc4-kms-v3d` enabled for your system setup in `/boot/config.txt`, either comment it out or remove the line.  
If you are for some reason wanting to test the RTG on a Raspberry Pi 4, you are on your own. Not only can't I test it, you must also install a distro with some flavor of X window manager for it to perform well at all.

# Instructions

Simply mount the PiStorm HDF using PiSCSI and run the RTG driver installer from there. Just follow the instructions, as you are required to read some things and click the things it asks you to click.

**Note:** Because the RTG driver installer is now available on the PiStorm HDF included with the GitHub repo, there is typically no need to do any of the manual setup listed below. It is merely left in for the sake of legacy information being available.


# Legacy installation instructions

**If you are still reading here, and you did not notice the Note or the instructions above, please go back and read it. There is no need to do this any longer.**  
Some familiarity with P96 and AmigaOS is currently required, as you have to edit a Monitor file and create a `Picasso96Settings` file for the available resolutions.

Setup for PiGFX/PiStorm RTG is not entirely straightforward, unlike PiSCSI some files need to be transferred to the Amiga side. Here are the steps required to get PiStorm RTG up and running:

* Install P96/Picasso96 on the Amiga side. Aminet Picasso96 requires at least Kickstart 2.05 (2.0?) and P96 2.4+ requires at least Kickstart 3.1 and a 68020 processor.
* Select any graphics driver you want from the list of available ones in the installer, you will need to edit the tooltypes for the Monitor file it installs to load the PiGFX driver instead, something like the Picasso IV or CyberVision 64/3D is recommended for the other tooltypes to match up.
* Grab `pigfx020.card` from the `rtg_driver_amiga` directory and copy it to the drawer `LIBS:Picasso96` on the Amiga.
* Edit the tooltypes for Monitor file you installed to load `pigfx020.card` instead, this will initialize the PiGFX driver on boot. You can also move the Monitor file out of the `DEVS:Monitors` driver and double click it from elsewhere to load the driver manually if so desired.
* Once you've rebooted and loaded the PiGFX driver, launch `Picasso96Mode` from the `Prefs` drawer on your system volume, select `PiStorm RTG` from the list of boards and add the resolutions you want/need to the list.
* Open `ScreenMode` in the `Prefs` drawer on your system volume and select the video mode you want, or launch an RTG game/application.
* Enjoy! (Maybe... if it works...)
