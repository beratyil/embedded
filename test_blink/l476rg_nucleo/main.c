/*
 * ============================================================================
 *  main.c  --  NUCLEO-L476RG saf register seviyesi BLINK
 * ============================================================================
 *
 *  HEDEF DONANIM : STM32L476RG, Cortex-M4F, 1 MB Flash, 128 KB SRAM
 *  KART          : NUCLEO-L476RG (MB1136)
 *
 *  LED BAGLANTISI:
 *      PA5 -> LD2  YESIL   (kart uzerindeki tek kullanici LED'i)
 *
 *  LED pinden GND'ye dogru bagli (aktif-YUKSEK):
 *      pin = 1  -> LED yanar
 *      pin = 0  -> LED soner
 *
 *  KURAL: Hicbir kutuphane yok (HAL / LL / CMSIS / libc yok).
 *
 * ----------------------------------------------------------------------------
 *  DIKKAT -- STM32F407 ile FARKLAR
 * ----------------------------------------------------------------------------
 *  Bu iki cip ayni Cortex-M4 cekirdegini kullanir, ama cevre birimlerinin
 *  ADRESLERI ve saat register'lari FARKLIDIR. F407 kodunu kopyalayip
 *  adresleri degistirmeden kullanirsaniz kod sessizce calismaz.
 *
 *    Konu                | STM32F407VG        | STM32L476RG
 *    --------------------+--------------------+---------------------------
 *    GPIO veri yolu      | AHB1               | AHB2
 *    GPIO taban adresi   | 0x4002 0000        | 0x4800 0000
 *    RCC taban adresi    | 0x4002 3800        | 0x4002 1000
 *    GPIO saat register'i| AHB1ENR (ofs 0x30) | AHB2ENR (ofs 0x4C)
 *    Reset sonrasi saat  | HSI  16 MHz        | MSI   4 MHz
 *    Kullanici LED'i     | PD12..PD15 (4 adet)| PA5 (1 adet)
 *
 *  GPIO register'larinin KENDI ic ofsetleri (MODER 0x00, ODR 0x14, BSRR 0x18)
 *  her iki ailede de aynidir; degisen sadece portun taban adresidir.
 *
 *  SAAT (CLOCK) DURUMU:
 *      STM32L4 ailesi reset sonrasi MSI (Multi-Speed Internal) osilatoru ile
 *      4 MHz'de baslar -- F4'teki 16 MHz HSI degil. Asagidaki SysTick hesabi
 *      bu 4 MHz'e goredir. Bu deger yanlis olursa LED dogru yanar ama
 *      zamanlama 4 kat saser.
 * ============================================================================
 */

/* ---------------------------------------------------------------------------
 * 0) TEMEL TIPLER
 * ---------------------------------------------------------------------------
 * Kutuphane yok, <stdint.h> bile yok. ARM Cortex-M ABI'sinde 'unsigned int'
 * tam olarak 32 bittir.
 *
 * 'volatile': donanim register'inin degeri programimiz disindaki nedenlerle
 * degisebilir. Bu anahtar kelime olmadan derleyici okumayi "gereksiz" sayip
 * silebilir ve kod calismaz.
 * ------------------------------------------------------------------------- */
typedef unsigned int          u32;   /* 32-bit isaretsiz tamsayi            */
typedef volatile unsigned int vu32;  /* 32-bit donanim register'i           */

/* Bir adresi 32-bit donanim register'i olarak yorumlayan makro. */
#define REG32(adres)  (*(vu32 *)(adres))


/* ---------------------------------------------------------------------------
 * 1) HAFIZA HARITASI  (Referans: RM0351, Tablo 1 "Memory map")
 * ---------------------------------------------------------------------------
 * STM32L4'te cevre birimleri su bloklarda toplanmistir:
 *
 *   0x4000 0000  APB1
 *   0x4001 0000  APB2
 *   0x4002 0000  AHB1   <- RCC burada (0x4002 1000)
 *   0x4800 0000  AHB2   <- GPIO portlari burada
 *
 * AHB2 uzerinde GPIO portlari 0x400 (1 KB) araliklarla dizilir:
 *   GPIOA = 0x4800 0000   <- LED'imiz burada
 *   GPIOB = 0x4800 0400
 *   GPIOC = 0x4800 0800   (mavi kullanici butonu PC13 burada)
 *   GPIOD = 0x4800 0C00
 * ------------------------------------------------------------------------- */
