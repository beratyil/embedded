/*
 * ============================================================================
 *  main.c  --  STM32F407VG DISCOVERY  |  I2C1 MASTER (yoklamali / polling)
 * ============================================================================
 *
 *  HEDEF DONANIM : STM32F407VG, Cortex-M4F, 1 MB Flash, 128 KB SRAM
 *  KART          : STM32F4DISCOVERY / STM32F407G-DISC1
 *  ROL           : I2C YONETICISI (master) -- saati o uretir, konusmayi o baslatir
 *  KARSI TARAF   : NUCLEO-L476RG, 7-bit adres 0x42 (slave)
 *
 *  KURAL: Hicbir kutuphane yok (HAL / LL / CMSIS / libc yok). Sadece register.
 *
 * ----------------------------------------------------------------------------
 *  KABLOLAMA  (bu iki teli takmadan hicbir sey calismaz)
 * ----------------------------------------------------------------------------
 *      F407 DISCOVERY              NUCLEO-L476RG
 *      --------------              -------------
 *      PB6  (I2C1_SCL)  <------->  PB8  (CN5-10, uzerinde "SCL/D15" yazar)
 *      PB7  (I2C1_SDA)  <------->  PB9  (CN5-9,  uzerinde "SDA/D14" yazar)
 *      GND              <------->  GND        <-- SART! Ortak toprak olmadan
 *                                                 mantik seviyeleri anlamsizdir.
 *
 *      Ayrica HER IKI HATTA da 3.3V'a giden 4.7k pull-up direnc:
 *
 *              3V3 ---[4.7k]---+--- SCL
 *              3V3 ---[4.7k]---+--- SDA
 *
 *      NEDEN PULL-UP?  I2C hatti "acik drenaj"dir (open-drain): cihazlar
 *      hatti sadece GND'ye CEKEBILIR, 3.3V'a SUREMEZ. Hatti bosta 1'e
 *      cekecek olan bu dirençlerdir. Yoksa hat havada kalir ve haberlesme
 *      olmaz. Kodda dahili pull-up'lari da aciyoruz ama onlar ~40k
 *      civarindadir; kisa kablo + 100 kHz'de bazen yeter, GUVENILIR DEGILDIR.
 *      Harici direnc takin.
 *
 *  NOT (Discovery'ye ozgu) -- UM1472'nin pin tablosundan dogrulanmistir:
 *      PB6 -> kart uzerinde "SCL" adli nete baglidir (CS43L22 ses kodegi)
 *      PB7 -> kart fonksiyonu YOK, tamamen serbest
 *      PB9 -> kart uzerinde "SDA" netidir (kodegin veri hatti)
 *
 *  Yani SCL hattimiz kodekle ortaktir. Sorun degil, cunku kodegin RESET ucu
 *  (PD4) reset sonrasi LOW'dur; cip reset halinde kalir, I2C uclari yuksek
 *  empedanstadir ve hatta karismaz. Ayrica adresi 0x4A, bizimki 0x42.
 *
 *  PRATIK SONUC: PB6 kartin kendi I2C neti uzerinde oldugu icin orada
 *  buyuk olasilikla ZATEN bir pull-up vardir; PB7 ise bos bir pindir ve
 *  pull-up'i kesinlikle disaridan vermeniz gerekir.
 *
 * ----------------------------------------------------------------------------
 *  LED'LERIN ANLAMI (kart uzerindeki 4 LED durum ekranimiz)
 * ----------------------------------------------------------------------------
 *      PD13 TURUNCU : Her turda yanip soner -> "kod calisiyor" (kalp atisi)
 *      PD12 YESIL   : Son tur BASARILI -> karsi taraf dogru cevap verdi
 *      PD14 KIRMIZI : Son turda HATA var (veri yanlis veya haberlesme koptu)
 *      PD15 MAVI    : Karsi taraftan hic ses yok (adres NACK / zaman asimi)
 *                     -> genelde "kablo takili degil" demektir
 *
 *      Su an kartlar birbirine bagli DEGILSE beklenen tablo:
 *          turuncu yanip soner + kirmizi yanar + mavi yanar
 *      Kablolari taktiktan sonra:
 *          turuncu yanip soner + YESIL yanar (mavi ve kirmizi soner)
 *
 * ----------------------------------------------------------------------------
 *  HER TURDA (500 ms) YAPILAN IS
 * ----------------------------------------------------------------------------
 *      1) Slave'in 0x01 (LED_CTRL) register'ina 0/1 yaz  -> Nucleo LED'i yakar/soner
 *      2) Slave'in 0x03 (ECHO) register'ina bir sayac yaz
 *      3) 0x00'dan baslayarak 4 register'i tek seferde oku
 *      4) Gelen veriyi dogrula:  reg[0] == 0x5A (kimlik) ve reg[3] == yazdigimiz sayac
 *
 *  Bu 3 adim I2C'nin ucunu da kullanir: cok baytli yazma, tekrar-START ile
 *  yon degistirme ve cok baytli okuma.
 * ============================================================================
 */

/* ---------------------------------------------------------------------------
 * 0) TEMEL TIPLER
 * ---------------------------------------------------------------------------
 * Kutuphane yok, <stdint.h> bile yok. ARM Cortex-M ABI'sinde 'unsigned int'
 * tam olarak 32 bittir, 'unsigned char' 8 bittir.
 *
 * 'volatile': donanim register'inin degeri programimiz disindaki nedenlerle
 * degisebilir (bayraklari donanim set eder). Bu anahtar kelime olmadan
 * derleyici "bu degeri zaten okumustum" deyip okumayi siler ve kod calismaz.
 * ------------------------------------------------------------------------- */
typedef unsigned char         u8;    /*  8-bit isaretsiz (I2C bayti)         */
typedef unsigned int          u32;   /* 32-bit isaretsiz tamsayi             */
typedef volatile unsigned int vu32;  /* 32-bit donanim register'i            */

