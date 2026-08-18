# `l476rg_nucleo` — iki ArduCAM OV2640, tek Nucleo

NUCLEO-L476RG üzerinde **iki adet ArduCAM Mini 2MP** (OV2640 + ArduChip +
AL422B FIFO) modülünü register seviyesinde sürer. Kütüphane yok: HAL, LL,
CMSIS, libc — hiçbiri.

---

## 1. Kablolama

**CS dışında her şey ortak.** Tüm sinyaller Arduino başlığı üzerinde,
tek sırada:

| ArduCAM pini | Nucleo pini | Konnektör | Not |
|---|---|---|---|
| `+5V` | +5V | CN6-5 | ⚠️ **3.3V değil** — aşağıya bak |
| `GND` | GND | CN6-6 | |
| `SCL` | PB8 | CN5-10 (D15) | I2C1_SCL, AF4 — **iki kamerada da ortak** |
| `SDA` | PB9 | CN5-9 (D14) | I2C1_SDA, AF4 — **ortak** |
| `SCK` | PA5 | CN5-6 (D13) | SPI1_SCK, AF5 — **ortak** |
| `MISO` | PA6 | CN5-5 (D12) | SPI1_MISO, AF5 — **ortak** |
| `MOSI` | PA7 | CN5-4 (D11) | SPI1_MOSI, AF5 — **ortak** |
| `CS` (kamera 0) | PB6 | CN5-3 (D10) | GPIO çıkış — **ayrı** |
| `CS` (kamera 1) | PC7 | CN5-2 (D9) | GPIO çıkış — **ayrı** |

```
                        +5V ──┬──────────────┬─────  CN6-5
                        GND ──┼──────────────┼─────  CN6-6
   ┌──────────┐              │              │
   │ ArduCAM  │  SCL ────────┼──────────────┼─────  PB8   ┐
   │    #0    │  SDA ────────┼──────────────┼─────  PB9   │ ORTAK
   │          │  SCK ────────┼──────────────┼─────  PA5   │ (7 tel)
   │          │  MISO ───────┼──────────────┼─────  PA6   │
   │          │  MOSI ───────┼──────────────┼─────  PA7   ┘
   │          │  CS ─────────┼──────────────┼─────  PB6   ← yalnız #0
   └──────────┘              │              │
   ┌──────────┐              │              │
   │ ArduCAM  │  SCL/SDA/SCK/MISO/MOSI ─────┘   (yukarıdakilerle paralel)
   │    #1    │  CS ─────────────────────────────  PC7   ← yalnız #1
   └──────────┘
```

Toplam 9 tel: 7 ortak + 2 ayrı CS.

### Besleme — 5V, 3.3V değil

Modülün üzerinde 3.3V LDO var. Girişine 3.3V verirseniz dropout'a girer,
çıkış ~2.1V'a düşer ve kamera çalışmaz. **VCC mutlaka +5V.**

I/O seviyesi 3.3V, **seviye çevirici gerekmiyor**. Gerekçe: ArduCAM kendi
donanım notunda host olarak Raspberry Pi ve BeagleBone Black listeliyor —
ikisi de 3.3V ve 5V toleransı yok. 5V I/O olsa o kartları yakardı.

### Akım — sınırdasınız

İki modül kabaca ~200 mA çeker. UM1724 §7.5.2: USB'den beslenen Nucleo'nun
toplam bütçesi **300 mA** (JP1 OFF) ve *"If the maximum current consumption
of the NUCLEO and its extension boards exceeds 300 mA, it is mandatory to
power the board using an external supply."*

Kameralar rastgele donuyor / reset atıyorsa **ilk şüpheli budur.**
Çözüm: CN7-6 (`E5V`) üzerinden harici 5V (500 mA).

### LD2 kullanılamıyor

PA5 hem `SPI1_SCK` hem de yeşil LD2. SPI saati akarken LED kırpışır, durum
LED'i olarak kullanılamaz. Karşılığında bedava bir *"SPI trafiği var"*
göstergesi: yakalama sırasında LD2 hiç kırpışmıyorsa SPI hiç çalışmıyordur.

