# PiGFX/PiStorm RTG driver for Amiga

A reasonably complete RTG driver for the PiStorm, compatible with P96 **2.4** and above.

While it's not intended to be incompatible with the free Picasso96 available on AmiNet, there appears to be some issues with resolution switching among other things with it at the time of writing this.

The driver has support and acceleration for all common P96 features except for screen dragging hardware mouse cursor. Hardware mouse cursor is planned, but screen dragging... not at present, as it would require uploading two full screen size textures every single frame.

Some familiarity with P96 and AmigaOS is currently required, as you have to edit a Monitor file and create a `Picasso96Settings` file for the available resolutions.

(RTG video output is over the Raspberry Pi HDMI.)

# Instructions

Setup for PiGFX/PiStorm RTG is not entirely straightforward, unlike PiSCSI some files need to be transferred to the Amiga side. Here are the steps required to get PiStorm RTG up and running:

* Install P96/Picasso96 on the Amiga side. Aminet Picasso96 requires at least Kickstart 2.05 (2.0?) and P96 2.4+ requires at least Kickstart 3.1 and a 68020 processor.
* Select any graphics driver you want from the list of available ones in the installer, you will need to edit the tooltypes for the Monitor file it installs to load the PiGFX driver instead, something like the Picasso IV or CyberVision 64/3D is recommended for the other tooltypes to match up.
* Grab `pigfx020.card` from the `rtg_driver_amiga` directory and copy it to the drawer `LIBS:Picasso96` on the Amiga.
* Edit the tooltypes for Monitor file you installed to load `pigfx020.card` instead, this will initialize the PiGFX driver on boot. You can also move the Monitor file out of the `DEVS:Monitors` driver and double click it from elsewhere to load the driver manually if so desired.
* Once you've rebooted and loaded the PiGFX driver, launch `Picasso96Mode` from the `Prefs` drawer on your system volume, select `PiStorm RTG` from the list of boards and add the resolutions you want/need to the list.
* Open `ScreenMode` in the `Prefs` drawer on your system volume and select the video mode you want, or launch an RTG game/application.
* Enjoy! (Maybe... if it works...)