#define REG32(adres)  (*(vu32 *)(adres))


/* ---------------------------------------------------------------------------
 * 1) HAFIZA HARITASI  (Referans: RM0090, Tablo 1 "Register boundary addresses")
 * ---------------------------------------------------------------------------
 *   0x4000 0000  APB1  <- I2C1 burada (0x4000 5400)
 *   0x4001 0000  APB2
 *   0x4002 0000  AHB1  <- GPIO portlari ve RCC burada
 * ------------------------------------------------------------------------- */
#define RCC_BASE      0x40023800U
#define GPIOB_BASE    0x40020400U            /* I2C pinleri  (PB6, PB7)      */
#define GPIOD_BASE    0x40020C00U            /* Durum LED'leri (PD12..PD15)  */
#define I2C1_BASE     0x40005400U


/* ---------------------------------------------------------------------------
 * 2) RCC  --  Saat kapilari
 * ---------------------------------------------------------------------------
 * Guc tasarrufu icin her cevre birimi reset aninda SAATSIZ gelir. Saati
 * kapali bir bloga yazma islemi kaybolur, okuma 0 doner.
 * ------------------------------------------------------------------------- */
#define RCC_AHB1ENR   REG32(RCC_BASE + 0x30U)  /* GPIO saatleri              */
#define RCC_APB1ENR   REG32(RCC_BASE + 0x40U)  /* I2C1 saati                 */
#define RCC_APB1RSTR  REG32(RCC_BASE + 0x20U)  /* I2C1 donanim reset'i       */

#define RCC_GPIOBEN   (1U << 1)              /* AHB1ENR bit 1  = GPIOB       */
#define RCC_GPIODEN   (1U << 3)              /* AHB1ENR bit 3  = GPIOD       */
#define RCC_I2C1EN    (1U << 21)             /* APB1ENR bit 21 = I2C1        */
#define RCC_I2C1RST   (1U << 21)             /* APB1RSTR bit 21 = I2C1 reset */


/* ---------------------------------------------------------------------------
 * 3) GPIO REGISTER'LARI  (RM0090, Bolum 8.4)
 * ---------------------------------------------------------------------------
 *   0x00 MODER   2 bit/pin : 00 giris, 01 cikis, 10 ALTERNATIF, 11 analog
 *   0x04 OTYPER  1 bit/pin : 0 push-pull, 1 ACIK DRENAJ (I2C icin sart)
 *   0x08 OSPEEDR 2 bit/pin : slew rate
 *   0x0C PUPDR   2 bit/pin : 00 yok, 01 pull-up, 10 pull-down
 *   0x14 ODR     1 bit/pin : cikis verisi
 *   0x18 BSRR    1 bit/pin : atomik set (alt 16 bit) / reset (ust 16 bit)
 *   0x20 AFRL    4 bit/pin : pin 0..7  icin alternatif fonksiyon numarasi
 *   0x24 AFRH    4 bit/pin : pin 8..15 icin
 * ------------------------------------------------------------------------- */
#define GPIOB_MODER   REG32(GPIOB_BASE + 0x00U)
#define GPIOB_OTYPER  REG32(GPIOB_BASE + 0x04U)
#define GPIOB_OSPEEDR REG32(GPIOB_BASE + 0x08U)
#define GPIOB_PUPDR   REG32(GPIOB_BASE + 0x0CU)
#define GPIOB_BSRR    REG32(GPIOB_BASE + 0x18U)
#define GPIOB_AFRL    REG32(GPIOB_BASE + 0x20U)

#define GPIOD_MODER   REG32(GPIOD_BASE + 0x00U)
#define GPIOD_OTYPER  REG32(GPIOD_BASE + 0x04U)
#define GPIOD_OSPEEDR REG32(GPIOD_BASE + 0x08U)
#define GPIOD_PUPDR   REG32(GPIOD_BASE + 0x0CU)
#define GPIOD_BSRR    REG32(GPIOD_BASE + 0x18U)

/* I2C1 pinleri */
#define PIN_SCL       6U                     /* PB6 = I2C1_SCL               */
#define PIN_SDA       7U                     /* PB7 = I2C1_SDA               */
#define AF4_I2C1      4U                     /* PB6/PB7 icin AF numarasi     */

/* Durum LED'leri (hepsi GPIOD, aktif-YUKSEK) */
#define LED_YESIL     (1U << 12)             /* PD12 */
#define LED_TURUNCU   (1U << 13)             /* PD13 */
#define LED_KIRMIZI   (1U << 14)             /* PD14 */
#define LED_MAVI      (1U << 15)             /* PD15 */
#define LED_HEPSI     (LED_YESIL | LED_TURUNCU | LED_KIRMIZI | LED_MAVI)


/* ---------------------------------------------------------------------------
 * 4) I2C1 REGISTER'LARI  (RM0090, Bolum 27.6)
 * ---------------------------------------------------------------------------
 * DIKKAT: STM32F4'un I2C birimi "eski nesil"dir (I2C v1). Islemleri SR1/SR2
 * bayraklarini sirayla yoklayarak yurutursunuz ve bazi bayraklar OKUMA ILE
 * temizlenir. STM32L4'teki (v2) TIMINGR/ISR/ICR yapisi bambaska bir dunyadir;
 * iki dosyayi karsilastirinca fark net gorulur.
 * ------------------------------------------------------------------------- */
#define I2C1_CR1      REG32(I2C1_BASE + 0x00U)  /* Kontrol 1                 */
#define I2C1_CR2      REG32(I2C1_BASE + 0x04U)  /* Kontrol 2 (FREQ alani)    */
#define I2C1_DR       REG32(I2C1_BASE + 0x10U)  /* Veri (8 bit)              */
#define I2C1_SR1      REG32(I2C1_BASE + 0x14U)  /* Durum 1 (olay bayraklari) */
#define I2C1_SR2      REG32(I2C1_BASE + 0x18U)  /* Durum 2 (BUSY, MSL...)    */
#define I2C1_CCR      REG32(I2C1_BASE + 0x1CU)  /* Saat kontrol (hiz)        */
#define I2C1_TRISE    REG32(I2C1_BASE + 0x20U)  /* Azami yukselme suresi     */

