/*
 * ============================================================================
 *  main.c  --  NUCLEO-L476RG  |  IKI ADET ArduCAM Mini 2MP (OV2640)
 * ============================================================================
 *
 *  HEDEF DONANIM : STM32L476RG, Cortex-M4F, 1 MB Flash, 96 KB SRAM1 + 32 KB SRAM2
 *  KART          : NUCLEO-L476RG (MB1136)
 *  CEVRE BIRIM   : 2 x ArduCAM-M-2MP (OmniVision OV2640 + ArduChip + AL422B FIFO)
 *
 *  KURAL: Hicbir kutuphane yok (HAL / LL / CMSIS / libc yok). Sadece register.
 *
 * ----------------------------------------------------------------------------
 *  KABLOLAMA  --  hepsi ARDUINO basligi CN5 uzerinde, tek sira
 * ----------------------------------------------------------------------------
 *      ArduCAM (her ikisi de)        NUCLEO-L476RG
 *      ----------------------        -------------
 *      +5V   ---------------------   CN6-5  (+5V)      <-- 3.3V DEGIL, asagiya bak
 *      GND   ---------------------   CN6-6  (GND)
 *      SCL   ---------------------   PB8    CN5-10 / D15
 *      SDA   ---------------------   PB9    CN5-9  / D14
 *      SCK   ---------------------   PA5    CN5-6  / D13
 *      MISO  ---------------------   PA6    CN5-5  / D12
 *      MOSI  ---------------------   PA7    CN5-4  / D11
 *
 *      CS (kamera 0) -------------   PB6    CN5-3  / D10
 *      CS (kamera 1) -------------   PC7    CN5-2  / D9
 *
 *  YANI: CS DISINDA HER SEY ORTAK. Iki kamera ayni I2C hattina ve ayni SPI
 *  hattina paralel baglanir; yalnizca CS pinleri ayridir. Bu, ArduCAM'in kendi
 *  donanim notundaki resmi coklu-kamera baglantisinin (ArduCAM-M-2MP Hardware
 *  Application Note, Bolum 2.2, Sekil 3) aynisidir.
 *
 * ----------------------------------------------------------------------------
 *  NEDEN ORTAK I2C CALISIYOR?  (ilk bakista calismamasi gerekir)
 * ----------------------------------------------------------------------------
 *  OV2640'in SCCB adresi FABRIKADA SABITTIR: 0x60 yazma / 0x61 okuma, yani
 *  7-bit 0x30. Adres secme pini yoktur. Iki sensor ayni hatta -> ayni adres.
 *  Normalde bu bir cakismadir.
 *
 *  Kurtaran sey is bolumudur:
 *
 *      I2C  -> yalnizca SENSORU ayarlar (cozunurluk, JPEG, pozlama).
 *              Ikisine de AYNI ayari yaziyoruz, yani yayin (broadcast) yapmis
 *              oluyoruz. Iki sensor de ayni anda ACK ceker; acik drenaj
 *              hatta iki cihazin birlikte "0" cekmesi elektriksel olarak
 *              sorunsuzdur.
 *
 *      SPI  -> ASIL IS burada: yakalama tetigi, FIFO okuma, durum bayraklari.
 *              Bunlar ArduChip register'laridir ve CS ayri oldugu icin
 *              KAMERA BASINA BAGIMSIZDIR.
 *
 *  BEDELI: I2C'den OKUMA yaparsak iki sensor de ayni anda veriyi surer. Ayni
 *  cip olduklari icin ayni degeri surerler ve sonuc dogru cikar -- ama bir
 *  kamera olu/taksiz ise bunu I2C'den ANLAYAMAYIZ. Bu yuzden:
 *
 *      >>> KAMERA VARLIK TESTI I2C'DEN DEGIL, SPI'DAN YAPILIR. <<<
 *
 *  Asagidaki asama 1 tam olarak bunu yapar: her kameranin ArduChip'ine ayri
 *  ayri yazip geri okur. CS ayri oldugu icin bu test kamerayi tek tek eler.
 *
 *  SINIR: Iki kameraya FARKLI ayar veremezsiniz (ornegin biri 320x240, digeri
 *  640x480). Ayni yapilandirma yetiyorsa bu topoloji en az pinle en iyi cozum.
 *  Farkli ayar gerekirse ikinci kamerayi I2C3'e (PC0/PC1, CN8-6/CN8-5) alin.
 *
 * ----------------------------------------------------------------------------
 *  BESLEME  --  5V, 3.3V DEGIL
 * ----------------------------------------------------------------------------
 *  Modulun uzerinde 3.3V LDO vardir; girisine 3.3V verirseniz dropout'a girer
 *  ve cikis ~2.1V'a duser, kamera calismaz. VCC mutlaka +5V (CN6-5).
 *
 *  I/O seviyesi 3.3V'tur, seviye cevirici GEREKMEZ. Gerekce: ArduCAM ayni
 *  donanim notunda host olarak Raspberry Pi ve BeagleBone Black listeler;
 *  ikisi de 3.3V'tur ve 5V toleransi yoktur. 5V I/O olsa o kartlar yanardi.
 *
 *  AKIM: iki modul kabaca ~200 mA ceker. UM1724'e gore USB'den beslenen
 *  Nucleo'nun toplam butcesi 300 mA'dir (JP1 OFF). Sinirdasiniz. Kamera
 *  reset atiyor / rastgele donuyorsa ilk supheli budur: CN7-6 (E5V) uzerinden
 *  harici 5V verin (500 mA).
 *
 * ----------------------------------------------------------------------------
 *  LD2 KULLANILAMIYOR
 * ----------------------------------------------------------------------------
 *  PA5 hem SPI1_SCK hem de kart uzerindeki yesil LD2'dir. SPI saati akarken
 *  LED kirpisir; bunu bir durum LED'i olarak kullanamayiz. Karsiliginda
 *  bedava bir "SPI trafigi var" gostergesi kazandik: yakalama sirasinda LD2
 *  hic kirpismiyor ise SPI hic calismiyor demektir.
 *
 *  Not: PA5 datasheet'te TT_a yapisindadir, yani yalnizca 3.6V toleranslidir
 *  (PA6/PA7/PB8/PB9/PC7 ise FT = 5V toleransli). PA5 cikis oldugu icin
 *  karsidan gerilim gelmez, sorun degildir -- ama bu pine baska bir sey
 *  baglayacaksaniz aklinizda olsun.
 *
 * ----------------------------------------------------------------------------
 *  PROGRAM NE YAPAR
 * ----------------------------------------------------------------------------
 *   Asama 1  SPI baglantisi     : her kameranin ArduChip'ine 0x55/0xAA yazip
 *                                 geri okur + surum register'ini (0x40) dogrular
 *   Asama 2  I2C baglantisi     : OV2640 kimligini okur (PIDH/PIDL)
 *   Asama 3  Sensor kurulumu    : JPEG tablolarini YAYIN olarak yazar
 *   Asama 4  Surekli dongu      : sirayla her kameradan bir kare yakalar,
 *                                 FIFO uzunlugunu ve ilk baytlari raporlar
 *
 *  Asama 4'te ilk iki bayt 0xFF 0xD8 (JPEG SOI isareti) cikiyorsa sistem
 *  uctan uca calisiyor demektir -- bilgisayarda hicbir sey calistirmadan,
 *  yalnizca seri terminale bakarak dogrulanabilir.
 *
 *  GORUNTU_AKISI=1 ile derlenirse ayrica ham JPEG baytlarini UART'a doker;
 *  tools/jpeg_al.py bunlari .jpg dosyasina cevirir.
 *
 * ----------------------------------------------------------------------------
 *  NEDEN HIC KESME YOK?
 * ----------------------------------------------------------------------------
 *  Bu programda tek bir is akisi var ve her adim bir oncekini bekliyor:
 *  yakala -> bitmesini bekle -> oku -> gonder. Bekleyecek baska is olmadigi
 *  icin kesme kurmak kodu sadece karmasiklastirirdi. (test_i2c'deki slave
 *  ise kesme kullanmak ZORUNDAYDI: master ne zaman konusacagini kendi secer.)
 *
 *  Ayrica SPI master oldugumuz icin veri kaybi riski yok: yazilim yavas
 *  kalirsa saat de yavaslar, tasma olmaz. Yalnizca is uzar.
 * ============================================================================
 */

#include "ov2640_regs.h"


/* ---------------------------------------------------------------------------
 * 0) DERLEME SECENEKLERI  --  Makefile'dan -D ile ezilebilir
 * ------------------------------------------------------------------------- */

/* Yakalanan JPEG baytlarini UART'a dokelim mi?
 *   0 = hayir, yalnizca teshis raporu (varsayilan; ilk denemede bunu kullanin)
 *   1 = evet, tools/jpeg_al.py ile dosyaya cevirin
 * Kullanim:  make GORUNTU_AKISI=1 flash                                     */
#ifndef GORUNTU_AKISI
#define GORUNTU_AKISI  0
#endif

/* Cozunurluk: 160, 320 veya 640.  Kullanim:  make COZUNURLUK=640 flash      */
#ifndef COZUNURLUK
#define COZUNURLUK     320
#endif

