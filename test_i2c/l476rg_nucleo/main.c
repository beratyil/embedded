/*
 * ============================================================================
 *  main.c  --  NUCLEO-L476RG  |  I2C1 SLAVE (kesme tabanli)
 * ============================================================================
 *
 *  HEDEF DONANIM : STM32L476RG, Cortex-M4F, 1 MB Flash, 96 KB SRAM1 + 32 KB SRAM2
 *  KART          : NUCLEO-L476RG (MB1136)
 *  ROL           : I2C KOLESI (slave), 7-bit adres 0x42
 *  KARSI TARAF   : STM32F4DISCOVERY (master)
 *
 *  KURAL: Hicbir kutuphane yok (HAL / LL / CMSIS / libc yok). Sadece register.
 *
 * ----------------------------------------------------------------------------
 *  KABLOLAMA
 * ----------------------------------------------------------------------------
 *      NUCLEO-L476RG                    F407 DISCOVERY
 *      -------------                    --------------
 *      PB8  (CN5-10, "SCL/D15")  <--->  PB6  (I2C1_SCL)
 *      PB9  (CN5-9,  "SDA/D14")  <--->  PB7  (I2C1_SDA)
 *      GND  (CN6-6 veya CN7-8,20)<--->  GND       <-- SART
 *
 *      + her iki hatta 3.3V'a 4.7k pull-up direnc (bkz. master dosyasi).
 *
 * ----------------------------------------------------------------------------
 *  GORSEL / METINSEL GERI BILDIRIM
 * ----------------------------------------------------------------------------
 *      LD2 (PA5 yesil) : Master'in LED_CTRL register'ina yazdigi degeri
 *                        gosterir. Master her 500 ms'de bunu degistirdigi
 *                        icin baglanti kuruldugunda LED yanip sonmeye baslar.
 *                        LED HIC yanip sonmuyorsa hat kurulmamis demektir.
 *
 *      USART2 -> ST-LINK sanal COM portu (PA2 = TX). Bilgisayardan:
 *              screen /dev/ttyACM0 115200        (cikmak icin Ctrl-A, K)
 *          veya
 *              minicom -D /dev/ttyACM0 -b 115200
 *      Her tamamlanan I2C isleminde bir satir basar. Iki ttyACM varsa
 *      Nucleo genelde numarasi kucuk olandir; ikisini de deneyin.
 *
 * ----------------------------------------------------------------------------
 *  REGISTER HARITASI  --  master bizi bir sensor gibi gorur
 * ----------------------------------------------------------------------------
 *   Adres  Isim       Erisim  Aciklama
 *   -----  ---------  ------  ---------------------------------------------
 *   0x00   WHO_AM_I   R       Sabit 0x5A. Master bununla kimligimizi dogrular
 *   0x01   LED_CTRL   R/W     bit0 -> LD2 (PA5)
 *   0x02   COUNTER    R       Tamamlanan I2C islem sayisi (canlilik isareti)
 *   0x03   ECHO       R/W     Yazilan deger aynen geri okunur (hat testi)
 *
 *  ERISIM SEKLI (gercek I2C sensorleriyle ayni):
 *      Yazma : [START][0x42+W][register no][veri][STOP]
 *      Okuma : [START][0x42+W][register no][TEKRAR-START][0x42+R][veri...][STOP]
 *  Okuma sirasinda her bayttan sonra register isaretcisi kendiliginden
 *  ilerler, boylece master tek istekte 4 register'i pesi sira alabilir.
 *
 * ----------------------------------------------------------------------------
 *  NEDEN KESME? (master yoklama kullaniyordu)
 * ----------------------------------------------------------------------------
 *  Master konusmayi ne zaman baslatacagina kendi karar verir; slave ise
 *  cagrildigi ANDA cevap vermek zorundadir. Ana dongude yoklama yapsaydik,
 *  baska bir isle mesgulken gelen bir istegi kacirabilirdik. Kesme ile
 *  ana dongu bosta beklerken bile haberlesme kusursuz yurur.
 *
 * ----------------------------------------------------------------------------
 *  DIKKAT -- STM32F4 ile I2C FARKI (iki dosyayi karsilastirin)
 * ----------------------------------------------------------------------------
 *  Bu iki cip ayni cekirdegi kullanir ama I2C birimleri BASKA NESILDIR:
 *
 *    Konu               | STM32F407 (I2C v1)      | STM32L476 (I2C v2)
 *    -------------------+-------------------------+------------------------
 *    Hiz ayari          | FREQ + CCR + TRISE      | tek register: TIMINGR
 *    Durum bayraklari   | SR1 / SR2               | ISR (tek register)
 *    Bayrak temizleme   | okuma sirasi (SR1,SR2)  | ICR'ye 1 yazmak
 *    Saat kaynagi       | daima PCLK1             | secilebilir (biz HSI16)
 *    Adres eslesmesi    | ADDR + SR1/SR2 okuma    | ADDR + ICR'ye ADDRCF
 *
 *  Yani F4 kodunu adres degistirip kopyalamak ise YARAMAZ; mantik farklidir.
 * ============================================================================
 */

