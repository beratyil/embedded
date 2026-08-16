/*
 * ============================================================================
 *  startup_stm32f407vg.s  --  Elle yazilmis baslangic kodu (startup)
 * ============================================================================
 *
 *  ST'nin hazir startup dosyasi KULLANILMADI. Cortex-M4'un reset davranisi
 *  burada sifirdan kodlanmistir.
 *
 *  CORTEX-M RESET SIRASI (donanimin yaptigi):
 *    1. Islemci 0x0000_0000 adresindeki ilk 32-bit kelimeyi okur ve
 *       MSP (Main Stack Pointer) register'ina yukler.
 *    2. 0x0000_0004 adresindeki ikinci kelimeyi okur ve PC'ye yukler,
 *       yani oradaki adrese dallanir -> Reset_Handler.
 *
 *  Not: Discovery kartinda BOOT0=0 oldugu icin 0x0000_0000 adresi
 *  Flash'in basi olan 0x0800_0000'a "aynalanir" (alias). Bu yuzden vektor
 *  tablosunu Flash'in en basina koymamiz yeterlidir.
 *
 *  Reset_Handler'in yazilim tarafinda yapmasi gerekenler:
 *    a) .data bolumunu Flash'tan SRAM'e kopyalamak
 *       (ilk degeri olan global degiskenler)
 *    b) .bss bolumunu sifirlamak
 *       (ilk degeri olmayan / 0 olan global degiskenler)
 *    c) main()'i cagirmak
 * ============================================================================
 */

    .syntax unified            /* Modern ARM/Thumb assembler sozdizimi       */
    .cpu    cortex-m4          /* Hedef cekirdek                             */
    .fpu    softvfp            /* FPU kullanmiyoruz, yazilim float           */
    .thumb                     /* Cortex-M yalnizca Thumb-2 calistirir       */


/* ---------------------------------------------------------------------------
 * Linker script (stm32f407vg.ld) tarafindan uretilen sembollere referanslar.
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
 *  Asagidaki vektor tablosundaki tum kesmeler, ayri ayri tanimlanmadiklari
 *  surece "weak" baglantiyla buraya yonlendirilir. Beklenmedik bir kesme
 *  (ornegin HardFault) olustugunda program burada sonsuz donguye girer;
 *  debugger ile durdurdugunuzda PC bu adreste olur ve hatayi anlarsiniz.
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
 *  Bu ornekte sadece cekirdek sistem kesmeleri listelenmistir; blink icin
 *  hicbir cevre birimi kesmesi kullanilmadigindan IRQ tablosu kisa tutuldu.
 * ==========================================================================*/
    .section .isr_vector, "a", %progbits
    .type   g_pfnVectors, %object
    .global g_pfnVectors
g_pfnVectors:
    .word   _estack               /*  0  Yigin tepesi (SRAM'in en ustu)      */
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
    /* Bu noktadan sonra IRQ0 (WWDG), IRQ1 (PVD), ... gelirdi.
     * Kesme kullanmadigimiz icin tabloyu burada bitiriyoruz. */

    .size   g_pfnVectors, .-g_pfnVectors


/*
 * ============================================================================
 *  ZAYIF (weak) TAKMA ADLAR
 * ============================================================================
 *  "weak" bir sembol, baska bir dosyada ayni isimde GERCEK bir fonksiyon
 *  tanimlanirsa onun lehine sessizce devre disi kalir. Boylece ileride
 *  ornegin SysTick_Handler yazmak isterseniz, bu dosyaya dokunmadan
 *  main.c icinde tanimlamaniz yeterli olur.
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
