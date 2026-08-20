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

### `test_arducam_ov2640/` — iki kamera, tek Nucleo ✅ çalışıyor

NUCLEO-L476RG'ye **iki ArduCAM Mini 2MP** (OV2640 sensör + ArduChip CPLD +
AL422B FIFO). Topoloji ArduCAM'in kendi donanım notundan (§2.2, Şekil 3):
**I2C ve SPI ortak, yalnızca CS ayrı.** Toplam 9 tel, hepsi CN5 üzerinde.

| Sinyal | Pin | Konnektör |
|---|---|---|
| `SCL` / `SDA` — ortak | PB8 / PB9 | CN5-10 / CN5-9 (AF4) |
| `SCK` / `MISO` / `MOSI` — ortak | PA5 / PA6 / PA7 | CN5-6 / CN5-5 / CN5-4 (AF5) |
| `CS` kamera 0 / kamera 1 — **ayrı** | PB6 / PC7 | CN5-3 / CN5-2 (GPIO) |
| `+5V` / `GND` | — | CN6-5 / CN6-6 |

**Adres çakışması neden sorun değil:** OV2640'ın SCCB adresi fabrikada
sabittir (`0x30`, 7-bit; adres seçme pini yok) ve iki sensör aynı hatta.
Ama I2C yalnızca *sensörü ayarlar* ve ikisine **aynı** ayarı istiyoruz →
tek yazma ikisine birden gider (yayın), ikisi de ACK çeker, açık drenajda
sorunsuz. Asıl iş (yakalama tetiği, FIFO okuma) SPI'da ve CS ayrı olduğu
için kamera başına bağımsız.

> **Bedeli:** I2C okuması hatta kaç sensör olduğunu SÖYLEMEZ (ikisi de aynı
> değeri sürer). Bu yüzden *kamera varlık testi I2C'den değil SPI'dan*
> yapılır — ArduChip `0x00`'a `0x55`/`0xAA` yaz-oku.
> **Sınırı:** iki kameraya farklı ayar verilemez. Gerekirse ikinciyi I2C3'e
> (PC0/PC1, CN8-6/CN8-5) alın.

**Saat:** SYSCLK = HSI16 = 16 MHz. Sebep: SPI'ın en hızlı bölmesi PCLK/2'dir
ve bu, ArduChip'in tavanı olan 8 MHz'e tam oturur. 16 MHz'de Flash 0
wait-state ister, ayar gerekmez (RM0351 Tablo 11).

Sürücünün tamamı **yoklamalı**, kesme yok. Görüntü RAM'de tamponlanmaz —
FIFO'dan bayt okunup doğrudan UART'a yazılır; darboğaz UART olduğu için
640x480 bile 128 KB SRAM'i zorlamaz.

**Neden Nucleo:** Discovery'nin VCP'si MCU'ya bağlı değil (bkz. §6), yani
görüntüyü dışarı verecek yolu yok. F407'nin DCMI'ı olsa da ArduCAM Mini
paralel veri yolunu dışarı vermiyor; DCMI için çıplak OV2640 (24 pin DVP)
modülü gerekir.

**Durum:** tek kamerayla uçtan uca doğrulandı — geçerli 320x240 baseline
JPEG, tanınabilir fotoğraf. İkinci kamera besleme sorunu nedeniyle bekliyor
(kod tek kamerayla da çalışacak şekilde yazıldı).

### `test_dht22_ssd1306/` — sensorden ekrana 🔧 donanim testi bekliyor

NUCLEO-L476RG: **DHT22** okunur, deger **SSD1306 128x64 OLED**'e yazilir.
Depodaki ilk **cikti cihazli** proje.

| Sinyal | Pin | Konnektör |
|---|---|---|
| OLED `SCL` / `SDA` | PB8 / PB9 | CN5-10 / CN5-9 (AF4, I2C1) |
| DHT22 `DATA` | PA10 | CN9-3 (D2) |
| Besleme | 3V3 / GND | CN6-4 / CN6-6 |

Her ikisi de **3V3**'ten beslenir → tüm mantık seviyeleri STM32 ile aynı,
seviye çevirici yok. Toplam ~25 mA, besleme sorunu beklenmiyor.