/* ---------------------------------------------------------------------------
 * 0) TEMEL TIPLER
 * ------------------------------------------------------------------------- */
typedef unsigned char         u8;
typedef unsigned int          u32;
typedef volatile unsigned int vu32;

#define REG32(adres)  (*(vu32 *)(adres))


/* ---------------------------------------------------------------------------
 * 1) HAFIZA HARITASI  (RM0351, Tablo 1)
 * ---------------------------------------------------------------------------
 *   0x4000 0000  APB1  <- I2C1 (0x4000 5400), USART2 (0x4000 4400)
 *   0x4002 0000  AHB1  <- RCC  (0x4002 1000)
 *   0x4800 0000  AHB2  <- GPIO portlari      (F4'te AHB1'deydi!)
 * ------------------------------------------------------------------------- */
#define RCC_BASE      0x40021000U
#define GPIOA_BASE    0x48000000U            /* LD2 (PA5), USART2_TX (PA2)   */
#define GPIOB_BASE    0x48000400U            /* I2C1 pinleri (PB8, PB9)      */
#define I2C1_BASE     0x40005400U
#define USART2_BASE   0x40004400U


/* ---------------------------------------------------------------------------
 * 2) RCC  --  Saat kapilari ve saat kaynagi secimi
 * ------------------------------------------------------------------------- */
#define RCC_CR        REG32(RCC_BASE + 0x00U)  /* Osilator kontrol           */
#define RCC_AHB2ENR   REG32(RCC_BASE + 0x4CU)  /* GPIO saatleri              */
#define RCC_APB1ENR1  REG32(RCC_BASE + 0x58U)  /* I2C1, USART2 saatleri      */
#define RCC_APB1RSTR1 REG32(RCC_BASE + 0x38U)  /* I2C1 donanim reset'i       */
#define RCC_CCIPR     REG32(RCC_BASE + 0x88U)  /* Cevre birimi saat kaynaklari*/

#define RCC_HSION     (1U << 8)              /* CR: 16 MHz dahili RC'yi ac   */
#define RCC_HSIRDY    (1U << 10)             /* CR: HSI16 kararli mi?        */

#define RCC_GPIOAEN   (1U << 0)              /* AHB2ENR bit 0                */
#define RCC_GPIOBEN   (1U << 1)              /* AHB2ENR bit 1                */

#define RCC_USART2EN  (1U << 17)             /* APB1ENR1 bit 17              */
#define RCC_I2C1EN    (1U << 21)             /* APB1ENR1 bit 21              */
#define RCC_I2C1RST   (1U << 21)             /* APB1RSTR1 bit 21             */

/* CCIPR: hangi cevre biriminin saatini nereden alacagini secer.
 *    bit  3:2  USART2SEL : 00=PCLK1  01=SYSCLK  10=HSI16  11=LSE
 *    bit 13:12 I2C1SEL   : 00=PCLK1  01=SYSCLK  10=HSI16
 * Ikisini de HSI16'ya baglayacagiz -- nedeni asagida saat bolumunde. */
#define CCIPR_USART2SEL_HSI16  (2U << 2)
#define CCIPR_I2C1SEL_HSI16    (2U << 12)


/* ---------------------------------------------------------------------------
 * 3) GPIO REGISTER'LARI  (RM0351, Bolum 8.4)
 * ------------------------------------------------------------------------- */
#define GPIOA_MODER   REG32(GPIOA_BASE + 0x00U)
#define GPIOA_OTYPER  REG32(GPIOA_BASE + 0x04U)
#define GPIOA_OSPEEDR REG32(GPIOA_BASE + 0x08U)
#define GPIOA_PUPDR   REG32(GPIOA_BASE + 0x0CU)
#define GPIOA_BSRR    REG32(GPIOA_BASE + 0x18U)
#define GPIOA_AFRL    REG32(GPIOA_BASE + 0x20U)

#define GPIOB_MODER   REG32(GPIOB_BASE + 0x00U)
#define GPIOB_OTYPER  REG32(GPIOB_BASE + 0x04U)
#define GPIOB_OSPEEDR REG32(GPIOB_BASE + 0x08U)
#define GPIOB_PUPDR   REG32(GPIOB_BASE + 0x0CU)
#define GPIOB_AFRH    REG32(GPIOB_BASE + 0x24U)  /* pin 8..15 icin           */

#define LED_PIN       5U                     /* PA5 = LD2 (yesil)            */
#define M_LED         (1U << LED_PIN)

#define PIN_TX        2U                     /* PA2 = USART2_TX (AF7)        */
#define AF7_USART2    7U

#define PIN_SCL       8U                     /* PB8 = I2C1_SCL (AF4)         */
#define PIN_SDA       9U                     /* PB9 = I2C1_SDA (AF4)         */
#define AF4_I2C1      4U


