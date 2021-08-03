#!/bin/bash

# Version 0.01 - 2021-06-03 - Daniel "MrD" Andersson
#
#
# Make installation of PiStorm automated, can enable extra services for convinience, updates the system
# Expected to be started as root, will otherwise quit

if [ "$EUID" -ne 0 ]
  then echo "This installer must be started as root, please run \"sudo $0\""
  exit
fi


#
# Begin with updating the system
#

echo "Starting the operating system updates, this may take a while to download updates and install"
apt-get update
apt-get upgrade -y

echo "Operating system update completed, installing packages needed for PiStorm and other system utils"

apt-get -y install git libsdl2-dev openocd ntp ntpdate

echo "Enabling and starting NTP (Time Sync) client"
systemctl enable ntp
systemctl start ntp

echo "Downloading and compiling PiStorm source code"

cd /usr/local/src
git clone https://github.com/captain-amygdala/pistorm.git
cd pistorm
make

chmod a+x nprog.sh nprog_240.sh
chmod a+x flash.sh

echo "Installing PiStorm to autostart"
cp linux_install/pistorm.service /etc/systemd/system
systemctl daemon-reload
systemctl enable pistorm.service

echo "FPGA Bitstream update"

./flash.sh
[ $? -eq 0 ] || echo "CPLD flashing seems to have failed, please check information above and your PiStorm setup"

echo "Setting up PiStorm config and data files in /srv/pistorm directory"

mkdir /srv/pistorm
chmod g+w /srv/pistorm
chgrp pi /srv/pistorm
cp default.cfg /srv/pistorm/pistorm.cfg
chmod g+w /srv/pistorm//pistorm.cfg
chgrp pi /srv/pistorm/pistorm.cfg
sed -i 's/PI0.hdf/\/srv\/pistorm\/PI0.hdf/g' /srv/pistorm/pistorm.cfg
sed -i 's/PI1.hdf/\/srv\/pistorm\/PI1.hdf/g' /srv/pistorm/pistorm.cfg
sed -i 's/file=/file=\/srv\/pistorm\//g' /srv/pistorm/pistorm.cfg

echo "Starting PiStorm, your retro 68k computer should become alive"
systemctl start pistorm.service

echo -n "Add optional software for easy management - SMB fileshare (Y/N)"
read answer
case $answer in 
  [Yy]* ) apt install samba
     export DEBIAN_FRONTEND=noninteractive
     apt -y install samba
      echo "[pistorm]
          comment = PiStorm share
          browseable = yes
          path = /srv/pistorm
          guest ok = no
          writable = yes
          read only = no
          " >> /etc/samba/smb.conf
      echo "Set the password for the Samba user pi"
      smbpasswd -a pi
      systemctl enable smbd
      service smbd start
      echo "Samba started, browse to \\\\<YOURIP>\\pistorm use username pi with your choosen password to access the PiStorm config file"
    ;;
  [Nn]* ) echo "No installation of optional software"   
     ;;
  *) echo "Invalid option";;
esac


echo "Installation completed, you can now go ahead and restart."