# EfesOS Geliştirme Raporu

## Tamamlananlar

- BIOS boot zinciri, A20 doğrulaması, E820 bellek haritası, deterministik `.bss` ve seri tanılama sertleştirildi.
- E820 tabanlı PMM, null-page koruması, salt-okunur kernel sayfaları, dinamik VMM ve canary/guard-page heap eklendi.
- Vektör duyarlı IDT/PIC/PIT, kuyruklu klavye sürücüsü ve IRQ dışında çalışan olay döngüsü eklendi.
- Koruma sayfalı preemptive kernel-thread scheduler ve TSS tabanlı gerçek ring-3 geçişi eklendi.
- `int 0x80` syscall ABI’si, geçersiz çağrı reddi ve ring-3 istisna izolasyonu eklendi.
- PCI taraması, zaman aşımı kontrollü ATA PIO arayüzü ve MBR FAT16/VFS parser’ı eklendi.
- RAMFS için sınırlı `write`/`rm` işlemleri eklendi; yol ayraçları ve taşan girdiler reddediliyor.
- ELF32 başlık/segment doğrulaması W^X, adres aralığı ve taşma kontrolleriyle eklendi.
- GitHub Actions; build, imaj doğrulama, FAT host testi ve QEMU ring-3 smoke testini çalıştırıyor.

## Doğrulama

- `scripts/build.ps1` başarılı.
- `scripts/fat-self-test.ps1` başarılı.
- QEMU smoke testi 16 MiB ve 128 MiB ile başarılı; 14 kritik boot/runtime işaretçisi doğrulanıyor.
- QEMU’da ring-3 syscall çalışması ve kullanıcı page-fault izolasyonu gözlendi.
- Değişiklikler `codex/core-hardening` dalında checkpoint commit’leriyle kaydedildi.

## Bilinen sınırlar

ATA IDENTIFY çalışıyor; ancak QEMU IDE test imajında PIO veri okuması beklenen FAT sektörünü doğrulayamadığı için FAT mount güvenli şekilde reddediliyor. Bu nedenle disk yazma ve kalıcı dosya sistemi kullanıcıya açılmadı. Genel ELF yükleme, data taşıyan syscall pointer doğrulaması, authentication, secure boot, ağ/USB/SMP ve tam VFS hâlâ sonraki aşamalardır.

GitHub’a otomatik push yapılmadı; `origin/main` değiştirilmedi.
