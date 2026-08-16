/*
 * ============================================================================
 *  main.c  --  STM32F407VG (STM32F4-Discovery) saf register seviyesi BLINK
 * ============================================================================
 *
 *  HEDEF DONANIM : STM32F407VG, Cortex-M4F, 1 MB Flash, 128 KB SRAM
 *  KART          : STM32F407G-DISC1 / STM32F4DISCOVERY
 *
 *  LED BAGLANTILARI (kart semasindan):
 *      PD12 -> LD4  YESIL   (green)
 *      PD13 -> LD3  TURUNCU (orange)
 *      PD14 -> LD5  KIRMIZI (red)
 *      PD15 -> LD6  MAVI    (blue)
 *
 *  LED'ler pinden GND'ye dogru bagli (aktif-YUKSEK):
 *      pin = 1  -> LED yanar
 *      pin = 0  -> LED soner
 *
 *  KURAL: Hicbir kutuphane yok.
 *      - HAL yok, LL yok, CMSIS yok, standart C kutuphanesi yok.
 *      - <stdint.h> dahi dahil edilmedi; tipleri asagida kendimiz tanimliyoruz.
 *      - Tum donanim erisimi, hafiza haritasindaki adreslere dogrudan
 *        yazma/okuma ile yapiliyor.
 *
 *  SAAT (CLOCK) DURUMU:
 *      Reset sonrasi islemci dahili RC osilatoru HSI ile 16 MHz'de calisir.
 *      PLL'i 168 MHz'e cikarmak bu ornegin kapsami disinda; 16 MHz yeterli.
 *      Asagidaki SysTick hesabi bu 16 MHz'e gore yapilmistir.
 * ============================================================================
 */

/* ---------------------------------------------------------------------------
 * 0) TEMEL TIPLER
 * ---------------------------------------------------------------------------
 * Kutuphane kullanmadigimiz icin <stdint.h> yok. ARM Cortex-M ABI'sinde
 * 'unsigned int' tam olarak 32 bittir, dolayisiyla guvenle kullanabiliriz.
 *
 * 'volatile' anahtar kelimesi KRITIK: donanim register'larinin degeri
 * program disindaki nedenlerle (donanimin kendisi tarafindan) degisebilir.
 * volatile olmadan derleyici "bu degeri zaten okumustum" deyip okumayi
 * optimize edip silebilir ve kod calismaz.
 * ------------------------------------------------------------------------- */
typedef unsigned int          u32;   /* 32-bit isaretsiz tamsayi            */
typedef volatile unsigned int vu32;  /* 32-bit donanim register'i           */

/* Bir adresi "32-bit donanim register'i" olarak yorumlayan yardimci makro.
 * (*(vu32 *)adres)  ->  adresi vu32 pointer'a cevir, sonra icerigine eris.
 * Hem okunabilir hem de yazilabilir bir ifade uretir (lvalue). */
#define REG32(adres)  (*(vu32 *)(adres))


/* ---------------------------------------------------------------------------
 * 1) HAFIZA HARITASI  (Referans: RM0090, Tablo 1 "Register boundary addresses")
 * ---------------------------------------------------------------------------
 * STM32F4'te cevre birimleri (peripheral) bloklara ayrilmistir:
 *
 *   0x4000 0000  APB1 baslangici
 *   0x4001 0000  APB2 baslangici
 *   0x4002 0000  AHB1 baslangici   <- GPIO'lar ve RCC burada
 *
 * AHB1 uzerinde GPIO portlari 0x400 (1 KB) araliklarla dizilmistir:
 *   GPIOA = 0x4002 0000
 *   GPIOB = 0x4002 0400
 *   GPIOC = 0x4002 0800
 *   GPIOD = 0x4002 0C00   <- LED'lerimiz burada
 * ------------------------------------------------------------------------- */
#define PERIPH_BASE   0x40000000U            /* Tum cevre birimlerinin koku  */
#define AHB1_BASE     (PERIPH_BASE + 0x00020000U)  /* 0x4002 0000            */

#define GPIOD_BASE    (AHB1_BASE + 0x0C00U)  /* 0x4002 0C00                  */
#define RCC_BASE      (AHB1_BASE + 0x3800U)  /* 0x4002 3800                  */