#define GPIOA_BASE    0x48000000U            /* AHB2 -- F4'ten farkli!       */
#define RCC_BASE      0x40021000U            /* AHB1 -- F4'ten farkli!       */


/* ---------------------------------------------------------------------------
 * 2) RCC (Reset and Clock Control) REGISTER'LARI
 * ---------------------------------------------------------------------------
 * Guc tasarrufu icin her cevre birimi reset aninda SAATSIZ gelir. Saati
 * kapali bir bloga yazma islemi kaybolur, okuma 0 doner. Bu yuzden GPIOA'ya
 * dokunmadan ONCE saatini acmak zorundayiz.
 *
 * STM32L4'te GPIO'lar AHB2 uzerindedir; dolayisiyla saat biti AHB1ENR'de
 * degil, AHB2ENR'dedir (ofset 0x4C).
 *
 * AHB2ENR bit dizilimi (RM0351, 6.4.17):
 *      bit 0 : GPIOAEN   <- bizim ihtiyacimiz olan bit
 *      bit 1 : GPIOBEN
 *      bit 2 : GPIOCEN
 *      bit 3 : GPIODEN
 *      ...
 * ------------------------------------------------------------------------- */
#define RCC_AHB2ENR   REG32(RCC_BASE + 0x4CU)
#define RCC_GPIOAEN   (1U << 0)              /* AHB2ENR bit 0 = GPIOA saati  */


/* ---------------------------------------------------------------------------
 * 3) GPIOA REGISTER'LARI  (RM0351, Bolum 8.4)
 * ---------------------------------------------------------------------------
 *   Offset  Isim      Genislik/pin   Aciklama
 *   ------  --------  ------------   --------------------------------------
 *   0x00    MODER     2 bit          Pin modu: giris/cikis/alternatif/analog
 *   0x04    OTYPER    1 bit          Cikis tipi: push-pull / open-drain
 *   0x08    OSPEEDR   2 bit          Cikis hizi (slew rate)
 *   0x0C    PUPDR     2 bit          Dahili pull-up / pull-down
 *   0x10    IDR       1 bit (RO)     Input Data Register  - pin'i OKU
 *   0x14    ODR       1 bit (RW)     Output Data Register - pin'e YAZ
 *   0x18    BSRR      1 bit (WO)     Bit Set/Reset - atomik set & clear
 *   0x28    BRR       1 bit (WO)     Bit Reset (L4'e ozgu; F4'te YOK)
 *
 * "2 bit/pin" olan register'larda pin N'in alani (2*N) bitinden baslar.
 * Ornek: PA5 -> MODER'de bit 11:10.
 * ------------------------------------------------------------------------- */
#define GPIOA_MODER   REG32(GPIOA_BASE + 0x00U)
#define GPIOA_OTYPER  REG32(GPIOA_BASE + 0x04U)
#define GPIOA_OSPEEDR REG32(GPIOA_BASE + 0x08U)
#define GPIOA_PUPDR   REG32(GPIOA_BASE + 0x0CU)
#define GPIOA_BSRR    REG32(GPIOA_BASE + 0x18U)

/* LED pin numarasi ve bit maskesi. */
#define LED_PIN       5U                     /* PA5 = LD2 (yesil)            */
#define M_LED         (1U << LED_PIN)        /* 0x0020                       */


/* ---------------------------------------------------------------------------
 * 4) SysTick  --  Cortex-M cekirdeginin icindeki 24-bit geri sayici
 * ---------------------------------------------------------------------------
 * SysTick bir ST cevre birimi degil, ARM cekirdeginin parcasidir. Bu yuzden:
 *   - Adresi her Cortex-M cipte aynidir (0xE000E010),
 *   - RCC ile ayrica saat acmaya GEREK YOKTUR.
 * F407 kodundaki SysTick bolumu ile bu bolum birebir aynidir; degisen tek
 * sey asagidaki frekans sabitidir.
 *
 * Neden bos dongu (for(;;){}) yerine SysTick?
 *   - Bos dongunun suresi optimizasyon seviyesine gore degisir.
 *   - SysTick donanim sayacidir; -O0 ile -O2 arasinda suresi degismez.
 * ------------------------------------------------------------------------- */
