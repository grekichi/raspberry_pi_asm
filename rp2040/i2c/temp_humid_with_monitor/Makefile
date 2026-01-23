NAME = temp_humid_m

ARMGNU ?= arm-none-eabi

XCPU = -mcpu=cortex-m0plus

AOPS = --warn --fatal-warnings $(XCPU)
COPS = -Wall -O2 -nostartfiles -ffreestanding $(XCPU)
LOPS = -nostdlib

LDLIBS = /usr/share/gcc-arm-none-eabi-10.3-2021.10/lib/gcc/arm-none-eabi/10.3.1/libgcc.a
LIBS = /usr/share/gcc-arm-none-eabi-10.3-2021.10/arm-none-eabi/lib/libc.a

.PHONY: all clean

all : $(NAME).uf2

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

$(NAME).uf2 : $(NAME).bin makeuf2
		./makeuf2 $(NAME).bin $(NAME).uf2

start.o : start.s
		$(ARMGNU)-as $(AOPS) start.s -o start.o

$(NAME).o : $(NAME).c
		$(ARMGNU)-gcc $(COPS) -mthumb -c $(NAME).c -o $(NAME).o

# uart.o : uart.c
# 		$(ARMGNU)-gcc $(COPS) uart.c -o uart.o

$(NAME).bin : rp2040.ld start.o $(NAME).o
		$(ARMGNU)-ld -T rp2040.ld start.o $(NAME).o -o $(NAME).elf $(LDLIBS)
#$(LIBS)
		$(ARMGNU)-objdump -D $(NAME).elf > $(NAME).list
		$(ARMGNU)-objcopy -O binary $(NAME).elf $(NAME).bin
