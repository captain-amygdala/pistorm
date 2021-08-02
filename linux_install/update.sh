#!/bin/bash

# Version 0.01 - 2021-06-03 - Daniel "MrD" Andersson
#
#

#
# This script updates both OS and PiStorm (if needed), needs to run as root
# Any local changes to files in /usr/local/src/pistorm will be removed using this script. 
# 


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
echo "Operating system update completed, starting update of PiStorm"

#
# Check if PiStorm needs to be updated
#

cd /usr/local/src/pistorm

pichanged=0
git remote update && git status -uno | grep -q 'Your branch is behind' && pichanged=1
if [ $pichanged = 1 ]; then
    echo "Updating source code from Git"
    git reset --hard HEAD
    git pull
    echo "Compliation of Pistorm"
    make clean
    make
    echo "Running CPLD flashing"
    ./flash.sh
    [ $? -eq 0 ] || echo "CPLD flashing seems to have failed, please check information above and your PiStorm setup"
    systemctl restart pistorm
else
    echo "PiStorm is already up to date"
fi

echo "Update completed, you can now go ahead and restart."