/* ---------------------------------------------------------------------------
 * 4) I2C1 REGISTER'LARI  (RM0351, Bolum 39.7)  --  "yeni nesil" I2C
 * ------------------------------------------------------------------------- */
#define I2C1_CR1      REG32(I2C1_BASE + 0x00U)  /* Kontrol 1 (+ kesme izinleri)*/
#define I2C1_CR2      REG32(I2C1_BASE + 0x04U)  /* Kontrol 2 (master icin)   */
#define I2C1_OAR1     REG32(I2C1_BASE + 0x08U)  /* Kendi adresimiz           */
#define I2C1_OAR2     REG32(I2C1_BASE + 0x0CU)  /* Ikinci adres (kullanmiyoruz)*/
#define I2C1_TIMINGR  REG32(I2C1_BASE + 0x10U)  /* Tum zamanlama tek yerde   */
#define I2C1_ISR      REG32(I2C1_BASE + 0x18U)  /* Durum bayraklari          */
#define I2C1_ICR      REG32(I2C1_BASE + 0x1CU)  /* Bayrak temizleme          */
#define I2C1_RXDR     REG32(I2C1_BASE + 0x24U)  /* Gelen bayt                */
#define I2C1_TXDR     REG32(I2C1_BASE + 0x28U)  /* Giden bayt                */

/* --- CR1 bitleri --- */
#define I2C_CR1_PE        (1U << 0)   /* Peripheral Enable                   */
#define I2C_CR1_TXIE      (1U << 1)   /* "Bayt ver" kesmesi                  */
#define I2C_CR1_RXIE      (1U << 2)   /* "Bayt geldi" kesmesi                */
#define I2C_CR1_ADDRIE    (1U << 3)   /* "Adresim cagrildi" kesmesi          */
#define I2C_CR1_NACKIE    (1U << 4)   /* "NACK aldim" kesmesi                */
#define I2C_CR1_STOPIE    (1U << 5)   /* "STOP gordum" kesmesi               */
#define I2C_CR1_ERRIE     (1U << 7)   /* Hata kesmeleri (BERR/ARLO/OVR)      */
#define I2C_CR1_NOSTRETCH (1U << 17)  /* 1 = saati germe (slave icin TEHLIKE)*/

/* --- ISR bitleri (durum) --- */
#define I2C_ISR_TXE       (1U << 0)   /* TXDR bos (1 YAZILARAK bosaltilabilir)*/
#define I2C_ISR_TXIS      (1U << 1)   /* Gonderilecek bayt bekleniyor        */
#define I2C_ISR_RXNE      (1U << 2)   /* RXDR'de bayt var                    */
#define I2C_ISR_ADDR      (1U << 3)   /* Adresimiz eslesti (slave)           */
#define I2C_ISR_NACKF     (1U << 4)   /* Karsi taraf NACK gonderdi           */
#define I2C_ISR_STOPF     (1U << 5)   /* STOP kosulu algilandi               */
#define I2C_ISR_BERR      (1U << 8)   /* Bus hatasi                          */
#define I2C_ISR_ARLO      (1U << 9)   /* Hakem kaybi                         */
#define I2C_ISR_OVR       (1U << 10)  /* Tasma / yetismedik                  */
#define I2C_ISR_DIR       (1U << 16)  /* 0 = master YAZIYOR, 1 = master OKUYOR*/

/* --- ICR bitleri (temizleme: ilgili bite 1 yaz) --- */
#define I2C_ICR_ADDRCF    (1U << 3)
#define I2C_ICR_NACKCF    (1U << 4)
#define I2C_ICR_STOPCF    (1U << 5)
#define I2C_ICR_BERRCF    (1U << 8)
#define I2C_ICR_ARLOCF    (1U << 9)
#define I2C_ICR_OVRCF     (1U << 10)

/* --- OAR1 --- */
#define I2C_OAR1_OA1EN    (1U << 15)  /* Adres 1'i etkinlestir                */


/* ---------------------------------------------------------------------------
 * 5) USART2 REGISTER'LARI  (RM0351, Bolum 40.8)  --  hata ayiklama ciktisi
 * ------------------------------------------------------------------------- */
#define USART2_CR1    REG32(USART2_BASE + 0x00U)
#define USART2_BRR    REG32(USART2_BASE + 0x0CU)
#define USART2_ISR    REG32(USART2_BASE + 0x1CU)
#define USART2_TDR    REG32(USART2_BASE + 0x28U)

#define USART_CR1_UE  (1U << 0)              /* USART Enable                 */
#define USART_CR1_TE  (1U << 3)              /* Transmitter Enable           */
#define USART_ISR_TXE (1U << 7)              /* Gonderme register'i bos      */