/* --- CR1 bitleri --- */
#define I2C_CR1_PE       (1U << 0)   /* Peripheral Enable                    */
#define I2C_CR1_START    (1U << 8)   /* START kosulu uret                    */
#define I2C_CR1_STOP     (1U << 9)   /* STOP kosulu uret                     */
#define I2C_CR1_ACK      (1U << 10)  /* Alinan bayta ACK don                 */
#define I2C_CR1_POS      (1U << 11)  /* 2 baytlik okumada NACK'in yeri       */
#define I2C_CR1_SWRST    (1U << 15)  /* Yazilimsal reset                     */

/* --- SR1 bitleri (olaylar) --- */
#define I2C_SR1_SB       (1U << 0)   /* START gonderildi                     */
#define I2C_SR1_ADDR     (1U << 1)   /* Adres gonderildi ve ACK'landi        */
#define I2C_SR1_BTF      (1U << 2)   /* Byte Transfer Finished               */
#define I2C_SR1_RXNE     (1U << 6)   /* Alma register'i dolu                 */
#define I2C_SR1_TXE      (1U << 7)   /* Gonderme register'i bos              */
#define I2C_SR1_BERR     (1U << 8)   /* Bus hatasi                           */
#define I2C_SR1_ARLO     (1U << 9)   /* Hakem kaybi (arbitration lost)       */
#define I2C_SR1_AF       (1U << 10)  /* Acknowledge Failure = NACK geldi     */
#define I2C_SR1_OVR      (1U << 11)  /* Tasma                                */

/* --- SR2 bitleri --- */
#define I2C_SR2_MSL      (1U << 0)   /* 1 = master modundayiz                */
#define I2C_SR2_BUSY     (1U << 1)   /* Hat mesgul                           */


/* ---------------------------------------------------------------------------
 * 5) SysTick  --  Cortex-M cekirdeginin icindeki 24-bit geri sayici
 * ---------------------------------------------------------------------------
 * SysTick ST'nin degil, ARM cekirdeginin parcasidir: adresi her Cortex-M'de
 * aynidir ve RCC ile saat acmak gerekmez.
 *
 * SAAT DURUMU: STM32F4 reset sonrasi HSI (dahili 16 MHz RC) ile calisir.
 * RCC_CFGR reset degeri 0 oldugu icin AHB ve APB1 bolucusu 1'dir:
 *      SYSCLK = HCLK = PCLK1 = 16 MHz
 * PLL kurmuyoruz; 16 MHz hem SysTick hem I2C icin fazlasiyla yeterli ve
 * kodun tamami tek bir bilinen frekansa dayaniyor.
 * ------------------------------------------------------------------------- */
#define SYSTICK_CTRL  REG32(0xE000E010U)
#define SYSTICK_LOAD  REG32(0xE000E014U)
#define SYSTICK_VAL   REG32(0xE000E018U)

#define SYSTICK_ENABLE    (1U << 0)
#define SYSTICK_CLKSOURCE (1U << 2)
#define SYSTICK_COUNTFLAG (1U << 16)

#define SISTEM_SAATI_HZ   16000000U          /* HSI, reset sonrasi           */
#define PCLK1_HZ          16000000U          /* APB1 saati (I2C'nin saati)   */
#define TICK_1MS          ((SISTEM_SAATI_HZ / 1000U) - 1U)


/* ---------------------------------------------------------------------------
 * 6) HABERLESME PROTOKOLU  --  Slave'in "register haritasi"
 * ---------------------------------------------------------------------------
 * Karsi tarafi gercek bir I2C sensoru gibi tasarladik: icinde numarali
 * register'lar var, once hangisiyle ilgilendigimizi soyluyoruz, sonra
 * okuyor veya yaziyoruz. Ticari sensorlerin (MPU6050, BME280...) calisma
 * mantigi da tam olarak budur.
 *
 *   Adres  Isim       Erisim  Aciklama
 *   -----  ---------  ------  -------------------------------------------
 *   0x00   WHO_AM_I   R       Sabit 0x5A. "Dogru cihazla mi konusuyorum?"
 *   0x01   LED_CTRL   R/W     bit0 -> Nucleo uzerindeki LD2 (PA5)
 *   0x02   COUNTER    R       Slave'in tamamladigi islem sayisi
 *   0x03   ECHO       R/W     Ne yazarsak aynen geri okunur (hat testi)
 * ------------------------------------------------------------------------- */
#define SLAVE_ADRES   0x42U                  /* 7-bit adres                  */

#define REG_WHO_AM_I  0x00U
#define REG_LED_CTRL  0x01U
#define REG_COUNTER   0x02U
#define REG_ECHO      0x03U

#define WHO_AM_I_BEKLENEN  0x5AU             /* Slave'in donmesi gereken imza*/


/* ---------------------------------------------------------------------------
 * 7) HATA KODLARI
 * ------------------------------------------------------------------------- */
typedef enum {
    I2C_TAMAM = 0,        /* Islem basarili                                  */
    I2C_HATA_MESGUL,      /* Hat basta bile serbest degildi (SDA/SCL LOW?)   */
    I2C_HATA_START,       /* START kosulu uretilemedi                        */
    I2C_HATA_ADRES_NACK,  /* Adresi kimse sahiplenmedi -> slave yok/kablo yok*/
    I2C_HATA_VERI_NACK,   /* Slave veriyi kabul etmedi                       */
    I2C_HATA_ZAMAN_ASIMI  /* Bayrak beklerken sure doldu                     */
} i2c_durum_t;


