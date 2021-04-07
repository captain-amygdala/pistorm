CFLAGS += -w -g -O3 -std=c99 -march=armv8-a -mfloat-abi=hard -mfpu=neon-fp-armv8
LDFLAGS += -lm

GENCPU = gencpu.o
BUILD68K = build68k.o
TABLE68K = table68k.o
CPUEMU = cpuemu_31.o \
	cpuemu_32.o \
	cpustbl.o

OBJ = cpuemu_common.o \
	cpummu030.o \
	cpummu.o \
	fpp.o \
	newcpu.o \
	hatari-glue.o \
	readcpu.o \
	custom.o \
	m68000.o
#	memory.o \
.PHONY: clean

cpu.a: $(CPUEMU) $(TABLE68K) $(OBJ)
	@echo " [ AR ] cpu.a"
	@$(AR) -cr cpu.a $(OBJ) $(TABLE68K) $(CPUEMU) 

clean:
	echo " [ RM ] $(OBJ) $(BUILD68K) $(GENCPU) $(TABLE68K) $(CPUEMU) table68k.c build68k gencpu cpuemu_31.c cpuemu_32.c cpustbl.c cpustbl.h cpu.a generated.out"
	@rm -f $(OBJ) $(BUILD68K) $(GENCPU) $(TABLE68K) $(CPUEMU) table68k.c build68k gencpu cpuemu_31.c cpuemu_32.c cpustbl.c cpustbl.h cpu.a generated.out

build68k: $(BUILD68K)
	@echo " [ LD ]	$@"
	@$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

gencpu: $(GENCPU) $(TABLE68K) readcpu.o
	@echo " [ LD ]	$@"
	@$(CC) $(CFLAGS) -o $@ $< readcpu.o $(TABLE68K) $(LDFLAGS)

generated.out: gencpu
	@echo " [GEN ]	cpu specifics"
	@./gencpu
	@touch $@

table68k.c: build68k
	@echo " [GEN ]	$@"
	@./build68k < table68k > table68k.c

cpuemu_31.o: cpuemu_31.c
	@echo " [ CC ]	$*.c"
	@$(CC) $(CFLAGS) -c -o $@ $*.c

cpuemu_32.o: cpuemu_32.c
	@echo " [ CC ]	$*.c"
	@$(CC) $(CFLAGS) -c -o $@ $*.c

cpustbl.o: cpustbl.c
	@echo " [ CC ]	$*.c"
	@$(CC) $(CFLAGS) -c -o $@ $*.c

cpustbl.c: generated.out
cpuemu_32.c: generated.out
cpuemu_31.c: generated.out

%.o: %.c
	@echo " [ CC ]	$<"
	@$(CC) $(CFLAGS) -c -o $@ $< 
