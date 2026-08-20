# embedded — STM32 bare-metal çalışma alanı

İki STM32 geliştirme kartı üzerinde **kütüphanesiz (register seviyesi)**
gömülü yazılım projeleri. HAL yok, LL yok, CMSIS yok, libc yok, ST'nin hazır
startup dosyası yok — vektör tablosundan linker script'e kadar her şey elle
yazılıyor.

## Donanım

| Kart | MCU | Chip ID | ST-Link seri no |
|---|---|---|---|
| STM32F4DISCOVERY | STM32F407VG | `0x413` | `066FFF565257867767154920` |
| NUCLEO-L476RG | STM32L476RG | `0x415` | `0669FF353637503457045439` |

İkisi de aynı anda USB'ye bağlı ve aynı USB kimliğiyle (`0483:374b`) görünür.
Bu yüzden her `Makefile`'da `SERIAL` değişkeni doludur — `st-flash` doğru
kartı seri numarasından bulur.

## Projeler

| Proje | Konu | Durum |
|---|---|---|
| [`test_blink/`](test_blink/) | LED yakıp söndürme, SysTick gecikme. Register kalıbının en sade hali; yeni proje yazarken startup ve linker script buradan kopyalanır. | ✅ |
| [`test_i2c/`](test_i2c/) | İki kart arası I2C. Discovery master (I2C v1, yoklama), Nucleo slave (I2C v2, kesme). Aynı çekirdek, farklı çevre birimi kuşağı. | ✅ çalışıyor |
| [`test_arducam_ov2640/`](test_arducam_ov2640/) | Tek Nucleo'ya **iki ArduCAM Mini 2MP** (OV2640). Ortak I2C + ortak SPI, yalnızca CS ayrı. | ✅ tek kamerayla çalışıyor (gerçek 320x240 JPEG fotoğraf) |
| [`test_dht22_ssd1306/`](test_dht22_ssd1306/) | **DHT22** sıcaklık/nem → **SSD1306 OLED**. Tek telli zamanlama protokolü + sayfa düzenli video RAM. | 🔧 kod hazır, donanım testi bekliyor |

## Hızlı başlangıç

```bash
cd test_arducam_ov2640
make probe           # bağlı ST-Link'leri gör
make flash           # yükle
make monitor         # çıktıyı izle
```

Her projenin kendi `README.md`'si kablolama, beklenen çıktı ve arıza ayıklama
tablosunu içerir.

## Araçlar

`arm-none-eabi-gcc` 13.2.1 · `stlink-tools` 1.8.0 (`st-flash`, `st-info`,
`st-util`, `st-trace`) · `pdftotext` · `wget` · `python3` (yalnızca stdlib)

## Belgeler

`test_i2c/docs/` boş gelir; `bash test_i2c/docs/indir.sh` ile ST'den iner
(repoya konmadı: 54 MB ve ST telifli). Bu depodaki **her register değeri ve
pin iddiası** o belgelerden doğrulanmıştır — yeni bir sayı eklemeden önce
aynısını yapın.

> `curl` st.com'dan PDF indiremiyor (HTTP/2 stream hatası, HTTP/1.1'de
> süresiz takılma). `wget` sorunsuz çalışıyor.

Ayrıntılı kurallar, kod stili ve ortam tuzakları için [`CLAUDE.md`](CLAUDE.md).
