# PiStorm Linux Installation instructions

This method is semi automated, it will require minimal Linux knowledge and shall be a quick way to start enjoying your PiStorm.

The method is tested with the 2021-05-07 release of Raspios Buster Lite available at https://www.raspberrypi.org/software/operating-systems/

Download the Raspios Buster Lite image and extract the ZIP into a folder, producing an .img file which needs to be written to SD card, recommended is to use BalenaEtcher (https://www.balena.io/etcher/). 

After writing the image to SD card, keep the SD card in the computer and open the drive and follow the next steps to enable WiFi. 

Create the file wpa_supplicant.conf which enables WiFi, adding your WiFi settings to our file

```
# Countrycode as per Alpha 2 Code https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2#Officially_assigned_code_elements

country=SE
update_config=1
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev

# SSID is the name of your WiFi network, case sensitive
# PSK is the "password" to your WiFI. 

network={
    ssid="testing"
    psk="testingPassword"
    key_mgmt=WPA-PSK
}
```

Create an empty file named "ssh" which enables remote access via SSH 

Put the SD card into the Pi and boot it up, takes a minute or two for the first boot, after that login with SSH to your Pi.
```
Username: pi
Password: raspberry
```

For security reason consider changing the default password with command "passwd" and choose a new strong password.

```
pi@raspberrypi:~ $ passwd
Changing password for pi.
Current password:
New password:
Retype new password:
passwd: password updated successfully
```

Next download and run the installation script from GitHub with the following command

```
sudo bash <(curl -s https://raw.githubusercontent.com/captain-amygdala/pistorm/main/linux_install/pistorm_installer.sh)
```
After this if no error has occured PiStorm is fully installed and ready to be used, you can restart the Pi to ensure that the latest versions of software is loaded.

Configuration file is located in /srv/pistorm, where you also can put all your ROMs and HDD images. 

PiStorm is installed as a service that is auto started, to restart pistorm login and run

```
sudo service pistorm restart
```

As per current version the redirect to the pistorm.log does not fully seems to work, if you need to see what PiStorm outputs login via SSH and use

```
sudo service pistorm stop
sudo /usr/local/src/pistorm/emulator --config-file /srv/pistorm/pistorm.cfg
```


# PiStorm Linux update instructions

To perform an update of the system use the provided update script, the update script expects you to have installed PiStorm using the installation script. The update will overwrite any local changes made to /usr/local/src/pistrom but keeps all your local configuration changes in /srv/pistorm folder. 

```
sudo /usr/local/src/pistorm/linux_install/update.sh
```

After update is completed you can restart the Pi to ensure that the lastes versions of software is loaded. 


Installer includes an option to install Samba to enable that you can map the share and edit the configuration file and/or copy HDD images files/roms to your PiStorm. When editing the pistorm.cfg this way ensure to save it using Unix/Linux format, most modern editors support this.



