EXENAME          = emulator

MAINFILES        = emulator.c \
	memory_mapped.c \
	config_file/config_file.c \
	config_file/rominfo.c \
	input/input.c \
	gpio/ps_protocol.c \
	platforms/platforms.c \
	platforms/amiga/amiga-autoconf.c \
	platforms/amiga/amiga-platform.c \
	platforms/amiga/amiga-registers.c \
	platforms/amiga/amiga-interrupts.c \
	platforms/mac68k/mac68k-platform.c \
	platforms/dummy/dummy-platform.c \
	platforms/dummy/dummy-registers.c \
	platforms/amiga/Gayle.c \
	platforms/amiga/hunk-reloc.c \
	platforms/amiga/cdtv-dmac.c \
	platforms/amiga/rtg/rtg.c \
	platforms/amiga/rtg/rtg-output-raylib.c \
	platforms/amiga/rtg/rtg-gfx.c \
	platforms/amiga/piscsi/piscsi.c \
	platforms/amiga/pistorm-dev/pistorm-dev.c \
	platforms/shared/rtc.c \
	platforms/shared/common.c

MUSASHIFILES     = m68kcpu.c m68kdasm.c softfloat/softfloat.c softfloat/softfloat_fpsp.c
MUSASHIGENCFILES = m68kops.c
MUSASHIGENHFILES = m68kops.h
MUSASHIGENERATOR = m68kmake

EXE =
EXEPATH = ./

.CFILES   = $(MAINFILES) $(MUSASHIFILES) $(MUSASHIGENCFILES)
.OFILES   = $(.CFILES:%.c=%.o) a314/a314.o

CC        = gcc
CXX       = g++
WARNINGS  = -Wall -Wextra -pedantic -Wno-unused-parameter

	LFLAGS    = $(WARNINGS) -L/usr/local/lib -L/opt/vc/lib -L./raylib_drm -lraylib -lGLESv2 -lEGL -lgbm -ldrm -ldl -lstdc++ -lvcos -lvchiq_arm -lvchostif -lasound
	CFLAGS    = $(WARNINGS) -I. -I./raylib_drm -I/opt/vc/include/ -march=armv8-a -mtune=cortex-a53 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -O3 -lstdc++ $(ACFLAGS)

TARGET = $(EXENAME)$(EXE)

DELETEFILES = $(MUSASHIGENCFILES) $(MUSASHIGENHFILES) $(.OFILES) $(.OFILES:%.o=%.d) $(TARGET) $(MUSASHIGENERATOR)$(EXE)


all: $(MUSASHIGENCFILES) $(MUSASHIGENHFILES) $(TARGET)

clean:
	rm -f $(DELETEFILES)

$(TARGET):  $(MUSAHIGENCFILES:%.c=%.o) $(.CFILES:%.c=%.o) a314/a314.o
	$(CC) -o $@ $^ -O3 -pthread $(LFLAGS) -lm -lstdc++

a314/a314.o: a314/a314.cc a314/a314.h
	$(CXX) -MMD -MP -c -o a314/a314.o -O3 a314/a314.cc -march=armv8-a -mtune=cortex-a53 -O3 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -I. -I..

$(MUSASHIGENCFILES) $(MUSASHIGENHFILES): $(MUSASHIGENERATOR)$(EXE)
	$(EXEPATH)$(MUSASHIGENERATOR)$(EXE)

$(MUSASHIGENERATOR)$(EXE):  $(MUSASHIGENERATOR).c
	$(CC) -o  $(MUSASHIGENERATOR)$(EXE)  $(MUSASHIGENERATOR).c

-include $(.CFILES:%.c=%.d) $(MUSASHIGENCFILES:%.c=%.d) a314/a314.d $(MUSASHIGENERATOR).d