/* ---------------------------------------------------------------------------
 * 6) NVIC  --  Kesme denetleyicisi (Cortex-M cekirdeginin parcasi)
 * ---------------------------------------------------------------------------
 * ISER = Interrupt Set-Enable Register. Her bit bir IRQ numarasidir.
 * 32 IRQ bir register'a sigar:
 *      ISER[0] -> IRQ  0..31
 *      ISER[1] -> IRQ 32..63
 *
 * STM32L476'da:
 *      IRQ 31 = I2C1_EV  (olaylar: adres, veri, STOP)
 *      IRQ 32 = I2C1_ER  (hatalar)
 * IRQ 31 tam sinirda oldugu icin biri ISER[0]'in son bitine, digeri
 * ISER[1]'in ilk bitine dusuyor -- karistirmasi kolay bir ayrinti.
 * ------------------------------------------------------------------------- */
#define NVIC_ISER0    REG32(0xE000E100U)
#define NVIC_ISER1    REG32(0xE000E104U)

#define IRQ_I2C1_EV   31U
#define IRQ_I2C1_ER   32U


/* ---------------------------------------------------------------------------
 * 7) SAAT DURUMU
 * ---------------------------------------------------------------------------
 * STM32L4 reset sonrasi MSI osilatoru ile 4 MHz'de baslar (F4'teki 16 MHz
 * HSI degil). Cekirdegi 4 MHz'de birakiyoruz -- yapacak isimiz yok, kesme
 * bekliyoruz.
 *
 * ANCAK I2C ve USART'i MSI'ya baglamiyoruz. Iki nedenle:
 *   1) MSI'nin frekansi ayarlanabilir; ileride degistirirseniz haberlesme
 *      sessizce bozulurdu. HSI16 her zaman 16 MHz'dir.
 *   2) MSI'nin dogrulugu (kalibrasyonsuz) UART icin sinirda kalir; HSI16
 *      yaklasik %1 ile guvenli taraftadir.
 * CCIPR ile ikisini de HSI16'ya baglayip HSI16'yi aciyoruz.
 * ------------------------------------------------------------------------- */
#define HSI16_HZ      16000000U
#define UART_BAUD     115200U


/* ---------------------------------------------------------------------------
 * 8) SLAVE DURUMU  --  kesme ile ana dongunun paylastigi degiskenler
 * ---------------------------------------------------------------------------
 * Hepsi 'volatile': degerleri kesme icinde degisiyor. Bu anahtar kelime
 * olmadan derleyici ana dongude "bu degisken degismiyor" varsayimi yapip
 * kontrolu tamamen silebilir.
 * ------------------------------------------------------------------------- */
#define SLAVE_ADRES   0x42U                  /* 7-bit adresimiz              */

#define REG_WHO_AM_I  0U
#define REG_LED_CTRL  1U
#define REG_COUNTER   2U
#define REG_ECHO      3U
#define REG_ADET      4U

/* Register dosyamiz. WHO_AM_I sabit 0x5A ile baslar; bu deger .data
 * bolumunde durur ve startup kodu tarafindan Flash'tan SRAM'e kopyalanir. */
static volatile u8 registerlar[REG_ADET] = { 0x5AU, 0x00U, 0x00U, 0x00U };

/* Su an hangi register'la ilgileniyoruz (master'in verdigi indeks). */
static volatile u8  reg_isaretcisi;

/* Adres eslesmesinden sonra gelen ILK bayt veri degil, register numarasidir.
 * Bu bayrak o ilk bayti ayirt eder. */
static volatile u8  ilk_bayt_bekleniyor;

/* Ana dongudeki UART raporu icin toplanan bilgiler. */
static volatile u32 toplam_islem;            /* tamamlanan I2C islemi sayisi */
static volatile u8  son_yazilan_reg;
static volatile u8  son_yazilan_veri;
static volatile u8  rapor_hazir;             /* 1 -> ana dongu satir bassin  */
static volatile u32 hata_sayisi;             /* BERR/ARLO/OVR toplami        */


/* ===========================================================================
 *  9) HSI16 osilatorunu ac
 * ===========================================================================
 *  Hem I2C hem USART bu 16 MHz kaynaga baglanacak. SIRA ONEMLIDIR: bir cevre
 *  biriminin saat kaynagini HSI16 yapmadan once HSI16'nin gercekten calisiyor
 *  olmasi gerekir. Bu yuzden main'de ilk cagrilan fonksiyon budur.
 *
 *  (Cekirdek yine MSI ile 4 MHz'de kalir; onu degistirmiyoruz.)
 * ========================================================================= */
static void hsi16_ac(void)
{
    RCC_CR |= RCC_HSION;
    /* HSIRDY, osilator kararli hale gelene kadar 0 kalir. Beklemezsek
     * baud/zamanlama hesaplari birkac mikrosaniye boyunca yanlis olur. */
    while ((RCC_CR & RCC_HSIRDY) == 0U) { }
}


/* ===========================================================================
 *  10) USART2  --  115200 8N1, sadece gonderme
 * ========================================================================= */