/* UART hizi. Akis kullanacaksaniz Makefile'da BAUD=460800 yapin.
 * 16 MHz'de BRR yuvarlama hatasi: 115200 -> %0.08, 460800 -> %0.36 (ikisi de
 * guvenli), 921600 -> %2.1 (SINIRDA, onerilmez).                            */
#ifndef UART_BAUD
#define UART_BAUD      115200U
#endif

#if   COZUNURLUK == 160
  #define COZ_TABLO  OV2640_160x120_JPEG
  #define COZ_METIN  "160x120"
#elif COZUNURLUK == 320
  #define COZ_TABLO  OV2640_320x240_JPEG
  #define COZ_METIN  "320x240"
#elif COZUNURLUK == 640
  #define COZ_TABLO  OV2640_640x480_JPEG
  #define COZ_METIN  "640x480"
#else
  #error "COZUNURLUK yalnizca 160, 320 veya 640 olabilir"
#endif


/* ---------------------------------------------------------------------------
 * 1) TEMEL TIPLER
 * ------------------------------------------------------------------------- */
typedef unsigned char          u8;
typedef unsigned int           u32;
typedef volatile unsigned int  vu32;
typedef volatile unsigned char vu8;

#define REG32(adres)  (*(vu32 *)(adres))
#define REG8(adres)   (*(vu8  *)(adres))


/* ---------------------------------------------------------------------------
 * 2) HAFIZA HARITASI  (RM0351, Tablo 1)
 * ---------------------------------------------------------------------------
 *   0x4000 0000  APB1  <- I2C1 (0x4000 5400), USART2 (0x4000 4400)
 *   0x4001 0000  APB2  <- SPI1 (0x4001 3000)      <-- SPI1 APB2'DEDIR
 *   0x4002 0000  AHB1  <- RCC  (0x4002 1000)
 *   0x4800 0000  AHB2  <- GPIO portlari           (F4'te AHB1'deydi!)
 *
 *  SPI1'in APB2'de olmasi onemli: saat kapisi RCC_APB2ENR'dedir, I2C/USART
 *  gibi APB1ENR1'de DEGILDIR. Yanlis register'a yazarsaniz cevre birimi
 *  sessizce olu kalir (saatsiz bir cevre biriminin register'lari 0 okunur).
 * ------------------------------------------------------------------------- */
#define RCC_BASE      0x40021000U
#define GPIOA_BASE    0x48000000U       /* PA2 UART, PA5/6/7 SPI1            */
#define GPIOB_BASE    0x48000400U       /* PB6 CS0, PB8/PB9 I2C1             */
#define GPIOC_BASE    0x48000800U       /* PC7 CS1                           */
#define I2C1_BASE     0x40005400U
#define SPI1_BASE     0x40013000U
#define USART2_BASE   0x40004400U
#define SYSTICK_BASE  0xE000E010U


/* ---------------------------------------------------------------------------
 * 3) RCC  --  Saat kapilari, saat kaynaklari, sistem saati
 * ------------------------------------------------------------------------- */
#define RCC_CR        REG32(RCC_BASE + 0x00U)
#define RCC_CFGR      REG32(RCC_BASE + 0x08U)  /* sistem saati secimi        */
#define RCC_APB1RSTR1 REG32(RCC_BASE + 0x38U)
#define RCC_APB2RSTR  REG32(RCC_BASE + 0x40U)
#define RCC_AHB2ENR   REG32(RCC_BASE + 0x4CU)  /* GPIO saatleri              */
#define RCC_APB1ENR1  REG32(RCC_BASE + 0x58U)  /* I2C1, USART2               */
#define RCC_APB2ENR   REG32(RCC_BASE + 0x60U)  /* SPI1                       */
#define RCC_CCIPR     REG32(RCC_BASE + 0x88U)  /* cevre birimi saat kaynagi  */

#define RCC_HSION     (1U << 8)
#define RCC_HSIRDY    (1U << 10)

#define RCC_GPIOAEN   (1U << 0)
#define RCC_GPIOBEN   (1U << 1)
#define RCC_GPIOCEN   (1U << 2)

#define RCC_USART2EN  (1U << 17)             /* APB1ENR1 bit 17             */
#define RCC_I2C1EN    (1U << 21)             /* APB1ENR1 bit 21             */
#define RCC_I2C1RST   (1U << 21)
#define RCC_SPI1EN    (1U << 12)             /* APB2ENR  bit 12 (RM0351 6.4.21)*/
#define RCC_SPI1RST   (1U << 12)             /* APB2RSTR bit 12             */

/* CFGR: SW[1:0] sistem saatini secer, SWS[3:2] gerceklesen secimi gosterir.
 *    00 = MSI (reset degeri)   01 = HSI16   10 = HSE   11 = PLL            */
#define CFGR_SW_HSI16   (1U << 0)
#define CFGR_SWS_MASK   (3U << 2)
#define CFGR_SWS_HSI16  (1U << 2)

/* CCIPR: bit 3:2 USART2SEL, bit 13:12 I2C1SEL. 10 = HSI16 */
#define CCIPR_USART2SEL_HSI16  (2U << 2)
#define CCIPR_I2C1SEL_HSI16    (2U << 12)

#define HSI16_HZ      16000000U


/* ---------------------------------------------------------------------------
 * 4) GPIO  --  uc porta birden dokundugumuz icin taban adresli makrolar
 * ---------------------------------------------------------------------------
 * test_i2c'de her port icin ayri makro yazilmisti (tek portla calisiliyordu).
 * Burada A, B ve C portlarina birden dokunuyoruz; ayni satirlari uc kez
 * yazmak yerine tabani parametre aliyoruz. Uretilen kod aynidir: taban
 * adresler derleme zamaninda sabittir.
 *
 * AFR hilesi: alternatif fonksiyon secimi iki register'a bolunmustur,
 * AFRL (0x20) pin 0..7, AFRH (0x24) pin 8..15. Pin numarasinin 3. bitini
 * kaydirarak dogru register'i sececek tek bir makro yaziyoruz.
 * ------------------------------------------------------------------------- */
#define GPIO_MODER(t)    REG32((t) + 0x00U)
#define GPIO_OTYPER(t)   REG32((t) + 0x04U)
#define GPIO_OSPEEDR(t)  REG32((t) + 0x08U)
#define GPIO_PUPDR(t)    REG32((t) + 0x0CU)
#define GPIO_BSRR(t)     REG32((t) + 0x18U)
#define GPIO_AFR(t, pin) REG32((t) + 0x20U + (((pin) >> 3) * 4U))

/* MODER degerleri: 00 giris, 01 cikis, 10 alternatif fonksiyon, 11 analog */
#define MODER_CIKIS   1U
#define MODER_AF      2U

#define AF4_I2C1      4U
#define AF5_SPI1      5U
#define AF7_USART2    7U

/* --- Pin atamalari (UM1724 Tablo 23 ile dogrulandi) --- */
#define PIN_TX        2U     /* PA2  USART2_TX  -> ST-LINK sanal COM         */
#define PIN_SCK       5U     /* PA5  SPI1_SCK   CN5-6  D13  (= LD2)          */
#define PIN_MISO      6U     /* PA6  SPI1_MISO  CN5-5  D12                   */
#define PIN_MOSI      7U     /* PA7  SPI1_MOSI  CN5-4  D11                   */
#define PIN_CS0       6U     /* PB6  kamera 0 CS  CN5-3  D10                 */
#define PIN_CS1       7U     /* PC7  kamera 1 CS  CN5-2  D9                  */
#define PIN_SCL       8U     /* PB8  I2C1_SCL   CN5-10 D15                   */
#define PIN_SDA       9U     /* PB9  I2C1_SDA   CN5-9  D14                   */


/* ---------------------------------------------------------------------------
 * 5) SysTick  --  gecikme olcumu (kesmesiz, COUNTFLAG yoklamali)
 * ---------------------------------------------------------------------------
 * test_blink'teki kalibin aynisi. COUNTFLAG (bit 16) sayac 0'a her
 * ulastiginda 1 olur ve CTRL OKUNDUGUNDA kendiliginden temizlenir. Bu
 * "okuyunca silinir" davranisi onemlidir: bayragi ayrica temizlememiz
 * gerekmez, ama CTRL'yi baska bir amacla okursak bir tik kaybederiz.
 * ------------------------------------------------------------------------- */
#define STK_CTRL      REG32(SYSTICK_BASE + 0x00U)
#define STK_LOAD      REG32(SYSTICK_BASE + 0x04U)
#define STK_VAL       REG32(SYSTICK_BASE + 0x08U)

#define STK_ENABLE    (1U << 0)
#define STK_CLKSOURCE (1U << 2)              /* 1 = islemci saati (AHB)      */
#define STK_COUNTFLAG (1U << 16)


