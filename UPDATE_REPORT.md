# EfesOS Geliştirme Raporu

## Tamamlananlar

- BIOS boot zinciri, A20 doğrulaması, E820 bellek haritası, deterministik `.bss` ve seri tanılama sertleştirildi.
- Erken boot aşamasında CPUID yetenek yoklaması eklendi; PAE, NX ve TSC durumu seri tanıda raporlanıyor. PAE destekleniyorsa sayfalama PAE backend’ine geçiyor; NX destekleniyorsa EFER.NXE ve donanımsal NX etkinleştiriliyor, desteklenmeyen CPU’larda legacy fallback korunuyor.
- ELF yükleyici, dynamic-linker gerektiren imajları reddeden relocation-free `ET_DYN` desteği kazandı; bu imajlara stack ile çakışmayan bounded per-load load bias uygulanıyor. `ET_EXEC` geriye dönük uyum için sabit kalıyor; bu tam ASLR değildir.
- E820 tabanlı PMM, null-page koruması, salt-okunur kernel sayfaları, dinamik VMM ve canary/guard-page heap eklendi. Boot metadata bilinmeyen handoff bayraklarını, sıfır E820 türünü, ayrılmış attribute bitlerini ve canonical olmayan BIOS alanlarını fail-closed reddediyor; usable kayıtlar önce, reserved/ACPI/unknown kayıtlar sonra uygulanarak firmware sırasından bağımsız reserved-over-usable önceliği sağlanıyor.
- Vektör duyarlı IDT/PIC/PIT, kuyruklu klavye sürücüsü ve IRQ dışında çalışan olay döngüsü eklendi.
- Koruma sayfalı preemptive kernel-thread scheduler ve TSS tabanlı gerçek ring-3 geçişi eklendi.
- `int 0x80` syscall ABI’si, geçersiz çağrı reddi ve ring-3 istisna izolasyonu eklendi.
- PCI taraması, zaman aşımı kontrollü ve aygıt-hazırlık yeniden denemeli ATA PIO arayüzü ile MBR FAT16/VFS parser’ı eklendi. IDT sonrasında ATA primary kanal IRQ14 tamamlanmasına geçiyor; kesme gecikir/yoksa bounded polling fallback kullanılıyor ve transferler preemption altında birbirine karışmaması için tek sahipli yürütülüyor.
- 512 bayt sektör ve 128 sektör/istek sınırı olan sürücüden bağımsız blok aygıt katmanı eklendi. Başlatılmamış aygıtlar, null tamponlar, sıfır/taşan transferler ve aygıt sonunu aşan LBA aralıkları sürücü callback’i çağrılmadan reddediliyor; ATA’nın VFS’ye sunduğu görünüm bilerek salt-okunur tutuluyor.
- RAMFS için sınırlı `write`/`rm` işlemleri eklendi; yol ayraçları ve taşan girdiler reddediliyor.
- RAMFS okuma isimleri de aynı bounded isim doğrulamasından geçiyor; VGA metin çıktısı null pointer’ı fail-closed yutuyor.
- ELF32 başlık/segment doğrulaması ve gerçek kullanıcı sayfası yüklemesi W^X, adres aralığı, taşma, BSS sıfırlama ve son izin kontrolleriyle eklendi.
- ELF doğrulaması artık `e_ehsize` alanını ve sanal adresin üst sınırını açıkça denetliyor; yüksek adres unsigned taşması malformed image olarak reddediliyor.
- ELF segment sayfası hesabı artık unsigned kapasite taşmasına karşı açıkça sınırlandırılıyor; 1024 sayfadan büyük BSS istekleri doğrulama aşamasında reddediliyor.
- ELF yükleyici executable sayfaları yazılım execute bitiyle işaretliyor; scheduler ve syscall sınırlarında ring-3 EIP’in executable kullanıcı sayfasında olduğu doğrulanıyor. Bu, non-PAE donanım NX eksikliğini tamamen çözmez ancak veri/yığın sayfasından çalıştırmayı daraltır.
- Paging map/protect API’leri de writable+executable birleşimini reddediyor; W^X artık yalnızca ELF girdisinin iyi niyetine bağlı değil ve VMM self-test’inde negatif fixture ile korunuyor.
- Veri taşıyan `write` syscall’i için kullanıcı sayfa/izin/taşma doğrulaması, bounded kernel kopyası ve geçersiz pointer reddi eklendi.
- Tanımsız syscall çağrıları adsız `0xFFFFFFFF` yerine açık `ENOSYS` kodu döndürüyor; ABI self-test’i bu sözleşmeyi kilitliyor.
- Kullanıcı page fault sonrası ELF ve kullanıcı yığını kaynaklarının scheduler’a dönmeden geri kazanılması eklendi.
- Terminated görev slotları artık kernel çalışma sırasında güvenli biçimde yeniden kullanılabiliyor; kullanıcı demo süreci fault sonrası dört ardışık kez yeniden başlatılarak adres alanı/yığın temizliği, tekrar tahsis ve fiziksel bellek muhasebesi stres altında doğrulanıyor.
- Kullanıcı süreçleri için kernel PDE’lerini paylaşan özel page directory’ler, scheduler CR3 geçişi ve adres alanı yıkımı eklendi.
- Scheduler user-task kayıtları artık aktif başka bir görevin kullandığı CR3’ü yeniden paylaşmayı reddediyor; negatif kernel self-test’i bu sahiplik kuralını doğruluyor.
- Sekize kadar bounded ring-3 süreç ayrı adres alanlarında çalışabiliyor; ilk dört demo süreç fault temizliği ve task generation kimliğiyle QEMU isolation marker’ı üzerinden doğrulanıyor.
- Genel amaçlı kernel-only `user_process_spawn` yolu eklendi; bounded ELF imaj boyutu, otomatik stack bölgesi, address-space sahipliği ve cleanup akışı ortaklaştırıldı.
- Shell’e `run NAME` eklendi; salt-okunur FAT’tan alınan dosya yalnızca bounded ELF doğrulamasından geçerse ring-3 süreci olarak başlatılıyor ve mevcut ownership/cleanup zincirini kullanıyor.
- Deterministik FAT16 fixture artık geçerli bir `RUN.ELF` içeriyor; QEMU monitor ile `run RUN.ELF` komutu gönderilerek FAT okuma → ELF doğrulama/yükleme → ring-3 `WRITE`/`EXIT` zinciri uçtan uca doğrulanıyor.
- Scheduler kullanıcı görev kayıtları kernel CR3 bağlamını ve sayfa hizalı kullanıcı stack üstünü zorunlu kılıyor; hatalı iç API çağrıları fail-closed reddediliyor.
- Scheduler user-task kayıtları artık aday CR3’e IRQ’lar kapalıyken geçip entry sayfasının executable, başlangıç stack sayfasının tamamen writable olduğunu doğruluyor; hatalı bağlam slot tahsis edilmeden reddediliyor ve boot self-test’iyle korunuyor.
- Seri tanılama çıktısı kritik bölümlerde atomikleştirildi; preemption sırasında log satırlarının bölünmesi engellendi.
- Paging API’si kullanıcı bayrağını korunan taban adresin altında reddediyor; bu kural VMM self-test’iyle doğrulanıyor.
- Paging map API’si artık tanımsız izin flag’lerini sessizce kırpmıyor; bilinmeyen bitler reddediliyor ve VMM negatif self-test’iyle korunuyor.
- Paging map API’si null fiziksel frame’i reddediyor; kernel düşük identity eşlemelerini yanlışlıkla unmap etmeye izin vermiyor ve iki koşul VMM self-test’inde doğrulanıyor.
- Usercopy ve executable-entry doğrulaması artık her sayfada hem PDE hem PTE’nin USER/PRESENT izinlerini denetliyor; yazılım kontrolü gerçek donanım page-table yürüyüşüyle aynı kararı veriyor.
- Paging, kernel page-table’ı paylaşan bir PDE üzerinde kullanıcı bayrağını etkinleştirmiyor; address-space cleanup fiziksel tablo kimliğiyle paylaşılan kernel tablolarını koruyor ve bu kural VMM self-test’inde doğrulanıyor.
- Özel page-table içine user PTE eklenirken mevcut supervisor PTE’lerin bulunduğu karışık tablo reddediliyor; PDE kullanıcı bitinin gelecekte yanlışlıkla genişlemesi önleniyor ve VMM boot self-test’i bu negatif senaryoyu çalıştırıyor.
- Özel CR3’ler kernel ile paylaşılan page-table’larda map/protect/unmap işlemlerini tamamen reddediyor; böylece yalnızca PDE bayrağını değiştirerek kernel identity/framebuffer eşlemelerini kullanıcıya açma veya global tabloyu bozma yolu kapatıldı.
- Kernel page directory’sinde kullanıcı bayrağıyla map/protect işlemleri tamamen reddediliyor; ELF runtime self-test’i de bu politika ile uyumlu olarak geçici özel adres alanında çalışıyor.
- Özel CR3’teki scheduler cleanup’i paylaşılan kernel stack tablolarını değiştirmeden önce kernel CR3’e geçiyor ve eski CR3’e dönüyor; böylece izolasyon koruması kaynak geri kazanımıyla uyumlu tutuluyor.
- CR3 geçişi ve adres alanı yok etme artık yalnızca kayıtlı page directory’lerle yapılabiliyor; rastgele fiziksel adresler fail-closed reddediliyor.
- Journal replay/append mutlak LBA bölgesinin 32-bit sarmasını I/O’dan önce reddediyor; host testi taşan başlangıç LBA’sında callback’in hiç çağrılmadığını doğruluyor.
- Adres alanı geçişi/yıkımı başarısız olursa cleanup sessizce devam etmiyor; kernel fail-closed panic ile duruyor.
- Kullanıcı ELF veya stack unmap cleanup’ı başarısız olursa artık fiziksel kaynak kaybını gizlemeden fail-closed panic uygulanıyor.
- Kullanıcı stack cleanup’i unmap edilen fiziksel frame’i kayıtlı `stack_frame` sahibiyle karşılaştırıyor; eşleşmeyen metadata beklenmeyen frame’i serbest bırakmadan panic ile duruyor.
- ELF unload cleanup’i artık tüm image sayfalarını önce preflight ediyor; eksik mapping veya başarısız unmap başarı gibi raporlanmıyor ve ikinci unload negatif self-test’iyle doğrulanıyor.
- Kısmi ELF yükleme rollback’i de unmap başarısızlığını fail-closed panic olarak ele alıyor; hâlâ eşlenmiş frame’in yanlışlıkla serbest bırakılması önleniyor.
- Scheduler, kernel page directory’sini ring-3 task’a vermeyi reddediyor; sahiplik kaydı bulunmayan bir user fault da sessizce devam etmek yerine fail-closed panic ile duruyor.
- Scheduler slotları generation tabanlı PID üretiyor; ring-3 `GET_PID` ABI’si doğrulanarak ileride stale task-index sahiplik saldırılarını önleyecek kimlik temeli oluşturuluyor.
- PID generation sayacı encoding sınırında sarma yapmıyor; tükenen slot güvenli kaynak kıtlığı olarak reddedilerek eski generation kimliğiyle çakışma önleniyor.
- User-process cleanup kayıtları slot indeksiyle birlikte generation-PID eşleşmesi istiyor; stale cleanup çağrıları yeni sürecin adres alanına dokunamıyor.
- Smoke test’i eski logları reddediyor ve seri çıktıyı gerçek süreç stdout’undan doğruluyor; boş loglar artık başarılı sayılmıyor.
- Scheduler görevleri için 1–8 arası sınırlı öncelik zaman dilimleri ve zorunlu gönüllü `yield` geçişi eklendi.
- FAT16 mount bozuk BPB/cluster geometrisini, aşırı cluster boyutunu ve LBA toplam taşmasını ayrı hata kodlarıyla fail-closed reddediyor.
- Host FAT testi artık geçersiz sektör boyutu, aşırı cluster boyutu ve LBA taşması fixture’larını da doğruluyor.
- Boot metadata host testi E820 sıfır uzunluk/64-bit taşma ve güvenli olmayan font adreslerini reddediyor; CI build adımına eklendi.
- FAT host testi 2.048 deterministik bozuk-imaj mutasyonunda mount/directory/file API’lerini çalıştırarak parser’ın bounded ret davranışını tarıyor.
- FAT host testi üç kopyalı fixture ile üçüncü aynadaki cluster-chain ayrışmasını da doğruluyor.
- ELF loader self-test’i artık bozuk program-header/segment offset, boyut, adres, flag, alignment ve entry fixture’larını da reddediyor; malformed image kabulü regresyon kapsamına alındı.
- Stage-2, kernel yüklemesi sonrası build tarafından üretilen SHA-256 özetini doğruluyor; doğrulama başarısızsa protected mode’a geçmiyor ve başarı biti boot metadata üzerinden kernel tarafından zorunlu tutuluyor. Bu bütünlük kontrolüdür; build özeti imzalı olmadığı için kriptografik secure-boot/kimlik doğrulaması değildir.
- Release için tam imaj üzerinde RSA-3072 detached signature üretme/doğrulama PowerShell araçları eklendi; CI geçerli imzayı ve tek bayt değiştirilmiş imajın reddini doğruluyor. Public key dış güven köküyle dağıtılmadıkça bu yalnızca doğrulama mekanizmasıdır; BIOS floppy loader imzayı runtime’da zorunlu tutmaz.
- Stage-2 VGA font BIOS çağrısının CF ve fiziksel adres aralığını doğruluyor; geçersiz/eksik font artık grafik donanımı varmış gibi işaretlenmiyor.
- E820 BIOS çağrısı, BIOS’un tampon işaretçisini değiştirmesine karşı kayıtlı giriş ofsetini koruyor; VGA kernel sürücüsü font adresi üst sınırını bağımsız olarak tekrar doğruluyor.
- VGA sürücüsü PCI BAR/command yazmadan önce bilinen BGA vendor/device/class kimliğini ve BGA ID register’ını doğruluyor; farklı PCI donanımı yanlışlıkla değiştirilmiyor.
- PCI aygıt kayıtları artık type-0 endpoint BAR’larını salt-okunur biçimde 32/64-bit memory veya I/O türüyle ayrıştırıyor; bridge header’ları atlanıyor ve size-probe yazımı yapılmıyor. Shell `pci` komutu bu kaynakları gösteriyor.
- PCI BAR kayıtları çekirdek içinde tür/hizalama self-test’i ile doğrulanıyor; malformed kayıtlar sürücü katmanına ulaşmadan boot’u fail-closed durduruyor.
- PMM, hizasız yüksek adresli E820 aralıklarını taşmasız yuvarlıyor; 64-bit taban adresi ekleme taşması düşük fiziksel blokları yanlışlıkla kullanılabilir yapamıyor.
- PMM başlangıcı, linker’ın bildirdiği kernel başlangıç/bitiş aralığını doğruluyor; ters veya boş aralıkta bellek serbest bırakılmadan fail-closed duruyor.
- ATA ham yazma yolu her boot’ta write-protected başlıyor ve kernel bunu zorunlu kontrol ediyor; journaling/transaction katmanı olmadan fiziksel disk değişikliği gerçekleşmiyor.
- Kalıcı depolama için ilk journal kayıt sözleşmesi eklendi: bounded isim/içerik alanları, CRC, terminal commit işareti ve ayrılmış alan doğrulaması var; bozuk veya yarım kayıtlar replay edilmeden reddediliyor. Genel ATA yazması hâlâ kapalı; yalnızca doğrulanmış journal window kullanılabiliyor.
- Journal replay katmanı iki geçişli tasarlandı: superblock, contiguous log, artan sequence ve boşluk sonrası sektörler önce tamamen doğrulanıyor; ancak kayıtlar geçerliyse tüketiciye uygulanıyor. Geçerli commit-cleared terminal kayıt yarım append olarak yok sayılıyor, CRC/commit/padding bozukluğu ise fail-closed. Host testi bozuk kayıt, tam replay ve torn-tail kurtarmasını doğruluyor.
- Journal append API’si iki fazlı yayın ve read-back doğrulaması kullanıyor; ikinci yazma yarıda kalırsa yalnızca terminal commit’siz kayıt yok sayılıyor, önceki commit’li kayıtlar güvenle replay edilebiliyor. Geçersiz CRC veya commit alanı yine fail-closed. Host testi başarılı transaction’ları ve yarım ikinci yazma kurtarmasını ayrı ayrı doğruluyor. ATA’nın journal dışı global yazma yolu korumalı.
- Kernel journal replay’i artık VFS volume geometrisini de kontrol ediyor; ayrılmamış veya FAT volume ile çakışan son sektörler geçerli görünümlü olsa bile journal olarak yorumlanmıyor.
- Doğrulanmış journal bölgesi artık `persistent_ramfs` katmanıyla RAMFS `write/rm` işlemlerine bağlanıyor: append öncesi preflight, yalnız journal aralığına izin veren ATA write-window, commit sonrası uygulama ve yeniden başlatma replay’i var. Biçimlendirilmemiş veya sequence alanı tükenmiş journal kalıcı yazmayı etkinleştirmiyor.
- Yeni diskte kalıcı RAMFS başlatmak için `pformat` eklendi; komut yalnızca FAT dışındaki tamamen boş tail bölgesini ve yerleşik varsayılanları değiştirilmemiş RAMFS’i biçimlendiriyor, dolu veya mevcut journal’ı reddediyor.
- RAMFS journal tüketicisi girişleri tekrar doğruluyor; isim sonlandırması/kanonikliği ve metin payload’ındaki NUL baytları reddediliyor, remove kayıtları idempotent uygulanıyor.
- Kernel, write-protected ATA yolunu gerçek `ata_write_sectors` çağrısıyla self-test ediyor; koruma açıkken kabul edilen bir yazma artık boot smoke testini fail-closed düşürüyor.
- VFS artık ATA global fonksiyonlarına bağlı değil; doğrulanmış bir `block_device` üzerinden FAT superfloppy/MBR mount ve bounded retry yapıyor. Kernel, ATA aygıtının kapasite ve salt-okunur yetenek sözleşmesini boot sırasında doğruluyor.
- Syscall girişinde ring-3 dönüş çerçevesi artık segment selector, EFLAGS, user SS ve writable user ESP ile doğrulanıyor; bozuk çerçeve dispatch edilmeden user fault olarak izole ediliyor.
- Kullanıcı stack region seçimi artık PIT/scheduler zamanlamasından beslenen bounded non-cryptographic seed ile çeşitleniyor; ELF loader tüm stack rezerv alanını executable image çakışmalarına karşı kapatıyor. Bu tam ASLR değil, savunma-derinliği katmanı.
- Scheduler stack’leri artık 8 sayfalık bounded kapasite ve guard-page yanında alt sınır canary’si taşıyor; scheduling/reclaim öncesi bozulma görülürse kernel fail-closed panic veriyor. Restart accounting bu yeni allocation boyutuna göre güncellendi.
- FAT kök dizini taraması artık BPB’de belirtilen gerçek entry sayısının dışına çıkmıyor; son sektör artıkları dosya gibi kabul edilmiyor ve host fixture ile doğrulanıyor.
- FAT dosya zinciri cycle testi strict bounded guard ile doğrulanıyor; döngülü cluster zinciri dosya okumasını fail-closed durduruyor.
- FAT mount artık BPB’nin bildirdiği cluster sayısının FAT tablosu kapasitesine sığdığını doğruluyor; eksik FAT girdileriyle yapılan taşma/yanlış sektör okuması host fixture ile reddediliyor.
- FAT mount, tüm FAT kopyalarının reserved-entry imzasını doğruluyor; aynalı tablolardan biri bozuksa volume fail-closed reddediliyor.
- FAT/VFS dosya çözümlemesi bounded 8.3 alt-dizin yollarını destekliyor; mutlak yollar, boş bileşenler, `..` traversal, dosya üzerinden alt-yol ve döngülü/bozuk directory cluster zincirleri fail-closed reddediliyor. Host regresyonu nested read ve traversal reddini doğruluyor.
- FAT dosya zinciri okuması, tüketilen her FAT girdisini tüm aynalı kopyalar ile karşılaştırıyor; kopyalar ayrışırsa okuma reddediliyor.
- FAT16 boş dosyalar yalnızca `start_cluster=0` ile kabul ediliyor; boyutu sıfır olup dangling cluster taşıyan bozuk directory entry’leri reddediliyor.
- Terminated task’ların kernel stack’leri aktif interrupt stack’i korunarak sonraki scheduler geçişinde geri kazanılıyor.
- Birden fazla task aynı scheduler geçişi arasında sonlandığında kernel stack cleanup kayıtları bounded bitmask ile tutuluyor; tek pending slot nedeniyle kaynak sızıntısı oluşmuyor.
- Scheduler kernel stack cleanup’ı guard sayfasını ve her frame’in fiziksel sahiplik kaydını doğruluyor; eksik/eşleşmeyen mapping fail-closed panic ile gizlenmiyor.
- Scheduler’da runnable görev kalmadığında geçersiz/sonlandırılmış frame’e dönülmüyor; dispatch fail-closed panic ile duruyor.
- Scheduler görev adları bounded sabit buffer’a kopyalanıyor; uzun veya sonlandırılmamış adlar metadata pointer’ı olarak saklanmıyor.
- Scheduler için güvenli bloklama/uyandırma primitive’leri eklendi; mevcut görev bloklanırken başka runnable görev yoksa bloklama reddediliyor ve yaşam döngüsü self-test’i boot sırasında çalışıyor.
- Scheduler’ın görev durumu, PID sahipliği ve wake/block geçişleri artık IRQ-korumalı kritik bölümlerde yürütülüyor; IPC gönderimi ile timer preemption arasındaki yarış penceresi kapatıldı.
- Ring-3 task oluşturma API’si entry ve user stack-top adreslerini 4 MiB–3 GiB kullanıcı aralığına sınırlandırıyor; kernel adresleri için negatif boot self-test’i eklendi.
- ATA, IDENTIFY 48-bit desteği ilan ettiğinde 48-bit PIO komutlarını kullanıyor; 32-bit LBA API’sinin temsil edemediği daha büyük kapasite fail-closed reddediliyor. Doğrudan ATA okuma çağrıları da sınırlı kanal reseti sonrası üç denemeye kadar retry yapıyor; kısmi yazmayı çoğaltmamak için yazma retry edilmiyor.
- Sabit BGA framebuffer penceresi PMM’de paging öncesi rezerve edildi; yüksek RAM’li makinelerde MMIO sayfalarının kernel/user tahsisleriyle alias olması engellendi.
- VFS, FAT BPB volume geometrisini gerçek ATA sektör kapasitesiyle mount sırasında karşılaştırıyor; fiziksel aygıtı aşan volume artık sonradan hata vermek yerine fail-closed reddediliyor.
- Sabit boyutlu, kesme güvenli ve taşma kontrollü kernel IPC mesaj kuyruğu eklendi; FIFO, kapasite ve aşırı uzun mesaj reddi boot self-test’iyle doğrulanıyor. `IPC_SEND`/`IPC_RECEIVE` syscall’leri yalnızca 64 baytlık bounded mesajlar ve doğrulanmış kullanıcı aralıklarıyla sunuluyor.
- Ring-3 IPC negatif testi, supervisor adresine gönderimi `EFAULT` ile reddediyor; bu reddetme ayrı QEMU smoke marker’ı ve CI kontrolüyle izleniyor.
- IPC mesajları artık gönderici/hedef generation-PID bilgisi taşıyor; `IPC_SEND_TO` yalnızca aktif bir kullanıcı sürecine yönlendirme yapıyor, alıcı tarafı da yayın veya kendi PID’si olmayan mesajları tüketemiyor. Hedef süreç uyandırma kancası scheduler’a bağlandı ve kuyruk self-test’i hedef izolasyonunu doğruluyor.
- `IPC_RECEIVE_WAIT` eklendi: eşleşen mesaj yoksa kullanıcı görevi scheduler tarafından bloklanıyor, hedef/yayın gönderimiyle uyandırılıyor ve kullanıcı alanı çağrıyı yeniden deneyebiliyor. Süreç fault/exit olduğunda eski generation-PID’ye ait hedefli mesajlar kuyruktan siliniyor.
- Süreç fault/exit’inde sender tarafında kalan orphan IPC mesajları da siliniyor; hem alıcı hem gönderici yönündeki kuyruk kaynakları generation-PID ile temizleniyor.
- Kullanıcı yığını artık ayrılmış bir guard sayfasının üstünde kuruluyor; yığın aşağı taşarsa komşu sayfaya yazmak yerine izole user fault yoluna giriyor.
- Kullanıcı süreç spawn’ı ELF yüklemesinden sonra ayrılmış stack-guard sayfasının hâlâ unmapped olduğunu zorunlu doğruluyor; guard’ı tüketen imaj fail-closed cleanup ile reddediliyor.
- Boot self-test’i guard adresini segment olarak kullanan ELF’i bilerek reddettiriyor ve cleanup sonrasında sistemi normal süreç başlatmaya devam ettiriyor.
- Kullanıcı stack frame’i süreç çalıştırılmadan önce tamamen sıfırlanıyor; fiziksel frame yeniden kullanımı önceki süreç/kernel verisini sızdıramıyor.
- Fiziksel bellek yöneticisi artık kullanıcı çerçeveleri için bounded sahiplik sınıfı tutuyor; kullanıcı eşlemeleri yalnızca identity-map dışı aralıktan `pmm_alloc_user_block` ile alınan frame’leri kabul ediyor ve özel adres alanı yıkımı serbest bırakmadan önce tüm sahipliği preflight ediyor.
- Kullanıcı frame’i eşlenirken tekil claim alıyor ve unmap sırasında bırakılıyor; aynı frame’in alias’lanması, eşleme sürerken serbest bırakılması ve kullanıcı/kernel izin sınırının sessizce değiştirilmesi fail-closed reddediliyor.
- Scheduler kernel stack sayfaları da ilk frame kurulmadan önce sıfırlanıyor; görev slotu yeniden kullanımında eski kernel local verisi taşınmıyor.
- Her yeni kullanıcı süreci, on altı bounded stack bölgesinden generation/reap durumuna göre farklı bir stack bölgesi alıyor; stack adresi process kaydında tutulup cleanup sırasında aynı adrese göre doğrulanıyor.
- Kullanıcı süreç spawn’ı artık aktif kayıtları tarayarak boş stack bölgesini seçiyor; yeniden başlatma zamanlaması etkin stack adresi çakışması oluşturmuyor ve bölge yoksa oluşturma fail-closed reddediliyor.
- `EXIT` syscall’i gerçek demo akışında doğrulandı; normal çıkan user task’ın adres alanı/yığını fault yolu ile aynı fail-closed cleanup’tan geçiyor ve slot güvenle yeniden kullanılıyor.
- GitHub Actions; build, imaj doğrulama, FAT/boot/ELF host testleri ve QEMU ring-3 smoke testini çalıştırıyor.
- ELF yükleyici doğrulaması artık paging/PMM stub’larıyla bağımsız host testinde ve CI’da da çalıştırılıyor; host derlemesi integer-pointer uyarısı bastırmadan tam `-Werror` kapısını kullanıyor.
- ELF host testi, geçerli fixture’ın her byte’ını ve tüm truncated boyutlarını deterministik olarak tarayarak parser’ın offset/count/size taşmalarında bounded kalmasını da doğruluyor.
- FAT ve ELF host testleri CI’da AddressSanitizer/UndefinedBehaviorSanitizer ile ikinci bir bellek güvenliği kapısından da geçiriliyor.
- Linux Makefile imaj üretimi artık hedef floppy alanını her derlemede sıfırlıyor; daha kısa yeni kernel derlemelerinde eski sektör verisi artefakta taşınmıyor.
- Ardışık iki build’in SHA-256 imaj özetini karşılaştıran reproducible-build self-test’i eklendi; build pipeline değişikliklerinde gizli/non-deterministic çıktı farkı yakalanıyor.
- CI checkout action’ı Node.js 24 tabanlı `actions/checkout` v7.0.1’in doğrulanmış tam commit SHA’sına sabitlendi ve checkout credential kalıcılığı kapatıldı; workflow tedarik zinciri riski azaltıldı ve eski Node.js 20 uyarısı giderildi.
- Dependabot, sabitlenmiş GitHub Actions bağımlılıklarını haftalık güncelleme PR’larıyla takip edecek şekilde etkinleştirildi.
- CI’a kernel C kaynaklarını Clang static analyzer ile tarayan bir adım eklendi; yalnızca bilinçli sabit donanım adresi erişimleri hariç tutuluyor.
- PowerShell derleme ve self-test betikleri PATH üzerinden çözülen araçlarda yalnızca gerçek `Application` komutlarını ve dolu kaynak yolunu kabul ediyor; profil function/alias tanımları compiler veya QEMU yerine çalıştırılamıyor.
- Build ve smoke doğrulaması artık 0x55AA boot imzasını zorunlu tutuyor ve tüm marker’lar bulunsa bile `KERNEL PANIC` çıktısını başarısız sayıyor.
- IDT artık 256 vektörün tamamına ortak stub kuruyor; 129–255 arası beklenmeyen vektörler boş descriptor yerine fail-closed dispatcher’a ulaşıyor.
- Breakpoint exception self-test’i artık yalnızca ring-0 çağrısında kabul ediliyor; ring-3 `int3` çağrıları diğer user exception’lar gibi izolasyonlu cleanup’a yönlendiriliyor.

