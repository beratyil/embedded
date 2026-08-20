/*
 * ============================================================================
 *  startup_stm32l476rg.s  --  Elle yazilmis baslangic kodu (startup)
 * ============================================================================
 *
 *  ST'nin hazir startup dosyasi KULLANILMADI.
 *
 *  NOT: Bu dosya, F407 icin yazilan startup ile neredeyse BIREBIR AYNIDIR.
 *  Sebebi onemli: burada yapilan islerin hicbiri ST'ye ait degildir, hepsi
 *  ARM Cortex-M cekirdeginin ve C dilinin gerektirdigi islerdir. Cipe ozel
 *  olan seyler (GPIO adresleri, saat register'lari) main.c'de; hafiza
 *  boyutlari ise linker script'tedir.
 *
 *  BU PROJEDE HICBIR KESME KULLANILMIYOR. Kamera surucusu bastan sona
 *  yoklamalidir (polling): SPI'da her bayt icin RXNE beklenir, I2C'de TXIS/
 *  RXNE beklenir, gecikmeler SysTick COUNTFLAG ile olculur. Dolayisiyla
 *  asagidaki vektor tablosundaki girdilerin TAMAMI Default_Handler'a bakar.
 *
 *  Peki tablo neden hala IRQ 32'ye kadar uzun? Iki sebeple:
 *    1) Dosya test_i2c'den kopyalandi ve cipe ozgu kismi degistirmemek,
 *       iki projeyi karsilastirmayi kolaylastiriyor.
 *    2) Ileride yakalama bitisini beklemek icin EXTI (VSYNC kesmesi) veya
 *       FIFO okumasini DMA'ya devretmek isterseniz tablo hazir olacak.
 *
 *  Yer israfi degil: tablo Flash'ta ~200 bayt tutar.
 *
 *  CORTEX-M RESET SIRASI (donanimin yaptigi):
 *    1. Islemci 0x0000_0000 adresindeki ilk 32-bit kelimeyi okur ve
 *       MSP (Main Stack Pointer) register'ina yukler.
 *    2. 0x0000_0004 adresindeki ikinci kelimeyi okur ve PC'ye yukler,
 *       yani oradaki adrese dallanir -> Reset_Handler.
 *
 *  Nucleo kartinda BOOT0 pini GND'ye cekili oldugu icin 0x0000_0000 adresi
 *  Flash'in basi olan 0x0800_0000'a "aynalanir" (alias). Bu yuzden vektor
 *  tablosunu Flash'in en basina koymamiz yeterlidir.
 *
 *  Reset_Handler'in yazilim tarafinda yapmasi gerekenler:
 *    a) .data bolumunu Flash'tan SRAM'e kopyalamak
 *    b) .bss bolumunu sifirlamak
 *    c) main()'i cagirmak
 * ============================================================================
 */

    .syntax unified            /* Modern ARM/Thumb assembler sozdizimi       */
    .cpu    cortex-m4          /* Hedef cekirdek (L476 de Cortex-M4)         */
    .fpu    softvfp            /* FPU kullanmiyoruz, yazilim float           */
    .thumb                     /* Cortex-M yalnizca Thumb-2 calistirir       */


/* ---------------------------------------------------------------------------
 * Linker script (stm32l476rg.ld) tarafindan uretilen sembollere referanslar.
 * Bunlar birer adres etiketidir, degisken degildir.
 * ------------------------------------------------------------------------- */
    .word   _sidata            /* .data'nin Flash'taki kaynak adresi         */
    .word   _sdata             /* .data'nin SRAM'deki baslangici             */
    .word   _edata             /* .data'nin SRAM'deki bitisi                 */
    .word   _sbss              /* .bss baslangici                            */
    .word   _ebss              /* .bss bitisi                                */


/*
 * ============================================================================
 *  Reset_Handler  --  Reset sonrasi calisan ilk yazilim kodu
 * ============================================================================
 */
    .section .text.Reset_Handler, "ax", %progbits
    .type   Reset_Handler, %function
    .global Reset_Handler