/* ---------------------------------------------------------------------------
 * 8) ZAMAN ASIMI SAYACI
 * ---------------------------------------------------------------------------
 * Bir bayragi "while (bayrak == 0);" diye beklemek gomulu sistemde en sik
 * yapilan hatadir: kablo cikarsa program sonsuza kadar orada kilitlenir ve
 * kart olmus gibi gorunur. Bu yuzden her beklemenin bir ust siniri var.
 *
 * Neden SysTick milisaniyesi degil de dongu sayaci? Cunku asagida bazi
 * kritik bolumlerde kesmeler kapali olacak ve zaman sayaci ilerlemeyecek.
 * Dongu sayaci her kosulda calisir. Suresi optimizasyona gore degisir ama
 * bir zaman asimi icin "yaklasik" olmasi yeterlidir.
 * ------------------------------------------------------------------------- */
#define I2C_ZAMAN_ASIMI   100000U            /* 16 MHz'de kabaca birkac 10 ms*/


/* ---------------------------------------------------------------------------
 * 9) KESME ACMA/KAPAMA  --  CMSIS olmadan
 * ---------------------------------------------------------------------------
 * CMSIS'in __disable_irq() fonksiyonunun yaptigi tek sey asagidaki tek
 * komuttur: PRIMASK bitini set edip tum maskelenebilir kesmeleri susturur.
 *
 * Bu programda hic kesme acmiyoruz, dolayisiyla teknik olarak gereksizler.
 * Yine de duruyorlar; cunku F4'un I2C'sinde okuma sirasindaki bazi adimlar
 * (ST'nin AN2824 notunda anlatilan) araya kesme girerse BOZULUR ve bu kodu
 * ileride kesmeli bir projeye kopyalarsaniz sessizce hatali calisir.
 * ------------------------------------------------------------------------- */
static inline void kesmeleri_kapat(void) { __asm volatile ("cpsid i" ::: "memory"); }
static inline void kesmeleri_ac(void)    { __asm volatile ("cpsie i" ::: "memory"); }


/* ---------------------------------------------------------------------------
 * 10) SysTick ile milisaniye beklemesi
 * ------------------------------------------------------------------------- */
static void systick_baslat(void)
{
    SYSTICK_LOAD = TICK_1MS;
    SYSTICK_VAL  = 0U;
    /* CLKSOURCE=1 -> islemci saati. TICKINT vermiyoruz: kesme yerine
     * COUNTFLAG bayragini yokluyoruz, boylece NVIC'e hic dokunmuyoruz. */
    SYSTICK_CTRL = SYSTICK_CLKSOURCE | SYSTICK_ENABLE;
}

static void bekle_ms(u32 milisaniye)
{
    /* COUNTFLAG, sayac 0'a her indiginde 1 olur ve CTRL OKUNDUGU anda
     * kendiliginden 0'a doner. Yani her okuma "bir periyot doldu mu?"
     * sorusunun cevabidir. */
    while (milisaniye != 0U) {
        while ((SYSTICK_CTRL & SYSTICK_COUNTFLAG) == 0U) { }
        milisaniye--;
    }
}


/* ---------------------------------------------------------------------------
 * 11) LED'ler
 * ---------------------------------------------------------------------------
 * BSRR ile yaziyoruz: tek yazma islemi, atomik. (ODR |= maske uc islemdir
 * ve araya kesme girerse kaybolabilir.)
 * ------------------------------------------------------------------------- */
static void ledleri_kur(void)
{
    RCC_AHB1ENR |= RCC_GPIODEN;
    (void)RCC_AHB1ENR;                       /* saatin oturmasi icin geri oku*/

    /* PD12..PD15 -> genel amacli cikis (01), push-pull, dusuk hiz. */
    GPIOD_MODER   &= ~(0xFFU << 24);         /* 4 pinin 2'ser biti = 8 bit   */
    GPIOD_MODER   |=  (0x55U << 24);         /* 01 01 01 01 = dordu de cikis */
    GPIOD_OTYPER  &= ~LED_HEPSI;             /* push-pull                    */
    GPIOD_OSPEEDR &= ~(0xFFU << 24);         /* dusuk hiz yeter              */
    GPIOD_PUPDR   &= ~(0xFFU << 24);         /* dahili direnc gereksiz       */

    GPIOD_BSRR = (LED_HEPSI << 16);          /* hepsi sonuk baslasin         */
}

static inline void led_yak(u32 maske)    { GPIOD_BSRR = maske; }
static inline void led_sondur(u32 maske) { GPIOD_BSRR = (maske << 16); }


/* ===========================================================================
 *  12) I2C1'i MASTER OLARAK KUR
 * ===========================================================================
 *  Sira onemlidir: once pinler, sonra cevre birimi. Cevre birimi PE=1 ile
 *  acildiktan sonra CCR/TRISE/FREQ degistirilemez (degisiklikler dikkate
 *  alinmaz), bu yuzden tum zamanlama ayarlari PE'den ONCE yapilir.
 * ========================================================================= */
