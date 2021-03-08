EXENAME          = emulator

MAINFILES        = emulator.c \
	memory_mapped.c \
	config_file/config_file.c \
	input/input.c \
	gpio/ps_protocol.c \
	platforms/platforms.c \
	platforms/amiga/amiga-autoconf.c \
	platforms/amiga/amiga-platform.c \
	platforms/amiga/amiga-registers.c \
	platforms/dummy/dummy-platform.c \
	platforms/dummy/dummy-registers.c \
	platforms/amiga/Gayle.c \
	platforms/amiga/hunk-reloc.c \
	platforms/amiga/gayle-ide/ide.c \
	platforms/amiga/cdtv-dmac.c \
	platforms/amiga/rtg/rtg.c \
	platforms/amiga/rtg/rtg-output.c \
	platforms/amiga/rtg/rtg-gfx.c \
	platforms/amiga/piscsi/piscsi.c \
	platforms/amiga/net/pi-net.c \
	platforms/shared/rtc.c

MUSASHIFILES     = m68kcpu.c m68kdasm.c softfloat/softfloat.c
MUSASHIGENCFILES = m68kops.c
MUSASHIGENHFILES = m68kops.h
MUSASHIGENERATOR = m68kmake

# EXE = .exe
# EXEPATH = .\\
EXE =
EXEPATH = ./

.CFILES   = $(MAINFILES) $(MUSASHIFILES) $(MUSASHIGENCFILES)
.OFILES   = $(.CFILES:%.c=%.o)

CC        = gcc
WARNINGS  = -Wall -Wextra -pedantic
CFLAGS    = $(WARNINGS) -I. -march=armv8-a -mfloat-abi=hard -mfpu=neon-fp-armv8 -O3 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
LFLAGS    = $(WARNINGS) `sdl2-config --libs`

TARGET = $(EXENAME)$(EXE)

DELETEFILES = $(MUSASHIGENCFILES) $(MUSASHIGENHFILES) $(.OFILES) $(TARGET) $(MUSASHIGENERATOR)$(EXE)


all: $(TARGET)

clean:
	rm -f $(DELETEFILES)


$(TARGET): $(MUSASHIGENHFILES) $(.OFILES) Makefile
	$(CC) -o $@ $(.OFILES) -O3 -pthread $(LFLAGS) -lm

$(MUSASHIGENCFILES) $(MUSASHIGENHFILES): $(MUSASHIGENERATOR)$(EXE)
	$(EXEPATH)$(MUSASHIGENERATOR)$(EXE)

$(MUSASHIGENERATOR)$(EXE):  $(MUSASHIGENERATOR).c
	$(CC) -o  $(MUSASHIGENERATOR)$(EXE)  $(MUSASHIGENERATOR).c