static void uart_kur(void)
{
    RCC_AHB2ENR |= RCC_GPIOAEN;
    (void)RCC_AHB2ENR;

    /* PA2 -> alternatif fonksiyon 7 (USART2_TX), push-pull, yuksek hiz. */
    GPIOA_MODER   &= ~(3U << (PIN_TX * 2));
    GPIOA_MODER   |=  (2U << (PIN_TX * 2));      /* 10 = alternatif fonksiyon*/
    GPIOA_OTYPER  &= ~(1U << PIN_TX);            /* push-pull                */
    GPIOA_OSPEEDR |=  (3U << (PIN_TX * 2));      /* very high speed          */
    GPIOA_PUPDR   &= ~(3U << (PIN_TX * 2));
    GPIOA_AFRL    &= ~(0xFU << (PIN_TX * 4));
    GPIOA_AFRL    |=  (AF7_USART2 << (PIN_TX * 4));

    /* Saat kaynagi: HSI16 (yukarida acikladigimiz nedenle). */
    RCC_CCIPR    &= ~(3U << 2);                  /* USART2SEL alanini sifirla*/
    RCC_CCIPR    |=  CCIPR_USART2SEL_HSI16;
    RCC_APB1ENR1 |=  RCC_USART2EN;
    (void)RCC_APB1ENR1;

    USART2_CR1 = 0U;                             /* once kapat               */

    /* BRR = saat / baud  (16 kat asiri ornekleme, OVER8=0 iken bolme yok)
     *     = 16 000 000 / 115 200 = 138.9 -> 139
     * Yuvarlamadan dogan hata %0.08; UART'in toleransi %2-3 civari. */
    USART2_BRR = (HSI16_HZ + (UART_BAUD / 2U)) / UART_BAUD;

    /* TE = vericiyi ac, UE = birimi ac. Veri bicimi varsayilan: 8 veri
     * biti, parite yok, 1 stop biti (8N1). */
    USART2_CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void uart_bayt(u8 c)
{
    while ((USART2_ISR & USART_ISR_TXE) == 0U) { }  /* yer acilsin diye bekle*/
    USART2_TDR = c;
}

static void uart_metin(const char *s)
{
    while (*s != '\0') {
        uart_bayt((u8)*s++);
    }
}

/* Bir bayti "0x5A" bicminde basar. printf yok, dolayisiyla elle. */
static void uart_hex8(u8 deger)
{
    static const char rakam[] = "0123456789ABCDEF";
    uart_bayt('0');
    uart_bayt('x');
    uart_bayt((u8)rakam[(deger >> 4) & 0xFU]);
    uart_bayt((u8)rakam[deger & 0xFU]);
}

/* Ondalik sayi basar (sifir icin ozel durum, sonra ters cevirme). */
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
 *  11) LED (LD2 / PA5)
 * ========================================================================= */
static void led_kur(void)
{
    RCC_AHB2ENR |= RCC_GPIOAEN;
    (void)RCC_AHB2ENR;

    /* DIKKAT: GPIOA'da PA13/PA14 = SWDIO/SWCLK, yani ST-LINK'in hattidir.
     * MODER'e dogrudan "=" ile yazarsaniz debugger baglantisini keser ve
     * karti tekrar programlamak zorlasir. Daima oku-maskele-yaz. */
    GPIOA_MODER   &= ~(3U << (LED_PIN * 2));
    GPIOA_MODER   |=  (1U << (LED_PIN * 2));     /* 01 = cikis               */
    GPIOA_OTYPER  &= ~M_LED;                     /* push-pull                */
    GPIOA_OSPEEDR &= ~(3U << (LED_PIN * 2));     /* dusuk hiz yeter          */
    GPIOA_PUPDR   &= ~(3U << (LED_PIN * 2));

    GPIOA_BSRR = (M_LED << 16);                  /* baslangicta sonuk        */
}

/* BSRR ile tek yazmada, atomik olarak. */
static inline void led_ayarla(u32 acik)
{
    GPIOA_BSRR = acik ? M_LED : (M_LED << 16);
}


/* ===========================================================================
 *  12) I2C1'i SLAVE OLARAK KUR
 * ========================================================================= */