/* ---------------------------------------------------------------------------
 * 2) RCC (Reset and Clock Control) REGISTER'LARI
 * ---------------------------------------------------------------------------
 * STM32'de her cevre birimi, guc tasarrufu icin reset aninda SAATSIZ gelir.
 * Saati kapali bir bloga yazmaya calisirsaniz yazma kaybolur, okuma 0 doner.
 * Bu yuzden GPIOD'ye dokunmadan ONCE saatini acmak zorundayiz.
 *
 * AHB1ENR = "AHB1 peripheral clock ENable Register", offset 0x30.
 * Bit dizilimi (RM0090, 6.3.10):
 *      bit 0 : GPIOAEN
 *      bit 1 : GPIOBEN
 *      bit 2 : GPIOCEN
 *      bit 3 : GPIODEN   <- bizim ihtiyacimiz olan bit
 *      ...
 * ------------------------------------------------------------------------- */
#define RCC_AHB1ENR   REG32(RCC_BASE + 0x30U)
#define RCC_GPIODEN   (1U << 3)              /* AHB1ENR bit 3 = GPIOD saati  */


/* ---------------------------------------------------------------------------
 * 3) GPIOD REGISTER'LARI  (RM0090, Bolum 8.4)
 * ---------------------------------------------------------------------------
 * Her GPIO portunun kendi 1 KB'lik blogunda su register'lar bulunur:
 *
 *   Offset  Isim      Genislik/pin   Aciklama
 *   ------  --------  ------------   --------------------------------------
 *   0x00    MODER     2 bit          Pin modu: giris/cikis/alternatif/analog
 *   0x04    OTYPER    1 bit          Cikis tipi: push-pull / open-drain
 *   0x08    OSPEEDR   2 bit          Cikis hizi (slew rate)
 *   0x0C    PUPDR     2 bit          Dahili pull-up / pull-down
 *   0x10    IDR       1 bit (RO)     Input Data Register  - pin'i OKU
 *   0x14    ODR       1 bit (RW)     Output Data Register - pin'e YAZ
 *   0x18    BSRR      1 bit (WO)     Bit Set/Reset - atomik set & clear
 *
 * "2 bit/pin" olan register'larda pin N'in alani  (2*N) bitinden baslar.
 * Ornek: PD12 -> MODER'de bit 25:24,  PD15 -> MODER'de bit 31:30.
 * ------------------------------------------------------------------------- */
#define GPIOD_MODER   REG32(GPIOD_BASE + 0x00U)
#define GPIOD_OTYPER  REG32(GPIOD_BASE + 0x04U)
#define GPIOD_OSPEEDR REG32(GPIOD_BASE + 0x08U)
#define GPIOD_PUPDR   REG32(GPIOD_BASE + 0x0CU)
#define GPIOD_ODR     REG32(GPIOD_BASE + 0x14U)
#define GPIOD_BSRR    REG32(GPIOD_BASE + 0x18U)

/* LED pin numaralari ve bit maskeleri.
 * Maske = ilgili pinin ODR/BSRR icindeki 1-bitlik konumu. */
#define LED_YESIL     12U
#define LED_TURUNCU   13U
#define LED_KIRMIZI   14U
#define LED_MAVI      15U

#define M_YESIL       (1U << LED_YESIL)      /* 0x1000 */
#define M_TURUNCU     (1U << LED_TURUNCU)    /* 0x2000 */
#define M_KIRMIZI     (1U << LED_KIRMIZI)    /* 0x4000 */
#define M_MAVI        (1U << LED_MAVI)       /* 0x8000 */
#define M_HEPSI       (M_YESIL | M_TURUNCU | M_KIRMIZI | M_MAVI)  /* 0xF000 */


/* ---------------------------------------------------------------------------
 * 4) SysTick  --  Cortex-M cekirdeginin icindeki 24-bit geri sayici
 * ---------------------------------------------------------------------------
 * SysTick bir ST cevre birimi degil, ARM cekirdeginin parcasidir; bu yuzden
 * adresi 0xE000E010'daki "System Control Space" bolgesindedir ve RCC ile
 * ayrica saat acmaya gerek yoktur.
 *
 * Neden bos dongu (for(;;){}) yerine SysTick?
 *   - Bos dongunun suresi derleyici optimizasyon seviyesine gore degisir.
 *   - SysTick donanim sayacidir; suresi -O0 ile -O2 arasinda degismez.
 *
 * Register'lar (ARMv7-M Architecture Reference Manual, B3.3):
 *   0xE000E010  CTRL   bit0 ENABLE, bit1 TICKINT, bit2 CLKSOURCE,
 *                      bit16 COUNTFLAG (okununca temizlenir)
 *   0xE000E014  LOAD   yeniden yukleme degeri (24 bit, max 0x00FFFFFF)
 *   0xE000E018  VAL    anlik sayac degeri (yazmak sayaci sifirlar)
 * ------------------------------------------------------------------------- */
