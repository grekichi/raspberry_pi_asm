ARMGNU ?= arm-none-eabi

XCPU = -mcpu=cortex-m0plus

AOPS = --warn --fatal-warnings $(XCPU)
COPS = -Wall -O2 -nostdlib -nostartfiles -ffreestanding $(XCPU)

.PHONY: all clean

all : segled.uf2

clean:
		rm -f *.bin
		rm -r *.o
		rm -f *.elf
		rm -f *.list
		rm -f *.uf2
		rm -f makeuf2

# ------------------------------------------------------

makeuf2 : makeuf2.c crcpico.h
		gcc -O2 makeuf2.c -o makeuf2

segled.uf2 : segled.bin makeuf2
		./makeuf2 segled.bin segled.uf2

start.o : start.s
		$(ARMGNU)-as $(AOPS) start.s -o start.o

segled.o : segled.c
		$(ARMGNU)-gcc $(COPS) -mthumb -c segled.c -o segled.o

segled.bin : rp2040.ld start.o segled.o
		$(ARMGNU)-ld -T rp2040.ld start.o segled.o -o segled.elf
		$(ARMGNU)-objdump -D segled.elf > segled.list
		$(ARMGNU)-objcopy -O binary segled.elf segled.bin