static void i2c_slave_kur(void)
{
    /* --- 12.1  I2C1'in saat kaynagini HSI16 yap ------------------------- *
     * Bu secim TIMINGR'yi de anlamlandirir: asagidaki degerler 16 MHz'e
     * gore hesaplanmistir.
     *
     * Alani once temizleyip sonra yaziyoruz. Reset degeri zaten 00 (PCLK1)
     * oldugu icin sade bir |= de calisirdi, ama bu kalip kodu "her durumda
     * dogru" yapar: baska bir yerde bu alan degistirilmis olsa bile. */
    RCC_CCIPR &= ~(3U << 12);                    /* I2C1SEL alanini sifirla  */
    RCC_CCIPR |=  CCIPR_I2C1SEL_HSI16;

    /* --- 12.2  Pinlerin saati ve yapilandirmasi -------------------------- */
    RCC_AHB2ENR |= RCC_GPIOBEN;
    (void)RCC_AHB2ENR;

    /* PB8/PB9 -> AF4 (I2C1), ACIK DRENAJ, pull-up.
     * Acik drenaj sarttir: I2C'de cihazlar hatti yalnizca GND'ye ceker,
     * yukari cekmeyi pull-up direncleri yapar. Push-pull birakilirsa iki
     * cip ayni anda zit seviye surunce kisa devre olur. */
    GPIOB_MODER   &= ~((3U << (PIN_SCL * 2)) | (3U << (PIN_SDA * 2)));
    GPIOB_MODER   |=  ((2U << (PIN_SCL * 2)) | (2U << (PIN_SDA * 2)));

    GPIOB_OTYPER  |=  ((1U << PIN_SCL) | (1U << PIN_SDA));

    GPIOB_OSPEEDR |=  ((3U << (PIN_SCL * 2)) | (3U << (PIN_SDA * 2)));

    GPIOB_PUPDR   &= ~((3U << (PIN_SCL * 2)) | (3U << (PIN_SDA * 2)));
    GPIOB_PUPDR   |=  ((1U << (PIN_SCL * 2)) | (1U << (PIN_SDA * 2)));

    /* AFRH: pin 8..15 icin. Pin N'in alani (N-8)*4 bitinden baslar. */
    GPIOB_AFRH &= ~((0xFU << ((PIN_SCL - 8U) * 4U)) | (0xFU << ((PIN_SDA - 8U) * 4U)));
    GPIOB_AFRH |=  ((AF4_I2C1 << ((PIN_SCL - 8U) * 4U)) |
                    (AF4_I2C1 << ((PIN_SDA - 8U) * 4U)));

    /* --- 12.3  I2C1 saati + donanim reset'i ------------------------------ */
    RCC_APB1ENR1 |= RCC_I2C1EN;
    (void)RCC_APB1ENR1;

    RCC_APB1RSTR1 |=  RCC_I2C1RST;
    RCC_APB1RSTR1 &= ~RCC_I2C1RST;

    /* --- 12.4  Ayarlar PE=0 iken yapilir -------------------------------- */
    I2C1_CR1 = 0U;

    /* --- 12.5  TIMINGR  -------------------------------------------------
     * F4'te FREQ + CCR + TRISE olan her sey burada tek register'da:
     *
     *   bit 31:28  PRESC  = 3   -> on bolucu: t_presc = (3+1)/16MHz = 250 ns
     *   bit 23:20  SCLDEL = 4   -> veri kurulum suresi (data setup)
     *   bit 19:16  SDADEL = 2   -> veri tutma suresi (data hold)
     *   bit 15:8   SCLH   = 0x0F-> SCL yuksek suresi (16 * 250ns = 4.0 us)
     *   bit  7:0   SCLL   = 0x13-> SCL dusuk suresi  (20 * 250ns = 5.0 us)
     *
     * RM0351'deki resmi formulle dogrulayalim:
     *
     *   t_SCL = t_SYNC1 + t_SYNC2 + [(SCLH+1) + (SCLL+1)] x (PRESC+1) x t_I2CCLK
     *         = t_SYNC1 + t_SYNC2 + [(15+1) + (19+1)] x 4 x 62.5ns
     *         = t_SYNC1 + t_SYNC2 + 9.0 us
     *
     * t_SYNC1 + t_SYNC2 filtre ve kenar algilama gecikmeleridir, ~1 us tutar.
     * Toplam ~10 us -> 100 kHz. (Referans: RM0351, Bolum 39.4.9 "I2C timings")
     *
     * SLAVE ICIN NOT: SCL'i master surdugu icin SCLH/SCLL bizde kullanilmaz;
     * bizim icin anlamli olanlar PRESC, SDADEL ve SCLDEL'dir (verinin ne
     * zaman gecerli sayilacagi). Yine de tam degeri yaziyoruz ki bu dosyayi
     * master'a cevirmek isterseniz tek satir degistirmeniz yetsin. */
    I2C1_TIMINGR = 0x30420F13U;

    /* --- 12.6  Kendi adresimiz ------------------------------------------
     * OAR1'i degistirmeden once OA1EN'i temizlemek gerekir (donanim kurali).
     * 7-bit adres, register'da BIR SOLA KAYIK durur: cunku hatta giden ilk
     * bayt "adres(7) + yon(1)" seklindedir ve donanim ayni hizalamayi
     * bekler. 0x42 << 1 = 0x84. */
    I2C1_OAR1 = 0U;
    I2C1_OAR1 = I2C_OAR1_OA1EN | (SLAVE_ADRES << 1);
    I2C1_OAR2 = 0U;                              /* ikinci adres kapali      */

    /* --- 12.7  Kesmeleri ac ve birimi calistir --------------------------
     * NOSTRETCH bitini 0 birakiyoruz (reset degeri). Bu, "hazir olana kadar
     * SCL'i asagida tut" iznidir; yazilimla yonetilen bir slave icin
     * HAYATIDIR. 1 yapilirsa kesmeye yetisemedigimiz her bayt kaybolur. */
    I2C1_CR1 = I2C_CR1_PE     |
               I2C_CR1_ADDRIE |          /* adresim cagrildi                 */
               I2C_CR1_RXIE   |          /* bayt geldi                       */
               I2C_CR1_TXIE   |          /* bayt istiyorlar                  */
               I2C_CR1_NACKIE |          /* karsi taraf "yeter" dedi         */
               I2C_CR1_STOPIE |          /* islem bitti                      */
               I2C_CR1_ERRIE;            /* hata olustu                      */

    /* --- 12.8  NVIC'te ilgili IRQ'lari ac -------------------------------
     * Cevre birimi kesmeyi "isteyebilir"; NVIC'te acilmazsa cekirdege
     * ulasmaz. Oncelik ayari yapmiyoruz: reset degeri olan 0 zaten en
     * yuksek onceliktir ve tek kesme kaynagimiz var. */
    NVIC_ISER0 = (1U << IRQ_I2C1_EV);            /* IRQ 31 -> ISER[0] bit 31 */
    NVIC_ISER1 = (1U << (IRQ_I2C1_ER - 32U));    /* IRQ 32 -> ISER[1] bit 0  */
}


