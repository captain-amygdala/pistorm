# pistorm

Simple quickstart



* Download Raspberry OS from https://www.raspberrypi.org/software/operating-systems/ , the Lite version is sufficent
* Write the Image to a SD Card (8GB sized is plenty, for larger HDD Images pick a bigger one)
* Install the pistorm adapter inplace of the orignal CPU into the Amiga500. Make sure the pistorm sits flush and correct in the Amiga.
  The correct orientation on the pistorm is the USB port facing towards you and the HDMI port is facing to the right

  If the pistorm should not stay in place properly (jumping out of the CPU socket) then bend the pins of the pistorm very very very slightly
  outwards. Double check that all is properly in place and no pins are bend.

* Connect a HDMI Display and a USB Keyboard to the pistorm. Using a USB Hub is possible, connect the Amiga to the PSU and PAL Monitor
* Insert the SD into the Raspberry, Power on the Amiga now. You should see a Rainbow colored screen on the HDMI Monitor and the pistrom booting


* As soon as the boot process is finished (on the first run it reboots automatically after resizing the filesystems to your SD) you should be greeted
  with the login prompt
* Log in as user : pi , password : raspberry (The keyboard is set to US Layout on first boot!)
* run : sudo raspi-config
* Setup your preferences like keyboard layout,language etc.
* Setup your Wifi credentials
* Enable SSH at boot time
* Exit raspi-config 
  
  You can now reach the pistorm over SSH , look into you router webpage to find the IP of the pistorm or run : ifconfig 

run : sudo apt-get install git

run : cd pistorm

run : make


to start the pistorm emulator 

run : ./run.sh 

to exit emulation
ctrl+c

If you want to use the minimal hdd image you need to unpack it :
run : tar xvfz hd0.tar.gz 

**Currently the emulation is a bit buggy on IDE Interrupts, so it takes ages to boot from the emulated HDD. This will be fixed soon :) 