static void i2c_kur(void)
{
    /* --- 12.1  Pinlerin saati ------------------------------------------- */
    RCC_AHB1ENR |= RCC_GPIOBEN;
    (void)RCC_AHB1ENR;

    /* --- 12.2  PB6/PB7 -> alternatif fonksiyon, ACIK DRENAJ --------------
     * I2C'nin can damari burasidir:
     *
     *   MODER  = 10 (alternatif fonksiyon) -> pini I2C birimine devret
     *   AFR    = 4  (AF4 = I2C1)           -> "hangi" alternatif fonksiyon
     *   OTYPER = 1  (open-drain)           -> pin sadece 0'a cekebilsin;
     *                1'i harici pull-up versin. Push-pull birakirsaniz iki
     *                cip ayni anda biri 1 biri 0 surunce kisa devre olur.
     *   PUPDR  = 01 (dahili pull-up)       -> harici direnc yokken yardimci
     *                olur ama tek basina guvenilmez (yaklasik 40k).
     */
    GPIOB_MODER   &= ~((3U << (PIN_SCL * 2)) | (3U << (PIN_SDA * 2)));
    GPIOB_MODER   |=  ((2U << (PIN_SCL * 2)) | (2U << (PIN_SDA * 2)));

    GPIOB_OTYPER  |=  ((1U << PIN_SCL) | (1U << PIN_SDA));   /* acik drenaj  */

    GPIOB_OSPEEDR &= ~((3U << (PIN_SCL * 2)) | (3U << (PIN_SDA * 2)));
    GPIOB_OSPEEDR |=  ((3U << (PIN_SCL * 2)) | (3U << (PIN_SDA * 2))); /* very high */

    GPIOB_PUPDR   &= ~((3U << (PIN_SCL * 2)) | (3U << (PIN_SDA * 2)));
    GPIOB_PUPDR   |=  ((1U << (PIN_SCL * 2)) | (1U << (PIN_SDA * 2))); /* pull-up */

    /* AFRL: her pin icin 4 bit. Pin 6 -> bit 27:24, pin 7 -> bit 31:28. */
    GPIOB_AFRL &= ~((0xFU << (PIN_SCL * 4)) | (0xFU << (PIN_SDA * 4)));
    GPIOB_AFRL |=  ((AF4_I2C1 << (PIN_SCL * 4)) | (AF4_I2C1 << (PIN_SDA * 4)));

    /* --- 12.3  I2C1'in saatini ac ve birimi sifirla ---------------------- */
    RCC_APB1ENR |= RCC_I2C1EN;
    (void)RCC_APB1ENR;

    /* Donanim reset'i: onceki calismadan kalmis takili bir durum varsa
     * (ornegin yarim kalmis transfer) temizlenir. */
    RCC_APB1RSTR |=  RCC_I2C1RST;
    RCC_APB1RSTR &= ~RCC_I2C1RST;

    /* --- 12.4  Ayarlardan once birimi KAPAT ------------------------------ */
    I2C1_CR1 &= ~I2C_CR1_PE;

    /* --- 12.5  FREQ: "APB1 saatim kac MHz?" ------------------------------
     * Cevre birimi zamanlamalari (kurulum/tutma sureleri) hesaplayabilmek
     * icin kendi giris saatini bilmek zorundadir. Bu alan bir BOLUCU DEGIL,
     * sadece bir BILGIDIR ve MHz cinsinden yazilir.
     *      PCLK1 = 16 MHz -> FREQ = 16                                     */
    I2C1_CR2 = (PCLK1_HZ / 1000000U);

    /* --- 12.6  CCR: SCL hizi --------------------------------------------
     * Standart mod (100 kHz) icin RM0090'daki formul:
     *      T_scl_yuksek = CCR * T_pclk1
     *      T_scl        = 2 * CCR * T_pclk1     (yuzde 50 gorev cevrimi)
     * Buradan:
     *      CCR = PCLK1 / (2 * f_scl) = 16e6 / (2 * 100e3) = 80
     * F/S biti 0 birakilir -> standart mod (100 kHz).
     * Neden 400 kHz degil? Uzun/ekransiz jumper kablolarda 100 kHz cok daha
     * affedicidir. Once bu calissin, hizlandirmak sonra tek satir. */
    I2C1_CCR = (PCLK1_HZ / (2U * 100000U));  /* = 80 */

    /* --- 12.7  TRISE: azami yukselme suresi ------------------------------
     * I2C standardi, 100 kHz'de sinyalin 0'dan 1'e cikisinin en fazla
     * 1000 ns surmesine izin verir. Cevre birimi bunu saat cevrimi cinsinden
     * bilmek ister:
     *      TRISE = (1000 ns / T_pclk1) + 1 = (1000ns * 16MHz) + 1 = 17     */
    I2C1_TRISE = (PCLK1_HZ / 1000000U) + 1U; /* = 17 */

    /* --- 12.8  Birimi calistir ------------------------------------------- */
    I2C1_CR1 |= I2C_CR1_PE;

    /* Alinan baytlara ACK ile cevap ver (okuma yaparken gerekli). */
    I2C1_CR1 |= I2C_CR1_ACK;
}


/* ---------------------------------------------------------------------------
 * 13) Yardimci: bir SR1 bayragini bekle
 * ---------------------------------------------------------------------------
 * Donus: 1 = bayrak geldi, 0 = zaman asimi.
 * ------------------------------------------------------------------------- */
static int bayrak_bekle(u32 maske)
{
    u32 sayac = I2C_ZAMAN_ASIMI;
    while ((I2C1_SR1 & maske) == 0U) {
        if (--sayac == 0U) {
            return 0;
        }
    }
    return 1;
}

/* Veri gonderirken TXE/BTF beklerken slave araya NACK atabilir; o yuzden
 * ayni anda AF bayragini da kollamak gerekir. Donus: i2c_durum_t. */
static i2c_durum_t bayrak_bekle_nack_kollayarak(u32 maske)
{
    u32 sayac = I2C_ZAMAN_ASIMI;
    for (;;) {
        u32 sr1 = I2C1_SR1;

        if ((sr1 & maske) != 0U) {
            return I2C_TAMAM;
        }
        if ((sr1 & I2C_SR1_AF) != 0U) {      /* slave "yeter" dedi           */
            I2C1_SR1  = ~I2C_SR1_AF;         /* bayragi temizle (asagida not)*/
            I2C1_CR1 |= I2C_CR1_STOP;
            return I2C_HATA_VERI_NACK;
        }
        if (--sayac == 0U) {
            I2C1_CR1 |= I2C_CR1_STOP;
            return I2C_HATA_ZAMAN_ASIMI;
        }
    }
}
/* NOT -- "I2C1_SR1 = ~I2C_SR1_AF" neden dogru bir temizleme?
 * SR1'deki hata bayraklari "rc_w0" tipindedir: 0 YAZMAK temizler, 1 yazmak
 * hicbir sey yapmaz. Yani tersini yazinca sadece hedefledigimiz bit 0 olur,
 * digerlerine 1 gider ve onlara dokunulmaz. */


