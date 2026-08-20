# `l476rg_nucleo` — DHT22 → SSD1306 OLED

NUCLEO-L476RG üzerinde **DHT22** sıcaklık/nem sensörünü okuyup değeri
**SSD1306 128x64 OLED** ekrana yazar. Kütüphane yok: HAL, LL, CMSIS, libc —
hiçbiri. Kayan nokta da yok.

---

## 1. Kablolama

İki cihaz da **3V3**'ten beslenir, dolayısıyla tüm mantık seviyeleri
STM32 ile birebir aynıdır. Seviye çevirici yok, 5V yok.

### SSD1306 OLED (I2C)

| OLED | Nucleo | Konnektör |
|---|---|---|
| `VCC` | 3V3 | CN6-4 |
| `GND` | GND | CN6-6 |
| `SCL` | PB8 | CN5-10 (D15) — I2C1_SCL, AF4 |
| `SDA` | PB9 | CN5-9 (D14) — I2C1_SDA, AF4 |

### DHT22 / AM2302

| DHT22 | Nucleo | Konnektör |
|---|---|---|
| `VCC` (pin 1) | 3V3 | CN6-4 |
| `DATA` (pin 2) | PA10 | CN9-3 (D2) |
| `NC` (pin 3) | — | bağlanmaz |
| `GND` (pin 4) | GND | CN6-6 |

```
                    3V3 ──┬──────────────┬──  CN6-4
                    GND ──┼──────────────┼──  CN6-6
   ┌───────────┐          │              │
   │  SSD1306  │  SCL ────┼──────────────┼──  PB8   CN5-10
   │  128x64   │  SDA ────┼──────────────┼──  PB9   CN5-9
   └───────────┘          │              │
   ┌───────────┐          │              │
   │   DHT22   │  DATA ───┼──────────────┼──  PA10  CN9-3
   └───────────┘          │              │
                          └── 4.7k-10k ──┘   (DATA → 3V3, aşağıya bak)
```

### ⚠️ DHT22 pull-up direnci

DATA hattında 3.3V'a **4.7 kΩ – 10 kΩ** pull-up olmalı.

- **Hazır modül** (3 pinli küçük kart) kullanıyorsanız üzerinde zaten var,
  ekstra direnç **eklemeyin**.
- **Çıplak 4 pinli sensör** kullanıyorsanız **direnci ekleyin**. Kod dahili
  pull-up'ı da açıyor ama o ~40 kΩ'dur; 50 µs'lik kenarları toparlamaya
  yetmez ve veri bozulur (`SAGLAMA HATASI` görürsünüz).

### Besleme

İkisi birden ~25 mA çeker (OLED ~20, DHT22 ~1.5). Nucleo'nun USB'den
300 mA'lik bütçesine göre çok rahat — kamera projesindeki besleme sorunu
burada beklenmiyor.

---

## 2. Derleme ve yükleme

```bash
make                 # derle
make flash           # Nucleo'ya yükle
make monitor         # UART çıktısını izle (115200)
make clean
```

Ayarlar:

```bash
make BAUD=230400 flash     # UART hızı (koda ve monitor'a birlikte gider)
make ARALIK=5000 flash     # ölçüm aralığı ms
```

`ARALIK` 2000'in altına indirilemez — derleme zamanında `#error` ile
engellenir. DHT22 datasheet'i *"Collecting period should be : >2 second"*
der; daha sık okumak sensörü ısıtır ve değeri bozar.

---

## 3. Ekran düzeni

Tek font, farklı tam sayı ölçekleriyle:

```
┌────────────────────────────────┐
│ DHT22 + SSD1306                │  y=0   ölçek 1  ( 8 px)
│                                │
│  23.4°C                        │  y=12  ölçek 3  (24 px)
│                                │
│  NEM 45.6%                     │  y=40  ölçek 2  (16 px)
│ ok:12 hata:0                   │  y=56  ölçek 1  ( 8 px)
└────────────────────────────────┘
        128 x 64
```

Ölçek **tam sayı** tutuldu: ara değer (1.5x gibi) piksel ızgarasına
oturmaz ve harfler tırtıklı çıkar. Tam sayı ölçekte her font pikseli
`ölçek × ölçek` bir kareye dönüşür, sonuç keskin kalır. Böylece tek font
tablosuyla hem küçük etiket hem büyük sıcaklık yazılabiliyor.

Ölçek 3'te bir karakter 18 px; en uzun hal `-12.3°C` = 7 karakter = 126 px,
128'e tam sığar.

