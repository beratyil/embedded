# =============================================================================
#  Makefile  --  STM32F407VG DISCOVERY  |  I2C MASTER
# =============================================================================
#  Kullanim:
#     make          -> derle (i2c_master.elf / .hex / .bin uretir)
#     make flash    -> Discovery'ye yukle
#     make probe    -> bagli ST-LINK'leri ve seri numaralarini listele
#     make openocd  -> OpenOCD ile yukle (alternatif)
#     make size     -> bolum boyutlarini goster
#     make disasm   -> uretilen makine kodunu assembly olarak dok
#     make clean    -> uretilen dosyalari sil
#
#  IKI KART BIRDEN BAGLI OLDUGU ICIN:
#     Discovery ve Nucleo ayni USB kimligiyle (0483:374b) gorunur. st-flash
#     hangisine yazacagini seri numarasindan ayirt eder; asagida bu kartin
#     seri numarasi HAZIR YAZILIDIR, bu yuzden "make flash" dogru karta gider.
#     Kart degistirirseniz "make probe" ile yeni numarayi ogrenip
#     SERIAL degiskenini guncelleyin veya komut satirindan verin:
#         make flash SERIAL=066FFF...
# =============================================================================

TARGET = i2c_master

# ---- Arac zinciri -----------------------------------------------------------
# arm-none-eabi = "ARM mimarisi, uretici yok, isletim sistemi yok (bare metal),
#                  gomulu ABI". Kartta Linux olmadigi icin bu on eki kullaniyoruz.
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump
SIZE    = $(PREFIX)size

# ---- Kaynak dosyalar --------------------------------------------------------
C_SRC    = main.c
ASM_SRC  = startup_stm32f407vg.s
LDSCRIPT = stm32f407vg.ld

OBJS = $(C_SRC:.c=.o) $(ASM_SRC:.s=.o)

# ---- Cekirdek secenekleri ---------------------------------------------------
#   -mcpu=cortex-m4   : hedef cekirdek
#   -mthumb           : Cortex-M yalnizca Thumb-2 komut setini calistirir
#   -mfloat-abi=soft  : FPU'yu kullanma (I2C icin gerek yok; ayrica FPU'yu
#                       kullanmak istersek once CPACR ile acmak gerekirdi)
CPU = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

# ---- Derleyici bayraklari ---------------------------------------------------
#   -O2                  : optimizasyon. Zamanlamalar SysTick ve donanim
#                          bayraklarina dayandigi icin guvenlidir.
#   -g3                  : debug bilgisi (gdb icin)
#   -ffreestanding       : "isletim sistemi ve standart kutuphane yok" modu
#   -fno-builtin         : derleyici memcpy/printf gibi cagrilar uretmesin
#   -ffunction-sections  : her fonksiyonu ayri bolume koy...
#   -fdata-sections      : ...her veriyi de. Boylece --gc-sections
#                          kullanilmayanlari atabilir.
#   -Wall -Wextra        : tum uyarilar acik
CFLAGS  = $(CPU) -O2 -g3 -std=c11 \
          -ffreestanding -fno-builtin \
          -ffunction-sections -fdata-sections \
          -Wall -Wextra -Wshadow -Wundef

ASFLAGS = $(CPU) -g3 -x assembler-with-cpp

# ---- Baglayici (linker) bayraklari ------------------------------------------
LDFLAGS = $(CPU) -T$(LDSCRIPT) -nostdlib -nostartfiles \
          -Wl,--gc-sections \
          -Wl,-Map=$(TARGET).map,--cref

# ---- Hedef kart -------------------------------------------------------------
# Bu satirdaki numara SIZIN Discovery kartinizin ST-LINK seri numarasidir
# (st-info --probe ciktisindan alindi, chipid 0x413 = STM32F4x5).
SERIAL  ?= 066FFF565257867767154920
STFLAGS  = $(if $(SERIAL),--serial $(SERIAL),)

# =============================================================================
#  Kurallar
# =============================================================================
.PHONY: all clean flash probe openocd size disasm

all: $(TARGET).elf $(TARGET).hex $(TARGET).bin size

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(ASFLAGS) -c $< -o $@

# Nesne dosyalari -> ELF (sembolleri ve debug bilgisini iceren tam cikti)
$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

# ELF -> HEX (Intel HEX, STM32CubeProgrammer icin)
$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

# ELF -> BIN (ham ikili; st-flash bunu ister)
$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary -S $< $@

# text = Flash'taki kod + sabitler
# data = SRAM'de yasayan, ilk degeri Flash'ta duran degiskenler
# bss  = SRAM'de sifirlanan degiskenler (Flash'ta yer kaplamaz)
size: $(TARGET).elf
	@echo ""
	@$(SIZE) --format=berkeley $<
	@echo ""

disasm: $(TARGET).elf
	$(OBJDUMP) -d -S $< > $(TARGET).lst
	@echo "-> $(TARGET).lst olusturuldu"

probe:
	st-info --probe

# Flash'in baslangic adresi 0x8000000'dir; ham .bin oraya yazilir.
flash: $(TARGET).bin
	st-flash $(STFLAGS) --reset write $(TARGET).bin 0x8000000

openocd: $(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
	        -c "program $(TARGET).elf verify reset exit"

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).hex $(TARGET).bin \
	      $(TARGET).map $(TARGET).lst
