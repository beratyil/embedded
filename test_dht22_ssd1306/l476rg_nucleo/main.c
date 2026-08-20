/*
 * ============================================================================
 *  main.c  --  NUCLEO-L476RG  |  DHT22 sicaklik/nem  ->  SSD1306 OLED
 * ============================================================================
 *
 *  HEDEF DONANIM : STM32L476RG, Cortex-M4F, 1 MB Flash, 96 KB SRAM1 + 32 KB SRAM2
 *  KART          : NUCLEO-L476RG (MB1136)
 *  SENSOR        : DHT22 / AM2302  (tek telli, kendi protokolu)
 *  EKRAN         : SSD1306 128x64 OLED  (I2C)
 *
 *  KURAL: Hicbir kutuphane yok (HAL / LL / CMSIS / libc yok). Sadece register.
 *
 * ----------------------------------------------------------------------------
 *  KABLOLAMA
 * ----------------------------------------------------------------------------
 *      SSD1306 OLED            NUCLEO-L476RG
 *      ------------            -------------
 *      VCC   ----------------  3V3   CN6-4
 *      GND   ----------------  GND   CN6-6
 *      SCL   ----------------  PB8   CN5-10 (D15)   I2C1_SCL, AF4
 *      SDA   ----------------  PB9   CN5-9  (D14)   I2C1_SDA, AF4
 *
 *      DHT22 / AM2302          NUCLEO-L476RG
 *      --------------          -------------
 *      VCC (pin 1) ----------  3V3   CN6-4
 *      DATA(pin 2) ----------  PA10  CN9-3  (D2)
 *      NC  (pin 3)              -- bagli degil
 *      GND (pin 4) ----------  GND   CN6-6
 *
 *  DHT22 DATA hattina 3.3V'a 4.7k - 10k PULL-UP gerekir. Hazir modul
 *  (3 pinli kart) kullaniyorsaniz uzerinde zaten vardir, eklemeyin.
 *  Ciplak 4 pinli sensor kullaniyorsaniz DIRENCI EKLEYIN: ic pull-up
 *  (~40 kohm) 50 us'lik kenarlar icin zayif kalir, veri bozulur.
 *
 *  BESLEME NOTU: ikisi birden ~25 mA ceker (OLED ~20, DHT22 ~1.5). Nucleo'nun
 *  USB'den 300 mA'lik butcesine gore rahat. (Kamera projesindeki gibi bir
 *  besleme sorunu burada beklenmiyor.)
 *
 *  GERILIM: her ikisi de 3V3'ten besleniyor, dolayisiyla mantik seviyeleri
 *  STM32 ile birebir ayni. Seviye cevirici yok, 5V yok, tolerans derdi yok.
 *  (DHT22 datasheet: besleme 3.3-6V. SSD1306 modulleri genelde 3.3-5V.)
 *
 * ----------------------------------------------------------------------------
 *  DHT22 PROTOKOLU  --  I2C degil, SPI degil, UART degil
 * ----------------------------------------------------------------------------
 *  Tek telli, tamamen zamanlamaya dayali kendine ozgu bir protokol. Cipte
 *  bunu yapan bir cevre birimi YOK; her kenari yazilimla olcecegiz.
 *
 *      1. Biz hatti en az 1 ms LOW cekeriz          (baslat isareti)
 *      2. Birakiriz; sensor 80 us LOW + 80 us HIGH ile cevap verir
 *      3. Ardindan 40 bit gelir. Her bit:
 *             50 us LOW  +  26-28 us HIGH  ->  bit 0
 *             50 us LOW  +  70    us HIGH  ->  bit 1
 *         Yani bitin degeri HIGH suresinde saklidir; esik ~50 us.
 *      4. 40 bit = 5 bayt:
 *             [nem_tam][nem_ondalik][sic_tam][sic_ondalik][saglama]
 *         saglama = ilk dort baytin toplaminin alt 8 biti
 *
 *  (Kaynak: DHT22 datasheet, Aosong Electronics, Bolum 6.)
 *
 *  OLCUM ARALIGI: datasheet "Collecting period should be : >2 second" der.
 *  Daha sik okumak sensoru isitir ve degeri bozar. Ana dongu 2 s bekler.
 *
 * ----------------------------------------------------------------------------
 *  NEDEN TIM2 SAYAC OLARAK KULLANILIYOR?
 * ----------------------------------------------------------------------------
 *  26-28 us ile 70 us'yi ayirt etmemiz gerekiyor. Bunu "dongude sayarak"
 *  yapmak derleyici optimizasyonuna ve saat hizina bagimli, kirilgan bir
 *  cozumdur -- -O2'yi -O0 yapinca calismaz hale gelir.
 *
 *  Bunun yerine TIM2'yi 1 MHz'e bolup serbest calisan bir MIKROSANIYE
 *  SAYACI yapiyoruz. Olcum artik "iki CNT okumasinin farki" oluyor:
 *  derleyiciden ve saat degisikliginden bagimsiz.
 *
 *  TIM2 ozellikle secildi: L4'te TIM2 ve TIM5 32 BITTIR (TIM3/TIM4 16 bit).
 *  32 bit sayac 71 dakikada bir tasar; 16 bit olsaydi 65 ms'de tasar ve
 *  fark hesabinda surekli tasma dusunmek gerekirdi.
 *
 * ----------------------------------------------------------------------------
 *  DHT22 PINI NEDEN ACIK DRENAJ?
 * ----------------------------------------------------------------------------
 *  Protokol cift yonlu: once biz suruyoruz, sonra sensor suruyor. Klasik
 *  yaklasim her yon degisiminde MODER'i cikis/giris arasinda degistirmektir.
 *
 *  Bunun yerine pini BIR KEZ acik drenaj cikis yapiyoruz:
 *      ODR = 0  ->  hat asagi cekilir (biz suruyoruz)
 *      ODR = 1  ->  cikis yuksek empedansa gider, pull-up hatti yukari ceker
 *                   (yani "biraktik" demek) -- ve IDR hattin GERCEK halini
 *                   okumaya devam eder, sensor surerken bile.
 *
 *  Boylece yon degisimi tek bir ODR yazmasina iner. Hem daha hizli hem de
 *  MODER degisimi sirasinda olusabilecek kisa devre/glitch riski yok.
 *
 * ----------------------------------------------------------------------------
 *  SSD1306  --  cerceve tamponu ve sayfa duzeni
 * ----------------------------------------------------------------------------
 *  Ekranin video RAM'i satir satir DEGIL, "sayfa" halinde adreslenir:
 *  bir bayt ekranda UST USTE 8 PIKSELDIR. 128x64 ekran = 8 sayfa x 128 sutun
 *  = 1024 bayt.
 *
 *      bayt bit 0 -> sayfanin en ust pikseli
 *      bayt bit 7 -> sayfanin en alt pikseli
 *
 *  Font tablomuz da ayni duzende (bkz. font5x7.h), bu yuzden karakter
 *  baytlarini bit dondurmeden dogrudan kullanabiliyoruz.
 *
 *  Ekrani her seferinde bastan cizmiyoruz: RAM'de 1024 baytlik bir cerceve
 *  tamponu tutup, hazir olunca tek seferde gonderiyoruz. Boylece ekranda
 *  yarim cizilmis ara goruntu (titreme) olusmaz.
 *
 *  TUZAK -- I2C_CR2'deki NBYTES alani 8 BITTIR, yani tek islemde en fazla
 *  255 bayt gonderilebilir. 1024 baytlik cerceve tek seferde SIGMAZ.
 *  Cozum: sayfa sayfa gondermek (8 islem x 129 bayt). SSD1306'nin RAM
 *  isaretcisi islemler arasinda kendiliginden ilerledigi icin bu sorunsuz
 *  calisir. (Alternatif RELOAD kipidir ama gereksiz karmasiklik olurdu.)
 * ============================================================================
 */