#define SYSTICK_CTRL  REG32(0xE000E010U)
#define SYSTICK_LOAD  REG32(0xE000E014U)
#define SYSTICK_VAL   REG32(0xE000E018U)

#define SYSTICK_ENABLE    (1U << 0)   /* Sayaci calistir                     */
#define SYSTICK_CLKSOURCE (1U << 2)   /* 1 = islemci saati (bolunmemis HSI)  */
#define SYSTICK_COUNTFLAG (1U << 16)  /* Sifira ulasildiginda 1 olur         */

/* Reset sonrasi calistigimiz HSI osilatorunun frekansi. */
#define SISTEM_SAATI_HZ   16000000U

/* 1 ms'de gecen islemci cevrimi sayisi = 16 000 000 / 1000 = 16 000.
 * Sayac LOAD'dan 0'a kadar (LOAD + 1) adimda indigi icin 1 cikariyoruz. */
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
     *                yoklayacagiz (polling). Bu sayede NVIC/handler gerekmez. */
    SYSTICK_CTRL = SYSTICK_CLKSOURCE | SYSTICK_ENABLE;
}


/* ---------------------------------------------------------------------------
 * 6) Milisaniye cinsinden bekleme
 * ---------------------------------------------------------------------------
 * COUNTFLAG biti, sayac 0'a her ulastiginda donanim tarafindan 1 yapilir ve
 * CTRL register'i OKUNDUGU anda otomatik olarak 0'a doner. Yani her okuma
 * "bir periyot doldu mu?" sorusunun cevabidir; ayrica temizlemek gerekmez.
 * ------------------------------------------------------------------------- */
static void bekle_ms(u32 milisaniye)
{
    while (milisaniye != 0U) {
        /* COUNTFLAG 1 olana kadar bosta bekle -> 1 ms geciyor */
        while ((SYSTICK_CTRL & SYSTICK_COUNTFLAG) == 0U) {
            /* bilincli olarak bos: donanim sayacini yokluyoruz */
        }
        milisaniye--;
    }
}


/* ---------------------------------------------------------------------------
 * 7) LED pinlerini cikis olarak yapilandir
 * ---------------------------------------------------------------------------
 * Register'lari degistirirken daima "oku - maskele - yaz" (read-modify-write)
 * yontemini kullaniyoruz. Dogrudan "= deger" yazmak ayni port uzerindeki
 * diger 12 pinin ayarlarini silerdi.
 * ------------------------------------------------------------------------- */
static void led_pinlerini_kur(void)
{
    /* --- 7.1  GPIOD'nin saatini ac ------------------------------------- */
    RCC_AHB1ENR |= RCC_GPIODEN;

    /* Saat aciliminin cevre birimine ulasmasi birkac cevrim surer. Hemen
     * ardindan gelen yazma kaybolabilir; register'i geri okumak bu gecikmeyi
     * garantiler (ST'nin errata'da onerdigi standart yontem). */
    (void)RCC_AHB1ENR;

    /* --- 7.2  MODER: pinleri "genel amacli cikis" yap ------------------- */
    /* MODER'de her pin 2 bit kaplar:
     *      00 = Giris (reset degeri)
     *      01 = Genel amacli cikis      <- istedigimiz
     *      10 = Alternatif fonksiyon
     *      11 = Analog
     * PD12 -> bit 25:24, PD13 -> 27:26, PD14 -> 29:28, PD15 -> 31:30
     *
     * Once o 4 pine ait 8 biti '11' maskesiyle temizliyoruz, sonra '01'
     * degerlerini yerlestiriyoruz. */
    GPIOD_MODER &= ~((3U << (LED_YESIL   * 2)) |
                     (3U << (LED_TURUNCU * 2)) |
                     (3U << (LED_KIRMIZI * 2)) |
                     (3U << (LED_MAVI    * 2)));

    GPIOD_MODER |=  ((1U << (LED_YESIL   * 2)) |
                     (1U << (LED_TURUNCU * 2)) |
                     (1U << (LED_KIRMIZI * 2)) |
                     (1U << (LED_MAVI    * 2)));

    /* --- 7.3  OTYPER: push-pull cikis ---------------------------------- */
    /* 0 = push-pull (pin hem VDD'ye hem GND'ye surebilir)  <- LED icin dogru
     * 1 = open-drain (sadece GND'ye ceker, harici pull-up gerekir)
     * Reset degeri zaten 0'dir; yine de niyetimizi acikca belirtiyoruz. */
    GPIOD_OTYPER &= ~M_HEPSI;

    /* --- 7.4  OSPEEDR: dusuk hiz yeterli -------------------------------- */
    /* 00 = Low, 01 = Medium, 10 = High, 11 = Very high.
     * LED saniyede birkac kez yanip soner; yuksek slew rate sadece gereksiz
     * akim tuketimi ve elektromanyetik gurultu uretir. 00'da birakiyoruz. */
    GPIOD_OSPEEDR &= ~((3U << (LED_YESIL   * 2)) |
                       (3U << (LED_TURUNCU * 2)) |
                       (3U << (LED_KIRMIZI * 2)) |
                       (3U << (LED_MAVI    * 2)));

    /* --- 7.5  PUPDR: dahili direnc yok ---------------------------------- */
    /* Pin push-pull cikis oldugu icin seviyeyi surekli surer; pull-up veya
     * pull-down bir islev gormez, sadece bosuna akim akitir. 00 = yok. */
    GPIOD_PUPDR &= ~((3U << (LED_YESIL   * 2)) |
                     (3U << (LED_TURUNCU * 2)) |
                     (3U << (LED_KIRMIZI * 2)) |
                     (3U << (LED_MAVI    * 2)));

    /* --- 7.6  Baslangicta tum LED'ler sonuk ----------------------------- */
    /* BSRR'nin ust yarisi (bit 31:16) RESET (0 yap) alanidir.
     * M_HEPSI = 0xF000 oldugundan 16 sola kaydirinca 0xF000_0000 olur. */
    GPIOD_BSRR = (M_HEPSI << 16);
}


