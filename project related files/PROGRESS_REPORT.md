# CS342 Project 3 - Progress Report

Proje klasörünüzü (`for dijikstra` altındaki dosyaları) detaylıca inceledim. Şu an projenin asıl zorlu kısımlarının ("Library implementation" ve deney dataları) tamamen bitirildiğini görüyorum. Harika bir iş çıkarmışsınız!

Teslim paketini hazırlayana kadar eksik kalan ve tamamlanması gereken **ufak ama zorunlu** detaylar var. İşte son durum:

## ✅ TAMAMLANANLAR (Completed)

- `[x]` **Core Library İskeleti (`rsm.c`):** Sistemin kullandığı Shared Memory (`shm_open`, `mmap`) blokları ve POSIX Semafor (`sem_t`) altyapısı eksiksiz kurulmuş. (Mutex yasaklarına vs. harfiyen uyulmuş.)
- `[x]` **Sistem Döngüleri:** `init`, `destroy`, `started` ve `ended` süreçleri başarılı bir şekilde yazılmış.
- `[x]` **Banker's Algoritması (Avoidance):** `claim`, `request` ve `release` kısımlarında Safe State kontrolleri tam olarak kitaba uygun (`can_satisfy`, `is_safe_state`) uygulanmış. 
- `[x]` **Deadlock Detection:** İstenildiği gibi aktif ve blocklu process'ler üzerinden textbook formatında Work/Finish tespiti kodu eklenmiş.
- `[x]` **Durum Yazdırma:** PDF'teki "Exist, Available, Allocation, Request, MaxDemand, Need" matrix formatı birebir hazırlanmış.
- `[x]` **Performans Deneyleri Verisi:** `experiment.py` kodlarıyla deney çalıştırılmış ve `plot_results.py` ile `experiment_plots.png` başarıyla elde edilmiş.

---

## ⏳ TAMAMLANMAYANLAR (Pending / Leftover)

- `[ ]` **`myapp.c` Geliştirilmesi:**
  - Hoca zorunlu şart olarak şu anki `app.c` yerine **`myapp.c`** adında kendi uygulamamızı istiyor.
  - Şart: **En az 3 Process** ve **en az 5 Resource Type** yaratılmalı. 
  - `avoidflag` 0 verildiğinde bilerek Deadlock yaratıp göstermesi; 1 verildiğinde Avoidance'ın işlediğini göstermesi gerekiyor. Bizim klasörde şu an sadece `app.c` kalmış, bu dosyayı baştan yaratıp tasarlamamız şart.
- `[ ]` **`Makefile` Güncellemesi:**
  - Mevcut `Makefile` sadece `app.c` derliyor. `myapp` komutunu `all` kısmına ve derleme hedefine eklememiz gerekiyor.
- `[ ]` **Final Raporunun Yazılması (`report.pdf`):**
  - Hazırladığınız deney görselini (`experiment_plots.png`) ve yorumlamalarınızı (Avoidance hız/zaman kıyaslamalarını vs.) derleyip bir PDF raporuna dökmeniz lazım. (%20 puan buradan gelecek)
- `[ ]` **Moodle Paketleme ve `README.txt`:**
  - Proje 3 kişi olduğu için `2220XXXX-2230XXXX-2220XXXX` formatında klasör oluşturulup `tar.gz` ile sıkıştırılacak.
  - Klasör içerisinde isim ve numaranızı tutan basit bir `README.txt` oluşturulacak.

---

### Sonraki Adım
İsterseniz **`myapp.c`** dosyasının yazılmasını ve **`Makefile`** güncellemelerini benim yöneteceğim bir planlama ile hemen halledebiliriz. Kodlamaya girişmek için onay veriyor musunuz, yoksa başka bir rapor sürecine mi öncelik verelim?
