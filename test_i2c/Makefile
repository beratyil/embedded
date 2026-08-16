# =============================================================================
#  test_i2c  --  ust seviye Makefile
# =============================================================================
#  Iki karti tek yerden yonetmek icin. Asil is alt klasorlerdeki Makefile'larda;
#  bu dosya sadece onlari cagirir.
#
#     make                 -> her iki projeyi de derle
#     make flash           -> ikisini de yukle (once slave, sonra master)
#     make flash-slave     -> yalnizca Nucleo-L476RG (slave)
#     make flash-master    -> yalnizca F407 Discovery (master)
#     make monitor         -> Nucleo'nun UART ciktisini izle
#     make probe           -> bagli ST-LINK'leri listele
#     make clean           -> her ikisini de temizle
#
#  YUKLEME SIRASI NEDEN ONEMLI?
#     Once slave yuklenirse, master ilk turunda karsisinda hazir bir cihaz
#     bulur ve yesil LED hemen yanar. Ters sirada birkac saniye kirmizi
#     gorursunuz -- zarari yok, sadece kafa karistirir.
# =============================================================================

MASTER_DIR = f407vg_discovery
SLAVE_DIR  = l476rg_nucleo

.PHONY: all master slave flash flash-master flash-slave monitor probe clean

all: slave master

master:
	@echo "==> MASTER derleniyor (STM32F407VG Discovery)"
	@$(MAKE) --no-print-directory -C $(MASTER_DIR)

slave:
	@echo "==> SLAVE derleniyor (NUCLEO-L476RG)"
	@$(MAKE) --no-print-directory -C $(SLAVE_DIR)

# Once slave, sonra master (yukaridaki nedenle).
flash: flash-slave flash-master

flash-slave:
	@echo "==> SLAVE yukleniyor (NUCLEO-L476RG)"
	@$(MAKE) --no-print-directory -C $(SLAVE_DIR) flash

flash-master:
	@echo "==> MASTER yukleniyor (STM32F407VG Discovery)"
	@$(MAKE) --no-print-directory -C $(MASTER_DIR) flash

monitor:
	@$(MAKE) --no-print-directory -C $(SLAVE_DIR) monitor

probe:
	st-info --probe

clean:
	@$(MAKE) --no-print-directory -C $(MASTER_DIR) clean
	@$(MAKE) --no-print-directory -C $(SLAVE_DIR) clean
	@echo "Temizlendi."
