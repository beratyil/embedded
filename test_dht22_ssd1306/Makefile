# =============================================================================
#  test_dht22_ssd1306  --  ust seviye Makefile
# =============================================================================
#  Asil is alt klasordeki Makefile'da; bu dosya sadece onu cagirir.
#
#     make                   -> derle
#     make flash             -> Nucleo'ya yukle
#     make monitor           -> UART ciktisini izle
#     make probe             -> bagli ST-Link'leri listele
#     make clean             -> temizle
#
#     make BAUD=230400 flash -> UART hizi
#     make ARALIK=5000 flash -> olcum araligi ms (asgari 2000)
#
#  BURAYA "export DEGISKEN" YAZMAYIN. Komut satiri degiskenleri alt make'e
#  MAKEFLAGS ile zaten gecer; export ise degisken verilmediginde onu BOS
#  deger olarak ortama koyar ve alt Makefile'daki "?=" varsayilani atlanir.
#  (Kamera projesinde bu tuzaga dusuldu, bkz. CLAUDE.md Bolum 6.)
# =============================================================================

NUCLEO_DIR = l476rg_nucleo

.PHONY: all flash monitor probe size disasm clean

all:
	@echo "==> NUCLEO-L476RG derleniyor (DHT22 -> SSD1306)"
	@$(MAKE) --no-print-directory -C $(NUCLEO_DIR)

flash:
	@echo "==> NUCLEO-L476RG yukleniyor"
	@$(MAKE) --no-print-directory -C $(NUCLEO_DIR) flash

monitor:
	@$(MAKE) --no-print-directory -C $(NUCLEO_DIR) monitor

size:
	@$(MAKE) --no-print-directory -C $(NUCLEO_DIR) size

disasm:
	@$(MAKE) --no-print-directory -C $(NUCLEO_DIR) disasm

probe:
	st-info --probe

clean:
	@$(MAKE) --no-print-directory -C $(NUCLEO_DIR) clean
	@echo "Temizlendi."
