/*
 * ============================================================================
 *  stm32l476rg.ld  --  GNU ld baglayici (linker) betigi
 * ============================================================================
 *
 *  Linker script, derlenmis kod ve verinin cipin hafiza haritasinda NEREYE
 *  yerlesecegini anlatir.
 *
 *  STM32L476RG HAFIZA HARITASI:
 *      0x0800 0000 - 0x080F FFFF   Flash    1024 KB  (kalici, kod + sabitler)
 *      0x2000 0000 - 0x2001 7FFF   SRAM1      96 KB  (degiskenler + yigin)
 *      0x1000 0000 - 0x1000 7FFF   SRAM2      32 KB  (ayri blok)
 *
 *  F407 ile FARK -- bu dosyanin var olma sebebi:
 *      Konu        | STM32F407VG              | STM32L476RG
 *      ------------+--------------------------+--------------------------
 *      Flash       | 1024 KB @ 0x0800 0000    | 1024 KB @ 0x0800 0000  (ayni)
 *      Ana SRAM    |  128 KB @ 0x2000 0000    |   96 KB @ 0x2000 0000
 *      Ikinci RAM  |   64 KB CCM @ 0x1000 0000|   32 KB SRAM2 @ 0x1000 0000
 *      Yigin tepesi|      0x2002 0000         |      0x2001 8000
 *
 *  SRAM2 hakkinda: 0x1000 0000 adresinden erisilir, ayrica SRAM1'in hemen
 *  ardina (0x2001 8000) aynalanir. F4'teki CCM'den farkli olarak SRAM2'ye
 *  DMA erisebilir. Bu projede kullanmiyoruz, ama tanimli birakiyoruz ki
 *  ileride veri koymak istediginizde hazir olsun.
 * ============================================================================
 */

/* Programin giris noktasi. Debugger ve "--gc-sections" bu sembolden baslayarak
 * hangi kodun gercekten kullanildigini bulur. */
ENTRY(Reset_Handler)


/* ---------------------------------------------------------------------------
 *  Hafiza bolgeleri
 *  Izin bayraklari:  r = okunabilir, w = yazilabilir, x = calistirilabilir
 * ------------------------------------------------------------------------- */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 96K   /* SRAM1 */
    RAM2  (rw)  : ORIGIN = 0x10000000, LENGTH = 32K   /* SRAM2 */
}


/* ---------------------------------------------------------------------------
 *  Yigin (stack) tepesi
 * ---------------------------------------------------------------------------
 *  Cortex-M'de yigin YUKARIDAN ASAGIYA buyur (full-descending). Bu yuzden
 *  baslangic noktasi SRAM1'in EN UST adresidir ve her "push" ile asagi iner.
 *  Bu deger vektor tablosunun 0. girdisine yazilir.
 *
 *  0x20000000 + 96K = 0x20018000
 * ------------------------------------------------------------------------- */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* Yigin ve heap icin ayrilmasi istenen asgari alan. Asagida, kalan bos RAM'in
 * bu miktari karsilayip karsilamadigini kontrol edecegiz. */
_Min_Stack_Size = 0x400;   /*  1 KB yigin */
_Min_Heap_Size  = 0x000;   /*  malloc kullanmiyoruz -> heap yok */