#include "font5x7.h"


/* ---------------------------------------------------------------------------
 * 0) DERLEME SECENEKLERI  --  Makefile'dan -D ile ezilebilir
 * ------------------------------------------------------------------------- */

/* UART hizi. 16 MHz'de BRR yuvarlama hatasi 115200 icin %0.08. */
#ifndef UART_BAUD
#define UART_BAUD      115200U
#endif

/* Iki olcum arasindaki bekleme (ms). Datasheet en az 2000 ms ister. */
#ifndef OLCUM_ARALIGI_MS
#define OLCUM_ARALIGI_MS  2000U
#endif

#if OLCUM_ARALIGI_MS < 2000
#error "DHT22 datasheet'i 2 saniyeden sik olcumu yasaklar (Bolum 7)"
#endif


/* ---------------------------------------------------------------------------
 * 1) TEMEL TIPLER
 * ------------------------------------------------------------------------- */
typedef unsigned char          u8;
typedef unsigned int           u32;
typedef signed int             i32;
typedef volatile unsigned int  vu32;

#define REG32(adres)  (*(vu32 *)(adres))


/* ---------------------------------------------------------------------------
 * 2) HAFIZA HARITASI  (RM0351, Tablo 1)
 * ---------------------------------------------------------------------------
 *   0x4000 0000  APB1  <- TIM2 (0x4000 0000), I2C1 (0x4000 5400),
 *                         USART2 (0x4000 4400)
 *   0x4002 0000  AHB1  <- RCC  (0x4002 1000)
 *   0x4800 0000  AHB2  <- GPIO portlari
 * ------------------------------------------------------------------------- */
#define RCC_BASE      0x40021000U
#define GPIOA_BASE    0x48000000U       /* PA2 UART, PA5 LD2, PA10 DHT22     */
#define GPIOB_BASE    0x48000400U       /* PB8/PB9 I2C1                      */
#define TIM2_BASE     0x40000000U
#define I2C1_BASE     0x40005400U
#define USART2_BASE   0x40004400U
#define SYSTICK_BASE  0xE000E010U


/* ---------------------------------------------------------------------------
 * 3) RCC  --  Saat kapilari ve kaynak secimi
 * ------------------------------------------------------------------------- */
#define RCC_CR        REG32(RCC_BASE + 0x00U)
#define RCC_CFGR      REG32(RCC_BASE + 0x08U)
#define RCC_APB1RSTR1 REG32(RCC_BASE + 0x38U)
#define RCC_AHB2ENR   REG32(RCC_BASE + 0x4CU)
#define RCC_APB1ENR1  REG32(RCC_BASE + 0x58U)
#define RCC_CCIPR     REG32(RCC_BASE + 0x88U)

#define RCC_HSION     (1U << 8)
#define RCC_HSIRDY    (1U << 10)

#define RCC_GPIOAEN   (1U << 0)
#define RCC_GPIOBEN   (1U << 1)

#define RCC_TIM2EN    (1U << 0)         /* APB1ENR1 bit 0  (RM0351 6.4.19)   */
#define RCC_USART2EN  (1U << 17)
#define RCC_I2C1EN    (1U << 21)
#define RCC_I2C1RST   (1U << 21)

/* CFGR: SW[1:0] 00=MSI 01=HSI16 ; SWS[3:2] gerceklesen secim */
#define CFGR_SW_HSI16   (1U << 0)
#define CFGR_SWS_MASK   (3U << 2)
#define CFGR_SWS_HSI16  (1U << 2)

/* CCIPR: bit 3:2 USART2SEL, bit 13:12 I2C1SEL ; 10 = HSI16 */
#define CCIPR_USART2SEL_HSI16  (2U << 2)
#define CCIPR_I2C1SEL_HSI16    (2U << 12)

#define HSI16_HZ      16000000U


/* ---------------------------------------------------------------------------
 * 4) GPIO
 * ------------------------------------------------------------------------- */
#define GPIO_MODER(t)    REG32((t) + 0x00U)
#define GPIO_OTYPER(t)   REG32((t) + 0x04U)
#define GPIO_OSPEEDR(t)  REG32((t) + 0x08U)
#define GPIO_PUPDR(t)    REG32((t) + 0x0CU)
#define GPIO_IDR(t)      REG32((t) + 0x10U)
#define GPIO_BSRR(t)     REG32((t) + 0x18U)
#define GPIO_AFR(t, pin) REG32((t) + 0x20U + (((pin) >> 3) * 4U))

#define MODER_CIKIS   1U
#define MODER_AF      2U

#define AF4_I2C1      4U
#define AF7_USART2    7U

#define PIN_TX        2U     /* PA2   USART2_TX -> ST-LINK sanal COM         */
#define PIN_LED       5U     /* PA5   LD2 (yesil)                            */
#define PIN_DHT       10U    /* PA10  CN9-3 / D2   (DS10198: FT_lu)          */
#define PIN_SCL       8U     /* PB8   CN5-10 / D15                           */
#define PIN_SDA       9U     /* PB9   CN5-9  / D14                           */

#define M_LED         (1U << PIN_LED)


/* ---------------------------------------------------------------------------
 * 5) TIM2  --  serbest calisan 1 MHz (mikrosaniye) sayaci
 * ---------------------------------------------------------------------------
 * Ofsetler RM0351 Bolum 31.4 ile dogrulandi.
 * ------------------------------------------------------------------------- */
