# EfesOS

EfesOS; C ve NASM Assembly ile yazılmış, deneysel bir 32-bit x86 işletim sistemidir. BIOS floppy imajından açılır ve QEMU üzerinde çalışır.

[English README](README.md)

## Durum

EfesOS bir öğrenme projesidir; üretim ortamı işletim sistemi değildir. Kullanıcı modu izolasyonu, çalıştırma izni denetimi, disk dosya sistemi, kimlik doğrulama, secure boot veya kalıcı depolama içermez. Hassas verilerle ya da bir güvenlik sınırı olarak kullanılmamalıdır.

## Özellikler

- A20 doğrulamalı, yeniden deneyen iki aşamalı BIOS bootloader ve 1.44 MiB floppy imajı
- BIOS E820 bellek haritası aktarımı ve deterministik `.bss` başlangıcı
- 32-bit protected mode, GDT, vektör duyarlı IDT, PIC, PIT ve tamponlanmış donanım klavye girişi
- Koruma sayfalı görev yığınları ve PIT tabanlı bağlam değişimi olan öncelikli kernel-thread scheduler
- Shell ve oyunları donanım IRQ'ları dışında çalıştıran ertelenmiş olay döngüsü
- Sınırlı `pci` tanılama komutuyla PCI yapılandırma alanı taraması
- 32-bit adres alanını kapsayan E820 tabanlı fiziksel bellek yöneticisi
- Null-page koruması, salt-okunur kernel kod/verisi, dinamik sayfa eşleme ve korumalı kernel heap'i
- Kaydırma destekli VGA metin/grafik çıktısı
- İngilizce (US) ve Türkçe Q klavye modları
- Etkileşimli shell, sınırlı yazılabilir RAM dosya sistemi, scheduler demosu, Snake ve slot oyunları

## Gereksinimler

- NASM
- `i686-elf-gcc`, `i686-elf-ld`, `i686-elf-objcopy` veya LLVM (`clang`, `ld.lld`, `llvm-objcopy`)
- QEMU (`qemu-system-i386`)

Windows derleme betiği varsa GNU cross-toolchain'i, yoksa Clang'in `i686-none-elf` hedefini kullanır. Araçları güvenilir bir kaynaktan edin ve kurmadan önce checksum doğrulaması yap. EfesOS derleyici ikililerini indirmez veya depoda barındırmaz.

## Windows'ta derleme ve çalıştırma

Depo kök klasöründe PowerShell açıp şu komutu çalıştır:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Run
```

Bu betik `build\efesos.img` imajını üretir, boyutunu doğrular ve QEMU'yu başlatır. Yalnızca derlemek için `-Run` parametresini kaldır.

Kernel değişikliklerinden sonra başlıksız boot smoke testini çalıştır:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1
```

Test, üretilen imajı QEMU'da açar ve başarılı sayılmadan önce COM1 üzerinde beklenen kernel aşamasını görmeyi zorunlu tutar.

## Shell komutları

| Komut | Açıklama |
| --- | --- |
| `help`, `clear`, `about`, `mem`, `heap`, `input` | Temel kernel ve kuyruk bilgileri |
| `uptime`, `ps`, `demo`, `counter` | Kernel ve scheduler durumu |
| `echo`, `history`, `color` | Shell araçları |
| `ls`, `cat README`, `cat MOTD`, `cat EFES` | RAM dosya sistemi |
| `snake`, `slot` | Mini oyunlar |
| `en`, `tr` | Dil ve klavye düzenini değiştirir |
| `reboot`, `shutdown` | QEMU misafirini denetler |

Snake için `W`, `A`, `S`, `D` tuşlarıyla hareket edilir; çıkış `Q` tuşudur. Slot oyununda Space çevirir, `Q` çıkar.

## Depo yapısı

```text
boot/       BIOS stage-1 ve stage-2 yükleyicileri
cpu/        IDT, PIC, PIT ve kesme stub'ları
drivers/    VGA ve klavye sürücüleri
fs/         Bellek içi dosya sistemi
games/      Snake ve slot oyun mantığı
include/    Paylaşılan başlık dosyaları
kernel/     Giriş noktası, GDT, splash ve linker betiği
memory/     Fiziksel/sanal bellek yöneticileri ve korumalı kernel heap'i
process/    Scheduler ve demo görevleri
scripts/    Windows derleme ve QEMU smoke-test yardımcıları
shell/      Komut satırı shell'i
```

## Güvenlik notları

- CPU istisnaları yakalanır ve misafir sistem durdurulur; işlenmemiş üçlü hataya düşülmez.
- Shell girdisi ve komut geçmişi sabit boyutlu, sınırları belirli tamponlar kullanır.
- Klavye IRQ girdisi sınırlı tek-üretici/tek-tüketici kuyruğu kullanır ve düşen girdileri raporlar.
- Projedeki tüm kod henüz ring 0'da çalışır. Kernel metni ve salt-okunur verisi yazmaya karşı korunur, ancak henüz kullanıcı/kernel güvenlik sınırı yoktur.
- Bildirim yönergeleri için [SECURITY.md](SECURITY.md) dosyasına bak.

## Lisans

EfesOS [MIT Lisansı](LICENSE) ile dağıtılır.