> PA5 datasheet'te `TT_a`, yani yalnızca **3.6V toleranslı**
> (PA6/PA7/PB8/PB9/PC7 ise `FT` = 5V toleranslı). PA5 çıkış olduğu için
> sorun değil, ama bu pine başka bir şey bağlayacaksanız aklınızda olsun.

---

## 2. Derleme ve yükleme

```bash
make                 # derle
make flash           # Nucleo'ya yükle
make monitor         # teşhis çıktısını izle (115200)
make clean
```

Ayarlar komut satırından:

```bash
make COZUNURLUK=640 flash        # 160 / 320 (varsayılan) / 640
make GORUNTU_AKISI=1 flash       # ham JPEG'i UART'a dök
make BAUD=460800 flash           # hızlı akış için
```

`BAUD` tek yerden yönetilir: hem koda `-DUART_BAUD` olarak geçer hem de
`make monitor` aynı hızı kullanır. İkisini elle eşitlemeye çalışmak bu tür
projelerde en çok zaman kaybettiren hatadır.

---

## 3. Program ne yapar

Dört aşama. Her aşama bir öncekini varsayar, sırayla arıza ayıklamak için
tasarlandı.

| Aşama | Ne test eder | Nasıl |
|---|---|---|
| **1** | SPI, **kamera başına** | ArduChip `0x00`'a `0x55`/`0xAA` yaz-oku + sürüm `0x40` |
| **2** | I2C, ortak hat | OV2640 kimliği `PIDH`/`PIDL` |
| **3** | Sensör kurulumu | ~250 register'ı **yayın** olarak yaz |
| **4** | Uçtan uca | Sırayla yakala, FIFO uzunluğu + ilk baytlar |

### Beklenen çıktı

```
=====================================================
 NUCLEO-L476RG  |  2 x ArduCAM Mini 2MP (OV2640)
=====================================================
 SPI1 : PA5=SCK  PA6=MISO  PA7=MOSI   @ 8 MHz, mode 0
 I2C1 : PB8=SCL  PB9=SDA              @ 100 kHz
 CS   : PB6=KAM0  PC7=KAM1
 Coz. : 320x240   (JPEG)

[1] SPI baglantisi (kamera basina, CS ayri):
  KAM0 : yaz/oku 0x55->0x55  0xAA->0xAA  surum(0x40)=0x73   -> TAMAM (yeni revizyon)
  KAM1 : yaz/oku 0x55->0x55  0xAA->0xAA  surum(0x40)=0x73   -> TAMAM (yeni revizyon)
  -> 2 kamera bulundu.

[2] I2C / OV2640 kimligi (ORTAK hat, yayin):
  PIDH=0x26  PIDL=0x42   -> OV2640 dogrulandi
  (not: bu test kamera SAYISINI olcmez -- bkz. asama 1)

[3] OV2640 JPEG kurulumu (yayin, ~250 register)...
  basarisiz yazma: 0   -> TAMAM

[3.5] VSYNC etkinligi (sensor kare uretiyor mu?):
  KAM0 : VSYNC gecisi = 10   -> kare uretiliyor

[4] Yakalama dongusu basliyor.
    Goruntu akisi kapali (yalnizca rapor).

tur 1  KAM0  uzunluk=3080  bas=0xFF 0xD8 0xFF 0xE0  -> JPEG SOI TAMAM
tur 1  KAM1  uzunluk=3096  bas=0xFF 0xD8 0xFF 0xE0  -> JPEG SOI TAMAM
```

`[3.5]` aşaması bilerek eklendi: "FIFO boş" iki çok farklı sebepten olur
(sensör hiç kare üretmiyor / üretiyor ama FIFO yolu yanlış) ve rapor
ikisinde de aynı görünür. VSYNC geçişi saymak bu ikisini ayırır —
sahada tam olarak bu ölçüm sayesinde doğru yere bakıldı.

**`JPEG SOI TAMAM` görürseniz sistem uçtan uca çalışıyor demektir** —
bilgisayarda hiçbir şey çalıştırmadan, yalnızca seri terminale bakarak.
`0xFF 0xD8` JPEG dosyalarının başındaki *Start Of Image* işaretidir.

---

## 4. Görüntüyü bilgisayara almak

