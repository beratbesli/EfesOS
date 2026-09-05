# EfesOS Proje Devir Teslim Belgesi

Bu belge, projeyi daha önce hiç görmemiş bir geliştiricinin neyin çalıştığını,
hangi güvenlik sınırlarının bulunduğunu, nasıl derleyip test edeceğini ve sırada
ne yapılması gerektiğini anlayabilmesi için hazırlanmıştır.

Son kapsamlı gözden geçirme: **5 Eylül 2026**

Kod checkpoint'i: **`e22ec2c` (`ahci: support bounded multi-port devices`)**

Ana uzak depo: **`https://github.com/beratbesli/EfesOS.git`**

Hedef dal: **`main`**

## 1. Bir cümlede proje

EfesOS, BIOS üzerinden floppy imajından açılan; C ve NASM ile yazılmış,
freestanding 32-bit x86 deneysel bir işletim sistemidir. QEMU üzerinde korumalı
mod, bellek yönetimi, kesmeler, zamanlayıcı, ring-3 süreçler, IPC, salt-okunur
FAT16, sınırlı kalıcı RAMFS günlüğü ve ATA/AHCI disk okuma yollarını gösterir.

## 2. Bugünkü gerçek durum

Proje artık yalnızca ekrana yazı basan bir boot deneyi değildir. Çalışan bir
iki-aşamalı boot zinciri, fiziksel/sanal bellek yönetimi, kesme altyapısı,
preemptive scheduler, ayrı kullanıcı adres alanları, doğrulamalı ELF yükleme,
bounded IPC, dosya sistemi katmanları ve test edilen disk sürücüleri vardır.

Buna rağmen **üretim işletim sistemi değildir**. Kimlik doğrulama, yetki modeli,
runtime secure boot, tam ASLR, ağ, USB, SMP, genel amaçlı yazılabilir dosya
sistemi ve geniş gerçek donanım desteği yoktur. Hassas veri için, internete açık
bir makine için veya güvenlik sınırı olarak kullanılmamalıdır.

Yaklaşık durum değerlendirmesi:

- Eğitim/araştırma çekirdeği ve QEMU demosu olarak: ileri ve kullanılabilir.
- Kendi bileşenleri içinde fail-closed davranış ve test disiplini olarak: güçlü
  bir temel var.
- Günlük masaüstü/sunucu işletim sistemi olarak: henüz çok erken.
- Daha önce belirlenen geniş yol haritasına göre: yaklaşık `%35-%45`.

## 3. Açılış akışı

Sistem aşağıdaki sırayla açılır:

1. BIOS, `boot/boot.asm` içindeki 512 baytlık stage-1'i `0x7C00` adresine yükler.
2. Stage-1, ilk floppy track içinde kalan 12 sektörlük stage-2'yi üç denemeye
   kadar okuyup `0x8000` adresine aktarır.
3. `boot/stage2.asm`; E820 bellek haritasını, ACPI RSDP adresini ve güvenli VGA
   font bilgisini toplar, A20'yi açıp doğrular ve kernel'i `0x10000` adresine
   yükler.
4. Stage-2, build sırasında gömülen SHA-256 ile kernel baytlarını doğrular.
   Uyuşmazlıkta protected mode'a geçmeden durur.
5. `kernel/kernel_entry.asm` GDT/stack/BSS hazırlığını yapar ve
   `kernel_main()` fonksiyonuna geçer.
6. `kernel/kernel.c` boot metadata, PMM, IDT/syscall, paging, ACPI, APIC/HPET,
   ATA/AHCI, VFS, heap, scheduler, RAMFS/journal, IPC ve ring-3 self-test'lerini
   sıralı biçimde başlatır.
7. Kritik testlerden biri başarısız olursa kernel devam etmek yerine panic ile
   durur. Başarılı durumda splash, shell ve ertelenmiş olay döngüsü çalışır.