/* ===========================================================================
 *  13) I2C1 OLAY KESMESI  --  isin kalbi
 * ===========================================================================
 *  Bu fonksiyonun adi rastgele degildir: startup_stm32l476rg.s icindeki
 *  vektor tablosunda 31 numarali IRQ girdisi tam olarak bu ismi arar.
 *  Ismi yanlis yazarsaniz derleyici uyarmaz, kesme sessizce Default_Handler'a
 *  gider ve program sonsuz donguye girer.
 *
 *  Bir islem sirasinda olaylarin sirasi:
 *
 *    YAZMA   [START][0x42+W]   -> ADDR (DIR=0)
 *            [register no]     -> RXNE   (ilk bayt = indeks)
 *            [veri]            -> RXNE   (registerlar[indeks] = veri)
 *            [STOP]            -> STOPF
 *
 *    OKUMA   [START][0x42+W]   -> ADDR (DIR=0)
 *            [register no]     -> RXNE
 *            [TEKRAR-START]
 *            [0x42+R]          -> ADDR (DIR=1)
 *            (master her bayti aldikca) -> TXIS, TXIS, ...
 *            (son bayta NACK)  -> NACKF
 *            [STOP]            -> STOPF
 * ========================================================================= */
void I2C1_EV_IRQHandler(void)
{
    u32 isr = I2C1_ISR;

    /* --- 13.1  ADDR: birisi bizim adresimizi cagirdi --------------------- */
    if ((isr & I2C_ISR_ADDR) != 0U) {

        if ((isr & I2C_ISR_DIR) != 0U) {
            /* DIR=1 -> master OKUMAK istiyor, sirada biz gonderecegiz.
             * TXDR'de onceki islemden kalmis bir bayt olabilir; TXE'ye 1
             * YAZMAK bu register'i bosaltir (bu bit bu yuzden yazilabilir).
             * Bosaltmazsak master ilk bayt olarak eski cop veriyi alir. */
            I2C1_ISR |= I2C_ISR_TXE;
        } else {
            /* DIR=0 -> master YAZACAK. Gelecek ILK bayt, hangi register'la
             * ilgilendigini soyleyen indekstir. */
            ilk_bayt_bekleniyor = 1U;
        }

        /* ADDR'i temizlemek "hazirim, devam et" demektir: bu ana kadar
         * donanim SCL'i asagida tutarak master'i bekletiyordu. */
        I2C1_ICR = I2C_ICR_ADDRCF;
    }

    /* --- 13.2  RXNE: master bize bir bayt yazdi -------------------------- */
    if ((isr & I2C_ISR_RXNE) != 0U) {
        u8 gelen = (u8)I2C1_RXDR;        /* okumak bayragi kendiliginden siler*/

        if (ilk_bayt_bekleniyor != 0U) {
            /* Ilk bayt = register indeksi. Maskeleme, hatali/kotu niyetli
             * bir indeksin dizinin disina tasmasini engeller. */
            reg_isaretcisi      = (u8)(gelen % REG_ADET);
            ilk_bayt_bekleniyor = 0U;
        } else {
            /* Sonraki baytlar veri. WHO_AM_I ve COUNTER salt okunurdur;
             * gercek sensorler de sabit kimlik register'ina yazdirmaz. */
            if (reg_isaretcisi == REG_LED_CTRL || reg_isaretcisi == REG_ECHO) {
                registerlar[reg_isaretcisi] = gelen;

                if (reg_isaretcisi == REG_LED_CTRL) {
                    led_ayarla(gelen & 1U);   /* tek BSRR yazmasi, cok hizli */
                }
                son_yazilan_reg  = reg_isaretcisi;
                son_yazilan_veri = gelen;
            }
            /* Isaretciyi ilerlet: master pesi sira birden fazla register
             * yazabilsin (otomatik artan adresleme). */
            reg_isaretcisi = (u8)((reg_isaretcisi + 1U) % REG_ADET);
        }
    }

    /* --- 13.3  TXIS: master bizden bir bayt bekliyor --------------------- */
    if ((isr & I2C_ISR_TXIS) != 0U) {
        /* TXDR'ye yazmak bayragi temizler ve SCL'i serbest birakir. */
        I2C1_TXDR      = registerlar[reg_isaretcisi];
        reg_isaretcisi = (u8)((reg_isaretcisi + 1U) % REG_ADET);
    }

    /* --- 13.4  NACKF: master "son bayti aldim, yeter" dedi --------------- *
     * Okuma isleminin normal bitisidir, hata degildir. Temizlenmezse bir
     * sonraki islem baslamaz.                                              */
    if ((isr & I2C_ISR_NACKF) != 0U) {
        I2C1_ICR = I2C_ICR_NACKCF;
    }

    /* --- 13.5  STOPF: islem tamamlandi ---------------------------------- */
    if ((isr & I2C_ISR_STOPF) != 0U) {
        I2C1_ICR = I2C_ICR_STOPCF;

        toplam_islem++;
        registerlar[REG_COUNTER] = (u8)toplam_islem;   /* 8 bit'e sigdigi kadar */

        ilk_bayt_bekleniyor = 0U;
        rapor_hazir         = 1U;        /* ana dongu UART'a satir bassin   */
    }
}


