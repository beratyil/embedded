# embedded — STM32 bare-metal çalışma alanı

Bu depo, iki STM32 geliştirme kartı üzerinde **kütüphanesiz (register seviyesi)**
gömülü yazılım öğrenme projelerini içerir.

---

## 1. Altın kural: kütüphane yok

HAL yok, LL yok, CMSIS yok, libc yok, ST'nin hazır startup dosyası yok.
Her şey elle yazılır:

- `main.c` — doğrudan register erişimi (`REG32(adres)` makrosu)
- `startup_*.s` — vektör tablosu, `.data` kopyalama, `.bss` sıfırlama, `main` çağrısı
- `*.ld` — linker script (bellek haritası, bölümler)
- `Makefile` — derleme + `st-flash` ile yükleme

`-nostdlib -nostartfiles -ffreestanding -fno-builtin` ile derlenir.

## 2. Kod stili (bunu koru)

- **Yorumlar Türkçe, ASCII** — şapkasız/noktasız: "Kullanim", "uretir", "cikis".
  Kaynak dosyalarda Türkçe karakter kullanma. (Markdown dosyalarında serbest.)
- Yorumlar **"ne" değil "neden"** anlatır. Kullanıcı elektronik mühendisi;
  donanım gerekçesini ister, yüzeysel açıklamayı değil.
- Dosyalar numaralı bölümlere ayrılır: `/* --- 12.3 Pinlerin saati --- */`
- Her register yazımında ilgili reference manual bölümü ve bit anlamları yazılır.
- Tuzaklar açıkça uyarılır (ör. GPIOA MODER'e `=` yazmak SWD'yi öldürür).
- Tipler: `u8`, `u32`, `vu32` typedef'leri; `<stdint.h>` bile yok.

## 3. Donanım

| Kart | MCU | Chip ID | ST-Link seri no |
|---|---|---|---|
| STM32F4DISCOVERY | STM32F407VG | `0x413` | `066FFF565257867767154920` |
| NUCLEO-L476RG | STM32L476RG | `0x415` | `0669FF353637503457045439` |

**İkisi de aynı anda USB'ye bağlı** ve aynı USB kimliğiyle (`0483:374b`) görünür.
Bu yüzden her `Makefile`'da `SERIAL` değişkeni doludur — `st-flash` doğru kartı
seri numarasından bulur. Kart değişirse `make probe` ile yeni numarayı al.

## 4. Projeler

### `test_blink/` — başlangıç
Her iki kartta LED yakıp söndürme. SysTick `COUNTFLAG` yoklamalı gecikme,
kesme yok. Register seviyesi kalıbın en sade hali; yeni proje yazarken
startup ve linker script'ler buradan kopyalanır.

### `test_i2c/` — iki kart arası I2C haberleşme ✅ çalışıyor
| | Discovery (F407) | Nucleo (L476) |
|---|---|---|
| Rol | **Master** | **Slave**, adres `0x42` |
| I2C nesli | v1 (`SR1`/`SR2`/`CCR`/`TRISE`) | v2 (`TIMINGR`/`ISR`/`ICR`) |
| Yöntem | Yoklama (polling) | Kesme (`I2C1_EV`/`I2C1_ER`) |
| Pinler | `PB6`=SCL, `PB7`=SDA (AF4) | `PB8`=SCL, `PB9`=SDA (AF4) |
| Saat | HSI 16 MHz, PCLK1=16 MHz | çekirdek MSI 4 MHz, I2C+UART **HSI16** |
| Ekstra | 4 durum LED'i (PD12-15) | USART2 log (PA2, 115200) + LD2 |

Bu ayrım bilinçli: iki çip aynı Cortex-M4 ama **I2C birimleri farklı nesil**.
F4 kodunu adres değiştirip L4'e kopyalamak işe yaramaz.

**Protokol** — slave kendini gerçek bir I2C sensörü gibi sunar:

| Reg | İsim | Erişim | Açıklama |
|---|---|---|---|
| `0x00` | `WHO_AM_I` | R | Sabit `0x5A` |
| `0x01` | `LED_CTRL` | R/W | bit0 → Nucleo LD2 |
| `0x02` | `COUNTER` | R | Tamamlanan işlem sayısı |
| `0x03` | `ECHO` | R/W | Yazılan değer geri okunur |

Master 500 ms'de bir: `LED_CTRL` yaz → `ECHO` yaz → tekrar-START ile 4 register
oku → doğrula. Tur başına **3 I2C işlemi**.

**Discovery LED anlamları:** 🟠PD13 kalp atışı · 🟢PD12 başarı ·
🔴PD14 hata · 🔵PD15 karşıdan cevap yok (kablo/slave problemi).

**Kablolama:** `PB6↔PB8`, `PB7↔PB9`, `GND↔GND`. Harici pull-up **gerekmiyor**:
her iki tarafta dahili pull-up açık (`PUPDR=01`), paralelde ~20 kΩ, kısa
jumper'larla `t_r` ~840 ns < 1000 ns sınırı. Sahada 0 hata ile doğrulandı.

