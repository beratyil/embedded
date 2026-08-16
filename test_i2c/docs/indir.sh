#!/usr/bin/env bash
# =============================================================================
#  indir.sh  --  Projede kullanilan ST belgelerini indirir
# =============================================================================
#  Kullanim:   bash docs/indir.sh
#
#  PDF'ler depoya konmadi (toplam ~54 MB ve ST'nin telifli belgeleri), bu
#  yuzden bu script ile dogrudan st.com'dan cekiliyorlar.
#
#  NEDEN wget, NEDEN curl DEGIL?
#      st.com'un /resource/ yolundan PDF cekerken curl takiliyor: HTTP/2'de
#      "stream not closed cleanly", HTTP/1.1'de ise suresiz bekleme. IPv4
#      zorlamak (-4), user-agent degistirmek veya range istegi de cozmuyor.
#      wget ayni URL'leri sorunsuz indiriyor. Sebebi buyuk olasilikla ST'nin
#      CDN'inin TLS/HTTP anlasmasindaki bir ozellik.
# =============================================================================

set -u
cd "$(dirname "$0")" || exit 1

UA="Mozilla/5.0"
BASE="https://www.st.com/resource/en"

indir() {
    local url="$1" ad="$2"

    if [ -f "$ad" ]; then
        echo "ATLA  $ad (zaten var)"
        return 0
    fi

    printf 'INDIR %s ... ' "$ad"
    if wget -q --tries=2 --timeout=90 -U "$UA" -O "$ad" "$url" \
       && file -b "$ad" | grep -q PDF; then
        echo "tamam ($(du -h "$ad" | cut -f1))"
    else
        echo "BASARISIZ"
        rm -f "$ad"
        return 1
    fi
}

echo "ST belgeleri indiriliyor (toplam ~54 MB, biraz surebilir)..."
echo ""

# ---- Datasheet'ler: elektriksel ozellikler, pin alternatif fonksiyon tablolari
indir "$BASE/datasheet/stm32f407vg.pdf" \
      "DS8626_STM32F407VG_datasheet.pdf"
indir "$BASE/datasheet/stm32l476rg.pdf" \
      "DS10198_STM32L476RG_datasheet.pdf"

# ---- Reference manual'lar: register tanimlari (en cok kullanilanlar)
#      RM0090 -> Bolum 27 I2C, Bolum 8 GPIO, Bolum 6 RCC
indir "$BASE/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf" \
      "RM0090_STM32F4_reference_manual.pdf"
#      RM0351 -> Bolum 39 I2C (39.4.9 zamanlama formulleri), Bolum 6 RCC
indir "$BASE/reference_manual/rm0351-stm32l47xxx-stm32l48xxx-stm32l49xxx-and-stm32l4axxx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf" \
      "RM0351_STM32L4_reference_manual.pdf"

# ---- Kart kullanim kilavuzlari: hangi pin nereye bagli
indir "$BASE/user_manual/um1472-discovery-kit-with-stm32f407vg-mcu-stmicroelectronics.pdf" \
      "UM1472_F407_Discovery_kullanim.pdf"
indir "$BASE/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf" \
      "UM1724_Nucleo64_kullanim.pdf"

# ---- Uygulama notu: F4'un cok baytli I2C okuma receteleri
#      Basligi STM32F10xxx der ama I2C birimi F4'teki ile ayni nesildir (v1).
indir "$BASE/application_note/an2824-stm32f10xxx-i2c-optimized-examples-stmicroelectronics.pdf" \
      "AN2824_I2C_master_ornekleri.pdf"

echo ""
echo "Bitti. Icerik:"
ls -lh ./*.pdf 2>/dev/null || echo "  (hicbir dosya inmedi)"