SECTIONS
{
    /* -----------------------------------------------------------------------
     *  1) .isr_vector  --  Vektor tablosu
     * -----------------------------------------------------------------------
     *  MUTLAKA Flash'in ilk baytlarinda (0x0800 0000) olmali; islemci reset
     *  aninda yigin adresini ve Reset_Handler'i tam olarak oradan okur.
     *  KEEP(), --gc-sections'in "bu tabloyu kimse cagirmiyor" deyip silmesini
     *  engeller.
     * --------------------------------------------------------------------- */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    /* -----------------------------------------------------------------------
     *  2) .text  --  Calistirilabilir kod
     * --------------------------------------------------------------------- */
    .text :
    {
        . = ALIGN(4);
        *(.text)           /* Tum fonksiyonlar                              */
        *(.text*)          /* -ffunction-sections ile olusan .text.main vb. */
        *(.glue_7)         /* ARM <-> Thumb gecis kodu (bu cipte kullanilmaz)*/
        *(.glue_7t)
        *(.eh_frame)       /* Istisna (exception) cerceve bilgisi           */

        KEEP(*(.init))     /* C++ statik kurucu kancalari                   */
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;        /* Kodun bittigi adres                           */
    } >FLASH

    /* -----------------------------------------------------------------------
     *  3) .rodata  --  Salt okunur veri
     * -----------------------------------------------------------------------
     *  const diziler, string sabitleri... Degismedikleri icin SRAM'e
     *  kopyalanmaz, dogrudan Flash'tan okunurlar.
     * --------------------------------------------------------------------- */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    /* ARM'a ozgu "unwind" tablolari: yigin cozumleme icin. Kullanmasak da
     * bagliyicinin sikayet etmemesi icin yer veriyoruz. */
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM       : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    /* C++ / bazi C ortamlarinda main oncesi calisan kurucu fonksiyon
     * isaretcileri. Bos kalsalar da tanimli olmalari gerekir. */
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } >FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } >FLASH

    /* Flash'ta .data'nin kopyasinin durdugu adres. Reset_Handler bu adresten
     * okuyup SRAM'e tasiyacak. */
    _sidata = LOADADDR(.data);

    /* -----------------------------------------------------------------------
     *  4) .data  --  Ilk degeri OLAN global/static degiskenler
     * -----------------------------------------------------------------------
     *  Bu bolumun IKI adresi vardir; linker script'in en kritik noktasi budur:
     *
     *      VMA (Virtual/Run address) = SRAM  -> ">RAM"
     *      LMA (Load address)        = Flash -> "AT>FLASH"
     *
     *  Yani degisken calisirken SRAM'de yasar, ama ilk degeri Flash'ta
     *  saklanir (elektrik kesilince SRAM silinir). Reset_Handler bu iki adres
     *  arasinda kopyalama yapar.
     * --------------------------------------------------------------------- */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;        /* startup kodunun okudugu baslangic sembolu     */
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;        /* bitis sembolu                                 */
    } >RAM AT>FLASH

    /* -----------------------------------------------------------------------
     *  5) .bss  --  Ilk degeri OLMAYAN (veya 0 olan) degiskenler
     * -----------------------------------------------------------------------
     *  Flash'ta yer kaplamazlar (hepsi 0 olacagi icin saklamaya gerek yok);
     *  yalnizca SRAM'de adres ayrilir ve Reset_Handler bu alani sifirlar.
     * --------------------------------------------------------------------- */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)          /* Eski tarz ortak (tentative) tanimlar          */
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM

    /* -----------------------------------------------------------------------
     *  6) Yigin/heap yer kontrolu
     * -----------------------------------------------------------------------
     *  Gercek bir bolum degil; sadece "degiskenlerden sonra yigin icin yeterli
     *  RAM kaldi mi?" sorusunu DERLEME ZAMANINDA sorar. Kalmadiysa linker
     *  hata verir -- calisirken yigin tasmasi yasamaktan cok daha iyidir.
     * --------------------------------------------------------------------- */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >RAM

    /* -----------------------------------------------------------------------
     *  7) Meta veri bolumleri
     * -----------------------------------------------------------------------
     *  .ARM.attributes derleyicinin urettigi ABI bilgisini tasir (hangi FPU,
     *  hangi komut seti vb.). Cipe yuklenmez, ELF icinde kalir; adres olarak
     *  0 verilir cunku bir hafiza bolgesine ait degildir.
     *
     *  Not: burada libc.a/libm.a'yi /DISCARD/ ile atmaya CALISMAYIN. Makefile
     *  zaten -nostdlib kullandigi icin o arsivler hic baglanmaz; linker
     *  script'te adlarini anmak ld'yi onlari DISKTE ARAMAYA zorlar ve
     *  "cannot find libc.a" hatasi verir.
     * --------------------------------------------------------------------- */
    .ARM.attributes 0 : { *(.ARM.attributes) }
}
