# Pi-AHI, AHI compatible "sound card" for the PiStorm.

**Please note:** Currently, this virtual AHI device does not work quite properly:  
* There is no interrupt to operate the virtual AHI device at precisely 50Hz, which is required by AHI itself, so it will not work well in for instance 60Hz RTG modes.
* It may crash or lock up when closing certain applications because they never call FreeAudio for some reason.
* The resampling is very crude, and will not sound amazing.

# Instructions

To enable Pi-AHI, you have to follow these steps:
* Add the line `setvar pi-ahi` to the config file you're currently using.
* On your Amiga, install AHI (version 4.18, 6.0 may work, but is not suitable).
* Copy the file `pi-ahi.audio` to `DEVS:AHI` and `PI-AHI` to `DEVS:AudioModes`.
* Open the AHI Prefs application and set up PiStorm AHI like you would any other AHI device.

By default, audio output is done over the 3.5mm audio output jack. If you wish to output audio over HDMI instead, you have to configure your Raspberry Pi to do this and add something like `plughw:0,0` to the end of the `setvar pi-ahi` line, for instance like this: `setvar pi-ahi plughw:0,0`.