```bash
make GORUNTU_AKISI=1 BAUD=460800 flash
make BAUD=460800 yakala
```

`tools/jpeg_al.py` seri porttan çerçeveli akışı ayıklar ve
`goruntuler/kam0_0001.jpg` gibi dosyalar yazar. **Dış bağımlılık yok** —
pyserial gerekmiyor, port `termios` ile ham kipte açılıyor.

Çerçeve biçimi:

```
#IMG:<kamera>:<uzunluk>\r\n
<tam olarak 'uzunluk' kadar ham bayt>
\r\n#SON\r\n
```

Uzunluk başlıkta verildiği için ham veride ayırıcı aramaya gerek yok —
JPEG içinde her bayt değeri geçebileceği için bu şart.

### Ne kadar sürer

| Çözünürlük | Tipik boyut | 115200 | 460800 |
|---|---|---|---|
| 160x120 | ~2-4 KB | ~0.3 s | ~0.08 s |
| 320x240 | ~2-12 KB | ~0.3-1.0 s | ~0.1-0.25 s |
| 640x480 | ~10-30 KB | ~1-2.6 s | ~0.3-0.7 s |

Boyut sahneye göre değişir. Aynı kamera, aynı sahne, 320x240:

| Ortam | FIFO uzunluğu | Gerçek JPEG |
|---|---|---|
| Oda ışığı **kapalı** | 3080 | ~2262 bayt |
| Oda ışığı **açık** | 7176 | ~6459-6578 bayt |

**Dosya boyutu iyi bir ışık göstergesidir**: 2 KB civarında takılıyorsa
sensöre ışık düşmüyordur. Işıklı sahnede kareler arası boyut da değişir
(6459 / 6577 / 6568 / 6578) — sabit boyut, donmuş bir görüntünün işaretidir.

> FIFO uzunluğu içeriği takip eder ama 1024'lük bloklara yuvarlanır
> (hep `N*1024 + 8`). Karanlıkta çekip "uzunluk hep aynı" diye düşünmeyin:
> durağan bir sahnede JPEG boyutu zaten neredeyse sabittir.

Darboğaz UART; SPI (8 MHz ≈ 800 KB/s) değil. Bu yüzden görüntü RAM'de
**tamponlanmaz**: FIFO'dan bir bayt okunup doğrudan UART'a yazılır.
Sonuç olarak 640x480 bile 128 KB SRAM'i hiç zorlamaz.

---

## 5. Arıza ayıklama

| Belirti | Muhtemel sebep |
|---|---|
| Hiçbir şey basmıyor | `make monitor` baud'u ≠ derlenen `BAUD`. Ya da `dialout` grubu yok. |
| `[1]`'de her iki kamera `SPI YOK` | VCC 3.3V'a bağlı (5V olmalı) · GND ortak değil · MISO/MOSI ters |
| `[1]`'de biri TAMAM biri değil | O kameranın CS teli veya modülün kendisi |
| `[1]` TAMAM, `[2]` `I2C CEVAP YOK` | SDA/SCL ters veya kopuk |
| `[2]` kimlik `0xFF 0xFF` | I2C hattında pull-up yok gibi davranıyor; modül beslemesiz olabilir |
| `[3]`'te başarısız yazma > 0 | Beslemede çökme — harici 5V deneyin |
| `[4]` `YAKALAMA ZAMAN ASIMI` | Sensör kare üretmiyor; `[3]` gerçekten geçti mi? |
| `[4]` `uzunluk=0` | VSYNC kutupluğu / FIFO temizliği; `AC_TIM` yazımına bakın |
| `[4]` `uzunluk=8` sabit | **VSYNC kutupluğu** — `AC_TIM` bit1 temiz olmalı (aşağıya bak) |
| `[4]` `SOI YOK` | Burst okumadaki kukla bayt sayısı (aşağıya bak) |
| Görüntü tamamen siyah | Lens kapağı/koruyucu film · karanlık oda. Boyut ~2 KB'de kalıyorsa ışık yok demektir |
| `make COZUNURLUK=640` işlemiyor | Bu tuzak çözüldü (`.ayarlar`), ama başka bayrak eklerseniz aynı şey olur |
| Kameralar rastgele donuyor | **Akım.** Harici 5V (E5V, CN7-6) |

