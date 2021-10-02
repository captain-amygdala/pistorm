vasmm68k_mot -quiet -phxass -Fhunk -m68020 -o PI-AHI prefsfile.a -I$VBCC/NDK39/include/include_i
m68k-amigaos-gcc pi-ahi.c -O2 -o pi-ahi.audio -Wall -Wextra -Wno-unused-parameter -nostartfiles -m68020