## 5. Komutlar

```bash
cd test_i2c
make                 # ikisini de derle
make flash           # ikisini de yükle (önce slave, sonra master)
make flash-slave     # yalnızca Nucleo
make flash-master    # yalnızca Discovery
make monitor         # Nucleo UART log'u (115200)
make probe           # bağlı ST-Link'leri listele
make clean
```

## 6. Ortam tuzakları (zor yoldan öğrenildi)

**`st-flash read` çekirdeği DURDURUR.** Okuduktan sonra kart donar, LED'ler
kalır. Devam ettirmek için `st-flash --serial <sn> reset`. Bu yüzden RAM'den
sayaç örneklemek için: reset → beklet → **tek** okuma. Arka arkaya okursanız
"sayaç ilerlemiyor" yanılgısına düşersiniz.

**Seri port için `dialout` grubu gerekir.** `/dev/ttyACM*` `root:dialout 0660`.
`sudo usermod -aG dialout $USER` + oturum kapat/aç. `st-flash`'ın çalışması
yanıltmasın: o libusb ile `/dev/bus/usb/...` düğümünü kullanır ve oraya
systemd-logind *uaccess* ACL'i ile zaten erişiminiz vardır. Farklı kapılar.

**`ttyACM` numaraları takma sırasına göre değişir.** Kart çıkarıp takınca
Nucleo `ttyACM0`→`ttyACM1` olabilir. Makefile bu yüzden
`/dev/serial/by-id/usb-STMicroelectronics_STM32_STLink_$(SERIAL)-if02`
kullanır — seri numarası içerdiği için her zaman doğru kartı gösterir.

**st.com PDF'lerini `curl` indiremiyor** (HTTP/2'de stream hatası, HTTP/1.1'de
süresiz takılma; `-4` ve UA değiştirmek çözmüyor). **`wget` sorunsuz çalışıyor** —
`docs/indir.sh` bunu kullanır.

**Discovery'nin sanal COM portu MCU'ya BAĞLI DEĞİL** (UM1472 §7.3.3: ST-Link
VCP uçları U2 pin 12/13, F407'nin USART'ına gitmiyor). Discovery'de `printf`
istenirse: harici USB-TTL dongle (`P1-14`=PA2, `P1-13`=PA3) veya SWO/ITM
(`SB12` köprüsüne bağlı, `st-trace` kurulu). Nucleo'da VCP normal çalışır.

## 7. Belgelerle doğrulanmış gerçekler

`docs/` boş gelir; `bash docs/indir.sh` ile ST'den indirilir (repoya konmadı:
54 MB ve ST telifli).

| Gerçek | Kaynak |
|---|---|
| `I2C1_EV`=IRQ 31 → vektör ofseti `0xBC`, `I2C1_ER`=IRQ 32 → `0xC0` | RM0351 vektör tablosu; objdump ile teyit |
| `TIMINGR=0x30420F13` → 100 kHz @ HSI16 | RM0351 §39.4.9 formülü ile hesaplandı (RM0351'de hazır örnek tablo YOK) |
| F407: `FREQ=16`, `CCR=80`, `TRISE=17` → 100 kHz @ PCLK1 16 MHz | RM0090 §27.6; üretilen makine kodunda teyit |
| Nucleo `PB8`=D15/SCL, `PB9`=D14/SDA (CN5), `+3V3`=CN6-4 | UM1724 |
| Discovery `PB6`→kart "SCL" neti (CS43L22), `PB7`→serbest, `PB9`→"SDA" neti | UM1472 pin tablosu |
| F407 `PB6`=I2C1_SCL, `PB7`=I2C1_SDA (AF4) | DS8626 pin tanım tablosu |
| F4 çok baytlı okuma reçeteleri (1/2/3+ bayt) | AN2824 (başlığı F10x der, I2C v1 IP aynı) |

**Yeni bir register değeri veya pin iddiası eklemeden önce `docs/` altındaki
belgeden doğrula.** Bu depodaki her sayı böyle doğrulandı.

## 8. Doğrulama tekniği (donanım olmadan test edilemeyen kod için)

UART'a erişilemediğinde slave'in RAM'i doğrudan okunabilir:

```bash
arm-none-eabi-nm -S i2c_slave.elf | grep toplam_islem   # adresi bul
st-flash --serial <sn> reset                            # serbest çalıştır
sleep 10                                                # dokunma
st-flash --serial <sn> read x.bin 0x20000000 16         # tek okuma
od -An -tx1 x.bin
```

10 saniyede `toplam_islem=60`, `ECHO=21`, `hata_sayisi=0` çıkması sistemin
uçtan uca çalıştığını kanıtlar (20 tur × 3 işlem = 60).

## 9. Araçlar

`arm-none-eabi-gcc` 13.2.1 · `stlink-tools` 1.8.0 (`st-flash`, `st-info`,
`st-util`, `st-trace`) · `pdftotext` (belge doğrulama için) · `wget`.