Kernel linker adresi `0x10000`'dir. `kernel/linker.ld`, kernel sonunun VGA ve
option-ROM bölgesinden önce `0x90000` altında kalmasını zorunlu tutar. Güncel
optimize kernel 209 sektördür; bu sınırı değiştirmek boot bellek yerleşiminin
yeniden tasarlanmasını gerektirir.

## 4. Kaynak dizinleri ve sorumluluklar

| Yol | Sorumluluk |
| --- | --- |
| `boot/` | BIOS stage-1/stage-2, A20, E820, ACPI adresi, kernel SHA-256 kontrolü |
| `cpu/` | CPUID, IDT, PIC/PIT, xAPIC/IOAPIC, interrupt assembly stub'ları |
| `drivers/` | VGA, klavye, seri port, RTC, ACPI/HPET, PCI, ATA, AHCI ve blok aygıtı |
| `memory/` | E820 normalizasyonu, PMM, paging/PAE/NX ve guarded heap |
| `process/` | Scheduler, ELF32 yükleyici, kullanıcı süreçleri ve örnek programlar |
| `kernel/` | Ana başlatma, syscall, IPC, panic, splash ve linker script |
| `fs/` | RAMFS, journal, kalıcı RAMFS, FAT16 parser ve VFS |
| `shell/` | Komut ayrıştırma ve kullanıcıya sunulan kernel shell'i |
| `games/` | Snake ve slot örnekleri |
| `include/` | Katmanlar arası public C sözleşmeleri |
| `tests/` | Host fixture'ları ve QEMU `blkdebug` hata profilleri |
| `scripts/` | Windows build, host testleri, QEMU smoke/recovery testleri, imza araçları |
| `.github/workflows/` | Linux build, analyzer, sanitizer ve QEMU CI kapıları |

## 5. Tamamlanmış ana yetenekler

### Boot ve bütünlük

- Retry'lı iki aşamalı BIOS bootloader ve standart 1.44 MiB floppy imajı.
- A20'nin gerçekten açık olduğunun kontrolü.
- En fazla 32 E820 kaydı; taşma, bozuk metadata ve overlap durumlarında
  reserved-over-usable normalizasyonu.
- Kernel için stage-2 SHA-256 bütünlük kontrolü ve bozuk kernel negatif testi.
- Tam imaj için harici RSA-3072 detached signature üretme/doğrulama araçları.
- Ardışık build'lerin byte düzeyinde aynı olduğunu doğrulayan reproducible build.

### CPU, kesmeler ve zaman

- 32-bit protected mode, GDT, 256 vektörlü IDT ve TSS.
- CPUID ile PAE, NX, TSC, RDRAND, MSR, APIC ve x2APIC yetenek raporu.
- Uygun CPU'da PAE ve donanımsal NX; uygun değilse legacy paging geri dönüşü.
- IRQ0/1/14 için firmware MADT verisiyle doğrulanmış xAPIC/IOAPIC yönlendirmesi.
- APIC kullanılamazsa dual 8259 PIC geri dönüşü.
- CMOS RTC duvar saati, ACPI tablo doğrulaması ve HPET monoton sayacı.
- HPET ile kalibre edilen local APIC scheduler timer; başarısızlıkta PIT geri dönüşü.

### Bellek ve süreç izolasyonu

- E820 tabanlı 4 KiB fiziksel frame yöneticisi ve kullanıcı frame sahipliği.
- Null-page koruması, kernel/user adres ayrımı, W^X ve özel CR3 adres alanları.
- Uygun donanımda NX; legacy modda ek yazılımsal executable-page kontrolü.
- 16 görev slotlu preemptive scheduler, guard sayfalı ve canary'li kernel stack'leri.
- En fazla sekiz kullanıcı süreci, ayrı page directory ve guard sayfalı kullanıcı
  stack'leri.
- Generation tabanlı PID; stale PID ve stale cleanup işlemlerinin reddi.
- Fault veya normal `EXIT` sonrasında ELF, stack ve IPC kaynaklarının geri alınması.
- Bounded `ET_EXEC` ve relocation gerektirmeyen `ET_DYN` ELF32 yükleme.
- Kısmi stack/load adres çeşitlendirmesi; RDRAND varsa seed'e karıştırılır.

