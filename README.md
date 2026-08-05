# BeerOS

BeerOS, C ve NASM sözdizimli x86 Assembly ile sıfırdan geliştirilen bir hobby işletim sistemi projesidir.

## Adım 1: BIOS bootloader

Bu aşamada disk imajı ham bir iki-sektörlü imajdır; henüz dosya sistemi yoktur.

| Disk sektörü | İçerik |
| --- | --- |
| 1 | `boot/boot.asm` ile üretilen 512 baytlık BIOS boot sector |
| 2 | `kernel/kernel_entry.asm` ile üretilen kernel giriş kodu |

Bootloader, BIOS'un `DL` kaydında verdiği önyükleme sürücüsünü saklar. Ardından BIOS `INT 13h` hizmetiyle ikinci sektörü `0x1000:0x0000` (fiziksel `0x10000`) adresine okur ve o adrese far jump yapar. Kernel giriş mesajı görünüyorsa, boot sector'ın diskten gerçek kernel kodunu yükleyip ona kontrolü devrettiği doğrulanmış olur.

## Adım 2: Kernel entry point

`kernel/kernel_entry.asm`, bootloader'dan `1000:0000` adresinde çalışmaya başlar. Kendi `DS`/`ES` kayıtlarını ayarlar, stack'i `0x9000:0xFFFE` adresine taşır ve BIOS'un verdiği boot sürücüsünü saklar. Bu aşamadaki metin çıktısı, yalnızca giriş kodunun testidir; 3. adımda bunun yerini C ile yazılan VGA sürücüsü alacaktır.

Şimdiki loader bilinçli olarak yalnızca bir kernel sektörü (`512` bayt) okur. Bu nedenle 2. adımdaki ilk entry kodu bu sınırda tutulacak; kernel büyümeye başladığında çok-sektörlü yükleme desteği eklenerek sektör sayısı kernel boyutundan hesaplanacaktır. Kernel giriş kodu yalnızca `DL` içindeki önyükleme sürücüsüne güvenmeli; kendi segment kayıtlarını, stack'ini ve çalışma kipini kendisi kurmalıdır.

## Klasör yapısı

```text
boot/       BIOS boot sector
kernel/     Kernel giriş noktası ve ana kernel kodu (Adım 2)
drivers/    VGA ve cihaz sürücüleri (Adım 3 ve sonrası)
cpu/        GDT, IDT ve kesme altyapısı
memory/     Fiziksel/sanal bellek yönetimi
process/    Process ve scheduler kodu
shell/      Kabuk
include/    Paylaşılan C başlıkları
scripts/    Yerel derleme yardımcıları
build/      Üretilen dosyalar (takip edilmez)
```

## Derleme ve QEMU testi

Bu iki aşamada yalnızca `nasm` ve `qemu-system-i386` gerekir. `i686-elf-gcc`, C ile yazılacak VGA sürücüsünün ekleneceği 3. adımdan itibaren derlemede kullanılacaktır.

Windows'ta Chocolatey ile kurulan NASM ve QEMU bazı kurulumlarda PATH'e shim eklemez. `scripts/build.ps1`, bu durumda `Program Files` altındaki standart NASM/QEMU konumlarını otomatik kullanır.

Windows PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Run
```

GNU Make bulunan bir ortamda (WSL/MSYS2 gibi):

```sh
make run
```

Beklenen QEMU çıktısı:

```text
BeerOS: kernel yukleniyor...
BeerOS kernel: entry point reached.
```
