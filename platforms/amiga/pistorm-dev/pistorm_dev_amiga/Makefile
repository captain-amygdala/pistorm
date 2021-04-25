CC=vc +kick13
CFLAGS=-c99 -DNDEBUG -lamiga -lauto

all: PiSimple PiStorm

PiSimple: simple_interact.c pistorm_dev.c
	$(CC) $(CFLAGS) $^ -o PiSimple

PiStorm: gui_interact.c pistorm_dev.c
	$(CC) $(CFLAGS) $^ -o PiStorm


