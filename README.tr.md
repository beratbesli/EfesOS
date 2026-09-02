# EfesOS

EfesOS; C ve NASM Assembly ile yazılmış, deneysel bir 32-bit x86 işletim sistemidir. BIOS floppy imajından açılır ve QEMU üzerinde çalışır.

[English README](README.md)

## Durum

EfesOS bir öğrenme projesidir; üretim ortamı işletim sistemi değildir. Temel bir ring-3 demo sınırı, doğrulamalı ELF segment yükleyicisi, salt-okunur kernel sayfaları ve salt-okunur FAT16 yoklaması vardır; ancak kimlik doğrulama, secure boot, imzalı ikili dosyalar ve üretim seviyesinde kalıcı dosya sistemi henüz yoktur. Hassas verilerle ya da bir güvenlik sınırı olarak kullanılmamalıdır.

## Özellikler

- A20 doğrulamalı, yeniden deneyen iki aşamalı BIOS bootloader ve 1.44 MiB floppy imajı
- BIOS E820 bellek haritası aktarımı ve deterministik `.bss` başlangıcı
- 32-bit protected mode, GDT, vektör duyarlı IDT, PIC, PIT ve tamponlanmış donanım klavye girişi
- Koruma sayfalı görev yığınları ve PIT tabanlı bağlam değişimi olan öncelikli kernel-thread scheduler
- Açık `yield` desteğiyle sınırlı öncelik zaman dilimleri
- Shell ve oyunları donanım IRQ'ları dışında çalıştıran ertelenmiş olay döngüsü
- Sınırlı `pci` tanılama komutuyla PCI yapılandırma alanı taraması
- Zaman aşımı ve hata denetimli ATA PIO birincil disk erişimi; disk yokluğu açıkça raporlanır
- Transactional dosya sistemi hazır olana kadar ATA ham yazmaları boot’ta korumalıdır
- Sınırlı 8.3 dizin/dosya okuması yapan salt-okunur FAT16 VFS (`diskls`, `diskcat`); doğrulanmış ELF’i diskten başlatma (`run NAME`)
- BSS sıfırlama, W^X denetimi ve son sayfa izinleriyle sınırlı ELF32 segment yükleyicisi
- Veri taşıyan seri syscall için taşma ve izin kontrolleri yapan sınırlı kullanıcı tamponu doğrulaması
- Hata alan demo süreçlerinin ELF sayfaları ve yığın çerçeveleri scheduler devam etmeden geri kazanılır
- Sekize kadar bounded kullanıcı süreci özel page directory alır; scheduler adres alanlarını CR3 ile değiştirir ve fault sonrası slotları yeniden kullanır
- Kernel’e özel bounded `user_process_spawn` API’si doğrulanmış ELF imajlarını sahiplikli adres alanlarına yükler; stack ve cleanup otomatik yönetilir
- Kullanıcı yığınlarının altında eşlenmemiş guard sayfası bulunur; aşağı yönlü stack taşması komşu eşlemelere ulaşmadan fault üretir
- Yeni kullanıcı süreçleri on altı bounded stack bölgesinden birini kullanır; tek bir sabit kullanıcı stack adresine bağımlılık azaltılır
- 16 mesaj/64 bayt sınırları, generation-PID hedefleme, scheduler uyandırma ve usercopy doğrulaması olan IPC syscall’leri (`IPC_SEND`, `IPC_RECEIVE`, `IPC_SEND_TO`, `IPC_RECEIVE_WAIT`, `EXIT`)
- Slot yeniden kullanımında stale kimlikleri önleyen generation tabanlı `GET_PID` syscall’i
- Protected mode’a geçmeden önce stage-2 CRC-32 kernel bütünlük doğrulaması (bütünlük, kimlik doğrulama değil)
- Kaynak temizlemeli ve scheduler slot yeniden kullanımlı bounded kullanıcı `EXIT` yaşam döngüsü
- 32-bit adres alanını kapsayan E820 tabanlı fiziksel bellek yöneticisi
- Null-page koruması, salt-okunur kernel kod/verisi, dinamik sayfa eşleme ve korumalı kernel heap'i
- Kaydırma destekli VGA metin/grafik çıktısı
- İngilizce (US) ve Türkçe Q klavye modları
- Etkileşimli shell, sınırlı yazılabilir RAM dosya sistemi, scheduler demosu, Snake ve slot oyunları

## Gereksinimler

- NASM
- `i686-elf-gcc`, `i686-elf-ld`, `i686-elf-objcopy` veya LLVM (`clang`, `ld.lld`, `llvm-objcopy`)
- QEMU (`qemu-system-i386`)
- Python 3 (Linux `make` derlemesinde kernel CRC-32 üretimi için)

Windows derleme betiği varsa GNU cross-toolchain'i, yoksa Clang'in `i686-none-elf` hedefini kullanır. Araçları güvenilir bir kaynaktan edin ve kurmadan önce checksum doğrulaması yap. EfesOS derleyici ikililerini indirmez veya depoda barındırmaz.

Tedarik zinciri güvenliği için derleme betiği depo içindeki `tools` dizinini varsayılan olarak taramaz. Doğrulanmış ikilileri bilerek buraya koyduysanız `-AllowLocalTools` ile açıkça izin verin ve önce hash değerlerini kontrol edin.

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

Dosya sistemi değişikliklerinde bağımsız FAT ayrıştırıcı testini çalıştır:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\fat-self-test.ps1
```

Süreç yükleyici değişikliklerinde bağımsız ELF doğrulama testini çalıştır:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\elf-self-test.ps1
```

Boot metadata değişikliklerinde E820 ve VGA font doğrulamasını çalıştır:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\boot-info-self-test.ps1
```

QEMU üzerindeki ATA/FAT okuma yolunun tamamını deterministik test diskiyle sına:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\create-test-disk.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -DiskImage .\build\test-disk.img
```

## Shell komutları

| Komut | Açıklama |
| --- | --- |
| `help`, `clear`, `about`, `mem`, `heap`, `input` | Temel kernel ve kuyruk bilgileri |
| `uptime`, `ps`, `demo`, `pci`, `disk`, `diskls`, `diskcat NAME`, `run NAME`, `counter` | Kernel, PCI, disk, scheduler ve doğrulanmış kullanıcı süreci başlatma |
| `echo`, `history`, `color` | Shell araçları |
| `ls`, `cat README`, `cat MOTD`, `cat EFES`, `write NAME CONTENT`, `rm NAME` | Sınırlı RAM dosya sistemi |
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

- CPU istisnaları ayrıştırılır; ring-3 demo görevinin hatası izole edilip görev sonlandırılır, kurtarılamayan kernel hatası sistemi durdurur.
- Shell girdisi ve komut geçmişi sabit boyutlu, sınırları belirli tamponlar kullanır.
- Klavye IRQ girdisi sınırlı tek-üretici/tek-tüketici kuyruğu kullanır ve düşen girdileri raporlar.
- Küçük bir ring-3 görevinde TSS geçiş yığını, kullanıcı sayfaları, kısıtlı syscall ABI'si ve hata sonlandırması bulunur. Bu tam bir süreç modeli değil, güvenlik sınırı gösterimidir.
- Bildirim yönergeleri için [SECURITY.md](SECURITY.md) dosyasına bak.

## Lisans

EfesOS [MIT Lisansı](LICENSE) ile dağıtılır.