## Doğrulama

- `scripts/build.ps1` başarılı.
- `scripts/block-device-self-test.ps1` başarılı; geçersiz yapılandırma, transfer üst sınırı, taşmasız son-sektör kontrolü, salt-okunur yazma reddi ve callback hata iletimi doğrulandı.
- `scripts/ata-irq-self-test.ps1` IRQ durum makinesinin disabled/ready/pending/error/fallback ve sequence-wrap davranışlarını doğruluyor. QEMU IDE regresyonu gerçek IRQ14 tamamlanmasını `irq-count=1`, `polling-fallbacks=0` ile kanıtlıyor; PIC yalnız handler’ı bulunan IRQ0/1/14 hatlarını sürücü hazır olduktan sonra açabiliyor.
- `scripts/fat-self-test.ps1` başarılı.
- `scripts/ramfs-self-test.ps1` başarılı; null, geçersiz ve sonlandırılmamış RAMFS isimleri bounded olarak reddediliyor.
- `scripts/boot-info-self-test.ps1` başarılı.
- `scripts/journal-self-test.ps1` başarılı; kayıt sektöründeki byte mutasyonları, geçersiz commit ve bozuk CRC fail-closed reddediliyor.
- `scripts/journal-self-test.ps1` ayrıca terminal torn-append sonrasında önceki commit’li kayıtların replay edilebildiğini ve 32-bit LBA taşmasında hiçbir I/O callback’inin çağrılmadığını doğruluyor.
- `scripts/sha256-self-test.ps1` ve `scripts/sha256_self_test.py`, build digest’inin bağımsız SHA-256 hesaplamasıyla eşleştiğini ve tek baytlık kernel bozulmasının farklı özet ürettiğini doğruluyor; CI’da `make sha256-self-test` kapısı etkin.
- `scripts/sha256-boot-negative-test.ps1` ve `scripts/sha256_boot_negative_test.py`, bozulmuş imajı QEMU’da boot edip stage-2’nin kernel girişinden önce seri `!` fail-closed işaretiyle durduğunu doğruluyor; CI’da `make sha256-boot-negative-test` kapısı etkin.
- `scripts/parser-fuzz-self-test.ps1`; boot metadata üzerinde her bayt/değer kombinasyonunu ve 16.384 sabit tohumlu çoklu mutasyonu (toplam 219.136 mutasyon), ELF üzerinde her bayt/değer kombinasyonunu ve tüm truncation boylarını, FAT üzerinde 2.048 deterministik bozuk imajı çalıştırıyor. E820 host testi ayrıca ters kayıt sırası, çakışan reserved/usable aralıklar, devre dışı kayıtlar ve bilinmeyen bellek türleri için fail-closed normalizasyonu doğruluyor; aynı yollar Linux CI’da ASan/UBSan altında çalışıyor.
- QEMU smoke testi başarılı; varsayılan profilde 42 kritik boot/runtime işaretçisi, deterministik ATA/FAT fixture’ında IRQ14 dahil 45 işaretçi doğrulanıyor.
- QEMU `-cpu qemu64` koşusu PAE backend’i ve hardware NX (`hardware-nx=0x00000001`) ile ELF/runtime akışını başarıyla tamamlıyor; varsayılan profil de NX’siz PAE yolunu doğruluyor.
- QEMU `-cpu qemu32,pae=off` koşusu legacy sayfalama fallback’ini (`paging mode=legacy hardware-nx=0x00000000`) ve ELF/runtime akışını başarıyla tamamlıyor.
- CPU yetenek yoklaması RDRAND desteğini de raporluyor; destek varsa ET_DYN load-bias ve kullanıcı stack yerleşim tohumuna donanımsal rastgelelik ekleniyor, desteklenmeyen CPU’larda bounded fallback korunuyor.
- QEMU `-cpu max` koşusu RDRAND yolunu (`rdrand=0x00000001`) ve PAE/NX ELF/runtime akışını başarıyla doğruluyor.
- QEMU’da ring-3 syscall çalışması ve kullanıcı page-fault izolasyonu gözlendi.
- Deterministik 4 MiB FAT16 imajıyla QEMU blok-aygıt/ATA/FAT/journal uçtan uca testi başarılı; IRQ14, mount, kök dizin, dosya okuması ve persistent replay dahil 45 işaretçi doğrulanıyor.
- `scripts/run-self-test.ps1` ile etkileşimli `run RUN.ELF` yolu QEMU’da başarılı; diskten yüklenen programın seri çıktısı doğrulandı.
- `scripts/run-self-test.ps1 -TestPersistentWrite` ile QEMU IDE diski üzerinde yalnızca ayrılmış journal-window’a gerçek `write`/flush yolu çalıştırıldı; QEMU kapandıktan sonra sektör kaydı magic/op/name/content/commit alanlarıyla doğrulandı.
- Aynı QEMU akışının `-TestPersistentFormat` varyantı boş tail üzerinde `pformat` + `write` çalıştırıyor ve ilk journal kaydını kapanış sonrası doğruluyor.
- Tam seri doğrulama koşusu (build, journal/RAMFS/FAT/ELF/boot-info host testleri, iki QEMU bellek profili, journal fixture ve disk ELF yolu) başarılı; iki ardışık imaj SHA-256 değeri eşleşti.
- Kernel C kaynaklarının Clang static analyzer taraması yeni değişikliklerle uyarısız tamamlandı. Windows ortamında ASan/UBSan runtime DLL’si bulunmadığından yerel sanitizer çalıştırması yapılmadı; Linux CI sanitizer kapısı etkin kalıyor.
- Değişiklikler `codex/core-hardening` dalında checkpoint commit’leriyle kaydedildi.
- `ET_DYN` yükleyici kodunun bounded kernel imajı sınırına sığması için linker düşük bellek üst sınırı açıkça 0x90000’a taşındı; VGA/option-ROM penceresinin altında kalma ASSERT’i korunuyor.

