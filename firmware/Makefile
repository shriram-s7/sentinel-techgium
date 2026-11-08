CC   = arm-none-eabi-gcc
CF   = -mcpu=cortex-m0plus -mthumb -Os -ffunction-sections -fdata-sections -Wall -nostartfiles -Wl,--gc-sections -Tg030.ld

all: base.elf diag.elf size

base.elf: main.c
	$(CC) $(CF) main.c -o $@ -lgcc

diag.elf: main.c sentinel.c sentinel.h
	$(CC) $(CF) -DUSE_SENTINEL main.c sentinel.c -o $@ -lgcc

size: base.elf diag.elf
	arm-none-eabi-size base.elf diag.elf

clean:
	rm -f *.elf *.o *.su
