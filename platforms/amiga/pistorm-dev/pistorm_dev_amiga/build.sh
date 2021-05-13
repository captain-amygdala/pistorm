m68k-amigaos-gcc pistorm_dev.c simple_interact.c -mregparm -m68020 -O2 -o PiSimple -Wno-unused-parameter -noixemul -DHAS_STDLIB -Wall
m68k-amigaos-gcc pistorm_dev.c copymems.c -m68020 -O2 -o CopyMems -Wno-unused-parameter -noixemul -DHAS_STDLIB -Wall -Wall -Wpedantic
