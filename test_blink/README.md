# Bare-Metal STM32 Blink Projeleri

İki farklı STM32 kartı için, **hiçbir kütüphane kullanmadan** (HAL yok, LL yok,
CMSIS yok, libc yok) yazılmış blink projeleri. Her projede startup kodu,
linker script ve tüm donanım erişimi elle, doğrudan register adreslerine
yazılarak kodlanmıştır.

## Klasör yapısı

```
test_blink/
├── README.md                    <- bu dosya
│
├── f407vg_discovery/            STM32F407VG — STM32F407G-DISC1
│   ├── main.c                   GPIOD kurulumu, SysTick, 4 LED kayan ışık
│   ├── startup_stm32f407vg.s    vektör tablosu + .data/.bss başlatma
│   ├── stm32f407vg.ld           1 MB Flash / 128 KB SRAM haritası
│   ├── Makefile
│   └── README.md
│
└── l476rg_nucleo/               STM32L476RG — NUCLEO-L476RG
    ├── main.c                   GPIOA kurulumu, SysTick, 1 LED 1 Hz blink
    ├── startup_stm32l476rg.s    vektör tablosu + .data/.bss başlatma
    ├── stm32l476rg.ld           1 MB Flash / 96 KB SRAM haritası
    ├── Makefile
    └── README.md
```

Her klasör **bağımsızdır** — kendi Makefile'ı, kendi linker script'i vardır.
İçine girip `make` demek yeterli, aralarında paylaşılan dosya yoktur.

## Hangi kart hangisi

| | F407VG Discovery | L476RG Nucleo |
|---|---|---|
| Kart | STM32F407G-DISC1 | NUCLEO-L476RG (MB1136) |
| Çekirdek | Cortex-M4F | Cortex-M4F |
| Flash | 1 MB @ `0x08000000` | 1 MB @ `0x08000000` |
| Ana SRAM | 128 KB @ `0x20000000` | 96 KB @ `0x20000000` |
| İkinci RAM | 64 KB CCM @ `0x10000000` | 32 KB SRAM2 @ `0x10000000` |
| Yığın tepesi | `0x20020000` | `0x20018000` |
| GPIO veri yolu | AHB1 | AHB2 |
| GPIO taban adresi | `0x40020000` | `0x48000000` |
| RCC taban adresi | `0x40023800` | `0x40021000` |
| GPIO saat register'ı | AHB1ENR (ofs `0x30`) | AHB2ENR (ofs `0x4C`) |
| Reset sonrası saat | HSI 16 MHz | MSI 4 MHz |
| Kullanıcı LED'i | PD12–PD15 (4 adet) | PA5 (1 adet) |
| OpenOCD hedefi | `target/stm32f4x.cfg` | `target/stm32l4x.cfg` |

**En sık yapılan hata:** F4 kodunu L4'e kopyalayıp adresleri güncellememek.
İki çip aynı çekirdeği kullandığı için kod derlenir, yüklenir ve *sessizce
çalışmaz* — saati açılmamış bir bloğa yapılan yazma kaybolur, okuma 0 döner.

## Kurulum

```bash
sudo apt install gcc-arm-none-eabi stlink-tools
```

ST-LINK'e sudo'suz erişmek için udev kuralı:

```bash
sudo tee /etc/udev/rules.d/49-stlinkv2-1.rules >/dev/null <<'EOF'
SUBSYSTEM=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374b", MODE="0666", GROUP="plugdev", TAG+="uaccess"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Kuraldan sonra kartların USB kablosunu çıkarıp takın.

## Kullanım

```bash
cd f407vg_discovery && make flash     # Discovery'ye yükle
cd l476rg_nucleo    && make flash     # Nucleo'ya yükle
```

### İki kart aynı anda bağlıysa — dikkat

Her iki kartın ST-LINK'i de **aynı USB kimliğiyle** görünür:

```
Bus 001 Device 005: ID 0483:374b STMicroelectronics ST-LINK/V2.1
Bus 001 Device 006: ID 0483:374b STMicroelectronics ST-LINK/V2.1
```

Bu durumda `st-flash` hangisine yazacağını bilemez ve **yanlış karta
yükleyebilir**. Ayırt etmenin yolu seri numarasıdır:

```bash
make probe                    # bağlı ST-LINK'leri ve seri numaralarını listeler
make flash SERIAL=0672FF...   # hedefi açıkça belirt
```

Her iki Makefile da `SERIAL` değişkenini destekler. Hangi seri numarasının
hangi karta ait olduğunu bir kez tespit edip not almak işi kolaylaştırır —
tek kart takılıyken `make probe` çalıştırmak en kolay yöntem.

## Ortak tasarım kararları

Her iki projede de:

- **Gecikmeler SysTick ile.** Boş döngünün süresi optimizasyon seviyesine göre
  değişir; SysTick donanım sayacıdır, `-O0` ile `-O2` arasında süre değişmez.
- **LED'lere BSRR ile yazılır, ODR ile değil.** `ODR |= maske` üç işlemdir
  (LDR/ORR/STR) ve araya kesme girerse aynı porttaki başka bir değişiklik
  kaybolur. `BSRR = maske` tek `STR`'dir, atomiktir.
- **Register'lar "oku-maskele-yaz" ile değiştirilir.** Doğrudan `=` yazmak
  aynı porttaki diğer pinlerin ayarlarını siler. L476'da bu ayrıca kritiktir:
  PA13/PA14 SWD hattıdır, bozarsanız kartı debugger'a kapatırsınız.
- **`-nostdlib -nostartfiles`** ile derlenir; `<stdint.h>` dahil hiçbir başlık
  dosyası kullanılmaz, tipler `main.c` içinde elle tanımlanır.