Reset_Handler:

    /* --- 1) Yigin isaretcisini (stack pointer) kur ------------------------
     * Donanim MSP'yi vektor tablosundan zaten yukledi, ama bootloader'dan
     * atlanarak gelinmis olma ihtimaline karsi acikca tekrar yazmak
     * savunmaci ve dogru bir aliskanliktir. */
    ldr     r0, =_estack
    mov     sp, r0

    /* --- 2) .data bolumunu Flash'tan SRAM'e kopyala -----------------------
     * "int sayac = 5;" gibi ILK DEGERI OLAN global degiskenler:
     * degerleri Flash'ta (kalici) durur, ama degisken SRAM'de yasar.
     * Bu yuzden calisma oncesi Flash -> SRAM kopyasi sarttir.
     *
     * r0 = hedef (SRAM'de yazacagimiz yer)
     * r1 = hedefin bitis adresi
     * r2 = kaynak (Flash)
     */
    ldr     r0, =_sdata
    ldr     r1, =_edata
    ldr     r2, =_sidata
    movs    r3, #0             /* r3 = kopyalama ofseti, 0'dan baslar        */
    b       DataKopyaKontrol   /* Once kosulu kontrol et (0 baytlik .data)   */

DataKopyaDongu:
    ldr     r4, [r2, r3]       /* r4 <- Flash[kaynak + ofset]                */
    str     r4, [r0, r3]       /* SRAM[hedef + ofset] <- r4                  */
    adds    r3, r3, #4         /* Ofseti bir 32-bit kelime ilerlet           */

DataKopyaKontrol:
    adds    r4, r0, r3         /* r4 = su anki hedef adresi                  */
    cmp     r4, r1             /* Bitise ulastik mi?                         */
    bcc     DataKopyaDongu     /* Hayir (r4 < r1) ise dongu devam            */

    /* --- 3) .bss bolumunu sifirla -----------------------------------------
     * "int sayac;" gibi ILK DEGERI OLMAYAN global degiskenler. C standardi
     * bunlarin 0 olmasini garanti eder; bu garantiyi burada biz sagliyoruz.
     * Flash'ta yer kaplamazlar, sadece SRAM'de adres ayrilir.
     *
     * r2 = silinecek alanin baslangici
     * r4 = silinecek alanin bitisi
     */
    ldr     r2, =_sbss
    ldr     r4, =_ebss
    movs    r3, #0             /* Yazacagimiz deger: 0                       */
    b       BssSilKontrol

BssSilDongu:
    str     r3, [r2]           /* SRAM[r2] <- 0                              */
    adds    r2, r2, #4         /* Sonraki kelimeye gec                       */

BssSilKontrol:
    cmp     r2, r4             /* Bitise geldik mi?                          */
    bcc     BssSilDongu        /* Hayir ise devam                            */

    /* --- 4) main()'i cagir -------------------------------------------------
     * Artik C calisma ortami (stack + .data + .bss) hazir. */
    bl      main

    /* --- 5) main donerse burada kilitlen ---------------------------------
     * Gomulu sistemde donulecek bir yer yok. Kacak bir donusu sessizce
     * belirsiz koda dusurmek yerine kontrollu sonsuz donguye aliyoruz. */
SonsuzDongu:
    b       SonsuzDongu

    .size   Reset_Handler, .-Reset_Handler


/*
 * ============================================================================
 *  Varsayilan kesme isleyicisi (Default_Handler)
 * ============================================================================
 *  Vektor tablosundaki tum kesmeler, ayri ayri tanimlanmadiklari surece
 *  "weak" baglantiyla buraya yonlendirilir. Beklenmedik bir kesme (ornegin
 *  HardFault) olustugunda program burada sonsuz donguye girer; debugger ile
 *  durdurdugunuzda PC bu adreste olur ve hatayi anlarsiniz.
 * ==========================================================================*/
    .section .text.Default_Handler, "ax", %progbits
    .type   Default_Handler, %function
    .global Default_Handler
Default_Handler:
    b       Default_Handler
    .size   Default_Handler, .-Default_Handler


/*
 * ============================================================================
 *  VEKTOR TABLOSU
 * ============================================================================
 *  Flash'in en basina yerlestirilir (linker script'te .isr_vector bolumu).
 *
 *  Yapisi:
 *    [0]  Yigin tepesi (stack top) -- adres degil, DEGER
 *    [1]  Reset_Handler adresi
 *    [2+] Sistem kesmeleri, ardindan cihaza ozgu (IRQ) kesmeleri
 *
 *  Ilk 16 girdi Cortex-M4'un cekirdek istisnalaridir ve her Cortex-M4 cipte
 *  aynidir. 16. girdiden itibaren cipe ozgu IRQ'lar baslar:
 *
 *      tablo indeksi = 16 + IRQ numarasi
 *
 *  TABLO KONUMA GORE CALISIR, ISME GORE DEGIL. Yani I2C1_EV_IRQHandler'in
 *  cagrilabilmesi icin tam olarak 47. sirada (16 + 31) olmasi gerekir; bu
 *  yuzden aradaki tum IRQ'lari atlamadan yazmak zorundayiz. Bir satir eksik
 *  olursa kesme yanlis fonksiyona gider ve hata bulmasi cok zor olur.
 *
 *  Tablo IRQ 32'de (I2C1_ER) bitiyor: bu projede daha yuksek numarali bir
 *  kesme kullanmiyoruz, dolayisiyla gerisini yazmak yer israfi olurdu.
 *  (Referans: RM0351, Tablo 58 "Vector table")
 * ==========================================================================*/
    .section .isr_vector, "a", %progbits
    .type   g_pfnVectors, %object
    .global g_pfnVectors