### Sahada çözülen üç tuzak

Bunlar tahminle değil ölçümle bulundu; kodda gerekçeleriyle yazılı.

**1. VSYNC kutupluğu ters.** `AC_TIM` (0x03) register'ının bit1'i. Sekiz aday
değer taranıp FIFO uzunluğuna bakıldı:

| TIM | Sonuç | TIM | Sonuç |
|---|---|---|---|
| `0x00` | 6152 bayt, geçerli JPEG | `0x02` | **8 bayt** |
| `0x08` | 6152 (bir bayt kaymış) | `0x0A` | **8 bayt** |
| `0x10` | 6152 bayt, geçerli JPEG | `0x12` | **8 bayt** |
| `0x18` | 6152 (bir bayt kaymış) | `0x1A` | **8 bayt** |

Desen tek bite indi: **bit1 set ise yakalama anında kesiliyor.** Bu modülde
VSYNC aktif *yüksek*. ArduCAM kütüphanesi burada `0x02` yazar ve yanına
"VSYNC is active HIGH" yorumunu koyar — bizim modülde `0x02` yakalamayı
öldürüyor. Referans kodu körü körüne kopyalamak yerine ölçmek gerekiyor.

bit3 (veri gecikmesi) açılırsa akış bir bayt kayıyor. bit4 (FIFO modu) bu
revizyonda etkisiz.

**2. Kukla bayt yok.** Donanım notu §5.3 *"The first byte read from the FIFO
is a dummy byte"* der. Bu revizyonda **yanlış**: atma varken akışın başı
`0xD8 0xFF` geliyordu, yani gerçek `FF D8 FF E0` dizisinin ilk baytı
yutulmuştu. `0x3C` komutundan sonraki bayt zaten geçerli veridir.

**3. FIFO uzunluğu JPEG boyutu değildir.** Bildirilen 3080, JPEG'in bitiş
işareti (`FF D9`) ise 2280. baytta. ArduChip uzunluğu blok boyutuna
yuvarlıyor (hep `N*1024 + 8` çıkıyor), aradaki fark dolgu baytı. FIFO'dan
bildirilen kadar okumak doğru, ama dosyaya yazarken `FF D9`'da kesmek
gerekiyor — `tools/jpeg_al.py` bunu yapıyor.

### ArduChip sürümü `0x40` değil `0x73` çıkarsa

Sorun değil. Eski donanım notu "2MP modelinde sabit 0x40" der ama yeni
revizyonlar başka değer döndürür ve **ArduCAM'in kendi kütüphanesi bu
register'ı hiçbir yerde kontrol etmez** — modül kimliğini OV2640'ın I2C
kimliğinden doğrular. Yeni revizyonlar ayrıca açılışta **CPLD yazılım
reset'i** ister (`0x07` ← `0x80`, 100 ms, `0x00`, 100 ms); bu register eski
belgenin Tablo 1'inde yok.

---

## 6. Dosyalar

| Dosya | İş |
|---|---|
| `main.c` | Sürücünün tamamı — saat, SPI, I2C/SCCB, ArduChip, yakalama |
| `ov2640_regs.h` | OV2640 register tabloları (ArduCAM kütüphanesinden çıkarıldı) |
| `startup_stm32l476rg.s` | Vektör tablosu, `.data` kopyalama, `.bss` sıfırlama |
| `stm32l476rg.ld` | Bellek haritası |
| `Makefile` | Derleme + `st-flash` + `monitor` + `yakala` |
| `tools/jpeg_al.py` | Host tarafı JPEG alıcı (stdlib, bağımlılıksız) |

---

## 7. Kodda dikkat edilen tuzaklar

**SPI DR'ye 8-bit erişim.** L4'ün SPI'ında `DR`'ye 32-bit yazmak, `DS=8`
iken FIFO'ya **iki bayt** iter. Bu yüzden `SPI1_DR8` bir `volatile u8`
işaretçisidir. Üretilen makine kodunda doğrulandı:

```
80003a4:  7321   strb  r1, [r4, #12]     <- 8-bit yazma
80003ac:  7b13   ldrb  r3, [r2, #12]     <- 8-bit okuma
```

