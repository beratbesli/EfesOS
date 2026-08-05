# BeerOS

BeerOS, C ve NASM sözdizimli x86 Assembly ile sıfırdan geliştirilen bir hobby işletim sistemi projesidir.

## Adım 1: BIOS bootloader

Bu aşamada disk imajı ham bir iki-sektörlü imajdır; henüz dosya sistemi yoktur.

| Disk sektörü | İçerik |
| --- | --- |
| 1 | `boot/boot.asm` ile üretilen 512 baytlık BIOS boot sector |
| 2 | `kernel/kernel_entry.asm` ile üretilen kernel giriş kodu |

Bootloader, BIOS'un `DL` kaydında verdiği önyükleme sürücüsünü saklar. Ardından BIOS `INT 13h` hizmetiyle ikinci sektörü `0x1000:0x0000` (fiziksel `0x10000`) adresine okur ve o adrese far jump yapar. Kernel giriş mesajı görünüyorsa, boot sector'ın diskten gerçek kernel kodunu yükleyip ona kontrolü devrettiği doğrulanmış olur.

## Adım 2 ve 4: Kernel entry point, GDT ve protected mode

`kernel/kernel_entry.asm`, bootloader'dan `1000:0000` adresinde çalışmaya başlar, üç girişli GDT'yi yükler ve CPU'yu 32-bit protected mode'a geçirir. Flat data segmentleri ile stack `0x90000` adresine kurulur.

## Adım 3: VGA driver

`drivers/vga.c`, C ile doğrudan `0xB8000` VGA metin belleğine yazar. Kernel, protected mode'a geçtikten sonra ekranı temizler ve bu sürücü üzerinden ilk mesajını görüntüler.

## Adım 5: IDT ve kesme işleme

`cpu/idt.c`, 256 girişli IDT'yi kurar ve `0x30` vektörüne bir interrupt gate yerleştirir. `cpu/interrupts.asm`, register'ları koruyup C handler'a geçen stub'ı içerir. Kernelin çalıştırdığı `int 0x30` testi, handler'ın VGA üzerinden ikinci mesajı yazmasıyla doğrulanır. Donanım IRQ'ları sonraki sürücü adımlarında PIC yapılandırmasıyla açılacaktır.

Build betiği kernel boyutunu hesaplar, ikinci disk sektöründen başlayacak biçimde 1-17 sektör arasına doldurur ve bootloader'ı doğru sektör sayısıyla yeniden derler.

## Klasör yapısı

```text
boot/       BIOS boot sector
kernel/     Kernel giriş noktası, GDT ve linker betiği
drivers/    VGA ve cihaz sürücüleri
cpu/        IDT ve kesme altyapısı
memory/     Fiziksel/sanal bellek yönetimi
process/    Process ve scheduler kodu
shell/      Kabuk
include/    Paylaşılan C başlıkları
scripts/    Yerel derleme yardımcıları
build/      Üretilen dosyalar (takip edilmez)
```

## Derleme ve QEMU testi

Bu aşamada `nasm`, `qemu-system-i386`, `i686-elf-gcc`, `i686-elf-ld` ve `i686-elf-objcopy` gerekir.

Windows'ta Chocolatey ile kurulan NASM ve QEMU bazı kurulumlarda PATH'e shim eklemez. `scripts/build.ps1`, bu durumda `Program Files` altındaki standart NASM/QEMU konumlarını otomatik kullanır.

Windows PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\install-toolchain.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Run
```

GNU Make bulunan bir ortamda (WSL/MSYS2 gibi):

```sh
make run
```

Beklenen QEMU çıktısı:

```text
BeerOS: protected mode ve VGA driver hazir.
BeerOS: IDT ve kesme handler calisiyor.
```