g_pfnVectors:
    .word   _estack               /*  0  Yigin tepesi (SRAM1'in en ustu)     */
    .word   Reset_Handler         /*  1  Reset                               */
    .word   NMI_Handler           /*  2  Maskelenemez kesme                  */
    .word   HardFault_Handler     /*  3  Ciddi hata (gecersiz adres vb.)     */
    .word   MemManage_Handler     /*  4  MPU ihlali                          */
    .word   BusFault_Handler      /*  5  Bus hatasi                          */
    .word   UsageFault_Handler    /*  6  Gecersiz komut / hizalama           */
    .word   0                     /*  7  Ayrilmis                            */
    .word   0                     /*  8  Ayrilmis                            */
    .word   0                     /*  9  Ayrilmis                            */
    .word   0                     /* 10  Ayrilmis                            */
    .word   SVC_Handler           /* 11  Supervisor Call                     */
    .word   DebugMon_Handler      /* 12  Debug Monitor                       */
    .word   0                     /* 13  Ayrilmis                            */
    .word   PendSV_Handler        /* 14  PendSV (RTOS gorev degisimi)        */
    .word   SysTick_Handler       /* 15  SysTick kesmesi                     */

    /* ---- Buradan sonrasi STM32L476'ya ozgu kesmeler (IRQ 0'dan itibaren) -*/
    .word   WWDG_IRQHandler                /* IRQ  0  Pencere bekci kopegi   */
    .word   PVD_PVM_IRQHandler             /* IRQ  1  Guc seviyesi izleme    */
    .word   TAMP_STAMP_IRQHandler          /* IRQ  2  Kurcalama / zaman damgasi*/
    .word   RTC_WKUP_IRQHandler            /* IRQ  3  RTC uyandirma          */
    .word   FLASH_IRQHandler               /* IRQ  4  Flash                  */
    .word   RCC_IRQHandler                 /* IRQ  5  Saat sistemi           */
    .word   EXTI0_IRQHandler               /* IRQ  6  Harici kesme hatti 0   */
    .word   EXTI1_IRQHandler               /* IRQ  7                         */
    .word   EXTI2_IRQHandler               /* IRQ  8                         */
    .word   EXTI3_IRQHandler               /* IRQ  9                         */
    .word   EXTI4_IRQHandler               /* IRQ 10                         */
    .word   DMA1_Channel1_IRQHandler       /* IRQ 11                         */
    .word   DMA1_Channel2_IRQHandler       /* IRQ 12                         */
    .word   DMA1_Channel3_IRQHandler       /* IRQ 13                         */
    .word   DMA1_Channel4_IRQHandler       /* IRQ 14                         */
    .word   DMA1_Channel5_IRQHandler       /* IRQ 15                         */
    .word   DMA1_Channel6_IRQHandler       /* IRQ 16                         */
    .word   DMA1_Channel7_IRQHandler       /* IRQ 17                         */
    .word   ADC1_2_IRQHandler              /* IRQ 18  Analog-dijital cevirici*/
    .word   CAN1_TX_IRQHandler             /* IRQ 19                         */
    .word   CAN1_RX0_IRQHandler            /* IRQ 20                         */
    .word   CAN1_RX1_IRQHandler            /* IRQ 21                         */
    .word   CAN1_SCE_IRQHandler            /* IRQ 22                         */
    .word   EXTI9_5_IRQHandler             /* IRQ 23  Harici kesme 5..9      */
    .word   TIM1_BRK_TIM15_IRQHandler      /* IRQ 24                         */
    .word   TIM1_UP_TIM16_IRQHandler       /* IRQ 25                         */
    .word   TIM1_TRG_COM_TIM17_IRQHandler  /* IRQ 26                         */
    .word   TIM1_CC_IRQHandler             /* IRQ 27                         */
    .word   TIM2_IRQHandler                /* IRQ 28                         */
    .word   TIM3_IRQHandler                /* IRQ 29                         */
    .word   TIM4_IRQHandler                /* IRQ 30                         */
    .word   I2C1_EV_IRQHandler             /* IRQ 31  <-- I2C1 OLAY kesmesi  */
    .word   I2C1_ER_IRQHandler             /* IRQ 32  <-- I2C1 HATA kesmesi  */

    .size   g_pfnVectors, .-g_pfnVectors