**`FRXTH` bayrağı.** L4'te RXNE eşiği varsayılan olarak 16 bittir; 8 bit
gönderirseniz RXNE **hiç kalkmaz** ve program sonsuza kadar bekler.
`SPI_CR2_FRXTH` bunu 8 bite indirir. F4'te bu bit yoktur.

**SCCB okuma tekrar-START kullanmaz.** OV sensörleri
`[adres+W][reg][STOP]` + `[adres+R][veri][STOP]` bekler. `test_i2c`'deki
master tekrar-START kullanıyordu; o kalıbı buraya kopyalarsanız okuma
sessizce `0xFF` döner.

**SPI1 APB2'dedir**, APB1ENR1 değil. Yanlış register'a yazarsanız çevre
birimi sessizce ölü kalır (saatsiz bir birimin register'ları 0 okunur).

**Tablo sonu iki bayta birden bakar.** `{0xFF, 0xFF}` bitiş işaretidir ama
`0xFF` aynı zamanda banka seçme register'ıdır ve tabloların **içinde**
defalarca geçer. Yalnızca register numarasına bakmak tabloyu ilk banka
değişiminde yarıda keser.

**CS pinleri önce YÜKSEK, sonra çıkış.** Ters sırada pin bir an 0 sürer ve
o kameranın CS'i kısa süreliğine seçili hale gelir.

**Makefile bayrak değişimini yakalar.** `make` yalnızca dosya zaman
damgalarına bakar; `main.c` değişmediği için `make COZUNURLUK=640` sessizce
**eski binary'yi** yüklerdi. `.ayarlar` dosyası bayrakları tutar ve `.o` ona
bağımlıdır, böylece ayar değişince yeniden derlenir. Sahada bu tuzağa
düşüldü: `GORUNTU_AKISI=1` yüklendi sanıldı, kart hâlâ eski kodu
çalıştırıyordu.

---

## 8. Belgelerle doğrulanmış gerçekler

| Gerçek | Kaynak |
|---|---|
| SPI1 tabanı `0x40013000` | RM0351 Tablo 1 |
| `SPI_CR2`: `FRXTH` bit 12, `DS[3:0]` bit 11:8 | RM0351 §42.6.2 |
| `SPI_CR1`: `BR[2:0]` bit 5:3 | RM0351 §42.6.1 |
| `RCC_APB2ENR` ofset `0x60`, `SPI1EN` bit 12 | RM0351 §6.4.21 |
| `I2C_CR2`: `AUTOEND` 25, `NBYTES` 23:16, `START` 13, `RD_WRN` 10 | RM0351 §39.7.2 |
| HCLK ≤ 16 MHz → **0 wait state** (Range 1) | RM0351 Tablo 11 |
| `TIMINGR=0x30420F13` → 100 kHz @ HSI16 | RM0351 §39.4.9 (test_i2c'de doğrulandı) |
| PA5=D13, PA6=D12, PA7=D11, PB6=D10, PC7=D9, PB8=D15, PB9=D14 | UM1724 Tablo 23 |
| PA5 = `TT_a` (3.6V) · PA6/PA7 = `FT_la` · PB8/PB9 = `FT_fl` · PC7 = `FT_l` | DS10198 Tablo 16 |
| USB'den 300 mA sınırı; `E5V` 500 mA; U4 regülatör 500 mA | UM1724 §7.5.2-7.5.4 |
| ArduChip register tablosu, SPI mode 0, azami 8 MHz, kukla bayt | ArduCAM-M-2MP Hardware Application Note §4-6 |
| OV2640 SCCB adresi sabit `0x60`/`0x61` (7-bit `0x30`) | aynı belge §3 |
| Çoklu kamera: I2C+SPI ortak, yalnızca CS ayrı | aynı belge §2.2, Şekil 3 |

ST belgeleri `../../test_i2c/docs/` altında (`bash indir.sh` ile inen).
ArduCAM belgesi depoda değil:
<https://blog.arducam.com/downloads/shields/ArduCAM_Mini_2MP_Camera_Shield_Hardware_Application_Note.pdf>

> `curl` st.com'da takılıyor, `wget` çalışıyor — ArduCAM için de `wget` kullanın.