**DHT22 — zamanlama problemi.** Çipte bu protokolü yapan çevre birimi yok;
her kenar yazılımla ölçülür. Bitin değeri HIGH süresinde saklı (26-28 µs =
`0`, 70 µs = `1`, eşik 50 µs). Bunu güvenilir ölçmek için **TIM2 1 MHz'e
bölünüp serbest bırakıldı**; ölçüm iki `CNT` okumasının farkı. Döngüde
saymak derleyici optimizasyonuna bağımlı olurdu.

> TIM2 özellikle seçildi: L4'te **TIM2 ve TIM5 32 bittir** (TIM3/TIM4 16 bit).
> 32 bit sayaç 71 dakikada taşar; 16 bit olsa 65 ms'de taşardı.
> `TIM2_EGR`'ye `UG` yazmak şart, yoksa prescaler gölge register'a yüklenmez
> ve ilk ölçümler 16 kat yanlış çıkar.

**DHT22 pini açık drenaj.** Protokol çift yönlü. `MODER`'i giriş/çıkış
arasında değiştirmek yerine pin bir kez açık drenaj yapıldı: `ODR=0` hattı
çeker, `ODR=1` bırakır, `IDR` her durumda gerçek hali okur.

**SSD1306 — bellek düzeni problemi.** Video RAM satır değil **sayfa**
düzeninde: bir bayt = üst üste 8 piksel. Font tablosu da aynı düzende
seçildi (bayt = sütun), böylece karakterler bit döndürmeden yazılıyor.
RAM'de 1024 baytlık çerçeve tamponu tutulup tek seferde gönderilir
(titreme yok). Tek font, tam sayı ölçekle büyütülür (1x/2x/3x).

> **TUZAK:** `I2C_CR2`'deki `NBYTES` 8 bittir → tek işlemde en fazla 255
> bayt. 1024 baytlık çerçeve sığmaz; sayfa sayfa gönderilir (8 × 129 bayt).

Kayan nokta yok: DHT22 değerleri zaten onda bir çözünürlükte tam sayı.

**Belgelenemeyen tek nokta:** negatif sıcaklığın *işaret + büyüklük*
kodlaması (ikiye tümleyen değil) elimizdeki datasheet sürümünde yazmıyor.
Kodda açıkça işaretli.

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

```bash
cd test_dht22_ssd1306
make flash                    # Nucleo'ya yukle
make monitor                  # UART ciktisi
make ARALIK=5000 flash        # olcum araligi ms (asgari 2000, #error korumali)
```

