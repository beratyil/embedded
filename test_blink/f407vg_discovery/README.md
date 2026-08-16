# STM32F407VG — Register Seviyesi Blink

STM32F407G-DISC1 / STM32F4DISCOVERY kartı için **hiçbir kütüphane kullanmadan**
(HAL yok, LL yok, CMSIS yok, libc yok) yazılmış blink projesi.
Startup kodu, linker script ve tüm donanım erişimi elle yazılmıştır.

## Dosyalar

| Dosya | Görevi |
|---|---|
| `main.c` | GPIOD kurulumu, SysTick ile 1 ms zaman tabanı, LED döngüsü |
| `startup_stm32f407vg.s` | Vektör tablosu, `.data` kopyalama, `.bss` sıfırlama, `main` çağrısı |
| `stm32f407vg.ld` | Flash/SRAM hafıza haritası ve bölüm yerleşimi |
| `Makefile` | Derleme ve karta yükleme kuralları |

## Donanım

LED'ler GPIOD üzerinde, aktif-YÜKSEK (pin = 1 → LED yanar):

| Pin | LED | Renk |
|---|---|---|
| PD12 | LD4 | Yeşil |
| PD13 | LD3 | Turuncu |
| PD14 | LD5 | Kırmızı |
| PD15 | LD6 | Mavi |

Saat kaynağı: reset sonrası varsayılan **HSI @ 16 MHz** (PLL kurulmadı).
`main.c` içindeki `SISTEM_SAATI_HZ` bu değere göre ayarlıdır.

## Gereksinimler

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

Kuraldan sonra kartın USB kablosunu çıkarıp takın.

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

Alternatif yükleyici: `make openocd` (hedef dosyası `stm32f4x.cfg`,
L4'ün `stm32l4x.cfg` dosyası değil).

## Beklenen davranış

4 LED sırayla (yeşil → turuncu → kırmızı → mavi) 150 ms yanıp söner.
Hepsinin birlikte yanıp sönmesini isterseniz `main.c` içindeki döngüyü
`led_yak(M_HEPSI); bekle_ms(500); led_sondur(M_HEPSI); bekle_ms(500);`
ile değiştirin.

## Notlar

- **Yükleme SWD üzerinden yapılır, COM portundan değil.** `/dev/ttyACM0`
  ST-LINK'in sanal seri portudur; programlama ST-LINK'in ayrı USB arayüzü
  üzerinden gider. Bu proje seri port kullanmaz.
- Gecikmeler boş döngü yerine SysTick donanım sayacıyla üretildiği için
  optimizasyon seviyesi (`-O0` / `-O2`) süreleri değiştirmez.
- LED'lere yazarken `ODR` yerine `BSRR` kullanılır: tek `STR` komutuyla
  atomik set/reset sağlar, kesme kaynaklı yarış koşulunu engeller.