/* ---------------------------------------------------------------------------
 * 14) Yardimci: START uret + adres bayti gonder
 * ---------------------------------------------------------------------------
 * adres_yon = (7-bit adres << 1) | yon      yon: 0 = yazma, 1 = okuma
 *
 * Donusteki ONEMLI ayrinti: basarili durumda ADDR bayragi HALA SET
 * birakilir. Cunku ADDR'i temizleme ani (SR1 sonra SR2 okuma) okuma
 * islemlerinde milisaniye hassasiyetinde onemlidir; bu karari cagirana
 * birakiyoruz.
 * ------------------------------------------------------------------------- */
static i2c_durum_t i2c_start_ve_adres(u8 adres_yon)
{
    u32 sayac;

    /* START kosulu iste. Halihazirda bir transfer icindeysek (STOP
     * gondermemissek) donanim bunu otomatik olarak TEKRAR-START yapar. */
    I2C1_CR1 |= I2C_CR1_START;

    /* SB = START gonderildi, hat bizim. */
    if (!bayrak_bekle(I2C_SR1_SB)) {
        I2C1_CR1 |= I2C_CR1_STOP;
        return I2C_HATA_START;
    }

    /* SB bayragi "SR1'i oku, sonra DR'ye yaz" ile temizlenir.
     * bayrak_bekle() zaten SR1'i okudu; simdi DR'ye adresi yaziyoruz. */
    I2C1_DR = adres_yon;

    /* Simdi iki sonuctan biri gelecek:
     *    ADDR -> bir cihaz "benim" deyip ACK verdi
     *    AF   -> kimse cevap vermedi (NACK). Kablo yok, adres yanlis veya
     *            karsi kartin kodu calismiyor demektir. */
    sayac = I2C_ZAMAN_ASIMI;
    for (;;) {
        u32 sr1 = I2C1_SR1;

        if ((sr1 & I2C_SR1_ADDR) != 0U) {
            return I2C_TAMAM;                /* ADDR bilerek temizlenmedi    */
        }
        if ((sr1 & I2C_SR1_AF) != 0U) {
            I2C1_SR1  = ~I2C_SR1_AF;
            I2C1_CR1 |= I2C_CR1_STOP;
            return I2C_HATA_ADRES_NACK;
        }
        if (--sayac == 0U) {
            I2C1_CR1 |= I2C_CR1_STOP;
            return I2C_HATA_ZAMAN_ASIMI;
        }
    }
}

/* ADDR bayragini temizlemenin TEK yolu: once SR1, sonra SR2 okumak.
 * Donanim bu iki okumayi bir "el sikisma" olarak gorur. Temizlenmezse
 * SCL hatti asagida tutulur (clock stretch) ve haberlesme donar. */
static inline void adr_bayragini_temizle(void)
{
    (void)I2C1_SR1;
    (void)I2C1_SR2;
}

/* Hat serbest mi? BUSY, baska bir master konusurken veya hat elektriksel
 * olarak asagi cakiliyken 1'dir. */
static i2c_durum_t hat_bosalmasini_bekle(void)
{
    u32 sayac = I2C_ZAMAN_ASIMI;
    while ((I2C1_SR2 & I2C_SR2_BUSY) != 0U) {
        if (--sayac == 0U) {
            return I2C_HATA_MESGUL;
        }
    }
    return I2C_TAMAM;
}


/* ===========================================================================
 *  15) MASTER YAZMA:  [START][ADRES+W][bayt]...[bayt][STOP]
 * ========================================================================= */
static i2c_durum_t i2c_yaz(u8 adres7, const u8 *veri, u32 adet)
{
    i2c_durum_t durum;
    u32 i;

    durum = hat_bosalmasini_bekle();
    if (durum != I2C_TAMAM) {
        return durum;
    }

    durum = i2c_start_ve_adres((u8)((adres7 << 1) | 0U));  /* yon biti 0 = W */
    if (durum != I2C_TAMAM) {
        return durum;
    }
    adr_bayragini_temizle();

    for (i = 0U; i < adet; i++) {
        /* TXE = "veri register'i bos, siradakini verebilirsin" */
        durum = bayrak_bekle_nack_kollayarak(I2C_SR1_TXE);
        if (durum != I2C_TAMAM) {
            return durum;
        }
        I2C1_DR = veri[i];
    }

    /* BTF = "son bayt kaydirma register'indan da cikti, hat gercekten bos".
     * STOP'u TXE'de degil BTF'de vermek gerekir; TXE'de verirsek son bayt
     * hatta cikmadan hatti kapatmis oluruz. */
    durum = bayrak_bekle_nack_kollayarak(I2C_SR1_BTF);
    if (durum != I2C_TAMAM) {
        return durum;
    }

    I2C1_CR1 |= I2C_CR1_STOP;
    return I2C_TAMAM;
}


/* ===========================================================================
 *  16) MASTER OKUMA:  [START][ADRES+R][bayt]...[bayt+NACK][STOP]
 * ===========================================================================
 *  F4'un I2C'sinde okuma, yazmadan cok daha zahmetlidir. Sebebi su:
 *
 *  Donanimda IKI kademe vardir -- disaridan bit bit dolan KAYDIRMA
 *  register'i ve yazilimin okudugu DR. Master, SON bayta "NACK" gonderip
 *  ardindan STOP vermek zorundadir; ama NACK karari, o bayt daha hatta
 *  gelirken alinmalidir. Yani ACK bitini "bir bayt onceden" kapatmak gerekir.
 *
 *  Bu yuzden ST, AN2824 uygulama notunda uzunluga gore UC AYRI recete verir.
 *  Asagidaki kod o receteleri birebir uygular.
 * ========================================================================= */
