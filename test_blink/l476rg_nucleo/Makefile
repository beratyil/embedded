# =============================================================================
#  Makefile  --  STM32L476RG (NUCLEO-L476RG) saf register seviyesi blink
# =============================================================================
#  Kullanim:
#     make          -> derle (blink.elf / .hex / .bin uretir)
#     make flash    -> st-flash ile karta yukle
#     make probe    -> bagli ST-LINK'leri ve seri numaralarini listele
#     make openocd  -> OpenOCD ile karta yukle (alternatif)
#     make size     -> bolum boyutlarini goster
#     make clean    -> uretilen dosyalari sil
#
#  IKI KART BIRDEN BAGLIYSA:
#     Discovery ve Nucleo ayni USB kimligiyle (0483:374b) gorunur; st-flash
#     hangisine yazacagini bilemez. Once "make probe" ile seri numarasini
#     ogrenin, sonra:
#         make flash SERIAL=0672FF...
# =============================================================================

TARGET = blink

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
ASM_SRC  = startup_stm32l476rg.s
LDSCRIPT = stm32l476rg.ld

OBJS = $(C_SRC:.c=.o) $(ASM_SRC:.s=.o)

# ---- Cekirdek secenekleri ---------------------------------------------------
#   -mcpu=cortex-m4   : hedef cekirdek (L476 de Cortex-M4)
#   -mthumb           : Cortex-M yalnizca Thumb-2 komut setini calistirir
#   -mfloat-abi=soft  : FPU'yu kullanma (blink icin gerek yok, kod daha kucuk)
CPU = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

# ---- Derleyici bayraklari ---------------------------------------------------
#   -O2                  : optimizasyon (SysTick kullandigimiz icin gecikme
#                          sureleri optimizasyondan etkilenmez)
#   -g3                  : debug bilgisi (gdb icin)
#   -ffreestanding       : "isletim sistemi ve standart kutuphane yok" modu
#   -fno-builtin         : derleyici memcpy/printf gibi cagrilar uretmesin
#   -ffunction-sections  : her fonksiyonu ayri bolume koy...
#   -fdata-sections      : ...her veriyi de. Boylece --gc-sections
#                          kullanilmayanlari atabilir.
CFLAGS  = $(CPU) -O2 -g3 -std=c11 \
          -ffreestanding -fno-builtin \
          -ffunction-sections -fdata-sections \
          -Wall -Wextra -Wshadow -Wundef

ASFLAGS = $(CPU) -g3 -x assembler-with-cpp

# ---- Baglayici (linker) bayraklari ------------------------------------------
#   -T$(LDSCRIPT)   : kendi linker script'imizi kullan
#   -nostdlib       : hicbir standart kutuphaneyi bagLAMA (libc, libgcc, crt0)
#   -nostartfiles   : GCC'nin hazir startup kodunu kullanma (bizimki var)
#   --gc-sections   : kullanilmayan bolumleri cikartip boyutu kucult
#   -Map            : detayli hafiza haritasi dosyasi uret
LDFLAGS = $(CPU) -T$(LDSCRIPT) -nostdlib -nostartfiles \
          -Wl,--gc-sections \
          -Wl,-Map=$(TARGET).map,--cref

# ---- Hedef kart secimi ------------------------------------------------------
# SERIAL bos birakilirsa st-flash buldugu ilk ST-LINK'i kullanir. Iki kart
# birden bagliyken bu YANLIS KARTA yazabilir; o durumda SERIAL verin.
SERIAL  ?=
STFLAGS  = $(if $(SERIAL),--serial $(SERIAL),)

# =============================================================================
#  Kurallar
# =============================================================================
.PHONY: all clean flash probe openocd size disasm

all: $(TARGET).elf $(TARGET).hex $(TARGET).bin size

# .c -> .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# .s -> .o
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

# Bolum boyutlari:
#   text = Flash'taki kod + sabitler
#   data = SRAM'de yasayan, ilk degeri Flash'ta duran degiskenler
#   bss  = SRAM'de sifirlanan degiskenler (Flash'ta yer kaplamaz)
size: $(TARGET).elf
	@echo ""
	@$(SIZE) --format=berkeley $<
	@echo ""

# Uretilen makine kodunu assembly olarak incelemek icin
disasm: $(TARGET).elf
	$(OBJDUMP) -d -S $< > $(TARGET).lst
	@echo "-> $(TARGET).lst olusturuldu"

# ---- Bagli kartlari listele -------------------------------------------------
# Ciktidaki "serial:" satiri, asagidaki SERIAL degiskenine verilecek degerdir.
probe:
	st-info --probe

# ---- Karta yukleme: st-flash (stlink-tools paketi) --------------------------
# Flash'in baslangic adresi 0x8000000'dir; ham .bin oraya yazilir.
flash: $(TARGET).bin
	st-flash $(STFLAGS) --reset write $(TARGET).bin 0x8000000

# ---- Karta yukleme: OpenOCD (alternatif) ------------------------------------
# DIKKAT: hedef dosyasi stm32l4x.cfg -- F4 icin kullanilan stm32f4x.cfg DEGIL.
openocd: $(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
	        -c "program $(TARGET).elf verify reset exit"

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).hex $(TARGET).bin \
	      $(TARGET).map $(TARGET).lst
