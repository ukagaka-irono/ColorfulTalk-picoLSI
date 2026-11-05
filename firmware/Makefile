# ColorfulTalk pico LSI ファームウェア（avr-gcc）
#   make            → pico-lsi.hex
#   make flash      → avrdude で書き込む（PROGRAMMER と PORT を指定）
# SPDX-License-Identifier: MIT
MCU      = atmega328p
F_CPU    = 16000000UL
CC       = avr-gcc
OBJCOPY  = avr-objcopy
SIZE     = avr-size
CFLAGS   = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -std=c99 -Wall -Wextra -Inano
SRC      = main.c nano/ctn.c nano/ctn_tables.c
PROGRAMMER ?= usbasp
PORT       ?= usb

all: pico-lsi.hex

pico-lsi.elf: $(SRC) nano/colorful_talk_nano.h nano/ctn_internal.h
	$(CC) $(CFLAGS) -o $@ $(SRC)
	$(SIZE) $@

pico-lsi.hex: pico-lsi.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

# ヒューズ: 外部水晶 16MHz、BOD 2.7V
fuses:
	avrdude -c $(PROGRAMMER) -P $(PORT) -p m328p -U lfuse:w:0xFF:m -U hfuse:w:0xD9:m -U efuse:w:0xFD:m

flash: pico-lsi.hex
	avrdude -c $(PROGRAMMER) -P $(PORT) -p m328p -U flash:w:pico-lsi.hex:i

clean:
	rm -f pico-lsi.elf pico-lsi.hex

.PHONY: all fuses flash clean
