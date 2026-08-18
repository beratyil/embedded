# `test_arducam_ov2640` — iki kamera, tek mikrodenetleyici

**ArduCAM Mini 2MP** (ArduCAM-M-2MP) modülleri ile register seviyesinde
görüntü yakalama. Modülün üzerinde üç çip var:

| Çip | İş | Erişim |
|---|---|---|
| **OmniVision OV2640** | 2 MP CMOS sensör + JPEG kodlayıcı | **I2C (SCCB)** |
| **ArduChip** (CPLD) | Yakalama zamanlaması, FIFO denetimi | **SPI** |
| **AL422B** | 384 KB kare tamponu (FIFO) | ArduChip üzerinden |

Kutunun üzerinde *"Arducam Mini module camera shield 2MP for Arduino UNO
Mega2560 board"* yazan modül budur. Sensörün gerçek adı **OV2640**.

---

## 1. Ana soru: iki kamera aynı karta bağlanır mı?

**Evet.** Üstelik tahminle değil — ArduCAM kendi donanım notunda
(*Hardware Application Note* §2.2, Şekil 3) **tek host'a dört kamera**
bağlanmış resmi bir şema veriyor:

| Sinyal | Nasıl |
|---|---|
| `+5V`, `GND` | ortak |
| `SCL`, `SDA` | **ortak** — hepsi aynı I2C hattında |
| `MISO`, `MOSI`, `SCLK` | **ortak** |
| `CSn` | **ayrı** — CS0n, CS1n, CS2n, CS3n |

## 2. Peki I2C adres çakışması ne oldu?

OV2640'ın SCCB adresi fabrikada sabittir: `0x60` yazma / `0x61` okuma
(7-bit `0x30`). Adres seçme pini **yok**. İki sensör aynı hatta → aynı
adres. Normalde bu bir çakışmadır.

Kurtaran şey iş bölümü:

```
        ┌─────────────────────────────────────────────┐
        │  I2C  →  yalnızca SENSÖRÜ ayarlar            │
        │          (çözünürlük, JPEG, pozlama)         │
        │          Aynı ayarı istiyoruz → YAYIN yeter  │
        ├─────────────────────────────────────────────┤
        │  SPI  →  ASIL İŞ: yakalama tetiği, FIFO      │
        │          okuma, durum bayrakları             │
        │          CS ayrı → KAMERA BAŞINA BAĞIMSIZ    │
        └─────────────────────────────────────────────┘
```

Tek bir I2C yazması hattaki bütün OV2640'lara ulaşır. İki sensör de aynı
anda ACK çeker; açık drenaj hatta iki cihazın birlikte "0" çekmesi
elektriksel olarak sorunsuzdur. Bireysellik SPI tarafında.

### Bedeli — ve nasıl kapatıldığı

I2C'den **okuma** yaparsak iki sensör de aynı anda veriyi sürer. Aynı çip
oldukları için aynı değeri sürerler ve sonuç doğru çıkar — ama bir kamera
ölü/taksız ise bunu I2C'den **anlayamayız**.

Bu yüzden kodda:

> **Kamera varlık testi I2C'den değil, SPI'dan yapılır.**

Aşama 1 tam olarak bunu yapar: her kameranın ArduChip'ine ayrı ayrı yazıp
geri okur. CS ayrı olduğu için bu test kameraları tek tek eler.

### Sınır

İki kameraya **farklı** ayar veremezsiniz (biri 320x240, diğeri 640x480
gibi). Aynı yapılandırma yetiyorsa bu topoloji en az pinle en iyi çözüm.

Farklı ayar gerekirse iki yol var:

1. İkinci kamerayı **I2C3**'e alın — PC0/PC1 (CN8-6/CN8-5). İki tel
   farkı, garantili çözüm.
2. ArduChip'in `0x05`/`0x06` GPIO register'larındaki *sensor power down*
   bitiyle kameraları sırayla susturun. Daha az tel ama modülde o pinin
   gerçekten bağlı olduğunu doğrulamak gerekir.

---

## 3. Neden Nucleo, neden Discovery değil?

Elimizdeki iki karttan **NUCLEO-L476RG** seçildi:

