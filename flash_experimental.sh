#!/bin/bash
set -o pipefail
if pgrep -x "emulator" > /dev/null
then
    echo "PiStorm emulator is running, please stop it first"
    exit 1
fi
if ! command -v openocd &> /dev/null
then
    echo "openocd is not installed, please run \"sudo apt install openocd\""
    exit 1
fi
echo -ne "Detecting CPLD... "
version=`sudo openocd -f nprog/detect.cfg 2>/dev/null | awk 'FNR == 3 { print $4 }'`
if [ $? -ne 0 ]
then
	echo "Error detecting CPLD."
	exit 1
fi
case $version in
	"0x020a10dd")
		echo "EPM240 detected!"
		./nprog_240_experimental.sh
		;;
	"0x020a20dd")
		echo "EPM570 detected!"
		./nprog_experimental.sh
		;;
	*)
		echo "Could not detect CPLD"
		exit 1
		;;
esac

