# test_i2c — İki STM32 kartı arasında register seviyesi I2C

STM32F4 Discovery ile Nucleo-L476RG'yi I2C üzerinden konuşturur.
**Hiçbir kütüphane yok**: HAL yok, LL yok, CMSIS yok, libc yok. Sadece
donanım register'ları, elle yazılmış startup kodu ve kendi linker script'imiz.

| Kart | Rol | I2C nesli | Yöntem |
|---|---|---|---|
| STM32F407VG Discovery | **Master** (yönetici) | I2C v1 (SR1/SR2/CCR) | Yoklama (polling) |
| NUCLEO-L476RG | **Slave** (köle, adres `0x42`) | I2C v2 (TIMINGR/ISR/ICR) | Kesme (interrupt) |

İki kart aynı Cortex-M4 çekirdeğini kullanır ama **I2C birimleri farklı
nesildir**. Bu proje bunu bilerek yapıyor: iki `main.c`'yi yan yana koyunca
aynı işin iki farklı donanımda nasıl yapıldığı görünür.

---

## 1. Kablolama

Şu an kartlar birbirine bağlı değil. Çalışması için 3 tel + 2 direnç gerekir:

```
   F407 DISCOVERY                          NUCLEO-L476RG
   ┌─────────────┐                         ┌─────────────┐
   │             │                         │             │
   │  PB6 (SCL)  ├────────────┬────────────┤ PB8  (SCL)  │  CN5-10  "D15"
   │             │            │            │             │
   │  PB7 (SDA)  ├──────┬─────┼────────────┤ PB9  (SDA)  │  CN5-9   "D14"
   │             │      │     │            │             │
   │  GND        ├──────┼─────┼────────────┤ GND         │  CN6-6
   └─────────────┘      │     │            └─────────────┘
                        │     │
                      4.7k   4.7k
                        │     │
                        └──┬──┘
                           │
                         3V3        (iki karttan herhangi birinin 3V3'ü)
```

**Pull-up dirençleri isteğe bağlı değildir.** I2C hattı *açık drenaj*'dır:
cihazlar hattı yalnızca GND'ye çekebilir, 3.3V'a süremez. Hattı boştayken 1'e
çeken şey bu dirençlerdir. Kodda dahili pull-up'ları da açıyoruz ama onlar
~40 kΩ civarındadır — kısa kabloda bazen yeter, **güvenilir değildir**.
4.7 kΩ takın.

**GND bağlantısı da zorunludur.** Ortak toprak olmadan iki kartın "0" ve "1"
kavramları birbirine göre anlamsızdır.

> **Discovery notu** (UM1472 pin tablosundan doğrulandı): PB6 kart üstünde
> `SCL` netine bağlıdır (CS43L22 ses kodeği), PB7 tamamen serbesttir, PB9 ise
> `SDA` netidir. Yani SCL hattımız kodekle ortak. Sorun değil: kodeğin RESET
> ucu (PD4) reset sonrası LOW olduğundan çip reset'te kalır, I2C uçları
> yüksek empedanstadır. Adresi de 0x4A, bizimki 0x42.
>
> **Pratik sonuç:** PB6 kartın kendi I2C neti üzerinde olduğu için orada
> muhtemelen zaten bir pull-up var; **PB7 boş bir pin, oraya pull-up'ı
> kesinlikle siz vermelisiniz.**

---

## 2. Derleme ve yükleme

```bash
cd test_i2c

make                # ikisini de derle
make flash          # ikisini de yükle (önce slave, sonra master)

make flash-slave    # yalnızca Nucleo
make flash-master   # yalnızca Discovery
make monitor        # Nucleo'nun UART çıktısını izle
make probe          # bağlı ST-LINK'leri listele
make clean
```

Her iki kart da aynı USB kimliğiyle (`0483:374b`) görünür, bu yüzden
`st-flash`'a hangi kartı hedeflediğini söylemek gerekir. **Seri numaraları
Makefile'lara zaten yazılı**, dolayısıyla yanlış karta yazma riski yok:

| Kart | ST-LINK seri no |
|---|---|
| Discovery F407VG | `066FFF565257867767154920` |
| Nucleo L476RG | `0669FF353637503457045439` |

