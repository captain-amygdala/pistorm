# Requires xdftool from amitools (https://github.com/cnvogelg/amitools/)
xdftool part.hdf create size=2Mi + format PiStorm ffs
xdftool part.hdf boot install
xdftool part.hdf write Disk.info
xdftool part.hdf write pistorm-dev/pistorm_dev_amiga/PiSimple
xdftool part.hdf write pistorm-dev/pistorm_dev_amiga/PiStorm
xdftool part.hdf write pistorm-dev/pistorm_dev_amiga/PiStorm.info
xdftool part.hdf write pistorm-dev/pistorm_dev_amiga/libs13
xdftool part.hdf write pistorm-dev/pistorm_dev_amiga/libs20
xdftool part.hdf write pistorm-dev/pistorm_dev_amiga/libs13.info
xdftool part.hdf write pistorm-dev/pistorm_dev_amiga/libs20.info
xdftool part.hdf write pistorm-dev/pistorm_dev_amiga/CopyMems
xdftool part.hdf write ../../a314/software-amiga a314
xdftool part.hdf makedir net
xdftool part.hdf write net/net_driver_amiga/pi-net.device net/pi-net.device
xdftool part.hdf makedir scsi
xdftool part.hdf write piscsi/device_driver_amiga/pi-scsi.device scsi/pi-scsi.device
xdftool part.hdf makedir rtg
xdftool part.hdf write rtg/rtg_driver_amiga/pigfx020.card rtg/pigfx020.card
xdftool part.hdf write rtg/rtg_driver_amiga/pigfx030.card rtg/pigfx030.card
rm pistorm.hdf
rdbtool pistorm.hdf create size=2.5Mi + init
rdbtool pistorm.hdf addimg part.hdf name=DH99
rm part.hdf