```bash
cd test_arducam_ov2640
make flash                    # Nucleo'ya yükle
make monitor                  # teşhis çıktısı (4 aşamalı rapor)
make yakala                   # görüntüleri .jpg kaydet (tools/jpeg_al.py)
make COZUNURLUK=640 flash     # 160 / 320 (varsayılan) / 640
make GORUNTU_AKISI=1 flash    # ham JPEG'i UART'a dök
make BAUD=460800 flash        # hızlı akış (BAUD hem koda hem monitor'a gider)
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

**`make` derleyici bayrağı değişimini YAKALAMAZ.** Yalnızca dosya zaman
damgalarına bakar; `main.c` değişmediyse `make COZUNURLUK=640` yeni `-D`
bayraklarına rağmen yeniden derlemez ve **sessizce eski binary'yi** yükler.
Bu tuzağa sahada düşüldü (`GORUNTU_AKISI=1` yüklendi sanıldı, kart eski
kodu çalıştırıyordu). Çözüm `test_arducam_ov2640/l476rg_nucleo/Makefile`
içinde: bayraklar `.ayarlar` dosyasına yazılır, `.o` ona bağımlı yapılır.
**Parametreli yeni bir Makefile yazarken aynısını yapın.**

**Üst Makefile'da `export DEGISKEN` YAZMAYIN.** Komut satırı değişkenleri
alt make'e `MAKEFLAGS` ile zaten geçer. Dahası `export`, değişken
verilmediğinde onu **boş değer** olarak ortama koyar; alt Makefile'daki
`?=` "zaten tanımlı" görüp varsayılanı atlar ve `-DX=` gibi bozuk bir
tanım üretir. Sonuç anlaşılması zor bir ön işlemci hata yığınıdır.

**Çevre birimi kartı USB enumerasyonunu düşürebilir.** ArduCAM takılıyken
Nucleo `lsusb`'de görünüyor ama `st-info` bulamıyordu. Ayırt edici işaret:
`/sys/bus/usb/devices/1-1/bConfigurationValue` **boş**, arayüz dizini yok.
Descriptor okuma 100 mA'lik varsayılan durumda çalışır; `SET_CONFIGURATION`
cihazı Nucleo'nun descriptor'ındaki 300 mA'e geçirir ve orada düşer. Yani
"kart görünüyor ama programlanamıyor" = **besleme**, kablo veya izin değil.
Tek kameranın bile düşürmesi normal değildir; kısa devre/yanlış tel arayın.

## 7. Belgelerle doğrulanmış gerçekler

Belgeler **`test_i2c/docs/`** altındadır (kökte `docs/` yok). Boş gelir;
`bash test_i2c/docs/indir.sh` ile ST'den indirilir (repoya konmadı: 54 MB
ve ST telifli). ArduCAM belgesi ayrı, `wget` ile:
`blog.arducam.com/downloads/shields/ArduCAM_Mini_2MP_Camera_Shield_Hardware_Application_Note.pdf`

| Gerçek | Kaynak |
|---|---|
| `I2C1_EV`=IRQ 31 → vektör ofseti `0xBC`, `I2C1_ER`=IRQ 32 → `0xC0` | RM0351 vektör tablosu; objdump ile teyit |
| `TIMINGR=0x30420F13` → 100 kHz @ HSI16 | RM0351 §39.4.9 formülü ile hesaplandı (RM0351'de hazır örnek tablo YOK) |
| F407: `FREQ=16`, `CCR=80`, `TRISE=17` → 100 kHz @ PCLK1 16 MHz | RM0090 §27.6; üretilen makine kodunda teyit |
| Nucleo `PB8`=D15/SCL, `PB9`=D14/SDA (CN5), `+3V3`=CN6-4 | UM1724 |
| Discovery `PB6`→kart "SCL" neti (CS43L22), `PB7`→serbest, `PB9`→"SDA" neti | UM1472 pin tablosu |
| F407 `PB6`=I2C1_SCL, `PB7`=I2C1_SDA (AF4) | DS8626 pin tanım tablosu |
| F4 çok baytlı okuma reçeteleri (1/2/3+ bayt) | AN2824 (başlığı F10x der, I2C v1 IP aynı) |
| SPI1 tabanı `0x40013000`; `RCC_APB2ENR` ofset `0x60`, `SPI1EN` bit 12 | RM0351 Tablo 1, §6.4.21 |
| `SPI_CR2`: `FRXTH` bit 12, `DS[3:0]` bit 11:8 · `SPI_CR1`: `BR[2:0]` bit 5:3 | RM0351 §42.6.1-2 |
| `I2C_CR2`: `AUTOEND` 25, `NBYTES` 23:16, `START` 13, `RD_WRN` 10 | RM0351 §39.7.2 |
| HCLK ≤ 16 MHz → **0 wait state** (VCORE Range 1) | RM0351 Tablo 11 |
| L476 `PA5`=`TT_a` (**yalnız 3.6V**) · `PA6`/`PA7`=`FT_la` · `PB8`/`PB9`=`FT_fl` · `PC7`=`FT_l` | DS10198 Tablo 16 |
| Nucleo `PA5`=D13, `PA6`=D12, `PA7`=D11, `PB6`=D10, `PC7`=D9 | UM1724 Tablo 23 |
| USB'den **300 mA** sınırı (JP1 OFF) · `E5V` (CN7-6) 500 mA · harici için JP5 pin 2-3 | UM1724 §7.5.2-7.5.4 |
| Nucleo'da `+5V` CN6-5 ile CN7-18 **aynı nettir** (ikisine bağlamak bütçeyi bölmez) | UM1724 §7.5.4 |
| ArduChip register tablosu · SPI mode 0 · azami 8 MHz | ArduCAM-M-2MP Hardware Application Note §4-6 |
| OV2640 SCCB adresi sabit `0x60`/`0x61` (7-bit `0x30`) | aynı belge §3 |
| Çoklu kamera: I2C+SPI ortak, yalnızca CS ayrı | aynı belge §2.2, Şekil 3 |

**Yeni bir register değeri veya pin iddiası eklemeden önce belgeden
doğrula.** Bu depodaki her sayı böyle doğrulandı.

### Belge de yanılır — ölçümle çelişirse ölçüm kazanır

ArduCAM donanım notu (Rev 1.0, 2015) ArduChip'in **ilk** revizyonunu
anlatıyor. Elimizdeki modül daha yeni (sürüm register'ı `0x40` yerine
`0x73`). Sahada ölçümle bulunan üç sapma:

| Belge / referans kod ne diyor | Gerçek (ölçüldü) |
|---|---|
| Burst okumada ilk bayt kukladır (§5.3) | **Kukla yok**; atmak JPEG'in ilk baytını yutuyor |
| `0x03` bit1 VSYNC kutupluğu; ArduCAM kütüphanesi `0x02` yazar | Bu modülde `0x02` yakalamayı **öldürüyor** (FIFO 8 baytta kalıyor); bit1 **temiz** olmalı |
| `0x07` diye bir register yok (Tablo 1) | Yeni revizyon açılışta **CPLD reset'i** ister: `0x80` → 100 ms → `0x00` → 100 ms |

Ayrıca: **FIFO uzunluğu JPEG boyutu değildir.** `N*1024 + 8` kalıbına
yuvarlanır, fark dolgu baytıdır (ölçüm: bildirilen 3080, gerçek JPEG 2264).
FIFO'dan bildirilen kadar okumak doğru, ama dosyaya yazarken `FF D9`'da
kesmek gerekir.

Ve `ov2640_regs.h`: değerlerin çoğu OV2640'ın **DSP bankasına** ait, OmniVision
bunu yayınlamamış. Deponun "her sayıyı belgeden doğrula" kuralının
uygulanamadığı tek yer — dosyanın başında açıkça yazılı, dokunmayın.

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

**Belirsiz bir register değerini tara, tahmin etme.** ArduChip `0x03`'te
hangi bit kombinasyonunun doğru olduğu bilinmiyordu. Sekiz adayı sırayla
yazıp her birinde yakalama yapan ve FIFO uzunluğunu raporlayan geçici bir
aşama eklendi; desen **tek turda** tek bite indi. Tahmin edip her seferinde
yeniden yüklemekten kat kat hızlı. Tarama işini bitirince kaldırılır, bulgu
yoruma yazılır.

**Ayırt edici ölçüm ekle.** Aynı belirti farklı sebeplerden gelir ve rapor
hepsinde aynı görünür. Örnek: "FIFO boş" hem *sensör hiç kare üretmiyor*
hem de *üretiyor ama FIFO yolu yanlış* durumunda aynıdır. ArduChip'in VSYNC
bitini yoklayıp geçiş saymak ikisini ayırdı ve besleme/sensör hipotezini
tek ölçümle eledi. Böyle bir ölçüm eklemek, birkaç tur tahmin yürütmekten
ucuzdur.

**Uçtan uca kanıtı donanımın kendisinden al.** Kamera projesinde yakalanan
karenin ilk baytları UART'a basılır: `FF D8` (JPEG SOI) görülüyorsa
bilgisayarda hiçbir şey çalıştırmadan tüm zincirin çalıştığı bilinir.
Ayrıca dosya boyutu ışık göstergesidir (karanlıkta ~2 KB, aydınlıkta
~6.5 KB) ve kareler arası boyut değişimi görüntünün donmadığını gösterir.

## 9. Araçlar

`arm-none-eabi-gcc` 13.2.1 · `stlink-tools` 1.8.0 (`st-flash`, `st-info`,
`st-util`, `st-trace`) · `pdftotext` + `pdftoppm` (belge doğrulama; şekilleri
görmek için sayfayı PNG'ye çevirmek gerekebilir) · `wget` · `python3`
(**yalnızca stdlib** — host araçlarında dış bağımlılık yok, seri port
`termios` ile açılır) · `file` (üretilen görüntüyü doğrulamak için).