static i2c_durum_t i2c_oku(u8 adres7, u8 *tampon, u32 adet)
{
    i2c_durum_t durum;
    u32 i = 0U;

    if (adet == 0U) {
        return I2C_TAMAM;
    }

    /* --- 16.1  ACK/POS ayari ADRESTEN ONCE yapilmali ---------------------
     * Cunku slave, adres ACK'lanir ACKLANMAZ ilk bayti gondermeye baslar;
     * o noktada ayarlar cok gec kalir. */
    if (adet == 1U) {
        I2C1_CR1 &= ~I2C_CR1_ACK;            /* tek bayt -> hemen NACK       */
    } else if (adet == 2U) {
        I2C1_CR1 &= ~I2C_CR1_ACK;
        I2C1_CR1 |=  I2C_CR1_POS;            /* NACK'i "siradaki" bayta uygula*/
    } else {
        I2C1_CR1 |=  I2C_CR1_ACK;            /* akis boyunca ACK             */
    }

    durum = i2c_start_ve_adres((u8)((adres7 << 1) | 1U));  /* yon biti 1 = R */
    if (durum != I2C_TAMAM) {
        I2C1_CR1 &= ~I2C_CR1_POS;
        return durum;
    }

    /* ---------------------------------------------------------------------
     * DURUM A: TEK BAYT
     * ---------------------------------------------------------------------
     * ADDR'i temizlemek slave'e "gonder" demektir. Bir bayt sonra durmak
     * istedigimiz icin STOP'u, bayt hatta akmaya baslamadan hemen
     * ADDR temizliginin ardindan istemeliyiz. Bu iki islemin ARASINA kesme
     * girerse slave ikinci bayti gondermeye baslar ve veri bozulur -- bu
     * yuzden kesmeler kapatiliyor.
     * ------------------------------------------------------------------- */
    if (adet == 1U) {
        kesmeleri_kapat();
        adr_bayragini_temizle();
        I2C1_CR1 |= I2C_CR1_STOP;
        kesmeleri_ac();

        if (!bayrak_bekle(I2C_SR1_RXNE)) {
            return I2C_HATA_ZAMAN_ASIMI;
        }
        tampon[0] = (u8)I2C1_DR;
    }
    /* ---------------------------------------------------------------------
     * DURUM B: IKI BAYT  (POS numarasi)
     * ---------------------------------------------------------------------
     * Iki bayti de hicbirini okumadan biriktiririz: BTF geldiginde 1. bayt
     * DR'de, 2. bayt kaydirma register'indadir. POS=1 sayesinde NACK zaten
     * 2. bayta gitmistir. Simdi STOP verip ikisini pesi sira okuruz.
     * ------------------------------------------------------------------- */
    else if (adet == 2U) {
        adr_bayragini_temizle();

        if (!bayrak_bekle(I2C_SR1_BTF)) {
            I2C1_CR1 &= ~I2C_CR1_POS;
            return I2C_HATA_ZAMAN_ASIMI;
        }

        kesmeleri_kapat();
        I2C1_CR1 |= I2C_CR1_STOP;
        tampon[0] = (u8)I2C1_DR;
        kesmeleri_ac();
        tampon[1] = (u8)I2C1_DR;

        I2C1_CR1 &= ~I2C_CR1_POS;            /* ayari geri al                */
    }
    /* ---------------------------------------------------------------------
     * DURUM C: UC VE DAHA FAZLA BAYT
     * ---------------------------------------------------------------------
     * Son UC bayta kadar sade bir "RXNE bekle, oku" dongusu isler.
     * Geriye 3 bayt kalinca AN2824'un finali devreye girer:
     *      BTF bekle -> ACK'i kapat -> N-2'yi oku
     *      BTF bekle -> STOP ver    -> N-1'i oku -> N'i oku
     * Boylece son bayta NACK gitmis ve STOP tam zamaninda verilmis olur.
     * ------------------------------------------------------------------- */
    else {
        u32 kalan = adet;

        adr_bayragini_temizle();

        while (kalan > 3U) {
            if (!bayrak_bekle(I2C_SR1_RXNE)) {
                I2C1_CR1 |= I2C_CR1_STOP;
                return I2C_HATA_ZAMAN_ASIMI;
            }
            tampon[i++] = (u8)I2C1_DR;
            kalan--;
        }

        /* Geriye tam 3 bayt kaldi: N-2, N-1, N */
        if (!bayrak_bekle(I2C_SR1_BTF)) {    /* DR = N-2, kaydirma = N-1     */
            I2C1_CR1 |= I2C_CR1_STOP;
            return I2C_HATA_ZAMAN_ASIMI;
        }

        kesmeleri_kapat();
        I2C1_CR1 &= ~I2C_CR1_ACK;            /* son bayta NACK gidecek       */
        tampon[i++] = (u8)I2C1_DR;           /* N-2 okundu                   */
        kesmeleri_ac();

        if (!bayrak_bekle(I2C_SR1_BTF)) {    /* DR = N-1, kaydirma = N       */
            I2C1_CR1 |= I2C_CR1_STOP;
            return I2C_HATA_ZAMAN_ASIMI;
        }

        kesmeleri_kapat();
        I2C1_CR1 |= I2C_CR1_STOP;            /* STOP'u simdi kuyruga koy     */
        tampon[i++] = (u8)I2C1_DR;           /* N-1 okundu                   */
        kesmeleri_ac();

        if (!bayrak_bekle(I2C_SR1_RXNE)) {
            return I2C_HATA_ZAMAN_ASIMI;
        }
        tampon[i++] = (u8)I2C1_DR;           /* N okundu (sonuncu)           */
    }

    /* Bir sonraki okuma icin ACK'i tekrar ac. */
    I2C1_CR1 |= I2C_CR1_ACK;
    return I2C_TAMAM;
}


