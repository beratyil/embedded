#!/usr/bin/env python3
# =============================================================================
#  jpeg_al.py  --  Nucleo'nun UART'a doktugu ham JPEG akisini dosyaya cevirir
# =============================================================================
#
#  KULLANIM
#      make GORUNTU_AKISI=1 flash        # once yazilimi akis modunda yukleyin
#      make yakala                       # ya da dogrudan:
#      python3 tools/jpeg_al.py --port /dev/serial/by-id/... --baud 115200
#
#  DIS BAGIMLILIK YOK. pyserial gerekmez; seri portu Python'un standart
#  kutuphanesindeki termios ile dogrudan ayarliyoruz. Bu, deponun geri
#  kalanindaki "kutuphanesiz" yaklasimla ayni ruhta ve kurulum derdini
#  tamamen ortadan kaldiriyor.
#
#  CERCEVE BICIMI (main.c icindeki GORUNTU_AKISI blogu ile ayni)
#      #IMG:<kamera>:<uzunluk>\r\n
#      <tam olarak 'uzunluk' kadar ham bayt>
#      \r\n#SON\r\n
#
#  Uzunluk basligin icinde verildigi icin ham veride ayirici aramak
#  gerekmiyor -- JPEG icinde her bayt degeri gecebilecegi icin bu onemli.
#  Baslik disindaki her sey ekrana metin olarak yazilir (teshis raporlari).
#
#  DIALOUT UYARISI
#      /dev/ttyACM* aygitlari root:dialout 0660'tir. Izin hatasi alirsaniz:
#          sudo usermod -aG dialout $USER
#      ve oturumu kapatip acin. (st-flash'in calisiyor olmasi yaniltmasin;
#      o libusb ile baska bir dugumu kullanir.)
# =============================================================================

import argparse
import os
import sys
import termios
import time

# termios'un tanidigi baud sabitleri. Listede olmayan bir hiz isterseniz
# ekleyin -- rastgele sayi verilemez, cekirdek sabit bir kume kabul eder.
BAUD_SABIT = {
    9600:   termios.B9600,
    19200:  termios.B19200,
    38400:  termios.B38400,
    57600:  termios.B57600,
    115200: termios.B115200,
    230400: termios.B230400,
    460800: termios.B460800,
    921600: termios.B921600,
}

BASLIK = b"#IMG:"


def port_ac(yol, baud):
    """Seri portu HAM (raw) kipte acar.

    Ham kip sart: varsayilan kanonik kipte cekirdek satir sonlarini
    yorumlar, 0x0D/0x0A cevirir ve 0x1A gibi baytlari ozel sayar. JPEG
    verisi icinde bu baytlarin hepsi gecer; ham kipe almazsak goruntu
    sessizce bozulur.
    """
    if baud not in BAUD_SABIT:
        sys.exit("Desteklenmeyen baud: %d  (secenekler: %s)"
                 % (baud, ", ".join(str(b) for b in sorted(BAUD_SABIT))))

    fd = os.open(yol, os.O_RDWR | os.O_NOCTTY)

    ayar = termios.tcgetattr(fd)
    ayar[0] = 0                                             # iflag: ham
    ayar[1] = 0                                             # oflag: ham
    ayar[2] = termios.CS8 | termios.CREAD | termios.CLOCAL  # cflag: 8N1
    ayar[3] = 0                                             # lflag: echo yok
    ayar[4] = BAUD_SABIT[baud]                              # giris hizi
    ayar[5] = BAUD_SABIT[baud]                              # cikis hizi
    ayar[6][termios.VMIN] = 1                               # en az 1 bayt bekle
    ayar[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, ayar)

    return fd


class Akis:
    """fd uzerinden tamponlu okuma. os.read kisa donebilir, bu yuzden
    'tam n bayt' ve 'satir sonuna kadar' islemlerini burada topluyoruz."""

    def __init__(self, fd):
        self.fd = fd
        self.tampon = bytearray()

    def _doldur(self):
        parca = os.read(self.fd, 4096)
        if not parca:
            raise EOFError("port kapandi")
        self.tampon.extend(parca)

    def bayt_al(self, n):
        while len(self.tampon) < n:
            self._doldur()
        veri = bytes(self.tampon[:n])
        del self.tampon[:n]
        return veri

    def satir_al(self):
        """\n gorene kadar okur ve satiri (sonlandirici olmadan) dondurur."""
        while True:
            k = self.tampon.find(b"\n")
            if k >= 0:
                satir = bytes(self.tampon[:k])
                del self.tampon[:k + 1]
                return satir.rstrip(b"\r")
            self._doldur()

    def basliga_kadar(self):
        """BASLIK'i bulana kadar okur; yolda gecen metni ekrana basar."""
        while True:
            k = self.tampon.find(BASLIK)
            if k >= 0:
                onceki = bytes(self.tampon[:k])
                del self.tampon[:k + len(BASLIK)]
                return onceki
            # Baslik tampon sinirina bolunmus olabilir; son birkac bayti
            # elde tutup gerisini metin olarak akitiyoruz.
            if len(self.tampon) > len(BASLIK):
                kes = len(self.tampon) - len(BASLIK)
                onceki = bytes(self.tampon[:kes])
                del self.tampon[:kes]
                yaz_metin(onceki)
            self._doldur()