/* ---------------------------------------------------------------------------
 * 6) SPI1  --  ArduChip ile konusma yolu  (RM0351, Bolum 42.6)
 * ---------------------------------------------------------------------------
 * STM32L4'un SPI'i F4'unkinden FARKLIDIR ve iki tuzagi vardir:
 *
 *  TUZAK 1 -- DS[3:0] ve FRXTH (CR2)
 *      L4'te SPI 4..16 bit arasi her veri boyunu destekler ve 32 baytlik
 *      FIFO'su vardir. Varsayilan RXNE esigi 16 BITTIR: 8 bit gonderirseniz
 *      RXNE HIC KALKMAZ ve program sonsuza kadar bekler. FRXTH=1 esigi
 *      8 bite indirir. F4'te bu bitlerin ikisi de yoktur.
 *
 *  TUZAK 2 -- DR'ye erisim GENISLIGI
 *      DR'ye 32-bit yazmak, DS=8 iken FIFO'ya IKI bayt birden iter.
 *      Bu yuzden asagida DR'ye daima 8-BIT isaretci ile erisiyoruz.
 *      Bu, "volatile u32 ile yaz" aliskanliginin sessizce bozuldugu
 *      nadir yerlerden biridir.
 *
 * ArduChip tarafi (Hardware Application Note, Bolum 4):
 *      - SPI mode 0, azami 8 MHz  ("do not over clock the maximum 8MHz")
 *      - Komut bayti: bit[7] 1=yazma / 0=okuma, bit[6:0] register adresi
 *      - CS, tum islem boyunca asagida kalmalidir
 *
 *  NOT: Belge "fixed SPI mode 0 with POL = 0 and PHA = 1" der; bu ifade
 *  kendi icinde celiskilidir (mode 0 = CPOL 0, CPHA 0). ArduCAM'in kendi
 *  Arduino kutuphanesi SPI_MODE0 kullanir, biz de onu kullaniyoruz.
 * ------------------------------------------------------------------------- */
#define SPI1_CR1      REG32(SPI1_BASE + 0x00U)
#define SPI1_CR2      REG32(SPI1_BASE + 0x04U)
#define SPI1_SR       REG32(SPI1_BASE + 0x08U)
#define SPI1_DR8      REG8 (SPI1_BASE + 0x0CU)   /* 8-BIT erisim: TUZAK 2   */

#define SPI_CR1_CPHA     (1U << 0)
#define SPI_CR1_CPOL     (1U << 1)
#define SPI_CR1_MSTR     (1U << 2)
#define SPI_CR1_BR_DIV2  (0U << 3)           /* 16 MHz / 2 = 8 MHz          */
#define SPI_CR1_SPE      (1U << 6)
#define SPI_CR1_SSI      (1U << 8)
#define SPI_CR1_SSM      (1U << 9)

#define SPI_CR2_DS_8BIT  (7U << 8)           /* DS[3:0] = 0111 -> 8 bit     */
#define SPI_CR2_FRXTH    (1U << 12)          /* RXNE esigi 8 bit: TUZAK 1   */

#define SPI_SR_RXNE      (1U << 0)
#define SPI_SR_TXE       (1U << 1)
#define SPI_SR_BSY       (1U << 7)


/* ---------------------------------------------------------------------------
 * 7) I2C1  --  OV2640'in SCCB hatti  (RM0351, Bolum 39.7)
 * ---------------------------------------------------------------------------
 * test_i2c'de bu birimi SLAVE olarak kullanmistik; burada MASTER'iz.
 * Master tarafinda is CR2 register'inda toplanir: adres, yon, bayt sayisi
 * ve START tek yazmada verilir, AUTOEND ise son bayttan sonra STOP'u
 * donanimin kendisine urettirir.
 * ------------------------------------------------------------------------- */
#define I2C1_CR1      REG32(I2C1_BASE + 0x00U)
#define I2C1_CR2      REG32(I2C1_BASE + 0x04U)
#define I2C1_TIMINGR  REG32(I2C1_BASE + 0x10U)
#define I2C1_ISR      REG32(I2C1_BASE + 0x18U)
#define I2C1_ICR      REG32(I2C1_BASE + 0x1CU)
#define I2C1_RXDR     REG32(I2C1_BASE + 0x24U)
#define I2C1_TXDR     REG32(I2C1_BASE + 0x28U)

#define I2C_CR1_PE        (1U << 0)

/* CR2 alanlari (RM0351 39.7.2 ile dogrulandi) */
#define I2C_CR2_RD_WRN    (1U << 10)         /* 1 = okuma                   */
#define I2C_CR2_START     (1U << 13)
#define I2C_CR2_AUTOEND   (1U << 25)
#define I2C_CR2_NBYTES(n) (((u32)(n) & 0xFFU) << 16)

#define I2C_ISR_TXIS      (1U << 1)
#define I2C_ISR_RXNE      (1U << 2)
#define I2C_ISR_NACKF     (1U << 4)
#define I2C_ISR_STOPF     (1U << 5)
#define I2C_ISR_BUSY      (1U << 15)

#define I2C_ICR_NACKCF    (1U << 4)
#define I2C_ICR_STOPCF    (1U << 5)
#define I2C_ICR_BERRCF    (1U << 8)
#define I2C_ICR_ARLOCF    (1U << 9)

/* OV2640 SCCB adresi. Belgede 8-bit bicimde "0x60 yazma / 0x61 okuma" diye
 * verilir; STM32 donanimi 7-bit ister, dolayisiyla 0x60 >> 1 = 0x30.
 * Bu adres DEGISTIRILEMEZ (adres secme pini yok) -- dosya basindaki
 * "ortak I2C" tartismasinin cikis noktasi budur. */
#define OV2640_ADRES   0x30U

/* Yoklamali beklemelerde sonsuz donguye girmemek icin kaba bir tavan.
 * 16 MHz'de bu dongu ~50 ms eder; 100 kHz I2C'de en uzun bayt ~90 us surer,
 * yani genis bir emniyet payi. Amac hiz degil, ASILMAMA garantisidir:
 * kablo takili degilse program donmak yerine hata dondurup rapor eder. */
#define I2C_ZAMAN_ASIMI  200000U


/* ---------------------------------------------------------------------------
 * 8) USART2  --  ST-LINK sanal COM portu  (RM0351, Bolum 40.8)
 * ------------------------------------------------------------------------- */
#define USART2_CR1    REG32(USART2_BASE + 0x00U)
#define USART2_BRR    REG32(USART2_BASE + 0x0CU)
#define USART2_ISR    REG32(USART2_BASE + 0x1CU)
#define USART2_TDR    REG32(USART2_BASE + 0x28U)

#define USART_CR1_UE  (1U << 0)
#define USART_CR1_TE  (1U << 3)
#define USART_ISR_TXE (1U << 7)


/* ---------------------------------------------------------------------------
 * 9) ArduChip REGISTER HARITASI
 * ---------------------------------------------------------------------------
 * Kaynak: ArduCAM-M-2MP Hardware Application Note, Tablo 1.
 * DIKKAT: Bunlar OV2640'in register'lari DEGILDIR. ArduChip, sensor ile
 * FIFO arasinda duran ayri bir CPLD'dir ve SPI'dan erisilir. OV2640'a ise
 * yalnizca I2C'den ulasilir. Iki adres alanini karistirmak bu projedeki en
 * kolay hata kaynagidir.
 * ------------------------------------------------------------------------- */
#define AC_TEST1        0x00U   /* RW  serbest test register'i (yaz/oku)     */
#define AC_CPLD_RESET   0x07U   /* RW  CPLD yazilim reset'i -- BELGEDE YOK    */
#define AC_FRAMES       0x01U   /* RW  bit[2:0] yakalanacak kare sayisi      */
#define AC_TIM          0x03U   /* RW  sensor arayuz zamanlamasi             */
#define AC_FIFO         0x04U   /* RW  FIFO kontrol (temizle / baslat)       */
#define AC_BURST_READ   0x3CU   /* RO  toplu FIFO okuma komutu               */
#define AC_SINGLE_READ  0x3DU   /* RO  tek bayt FIFO okuma                   */
#define AC_REV          0x40U   /* RO  surum; 2MP modelinde sabit 0x40       */
#define AC_TRIG         0x41U   /* RO  bit0 VSYNC, bit3 yakalama bitti       */
#define AC_FIFO_SIZE1   0x42U   /* RO  uzunluk[7:0]                          */
#define AC_FIFO_SIZE2   0x43U   /* RO  uzunluk[15:8]                         */
#define AC_FIFO_SIZE3   0x44U   /* RO  uzunluk[18:16]                        */

/* AC_TIM (0x03) bitleri -- donanim notu Tablo 1:
 *     bit0 HSYNC kutuplugu   bit1 VSYNC kutuplugu
 *     bit3 veri gecikmesi    bit4 FIFO modu
 *
 * DOGRU DEGER OLCUMLE BULUNDU (tahminle degil). Sekiz aday deger tek tek
 * denenip FIFO uzunluguna bakildi:
 *
 *     TIM=0x00 -> 6152 bayt, gecerli JPEG      TIM=0x02 -> 8 bayt
 *     TIM=0x08 -> 6152 bayt (bir bayt kaymis)  TIM=0x0A -> 8 bayt
 *     TIM=0x10 -> 6152 bayt, gecerli JPEG      TIM=0x12 -> 8 bayt
 *     TIM=0x18 -> 6152 bayt (bir bayt kaymis)  TIM=0x1A -> 8 bayt
 *
 * Desen tek bir bite indi: bit1 SET ise yakalama aninda kesiliyor (8 bayt),
 * TEMIZ ise kare duzgun geliyor. Yani bu modulde VSYNC AKTIF YUKSEK'tir.
 *
 * bit3 (veri gecikmesi) acilirsa akis bir bayt kayiyor -- JPEG'in bas
 * isareti bozuluyor, dolayisiyla 0 kalmali.
 * bit4 (FIFO modu) sonucu degistirmiyor; bu revizyonda etkisiz.
 *
 * TUZAK: ArduCAM'in kutuphanesi burada VSYNC_LEVEL_MASK (0x02) yazar ve
 * yanina "VSYNC is active HIGH" yorumunu koyar. Bizim modulde 0x02 yazmak
 * yakalamayi oldurur. Referans kodu koru koruna kopyalamak yerine olcmek
 * gerekiyor -- ArduChip revizyonlari arasinda bu bit degismis. */
