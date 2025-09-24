ARMGNU ?= arm-none-eabi

XCPU = -mcpu=cortex-m0

AOPS = --warn --fatal-warnings $(XCPU)
COPS = -Wall -O2 -nostdlib -nostartfiles -ffreestanding $(XCPU)

.PHONY: all clean

all : push_blink.uf2

clean:
		rm -f *.bin
		rm -r *.o
		rm -f *.elf
		rm -f *.list
		rm -f *.uf2
		rm -f makeuf2

makeuf2 : makeuf2.c crcpico.h
		gcc -O2 makeuf2.c -o makeuf2

push_blink.uf2 : push_blink.bin makeuf2
		./makeuf2 push_blink.bin push_blink.uf2

start.o : start.s
		$(ARMGNU)-as $(AOPS) start.s -o start.o

push_blink.o : push_blink.c
		$(ARMGNU)-gcc $(COPS) -mthumb -c push_blink.c -o push_blink.o

push_blink.bin : memmap.ld start.o push_blink.o
		$(ARMGNU)-ld -T memmap.ld start.o push_blink.o -o push_blink.elf
		$(ARMGNU)-objdump -D push_blink.elf > push_blink.list
		$(ARMGNU)-objcopy -O binary push_blink.elf push_blink.bin