### Syscall ve IPC

- `int 0x80` ABI: tick, yield, bounded write, IPC, PID ve exit çağrıları.
- Kullanıcı pointer'larında adres taşması, sayfa izni ve user/supervisor kontrolü.
- 16 mesajlık, mesaj başına 64 baytlık kernel IPC kuyruğu.
- Generation-PID hedefleme, blocking receive, wakeup ve süreç kapanışında kuyruk
  temizliği.

### Dosya sistemleri

- En fazla 16 dosya, 32 bayt isim ve 256 bayt içerik sınırı olan RAMFS.
- Salt-okunur FAT16 mount, MBR partition, 8.3 kök/alt dizin dosya okuma.
- FAT geometry, mirror, cluster-chain ve aygıt kapasitesi doğrulamaları.
- FAT'tan ELF okuyup doğruladıktan sonra ring-3 çalıştıran `run NAME` yolu.
- FAT volume dışında ayrılmış 65 sektörlük journal penceresinde iki fazlı append,
  CRC, commit marker, read-back ve torn-tail recovery.
- Journal yalnız legacy ATA PIO yazma penceresini açar. Genel ATA yazması, FAT
  metadata yazması ve AHCI yazması kapalıdır.

### Depolama sürücüleri

- Sürücüden bağımsız, kapasite/transfer/yazma yeteneği doğrulayan 512 baytlık
  blok aygıt sözleşmesi.
- Legacy primary-master ATA: PIO, IRQ14, uygun PCI IDE'de 4 KiB bounce-buffer
  bus-master DMA okuması ve bounded PIO fallback.
- ATA48 komutları vardır; public blok API 32-bit LBA kullandığı için daha büyük
  kapasite şimdilik fail-closed reddedilir.
- Q35/ICH9 AHCI: PCI sınıfı/BAR5, BIOS handoff, cache-disabled MMIO, MSI vektör
  51 ve polling fallback.
- Kullanılabilir AHCI denetleyicileri arasında bounded başlangıç failover'ı.
- Seçilen denetleyicide en fazla sekiz doğrudan SATA disk kaydı.
- Bir paylaşılan DMA descriptor/FIS/bounce kümesi üzerinde seri istek yürütme.
- Port değişiminde eski port motoru, `PxCI/PxSACT`, bus-master ve CLB/FB adresleri
  güvenli duruma getirilmeden yeni port açılmaz.
- Okuma hatasında bir COMRESET retry, sonra bir HBA reset retry; son hata sürücüyü
  MSI ve bus-master kapalı biçimde blok katmanından kaldırır.
- HBA resetinden sonra her disk controller-generation ile stale sayılır ve yeniden
  seçildiğinde saklanan IDENTIFY bilgisiyle tekrar kimlik doğrulamasından geçer.

## 6. Güvenlik modeli ve değişmezler

Kodda sık kullanılan `fail-closed` yaklaşımı şudur: bozuk/şüpheli girdiyi kabul
etmek veya DMA yetkisini belirsiz bırakmak yerine özellik kapatılır ya da kernel
panic ile durur.

Korunması gereken temel değişmezler:

1. Kullanıcı sayfaları `0x00400000` altına ve `0xC0000000` üstüne eşlenmez.
2. Aynı fiziksel kullanıcı frame'i iki kullanıcı mapping'i tarafından sahiplenilmez.
3. Kernel page table'ları özel kullanıcı CR3'lerinden değiştirilemez.
4. Bir sayfa aynı anda writable ve executable olamaz.
5. Usercopy her kapsanan sayfanın PDE/PTE user ve erişim izinlerini doğrular.
6. Disk istekleri kapasiteyi ve unsigned taşmayı callback'ten önce kontrol eder.
7. FAT parser disk boyutunu aşan geometriyi veya döngülü/uyuşmayan zinciri reddeder.
8. Genel disk yazması kapalıdır; journal penceresi dışına yazılamaz.
9. DMA belleği yalnız donanım motorunun durduğu ve bus-master'ın kapandığı
   read-back ile kanıtlandıktan sonra yeniden kullanılır veya serbest bırakılır.
