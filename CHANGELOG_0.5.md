# Changelog 0.5 — Follower Unbound

Bu sürümde harici mod bağımlılıklarından tamamen arındırılmış bağımsız mimari, SkyPrompt hızlı etkileşim butonlarının ergonomik revizyonu, demirci ocaklarında fiziksel animasyonlu otonom üretim ve A Horse's Life at/binek entegrasyonu ile atlı savaş (Mounted Combat) özellikleri eklenmiştir.

---

### 1. Tamamen Bağımsız & Özgün Mimari (Zero External DLL Dependency)
- **Harici Mod Bağımlılıklarının Kaldırılması:** Mod artık diğer modların DLL'lerine, harici formID'lerine veya üçüncü taraf içeriklerine bağımlı değildir.
- **Yerel CommonLibSSE-NG & SKSE Çekirdeği:** Tüm takipçi mekaniği, animasyon tetikleyicileri, paket yönlendirmeleri ve yapay zeka davranışları doğrudan Follower Unbound'un kendi native C++ mimarisi ve entegre Papyrus motor çağrılarıyla icra edilir.

---

### 2. SkyPrompt & Hızlı Etkileşim HUD Butonları Optimizasyonu
- **Ergonomik Boyutlandırma:** Önceki sürümde ekranı kaplayan, çok büyük ve alt alta yığılan hızlı etkileşim butonları küçültüldü; daha derli toplu ve şık bir arayüze kavuşturuldu.
- **Dinamik HUD Hizalaması:** Hedeflenen NPC'nin (takipçi, dost, nötr veya düşman) durumuna göre tuşlar ekranı boğmadan, net ve erişilebilir şekilde konumlandırıldı.

---

### 3. Demirci Ocaklarında Otonom Üretim (Ok, Arbelet Oku & Meşale)
- **Ocağa / Örse Doğru Fiziksel Yürüyüş:** Takipçiler arbelet oku, ok veya meşale üretecekleri zaman (`allowCraftBolts`, `allowCraftArrows`, `allowCraftTorches`), oldukları yerde durup eşya yaratmak yerine çevrelerindeki en yakın demirci ocağını (`CraftingSmithingForge`) tespit eder ve ocağın karşısına geçerler.
- **Çekiçleme Animasyonu & Metal Ses Efektleri:** Ocağın yanına gelen takipçi, demirci dövme animasyonunu (`IdleSmithingAnvil` / `IdleSmithing`) başlatır ve örs/çekiç ses efektlerini (`NPCHumanAnvilHammerB3D` / `NPCHumanAnvilHammerMetal3D`) icra eder.
- **Zaman Ayarlı Üretim Döngüsü:** Takipçi 4 saniye boyunca ocakta çalıştıktan sonra animasyon sonlandırılır ve üretilen oklar, arbelet okları veya meşaleler envanterine eklenir.

---

### 4. A Horse's Life At Entegrasyonu & Atlı Savaş (Mounted Combat)
- **Atanan Atı Otomatik Algılama:** *A Horse's Life* modu üzerinden veya oyunun aktör mülkiyet sistemiyle (`SetActorOwner` / `GetActorOwner`) takipçiye atanmış olan binek atları yüksek performanslı aktör listeleri (`ProcessLists::highActorHandles`) üzerinden anlık taranır.
- **Senkronize Biniş (Auto-Mount):** Oyuncu kendi atına bindiğinde, aktif takipçiler de kendilerine atanmış olan ata doğru yönelir ve binerler (mesafe yakınsa doğrudan biner, uzaktaysa ata doğru ilerler).
- **At Üstünde Savaş (Mounted Combat):** 
  - Oyuncu at üzerindeyken savaşa girildiğinde veya savaş başladığında takipçiler atlarından inmez.
  - At üzerinde kalmaya devam ederek silahlarını çeker, düşman hedeflerine mounted combat ile saldırır ve pozisyonlarını korurlar (`EvaluatePackage`).
- **Senkronize İniş (Auto-Dismount):** Oyuncu atından indiğinde tüm takipçiler de atlarından anında inerek oyuncunun arkasındaki yaya formasyonuna geri dönerler.
- **Açma/Kapatma Ayarı (INI Desteği):** `EnableHorseMounting` seçeneği hem `FollowerUnbound.ini` dosyasına hem de C++ ayar yönetimine eklendi; istendiğinde kapatılabilir.
- **OStim Sahne Koruması:** OStim sahneleri sırasında biniş ve iniş kontrolleri dondurularak sahnelerin veya animasyonların kesintiye uğraması engellendi.