#define SYSTICK_CTRL  REG32(0xE000E010U)
#define SYSTICK_LOAD  REG32(0xE000E014U)
#define SYSTICK_VAL   REG32(0xE000E018U)

#define SYSTICK_ENABLE    (1U << 0)   /* Sayaci calistir                     */
#define SYSTICK_CLKSOURCE (1U << 2)   /* 1 = islemci saati                   */
#define SYSTICK_COUNTFLAG (1U << 16)  /* Sifira ulasildiginda 1 olur         */

/* Reset sonrasi MSI osilatorunun varsayilan frekansi.
 * RCC_CR icindeki MSIRANGE alani reset degeri 0110 = 4 MHz'dir. */
#define SISTEM_SAATI_HZ   4000000U

/* 1 ms'deki islemci cevrimi = 4 000 000 / 1000 = 4000.
 * Sayac LOAD'dan 0'a (LOAD + 1) adimda indigi icin 1 cikariyoruz. */
#define TICK_1MS          ((SISTEM_SAATI_HZ / 1000U) - 1U)


/* ---------------------------------------------------------------------------
 * 5) SysTick'i 1 ms periyoduna kur
 * ------------------------------------------------------------------------- */
static void systick_baslat(void)
{
    SYSTICK_LOAD = TICK_1MS;   /* 1 ms'lik periyot                           */
    SYSTICK_VAL  = 0U;         /* Sayaci ve COUNTFLAG'i temizle              */

    /* CLKSOURCE=1 -> islemci saatini kullan (AHB/8 degil)
     * ENABLE=1    -> saymaya basla
     * TICKINT     -> 0 birakiyoruz: kesme istemiyoruz, bayragi kendimiz
     *                yoklayacagiz. Boylece NVIC ve handler gerekmez. */
    SYSTICK_CTRL = SYSTICK_CLKSOURCE | SYSTICK_ENABLE;
}


/* ---------------------------------------------------------------------------
 * 6) Milisaniye cinsinden bekleme
 * ---------------------------------------------------------------------------
 * COUNTFLAG biti sayac 0'a her ulastiginda donanimca 1 yapilir ve CTRL
 * register'i OKUNDUGU anda otomatik 0'a doner. Yani her okuma "bir periyot
 * doldu mu?" sorusunun cevabidir; ayrica temizlemek gerekmez.
 * ------------------------------------------------------------------------- */
static void bekle_ms(u32 milisaniye)
{
    while (milisaniye != 0U) {
        while ((SYSTICK_CTRL & SYSTICK_COUNTFLAG) == 0U) {
            /* bilincli olarak bos: donanim sayacini yokluyoruz */
        }
        milisaniye--;
    }
}


/* ---------------------------------------------------------------------------
 * 7) LED pinini cikis olarak yapilandir
 * ---------------------------------------------------------------------------
 * Register'lari daima "oku - maskele - yaz" yontemiyle degistiriyoruz.
 *
 * Bu, GPIOA'da OZELLIKLE onemlidir: PA13 ve PA14 pinleri SWDIO ve SWCLK'tir,
 * yani ST-LINK'in cip ile konustugu hattir. GPIOA_MODER'in reset degeri
 * 0xABFF FFFF'tir ve bu deger o iki pini "alternatif fonksiyon" modunda
 * tutar. MODER'e dogrudan "= deger" yazarsaniz SWD pinlerini bozar, karti
 * debugger'a kapatirsiniz (kurtarmak icin BOOT0/reset numaralari gerekir).
 * ------------------------------------------------------------------------- */