/* ===========================================================================
 *  14) I2C1 HATA KESMESI
 * ===========================================================================
 *  BERR : bus hatasi -- beklenmedik yerde START/STOP (genelde gurultu)
 *  ARLO : hakem kaybi -- baska bir master hattin kontrolunu aldi
 *  OVR  : tasma -- yazilim baytlara yetisemedi (NOSTRETCH=1 iken olur)
 *
 *  Bu bayraklar temizlenmezse kesme surekli tetiklenir ve program hicbir
 *  ise yaramadan donanip kalir. Bu yuzden bos birakmak yerine mutlaka
 *  temizliyor ve sayiyoruz.
 * ========================================================================= */
void I2C1_ER_IRQHandler(void)
{
    u32 isr = I2C1_ISR;

    if ((isr & (I2C_ISR_BERR | I2C_ISR_ARLO | I2C_ISR_OVR)) != 0U) {
        I2C1_ICR = I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF;
        hata_sayisi++;
    }
}


/* ===========================================================================
 *  15) main
 * ===========================================================================
 *  Ana dongu I2C isinin HICBIRINI yapmaz -- hepsi kesmede olur. Buradaki
 *  tek gorev, olan biteni UART'a rapor etmek.
 *
 *  Neden UART'a kesme icinde basmiyoruz? Bir satir yaklasik 40 bayt eder ve
 *  115200 baud'da ~3.5 ms surer. Kesme icinde bu kadar beklemek, o sirada
 *  gelen I2C baytlarini kacirmak demektir. Kural: kesme kisa olsun, uzun
 *  isler ana donguye birakilsin.
 * ========================================================================= */
int main(void)
{
    /* SIRA ONEMLI: HSI16 once acilir, cunku hem UART hem I2C saatini
     * ondan alacak. Kaynak hazir olmadan bagLAMAK, baud ve I2C zamanlamasini
     * ilk mikrosaniyelerde bozar. */
    hsi16_ac();
    led_kur();
    uart_kur();
    i2c_slave_kur();

    uart_metin("\r\n=== NUCLEO-L476RG  I2C SLAVE ===\r\n");
    uart_metin("Adres     : 0x42 (7-bit)\r\n");
    uart_metin("Pinler    : PB8=SCL, PB9=SDA (CN5)\r\n");
    uart_metin("Hiz       : 100 kHz, saat kaynagi HSI16\r\n");
    uart_metin("Bekleniyor: master'in ilk istegi...\r\n\r\n");

    for (;;) {
        if (rapor_hazir != 0U) {
            rapor_hazir = 0U;

            uart_metin("islem ");
            uart_sayi(toplam_islem);
            uart_metin("  LED=");
            uart_bayt((u8)('0' + (registerlar[REG_LED_CTRL] & 1U)));
            uart_metin("  ECHO=");
            uart_hex8(registerlar[REG_ECHO]);
            uart_metin("  son yazma: reg ");
            uart_hex8(son_yazilan_reg);
            uart_metin(" <- ");
            uart_hex8(son_yazilan_veri);

            if (hata_sayisi != 0U) {
                uart_metin("  [hata: ");
                uart_sayi(hata_sayisi);
                uart_metin("]");
            }
            uart_metin("\r\n");
        }
    }

    return 0;   /* buraya asla ulasilmaz */
}
