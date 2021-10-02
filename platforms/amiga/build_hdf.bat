REM Requires xdftool from amitools (https://github.com/cnvogelg/amitools/)
del pistorm.hdf
rdbtool pistorm.hdf create size=2.5Mi + init rdb_cyls=2
rdbtool pistorm.hdf add size=100% name=DH99 dostype=ffs
rdbtool pistorm.hdf fsadd dos1.bin fs=DOS1
xdftool pistorm.hdf open part=DH99 + format PiStorm ffs
xdftool pistorm.hdf open part=DH99 + write Disk.info
xdftool pistorm.hdf open part=DH99 + write pistorm-dev/pistorm_dev_amiga/PiSimple
xdftool pistorm.hdf open part=DH99 + write pistorm-dev/pistorm_dev_amiga/PiStorm
xdftool pistorm.hdf open part=DH99 + write pistorm-dev/pistorm_dev_amiga/PiStorm.info
xdftool pistorm.hdf open part=DH99 + write pistorm-dev/pistorm_dev_amiga/libs13
xdftool pistorm.hdf open part=DH99 + write pistorm-dev/pistorm_dev_amiga/libs20
xdftool pistorm.hdf open part=DH99 + write pistorm-dev/pistorm_dev_amiga/libs13.info
xdftool pistorm.hdf open part=DH99 + write pistorm-dev/pistorm_dev_amiga/libs20.info
xdftool pistorm.hdf open part=DH99 + write pistorm-dev/pistorm_dev_amiga/CopyMems
xdftool pistorm.hdf open part=DH99 + write ../../a314/software-amiga a314
xdftool pistorm.hdf open part=DH99 + makedir net
xdftool pistorm.hdf open part=DH99 + write net/net_driver_amiga/pi-net.device net/pi-net.device
xdftool pistorm.hdf open part=DH99 + makedir scsi
xdftool pistorm.hdf open part=DH99 + write piscsi/device_driver_amiga/pi-scsi.device scsi/pi-scsi.device
xdftool pistorm.hdf open part=DH99 + makedir rtg
xdftool pistorm.hdf open part=DH99 + write "rtg/PiGFX Install" rtg
xdftool pistorm.hdf open part=DH99 + write "rtg/PiGFX Install.info" rtg
xdftool pistorm.hdf open part=DH99 + makedir "rtg/PiGFX Install/Files"
xdftool pistorm.hdf open part=DH99 + write rtg/rtg_driver_amiga/pigfx020.card "rtg/PiGFX Install/Files/pigfx020.card"
xdftool pistorm.hdf open part=DH99 + write rtg/rtg_driver_amiga/PiGFX.info "rtg/PiGFX Install/Files/PiGFX.info"