## Bilinen sınırlar

ATA IDENTIFY, QEMU IDE PIO okuması ve IRQ14 tamamlanması doğrulandı; 48-bit destekli aygıtlarda yüksek LBA istekleri için EXT komutları hazır, 32-bit LBA API sınırı üzerindeki kapasite fail-closed reddediliyor. Çok-sektörlü okumalar tek bounded PIO komutuyla, IRQ/polling fallback ve üç denemeli bounded retry ile yürütülüyor; kısmi yazmayı çoğaltmamak için yazma retry edilmiyor. DMA ve gerçek donanım matrisi henüz yoktur. Disk yazmaları yalnızca doğrulanmış journal window ile sınırlı; FAT metadata yazımı hâlâ kapalı. IPC bounded beklemeli receive, generation-PID hedefleme ve scheduler uyandırma desteğine sahip; en fazla sekiz bounded kullanıcı süreci destekleniyor. Authentication, secure boot, imzalı modüller, tam ASLR, ağ/USB/SMP ve tam VFS sonraki aşamalardır.

## Öncelikli sonraki geliştirmeler

1. **Depolama (P1):** ATA IRQ14 + polling fallback QEMU’da doğrulandı; sırada bus-master DMA ve gerçek donanım matrisi var. FAT metadata yazımını ancak DMA hata kurtarma, journaling ve bütünlük kontrollerinden sonra aç.
2. **Donanım kapsamı (P2):** Yeni blok aygıt sözleşmesini kullanarak USB depolama/HID, ağ ve zamanlayıcı sürücülerini ekle; her biri için QEMU/host fixture testi yaz.
3. **Güvenlik (P2):** imzalı boot zinciri, kimlik doğrulama, tam ASLR, modül imzalama ve SMP kilitlemesini tasarla.
4. **Test kapsamı (P2):** Boot/E820/ELF/FAT property-fuzz ve sanitizer kapıları tamamlandı; gerçek donanım matrisi için sürekli regresyon kayıtları tut.

Checkpoint commit’leri `origin/main` dalına gönderiliyor; her checkpoint GitHub Actions build/analyzer/sanitizer/QEMU kapılarıyla doğrulanıyor.
