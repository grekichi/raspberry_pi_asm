ARMGNU ?= arm-none-eabi

XCPU = -mcpu=cortex-m0plus

AOPS = --warn --fatal-warnings $(XCPU)
COPS = -Wall -O2 -nostdlib -nostartfiles -ffreestanding $(XCPU)

.PHONY: all clean

all : sl_shift.uf2

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

sl_shift.uf2 : sl_shift.bin makeuf2
		./makeuf2 sl_shift.bin sl_shift.uf2

start.o : start.s
		$(ARMGNU)-as $(AOPS) start.s -o start.o

sl_shift.o : sl_shift.c
		$(ARMGNU)-gcc $(COPS) -mthumb -c sl_shift.c -o sl_shift.o

sl_shift.bin : rp2040.ld start.o sl_shift.o
		$(ARMGNU)-ld -T rp2040.ld start.o sl_shift.o -o sl_shift.elf
		$(ARMGNU)-objdump -D sl_shift.elf > sl_shift.list
		$(ARMGNU)-objcopy -O binary sl_shift.elf sl_shift.bin