def yaz_metin(ham):
    if ham:
        sys.stdout.write(ham.decode("ascii", "replace"))
        sys.stdout.flush()


def main():
    ap = argparse.ArgumentParser(
        description="ArduCAM JPEG akisini seri porttan alip .jpg olarak kaydeder")
    ap.add_argument("--port", required=True, help="seri port yolu")
    ap.add_argument("--baud", type=int, default=115200, help="baud (varsayilan 115200)")
    ap.add_argument("--klasor", default="goruntuler", help="kayit klasoru")
    ap.add_argument("--adet", type=int, default=0,
                    help="kac goruntu alinca cikilsin (0 = sinirsiz)")
    arg = ap.parse_args()

    os.makedirs(arg.klasor, exist_ok=True)

    fd = port_ac(arg.port, arg.baud)
    akis = Akis(fd)

    print("Dinleniyor: %s @ %d  ->  %s/" % (arg.port, arg.baud, arg.klasor))
    print("Cikmak icin Ctrl-C\n")

    sayac = 0
    try:
        while True:
            # Baslik oncesindeki teshis metnini ekrana bas.
            yaz_metin(akis.basliga_kadar())

            # "<kamera>:<uzunluk>" satiri
            try:
                alanlar = akis.satir_al().decode("ascii").split(":")
                kamera = int(alanlar[0])
                uzunluk = int(alanlar[1])
            except (ValueError, IndexError):
                print("\n[!] bozuk baslik, atlaniyor")
                continue

            if not (0 < uzunluk < 0x80000):
                print("\n[!] mantiksiz uzunluk: %d, atlaniyor" % uzunluk)
                continue

            t0 = time.time()
            veri = akis.bayt_al(uzunluk)
            sure = time.time() - t0

            # ArduChip'in bildirdigi FIFO uzunlugu, JPEG'in bitis isaretinden
            # (EOI = FF D9) SONRA dolgu baytlari icerir; uzunluk blok boyutuna
            # yuvarlanir. Olculen ornek: bildirilen 3080, gercek JPEG 2282.
            #
            # Bu yuzden dosyayi EOI'de KESIYORUZ. Kesmezsek gecerli bir
            # goruntunun sonuna cop eklenir; cogu goruntuleyici bunu yine de
            # acar ama dosya teknik olarak bozuk olur.
            #
            # FF D9'u ararken SOI'yi atliyoruz. JPEG icinde ham FF baytlari
            # 0x00 ile doldurulur (byte stuffing), dolayisiyla veri icinde
            # yanlislikla FF D9 gorme riski yoktur.
            kes = veri.find(b"\xff\xd9", 2)
            jpeg = veri[:kes + 2] if kes >= 0 else veri

            sayac += 1
            ad = os.path.join(arg.klasor, "kam%d_%04d.jpg" % (kamera, sayac))
            with open(ad, "wb") as f:
                f.write(jpeg)

            # Bozuk kareyi de KAYDEDIYORUZ: yarim gelmis bir goruntu, UART
            # hizinin yetmedigini gosteren degerli bir ipucudur.
            if veri[:2] != b"\xff\xd8":
                durum = "BOZUK (SOI yok)"
            elif kes < 0:
                durum = "BOZUK (EOI yok -- kare yarim)"
            else:
                durum = "TAMAM"

            print("%s  %6d bayt (FIFO %d, dolgu %d)  %5.2f s  %s"
                  % (ad, len(jpeg), uzunluk, uzunluk - len(jpeg), sure, durum))

            if arg.adet and sayac >= arg.adet:
                break

    except KeyboardInterrupt:
        print("\nDurduruldu.")
    except EOFError as e:
        print("\nBaglanti bitti: %s" % e)
    finally:
        os.close(fd)

    print("Toplam %d goruntu kaydedildi." % sayac)


if __name__ == "__main__":
    main()
