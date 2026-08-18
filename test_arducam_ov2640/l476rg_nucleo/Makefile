# =============================================================================
#  Makefile  --  NUCLEO-L476RG  |  2 x ArduCAM Mini 2MP (OV2640)
# =============================================================================
#  Kullanim:
#     make                  -> derle (arducam.elf / .hex / .bin uretir)
#     make flash            -> Nucleo'ya yukle
#     make monitor          -> UART ciktisini izle (ST-LINK sanal COM)
#     make yakala           -> goruntuleri .jpg olarak kaydet (tools/jpeg_al.py)
#     make probe            -> bagli ST-LINK'leri ve seri numaralarini listele
#     make size             -> bolum boyutlarini goster
#     make disasm           -> uretilen makine kodunu assembly olarak dok
#     make clean            -> uretilen dosyalari sil
#
#  AYARLAR (komut satirindan degistirilebilir):
#     make COZUNURLUK=640 flash        -> 160 / 320 (varsayilan) / 640
#     make GORUNTU_AKISI=1 flash       -> ham JPEG'i UART'a dok
#     make BAUD=460800 flash           -> hizli akis icin
#
#  BAUD tek yerden yonetilir: hem koda -DUART_BAUD olarak gecer hem de
#  "make monitor"/"make yakala" ayni hizi kullanir. Ikisini elle esitlemeye
#  calismak, bu tur projelerde en sik yapilan ve en cok zaman kaybettiren
#  hatadir (ekrana anlamsiz karakterler basar, kimse baud'dan suphelenmez).
# =============================================================================

TARGET = arducam

# ---- Ayarlanabilir parametreler --------------------------------------------
COZUNURLUK    ?= 320
GORUNTU_AKISI ?= 0
BAUD          ?= 115200

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
HEADERS  = ov2640_regs.h

OBJS = $(C_SRC:.c=.o) $(ASM_SRC:.s=.o)

# ---- Ayar degisikligini yakalama ------------------------------------------
# TUZAK: make yalnizca DOSYA ZAMAN DAMGALARINA bakar. "make COZUNURLUK=640"
# dediginizde main.c degismedigi icin main.o guncel sayilir ve -D bayraklari
# degismis olmasina ragmen YENIDEN DERLENMEZ. Sonuc: eski ayarlarla uretilmis
# binary'yi yuklersiniz ve neden degismedigini anlamazsiniz.
#
# Cozum: bayraklari bir dosyada tutup .o'yu ona bagimli yapmak. Bayraklar
# degisince dosya guncellenir, make de yeniden derler.
AYAR_DOSYA = .ayarlar

# ---- Cekirdek secenekleri ---------------------------------------------------
CPU = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

# ---- Derleyici bayraklari ---------------------------------------------------
# -DUART_BAUD ile BAUD degeri koda tasinir; boylece BRR hesabi ile monitor
# hizi hicbir zaman ayrisamaz.
TANIMLAR = -DCOZUNURLUK=$(COZUNURLUK) \
           -DGORUNTU_AKISI=$(GORUNTU_AKISI) \
           -DUART_BAUD=$(BAUD)U

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
.PHONY: all clean flash probe size disasm monitor yakala

all: $(TARGET).elf $(TARGET).hex $(TARGET).bin size

# Bayraklar degistiyse dosyayi guncelle (degismediyse dokunma -- yoksa her
# make cagrisi her seyi yeniden derlerdi).
.PHONY: ayar_guncelle
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
	@echo "  ayarlar: COZUNURLUK=$(COZUNURLUK)  GORUNTU_AKISI=$(GORUNTU_AKISI)  BAUD=$(BAUD)"
	@echo ""

disasm: $(TARGET).elf
	$(OBJDUMP) -d -S $< > $(TARGET).lst
	@echo "-> $(TARGET).lst olusturuldu"

probe:
	st-info --probe

flash: $(TARGET).bin
	st-flash $(STFLAGS) --reset write $(TARGET).bin 0x8000000

# Ham JPEG akisini dosyaya cevir. GORUNTU_AKISI=1 ile derlenmis olmasi gerekir.
yakala:
	@if [ ! -e "$(PORT)" ]; then \
	    echo "HATA: port bulunamadi ($(PORT))"; exit 1; \
	fi
	python3 tools/jpeg_al.py --port $(PORT) --baud $(BAUD)

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
