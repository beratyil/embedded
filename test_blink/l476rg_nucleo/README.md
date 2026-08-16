# STM32L476RG — Register Seviyesi Blink

NUCLEO-L476RG kartı için **hiçbir kütüphane kullanmadan**
(HAL yok, LL yok, CMSIS yok, libc yok) yazılmış blink projesi.
Startup kodu, linker script ve tüm donanım erişimi elle yazılmıştır.

## Dosyalar

| Dosya | Görevi |
|---|---|
| `main.c` | GPIOA kurulumu, SysTick ile 1 ms zaman tabanı, LED döngüsü |
| `startup_stm32l476rg.s` | Vektör tablosu, `.data` kopyalama, `.bss` sıfırlama, `main` çağrısı |
| `stm32l476rg.ld` | Flash/SRAM hafıza haritası ve bölüm yerleşimi |
| `Makefile` | Derleme ve karta yükleme kuralları |

## Donanım

| Pin | LED | Renk |
|---|---|---|
| PA5 | LD2 | Yeşil |

Aktif-YÜKSEK: pin = 1 → LED yanar. (PA5 aynı zamanda Arduino başlığında D13.)

Saat kaynağı: reset sonrası varsayılan **MSI @ 4 MHz** (PLL kurulmadı).
`main.c` içindeki `SISTEM_SAATI_HZ` bu değere göre ayarlıdır.

## F407 sürümünden farkları

Aynı Cortex-M4 çekirdeği, ama çevre birimi adresleri farklı:

| Konu | STM32F407VG | STM32L476RG |
|---|---|---|
| GPIO veri yolu | AHB1 | **AHB2** |
| GPIO taban adresi | `0x40020000` | **`0x48000000`** |
| RCC taban adresi | `0x40023800` | **`0x40021000`** |
| GPIO saat register'ı | AHB1ENR (ofs `0x30`) | **AHB2ENR (ofs `0x4C`)** |
| Reset sonrası saat | HSI 16 MHz | **MSI 4 MHz** |
| Ana SRAM | 128 KB @ `0x20000000` | **96 KB @ `0x20000000`** |
| Yığın tepesi | `0x20020000` | **`0x20018000`** |
| Kullanıcı LED'i | PD12–PD15 (4 adet) | **PA5 (1 adet)** |

GPIO register'larının kendi iç ofsetleri (MODER `0x00`, ODR `0x14`,
BSRR `0x18`) iki ailede de aynıdır; değişen sadece portun taban adresidir.

## Kullanım

```bash
make          # derle -> blink.elf / blink.hex / blink.bin
make probe    # bağlı ST-LINK'leri ve seri numaralarını listele
make flash    # st-flash ile 0x08000000 adresine yükle
make disasm   # üretilen makine kodunu blink.lst olarak dök
make clean    # üretilen dosyaları sil
```

**İki kart birden bağlıysa:** Discovery ve Nucleo aynı USB kimliğiyle
(`0483:374b`) görünür, `st-flash` hangisine yazacağını bilemez.
`make probe` ile seri numarasını öğrenip hedefi açıkça belirtin:

```bash
make flash SERIAL=0672FF...
```

Alternatif yükleyici: `make openocd` (hedef dosyası `stm32l4x.cfg`,
F4'ün `stm32f4x.cfg` dosyası değil).

## Beklenen davranış

LD2 (yeşil) 500 ms yanık, 500 ms sönük — 1 Hz.

## Notlar

- **PA13/PA14 = SWDIO/SWCLK.** GPIOA_MODER'in reset değeri `0xABFFFFFF` ve
  bu iki pini alternatif fonksiyonda tutuyor. MODER'e doğrudan `=` ile
  yazarsanız kartı debugger'a kapatırsınız. Kod bu yüzden her yerde
  "oku-maskele-yaz" kullanıyor.
- STM32L4'te pinler reset'te **analog** (`11`) gelir, F4'teki gibi giriş
  (`00`) değil — güç tüketimini azaltmak için. Kod önce temizleyip sonra
  yazdığı için sonuç değişmiyor.
- Gecikmeler boş döngü yerine SysTick donanım sayacıyla üretildiği için
  optimizasyon seviyesi (`-O0` / `-O2`) süreleri değiştirmez.