#define AC_TIM_DEGERI       0x00U

/* AC_FIFO bitleri */
#define AC_FIFO_TEMIZLE     0x01U   /* yazma-bitti bayragini sil            */
#define AC_FIFO_BASLAT      0x02U   /* yakalamayi baslat                    */
#define AC_FIFO_WR_SIFIRLA  0x10U   /* yazma isaretcisini basa al           */
#define AC_FIFO_RD_SIFIRLA  0x20U   /* okuma isaretcisini basa al           */

/* AC_TRIG bitleri */
#define AC_TRIG_VSYNC       0x01U   /* bit0: sensorun VSYNC pininin anlik hali */
#define AC_TRIG_BITTI       0x08U   /* bit3: kare FIFO'ya yazildi           */

/* AL422B FIFO kapasitesi: 3 Mbit = 384 KB. Okunan uzunluk bunu asiyorsa
 * deger cop demektir (genelde SPI'dan hic cevap gelmemis olur). */
#define AC_FIFO_BAYT        (384U * 1024U)


/* ---------------------------------------------------------------------------
 * 10) KAMERA TANIMLARI
 * ---------------------------------------------------------------------------
 * Iki kamera arasindaki TEK fark CS pinidir; geri kalan her sey ortaktir.
 * Bu yuzden yapinin icinde yalnizca CS bilgisi var.
 * ------------------------------------------------------------------------- */
typedef struct {
    const char *ad;        /* rapor satirlarinda gorunen isim               */
    u32         port;      /* CS pininin GPIO taban adresi                  */
    u8          cs_pin;    /* CS pin numarasi                               */
} kamera_t;

#define KAMERA_ADET 2U

static const kamera_t kameralar[KAMERA_ADET] = {
    { "KAM0", GPIOB_BASE, PIN_CS0 },   /* PB6 / CN5-3 / D10 */
    { "KAM1", GPIOC_BASE, PIN_CS1 },   /* PC7 / CN5-2 / D9  */
};

/* Asama 1'in sonucu: hangi kamera SPI'dan cevap verdi. Cevap vermeyen
 * kamerayi dongude atliyoruz -- tek kamerayla da calisilabilsin diye. */
static u8 kamera_var[KAMERA_ADET];


/* ===========================================================================
 *  11) SAAT  --  sistem saatini HSI16'ya al
 * ===========================================================================
 *  L4 reset sonrasi MSI ile 4 MHz'de baslar. Bu proje icin 4 MHz YETMEZ:
 *  SPI'in en hizli bolme orani PCLK/2'dir, yani 4 MHz'de SPI en fazla
 *  2 MHz olurdu ve FIFO okumasi dort kat uzardi.
 *
 *  HSI16'ya gecince SYSCLK = HCLK = PCLK1 = PCLK2 = 16 MHz olur
 *  (on boluculerin hepsi reset'te /1'dir) ve SPI tam olarak ArduChip'in
 *  tavani olan 8 MHz'e oturur.
 *
 *  FLASH BEKLEME DURUMU GEREKMIYOR: RM0351 Tablo 11'e gore VCORE Range 1'de
 *  HCLK <= 16 MHz icin 0 wait state gecerlidir; reset degeri de 0'dir.
 *  (80 MHz'e PLL ile cikarsaniz once LATENCY=4 yazmayi unutmayin -- sirasi
 *  ters olursa cip ilk komutta HardFault'a duser.)
 *
 *  PLL'e neden cikmadik? Ihtiyac yok: darbogaz UART, SPI degil. 16 MHz hem
 *  kodu sade tutuyor hem de saat agacini tek satirda anlasilir birakiyor.
 * ========================================================================= */
static void saat_kur(void)
{
    /* HSI16'yi ac ve kararli hale gelmesini bekle. */
    RCC_CR |= RCC_HSION;
    while ((RCC_CR & RCC_HSIRDY) == 0U) { }

    /* Sistem saatini HSI16'ya cevir. SWS alani donanimin gercekten gecis
     * yaptigini bildirene kadar beklemek sart: gecis anlik degildir ve
     * beklemezsek sonraki hesaplar hala 4 MHz varsayimiyla yapilir. */
    RCC_CFGR = (RCC_CFGR & ~3U) | CFGR_SW_HSI16;
    while ((RCC_CFGR & CFGR_SWS_MASK) != CFGR_SWS_HSI16) { }

    /* I2C1 ve USART2'yi de dogrudan HSI16'ya bagliyoruz. Su an PCLK1 zaten
     * 16 MHz, yani fark yok gibi gorunuyor -- ama ileride PLL'e cikip
     * cekirdegi hizlandirdiginizda bu iki birim 16 MHz'de KALIR ve ne
     * TIMINGR ne de BRR degeri bozulur. Bir satirla kazanilan bagisiklik. */
    RCC_CCIPR &= ~((3U << 2) | (3U << 12));
    RCC_CCIPR |= CCIPR_USART2SEL_HSI16 | CCIPR_I2C1SEL_HSI16;
}

/* SysTick ile ms gecikme. Fonksiyon her cagrildiginda sayaci yeniden kurar;
 * boylece baska bir yerde SysTick kullanilmis olsa bile dogru calisir. */
static void bekle_ms(u32 ms)
{
    STK_CTRL = 0U;                            /* once durdur                 */
    STK_LOAD = (HSI16_HZ / 1000U) - 1U;       /* 16000-1 -> tam 1 ms         */
    STK_VAL  = 0U;                            /* sayaci ve COUNTFLAG'i sifirla*/
    STK_CTRL = STK_CLKSOURCE | STK_ENABLE;

    while (ms-- > 0U) {
        /* CTRL'yi okumak COUNTFLAG'i temizler, bu yuzden ayrica silmiyoruz. */
        while ((STK_CTRL & STK_COUNTFLAG) == 0U) { }
    }

    STK_CTRL = 0U;
}


/* ===========================================================================
 *  12) USART2  --  8N1, yalnizca gonderme
 * ========================================================================= */
