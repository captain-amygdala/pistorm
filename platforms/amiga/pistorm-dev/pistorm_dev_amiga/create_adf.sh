# Requires xdftool from amitools (https://github.com/cnvogelg/amitools/)
xdftool pistorm.adf create
xdftool pistorm.adf format "PiStorm"
xdftool pistorm.adf write PiSimple
xdftool pistorm.adf write PiStorm
xdftool pistorm.adf write PiStorm.info
xdftool pistorm.adf write libs13
xdftool pistorm.adf write libs20
xdftool pistorm.adf write libs13.info
xdftool pistorm.adf write libs20.info
xdftool pistorm.adf write CopyMems