/* ===========================================================================
 *  17) REGISTER OKUMA:  yazma + TEKRAR-START + okuma
 * ===========================================================================
 *  I2C sensorlerinin klasik erisim sekli:
 *
 *    [START][ADRES+W][register no][TEKRAR-START][ADRES+R][veri...][STOP]
 *                                  ^^^^^^^^^^^^
 *  Arada STOP VERMIYORUZ. Cunku STOP verirsek hat serbest kalir ve
 *  (cok master'li bir sistemde) baska biri araya girip slave'in register
 *  isaretcisini degistirebilir. Tekrar-START hattin sahipligini birakmadan
 *  yon degistirmenin yoludur.
 * ========================================================================= */
static i2c_durum_t i2c_register_oku(u8 adres7, u8 reg, u8 *tampon, u32 adet)
{
    i2c_durum_t durum;

    durum = hat_bosalmasini_bekle();
    if (durum != I2C_TAMAM) {
        return durum;
    }

    /* --- 1. faz: hangi register'i istedigimizi soyle (STOP YOK) --------- */
    durum = i2c_start_ve_adres((u8)((adres7 << 1) | 0U));
    if (durum != I2C_TAMAM) {
        return durum;
    }
    adr_bayragini_temizle();

    durum = bayrak_bekle_nack_kollayarak(I2C_SR1_TXE);
    if (durum != I2C_TAMAM) {
        return durum;
    }
    I2C1_DR = reg;

    /* Tekrar-START'i vermeden once baytin gercekten cikmasini bekle. */
    durum = bayrak_bekle_nack_kollayarak(I2C_SR1_BTF);
    if (durum != I2C_TAMAM) {
        return durum;
    }

    /* --- 2. faz: yon degistir ve oku ------------------------------------
     * i2c_oku() icindeki i2c_start_ve_adres() yeni bir START ister; hat
     * halen bizde oldugu icin donanim bunu TEKRAR-START olarak uretir. */
    return i2c_oku(adres7, tampon, adet);
}


/* ===========================================================================
 *  18) main
 * ========================================================================= */
int main(void)
{
    u8  tampon[4];                   /* slave'den okunan 4 register          */
    u8  echo_degeri  = 0U;           /* her turda degisen test deseni        */
    u32 led_durumu   = 0U;           /* Nucleo LED'i icin 0/1                */
    u32 turuncu_acik = 0U;           /* kalp atisi LED'inin durumu           */

    ledleri_kur();
    systick_baslat();
    i2c_kur();

    for (;;) {
        i2c_durum_t durum;
        u8 gonderilecek[2];

        /* --- Kalp atisi: kodun donmedigini gosterir --------------------- */
        turuncu_acik = !turuncu_acik;
        if (turuncu_acik) {
            led_yak(LED_TURUNCU);
        } else {
            led_sondur(LED_TURUNCU);
        }

        /* Bu turun sonucunu bekleyen LED'leri sifirla. */
        led_sondur(LED_YESIL | LED_KIRMIZI | LED_MAVI);

        /* --- ADIM 1: Nucleo'nun LED'ini kontrol et ---------------------- *
         * [START][0x42+W][0x01][0 veya 1][STOP]                            */
        led_durumu = !led_durumu;
        gonderilecek[0] = REG_LED_CTRL;
        gonderilecek[1] = (u8)led_durumu;
        durum = i2c_yaz(SLAVE_ADRES, gonderilecek, 2U);

        /* --- ADIM 2: ECHO register'ina bir desen yaz -------------------- */
        if (durum == I2C_TAMAM) {
            echo_degeri++;
            gonderilecek[0] = REG_ECHO;
            gonderilecek[1] = echo_degeri;
            durum = i2c_yaz(SLAVE_ADRES, gonderilecek, 2U);
        }

        /* --- ADIM 3: 0x00'dan itibaren 4 register'i oku ----------------- *
         * Tek istekte 4 bayt: slave'in register isaretcisi her baytta
         * kendiliginden ilerler (tipki gercek sensorlerdeki gibi).         */
        if (durum == I2C_TAMAM) {
            durum = i2c_register_oku(SLAVE_ADRES, REG_WHO_AM_I, tampon, 4U);
        }

        /* --- ADIM 4: Sonucu degerlendir ve LED'lere yansit -------------- */
        if (durum != I2C_TAMAM) {
            led_yak(LED_KIRMIZI);

            /* "Karsi taraftan hic ses yok" sinifi: kablo yok, pull-up yok
             * ya da Nucleo'ya kod yuklenmemis. Veri seviyesindeki NACK'ten
             * ayirmak icin ayri bir LED veriyoruz. */
            if (durum == I2C_HATA_ADRES_NACK ||
                durum == I2C_HATA_ZAMAN_ASIMI ||
                durum == I2C_HATA_MESGUL ||
                durum == I2C_HATA_START) {
                led_yak(LED_MAVI);
            }

            /* Hata ne olursa olsun cevre birimini bastan kuruyoruz. Yarim
             * kalmis bir transfer geride takili bir START biti veya sonsuza
             * kadar 1 kalan BUSY birakabilir; bir sonraki tur bunlarla
             * baslarsa hicbir zaman toparlanamaz. Sifirlamak ucuz: 500 ms'de
             * bir, sadece hata varken calisir. */
            i2c_kur();
        }
        else if (tampon[REG_WHO_AM_I] != WHO_AM_I_BEKLENEN ||
                 tampon[REG_ECHO]     != echo_degeri) {
            /* Haberlesme yurudu ama icerik yanlis: adres dogru bir baska
             * cihazla mi konusuyoruz, yoksa hatta gurultu mu var? */
            led_yak(LED_KIRMIZI);
        }
        else {
            /* Kimlik dogru, echo dogru: baglanti saglam. */
            led_yak(LED_YESIL);
        }

        bekle_ms(500U);
    }

    return 0;   /* buraya asla ulasilmaz */
}