| | NUCLEO-L476RG | F407 Discovery |
|---|---|---|
| Sanal COM portu | ✅ çalışıyor | ❌ **MCU'ya bağlı değil** (UM1472 §7.3.3) |
| SRAM | 128 KB | 192 KB |
| I2C / SPI birimi | 3 / 3 | 3 / 3 |

Discovery'nin ST-Link VCP uçları F407'nin USART'ına gitmiyor — görüntüyü
dışarı verecek yolu yok. Bu tek başına belirleyici.

F407'nin **DCMI** (paralel kamera arayüzü) birimi var ve normalde kamera
için çok daha uygun olurdu; ama ArduCAM Mini paralel veri yolunu dışarı
vermiyor, yalnızca SPI/I2C. DCMI kullanmak için çıplak bir OV2640 modülü
(24 pin DVP) gerekir — ayrı bir proje konusu.

---

## 4. Klasörler

| Klasör | İçerik |
|---|---|
| [`l476rg_nucleo/`](l476rg_nucleo/) | Firmware, kablolama, arıza ayıklama — **detaylı README burada** |

---

## 5. Komutlar

```bash
make                             # derle
make flash                       # Nucleo'ya yükle
make monitor                     # teşhis çıktısını izle
make yakala                      # görüntüleri .jpg olarak kaydet
make probe                       # bağlı ST-Link'leri listele
make clean

make COZUNURLUK=640 flash        # 160 / 320 (varsayılan) / 640
make GORUNTU_AKISI=1 flash       # ham JPEG'i UART'a dök
make BAUD=460800 flash           # hızlı akış
```

---

## 6. Bu projede öğrenilenler

**Aynı çekirdek, farklı çevre birimi kuşağı — yine.** `test_i2c` F4 ile L4
arasındaki I2C farkını göstermişti. Burada aynı ders SPI'da tekrarlanıyor:
L4'ün SPI'ında `DS[3:0]` ve `FRXTH` var, F4'te yok. `FRXTH` unutulursa
8 bit gönderiminde `RXNE` hiç kalkmaz ve program sessizce donar.

**Veri yolu genişliği register erişiminde önemli.** `SPI_DR`'ye 32-bit
yazmak `DS=8` iken FIFO'ya iki bayt iter. `volatile u32` alışkanlığının
sessizce bozulduğu nadir yerlerden biri.

**Belgelenmemiş donanım da vardır.** `ov2640_regs.h`'deki değerlerin büyük
kısmı OV2640'ın DSP bankasına ait ve OmniVision bunu **yayınlamamış**.
Deponun "her sayıyı belgeden doğrula" kuralının uygulanamadığı tek yer;
bu yüzden dosyanın başında açıkça yazıyor.

**Tamponlamamak bir tasarım kararıdır.** Darboğaz UART olduğu için görüntü
RAM'de biriktirilmiyor — FIFO'dan bir bayt okunup doğrudan UART'a
yazılıyor. 640x480 bile 128 KB SRAM'i hiç zorlamıyor.

---

## 7. Durum

**Çalışıyor — gerçek fotoğraf alındı.** Tek kamerayla uçtan uca
doğrulandı: geçerli JPEG, `file` çıktısı *"JPEG image data, JFIF standard
1.01, baseline, precision 8, **320x240**, components 3"*, tanınabilir ve
doğru pozlanmış renkli görüntü (~6.5 KB). İkinci kameranın besleme sorunu çözülünce
aynı kod iki kamerayı da sürecek (kod tek kamerayla da çalışacak şekilde
yazıldı).

Sahada üç tuzak ölçümle çözüldü — ayrıntısı
[`l476rg_nucleo/README.md`](l476rg_nucleo/README.md) §5'te:
VSYNC kutupluğu, kukla bayt, FIFO dolgusu.

## 8. Sonraki adımlar

- [ ] İkinci kameranın beslemesini çöz (harici 5V, GND ortak)
- [ ] Akış hızını artır: PLL ile 80 MHz + `BAUD=921600`
- [ ] FIFO okumasını DMA'ya devret (SPI RX → bellek), CPU'yu boşalt
- [ ] İki kameradan eşzamanlı yakalama: `AC_FIFO_BASLAT`'ı iki CS'e arka
      arkaya yazmak ~10 µs arayla tetikler — stereo için yeterince yakın
- [ ] Farklı ayar gerekirse ikinci kamerayı I2C3'e (PC0/PC1) taşı