static void led_pinini_kur(void)
{
    /* --- 7.1  GPIOA'nin saatini ac ------------------------------------- */
    RCC_AHB2ENR |= RCC_GPIOAEN;

    /* Saat aciliminin cevre birimine ulasmasi birkac cevrim surer; hemen
     * ardindan gelen yazma kaybolabilir. Register'i geri okumak bu gecikmeyi
     * garantiler (ST'nin errata'da onerdigi standart yontem). */
    (void)RCC_AHB2ENR;

    /* --- 7.2  MODER: pini "genel amacli cikis" yap ---------------------- */
    /* MODER'de her pin 2 bit kaplar:
     *      00 = Giris
     *      01 = Genel amacli cikis      <- istedigimiz
     *      10 = Alternatif fonksiyon
     *      11 = Analog                  <- L4'te cogu pinin RESET degeri
     * PA5 -> bit 11:10
     *
     * Onemli fark: F4'te pinler reset'te GIRIS (00) gelirken, L4'te guc
     * tuketimini azaltmak icin ANALOG (11) gelir. Sonuc degismez, cunku
     * zaten once temizleyip sonra yaziyoruz. */
    GPIOA_MODER &= ~(3U << (LED_PIN * 2));   /* PA5 alanini temizle (00)     */
    GPIOA_MODER |=  (1U << (LED_PIN * 2));   /* PA5 = 01 -> cikis            */

    /* --- 7.3  OTYPER: push-pull cikis ---------------------------------- */
    /* 0 = push-pull (hem VDD'ye hem GND'ye surer)  <- LED icin dogru
     * 1 = open-drain (sadece GND'ye ceker, harici pull-up gerekir)
     * Reset degeri zaten 0; niyetimizi acikca belirtiyoruz. */
    GPIOA_OTYPER &= ~M_LED;

    /* --- 7.4  OSPEEDR: dusuk hiz yeterli -------------------------------- */
    /* 00 = Low, 01 = Medium, 10 = High, 11 = Very high.
     * LED saniyede birkac kez yanip soner; yuksek slew rate sadece gereksiz
     * akim tuketimi ve elektromanyetik gurultu uretir. */
    GPIOA_OSPEEDR &= ~(3U << (LED_PIN * 2));

    /* --- 7.5  PUPDR: dahili direnc yok ---------------------------------- */
    /* Pin push-pull cikis oldugu icin seviyeyi surekli surer; pull-up/down
     * bir islev gormez, sadece bosuna akim akitir. 00 = yok. */
    GPIOA_PUPDR &= ~(3U << (LED_PIN * 2));

    /* --- 7.6  Baslangicta LED sonuk ------------------------------------- */
    /* BSRR'nin ust yarisi (bit 31:16) RESET alanidir. */
    GPIOA_BSRR = (M_LED << 16);
}


/* ---------------------------------------------------------------------------
 * 8) LED yakma / sondurme  --  BSRR ile
 * ---------------------------------------------------------------------------
 * ODR'ye "oku-degistir-yaz" yapmak yerine BSRR kullaniyoruz:
 *
 *   ODR |= maske;   ->  3 islem: LDR, ORR, STR  (atomik DEGIL)
 *   BSRR = maske;   ->  1 islem: STR            (atomik)
 *
 * Arada bir kesme gelip ayni portun baska pinini degistirirse, ODR
 * yonteminde o degisiklik kaybolur (read-modify-write yarisi). BSRR tek
 * yazmada is bitirdigi icin bu yaris kosulu olusamaz.
 *
 * BSRR bit haritasi:
 *   bit 15:0   BSx -> 1 yazmak pini SET eder (1 yapar)
 *   bit 31:16  BRx -> 1 yazmak pini RESET eder (0 yapar)
 *   0 yazilan bitlerin etkisi yoktur; bu yuzden maskeleme gerekmez.
 * ------------------------------------------------------------------------- */
static void led_yak(void)
{
    GPIOA_BSRR = M_LED;              /* alt yari: SET   -> LED yanar */
}

static void led_sondur(void)
{
    GPIOA_BSRR = (M_LED << 16);      /* ust yari: RESET -> LED soner */
}


/* ---------------------------------------------------------------------------
 * 9) main  --  giris noktasi
 * ---------------------------------------------------------------------------
 * Bu fonksiyon startup_stm32l476rg.s icindeki Reset_Handler tarafindan,
 * .data kopyalanip .bss sifirlandiktan sonra cagrilir. Donulecek bir isletim
 * sistemi olmadigi icin main asla geri donmez.
 * ------------------------------------------------------------------------- */
int main(void)
{
    led_pinini_kur();      /* GPIOA saati + PA5 cikis */
    systick_baslat();      /* 1 ms'lik zaman tabani   */

    /* Sonsuz dongu: LED 500 ms yanik, 500 ms sonuk (1 Hz). */
    for (;;) {
        led_yak();
        bekle_ms(500U);
        led_sondur();
        bekle_ms(500U);
    }

    /* Buraya asla ulasilmaz; derleyiciyi memnun etmek icin. */
    return 0;
}
