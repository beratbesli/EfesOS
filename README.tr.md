# EfesOS

EfesOS; C ve NASM Assembly ile yazılmış, deneysel bir 32-bit x86 işletim sistemidir. BIOS floppy imajından açılır ve QEMU üzerinde çalışır.

[English README](README.md)

## Durum

EfesOS bir öğrenme projesidir; üretim ortamı işletim sistemi değildir. Temel bir ring-3 demo sınırı, doğrulamalı ELF segment yükleyicisi, salt-okunur kernel sayfaları ve salt-okunur FAT16 yoklaması vardır; ancak kimlik doğrulama, secure boot, imzalı ikili dosyalar ve üretim seviyesinde kalıcı dosya sistemi henüz yoktur. Hassas verilerle ya da bir güvenlik sınırı olarak kullanılmamalıdır.

## Özellikler

- A20 doğrulamalı, yeniden deneyen iki aşamalı BIOS bootloader ve 1.44 MiB floppy imajı
- BIOS E820 bellek haritası aktarımı, deterministik `.bss` başlangıcı, sıkı metadata doğrulaması ve reserved-over-usable çakışma normalizasyonu
- Erken CPUID yetenek yoklaması PAE/NX/TSC desteğini raporlar; destek varsa PAE sayfalama ve donanımsal NX etkinleşir, yoksa legacy fallback kullanılır
- 32-bit protected mode, GDT, vektör duyarlı IDT, PIC, PIT ve tamponlanmış donanım klavye girişi
- UIP/biçim/takvim doğrulamalı kararlı CMOS RTC duvar saati okuması ve `date` shell komutu
- Checksum doğrulamalı, sınırlandırılmış ACPI RSDP/RSDT/XSDT keşfi ve korumalı HPET tablo ayrıştırması
- Önbelleksiz MMIO, nanosaniye dönüşümü, 32-bit sayaç sarım bakımı ve otomatik PIT geri dönüşü olan doğrulanmış HPET monoton saati
- Koruma sayfalı görev yığınları ve PIT tabanlı bağlam değişimi olan öncelikli kernel-thread scheduler
- Açık `yield` desteğiyle sınırlı öncelik zaman dilimleri
- Shell ve oyunları donanım IRQ'ları dışında çalıştıran ertelenmiş olay döngüsü
- Salt-okunur type-0 BAR ayrıştırması ve sınırlı `pci` tanılama komutuyla PCI yapılandırma alanı taraması
- PCI BAR kayıtları sürücülere açılmadan önce çekirdek içinde tür/hizalama self-test’inden geçer
- IRQ14 tamamlanması, doğrulanmış 4 KiB bounce-buffer parçalarıyla bus-master DMA okumaları, otomatik PIO fallback, seri hale getirilmiş istekler ve açık disk-yokluğu tanısı olan zaman aşımı kontrollü ATA primary-master erişimi
- Sürücüden bağımsız 512 baytlık blok aygıt katmanı kapasiteyi, transfer sınırını ve isteğe bağlı yazma yeteneğini çağrıdan önce doğrular; VFS’ye salt-okunur ATA görünümü verilir
- ATA ham yazmaları varsayılan olarak boot’ta korumalıdır; yalnızca doğrulanmış journal penceresi transaction için açılabilir
- Sınırlı 8.3 kök/alt-dizin dosya okuması yapan salt-okunur FAT16 VFS (`diskls`, `diskcat NAME`, `diskcat DIR/NAME`); doğrulanmış ELF’i diskten başlatma (`run NAME`)
- FAT volume dışında doğrulanmış journal bölgesi varsa shell `write`/`rm` işlemleri kalıcı RAMFS journal’ına transaction olarak yazılır; `pformat` yalnızca tamamen boş journal tail’ini açıkça biçimlendirir
- BSS sıfırlama, W^X denetimi ve son sayfa izinleriyle sınırlı ELF32 segment yükleyicisi
- ELF executable sayfaları için yazılım execute biti ve scheduler/syscall sınırlarında EIP doğrulaması (non-PAE donanım NX’in yerine tamamen geçmez)
- CPU RDRAND varsa tohuma karıştırılan, yoksa scheduler/PIT fallback kullanan sınırlı per-boot user-stack ve ET_DYN yerleşim çeşitlendirmesi (tam ASLR değildir)
- Relocation içermeyen konumdan bağımsız `ET_DYN` imajları bounded rastgele yükleme tabanı alır; eski `ET_EXEC` imajları sabit kalır
- Veri taşıyan seri syscall için taşma ve izin kontrolleri yapan sınırlı kullanıcı tamponu doğrulaması
- Hata alan demo süreçlerinin ELF sayfaları ve yığın çerçeveleri scheduler devam etmeden geri kazanılır
- Sekize kadar bounded kullanıcı süreci özel page directory alır; scheduler adres alanlarını CR3 ile değiştirir ve fault sonrası slotları yeniden kullanır
- Kernel’e özel bounded `user_process_spawn` API’si doğrulanmış ELF imajlarını sahiplikli adres alanlarına yükler; stack ve cleanup otomatik yönetilir
- Kullanıcı yığınlarının altında eşlenmemiş guard sayfası bulunur; aşağı yönlü stack taşması komşu eşlemelere ulaşmadan fault üretir
- Yeni kullanıcı süreçleri on altı bounded stack bölgesinden birini kullanır; tek bir sabit kullanıcı stack adresine bağımlılık azaltılır
- 16 mesaj/64 bayt sınırları, generation-PID hedefleme, scheduler uyandırma ve usercopy doğrulaması olan IPC syscall’leri (`IPC_SEND`, `IPC_RECEIVE`, `IPC_SEND_TO`, `IPC_RECEIVE_WAIT`, `EXIT`)
- Slot yeniden kullanımında stale kimlikleri önleyen generation tabanlı `GET_PID` syscall’i
- Protected mode’a geçmeden önce stage-2 SHA-256 kernel bütünlük doğrulaması (bütünlük, kimlik doğrulama değil)
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
- Python 3 (Linux `make` derlemesinde kernel SHA-256 kelimelerini üretmek için)