---

## 4. Beklenen UART çıktısı

```
=====================================================
 NUCLEO-L476RG  |  DHT22 -> SSD1306 OLED
=====================================================
 I2C1  : PB8=SCL  PB9=SDA           @ 100 kHz
 DHT22 : PA10 (CN9-3 / D2), acik drenaj + pull-up
 TIM2  : 1 MHz serbest sayac (us olcumu)

[1] SSD1306 araniyor (0x3C / 0x3D)... bulundu: 0x3C
[2] DHT22 kararli hale gelsin diye 2 s bekleniyor...

sicaklik 23.4 C   nem 45.6 %   (ok 1 / hata 0)
sicaklik 23.4 C   nem 45.7 %   (ok 2 / hata 0)
```

Ekran adresi **otomatik bulunur**: modüllerin çoğu `0x3C`, bazıları
`0x3D`'dir (SA0 pini). İkisi de denenir, cevap veren kullanılır — "neden
çalışmıyor" diye uğraşmayasınız diye.

Ekran bulunamazsa program **durmaz**: ölçümler UART'a basılmaya devam eder,
böylece sensörün çalışıp çalışmadığını yine görebilirsiniz.

---

## 5. Arıza ayıklama

| Belirti | Muhtemel sebep |
|---|---|
| UART'ta hiçbir şey yok | `make monitor` baud'u ≠ derlenen `BAUD` · `dialout` grubu |
| `SSD1306 ... BULUNAMADI` | SDA/SCL ters veya kopuk · modül beslemesiz · adres 0x3C/0x3D dışında |
| Ekran bulundu ama **kapkaranlık** | Şarj pompası (`0x8D`,`0x14`) — kodda var; yoksa panel gerilimi üretilmez |
| Görüntü **baş aşağı** | `SSD_SEG_TERS`→`0xA0`, `SSD_COM_TERS`→`0xC0` yapın |
| Ekranın yarısı bozuk / kayık | 128x32 panel kullanıyorsunuz: `SSD_YUKSEKLIK` 32, `SSD_COM_PIN` argümanı `0x02` |
| `CEVAP YOK` | DHT22 DATA teli · besleme · pull-up yok |
| `SAGLAMA HATASI` (ara sıra) | Pull-up zayıf (dahili ~40 kΩ) veya kablo uzun → 4.7 kΩ ekleyin |
| `ZAMAN ASIMI` | Sinyal bozuk; kabloyu kısaltın, DATA'yı GND'ye paralel uzatmayın |
| İlk okuma hep hatalı | Normal olabilir; sensör açılışta 1 s kararsızdır (kod 2 s bekliyor) |
| Sıcaklık 0 °C'nin altında saçma | Sign-magnitude varsayımı — aşağıya bakın |

---

## 6. Kodda dikkat edilen noktalar

**DHT22 pini açık drenaj.** Protokol çift yönlü. Klasik yaklaşım her yön
değişiminde `MODER`'i giriş/çıkış arasında değiştirmektir. Bunun yerine pin
bir kez açık drenaj çıkış yapılıyor: `ODR=0` hattı aşağı çeker, `ODR=1`
yüksek empedansa gider (yani bırakır) ve `IDR` her durumda hattın gerçek
halini okumaya devam eder. Yön değişimi tek bir `BSRR` yazmasına iner.

**TIM2 mikrosaniye sayacı.** 26-28 µs ile 70 µs'yi "döngüde sayarak" ayırmak
derleyici optimizasyonuna bağımlı, kırılgan bir çözümdür — `-O2`'yi `-O0`
yapınca bozulur. TIM2 1 MHz'e bölünüp serbest bırakıldı; ölçüm artık iki
`CNT` okumasının farkı. **TIM2 özellikle seçildi:** L4'te TIM2 ve TIM5
32 bittir (TIM3/TIM4 16 bit). 32 bit sayaç 71 dakikada taşar; 16 bit olsa
65 ms'de taşar ve her fark hesabında taşma düşünmek gerekirdi.

**`TIM2_EGR`'ye `UG` yazmak şart.** Prescaler gölge register'a ancak
güncelleme olayında yüklenir. `UG` yazılmazsa ilk ölçümler 16 kat yanlış
çıkar — fark edilmesi zor bir hata.

**I2C `NBYTES` 8 bittir.** Tek işlemde en fazla 255 bayt. 1024 baytlık
çerçeve tamponu tek seferde sığmaz; sayfa sayfa gönderiliyor (8 işlem ×
129 bayt). SSD1306'nın RAM işaretçisi işlemler arasında korunduğu için bu
sorunsuz çalışır.