/* ---------------------------------------------------------------------------
 * 8) LED yakma / sondurme  --  BSRR ile
 * ---------------------------------------------------------------------------
 * ODR'ye "oku-degistir-yaz" yapmak yerine BSRR kullaniyoruz. Nedeni:
 *
 *   ODR |= maske;   ->  3 islem: LDR, ORR, STR  (atomik DEGIL)
 *   BSRR = maske;   ->  1 islem: STR            (atomik)
 *
 * Arada bir kesme gelip ayni portun baska bir pinini degistirirse, ODR
 * yonteminde o degisiklik kaybolur (read-modify-write yarisi). BSRR donanim
 * seviyesinde tek yazmada is bitirdigi icin bu yaris kosulu olusamaz.
 *
 * BSRR bit haritasi:
 *   bit 15:0   BSx -> 1 yazmak ilgili pini SET eder (1 yapar)
 *   bit 31:16  BRx -> 1 yazmak ilgili pini RESET eder (0 yapar)
 *   0 yazilan bitlerin hicbir etkisi yoktur; bu yuzden maskeleme gerekmez.
 * ------------------------------------------------------------------------- */
static void led_yak(u32 maske)
{
    GPIOD_BSRR = maske;              /* alt yari: SET   -> LED yanar */
}

static void led_sondur(u32 maske)
{
    GPIOD_BSRR = (maske << 16);      /* ust yari: RESET -> LED soner */
}


/* ---------------------------------------------------------------------------
 * 9) main  --  giris noktasi
 * ---------------------------------------------------------------------------
 * Bu fonksiyon startup_stm32f407vg.s icindeki Reset_Handler tarafindan,
 * .data kopyalanip .bss sifirlandiktan sonra cagrilir.
 * Gomulu sistemde donulecek bir isletim sistemi olmadigi icin main asla
 * geri donmez; sonsuz dongude kalir.
 * ------------------------------------------------------------------------- */
int main(void)
{
    /* Kullanacagimiz LED'ler, yanma sirasina gore. */
    const u32 sira[4] = { M_YESIL, M_TURUNCU, M_KIRMIZI, M_MAVI };

    led_pinlerini_kur();   /* GPIOD saati + PD12..PD15 cikis  */
    systick_baslat();      /* 1 ms'lik zaman tabani           */

    /* Sonsuz dongu: LED'ler sirayla yanip soner (kayan isik). */
    for (;;) {
        for (u32 i = 0U; i < 4U; i++) {
            led_yak(sira[i]);        /* siradaki LED'i yak    */
            bekle_ms(150U);          /* 150 ms goruntule      */
            led_sondur(sira[i]);     /* ayni LED'i sondur     */
        }
    }

    /* Buraya asla ulasilmaz; derleyiciyi memnun etmek icin. */
    return 0;
}
