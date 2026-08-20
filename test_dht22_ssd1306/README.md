# `test_dht22_ssd1306` — sensörden ekrana

DHT22 sıcaklık/nem sensörünü okuyup değeri SSD1306 OLED ekrana yazan
register seviyesi uygulama. Depodaki ilk **çıktı cihazlı** proje: önceki
projelerde sonucu UART'tan okuyorduk, burada ekranda görüyoruz.

| Cihaz | Arayüz | Neden ilginç |
|---|---|---|
| **DHT22 / AM2302** | tek telli, kendine özgü | Çipte bu protokolü yapan **çevre birimi yok** — her kenar yazılımla ölçülür |
| **SSD1306 128x64** | I2C | Video RAM satır değil **sayfa** düzeninde; bir bayt = üst üste 8 piksel |

---

## 1. İki farklı zorluk, tek projede

Bu proje bilinçli olarak birbirine hiç benzemeyen iki cihazı birleştiriyor.

**DHT22 — zamanlama problemi.** Ne I2C ne SPI ne UART. Tek telli, tamamen
darbe genişliğine dayalı bir protokol:

```
biz:     ──┐                    ┌──────────────────────────
           └────── ≥1 ms ───────┘   başlat işareti

sensör:  ─────────┐      ┌──────┐   ┌───┐  ┌──────┐
                  └ 80µs ┘ 80µs └50µ┘26µ└50┘  70µs  ...
                    cevap        bit=0      bit=1
```

Bitin değeri HIGH süresinde saklı: 26-28 µs → `0`, 70 µs → `1`. Eşik ~50 µs.
Bunu güvenilir ölçmek için TIM2 1 MHz'e bölünüp serbest çalışan bir
**mikrosaniye sayacı** yapıldı. "Döngüde sayarak" ölçmek derleyici
optimizasyonuna bağımlı olurdu — `-O2`'yi `-O0` yapınca bozulan cinsten.

**SSD1306 — bellek düzeni problemi.** Protokol basit (I2C), zor olan
video RAM'in düzeni: satır satır değil **sayfa** halinde adreslenir.
Bir bayt ekranda üst üste 8 pikseldir.

```
   sayfa 0  ├─ bayt bit 0  (en üst piksel)
            ├─ bayt bit 1
            │      ...
            └─ bayt bit 7
   sayfa 1  ├─ ...
```

Font tablosu da aynı düzende seçildi (bayt = sütun, bit = satır), bu yüzden
karakter baytları bit döndürmeden doğrudan çerçeve tamponuna yazılabiliyor.
Font satır tabanlı olsaydı her karakter için bit döndürmek gerekirdi.

---

## 2. Tasarım kararları

**Çerçeve tamponu.** Ekran doğrudan çizilmiyor; RAM'de 1024 baytlık tampon
hazırlanıp tek seferde gönderiliyor. Yarım çizilmiş ara görüntü (titreme)
oluşmuyor.

**Tek font, tam sayı ölçek.** Büyük sıcaklık için ikinci bir font tablosu
taşımak yerine 5x7 font tam sayı katlarla büyütülüyor (1x etiket, 2x nem,
3x sıcaklık). Ara ölçek (1.5x) piksel ızgarasına oturmadığı için harfleri
tırtıklaştırırdı.

**Kayan nokta yok.** DHT22 değerleri zaten onda bir çözünürlükte tam sayı
geliyor (234 = 23.4 °C). Float'a çevirmek yazılım float kütüphanesini
projeye sokardı — `-nostdlib` ile çalışan bir projede gereksiz yük.

**Ekran adresi otomatik bulunuyor.** Modüllerin çoğu `0x3C`, bazıları
`0x3D`. İkisi de denenir. Ekran hiç bulunamazsa program durmaz, ölçümleri
UART'a basmaya devam eder — sensörün çalışıp çalışmadığı yine görülür.

**Hata kodları ayrıştırılmış.** Tek bir "okuma başarısız" yerine
`CEVAP YOK` / `ZAMAN ASIMI` / `SAGLAMA HATASI` ayrı ayrı raporlanır. Her
biri protokolün farklı bir adımında takıldığımızı söyler, yani arızanın
kabloda mı zamanlamada mı olduğunu doğrudan gösterir.

---

## 3. Neden Nucleo?

Yine NUCLEO-L476RG:

- Sanal COM portu çalışıyor (Discovery'de MCU'ya bağlı değil, UM1472 §7.3.3)
- Ekran zaten çıktı veriyor ama **ekran çalışmazsa** UART tek teşhis yolumuz
- I2C1 (PB8/PB9) kalıbı bu depoda iki kez sahada doğrulandı

Discovery bu iş için de kullanılabilirdi, ama ekran kurulumu tutmazsa
hiçbir geri bildirim alamazdık.

---

## 4. Klasörler

| Klasör | İçerik |
|---|---|
| [`l476rg_nucleo/`](l476rg_nucleo/) | Firmware, kablolama, arıza ayıklama — **detaylı README burada** |

---

## 5. Komutlar

```bash
make                      # derle
make flash                # Nucleo'ya yükle
make monitor              # UART çıktısını izle
make probe                # bağlı ST-Link'leri listele
make clean

make BAUD=230400 flash    # UART hızı
make ARALIK=5000 flash    # ölçüm aralığı ms (asgari 2000)
```

`ARALIK` 2000'in altına inemez — derleme zamanında `#error` ile engellenir.
DHT22 datasheet'i 2 saniyeden sık ölçümü yasaklar.

---

## 6. Durum

**Kod tamamlandı, donanımda henüz denenmedi.** Derleme sıfır uyarı.

Doğrulanacaklar:

- [ ] SSD1306 adres taraması (0x3C mi 0x3D mi)
- [ ] Ekran yönü (ters çıkarsa `SSD_SEG_TERS`/`SSD_COM_TERS`)
- [ ] Panel boyutu — kod 128x64 varsayıyor, 128x32 ise iki satır değişecek
- [ ] DHT22 bit eşiği (50 µs) sahada yeterli mi
- [ ] Pull-up: hazır modülde dahili yeterli mi, çıplak sensörde direnç şart

Negatif sıcaklık kodlaması **belgeyle doğrulanamadı** — ayrıntısı
[`l476rg_nucleo/README.md`](l476rg_nucleo/README.md) §7'de.
