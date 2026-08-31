# EfesOS Geliştirme Raporu

## Tamamlananlar

- BIOS boot zinciri, A20 doğrulaması, E820 bellek haritası, deterministik `.bss` ve seri tanılama sertleştirildi.
- E820 tabanlı PMM, null-page koruması, salt-okunur kernel sayfaları, dinamik VMM ve canary/guard-page heap eklendi.
- Vektör duyarlı IDT/PIC/PIT, kuyruklu klavye sürücüsü ve IRQ dışında çalışan olay döngüsü eklendi.
- Koruma sayfalı preemptive kernel-thread scheduler ve TSS tabanlı gerçek ring-3 geçişi eklendi.
- `int 0x80` syscall ABI’si, geçersiz çağrı reddi ve ring-3 istisna izolasyonu eklendi.
- PCI taraması, zaman aşımı kontrollü ve aygıt-hazırlık yeniden denemeli ATA PIO arayüzü ile MBR FAT16/VFS parser’ı eklendi.
- RAMFS için sınırlı `write`/`rm` işlemleri eklendi; yol ayraçları ve taşan girdiler reddediliyor.
- ELF32 başlık/segment doğrulaması ve gerçek kullanıcı sayfası yüklemesi W^X, adres aralığı, taşma, BSS sıfırlama ve son izin kontrolleriyle eklendi.
- Veri taşıyan `write` syscall’i için kullanıcı sayfa/izin/taşma doğrulaması, bounded kernel kopyası ve geçersiz pointer reddi eklendi.
- Kullanıcı page fault sonrası ELF ve kullanıcı yığını kaynaklarının scheduler’a dönmeden geri kazanılması eklendi.
- GitHub Actions; build, imaj doğrulama, FAT host testi ve QEMU ring-3 smoke testini çalıştırıyor.

## Doğrulama

- `scripts/build.ps1` başarılı.
- `scripts/fat-self-test.ps1` başarılı.
- QEMU smoke testi 16 MiB ve 128 MiB ile başarılı; 17 kritik boot/runtime işaretçisi doğrulanıyor.
- QEMU’da ring-3 syscall çalışması ve kullanıcı page-fault izolasyonu gözlendi.
- Deterministik 4 MiB FAT16 imajıyla QEMU ATA/FAT uçtan uca testi başarılı; mount, kök dizin ve dosya okuması dahil 18 işaretçi doğrulanıyor.
- Değişiklikler `codex/core-hardening` dalında checkpoint commit’leriyle kaydedildi.

## Bilinen sınırlar

ATA IDENTIFY ve QEMU IDE PIO okuması doğrulandı; aygıt-hazırlık yarışında timeout içi polling ve üç denemeli bounded retry kullanılıyor. Disk yazma ve kalıcı dosya sistemi kullanıcıya hâlâ açılmadı. Per-process adres alanı, gelecekteki syscall’lerin tamamı için ABI doğrulaması, authentication, secure boot, ağ/USB/SMP ve tam VFS hâlâ sonraki aşamalardır.

## Öncelikli sonraki geliştirmeler

1. **Kullanıcı süreçleri (P0):** Per-process page directory/address-space geçişini, süreç kapanışında tüm sayfa çerçevelerinin geri kazanımını ve veri taşıyan her syscall için kullanıcı aralığı doğrulaması/kopyalama katmanını ekle.
2. **Çekirdek yaşam döngüsü (P1):** Scheduler görev durumlarını, öncelik/zaman dilimini, bekleme-uyandırma ve IPC kuyruklarını tanımla; sonlandırılan görevlerin yığın ve sayfa çerçevelerini geri kazan.
3. **Depolama (P1):** ATA sürücüsünü IRQ/DMA ve gerçek donanım matrisiyle doğrula; yazmayı ancak hata kurtarma, journaling ve FAT bütünlük kontrollerinden sonra aç.
4. **Donanım kapsamı (P2):** PCI BAR ayrıştırma, blok aygıt soyutlaması, USB/HID, ağ ve zamanlayıcı sürücülerini ekle; her biri için QEMU/host fixture testi yaz.
5. **Güvenlik (P2):** imzalı boot zinciri, kimlik doğrulama, ASLR, modül imzalama, SMP kilitleme ve fuzz/property testlerini tasarla.

GitHub’a otomatik push yapılmadı; `origin/main` değiştirilmedi.