10. Eski interrupt/PID/controller generation değerleri yeni işlemin başarısı
    olarak kabul edilmez.
11. Kritik cleanup yarım kalırsa sonraki göreve geçilmez.
12. QEMU smoke testinde herhangi bir `KERNEL PANIC:` satırı başarıyı geçersiz kılar.

Bu kurallar değiştirilirken pozitif test kadar mutlaka negatif fixture eklenmelidir.

## 7. Bilinen güvenlik ve ürün sınırları

### P0 - Gerçek kullanıcı/veri öncesinde zorunlu

- **Boot authenticity yok:** Stage-2 SHA-256 kazara/sonradan bozulmayı bulur fakat
  digest de imajın içindedir. Harici RSA imzası araçla doğrulanabilir, BIOS boot
  zinciri imzayı kendisi zorunlu tutmaz. UEFI Secure Boot veya gömülü güven kökü yok.
- **Kimlik doğrulama ve yetkilendirme yok:** Kullanıcı hesabı, parola, UID/GID,
  dosya izni veya capability modeli yoktur. Shell bir kernel bileşenidir.
- **Üretim dosya sistemi yok:** FAT salt-okunur; yazılabilir kalıcılık yalnız küçük,
  özel ATA journal penceresidir. Güç kaybı ve gerçek disk matrisi yeterince sınanmadı.
- **İmzalı kullanıcı programı/modül politikası yok:** ELF yapısal olarak
  doğrulanır fakat yayıncı kimliği doğrulanmaz; yüklenebilir kernel modülü sistemi yok.
- **Gerçek donanım güvence matrisi yok:** Bugünkü kanıt esas olarak QEMU ve host
  fixture'larına dayanır.

### P1 - Sıradaki çekirdek/depolama çalışmaları

- Public blok API'yi 64-bit LBA ve daha geniş transfer sayaçlarına geçirmek.
- AHCI NCQ, çoklu komut slotu, hot-plug, ATAPI ve güvenli write/flush yolu.
- Journal/WAL sözleşmesini AHCI ve genel VFS yazma modeline genişletmek.
- Tam VFS namespace, inode/handle yaşam döngüsü, mount ve permission modeli.
- USB HID ve USB mass-storage.
- Ağ kartı sürücüsü, IP stack ve ağ girdileri için ayrı threat model/fuzzing.

### P2 - Ölçek ve platform

- AP başlatma, SMP scheduler ve tüm ortak yapılarda gerçek kilitleme.
- x2APIC runtime backend; parser x2APIC kaydını tanır fakat CPU'ları başlatmaz.
- Saat ayarlama, timezone, drift düzeltme ve güvenli NTP.
- UEFI/GPT/modern boot, 64-bit mimari ve daha geniş fiziksel cihaz desteği.
- Tam ASLR için kriptografik entropy havuzu ve tüm image/kernel yerleşiminin
  rastgeleleştirilmesi.

### P3 - Kullanılabilirlik

- Kullanıcı alanı libc/runtime, süreç oluşturma/IPC üst API'leri ve paket biçimi.
- Grafik compositor/window sistemi, ses, güç yönetimi ve gelişmiş shell araçları.
- Performans ölçümü, profiling, crash dump ve uzun süreli stress/fault injection.

## 8. Windows'ta derleme ve deneyimleme

Gerekenler:

- NASM
- LLVM (`clang`, `ld.lld`, `llvm-objcopy`) veya i686-elf GNU cross-toolchain
- QEMU (`qemu-system-i386`)
- Python 3

Depo kökünde PowerShell ile yalnız build:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

