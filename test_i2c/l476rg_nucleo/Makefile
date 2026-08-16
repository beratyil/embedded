# =============================================================================
#  Makefile  --  NUCLEO-L476RG  |  I2C SLAVE
# =============================================================================
#  Kullanim:
#     make          -> derle (i2c_slave.elf / .hex / .bin uretir)
#     make flash    -> Nucleo'ya yukle
#     make monitor  -> UART ciktisini izle (115200 8N1, ST-LINK sanal COM)
#     make probe    -> bagli ST-LINK'leri ve seri numaralarini listele
#     make openocd  -> OpenOCD ile yukle (alternatif)
#     make size     -> bolum boyutlarini goster
#     make disasm   -> uretilen makine kodunu assembly olarak dok
#     make clean    -> uretilen dosyalari sil
#
#  IKI KART BIRDEN BAGLI OLDUGU ICIN:
#     Discovery ve Nucleo ayni USB kimligiyle (0483:374b) gorunur. Asagidaki
#     SERIAL degeri BU Nucleo kartinin seri numarasidir, dolayisiyla
#     "make flash" yanlislikla Discovery'ye yazmaz.
#     Kart degistirirseniz: make probe -> yeni numarayi SERIAL'e yazin.
# =============================================================================

TARGET = i2c_slave

# ---- Arac zinciri -----------------------------------------------------------
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump
SIZE    = $(PREFIX)size

# ---- Kaynak dosyalar --------------------------------------------------------
C_SRC    = main.c
ASM_SRC  = startup_stm32l476rg.s
LDSCRIPT = stm32l476rg.ld

OBJS = $(C_SRC:.c=.o) $(ASM_SRC:.s=.o)

# ---- Cekirdek secenekleri ---------------------------------------------------
# L476 de Cortex-M4'tur; derleyici acisindan F407'den farki yoktur.
# Fark cevre birimlerinin adreslerinde ve davranisindadir (bkz. main.c).
CPU = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

# ---- Derleyici bayraklari ---------------------------------------------------
CFLAGS  = $(CPU) -O2 -g3 -std=c11 \
          -ffreestanding -fno-builtin \
          -ffunction-sections -fdata-sections \
          -Wall -Wextra -Wshadow -Wundef

ASFLAGS = $(CPU) -g3 -x assembler-with-cpp

# ---- Baglayici bayraklari ---------------------------------------------------
LDFLAGS = $(CPU) -T$(LDSCRIPT) -nostdlib -nostartfiles \
          -Wl,--gc-sections \
          -Wl,-Map=$(TARGET).map,--cref

# ---- Hedef kart -------------------------------------------------------------
# st-info --probe ciktisindan alindi (chipid 0x415 = STM32L47x/L48x).
SERIAL  ?= 0669FF353637503457045439
STFLAGS  = $(if $(SERIAL),--serial $(SERIAL),)

# ---- UART izleme ------------------------------------------------------------
# Nucleo'nun ST-LINK'i bir sanal COM portu sunar; PA2 (USART2_TX) oraya baglidir.
#
# /dev/ttyACM0 YAZMIYORUZ. O numara takma sirasina gore degisir: kartlardan
# birini cikarip geri takarsaniz numaralar yer degistirir ve yanlis kartin
# portunu dinlemeye baslarsiniz. /dev/serial/by-id/ altindaki isim ise ST-LINK
# seri numarasini icerdigi icin HER ZAMAN ayni karti gosterir.
#
# Yine de elle vermek isterseniz:  make monitor PORT=/dev/ttyACM1
PORT ?= /dev/serial/by-id/usb-STMicroelectronics_STM32_STLink_$(SERIAL)-if02
BAUD ?= 115200

# =============================================================================
#  Kurallar
# =============================================================================
.PHONY: all clean flash probe openocd size disasm monitor

all: $(TARGET).elf $(TARGET).hex $(TARGET).bin size

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(ASFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary -S $< $@

size: $(TARGET).elf
	@echo ""
	@$(SIZE) --format=berkeley $<
	@echo ""

disasm: $(TARGET).elf
	$(OBJDUMP) -d -S $< > $(TARGET).lst
	@echo "-> $(TARGET).lst olusturuldu"

probe:
	st-info --probe

flash: $(TARGET).bin
	st-flash $(STFLAGS) --reset write $(TARGET).bin 0x8000000

openocd: $(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
	        -c "program $(TARGET).elf verify reset exit"

# UART ciktisini izle. screen kuruluysa onu, degilse minicom'u dener.
# screen'den cikis: Ctrl-A sonra K, sonra y
monitor:
	@echo "Port: $(PORT) @ $(BAUD)  --  cikmak icin Ctrl-A K y"
	@if [ ! -e "$(PORT)" ]; then \
	    echo ""; \
	    echo "HATA: port bulunamadi. Nucleo takili mi? Kontrol: make probe"; \
	    exit 1; \
	fi
	@if [ ! -r "$(PORT)" ] || [ ! -w "$(PORT)" ]; then \
	    echo ""; \
	    echo "HATA: '$(PORT)' uzerinde izniniz yok (screen sessizce kapanir)."; \
	    echo "Seri portlar 'dialout' grubuna aittir. Kalici cozum:"; \
	    echo "    sudo usermod -aG dialout \$$USER"; \
	    echo "    (sonra oturumu kapatip acin; ya da: newgrp dialout)"; \
	    echo "Hemen denemek icin (salt okuma yeterli, kart sadece gonderiyor):"; \
	    echo "    sudo stty -F $(PORT) $(BAUD) raw -echo && sudo cat $(PORT)"; \
	    exit 1; \
	fi
	@if command -v screen >/dev/null 2>&1; then \
	    screen $(PORT) $(BAUD); \
	elif command -v minicom >/dev/null 2>&1; then \
	    minicom -D $(PORT) -b $(BAUD); \
	else \
	    echo "screen veya minicom bulunamadi. Kurmak icin:"; \
	    echo "    sudo apt install screen"; \
	    echo "Alternatif (kurulum gerektirmez):"; \
	    echo "    stty -F $(PORT) $(BAUD) raw -echo && cat $(PORT)"; \
	fi

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).hex $(TARGET).bin \
	      $(TARGET).map $(TARGET).lst
