# =============================================================================
#  test_arducam_ov2640  --  ust seviye Makefile
# =============================================================================
#  Asil is alt klasordeki Makefile'da; bu dosya sadece onu cagirir ve
#  parametreleri gecirir.
#
#     make                        -> derle
#     make flash                  -> Nucleo'ya yukle
#     make monitor                -> UART teshis ciktisini izle
#     make yakala                 -> goruntuleri .jpg olarak kaydet
#     make probe                  -> bagli ST-LINK'leri listele
#     make clean                  -> temizle
#
#  Parametreler alt Makefile'a oldugu gibi aktarilir:
#     make COZUNURLUK=640 flash
#     make GORUNTU_AKISI=1 BAUD=460800 flash
#
#  Su an tek kart var (Nucleo). Discovery de eklenirse buraya ikinci bir
#  dizin degiskeni girer -- test_i2c/Makefile'daki kalibin aynisi.
# =============================================================================

NUCLEO_DIR = l476rg_nucleo

# BURAYA "export COZUNURLUK" YAZMAYIN.
#
# Komut satirindan verilen degiskenler (make COZUNURLUK=640) alt make'e
# MAKEFLAGS uzerinden ZATEN kendiliginden gecer; export gereksizdir.
# Dahasi zararlidir: degisken verilmediginde export onu BOS DEGER olarak
# ortama koyar, alt Makefile'daki "COZUNURLUK ?= 320" ise "zaten tanimli"
# gorup varsayilani atlar ve -DCOZUNURLUK= seklinde bos bir tanim uretilir.
# Sonuc, anlasilmasi zor bir yigin on islemci hatasidir.

.PHONY: all flash monitor yakala probe size disasm clean

all:
	@echo "==> NUCLEO-L476RG derleniyor (2 x ArduCAM OV2640)"
	@$(MAKE) --no-print-directory -C $(NUCLEO_DIR)

flash:
	@echo "==> NUCLEO-L476RG yukleniyor"
	@$(MAKE) --no-print-directory -C $(NUCLEO_DIR) flash

monitor:
	@$(MAKE) --no-print-directory -C $(NUCLEO_DIR) monitor

yakala:
	@$(MAKE) --no-print-directory -C $(NUCLEO_DIR) yakala

size:
	@$(MAKE) --no-print-directory -C $(NUCLEO_DIR) size

disasm:
	@$(MAKE) --no-print-directory -C $(NUCLEO_DIR) disasm

probe:
	st-info --probe

clean:
	@$(MAKE) --no-print-directory -C $(NUCLEO_DIR) clean
	@echo "Temizlendi."