Build edip grafik QEMU penceresini açmak:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Run
```

Üretilen imaj `build\efesos.img` olur. `build/` Git tarafından izlenmez ve her
zaman yeniden üretilebilir.

Deterministik FAT16 test diskiyle QEMU:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\create-test-disk.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -DiskImage .\build\test-disk.img
```

Q35/AHCI ile:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -SkipBuild -Q35 -RequireHpet -DiskImage .\build\test-disk.img
```

## 9. Test stratejisi

Testler dört katmandır:

1. **Derleme sözleşmesi:** `-Wall -Wextra -Werror`, freestanding target, imaj
   boyutları, boot signature ve linker ASSERT.
2. **Host unit/property testleri:** Parser ve saf durum makineleri Windows/Linux
   uygulaması olarak çalışır; bozuk girdiler deterministik taranır.
3. **Sanitizer/static analyzer:** Linux CI host testlerini ASan/UBSan ile ve kernel
   C kaynaklarını Clang static analyzer ile tarar.
4. **QEMU entegrasyonu:** Gerçek boot, IRQ/MSI/polling, disk, ring-3, hata enjeksiyonu
   ve fail-closed marker'ları COM1 üzerinden doğrulanır.

Hızlı çekirdek kontrolü:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -SkipBuild
```

Parser sınırları:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\parser-fuzz-self-test.ps1
```

AHCI çoklu disk ve fallback:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-multi-device-self-test.ps1 -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-multi-device-self-test.ps1 -SkipBuild -DisableApic
```

AHCI failover ve hata kurtarma:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-controller-failover-self-test.ps1 -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-controller-failover-self-test.ps1 -SkipBuild -DisableApic
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-controller-failover-self-test.ps1 -SkipBuild -AllEmpty
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-recovery-self-test.ps1 -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-recovery-self-test.ps1 -SkipBuild -HbaEscalation
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-recovery-self-test.ps1 -SkipBuild -PersistentFailure
```

Ring-3 disk ELF ve kalıcı journal:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-self-test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-self-test.ps1 -TestPersistentWrite
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-self-test.ps1 -TestPersistentFormat
```