**Çerçeve tamponu → titreme yok.** Ekran doğrudan çizilmiyor; RAM'de
hazırlanıp tek seferde gönderiliyor.

**Kayan nokta yok.** DHT22 değerleri zaten onda bir çözünürlükte tam sayı
(234 = 23.4). Float'a çevirmek yazılım float kütüphanesini (birkaç KB)
projeye sokardı.

**Tampon boyutları en kötü duruma göre.** `satir[40]`: `"ok:"` + 10 hane +
`" hata:"` + 10 hane + sonlandırıcı = 30. Dar tutup "nasılsa sayaçlar
büyümez" demek gömülü sistemde en sinsi hata türüdür.

---

## 7. ⚠️ Belgelenmemiş varsayım: negatif sıcaklık

Sıcaklığın 16 bitlik ham değerinde en üst bit **işaret** bitidir ve değer
**ikiye tümleyen değildir** (işaret + büyüklük):

```c
if (ham_sic & 0x8000) sicaklik = -(ham_sic & 0x7FFF);
```

**Bu kodlama elimizdeki DHT22 datasheet sürümünde yazmıyor.** Bütün yaygın
sürücülerin kullandığı ve sahada geçerli olduğu bilinen kural bu, ama
deponun "her sayıyı belgeden doğrula" kuralına uyan bir kaynak bulunamadı.
Sıfırın altında ölçüm yapabiliyorsanız önce bunu doğrulayın.

---

## 8. Dosyalar

| Dosya | İş |
|---|---|
| `main.c` | Sürücünün tamamı — saat, TIM2, I2C, SSD1306, DHT22, çizim |
| `font5x7.h` | 5x7 font tablosu (Adafruit GFX `glcdfont.c`, BSD) |
| `startup_stm32l476rg.s` | Vektör tablosu, `.data` kopyalama, `.bss` sıfırlama |
| `stm32l476rg.ld` | Bellek haritası |
| `Makefile` | Derleme + `st-flash` + `monitor` |

---

## 9. Belgelerle doğrulanmış gerçekler

| Gerçek | Kaynak |
|---|---|
| TIM2 tabanı `0x40000000`; `CR1` 0x00, `EGR` 0x14, `CNT` 0x24, `PSC` 0x28, `ARR` 0x2C | RM0351 Tablo 1, §31.4 |
| TIM2 ve TIM5 **32 bit**, TIM3/TIM4 16 bit | RM0351 §31.2 |
| `RCC_APB1ENR1` bit 0 = `TIM2EN` | RM0351 §6.4.19 |
| HCLK ≤ 16 MHz → **0 wait state** (VCORE Range 1) | RM0351 Tablo 11 |
| `TIMINGR=0x30420F13` → 100 kHz @ HSI16 | RM0351 §39.4.9 (test_i2c'de doğrulandı) |
| Nucleo `PB8`=D15, `PB9`=D14, `PA10`=D2 (CN9-3) | UM1724 Tablo 23 |
| `PA10` = `FT_lu` (5V toleranslı) | DS10198 Tablo 16 |
| DHT22: başlat ≥1 ms · cevap 80 µs L + 80 µs H · bit 26-28 µs=0, 70 µs=1 | DHT22 datasheet (Aosong) §6 |
| DHT22: 5 bayt `RH_int RH_dec T_int T_dec checksum`, MSB-first, besleme 3.3-6V | aynı belge §6 |
| DHT22: ölçüm aralığı **> 2 saniye**; açılışta 1 s kararsız | aynı belge §7, §6 |
| SSD1306 adres `0111100`/`0111101` (0x3C/0x3D), SA0'a göre | SSD1306 datasheet §8.1.5 |
| Kontrol baytı: Co=0 D/C#=0 → `0x00` komut, D/C#=1 → `0x40` veri | aynı belge §8.1.5.1 |
| Komut kodları `81/A4/A6/AE/AF/20/21/22/A8/D3/D5/D9/DA/DB` | aynı belge §10 komut tablosu |
| Şarj pompası: `8Dh` → `14h` → `AFh` sırası şart | aynı belge, Charge Pump Command Table notu |

ST belgeleri `../../test_i2c/docs/` altında. Sensör/ekran belgeleri depoda
değil, `wget` ile:

- `cdn.sparkfun.com/assets/f/7/d/9/c/DHT22.pdf`
- `cdn-shop.adafruit.com/datasheets/SSD1306.pdf`
