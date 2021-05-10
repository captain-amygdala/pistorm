# Requires patool (in Debian / Ubuntu repos) and lha from https://github.com/jca02266/lha
tar -cvf pistorm.tar --transform 's,^,PiStorm/,' PiSimple PiStorm PiStorm.info libs13 libs13.info libs20 libs20.info
patool repack pistorm.tar pistorm.lha
rm pistorm.tar
