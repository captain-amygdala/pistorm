# PiAudio

PiAudio is a service that integrates with ALSA, the sound sub-system on the Raspberry Pi, and lets sound samples be played via Paula on the Amiga.

The *piaudio* program should run on the Amiga. It allocates two sounds channels, one left and one right. Using the following command the piaudio program can run in the background:
```
run piaudio >NIL:
```

The file *.asoundrc* should be stored in `/home/pi` on the RPi. Most programs that play audio on the RPi can then be used. One such program is mpg123, which is started as:
```
mpg123 -a amiga song.mp3
```