/*
 * ============================================================================
 *  ZAYIF (weak) TAKMA ADLAR
 * ============================================================================
 *  "weak" bir sembol, baska bir dosyada ayni isimde GERCEK bir fonksiyon
 *  tanimlanirsa onun lehine sessizce devre disi kalir. Boylece ileride
 *  ornegin SysTick_Handler yazmak isterseniz, bu dosyaya dokunmadan main.c
 *  icinde tanimlamaniz yeterli olur.
 * ==========================================================================*/
    .weak   NMI_Handler
    .thumb_set NMI_Handler, Default_Handler

    .weak   HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler

    .weak   MemManage_Handler
    .thumb_set MemManage_Handler, Default_Handler

    .weak   BusFault_Handler
    .thumb_set BusFault_Handler, Default_Handler

    .weak   UsageFault_Handler
    .thumb_set UsageFault_Handler, Default_Handler

    .weak   SVC_Handler
    .thumb_set SVC_Handler, Default_Handler

    .weak   DebugMon_Handler
    .thumb_set DebugMon_Handler, Default_Handler

    .weak   PendSV_Handler
    .thumb_set PendSV_Handler, Default_Handler

    .weak   SysTick_Handler
    .thumb_set SysTick_Handler, Default_Handler


/*
 * ----------------------------------------------------------------------------
 *  Cipe ozgu IRQ'lar icin zayif takma adlar
 * ----------------------------------------------------------------------------
 *  Hepsi varsayilan olarak Default_Handler'a (sonsuz dongu) baglidir.
 *  main.c icinde AYNI ISIMLE bir fonksiyon tanimlarsaniz, baglayici sizinkini
 *  tercih eder ve bu satir kendiliginden devre disi kalir.
 *
 *  BU PROJEDE main.c HICBIRINI TANIMLAMIYOR -- kamera surucusu tamamen
 *  yoklamali calisir. Tablo yalnizca ileride kesme eklemek istediginizde
 *  hazir olsun diye tam birakildi.
 *
 *  Ismi bir harf yanlis yazarsaniz derleyici hata VERMEZ: sizin fonksiyonunuz
 *  hicbir yerden cagrilmayan olu kod olur, kesme ise Default_Handler'a gidip
 *  program orada takilir. Kesme calismiyorsa once ismi kontrol edin.
 * --------------------------------------------------------------------------*/
    .macro  zayif_irq isim
    .weak   \isim
    .thumb_set \isim, Default_Handler
    .endm

    zayif_irq WWDG_IRQHandler
    zayif_irq PVD_PVM_IRQHandler
    zayif_irq TAMP_STAMP_IRQHandler
    zayif_irq RTC_WKUP_IRQHandler
    zayif_irq FLASH_IRQHandler
    zayif_irq RCC_IRQHandler
    zayif_irq EXTI0_IRQHandler
    zayif_irq EXTI1_IRQHandler
    zayif_irq EXTI2_IRQHandler
    zayif_irq EXTI3_IRQHandler
    zayif_irq EXTI4_IRQHandler
    zayif_irq DMA1_Channel1_IRQHandler
    zayif_irq DMA1_Channel2_IRQHandler
    zayif_irq DMA1_Channel3_IRQHandler
    zayif_irq DMA1_Channel4_IRQHandler
    zayif_irq DMA1_Channel5_IRQHandler
    zayif_irq DMA1_Channel6_IRQHandler
    zayif_irq DMA1_Channel7_IRQHandler
    zayif_irq ADC1_2_IRQHandler
    zayif_irq CAN1_TX_IRQHandler
    zayif_irq CAN1_RX0_IRQHandler
    zayif_irq CAN1_RX1_IRQHandler
    zayif_irq CAN1_SCE_IRQHandler
    zayif_irq EXTI9_5_IRQHandler
    zayif_irq TIM1_BRK_TIM15_IRQHandler
    zayif_irq TIM1_UP_TIM16_IRQHandler
    zayif_irq TIM1_TRG_COM_TIM17_IRQHandler
    zayif_irq TIM1_CC_IRQHandler
    zayif_irq TIM2_IRQHandler
    zayif_irq TIM3_IRQHandler
    zayif_irq TIM4_IRQHandler
    zayif_irq I2C1_EV_IRQHandler
    zayif_irq I2C1_ER_IRQHandler