#define TIM2_CR1      REG32(TIM2_BASE + 0x00U)
#define TIM2_EGR      REG32(TIM2_BASE + 0x14U)
#define TIM2_CNT      REG32(TIM2_BASE + 0x24U)
#define TIM2_PSC      REG32(TIM2_BASE + 0x28U)
#define TIM2_ARR      REG32(TIM2_BASE + 0x2CU)

#define TIM_CR1_CEN   (1U << 0)
#define TIM_EGR_UG    (1U << 0)


/* ---------------------------------------------------------------------------
 * 6) SysTick  --  ms gecikme (kesmesiz, COUNTFLAG yoklamali)
 * ------------------------------------------------------------------------- */
#define STK_CTRL      REG32(SYSTICK_BASE + 0x00U)
#define STK_LOAD      REG32(SYSTICK_BASE + 0x04U)
#define STK_VAL       REG32(SYSTICK_BASE + 0x08U)

#define STK_ENABLE    (1U << 0)
#define STK_CLKSOURCE (1U << 2)
#define STK_COUNTFLAG (1U << 16)


/* ---------------------------------------------------------------------------
 * 7) I2C1 MASTER  (RM0351, Bolum 39.7)
 * ------------------------------------------------------------------------- */
#define I2C1_CR1      REG32(I2C1_BASE + 0x00U)
#define I2C1_CR2      REG32(I2C1_BASE + 0x04U)
#define I2C1_TIMINGR  REG32(I2C1_BASE + 0x10U)
#define I2C1_ISR      REG32(I2C1_BASE + 0x18U)
#define I2C1_ICR      REG32(I2C1_BASE + 0x1CU)
#define I2C1_TXDR     REG32(I2C1_BASE + 0x28U)

#define I2C_CR1_PE        (1U << 0)

#define I2C_CR2_START     (1U << 13)
#define I2C_CR2_AUTOEND   (1U << 25)
#define I2C_CR2_NBYTES(n) (((u32)(n) & 0xFFU) << 16)

#define I2C_ISR_TXIS      (1U << 1)
#define I2C_ISR_NACKF     (1U << 4)
#define I2C_ISR_STOPF     (1U << 5)
#define I2C_ISR_BUSY      (1U << 15)

#define I2C_ICR_NACKCF    (1U << 4)
#define I2C_ICR_STOPCF    (1U << 5)
#define I2C_ICR_BERRCF    (1U << 8)
#define I2C_ICR_ARLOCF    (1U << 9)

#define I2C_ZAMAN_ASIMI   200000U


/* ---------------------------------------------------------------------------
 * 8) USART2  (RM0351, Bolum 40.8)
 * ------------------------------------------------------------------------- */
#define USART2_CR1    REG32(USART2_BASE + 0x00U)
#define USART2_BRR    REG32(USART2_BASE + 0x0CU)
#define USART2_ISR    REG32(USART2_BASE + 0x1CU)
#define USART2_TDR    REG32(USART2_BASE + 0x28U)

#define USART_CR1_UE  (1U << 0)
#define USART_CR1_TE  (1U << 3)
#define USART_ISR_TXE (1U << 7)


/* ---------------------------------------------------------------------------
 * 9) SSD1306 KOMUTLARI  (datasheet Rev 1.1, Bolum 10 "Command Table")
 * ---------------------------------------------------------------------------
 * Kontrol bayti (Bolum 8.1.5): Co=0, D/C#=0 -> 0x00 = bundan sonrasi KOMUT
 *                              Co=0, D/C#=1 -> 0x40 = bundan sonrasi VERI
 * Adres (Bolum 8.1.5): "0111100" (0x3C) veya "0111101" (0x3D), SA0 pinine
 * gore. Modullerin cogu 0x3C'dir; asagida ikisi de deneniyor.
 * ------------------------------------------------------------------------- */
#define SSD_KOMUT           0x00U
#define SSD_VERI            0x40U

#define SSD_ADRES_A         0x3CU
#define SSD_ADRES_B         0x3DU

#define SSD_KONTRAST        0x81U   /* + 1 bayt deger                        */
#define SSD_RAM_DEVAM       0xA4U   /* icerigi goster (test deseni degil)    */
#define SSD_NORMAL          0xA6U   /* ters cevirme yok                      */
#define SSD_EKRAN_KAPALI    0xAEU
#define SSD_EKRAN_ACIK      0xAFU
#define SSD_BELLEK_KIPI     0x20U   /* + 1 bayt: 00=yatay 01=dikey 02=sayfa  */
#define SSD_SUTUN_ARALIK    0x21U   /* + baslangic, bitis                    */
#define SSD_SAYFA_ARALIK    0x22U   /* + baslangic, bitis                    */
#define SSD_BASLANGIC_SATIR 0x40U   /* 0x40..0x7F                            */
#define SSD_SEG_TERS        0xA1U   /* sutun 127 -> SEG0 (yatay cevirme)     */
#define SSD_COM_TERS        0xC8U   /* COM tarama yonu ters (dikey cevirme)  */
#define SSD_COKLAMA         0xA8U   /* + 1 bayt: satir sayisi - 1            */
#define SSD_DIKEY_KAYDIR    0xD3U   /* + 1 bayt                              */
#define SSD_SAAT_BOLME      0xD5U   /* + 1 bayt                              */
#define SSD_ON_SARJ         0xD9U   /* + 1 bayt                              */
#define SSD_COM_PIN         0xDAU   /* + 1 bayt                              */
#define SSD_VCOMH           0xDBU   /* + 1 bayt                              */
#define SSD_SARJ_POMPASI    0x8DU   /* + 1 bayt: 0x14 = ac                   */

#define SSD_GENISLIK   128U
#define SSD_YUKSEKLIK  64U
#define SSD_SAYFA      (SSD_YUKSEKLIK / 8U)     /* 8 sayfa */

/* Cerceve tamponu: 128 x 8 = 1024 bayt. Ekrani parca parca degil, hazir
 * olunca tek seferde gonderiyoruz -- boylece titreme olmaz. */
static u8 cerceve[SSD_GENISLIK * SSD_SAYFA];

/* Calisma aninda bulunan ekran adresi (0 = ekran yok). */
static u8 ssd_adres;


/* ---------------------------------------------------------------------------
 * 10) DHT22 sonucu
 * ------------------------------------------------------------------------- */
typedef struct {
    i32 sicaklik;    /* onda bir derece: 234 = 23.4 C, -55 = -5.5 C          */
    i32 nem;         /* onda bir yuzde:  456 = %45.6                         */
} dht_olcum_t;

/* Hata kodlari -- her biri protokolun FARKLI bir adiminda takildigimizi
 * soyler. Tek bir "okuma basarisiz" yerine bunlari ayirmak, arizanin
 * kabloda mi zamanlamada mi oldugunu dogrudan gosterir. */