Windows derleme betiği varsa GNU cross-toolchain'i, yoksa Clang'in `i686-none-elf` hedefini kullanır. Araçları güvenilir bir kaynaktan edin ve kurmadan önce checksum doğrulaması yap. EfesOS derleyici ikililerini indirmez veya depoda barındırmaz.

Tedarik zinciri güvenliği için derleme betiği depo içindeki `tools` dizinini varsayılan olarak taramaz. Doğrulanmış ikilileri bilerek buraya koyduysanız `-AllowLocalTools` ile açıkça izin verin ve önce hash değerlerini kontrol edin.

Host bütünlük regresyonunu (tek bayt bozulma negatif testi dahil) derlemeden sonra `make sha256-self-test` komutuyla çalıştırabilirsin. `make sha256-boot-negative-test` ayrıca bozuk imajı boot edip stage-2’nin kernel girişinden önce reddettiğini kontrol eder.

Yayın imajını dağıtmadan önce güvenilir bir dış RSA-3072 anahtarıyla imzalayıp doğrula:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\sign-release.ps1 -PrivateKeyPath .\release-key.pem -SignaturePath .\build\efesos.img.sig
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\verify-release-signature.ps1 -PublicKeyPath .\release-key.pub.pem -SignaturePath .\build\efesos.img.sig
```

Bu yalnızca public key dışarıdan güvenilir biçimde dağıtılmışsa yayın artefaktını doğrular; BIOS floppy loader imzayı runtime’da zorunlu tutmaz.

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

Depolama sürücüsü değişikliklerinde bağımsız blok aygıt sınır/yetenek testini çalıştır:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\block-device-self-test.ps1
```

ATA veya PIC değişikliklerinden sonra ATA IRQ tamamlanma durum-makinesi testini çalıştır:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ata-irq-self-test.ps1
```

ATA DMA denetleyici, PRDT, aktarım modu ve tamamlanma sözleşmesini sınamak için:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ata-dma-self-test.ps1
```

RTC BCD/binary, 12/24 saat ve Gregoryen takvim dönüşümünü sınamak için:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\rtc-self-test.ps1
```

Sınırlandırılmış ACPI tablo ayrıştırıcısını ve HPET zaman dönüşümünü sınamak için:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\acpi-self-test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\hpet-self-test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -RequireHpet
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -DisableAcpi
```

HPET profili canlı MMIO sayacını; ACPI kapalı profil ise mevcut PIT yolunun önyüklenebilir kaldığını doğrular.

Boot veya parser sınırı değişikliklerinden sonra deterministik boot metadata, E820, ELF ve FAT property-fuzz paketini çalıştır:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\parser-fuzz-self-test.ps1
```

Dosya sistemi değişikliklerinde bağımsız FAT ayrıştırıcı testini çalıştır:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\fat-self-test.ps1
```

RAM dosya sistemi değişikliklerinde bounded isim ve içerik testini çalıştır:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ramfs-self-test.ps1
```

Journal kayıt formatı değişikliklerinde CRC/commit doğrulama testini çalıştır:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\journal-self-test.ps1
```

Kalıcı RAMFS append, yeniden başlatma replay’i ve idempotent silmeyi bellek-içi ATA backend’iyle sınamak için:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\persistent-self-test.ps1
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

Bu fixture ayrıca FAT alanının dışındaki journal bölgesinden bir kayıt replay eder; yazma yolu yine korumalıdır.

Etkileşimli shell → ring-3 disk ELF yolunu da sınamak için:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-self-test.ps1
```

QEMU test diskinde gerçek journal-window yazmasını da denemek için:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-self-test.ps1 -TestPersistentWrite
```

Boş journal tail’inde açık biçimlendirme yolunu da sınamak için:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-self-test.ps1 -TestPersistentFormat
```

Kasten boş ve FAT ile çakışmayan disk tail’inde shell’e `pformat` yazarak
kalıcı RAMFS’i başlatabilirsin. Dolu/zaten biçimli bölgeler veya yerleşik
varsayılanların dışında dosya içeren RAMFS biçimlendirmeyi reddeder.

Ardışık iki imaj derlemesinin byte düzeyinde aynı olduğunu doğrulamak için:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\reproducible-build-self-test.ps1
```

## Shell komutları

| Komut | Açıklama |
| --- | --- |
| `help`, `clear`, `about`, `mem`, `heap`, `input` | Temel kernel ve kuyruk bilgileri |
| `uptime`, `date`, `ps`, `demo`, `pci`, `disk`, `diskls`, `diskcat NAME`, `run NAME`, `counter` | Saat, kernel, PCI, disk, scheduler ve doğrulanmış kullanıcı süreci başlatma |
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
drivers/    VGA, klavye, RTC, ACPI/HPET, PCI, ATA ve genel blok aygıt sürücüleri
fs/         Bellek içi dosya sistemi
games/      Snake ve slot oyun mantığı
include/    Paylaşılan başlık dosyaları
kernel/     Giriş noktası, GDT, splash ve linker betiği
memory/     Fiziksel/sanal bellek yöneticileri ve korumalı kernel heap'i
process/    Scheduler ve demo görevleri
scripts/    Windows derleme, QEMU smoke-test ve disk ELF başlatma yardımcıları
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