static void uart_kur(void)
{
    RCC_AHB2ENR |= RCC_GPIOAEN;
    (void)RCC_AHB2ENR;                        /* yazma tamamlansin diye oku  */

    GPIO_MODER(GPIOA_BASE)   &= ~(3U << (PIN_TX * 2U));
    GPIO_MODER(GPIOA_BASE)   |=  (MODER_AF << (PIN_TX * 2U));
    GPIO_OTYPER(GPIOA_BASE)  &= ~(1U << PIN_TX);
    GPIO_OSPEEDR(GPIOA_BASE) |=  (3U << (PIN_TX * 2U));
    GPIO_AFR(GPIOA_BASE, PIN_TX) &= ~(0xFU << ((PIN_TX & 7U) * 4U));
    GPIO_AFR(GPIOA_BASE, PIN_TX) |=  (AF7_USART2 << ((PIN_TX & 7U) * 4U));

    RCC_APB1ENR1 |= RCC_USART2EN;
    (void)RCC_APB1ENR1;

    USART2_CR1 = 0U;
    /* BRR = saat / baud, en yakina yuvarlanmis (OVER8=0 -> 16x asiri ornekleme) */
    USART2_BRR = (HSI16_HZ + (UART_BAUD / 2U)) / UART_BAUD;
    USART2_CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void uart_bayt(u8 c)
{
    while ((USART2_ISR & USART_ISR_TXE) == 0U) { }
    USART2_TDR = c;
}

static void uart_metin(const char *s)
{
    while (*s != '\0') {
        uart_bayt((u8)*s++);
    }
}

static void uart_hex8(u8 deger)
{
    static const char rakam[] = "0123456789ABCDEF";
    uart_bayt('0');
    uart_bayt('x');
    uart_bayt((u8)rakam[(deger >> 4) & 0xFU]);
    uart_bayt((u8)rakam[deger & 0xFU]);
}

static void uart_sayi(u32 deger)
{
    char gecici[10];
    int  n = 0;

    if (deger == 0U) {
        uart_bayt('0');
        return;
    }
    while (deger > 0U) {
        gecici[n++] = (char)('0' + (deger % 10U));
        deger /= 10U;
    }
    while (n > 0) {
        uart_bayt((u8)gecici[--n]);
    }
}


/* ===========================================================================
 *  13) SPI1'i MASTER olarak kur
 * ========================================================================= */
static void spi_kur(void)
{
    RCC_AHB2ENR |= RCC_GPIOAEN;
    (void)RCC_AHB2ENR;

    /* PA5/PA6/PA7 -> AF5 (SPI1), push-pull, cok yuksek hiz.
     *
     * "Cok yuksek hiz" 8 MHz icin abartili gorunebilir ama degil: OSPEEDR
     * cikis surucusunun kenar dikligini belirler. Dusuk birakilirsa 8 MHz'de
     * kare dalga yuvarlanir, ArduChip kenari kacirir. Enerji/EMI acisindan
     * bedeli var, dogru karsiligi hizli ve temiz kenarlardir.
     *
     * MISO bir GIRIS olmasina ragmen ayni AF moduna alinir; yon secimini
     * cevre birimi yapar, GPIO'nun isi yalnizca pini SPI'a baglamaktir. */
    {
        const u8 pinler[3] = { PIN_SCK, PIN_MISO, PIN_MOSI };
        u32 i;
        for (i = 0U; i < 3U; i++) {
            u8 p = pinler[i];
            GPIO_MODER(GPIOA_BASE)   &= ~(3U << (p * 2U));
            GPIO_MODER(GPIOA_BASE)   |=  (MODER_AF << (p * 2U));
            GPIO_OTYPER(GPIOA_BASE)  &= ~(1U << p);
            GPIO_OSPEEDR(GPIOA_BASE) |=  (3U << (p * 2U));
            GPIO_PUPDR(GPIOA_BASE)   &= ~(3U << (p * 2U));
            GPIO_AFR(GPIOA_BASE, p)  &= ~(0xFU << ((p & 7U) * 4U));
            GPIO_AFR(GPIOA_BASE, p)  |=  (AF5_SPI1 << ((p & 7U) * 4U));
        }
    }

    /* SPI1 APB2'dedir -- APB1ENR1 DEGIL. */
    RCC_APB2ENR |= RCC_SPI1EN;
    (void)RCC_APB2ENR;

    RCC_APB2RSTR |=  RCC_SPI1RST;
    RCC_APB2RSTR &= ~RCC_SPI1RST;

    SPI1_CR1 = 0U;

    /* CR2 once: veri boyu ve RXNE esigi, birim ACILMADAN once ayarlanmali.
     * FRXTH atlanirsa 8 bit gonderiminde RXNE hic kalkmaz -> spi_bayt()
     * icinde sonsuz dongu. Bu hatanin belirtisi "kart aciliyor ama hicbir
     * sey basmiyor" seklindedir; bulmasi zordur. */
    SPI1_CR2 = SPI_CR2_DS_8BIT | SPI_CR2_FRXTH;

    /* CR1: master, mode 0 (CPOL=0, CPHA=0), fPCLK/2 = 8 MHz.
     *
     * SSM+SSI: NSS'i yazilim yonetir ve dahili olarak yuksek sayilir.
     * Bunu YAPMAZSAK donanim NSS'i dusuk gorup "baska bir master var"
     * sanar, MODF hatasi verip MSTR bitini kendiliginden siler. CS
     * pinlerini zaten kendimiz GPIO olarak suruyoruz. */
    SPI1_CR1 = SPI_CR1_MSTR | SPI_CR1_BR_DIV2 | SPI_CR1_SSM | SPI_CR1_SSI;
    SPI1_CR1 |= SPI_CR1_SPE;
}

/* Tek bayt gonder + ayni anda gelen bayti dondur.
 * SPI'da gonderme ve alma AYNI saat darbeleriyle olur; "yalnizca okumak"
 * diye bir sey yoktur, okumak icin de kukla bir bayt gondermek gerekir. */
static u8 spi_bayt(u8 giden)
{
    while ((SPI1_SR & SPI_SR_TXE) == 0U) { }
    SPI1_DR8 = giden;                          /* 8-bit erisim (bkz. TUZAK 2)*/
    while ((SPI1_SR & SPI_SR_RXNE) == 0U) { }
    return SPI1_DR8;
}


/* ===========================================================================
 *  14) CS pinleri  --  kameralari birbirinden ayiran tek sinyal
 * ========================================================================= */
static void cs_kur(void)
{
    u32 i;

    RCC_AHB2ENR |= RCC_GPIOBEN | RCC_GPIOCEN;
    (void)RCC_AHB2ENR;

    for (i = 0U; i < KAMERA_ADET; i++) {
        const kamera_t *k = &kameralar[i];

        /* SIRA ONEMLI: once pini YUKSEK yapip sonra cikisa alin.
         * Ters yaparsaniz pin bir an icin 0 surer; bu, o kameranin CS'ini
         * kisa sureligine secili hale getirir ve ArduChip yarim bir komut
         * gormus olabilir. Once BSRR, sonra MODER. */
        GPIO_BSRR(k->port) = (1U << k->cs_pin);

        GPIO_MODER(k->port)   &= ~(3U << (k->cs_pin * 2U));
        GPIO_MODER(k->port)   |=  (MODER_CIKIS << (k->cs_pin * 2U));
        GPIO_OTYPER(k->port)  &= ~(1U << k->cs_pin);
        GPIO_OSPEEDR(k->port) |=  (3U << (k->cs_pin * 2U));
        GPIO_PUPDR(k->port)   &= ~(3U << (k->cs_pin * 2U));
    }
}

/* CS aktif DUSUKtur: secmek icin 0, birakmak icin 1 surulur.
 * BSRR ile tek yazmada ve atomik olarak yapiliyor -- alt 16 bit "1 yap",
 * ust 16 bit "0 yap" anlamina gelir. */
static void cs_sec(const kamera_t *k)
{
    GPIO_BSRR(k->port) = (1U << (k->cs_pin + 16U));
}

static void cs_birak(const kamera_t *k)
{
    GPIO_BSRR(k->port) = (1U << k->cs_pin);
}


/* ===========================================================================
 *  15) ArduChip register erisimi  (SPI)
 * ========================================================================= */
static void ac_yaz(const kamera_t *k, u8 adres, u8 deger)
{
    cs_sec(k);
    (void)spi_bayt(adres | 0x80U);    /* bit7 = 1 -> yazma                   */
    (void)spi_bayt(deger);
    cs_birak(k);
}

static u8 ac_oku(const kamera_t *k, u8 adres)
{
    u8 deger;

    cs_sec(k);
    (void)spi_bayt(adres & 0x7FU);    /* bit7 = 0 -> okuma                   */
    deger = spi_bayt(0x00U);          /* kukla gonder, gelen veriyi al       */
    cs_birak(k);

    return deger;
}


/* ArduChip'in (CPLD) YAZILIM RESET'i -- her seyden once yapilmali.
 *
 * NEDEN BURADA, NEDEN BELGEDE YOK:
 * Elimizdeki donanim notu (Rev 1.0, 2015) Tablo 1'de 0x07 register'ini
 * listelemiyor; o belge ArduChip'in ILK revizyonunu anlatiyor. Yeni
 * revizyonlarda (surum register'i 0x40 yerine 0x73 donuyorsa sizinki de
 * oyle) CPLD, guc verildiginde belirsiz bir durumda kalabiliyor: SPI
 * register'lari duzgun cevap verir, sensor I2C'den konusur, ama yakalama
 * durum makinesi hic baslamaz. Belirtisi cok kandiricidir --
 * "her sey calisiyor ama FIFO bos" (uzunluk 8 civari, veri hep sifir).
 *
 * ArduCAM'in kendi 2MP Plus ornegi bu reset'i setup()'in EN BASINDA,
 * SPI testinden bile once yapar. Biz de oyle yapiyoruz.
 *
 * Bekleme sureleri referans koddan: 0x80 yaz, 100 ms, 0x00 yaz, 100 ms.
 * CS ayri oldugu icin bu islem kamera basinadir.
 */
static void arduchip_cpld_reset(const kamera_t *k)
{
    ac_yaz(k, AC_CPLD_RESET, 0x80U);
    bekle_ms(100U);
    ac_yaz(k, AC_CPLD_RESET, 0x00U);
    bekle_ms(100U);
}


/* ===========================================================================
 *  16) I2C1 MASTER  --  SCCB tasiyicisi
 * ========================================================================= */
static void i2c_kur(void)
{
    RCC_AHB2ENR |= RCC_GPIOBEN;
    (void)RCC_AHB2ENR;

    /* PB8/PB9 -> AF4 (I2C1), ACIK DRENAJ, dahili pull-up acik.
     *
     * Acik drenaj I2C'nin temelidir: cihazlar hatti yalnizca asagi ceker,
     * yukari cekmeyi direncler yapar. Iki kamera + STM32 ayni hatta oldugu
     * icin bu ozellikle onemli -- yayin yazarken iki sensor de ayni anda
     * ACK icin SDA'yi asagi ceker ve bu tamamen normaldir.
     *
     * PULL-UP HAKKINDA: test_i2c'de dahili pull-up (~40 kohm) yetmisti,
     * cunku karsi tarafta da dahili pull-up vardi ve paralelde ~20 kohm
     * ediyordu. BURADA DURUM FARKLI: ArduCAM modullerinin uzerinde kendi
     * pull-up direncleri vardir (tipik 4.7 kohm) ve iki modul paralelde
     * ~2.35 kohm yapar. Yani hattin yukari cekmesini zaten moduller
     * sagliyor; buradaki dahili pull-up sadece moduller taksiz iken hattin
     * havada kalmasini onleyen bir emniyettir. */
    GPIO_MODER(GPIOB_BASE) &= ~((3U << (PIN_SCL * 2U)) | (3U << (PIN_SDA * 2U)));
    GPIO_MODER(GPIOB_BASE) |=  ((MODER_AF << (PIN_SCL * 2U)) |
                                (MODER_AF << (PIN_SDA * 2U)));

    GPIO_OTYPER(GPIOB_BASE) |= (1U << PIN_SCL) | (1U << PIN_SDA);

    GPIO_OSPEEDR(GPIOB_BASE) |= (3U << (PIN_SCL * 2U)) | (3U << (PIN_SDA * 2U));

    GPIO_PUPDR(GPIOB_BASE) &= ~((3U << (PIN_SCL * 2U)) | (3U << (PIN_SDA * 2U)));
    GPIO_PUPDR(GPIOB_BASE) |=  ((1U << (PIN_SCL * 2U)) | (1U << (PIN_SDA * 2U)));

    GPIO_AFR(GPIOB_BASE, PIN_SCL) &= ~(0xFU << ((PIN_SCL & 7U) * 4U));
    GPIO_AFR(GPIOB_BASE, PIN_SCL) |=  (AF4_I2C1 << ((PIN_SCL & 7U) * 4U));
    GPIO_AFR(GPIOB_BASE, PIN_SDA) &= ~(0xFU << ((PIN_SDA & 7U) * 4U));
    GPIO_AFR(GPIOB_BASE, PIN_SDA) |=  (AF4_I2C1 << ((PIN_SDA & 7U) * 4U));

    RCC_APB1ENR1 |= RCC_I2C1EN;
    (void)RCC_APB1ENR1;

    RCC_APB1RSTR1 |=  RCC_I2C1RST;
    RCC_APB1RSTR1 &= ~RCC_I2C1RST;

    I2C1_CR1 = 0U;                     /* ayarlar PE=0 iken yapilir          */

    /* 100 kHz @ HSI16. Bu deger test_i2c'de RM0351 Bolum 39.4.9 formuluyle
     * hesaplanmis ve sahada dogrulanmisti; birebir ayni saat kaynagini
     * kullandigimiz icin aynen gecerlidir.
     *
     * Neden 400 kHz degil? I2C'den yalnizca ~300 register yaziyoruz (bir
     * kerelik, ~30 ms). Goruntu SPI'dan geliyor. Hizlandirmanin olculebilir
     * bir faydasi yok, buna karsilik iki modulun hat kapasitansi ile
     * yukselme suresi riski artardi. */
    I2C1_TIMINGR = 0x30420F13U;

    I2C1_CR1 = I2C_CR1_PE;
}

/* n bayt yaz. AUTOEND sayesinde son bayttan sonra STOP'u donanim uretir.
 * Donus: 0 = tamam, -1 = zaman asimi, -2 = NACK (kimse cevap vermedi). */
static int i2c_yaz(u8 adres7, const u8 *veri, u32 adet)
{
    u32 t = I2C_ZAMAN_ASIMI;

    while ((I2C1_ISR & I2C_ISR_BUSY) != 0U) {
        if (--t == 0U) { return -1; }
    }

    /* Onceki islemden kalmis bayraklari sil; kalirsa yeni islemi bozar. */
    I2C1_ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF;

    I2C1_CR2 = ((u32)adres7 << 1) | I2C_CR2_NBYTES(adet) |
               I2C_CR2_AUTOEND | I2C_CR2_START;

    while (adet-- > 0U) {
        t = I2C_ZAMAN_ASIMI;
        for (;;) {
            u32 isr = I2C1_ISR;
            if ((isr & I2C_ISR_TXIS) != 0U) { break; }
            if ((isr & I2C_ISR_NACKF) != 0U) {
                I2C1_ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF;
                return -2;
            }
            if (--t == 0U) { return -1; }
        }
        I2C1_TXDR = *veri++;
    }

    t = I2C_ZAMAN_ASIMI;
    while ((I2C1_ISR & I2C_ISR_STOPF) == 0U) {
        if (--t == 0U) { return -1; }
    }
    I2C1_ICR = I2C_ICR_STOPCF;

    return 0;
}

/* n bayt oku. */
static int i2c_oku(u8 adres7, u8 *veri, u32 adet)
{
    u32 t = I2C_ZAMAN_ASIMI;

    while ((I2C1_ISR & I2C_ISR_BUSY) != 0U) {
        if (--t == 0U) { return -1; }
    }

    I2C1_ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF;

    I2C1_CR2 = ((u32)adres7 << 1) | I2C_CR2_RD_WRN | I2C_CR2_NBYTES(adet) |
               I2C_CR2_AUTOEND | I2C_CR2_START;

    while (adet-- > 0U) {
        t = I2C_ZAMAN_ASIMI;
        for (;;) {
            u32 isr = I2C1_ISR;
            if ((isr & I2C_ISR_RXNE) != 0U) { break; }
            if ((isr & I2C_ISR_NACKF) != 0U) {
                I2C1_ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF;
                return -2;
            }
            if (--t == 0U) { return -1; }
        }
        *veri++ = (u8)I2C1_RXDR;
    }

    t = I2C_ZAMAN_ASIMI;
    while ((I2C1_ISR & I2C_ISR_STOPF) == 0U) {
        if (--t == 0U) { return -1; }
    }
    I2C1_ICR = I2C_ICR_STOPCF;

    return 0;
}


/* ===========================================================================
 *  17) SCCB  --  OV2640'in register protokolu
 * ===========================================================================
 *  SCCB, I2C'ye cok benzer ama AYNISI DEGILDIR. Bizi ilgilendiren tek fark
 *  okumada ortaya cikar:
 *
 *      I2C'de bir sensorden okurken TEKRAR-START kullanilir:
 *          [START][adres+W][reg][TEKRAR-START][adres+R][veri][STOP]
 *
 *      SCCB'de bu CALISMAZ. OV sensorleri iki ayri islem bekler:
 *          [START][adres+W][reg][STOP]      <-- STOP sart
 *          [START][adres+R][veri][STOP]
 *
 *  Bu yuzden asagida iki ayri cagri yapiyoruz ve AUTOEND'in urettigi STOP
 *  tam da istenen yerde olusuyor. test_i2c'deki master TEKRAR-START
 *  kullaniyordu -- oradaki kalibi buraya kopyalarsaniz okuma sessizce
 *  0xFF doner ve "kamera yok" sanirsiniz.
 * ========================================================================= */
static int sccb_yaz(u8 reg, u8 deger)
{
    u8 tampon[2];

    tampon[0] = reg;
    tampon[1] = deger;

    return i2c_yaz(OV2640_ADRES, tampon, 2U);
}

static int sccb_oku(u8 reg, u8 *deger)
{
    int sonuc = i2c_yaz(OV2640_ADRES, &reg, 1U);   /* ... [STOP] */

    if (sonuc != 0) {
        return sonuc;
    }
    return i2c_oku(OV2640_ADRES, deger, 1U);
}

/* Bir register tablosunu bastan sona yazar.
 *
 * Tablo sonu {0xFF, 0xFF} ciftidir. Yalnizca register numarasina bakip
 * "0xFF gorunce dur" DENEMEYIN: 0xFF banka secme register'idir ve tablolarin
 * ICINDE defalarca gecer (bkz. ov2640_regs.h). Iki bayta birden bakmak sart.
 *
 * Donus: basarisiz yazma sayisi. Iki kamera da takiliysa ve saglamsa 0 olmali. */
static u32 ov2640_tablo_yaz(const ov2640_reg_t *tablo)
{
    u32 hata = 0U;

    while (!(tablo->reg == 0xFFU && tablo->deger == 0xFFU)) {
        if (sccb_yaz(tablo->reg, tablo->deger) != 0) {
            hata++;
        }
        tablo++;
    }

    return hata;
}


/* ===========================================================================
 *  18) OV2640'i JPEG uretecek sekilde kur  --  YAYIN (her iki sensore)
 * ===========================================================================
 *  Tek bir sccb_yaz() cagrisi hatta bagli TUM OV2640'lara ulasir. Ayri ayri
 *  kurmak diye bir sey yok; zaten ayni ayari istedigimiz icin bu topolojiyi
 *  sectik.
 *
 *  SIRA DEGISTIRILEMEZ. Referans akis:
 *      1. COM7'ye 0x80 -> yazilim reset'i (tum register'lar varsayilana doner)
 *      2. JPEG_INIT    -> saat, pencere, DSP boru hatti
 *      3. YUV422       -> kodlayicinin girisinde bekledigi ara bicim
 *      4. JPEG         -> cikis bicimi JPEG
 *      5. COM10/0x15=0 -> cikis kutuplugu
 *      6. Cozunurluk   -> olcekleyici
 * ========================================================================= */
static u32 ov2640_jpeg_kur(void)
{
    u32 hata = 0U;

    /* Sensor bankasina gec ve yazilim reset'i at.
     * Reset'ten sonraki bekleme SART: sensorun ic PLL'i ve durum makinesi
     * oturana kadar SCCB yazmalari sessizce yutulur. ArduCAM referans kodu
     * da burada 100 ms bekler. */
    hata += (sccb_yaz(0xFFU, 0x01U) != 0) ? 1U : 0U;
    hata += (sccb_yaz(0x12U, 0x80U) != 0) ? 1U : 0U;
    bekle_ms(100U);

    hata += ov2640_tablo_yaz(OV2640_JPEG_INIT);
    hata += ov2640_tablo_yaz(OV2640_YUV422);
    hata += ov2640_tablo_yaz(OV2640_JPEG);

    hata += (sccb_yaz(0xFFU, 0x01U) != 0) ? 1U : 0U;
    hata += (sccb_yaz(0x15U, 0x00U) != 0) ? 1U : 0U;

    hata += ov2640_tablo_yaz(COZ_TABLO);

    /* Olcekleyicinin ilk kararli kareyi uretmesi icin kisa bir sure. */
    bekle_ms(200U);

    return hata;
}


/* ===========================================================================
 *  19) YAKALAMA  --  ArduChip tarafi, kamera basina bagimsiz
 * ========================================================================= */

/* Her kamera icin bir kerelik ArduChip ayari. */
static void arduchip_kur(const kamera_t *k)
{
    /* VSYNC kutuplugu: OV2640 icin aktif dusuk. Yanlis birakilirsa yakalama
     * yanlis kenardan tetiklenir; FIFO ya bos kalir ya da kare kayar. */
    ac_yaz(k, AC_TIM, AC_TIM_DEGERI);

    /* Tek kare yakala (bit[2:0] = 0 -> 1 kare). Daha fazlasi FIFO'yu
     * doldurur ve bizim tek kare okuma mantigimizi bozardi. */
    ac_yaz(k, AC_FRAMES, 0x00U);

    /* FIFO'yu ve bayragi temiz baslat. */
    ac_yaz(k, AC_FIFO, AC_FIFO_TEMIZLE);
}

/* Yakalamayi baslat: once isaretcileri ve bayragi sifirla, sonra tetikle. */
static void yakalama_baslat(const kamera_t *k)
{
    /* ArduCAM kutuphanesiyle BIREBIR ayni dizi. Onceden isaretci sifirlama
     * bitlerini (0x10|0x20) de yaziyorduk; referans kod bunu yapmiyor ve
     * yeni CPLD revizyonlarinda o bitlerin davranisi belgesiz. Referanstan
     * sapmamak, hata ayiklarken degisken sayisini azaltir. */
    ac_yaz(k, AC_FIFO, AC_FIFO_TEMIZLE);   /* flush_fifo()      */
    ac_yaz(k, AC_FIFO, AC_FIFO_TEMIZLE);   /* clear_fifo_flag() */
    ac_yaz(k, AC_FIFO, AC_FIFO_BASLAT);    /* start_capture()   */
}


/* Sensorun GERCEKTEN kare uretip uretmedigini olcer.
 *
 * NEDEN GEREKLI: "FIFO bos" iki cok farkli sebepten olur ve rapor ikisinde
 * de ayni gorunur:
 *    a) sensor hic goruntu uretmiyor  (besleme cokuyor, saat yok, kurulum
 *       gitmemis)
 *    b) sensor uretiyor ama yakalama/FIFO yolu yanlis kurulmus
 *
 * VSYNC her kare basinda kenar yapar. Bu biti hizlica yoklayip gecis sayarsak
 * ikisini ayirt ederiz -- tahmin yurutmeye gerek kalmaz:
 *    gecis = 0  -> (a) sensor sessiz
 *    gecis > 0  -> (b) kareler var, sorun FIFO tarafinda
 */
static u32 vsync_gecis_say(const kamera_t *k, u32 dongu)
{
    u32 gecis  = 0U;
    u8  onceki = (u8)(ac_oku(k, AC_TRIG) & AC_TRIG_VSYNC);
    u32 t;

    for (t = 0U; t < dongu; t++) {
        u8 simdi = (u8)(ac_oku(k, AC_TRIG) & AC_TRIG_VSYNC);
        if (simdi != onceki) {
            gecis++;
            onceki = simdi;
        }
    }
    return gecis;
}

/* Yakalamanin bitmesini bekle. Donus: 0 = bitti, -1 = zaman asimi.
 * Zaman asimi genelde su demektir: sensor hic kare uretmiyor (I2C kurulumu
 * gitmedi) ya da XCLK/VSYNC hatti bozuk. */
static int yakalama_bekle(const kamera_t *k, u32 azami_ms)
{
    while (azami_ms-- > 0U) {
        if ((ac_oku(k, AC_TRIG) & AC_TRIG_BITTI) != 0U) {
            return 0;
        }
        bekle_ms(1U);
    }
    return -1;
}

/* FIFO'ya yazilmis bayt sayisi.
 *
 * DIKKAT -- BU DEGER JPEG'IN GERCEK BOYUTU DEGILDIR. Olculdu: bildirilen
 * uzunluk 3080 iken JPEG'in bitis isareti (FF D9) 2280. bayttaydi. ArduChip
 * uzunlugu blok boyutuna yuvarliyor, aradaki fark dolgu bayti oluyor.
 * (Bildirilen degerler hep N*1024 + 8 kalibinda cikiyor.)
 *
 * Yani FIFO'dan bildirilen kadar bayt okumak DOGRUDUR -- eksik okumazsiniz.
 * Ama dosyaya yazarken FF D9'da kesmek gerekir; bunu host tarafinda
 * tools/jpeg_al.py yapiyor. */
static u32 fifo_uzunluk(const kamera_t *k)
{
    u32 d0 = ac_oku(k, AC_FIFO_SIZE1);
    u32 d1 = ac_oku(k, AC_FIFO_SIZE2);
    u32 d2 = ac_oku(k, AC_FIFO_SIZE3) & 0x7FU;

    /* Kutuphane 23 bitlik maske kullanir (eski belge 19 bit der). Genis
     * olani aliyoruz; mantik disi degerleri cagiran taraf zaten eliyor. */
    return ((d2 << 16) | (d1 << 8) | d0) & 0x07FFFFFU;
}

/* Toplu (burst) okumayi baslatir. CS ASAGIDA BIRAKILIR -- cagiran taraf
 * baytlari okuduktan sonra fifo_burst_bitir() cagirmak ZORUNDADIR.
 *
 * KUKLA BAYT YOK -- bu da olcumle belirlendi.
 *
 * Donanim notu (Bolum 5.3) "komuttan sonra okunan ilk bayt kukladir" der ve
 * kod once bir bayt atiyordu. Olcum bunun bu revizyonda YANLIS oldugunu
 * gosterdi: atma varken akisin basi 0xD8 0xFF geliyordu, yani gercek JPEG
 * basi olan 0xFF D8 FF E0 dizisinin ilk bayti yutulmustu.
 *
 * Dolayisiyla 0x3C komutundan hemen sonraki bayt ZATEN gecerli veridir.
 * (spi_bayt(0x3C) cagrisinin kendisi sirasinda donen bayt anlamsizdir ve
 * zaten atiliyor; ayrica bir okuma yapmak bir bayt kaybettiriyordu.) */
static void fifo_burst_basla(const kamera_t *k)
{
    cs_sec(k);
    (void)spi_bayt(AC_BURST_READ);
}

static void fifo_burst_bitir(const kamera_t *k)
{
    cs_birak(k);
}


/* ===========================================================================
 *  20) ASAMA 1  --  SPI baglantisini kamera basina dogrula
 * ===========================================================================
 *  Bu testin degeri, ORTAK I2C hattinin yapamadigi seyi yapmasidir:
 *  kameralari TEK TEK eler. CS ayri oldugu icin cevap veren gercekten o
 *  kameradir.
 *
 *  Iki asamali test:
 *    a) AC_TEST1'e iki farkli deger yazip geri oku. Tek deger yeterli
 *       degildir: hat surekli 0x00 veya 0xFF okuyorsa sansa dogru cikabilir.
 *       0x55 ve 0xAA birbirinin tersidir, ikisi de gecerse hat gercekten
 *       iki yone de calisiyor demektir.
 *    b) Surum register'i (0x40) yalnizca BILGI amaclidir, gecme sarti
 *       degildir. Eski donanim notu "2MP modelinde sabit 0x40" der, ama
 *       yeni ArduChip revizyonlari baska deger dondurur (ornegin 0x73) ve
 *       ArduCAM'in kendi kutuphanesi bu register'i HICBIR YERDE kontrol
 *       etmez -- modul kimligini OV2640'in I2C kimliginden dogrular.
 *       Biz de oyle yapiyoruz: burada sadece basiyoruz, karar asama 2'de.
 * ========================================================================= */
static int spi_baglanti_testi(const kamera_t *k)
{
    u8 geri1;
    u8 geri2;
    u8 surum;

    ac_yaz(k, AC_TEST1, 0x55U);
    geri1 = ac_oku(k, AC_TEST1);

    ac_yaz(k, AC_TEST1, 0xAAU);
    geri2 = ac_oku(k, AC_TEST1);

    surum = ac_oku(k, AC_REV);

    uart_metin("  ");
    uart_metin(k->ad);
    uart_metin(" : yaz/oku 0x55->");
    uart_hex8(geri1);
    uart_metin("  0xAA->");
    uart_hex8(geri2);
    uart_metin("  surum(0x40)=");
    uart_hex8(surum);

    if (geri1 != 0x55U || geri2 != 0xAAU) {
        uart_metin("   -> SPI YOK\r\n");
        return -1;
    }

    /* Surum bilgi amacli; gecme sarti degil (bkz. yukaridaki not). */
    uart_metin((surum == 0x40U) ? "   -> TAMAM (eski revizyon)\r\n"
                                : "   -> TAMAM (yeni revizyon)\r\n");
    return 0;
}


/* ===========================================================================
 *  21) main
 * ========================================================================= */
int main(void)
{
    u32 tur = 0U;
    u32 i;

    saat_kur();
    uart_kur();
    spi_kur();
    cs_kur();
    i2c_kur();

    uart_metin("\r\n\r\n");
    uart_metin("=====================================================\r\n");
    uart_metin(" NUCLEO-L476RG  |  2 x ArduCAM Mini 2MP (OV2640)\r\n");
    uart_metin("=====================================================\r\n");
    uart_metin(" SPI1 : PA5=SCK  PA6=MISO  PA7=MOSI   @ 8 MHz, mode 0\r\n");
    uart_metin(" I2C1 : PB8=SCL  PB9=SDA              @ 100 kHz\r\n");
    uart_metin(" CS   : PB6=KAM0  PC7=KAM1\r\n");
    uart_metin(" Coz. : " COZ_METIN "   (JPEG)\r\n");
    uart_metin("\r\n");

    /* --- ASAMA 1: SPI, kamera basina ---------------------------------- */
    uart_metin("[1] SPI baglantisi (kamera basina, CS ayri):\r\n");
    {
        u32 bulunan = 0U;

        /* Once CPLD reset'i -- SPI testinden bile once. Kamera takili
         * degilse bu yazmalar bosa gider, zarari yok. */
        for (i = 0U; i < KAMERA_ADET; i++) {
            arduchip_cpld_reset(&kameralar[i]);
        }

        for (i = 0U; i < KAMERA_ADET; i++) {
            kamera_var[i] = (spi_baglanti_testi(&kameralar[i]) == 0) ? 1U : 0U;
            bulunan += kamera_var[i];
        }

        if (bulunan == 0U) {
            uart_metin("\r\n  HICBIR KAMERA CEVAP VERMIYOR.\r\n");
            uart_metin("  Kontrol: +5V (3.3V DEGIL), GND ortak mi,\r\n");
            uart_metin("           SCK/MISO/MOSI ve CS kablolari.\r\n");
            uart_metin("  Program burada duruyor.\r\n");
            for (;;) { }
        }

        uart_metin("  -> ");
        uart_sayi(bulunan);
        uart_metin(" kamera bulundu.\r\n\r\n");
    }

    /* --- ASAMA 2: I2C ------------------------------------------------- */
    uart_metin("[2] I2C / OV2640 kimligi (ORTAK hat, yayin):\r\n");
    {
        u8  pidh = 0U;
        u8  pidl = 0U;
        int s1;
        int s2;

        /* Kimlik register'lari sensor bankasindadir. */
        (void)sccb_yaz(0xFFU, 0x01U);
        s1 = sccb_oku(OV2640_PIDH, &pidh);
        s2 = sccb_oku(OV2640_PIDL, &pidl);

        uart_metin("  PIDH=");
        uart_hex8(pidh);
        uart_metin("  PIDL=");
        uart_hex8(pidl);

        if (s1 != 0 || s2 != 0) {
            uart_metin("   -> I2C CEVAP YOK (SDA/SCL/GND?)\r\n");
        } else if (pidh == 0x26U && (pidl == 0x41U || pidl == 0x42U)) {
            uart_metin("   -> OV2640 dogrulandi\r\n");
        } else {
            uart_metin("   -> beklenmedik kimlik (0x26 / 0x41-42 bekleniyordu)\r\n");
        }

        /* HATIRLATMA: bu okuma hatta kac sensor oldugunu SOYLEMEZ. Iki
         * sensor de ayni degeri surdugu icin tek kamerayla da ayni sonuc
         * cikar. Kamera sayimi asama 1'de yapildi. */
        uart_metin("  (not: bu test kamera SAYISINI olcmez -- bkz. asama 1)\r\n\r\n");
    }

    /* --- ASAMA 3: sensor kurulumu ------------------------------------- */
    /* 193 + 10 + 9 + 40 = 252 register cifti, arti elle yazilan 4 tanesi. */
    uart_metin("[3] OV2640 JPEG kurulumu (yayin, ~250 register)...\r\n");
    {
        u32 hata = ov2640_jpeg_kur();

        uart_metin("  basarisiz yazma: ");
        uart_sayi(hata);
        uart_metin(hata == 0U ? "   -> TAMAM\r\n\r\n" : "   -> HATA VAR\r\n\r\n");
    }

    for (i = 0U; i < KAMERA_ADET; i++) {
        if (kamera_var[i] != 0U) {
            arduchip_kur(&kameralar[i]);
        }
    }

    /* --- ASAMA 3.5: sensor kare uretiyor mu? -------------------------- */
    uart_metin("[3.5] VSYNC etkinligi (sensor kare uretiyor mu?):\r\n");
    for (i = 0U; i < KAMERA_ADET; i++) {
        u32 gecis;

        if (kamera_var[i] == 0U) {
            continue;
        }
        gecis = vsync_gecis_say(&kameralar[i], 20000U);

        uart_metin("  ");
        uart_metin(kameralar[i].ad);
        uart_metin(" : VSYNC gecisi = ");
        uart_sayi(gecis);
        uart_metin(gecis == 0U
                   ? "   -> SENSOR SESSIZ (besleme/saat/kurulum)\r\n"
                   : "   -> kare uretiliyor (sorun FIFO tarafinda)\r\n");
    }
    uart_metin("\r\n");

    /* --- ASAMA 4: surekli yakalama ------------------------------------ */
    uart_metin("[4] Yakalama dongusu basliyor.\r\n");
#if GORUNTU_AKISI
    uart_metin("    Goruntu akisi ACIK -- tools/jpeg_al.py ile kaydedin.\r\n");
#else
    uart_metin("    Goruntu akisi kapali (yalnizca rapor).\r\n");
    uart_metin("    Acmak icin: make GORUNTU_AKISI=1 flash\r\n");
#endif
    uart_metin("\r\n");

    for (;;) {
        tur++;

        for (i = 0U; i < KAMERA_ADET; i++) {
            const kamera_t *k = &kameralar[i];
            u32 uzunluk;
            u8  bas[4];
            u32 j;

            if (kamera_var[i] == 0U) {
                continue;
            }

            uart_metin("tur ");
            uart_sayi(tur);
            uart_metin("  ");
            uart_metin(k->ad);
            uart_metin("  ");

            yakalama_baslat(k);

            /* 3 saniye, cok comert bir tavan: 320x240 JPEG normalde
             * ~100 ms'de biter. Uzun tutmamizin sebebi, sensorun ilk
             * karelerde pozlama ayarini oturtmasi icin zaman tanimak. */
            if (yakalama_bekle(k, 3000U) != 0) {
                uart_metin("YAKALAMA ZAMAN ASIMI (sensor kare uretmiyor)\r\n");
                continue;
            }

            uzunluk = fifo_uzunluk(k);

            uart_metin("uzunluk=");
            uart_sayi(uzunluk);

            if (uzunluk == 0U || uzunluk > AC_FIFO_BAYT) {
                uart_metin("  -> GECERSIZ\r\n");
                continue;
            }

            /* Ilk 4 bayti gosterelim: JPEG dosyalari daima 0xFF 0xD8 ile
             * baslar (SOI = Start Of Image). Bunu gormek, tum zincirin
             * (I2C kurulumu + sensor + FIFO + SPI okuma) calistiginin
             * bilgisayarda hicbir sey calistirmadan alinabilen kanitidir. */
            fifo_burst_basla(k);
            for (j = 0U; j < 4U; j++) {
                bas[j] = spi_bayt(0x00U);
            }

#if GORUNTU_AKISI
            /* Cerceve basligi, sonra HAM baytlar. Alici tarafta:
             *   #IMG:<kamera>:<uzunluk>\r\n  ardindan tam olarak <uzunluk> bayt
             * Ilk 4 bayti yukarida okuduk, once onlari gonderiyoruz. */
            uart_metin("  akis...\r\n");
            uart_metin("#IMG:");
            uart_sayi(i);
            uart_bayt(':');
            uart_sayi(uzunluk);
            uart_metin("\r\n");

            for (j = 0U; j < 4U && j < uzunluk; j++) {
                uart_bayt(bas[j]);
            }
            for (j = 4U; j < uzunluk; j++) {
                uart_bayt(spi_bayt(0x00U));
            }
            fifo_burst_bitir(k);
            uart_metin("\r\n#SON\r\n");
#else
            fifo_burst_bitir(k);

            uart_metin("  bas=");
            for (j = 0U; j < 4U; j++) {
                uart_hex8(bas[j]);
                uart_bayt(' ');
            }
            uart_metin((bas[0] == 0xFFU && bas[1] == 0xD8U)
                       ? " -> JPEG SOI TAMAM\r\n"
                       : " -> SOI YOK (bkz. fifo_burst_basla kukla bayt notu)\r\n");
#endif
        }

        bekle_ms(1000U);
    }

    return 0;   /* buraya asla ulasilmaz */
}