#define DHT_TAMAM         0
#define DHT_CEVAP_YOK    -1   /* baslat isaretinden sonra sensor sessiz      */
#define DHT_BIT_ZAMAN    -2   /* 40 bitin ortasinda kenar gelmedi            */
#define DHT_SAGLAMA      -3   /* bitler geldi ama saglama tutmadi            */


/* ===========================================================================
 *  11) SAAT  --  sistem saatini HSI16'ya al
 * ===========================================================================
 *  L4 reset sonrasi MSI ile 4 MHz'de baslar. HSI16'ya geciyoruz:
 *    - TIM2'yi tam 1 MHz'e bolmek kolaylasir (16 / 16 = 1)
 *    - MSI'nin frekansi ayarlanabilir; HSI16 her zaman 16 MHz'dir, yani
 *      mikrosaniye olcumu ileride baska bir ayardan etkilenmez
 *
 *  FLASH BEKLEME DURUMU GEREKMIYOR: RM0351 Tablo 11'e gore VCORE Range 1'de
 *  HCLK <= 16 MHz icin 0 wait state gecerlidir ve reset degeri zaten 0'dir.
 * ========================================================================= */
static void saat_kur(void)
{
    RCC_CR |= RCC_HSION;
    while ((RCC_CR & RCC_HSIRDY) == 0U) { }

    RCC_CFGR = (RCC_CFGR & ~3U) | CFGR_SW_HSI16;
    while ((RCC_CFGR & CFGR_SWS_MASK) != CFGR_SWS_HSI16) { }

    /* I2C ve USART'i dogrudan HSI16'ya bagla: ileride cekirdegi PLL ile
     * hizlandirsaniz bile bu iki birim 16 MHz'de kalir, TIMINGR ve BRR
     * degerleri bozulmaz. */
    RCC_CCIPR &= ~((3U << 2) | (3U << 12));
    RCC_CCIPR |= CCIPR_USART2SEL_HSI16 | CCIPR_I2C1SEL_HSI16;
}


/* ===========================================================================
 *  12) TIM2  --  1 MHz serbest sayac
 * ========================================================================= */
static void tim2_kur(void)
{
    RCC_APB1ENR1 |= RCC_TIM2EN;
    (void)RCC_APB1ENR1;

    /* APB1 on bolucusu reset'te /1 oldugu icin TIM2'nin saati = PCLK1 =
     * 16 MHz. PSC bir EKSIGI yazilir: 16 MHz / (15+1) = 1 MHz -> 1 tik = 1 us */
    TIM2_PSC = 16U - 1U;

    /* 32 bit tam aralik. Sayac tasinca 0'dan devam eder; fark hesabini
     * isaretsiz yaptigimiz icin tasma kendiliginden dogru sonuc verir
     * (ornek: 5 - 0xFFFFFFFE = 7, modulo aritmetigi). */
    TIM2_ARR = 0xFFFFFFFFU;

    /* UG: PSC'yi golge register'a HEMEN yukle. Bu yazilmazsa yeni bolme
     * orani ilk guncelleme olayina kadar gecerli olmaz ve ilk olcumler
     * 16 kat yanlis cikar -- fark edilmesi zor bir hata. */
    TIM2_EGR = TIM_EGR_UG;

    TIM2_CR1 = TIM_CR1_CEN;
}

static inline u32 us_simdi(void)
{
    return TIM2_CNT;
}

/* Mesgul bekleme, mikrosaniye. */
static void bekle_us(u32 us)
{
    u32 t0 = us_simdi();
    while ((us_simdi() - t0) < us) { }
}

/* SysTick ile ms gecikme. TIM2'yi mesgul etmiyoruz ki olcum icin serbest
 * kalsin (ve 32 bit sayacin surekliligi bozulmasin). */
static void bekle_ms(u32 ms)
{
    STK_CTRL = 0U;
    STK_LOAD = (HSI16_HZ / 1000U) - 1U;
    STK_VAL  = 0U;
    STK_CTRL = STK_CLKSOURCE | STK_ENABLE;

    while (ms-- > 0U) {
        while ((STK_CTRL & STK_COUNTFLAG) == 0U) { }
    }
    STK_CTRL = 0U;
}


/* ===========================================================================
 *  13) LED (LD2 / PA5)  --  bu projede serbest
 * ===========================================================================
 *  Kamera projesinde PA5 = SPI1_SCK oldugu icin LD2 kullanilamiyordu.
 *  Burada SPI yok, dolayisiyla LD2 tekrar durum LED'i olarak kullanilabilir:
 *  her BASARILI olcumde durum degistirir. Yanip sonmuyorsa sensor okunmuyor.
 * ========================================================================= */
static void led_kur(void)
{
    RCC_AHB2ENR |= RCC_GPIOAEN;
    (void)RCC_AHB2ENR;

    /* DIKKAT: GPIOA'da PA13/PA14 = SWDIO/SWCLK. MODER'e "=" ile yazmak
     * ST-LINK baglantisini keser. Daima oku-maskele-yaz. */
    GPIO_MODER(GPIOA_BASE)   &= ~(3U << (PIN_LED * 2U));
    GPIO_MODER(GPIOA_BASE)   |=  (MODER_CIKIS << (PIN_LED * 2U));
    GPIO_OTYPER(GPIOA_BASE)  &= ~M_LED;
    GPIO_OSPEEDR(GPIOA_BASE) &= ~(3U << (PIN_LED * 2U));
    GPIO_PUPDR(GPIOA_BASE)   &= ~(3U << (PIN_LED * 2U));

    GPIO_BSRR(GPIOA_BASE) = (M_LED << 16);
}

static void led_degistir(void)
{
    if ((GPIO_IDR(GPIOA_BASE) & M_LED) != 0U) {
        GPIO_BSRR(GPIOA_BASE) = (M_LED << 16);
    } else {
        GPIO_BSRR(GPIOA_BASE) = M_LED;
    }
}


/* ===========================================================================
 *  14) USART2  --  115200 8N1, yalnizca gonderme
 * ========================================================================= */
