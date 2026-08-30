# EfesOS

EfesOS; C ve NASM Assembly ile yazılmış, deneysel bir 32-bit x86 işletim sistemidir. BIOS floppy imajından açılır ve QEMU üzerinde çalışır.

[English README](README.md)

## Durum

EfesOS bir öğrenme projesidir; üretim ortamı işletim sistemi değildir. Kullanıcı modu izolasyonu, çalıştırma izni denetimi, disk dosya sistemi, kimlik doğrulama, secure boot veya kalıcı depolama içermez. Hassas verilerle ya da bir güvenlik sınırı olarak kullanılmamalıdır.

## Özellikler

- 1.44 MiB floppy imajı için BIOS stage-1 bootloader
- 32-bit protected mode, GDT, IDT, PIC, PIT ve donanım klavye girişi
- İlk 4 MiB için identity-mapped paging ve fiziksel bellek bitmap yöneticisi
- Kaydırma destekli VGA metin/grafik çıktısı
- İngilizce (US) ve Türkçe Q klavye modları
- Etkileşimli shell, RAM dosya sistemi, scheduler demosu, Snake ve slot oyunları

## Gereksinimler

- NASM
- `i686-elf-gcc`, `i686-elf-ld`, `i686-elf-objcopy`
- QEMU (`qemu-system-i386`)

Araç zincirini güvenilir bir kaynaktan edin ve kurmadan önce checksum doğrulaması yap. EfesOS derleyici ikililerini indirmez veya depoda barındırmaz.

## Windows'ta derleme ve çalıştırma

Depo kök klasöründe PowerShell açıp şu komutu çalıştır:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Run
```

Bu betik `build\efesos.img` imajını üretir, boyutunu doğrular ve QEMU'yu başlatır. Yalnızca derlemek için `-Run` parametresini kaldır.

## Shell komutları

| Komut | Açıklama |
| --- | --- |
| `help`, `clear`, `about`, `mem` | Temel shell bilgileri |
| `uptime`, `ps`, `demo`, `counter` | Kernel ve scheduler durumu |
| `echo`, `history`, `color` | Shell araçları |
| `ls`, `cat README`, `cat MOTD`, `cat EFES` | RAM dosya sistemi |
| `snake`, `slot` | Mini oyunlar |
| `en`, `tr` | Dil ve klavye düzenini değiştirir |
| `reboot`, `shutdown` | QEMU misafirini denetler |

Snake için `W`, `A`, `S`, `D` tuşlarıyla hareket edilir; çıkış `Q` tuşudur. Slot oyununda Space çevirir, `Q` çıkar.

## Depo yapısı

```text
boot/       BIOS boot sector
cpu/        IDT, PIC, PIT ve kesme stub'ları
drivers/    VGA ve klavye sürücüleri
fs/         Bellek içi dosya sistemi
games/      Snake ve slot oyun mantığı
include/    Paylaşılan başlık dosyaları
kernel/     Giriş noktası, GDT, splash ve linker betiği
memory/     Fiziksel bellek yöneticisi ve paging
process/    Scheduler ve demo görevleri
scripts/    Windows derleme yardımcısı
shell/      Komut satırı shell'i
```

## Güvenlik notları

- CPU istisnaları yakalanır ve misafir sistem durdurulur; işlenmemiş üçlü hataya düşülmez.
- Shell girdisi ve komut geçmişi sabit boyutlu, sınırları belirli tamponlar kullanır.
- Projedeki tüm kod ring 0'da çalışır ve kernel belleği yazılabilir olarak eşlenir. Bu bir güvenlik modeli değildir.
- Bildirim yönergeleri için [SECURITY.md](SECURITY.md) dosyasına bak.

## Lisans

EfesOS [MIT Lisansı](LICENSE) ile dağıtılır.
