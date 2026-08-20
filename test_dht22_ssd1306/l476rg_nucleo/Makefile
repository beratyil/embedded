# =============================================================================
#  Makefile  --  NUCLEO-L476RG  |  DHT22 -> SSD1306 OLED
# =============================================================================
#  Kullanim:
#     make                  -> derle (dht22_oled.elf / .hex / .bin uretir)
#     make flash            -> Nucleo'ya yukle
#     make monitor          -> UART ciktisini izle (ST-LINK sanal COM)
#     make probe            -> bagli ST-LINK'leri ve seri numaralarini listele
#     make size             -> bolum boyutlarini goster
#     make disasm           -> uretilen makine kodunu assembly olarak dok
#     make clean            -> uretilen dosyalari sil
#
#  AYARLAR (komut satirindan degistirilebilir):
#     make BAUD=230400 flash        -> UART hizi (koda ve monitor'a birlikte)
#     make ARALIK=5000 flash        -> olcum araligi ms (asgari 2000)
# =============================================================================

TARGET = dht22_oled

# ---- Ayarlanabilir parametreler --------------------------------------------
BAUD   ?= 115200
ARALIK ?= 2000

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
HEADERS  = font5x7.h

OBJS = $(C_SRC:.c=.o) $(ASM_SRC:.s=.o)

# ---- Ayar degisikligini yakalama --------------------------------------------
# TUZAK (kamera projesinde zaman kaybettirdi): make yalnizca DOSYA ZAMAN
# DAMGALARINA bakar. "make BAUD=230400" dediginizde main.c degismedigi icin
# main.o guncel sayilir ve -D bayraklari degismis olmasina ragmen YENIDEN
# DERLENMEZ -- eski ayarlarla uretilmis binary yuklenir.
#
# Cozum: bayraklari bir dosyada tutup .o'yu ona bagimli yapmak.
AYAR_DOSYA = .ayarlar

# ---- Cekirdek secenekleri ---------------------------------------------------
CPU = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

# ---- Derleyici bayraklari ---------------------------------------------------
TANIMLAR = -DUART_BAUD=$(BAUD)U -DOLCUM_ARALIGI_MS=$(ARALIK)U

CFLAGS  = $(CPU) -O2 -g3 -std=c11 \
          -ffreestanding -fno-builtin \
          -ffunction-sections -fdata-sections \
          -Wall -Wextra -Wshadow -Wundef \
          $(TANIMLAR)

ASFLAGS = $(CPU) -g3 -x assembler-with-cpp

# ---- Baglayici bayraklari ---------------------------------------------------
LDFLAGS = $(CPU) -T$(LDSCRIPT) -nostdlib -nostartfiles \
          -Wl,--gc-sections \
          -Wl,-Map=$(TARGET).map,--cref

# ---- Hedef kart -------------------------------------------------------------
# st-info --probe ciktisindan alindi (chipid 0x415 = STM32L47x/L48x).
# Discovery de ayni USB kimligiyle gorunur; SERIAL olmadan yanlis karta
# yazma riski var.
SERIAL  ?= 0669FF353637503457045439
STFLAGS  = $(if $(SERIAL),--serial $(SERIAL),)

# ---- UART izleme ------------------------------------------------------------
# /dev/ttyACM0 YAZMIYORUZ: numara takma sirasina gore degisir. by-id yolu
# ST-LINK seri numarasini icerdigi icin her zaman dogru karti gosterir.
PORT ?= /dev/serial/by-id/usb-STMicroelectronics_STM32_STLink_$(SERIAL)-if02

# =============================================================================
#  Kurallar
# =============================================================================
.PHONY: all clean flash probe size disasm monitor ayar_guncelle

all: $(TARGET).elf $(TARGET).hex $(TARGET).bin size

# Bayraklar degistiyse dosyayi guncelle (degismediyse dokunma -- yoksa her
# make cagrisi her seyi yeniden derlerdi).
ayar_guncelle:
	@echo '$(TANIMLAR)' | cmp -s - $(AYAR_DOSYA) 2>/dev/null || \
	    echo '$(TANIMLAR)' > $(AYAR_DOSYA)

$(AYAR_DOSYA): ayar_guncelle
	@:

%.o: %.c $(HEADERS) $(AYAR_DOSYA)
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
	@echo "  ayarlar: BAUD=$(BAUD)  ARALIK=$(ARALIK) ms"
	@echo ""

disasm: $(TARGET).elf
	$(OBJDUMP) -d -S $< > $(TARGET).lst
	@echo "-> $(TARGET).lst olusturuldu"

probe:
	st-info --probe

flash: $(TARGET).bin
	st-flash $(STFLAGS) --reset write $(TARGET).bin 0x8000000

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
	    exit 1; \
	fi
	@if command -v screen >/dev/null 2>&1; then \
	    screen $(PORT) $(BAUD); \
	elif command -v minicom >/dev/null 2>&1; then \
	    minicom -D $(PORT) -b $(BAUD); \
	else \
	    echo "screen veya minicom bulunamadi:  sudo apt install screen"; \
	    echo "Alternatif: stty -F $(PORT) $(BAUD) raw -echo && cat $(PORT)"; \
	fi

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).hex $(TARGET).bin \
	      $(TARGET).map $(TARGET).lst $(AYAR_DOSYA)