static void uart_kur(void)
{
    RCC_AHB2ENR |= RCC_GPIOAEN;
    (void)RCC_AHB2ENR;

    GPIO_MODER(GPIOA_BASE)   &= ~(3U << (PIN_TX * 2U));
    GPIO_MODER(GPIOA_BASE)   |=  (MODER_AF << (PIN_TX * 2U));
    GPIO_OTYPER(GPIOA_BASE)  &= ~(1U << PIN_TX);
    GPIO_OSPEEDR(GPIOA_BASE) |=  (3U << (PIN_TX * 2U));
    GPIO_AFR(GPIOA_BASE, PIN_TX) &= ~(0xFU << ((PIN_TX & 7U) * 4U));
    GPIO_AFR(GPIOA_BASE, PIN_TX) |=  (AF7_USART2 << ((PIN_TX & 7U) * 4U));

    RCC_APB1ENR1 |= RCC_USART2EN;
    (void)RCC_APB1ENR1;

    USART2_CR1 = 0U;
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
 *  15) METIN BICIMLENDIRME  --  kayan nokta YOK
 * ===========================================================================
 *  printf yok, float yok. Zaten olmamali: DHT22 degerleri onda bir cozunurlukte
 *  TAM SAYI olarak geliyor (234 = 23.4). Bunu float'a cevirmek hem gereksiz
 *  hem de yazilim float kutuphanesini (birkac KB) projeye sokardi.
 *
 *  Onda biri metne cevirirken sadece araya nokta koyuyoruz.
 * ========================================================================= */

/* Sonuna '\0' koyar. hedef en az 12 bayt olmali. Ornek: 234 -> "23.4" */
static void ondalik_metin(char *hedef, i32 onda_bir)
{
    char gecici[12];
    int  n = 0;
    int  k = 0;
    u32  mutlak;

    if (onda_bir < 0) {
        hedef[k++] = '-';
        mutlak = (u32)(-onda_bir);
    } else {
        mutlak = (u32)onda_bir;
    }

    /* Once ondalik hane, sonra tam kisim -- ters sirada uretip cevirecegiz. */
    gecici[n++] = (char)('0' + (mutlak % 10U));
    gecici[n++] = '.';
    mutlak /= 10U;

    if (mutlak == 0U) {
        gecici[n++] = '0';
    }
    while (mutlak > 0U) {
        gecici[n++] = (char)('0' + (mutlak % 10U));
        mutlak /= 10U;
    }

    while (n > 0) {
        hedef[k++] = gecici[--n];
    }
    hedef[k] = '\0';
}

/* Isaretsiz tam sayiyi metne cevirir. Sonuna '\0' koyar.
 * hedef en az 11 bayt olmali (32 bit icin en fazla 10 hane + sonlandirici). */
static void sayi_metin(char *hedef, u32 deger)
{
    char gecici[11];
    int  n = 0;
    int  k = 0;

    if (deger == 0U) {
        gecici[n++] = '0';
    }
    while (deger > 0U) {
        gecici[n++] = (char)('0' + (deger % 10U));
        deger /= 10U;
    }
    while (n > 0) {
        hedef[k++] = gecici[--n];
    }
    hedef[k] = '\0';
}

/* Basit metin ekleme (strcat yok -- libc yok). */
static void metin_ekle(char *hedef, const char *ek)
{
    while (*hedef != '\0') {
        hedef++;
    }
    while (*ek != '\0') {
        *hedef++ = *ek++;
    }
    *hedef = '\0';
}


/* ===========================================================================
 *  16) I2C1'i MASTER olarak kur
 * ========================================================================= */
static void i2c_kur(void)
{
    RCC_AHB2ENR |= RCC_GPIOBEN;
    (void)RCC_AHB2ENR;

    /* PB8/PB9 -> AF4 (I2C1), ACIK DRENAJ, dahili pull-up.
     * Acik drenaj I2C'nin temelidir: cihazlar hatti yalnizca asagi ceker.
     * SSD1306 modullerinin cogunda kart uzerinde pull-up vardir; buradaki
     * dahili pull-up modul taksiz iken hattin havada kalmasini onler. */
    GPIO_MODER(GPIOB_BASE) &= ~((3U << (PIN_SCL * 2U)) | (3U << (PIN_SDA * 2U)));
    GPIO_MODER(GPIOB_BASE) |=  ((MODER_AF << (PIN_SCL * 2U)) |
                                (MODER_AF << (PIN_SDA * 2U)));
    GPIO_OTYPER(GPIOB_BASE)  |= (1U << PIN_SCL) | (1U << PIN_SDA);
    GPIO_OSPEEDR(GPIOB_BASE) |= (3U << (PIN_SCL * 2U)) | (3U << (PIN_SDA * 2U));
    GPIO_PUPDR(GPIOB_BASE)   &= ~((3U << (PIN_SCL * 2U)) | (3U << (PIN_SDA * 2U)));
    GPIO_PUPDR(GPIOB_BASE)   |=  ((1U << (PIN_SCL * 2U)) | (1U << (PIN_SDA * 2U)));

    GPIO_AFR(GPIOB_BASE, PIN_SCL) &= ~(0xFU << ((PIN_SCL & 7U) * 4U));
    GPIO_AFR(GPIOB_BASE, PIN_SCL) |=  (AF4_I2C1 << ((PIN_SCL & 7U) * 4U));
    GPIO_AFR(GPIOB_BASE, PIN_SDA) &= ~(0xFU << ((PIN_SDA & 7U) * 4U));
    GPIO_AFR(GPIOB_BASE, PIN_SDA) |=  (AF4_I2C1 << ((PIN_SDA & 7U) * 4U));

    RCC_APB1ENR1 |= RCC_I2C1EN;
    (void)RCC_APB1ENR1;
    RCC_APB1RSTR1 |=  RCC_I2C1RST;
    RCC_APB1RSTR1 &= ~RCC_I2C1RST;

    I2C1_CR1 = 0U;

    /* 100 kHz @ HSI16. Bu deger test_i2c'de RM0351 Bolum 39.4.9 formuluyle
     * hesaplanmis ve sahada dogrulanmisti; ayni saat kaynagini kullandigimiz
     * icin aynen gecerli.
     *
     * Neden 400 kHz degil? Tam ekran tazeleme 100 kHz'de ~95 ms surer ve biz
     * ekrani 2 saniyede bir tazeliyoruz (DHT22'nin izin verdigi en sik hiz).
     * Hizlandirmanin gorunur bir faydasi olmazdi. */
    I2C1_TIMINGR = 0x30420F13U;

    I2C1_CR1 = I2C_CR1_PE;
}

/* n bayt yaz. AUTOEND ile STOP'u donanim uretir.
 * Donus: 0 = tamam, -1 = zaman asimi, -2 = NACK (adreste kimse yok). */
