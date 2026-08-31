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
- Terminated görev slotları artık kernel çalışma sırasında güvenli biçimde yeniden kullanılabiliyor; kullanıcı demo süreci fault sonrası dört ardışık kez yeniden başlatılarak adres alanı/yığın temizliği, tekrar tahsis ve fiziksel bellek muhasebesi stres altında doğrulanıyor.
- Kullanıcı süreçleri için kernel PDE’lerini paylaşan özel page directory’ler, scheduler CR3 geçişi ve adres alanı yıkımı eklendi.
- İki bounded ring-3 süreç aynı anda ayrı adres alanlarında başlatılıyor; fault temizliği task kimliğiyle eşleşiyor ve QEMU isolation marker’ı ile doğrulanıyor.
- Seri tanılama çıktısı kritik bölümlerde atomikleştirildi; preemption sırasında log satırlarının bölünmesi engellendi.
- Paging API’si kullanıcı bayrağını korunan taban adresin altında reddediyor; bu kural VMM self-test’iyle doğrulanıyor.
- Paging map API’si artık tanımsız izin flag’lerini sessizce kırpmıyor; bilinmeyen bitler reddediliyor ve VMM negatif self-test’iyle korunuyor.
- Adres alanı geçişi/yıkımı başarısız olursa cleanup sessizce devam etmiyor; kernel fail-closed panic ile duruyor.
- Kullanıcı ELF veya stack unmap cleanup’ı başarısız olursa artık fiziksel kaynak kaybını gizlemeden fail-closed panic uygulanıyor.
- Scheduler, kernel page directory’sini ring-3 task’a vermeyi reddediyor; sahiplik kaydı bulunmayan bir user fault da sessizce devam etmek yerine fail-closed panic ile duruyor.
- Scheduler slotları generation tabanlı PID üretiyor; ring-3 `GET_PID` ABI’si doğrulanarak ileride stale task-index sahiplik saldırılarını önleyecek kimlik temeli oluşturuluyor.
- Smoke test’i eski logları reddediyor ve seri çıktıyı gerçek süreç stdout’undan doğruluyor; boş loglar artık başarılı sayılmıyor.
- Scheduler görevleri için 1–8 arası sınırlı öncelik zaman dilimleri ve zorunlu gönüllü `yield` geçişi eklendi.
- FAT16 mount bozuk BPB/cluster geometrisini, aşırı cluster boyutunu ve LBA toplam taşmasını ayrı hata kodlarıyla fail-closed reddediyor.
- Host FAT testi artık geçersiz sektör boyutu, aşırı cluster boyutu ve LBA taşması fixture’larını da doğruluyor.
- FAT kök dizini taraması artık BPB’de belirtilen gerçek entry sayısının dışına çıkmıyor; son sektör artıkları dosya gibi kabul edilmiyor ve host fixture ile doğrulanıyor.
- FAT dosya zinciri cycle testi strict bounded guard ile doğrulanıyor; döngülü cluster zinciri dosya okumasını fail-closed durduruyor.
- Terminated task’ların kernel stack’leri aktif interrupt stack’i korunarak sonraki scheduler geçişinde geri kazanılıyor.
- Scheduler için güvenli bloklama/uyandırma primitive’leri eklendi; mevcut görev bloklanırken başka runnable görev yoksa bloklama reddediliyor ve yaşam döngüsü self-test’i boot sırasında çalışıyor.
- ATA kapasitesi 28-bit PIO adresleme sınırına bağlandı; daha büyük veya taşmış IDENTIFY kapasitesi aygıtı sessiz LBA sarmalaması yerine güvenli biçimde reddediyor.
- VFS, FAT BPB volume geometrisini gerçek ATA sektör kapasitesiyle mount sırasında karşılaştırıyor; fiziksel aygıtı aşan volume artık sonradan hata vermek yerine fail-closed reddediliyor.
- Sabit boyutlu, kesme güvenli ve taşma kontrollü kernel IPC mesaj kuyruğu eklendi; FIFO, kapasite ve aşırı uzun mesaj reddi boot self-test’iyle doğrulanıyor. `IPC_SEND`/`IPC_RECEIVE` syscall’leri yalnızca 64 baytlık bounded mesajlar ve doğrulanmış kullanıcı aralıklarıyla sunuluyor.
- Ring-3 IPC negatif testi, supervisor adresine gönderimi `EFAULT` ile reddediyor; bu reddetme ayrı QEMU smoke marker’ı ve CI kontrolüyle izleniyor.
- GitHub Actions; build, imaj doğrulama, FAT host testi ve QEMU ring-3 smoke testini çalıştırıyor.

## Doğrulama

- `scripts/build.ps1` başarılı.
- `scripts/fat-self-test.ps1` başarılı.
- QEMU smoke testi 16 MiB ve 128 MiB ile başarılı; 30 kritik boot/runtime işaretçisi doğrulanıyor.
- QEMU’da ring-3 syscall çalışması ve kullanıcı page-fault izolasyonu gözlendi.
- Deterministik 4 MiB FAT16 imajıyla QEMU ATA/FAT uçtan uca testi başarılı; mount, kök dizin ve dosya okuması dahil 31 işaretçi doğrulanıyor.
- Değişiklikler `codex/core-hardening` dalında checkpoint commit’leriyle kaydedildi.

## Bilinen sınırlar

ATA IDENTIFY ve QEMU IDE PIO okuması doğrulandı; aygıt-hazırlık yarışında timeout içi polling, üç denemeli bounded retry ve 28-bit kapasite reddi kullanılıyor. Disk yazma ve kalıcı dosya sistemi kullanıcıya hâlâ açılmadı. IPC syscall’leri bounded ve non-blocking’dir; kuyruk boş/dolu olduğunda `EAGAIN` döner. İki süreçlik bounded sahiplik ve restart akışı doğrulandı; genel amaçlı süreç oluşturma/çıkış API’si, daha geniş süreç tablosu ve IPC sahipliği hâlâ sonraki aşamalardır. Authentication, secure boot, ağ/USB/SMP ve tam VFS de sonraki aşamalardır.

## Öncelikli sonraki geliştirmeler

1. **Kullanıcı süreçleri (P0):** Bounded iki süreç sınırını genel süreç oluşturma/çıkış API’sine genişlet; süreç sahipliği, copy-on-write/ASLR seçeneklerini ve veri taşıyan her yeni syscall için kullanıcı aralığı doğrulamasını sürdür.
2. **Çekirdek yaşam döngüsü (P1):** IPC boş/dolu durumlarını scheduler bloklama/uyandırma ile bağla; süreç sahipliği ve sonlandırılan kernel görevlerinin diğer kaynaklarını geri kazan.
3. **Depolama (P1):** ATA sürücüsünü IRQ/DMA ve gerçek donanım matrisiyle doğrula; yazmayı ancak hata kurtarma, journaling ve FAT bütünlük kontrollerinden sonra aç.
4. **Donanım kapsamı (P2):** PCI BAR ayrıştırma, blok aygıt soyutlaması, USB/HID, ağ ve zamanlayıcı sürücülerini ekle; her biri için QEMU/host fixture testi yaz.
5. **Güvenlik (P2):** imzalı boot zinciri, kimlik doğrulama, ASLR, modül imzalama, SMP kilitleme ve fuzz/property testlerini tasarla.

GitHub’a otomatik push yapılmadı; `origin/main` değiştirilmedi.
