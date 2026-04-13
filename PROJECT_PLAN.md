# CS342 Project 3 (RSM Library) - Detaylı Proje Planı

Bu belge, Moodle üzerinden paylaşılan **Project 3 Dokümanı** (`project3.md`), **Instructor Tartışma Notları** (`DISCUSSION_SUMMARY.md`) ve grubun mesajlaşma geçmişi göz önüne alınarak grubun (Şebnem, Heysem, Hasan) iş dağılımını planlamak amacıyla oluşturulmuştur.

## 📌 Genel Proje Geliştirme Kuralları
- **Kesinlikle `mutex` ya da `condition variables` kullanılmayacak.** Sadece **POSIX semaphores** kullanılacak.
- Veriler işlemler arası **Shared Memory** kullanılarak aktarılacak.
- Bütün testler Ubuntu Linux 64-bit üzerinde başarılı olmalı (kendi Mac ortamımızda yapsak da son test Linux'ta olacak).
- `rsm_print_state` dışında kütüphanenin hiçbir fonksiyonu konsola (screen) print yapmamalı.

---

## 👥 Takım İş Bölümü ve Görev Durumları

### 1. Şebnem Bihter Keleş (Başlangıç ve Süreç Kayıtları) - ✅ *Tamamlandı/PR Bekliyor*
Şebnem projenin temel yapı taşlarını ve shared memory segment'ini başlatmayı üstlendi.
- **`int rsm_init(...)`**: Shared memory'i oluşturma, semaforların başlangıç değerlerini atama.
- **`int rsm_destroy()`**: Shared memory'i ve semaforları yok etme, kalıntı bırakmama. (Hocanın notlarına göre `rsm_process_ended` gibi global bir terminate noktası olarak da çalışabilir).
- **`int rsm_process_started(...)`**: Çocuk process'leri aktif olarak kaydetme. (Avoidance kapalıysa, diğer processler gelene kadar burada bariyer kurularak beklenecek).

### 2. Lütfü Heysem Kızıloğlu (Kaynak Tahsisi - Banker's Algoritması) - ⏳ *Devam Ediyor*
Sistemin beynini oluşturan, process'lerin kaynakları "claim" edip, sonrasında "request" ve "release" ettikleri tahsis/avoidance döngüsünün yazılması.
- **`int rsm_claim(...)`**: (Eğer `avoid == 1` ise) Process'in maksimum ihtiyaç duyacağı kaynağı belirtmesi. Sadece başlatmada 1 kez çağrılabilir kabul ediliyor.
- **`int rsm_request(...)`**: Kaynak isteği yapma. Eğer `avoid==1` ise algoritma ile "Safe State" kontrolü yapılacak. Kaynak verilemiyorsa veya unsafe ise process (semafor üzerinden) **bloke edilecek**.
- **`int rsm_release(...)`**: Alınan kaynakları sisteme geri bırakılıp, bloke olmuş diğer bekleyen (waiting) process'lerin semaforu kaldırılarak uyandırılması. (Uyandırma sırasında özel bir sıra yani FIFO gözetilmeyecek).

### 3. Hasan Tufan (Denetim, Test Uygulaması, Ekstra Geliştirmeler & Rapor) - 🔜 *Sıradaki Aşama*
Projenin denetim fonksiyonlarını, uygulamanın çalışacağı ana test simülasyonunu (`myapp.c`) ve notun %20'sini etkileyen raporlamayı üstlenecek kişi.
- **`int rsm_process_ended()`**: Projenin bir noktasında uygulama işini bitirdiğinde herkesi sonlandıran fonksiyon. Hoca "Uyguluma içinde cleanup sonrası `kill(0, SIGTERM)` ile herkesi kapatabilirsiniz" demiştir.
- **`int rsm_detection()`**: Eğer avoidance kapalıysa oluşabilecek deadlock'ları sayacak mekanizma. OS Ders kitabındaki **Work/Finish** algoritmasına birebir uyarak yazılacak. (Not: Sadece "active ve blocked/waiting" processler deadlocked sayılacaktır).
- **`void rsm_print_state()`**: Sistemi istenen `Exist, Available, Allocation, Request, MaxDemand, Need` matris formatında konsola sorunsuz dökme.
- **`myapp.c` Geliştirilmesi**:
  - En az **3 process** ve **5 resource type** barındıracak.
  - Komut satırından `avoidflag` parametresi alacak (`./myapp 0` veya `./myapp 1`).
  - Flag 0 iken **deadlock oluşturma senaryosu** tasarlanacak ve tespit edilecek.
  - Flag 1 iken avoidance'in deadlock'u nasıl önlediği gösterilecek.
- **`Makefile` Güncellemesi**: `make` denildiğinde `myapp`ı derleyecek ekleme yapılacak.
- **Performans Deneyleri ve Rapor (`report.pdf`)**: 
  - Örneğin: Kütüphanenin safety-check (safe state kontrol) işlevi, büyük matrislerde ne kadar zaman alıyor ölçülecek. Deney scriptleri hazırlanacak.
  - Grubun contribution formatında ve isimlerinin bulunduğu son Moodle paketlemesi (`.tar.gz`) yapılacak.

---

## 🚀 Proje İlerleme Akışı (Yol Haritası)

1. **Adım:** Şebnem'in attığı `rsm_init` ve `rsm_process_started` PR'ı, ana testlerin çalışması için ön koşul olduğundan hızla Master/Main branch'e birleştirilmeli.
2. **Adım:** Hasan'ın `rsm_print_state`i yazması. (Sistem durumunu konsola döken bu fonksiyon, Heysem'in request/release kısmını "debug" etmesi için büyük kolaylık sağlayacaktır, öncelikli yazılmalı).
3. **Adım:** Heysem'in `claim`, `request` algoritmalarının tamamlanması.
4. **Adım:** Hasan'ın `myapp.c` yapısını kurarak, sistem kodunu stress testine sokup, processler arası senkronizasyon (semafor) uyumunu test etmesi. Race condition hatalarının tespit edilmesi ve çözüm.
5. **Adım:** Linux 64-bit platformda (Ubuntu 24 test ortamı) kodların son doğrulamasının yapılıp çalışıp çalışmadığının onaylanması.
6. **Adım:** Deneylerin çıkarılıp raporların (Rapor.pdf) yazılması ve Moodle teslim süreci (13 Nisan 23:59 öncesi).