Deterministik imaj:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\reproducible-build-self-test.ps1
```

Alt sistem değiştirildiğinde ilgili `scripts/*-self-test.ps1` betiği de
çalıştırılmalıdır. Ayrıntılı komut listesi `README.md` ve `README.tr.md` içindedir.

## 10. Güncel doğrulama kanıtı

5 Eylül 2026'da çoklu AHCI checkpoint'i için yerelde şunlar başarıyla çalıştı:

- LLVM `i686-none-elf` build: 209 kernel sektörü.
- İki port/iki disk QEMU: MSI modunda dört okuma/dört IRQ.
- İki port/iki disk QEMU: APIC kapalı polling modunda dört okuma/sıfır IRQ.
- İki denetleyici failover: MSI, polling ve tüm adaylar boş profilleri.
- AHCI tek hata COMRESET recovery, çift hata HBA-reset escalation, HPET/PIT ve
  APIC'siz varyantlar.
- Kalıcı AHCI hatasında DMA/MSI iptali ve fail-closed panic.
- Blok/ATA/PCI/AHCI/RTC/ACPI/MADT/HPET/FAT/E820/ELF/RAMFS/journal/persistent
  host testleri.
- Boot metadata property testi: 219.136 deterministik mutasyon.
- Reproducible build SHA-256:
  `AF9DA14ED2C866F4FCCA3611BF5A6328A49B2543D9E11560FB198EC1962481D9`.
- Bozuk kernel'in stage-2 tarafından kernel girişinden önce reddi.
- FAT diskinden `RUN.ELF` yükleme, ring-3 çalıştırma ve temiz çıkış.
- Aynı checkpoint'in [GitHub Actions build, static analyzer, ASan/UBSan ve tüm
  QEMU profilleri](https://github.com/beratbesli/EfesOS/actions/runs/33969982444)
  de başarıyla tamamlandı.

## 11. Git ve GitHub durumu

- Uzak deponun adı EfesOS'tur ve `origin` doğru adrese gider.
- Takip edilen güncel kaynaklarda eski proje adları kalmamıştır. Bu adlar yalnız
  geçmişteki eski commit başlıklarında görülebilir; Git geçmişi değiştirilmedi.
- `e22ec2c` özellik checkpoint'i itibarıyla proje 304 küçük commit'e sahiptir;
  bu devir belgesi ayrı bir dokümantasyon checkpoint'i olarak sonradan eklenmiştir.
- Geliştirme `codex/core-hardening` dalında yapılıp doğrulanmış checkpoint'ler
  `origin/main` dalına gönderilmiştir.
- Eski commitlerde görülen kırmızı `X`, o commit oluşturulduğu anda CI'ın
  başarısız olduğunu gösteren değişmez tarihsel kayıttır. Sonraki düzeltme commit'i
  eski sonucu geriye dönük yeşile çevirmez. Güvenilir durum ölçütü güncel `main`
  HEAD'inin CI sonucudur; eski run'ları silmek veya geçmişi yeniden yazmak doğru
  bir hata düzeltme yöntemi değildir.

Yerel ve uzak kaynakların eşitliğini kontrol et:

```powershell
git fetch origin --prune
git status --short
git rev-list --left-right --count origin/main...HEAD
git diff --exit-code origin/main...HEAD
```

Beklenen sonuç: status boş, rev-list `0 0`, diff boş.

Bir checkpoint'i geri almak gerekirse paylaşılan geçmişi silmek yerine:

```powershell
git revert <commit-sha>
git push origin HEAD:main
```

## 12. Yeni geliştirici için çalışma kuralı

1. Önce `README.tr.md`, bu belge, `UPDATE_REPORT.md`, `SECURITY.md` ve
   `CONTRIBUTING.md` dosyalarını oku.
2. `git fetch`, temiz worktree ve yerel/uzak eşitlik kontrolü yap.
3. Tek bir küçük tehdit/özellik sınırı seç; birden fazla büyük alt sistemi aynı
   commit'te değiştirme.
4. Önce sözleşmeyi ve negatif testini yaz; sonra donanım/çekirdek entegrasyonunu yap.
5. Tüm döngülere kapasite veya timeout üst sınırı koy; integer toplama/çarpma
   öncesi taşmayı kontrol et.
6. Donanım register yazımlarını mümkünse read-back ile doğrula ve kısmi başarısızlık
   için rollback/fail-closed yolu tanımla.
7. DMA veya fiziksel frame serbest bırakmadan önce sahipliğin gerçekten kalktığını
   kanıtla.
8. İlgili host testleri, QEMU pozitif/negatif profilleri ve tam build'i çalıştır.
9. `git diff --check` ve `git diff` ile istemeden gelen dosyaları denetle.
10. Önemli ve doğrulanmış her aşamayı açıklayıcı mesajla commit et.
11. GitHub'a gönderdikten sonra güncel HEAD CI'ı yeşil olmadan aşamayı tamamlanmış
    sayma.
12. Güvenlik iddiasını testin gerçekten kanıtladığından daha geniş yazma.

## 13. Önerilen sıradaki somut iş

En güvenli devam noktası, yeni yazma veya paralellik açmadan önce blok katmanını
64-bit LBA'ya taşımaktır:

1. `block_device` kapasite ve read callback LBA tipini `uint64_t` yap.
2. Toplama/son-sektör kontrollerini taşmasız helper'larda merkezileştir.
3. ATA/AHCI ve VFS adaptörlerini kademeli geçir.
4. `UINT32_MAX` çevresi, aygıt sonu ve aşırı count için host testleri yaz.
5. QEMU küçük disk davranışının hiç değişmediğini doğrula.
6. Ancak bundan sonra NCQ ve yeni AHCI yazma/journaling tasarımına geç.

Bu sıra, mevcut sağlam salt-okunur yolu korurken gelecekteki disk kapasitesi ve
yazma özellikleri için doğru temel oluşturur.