static int i2c_yaz(u8 adres7, const u8 *veri, u32 adet)
{
    u32 t = I2C_ZAMAN_ASIMI;

    while ((I2C1_ISR & I2C_ISR_BUSY) != 0U) {
        if (--t == 0U) { return -1; }
    }

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


/* ===========================================================================
 *  17) SSD1306
 * ========================================================================= */

static int ssd_komut(u8 komut)
{
    u8 tampon[2];

    tampon[0] = SSD_KOMUT;
    tampon[1] = komut;

    return i2c_yaz(ssd_adres, tampon, 2U);
}

static int ssd_komut2(u8 komut, u8 arg)
{
    u8 tampon[3];

    tampon[0] = SSD_KOMUT;
    tampon[1] = komut;
    tampon[2] = arg;

    return i2c_yaz(ssd_adres, tampon, 3U);
}

/* Ekranin hangi adreste oldugunu bulur. Modullerin cogu 0x3C, bazilari
 * 0x3D'dir (SA0 pini). Kullaniciyi "neden calismiyor" diye ugrastirmak
 * yerine ikisini de deniyoruz: adrese bir komut yazip ACK gelip gelmedigine
 * bakmak yeterli. Donus: bulunan adres, yoksa 0. */
static u8 ssd_adres_bul(void)
{
    static const u8 adaylar[2] = { SSD_ADRES_A, SSD_ADRES_B };
    u32 i;

    for (i = 0U; i < 2U; i++) {
        ssd_adres = adaylar[i];
        /* Zararsiz bir komut: ekrani kapat. Cevap veren adres dogrudur. */
        if (ssd_komut(SSD_EKRAN_KAPALI) == 0) {
            return ssd_adres;
        }
    }

    ssd_adres = 0U;
    return 0U;
}

/* Baslatma dizisi. Sira datasheet Bolum 10 ve uygulama notundaki onerilen
 * akisa gore; sarj pompasi acilmadan ekran KARANLIK kalir (Bolum 1,
 * "Charge Pump Command Table" notu: 8Dh -> 14h -> AFh). */
static void ssd_baslat(void)
{
    ssd_komut(SSD_EKRAN_KAPALI);

    ssd_komut2(SSD_SAAT_BOLME, 0x80U);      /* onerilen varsayilan          */
    ssd_komut2(SSD_COKLAMA, SSD_YUKSEKLIK - 1U);  /* 64 satir -> 0x3F       */
    ssd_komut2(SSD_DIKEY_KAYDIR, 0x00U);
    ssd_komut(SSD_BASLANGIC_SATIR | 0x00U);

    /* Sarj pompasi: modulde harici yuksek gerilim kaynagi yok, panel
     * gerilimini cip kendi uretir. Bu komut atlanirsa her sey dogru
     * gorunur ama ekranda HICBIR SEY gozukmez. */
    ssd_komut2(SSD_SARJ_POMPASI, 0x14U);

    ssd_komut2(SSD_BELLEK_KIPI, 0x00U);     /* yatay adresleme              */

    /* Modulun yerlesimine gore goruntu bas asagi olabilir. Bu ikisi birlikte
     * 180 derece dondurur; ekraniniz ters duruyorsa SSD_SEG_TERS -> 0xA0 ve
     * SSD_COM_TERS -> 0xC0 yapin. */
    ssd_komut(SSD_SEG_TERS);
    ssd_komut(SSD_COM_TERS);

    ssd_komut2(SSD_COM_PIN, 0x12U);         /* 128x64 icin: alternatif COM  */
    ssd_komut2(SSD_KONTRAST, 0xCFU);
    ssd_komut2(SSD_ON_SARJ, 0xF1U);
    ssd_komut2(SSD_VCOMH, 0x40U);

    ssd_komut(SSD_RAM_DEVAM);
    ssd_komut(SSD_NORMAL);
    ssd_komut(SSD_EKRAN_ACIK);
}

/* Cerceve tamponunu ekrana gonder.
 *
 * NBYTES 8 bit oldugu icin (en fazla 255) 1024 bayt tek islemde gitmez.
 * Sayfa sayfa gonderiyoruz: her islem 1 kontrol bayti + 128 veri = 129 bayt.
 * SSD1306'nin RAM isaretcisi islemler arasinda korunur, bu yuzden ardisik
 * sayfalar dogru yere yazilir. */
static void ssd_goster(void)
{
    u8  gonder[1U + SSD_GENISLIK];
    u32 sayfa;
    u32 i;

    ssd_komut(SSD_SUTUN_ARALIK);
    ssd_komut(0U);
    ssd_komut(SSD_GENISLIK - 1U);

    ssd_komut(SSD_SAYFA_ARALIK);
    ssd_komut(0U);
    ssd_komut(SSD_SAYFA - 1U);

    for (sayfa = 0U; sayfa < SSD_SAYFA; sayfa++) {
        gonder[0] = SSD_VERI;
        for (i = 0U; i < SSD_GENISLIK; i++) {
            gonder[1U + i] = cerceve[(sayfa * SSD_GENISLIK) + i];
        }
        (void)i2c_yaz(ssd_adres, gonder, 1U + SSD_GENISLIK);
    }
}

static void ssd_temizle(void)
{
    u32 i;

    for (i = 0U; i < (SSD_GENISLIK * SSD_SAYFA); i++) {
        cerceve[i] = 0U;
    }
}


/* ===========================================================================
 *  18) CIZIM  --  cerceve tamponu uzerinde
 * ========================================================================= */

/* Tek piksel. Sinir disi istekleri sessizce yutar: cizim fonksiyonlarinin
 * her birinde ayri ayri sinir kontrolu yapmak yerine tek yerde topluyoruz. */
static void piksel(u32 x, u32 y)
{
    if ((x >= SSD_GENISLIK) || (y >= SSD_YUKSEKLIK)) {
        return;
    }
    cerceve[((y / 8U) * SSD_GENISLIK) + x] |= (u8)(1U << (y % 8U));
}

/* Tek karakter, tam sayi olcekle buyutulmus.
 *
 * Olcek neden tam sayi? Ara deger (1.5x gibi) piksel izgarasina oturmaz ve
 * harfler tirtikli cikar. Tam sayi olcekte her font pikseli olcek x olcek
 * bir kareye donusur; sonuc keskin kalir. Boylece tek fontla hem kucuk
 * etiket hem buyuk sicaklik yazabiliyoruz -- ikinci bir font tablosu
 * tasimaya gerek yok. */
static void karakter(u32 x, u32 y, char c, u32 olcek)
{
    u32 sutun;

    for (sutun = 0U; sutun < FONT_GENISLIK; sutun++) {
        u8  desen = font5x7[((u32)(u8)c * 5U) + sutun];
        u32 satir;

        for (satir = 0U; satir < FONT_YUKSEKLIK; satir++) {
            if ((desen & (1U << satir)) != 0U) {
                u32 dx;
                for (dx = 0U; dx < olcek; dx++) {
                    u32 dy;
                    for (dy = 0U; dy < olcek; dy++) {
                        piksel(x + (sutun * olcek) + dx,
                               y + (satir * olcek) + dy);
                    }
                }
            }
        }
    }
}

static void yazi(u32 x, u32 y, const char *s, u32 olcek)
{
    while (*s != '\0') {
        karakter(x, y, *s, olcek);
        x += FONT_ADIM * olcek;
        s++;
    }
}


/* ===========================================================================
 *  19) DHT22
 * ========================================================================= */

static inline u32 dht_hat(void)
{
    return (GPIO_IDR(GPIOA_BASE) >> PIN_DHT) & 1U;
}

static void dht_kur(void)
{
    RCC_AHB2ENR |= RCC_GPIOAEN;
    (void)RCC_AHB2ENR;

    /* ACIK DRENAJ cikis + dahili pull-up (dosya basindaki aciklama).
     * ODR=1 birakma, ODR=0 asagi cekme anlamina gelir; IDR her durumda
     * hattin gercek halini okur. */
    GPIO_MODER(GPIOA_BASE)   &= ~(3U << (PIN_DHT * 2U));
    GPIO_MODER(GPIOA_BASE)   |=  (MODER_CIKIS << (PIN_DHT * 2U));
    GPIO_OTYPER(GPIOA_BASE)  |=  (1U << PIN_DHT);          /* acik drenaj   */
    GPIO_OSPEEDR(GPIOA_BASE) |=  (3U << (PIN_DHT * 2U));
    GPIO_PUPDR(GPIOA_BASE)   &= ~(3U << (PIN_DHT * 2U));
    GPIO_PUPDR(GPIOA_BASE)   |=  (1U << (PIN_DHT * 2U));   /* pull-up       */

    GPIO_BSRR(GPIOA_BASE) = (1U << PIN_DHT);               /* hatti birak   */
}

#define SURE_ASILDI  0xFFFFFFFFU

/* Hat 'seviye'de kaldigi sureyi mikrosaniye olarak dondurur.
 * Zaman asiminda SURE_ASILDI doner -- boylece kablo takili degilse program
 * sonsuza kadar beklemez, hata dondurup rapor eder. */
static u32 seviye_suresi(u32 seviye, u32 azami_us)
{
    u32 t0 = us_simdi();

    while (dht_hat() == seviye) {
        if ((us_simdi() - t0) > azami_us) {
            return SURE_ASILDI;
        }
    }
    return us_simdi() - t0;
}

static int dht22_oku(dht_olcum_t *sonuc)
{
    u8  bayt[5] = { 0U, 0U, 0U, 0U, 0U };
    u32 i;
    u32 sure;
    u32 ham_nem;
    u32 ham_sic;
    u32 toplam;

    /* --- 1) Baslat isareti: hatti en az 1 ms asagi cek --------------------
     * Datasheet "at least 1ms" der; 1.2 ms guvenli tarafta kaliyor. */
    GPIO_BSRR(GPIOA_BASE) = (1U << (PIN_DHT + 16U));   /* asagi cek */
    bekle_us(1200U);
    GPIO_BSRR(GPIOA_BASE) = (1U << PIN_DHT);           /* birak     */

    /* --- 2) Sensorun cevabini bekle ---------------------------------------
     * Biz biraktiktan sonra pull-up hatti yukari ceker (20-40 us), ardindan
     * sensor 80 us LOW + 80 us HIGH ile cevap verir. Uc gecisin ucu de
     * gelmezse sensor yok/kablo kopuk demektir. */
    if (seviye_suresi(1U, 200U) == SURE_ASILDI) { return DHT_CEVAP_YOK; }
    if (seviye_suresi(0U, 200U) == SURE_ASILDI) { return DHT_CEVAP_YOK; }
    if (seviye_suresi(1U, 200U) == SURE_ASILDI) { return DHT_CEVAP_YOK; }

    /* --- 3) 40 bit oku ----------------------------------------------------
     * Her bit: ~50 us LOW (ayirac) + degeri belirleyen HIGH.
     * HIGH 26-28 us ise 0, 70 us ise 1. Esigi tam ortaya (50 us) koyuyoruz;
     * boylece toleranstan kaynakli sapmalar iki tarafa da genis pay birakir.
     *
     * Veri MSB-first gelir (datasheet: "DHT22 send out higher data bit
     * firstly"), bu yuzden her yeni biti sola kaydirip ekliyoruz. */
    for (i = 0U; i < 40U; i++) {
        if (seviye_suresi(0U, 200U) == SURE_ASILDI) { return DHT_BIT_ZAMAN; }

        sure = seviye_suresi(1U, 200U);
        if (sure == SURE_ASILDI) { return DHT_BIT_ZAMAN; }

        bayt[i / 8U] = (u8)((bayt[i / 8U] << 1) | ((sure > 50U) ? 1U : 0U));
    }

    /* --- 4) Saglama -------------------------------------------------------
     * Ilk dort baytin toplaminin alt 8 biti besinci bayta esit olmali. */
    toplam = (u32)bayt[0] + bayt[1] + bayt[2] + bayt[3];
    if ((u8)(toplam & 0xFFU) != bayt[4]) {
        return DHT_SAGLAMA;
    }

    /* --- 5) Coz -----------------------------------------------------------
     * Nem  : 16 bit isaretsiz, onda bir yuzde.
     * Sicaklik: 16 bitin en ust biti ISARET bitidir ve deger IKIYE TUMLEYEN
     *           DEGILDIR ("isaret + buyukluk").
     *
     *   >>> DIKKAT: Bu kodlama elimizdeki DHT22 datasheet surumunde YAZMIYOR.
     *   >>> Butun yaygin surucu kodlarinin kullandigi ve sahada gecerli
     *   >>> oldugu bilinen kural bu; ama deponun "her seyi belgeden dogrula"
     *   >>> kuralina uyan bir kaynak bulunamadi. Sifirin altinda olcum
     *   >>> yapabiliyorsaniz once bunu dogrulayin.
     */
    ham_nem = ((u32)bayt[0] << 8) | bayt[1];
    ham_sic = ((u32)bayt[2] << 8) | bayt[3];

    sonuc->nem = (i32)ham_nem;

    if ((ham_sic & 0x8000U) != 0U) {
        sonuc->sicaklik = -(i32)(ham_sic & 0x7FFFU);
    } else {
        sonuc->sicaklik = (i32)ham_sic;
    }

    return DHT_TAMAM;
}


/* ===========================================================================
 *  20) EKRAN DUZENI
 * ===========================================================================
 *  128x64 piksel, tek fontun farkli olcekleriyle:
 *
 *      y= 0  olcek 1 ( 8 px)  baslik
 *      y=12  olcek 3 (24 px)  sicaklik   -- ekranin yildizi, uzaktan okunur
 *      y=40  olcek 2 (16 px)  nem
 *      y=56  olcek 1 ( 8 px)  sayaclar
 *
 *  Olcek 3'te bir karakter 18 px genisligindedir; "-12.3\xF8C" gibi en uzun
 *  hal 7 karakter x 18 = 126 px eder ve 128'e tam sigar.
 * ========================================================================= */
static void ekran_ciz(const dht_olcum_t *olcum, int durum,
                      u32 basarili, u32 hatali)
{
    /* Tampon boyutlari en KOTU duruma gore:
     *   satir : "ok:" + 10 hane + " hata:" + 10 hane + '\0' = 30  -> 40 alindi
     *   sayi  : "-3276.7" + '\0' = 8 (i32'nin 15 bitlik ham degeri)  -> 12
     * Dar tutup "nasil olsa sayaclar buyumez" demek, gomulu sistemde en
     * sinsi hata turudur: yigin bozulur, belirti alakasiz bir yerde cikar. */
    char satir[40];
    char sayi[12];
    char sayi2[12];

    ssd_temizle();

    yazi(0U, 0U, "DHT22 + SSD1306", 1U);

    if (durum == DHT_TAMAM) {
        /* --- Sicaklik: buyuk --- */
        ondalik_metin(sayi, olcum->sicaklik);
        satir[0] = '\0';
        metin_ekle(satir, sayi);
        {
            /* Derece isareti font tablosunda 0xF8; string icine elle koyuyoruz */
            char derece[3];
            derece[0] = (char)FONT_DERECE;
            derece[1] = 'C';
            derece[2] = '\0';
            metin_ekle(satir, derece);
        }
        yazi(0U, 12U, satir, 3U);

        /* --- Nem: orta --- */
        ondalik_metin(sayi, olcum->nem);
        satir[0] = '\0';
        metin_ekle(satir, "NEM ");
        metin_ekle(satir, sayi);
        metin_ekle(satir, "%");
        yazi(0U, 40U, satir, 2U);
    } else {
        const char *mesaj;

        switch (durum) {
        case DHT_CEVAP_YOK: mesaj = "CEVAP YOK"; break;
        case DHT_BIT_ZAMAN: mesaj = "ZAMAN ASIMI"; break;
        case DHT_SAGLAMA:   mesaj = "SAGLAMA HATASI"; break;
        default:            mesaj = "HATA"; break;
        }
        yazi(0U, 16U, "SENSOR", 2U);
        yazi(0U, 36U, mesaj, 1U);
        yazi(0U, 46U, "kablo/pull-up?", 1U);
    }

    /* --- Sayaclar: kucuk --- */
    satir[0] = '\0';
    metin_ekle(satir, "ok:");
    sayi_metin(sayi2, basarili);
    metin_ekle(satir, sayi2);
    metin_ekle(satir, " hata:");
    sayi_metin(sayi2, hatali);
    metin_ekle(satir, sayi2);
    yazi(0U, 56U, satir, 1U);

    ssd_goster();
}


/* ===========================================================================
 *  21) main
 * ========================================================================= */
int main(void)
{
    dht_olcum_t olcum = { 0, 0 };
    u32 basarili = 0U;
    u32 hatali   = 0U;

    saat_kur();
    tim2_kur();
    led_kur();
    uart_kur();
    i2c_kur();
    dht_kur();

    uart_metin("\r\n\r\n");
    uart_metin("=====================================================\r\n");
    uart_metin(" NUCLEO-L476RG  |  DHT22 -> SSD1306 OLED\r\n");
    uart_metin("=====================================================\r\n");
    uart_metin(" I2C1  : PB8=SCL  PB9=SDA           @ 100 kHz\r\n");
    uart_metin(" DHT22 : PA10 (CN9-3 / D2), acik drenaj + pull-up\r\n");
    uart_metin(" TIM2  : 1 MHz serbest sayac (us olcumu)\r\n\r\n");

    /* --- Ekrani bul ve baslat ----------------------------------------- */
    uart_metin("[1] SSD1306 araniyor (0x3C / 0x3D)... ");
    if (ssd_adres_bul() != 0U) {
        uart_metin("bulundu: ");
        uart_hex8(ssd_adres);
        uart_metin("\r\n");
        ssd_baslat();
        ssd_temizle();
        yazi(0U, 24U, "BASLATILIYOR", 1U);
        ssd_goster();
    } else {
        /* Ekran yoksa da devam ediyoruz: sicaklik UART'tan okunabilir ve
         * kullanici en azindan sensorun calisip calismadigini gorur. */
        uart_metin("BULUNAMADI\r\n");
        uart_metin("    SDA/SCL/GND baglantisini ve modul beslemesini kontrol edin.\r\n");
        uart_metin("    Olcumler yine de UART'a basilacak.\r\n");
    }

    /* --- DHT22 acilis beklemesi --------------------------------------- *
     * Datasheet: "When power is supplied to sensor, don't send any
     * instruction to the sensor within one second to pass unstable status."
     * 2 saniye bekleyerek hem bu kurala hem de olcum araligina uyuyoruz. */
    uart_metin("[2] DHT22 kararli hale gelsin diye 2 s bekleniyor...\r\n\r\n");
    bekle_ms(2000U);

    for (;;) {
        int durum = dht22_oku(&olcum);

        if (durum == DHT_TAMAM) {
            char metin[12];

            basarili++;
            led_degistir();

            uart_metin("sicaklik ");
            ondalik_metin(metin, olcum.sicaklik);
            uart_metin(metin);
            uart_metin(" C   nem ");
            ondalik_metin(metin, olcum.nem);
            uart_metin(metin);
            uart_metin(" %   (ok ");
            uart_sayi(basarili);
            uart_metin(" / hata ");
            uart_sayi(hatali);
            uart_metin(")\r\n");
        } else {
            hatali++;

            uart_metin("HATA: ");
            switch (durum) {
            case DHT_CEVAP_YOK:
                uart_metin("sensor cevap vermiyor (kablo? besleme? pull-up?)");
                break;
            case DHT_BIT_ZAMAN:
                uart_metin("bit zaman asimi (sinyal bozuk / kablo uzun?)");
                break;
            case DHT_SAGLAMA:
                uart_metin("saglama tutmadi (gurultu?)");
                break;
            default:
                uart_metin("bilinmeyen");
                break;
            }
            uart_metin("   (ok ");
            uart_sayi(basarili);
            uart_metin(" / hata ");
            uart_sayi(hatali);
            uart_metin(")\r\n");
        }

        if (ssd_adres != 0U) {
            ekran_ciz(&olcum, durum, basarili, hatali);
        }

        bekle_ms(OLCUM_ARALIGI_MS);
    }

    return 0;   /* buraya asla ulasilmaz */
}
