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
- Kullanıcı süreçleri için kernel PDE’lerini paylaşan özel page directory’ler, scheduler CR3 geçişi ve adres alanı yıkımı eklendi.
- Seri tanılama çıktısı kritik bölümlerde atomikleştirildi; preemption sırasında log satırlarının bölünmesi engellendi.
- Paging API’si kullanıcı bayrağını korunan taban adresin altında reddediyor; bu kural VMM self-test’iyle doğrulanıyor.
- Adres alanı geçişi/yıkımı başarısız olursa cleanup sessizce devam etmiyor; kernel fail-closed panic ile duruyor.
- Smoke test’i eski logları reddediyor ve seri çıktıyı gerçek süreç stdout’undan doğruluyor; boş loglar artık başarılı sayılmıyor.
- Scheduler görevleri için 1–8 arası sınırlı öncelik zaman dilimleri ve zorunlu gönüllü `yield` geçişi eklendi.
- FAT16 mount bozuk BPB/cluster geometrisini, aşırı cluster boyutunu ve LBA toplam taşmasını ayrı hata kodlarıyla fail-closed reddediyor.
- Host FAT testi artık geçersiz sektör boyutu, aşırı cluster boyutu ve LBA taşması fixture’larını da doğruluyor.
- Terminated task’ların kernel stack’leri aktif interrupt stack’i korunarak sonraki scheduler geçişinde geri kazanılıyor.
- Sabit boyutlu, kesme güvenli ve taşma kontrollü kernel IPC mesaj kuyruğu eklendi; FIFO, kapasite ve aşırı uzun mesaj reddi boot self-test’iyle doğrulanıyor. `IPC_SEND`/`IPC_RECEIVE` syscall’leri yalnızca 64 baytlık bounded mesajlar ve doğrulanmış kullanıcı aralıklarıyla sunuluyor.
- GitHub Actions; build, imaj doğrulama, FAT host testi ve QEMU ring-3 smoke testini çalıştırıyor.

## Doğrulama

- `scripts/build.ps1` başarılı.
- `scripts/fat-self-test.ps1` başarılı.
- QEMU smoke testi 16 MiB ve 128 MiB ile başarılı; 23 kritik boot/runtime işaretçisi doğrulanıyor.
- QEMU’da ring-3 syscall çalışması ve kullanıcı page-fault izolasyonu gözlendi.
- Deterministik 4 MiB FAT16 imajıyla QEMU ATA/FAT uçtan uca testi başarılı; mount, kök dizin ve dosya okuması dahil 24 işaretçi doğrulanıyor.
- Değişiklikler `codex/core-hardening` dalında checkpoint commit’leriyle kaydedildi.

## Bilinen sınırlar

ATA IDENTIFY ve QEMU IDE PIO okuması doğrulandı; aygıt-hazırlık yarışında timeout içi polling ve üç denemeli bounded retry kullanılıyor. Disk yazma ve kalıcı dosya sistemi kullanıcıya hâlâ açılmadı. IPC syscall’leri bounded ve non-blocking’dir; kuyruk boş/dolu olduğunda `EAGAIN` döner, süreçlerin bekleme/uyandırma yaşam döngüsü henüz yoktur. Çoklu süreç yaşam döngüsü, gelecekteki syscall’lerin tamamı için ABI doğrulaması, authentication, secure boot, ağ/USB/SMP ve tam VFS hâlâ sonraki aşamalardır.

## Öncelikli sonraki geliştirmeler

1. **Kullanıcı süreçleri (P0):** Çoklu süreç yaşam döngüsünü, copy-on-write/ASLR seçeneklerini ve veri taşıyan her yeni syscall için kullanıcı aralığı doğrulaması/kopyalama katmanını ekle.
2. **Çekirdek yaşam döngüsü (P1):** Scheduler görev durumlarını ve bekleme-uyandırmayı ekle; bounded IPC için bloklama/uyandırma semantiği ve süreç sahipliği ekle, sonlandırılan kernel görevlerinin diğer kaynaklarını geri kazan.
3. **Depolama (P1):** ATA sürücüsünü IRQ/DMA ve gerçek donanım matrisiyle doğrula; yazmayı ancak hata kurtarma, journaling ve FAT bütünlük kontrollerinden sonra aç.
4. **Donanım kapsamı (P2):** PCI BAR ayrıştırma, blok aygıt soyutlaması, USB/HID, ağ ve zamanlayıcı sürücülerini ekle; her biri için QEMU/host fixture testi yaz.
5. **Güvenlik (P2):** imzalı boot zinciri, kimlik doğrulama, ASLR, modül imzalama, SMP kilitleme ve fuzz/property testlerini tasarla.

GitHub’a otomatik push yapılmadı; `origin/main` değiştirilmedi.