Kart değiştirirseniz `make probe` ile yeni numarayı öğrenip ilgili
Makefile'daki `SERIAL` satırını güncelleyin (veya `make flash SERIAL=...`).

---

## 3. Ne olacak?

Her 500 ms'de master şunları yapar:

1. Slave'in `LED_CTRL` register'ına 0/1 yazar → **Nucleo'nun LD2'si yanıp söner**
2. `ECHO` register'ına artan bir sayaç yazar
3. Tek istekte 4 register'ı birden okur (`WHO_AM_I`, `LED_CTRL`, `COUNTER`, `ECHO`)
4. Gelen `WHO_AM_I == 0x5A` ve `ECHO == yazdığı değer` mi diye doğrular

### Discovery'nin LED'leri = durum ekranı

| LED | Anlamı |
|---|---|
| 🟠 PD13 turuncu | Her turda yanıp söner → kod çalışıyor (kalp atışı) |
| 🟢 PD12 yeşil | Son tur **başarılı** — bağlantı sağlam |
| 🔴 PD14 kırmızı | Hata var (veri yanlış veya haberleşme koptu) |
| 🔵 PD15 mavi | Karşı taraftan hiç ses yok → genelde **kablo takılı değil** |

**Kablolar takılı değilken beklenen:** turuncu yanıp söner + kırmızı + mavi yanar.
**Takıldıktan sonra:** turuncu yanıp söner + yeşil yanar, Nucleo'nun LED'i 1 Hz blink.

### Nucleo'nun UART çıktısı

`make monitor` (115200 8N1, ST-LINK sanal COM portu üzerinden):

```
=== NUCLEO-L476RG  I2C SLAVE ===
Adres     : 0x42 (7-bit)
Pinler    : PB8=SCL, PB9=SDA (CN5)
Hiz       : 100 kHz, saat kaynagi HSI16
Bekleniyor: master'in ilk istegi...

islem 1  LED=1  ECHO=0x00  son yazma: reg 0x01 <- 0x01
islem 2  LED=1  ECHO=0x01  son yazma: reg 0x03 <- 0x01
islem 3  LED=1  ECHO=0x01  son yazma: reg 0x03 <- 0x01
```

İki `ttyACM` cihazı var; Nucleo hangisi emin değilseniz:
`ls -l /dev/serial/by-id/` veya `make monitor PORT=/dev/ttyACM1`.

---

## 4. Haberleşme protokolü

Slave'i gerçek bir I2C sensörü gibi tasarladık: içinde numaralı register'lar
var, önce hangisiyle ilgilendiğinizi söylüyor, sonra okuyor/yazıyorsunuz.
MPU6050, BME280 gibi ticari sensörlerin mantığı da tam olarak budur.

| Adres | İsim | Erişim | Açıklama |
|---|---|---|---|
| `0x00` | `WHO_AM_I` | R | Sabit `0x5A` — kimlik doğrulama |
| `0x01` | `LED_CTRL` | R/W | bit0 → Nucleo'nun LD2'si |
| `0x02` | `COUNTER` | R | Tamamlanan I2C işlem sayısı (canlılık) |
| `0x03` | `ECHO` | R/W | Yazılan değer aynen geri okunur (hat testi) |

```
Yazma : [START][0x42+W][register no][veri][STOP]
Okuma : [START][0x42+W][register no][TEKRAR-START][0x42+R][veri...][STOP]
                                     └─ arada STOP yok: hattın sahipliğini
                                        bırakmadan yön değiştiriyoruz
```
Okurken her bayttan sonra register işaretçisi kendiliğinden ilerler, böylece
tek istekte 4 register peş peşe alınabilir.

---

## 5. İki I2C nesli arasındaki fark

Kodun neden iki ayrı dosya olduğu buradan anlaşılır — F4 kodunu adresleri
değiştirip kopyalamak **işe yaramaz**, mantık farklıdır:

| Konu | STM32F407 (I2C v1) | STM32L476 (I2C v2) |
|---|---|---|
| Hız ayarı | `FREQ` + `CCR` + `TRISE` | tek register: `TIMINGR` |
| Durum bayrakları | `SR1` / `SR2` | `ISR` (tek register) |
| Bayrak temizleme | okuma sırası (önce SR1, sonra SR2) | `ICR`'ye 1 yazmak |
| Saat kaynağı | daima PCLK1 | seçilebilir (bu projede HSI16) |
| Çok baytlı okuma | `ACK`/`POS` dansı, uzunluğa göre 3 ayrı reçete | `NBYTES` ile doğrudan |

