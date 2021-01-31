#m68k-amigaos-gcc piscsi-amiga.c -ramiga-dev -noixemul -fbaserel -O2 -o pi-scsi.device -ldebug -lamiga -m68020
m68k-amigaos-gcc -m68020 -O2 -o pi-scsi.device -ramiga-dev -noixemul -fbaserel piscsi-amiga.c -ldebug -lamiga
m68k-amigaos-gcc -m68020 -O2 -o scsi.device -ramiga-dev -noixemul -fbaserel piscsi-amiga.c -ldebug -lamiga -D_FSCSIDEV
m68k-amigaos-gcc -m68020 -O2 -o 2nd.scsi.device -ramiga-dev -noixemul -fbaserel piscsi-amiga.c -ldebug -lamiga -D_FSCSI2ND
vasmm68k_mot.exe -m68020 -Fhunk -I$VBCC/NDK39/include/include_i bootrom.s -o a.out
#m68k-amigaos-as -m68020 bootrom.s &&
m68k-amigaos-objcopy --strip-all ./a.out ./bootrom
rm ./a.out