F4'teki çok baytlı okuma gerçekten zahmetlidir: son bayta NACK gönderip STOP
vermek gerekir, ama NACK kararı o bayt daha hatta gelirken alınmalıdır. Kod
bu yüzden uzunluğa göre üç ayrı reçete uygular (1 bayt / 2 bayt / 3+ bayt) ve
`main.c` içinde her birinin nedeni adım adım anlatılır. Reçetelerin kaynağı
ST'nin **AN2824** uygulama notudur (`docs/`).

---

## 6. Sorun giderme

| Belirti | Muhtemel sebep |
|---|---|
| Mavi + kırmızı yanıyor, yeşil hiç yanmıyor | Kablo yok/gevşek, GND bağlı değil, ya da Nucleo'ya kod yüklenmemiş |
| Turuncu bile yanmıyor | Discovery'ye kod yüklenmemiş veya kart beslenmiyor |
| Ara sıra kırmızı, ara sıra yeşil | Pull-up yok/zayıf, kablolar uzun veya hat gürültülü — 4.7k takın |
| Kırmızı sürekli, mavi sönük | Haberleşme var ama veri yanlış: hatta başka bir cihaz mı var? |
| Nucleo LED'i yanıp sönmüyor ama Discovery yeşil | Olamaz — yeşil zaten slave'in doğru cevabına bağlı |
| UART'ta çöp karakter | Yanlış baud. 115200 8N1 olmalı |
| `st-flash` "unknown chip id" | Yanlış seri numarası ya da kart uykuda; reset'e basıp tekrar deneyin |

Hattı gerçekten görmek isterseniz mantık analizörünü SCL/SDA/GND'ye takın;
100 kHz'de en ucuz analizör bile rahat okur.

---

## 7. Dosya yapısı

```
test_i2c/
├── Makefile                       üst seviye: ikisini birden derle/yükle
├── README.md                      bu dosya
├── docs/                          ST'den indirilen datasheet/manuel PDF'leri
├── f407vg_discovery/              MASTER
│   ├── main.c                     I2C v1 master, yoklama, bol yorumlu
│   ├── startup_stm32f407vg.s      elle yazılmış startup + vektör tablosu
│   ├── stm32f407vg.ld             linker script (1 MB Flash, 128 KB RAM)
│   └── Makefile
└── l476rg_nucleo/                 SLAVE
    ├── main.c                     I2C v2 slave, kesme tabanlı + UART log
    ├── startup_stm32l476rg.s      vektör tablosu IRQ 32'ye kadar uzatıldı
    ├── stm32l476rg.ld             linker script (1 MB Flash, 96 KB SRAM1)
    └── Makefile
```

Slave tarafında vektör tablosunun uzatılması şart: tablo **konuma göre**
çalışır, isme göre değil. `I2C1_EV_IRQHandler`'ın çağrılabilmesi için tam
olarak 47. sırada (16 çekirdek istisnası + IRQ 31) olması gerekir.

---

## 8. Hangi belge nerede?

`docs/` klasöründeki PDF'lerde bakılacak bölümler:

| Belge | İşe yarayan bölüm |
|---|---|
| RM0090 (F4 reference manual) | Bölüm 27 — I2C; Bölüm 8 — GPIO; Bölüm 6 — RCC |
| RM0351 (L4 reference manual) | Bölüm 39 — I2C (TIMINGR örnek tablosu dahil); Bölüm 6 — RCC |
| AN2824 | Master alıcı akış şemaları. Başlığı STM32F10xxx der ama I2C birimi F4'tekiyle aynı nesildir (v1), reçeteler birebir geçerlidir |
| UM1472 | Discovery şeması — hangi pin nereye bağlı |
| UM1724 | Nucleo-64 şeması — CN5/CN6/CN7/CN10 pin haritası |
| DS8626 / DS10198 | Elektriksel özellikler, pin alternatif fonksiyon tabloları |
