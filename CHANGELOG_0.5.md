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
  - `FU_FollowerMountedPackage` içindeki `kIgnoreCombat` (`0x00100000`) yapay zeka bayrağı kaldırılarak at üzerindeyken düşmanları görmezden gelme sorunu tamamen giderildi.
  - Oyuncu veya takipçi çatışmaya girdiğinde (`IsInCombat`), takipçi at üzerinde kalmaya devam eder, kınındaki silahını çeker (`DrawWeaponMagicHands`), oyuncunun hedef aldığı düşmana yönelir (`StartCombat`) ve at üzerinden aktif saldırı yapar (`Horsepower` / Vanilla mounted combat ile tam uyumlu).
  - **Açma/Kapatma Seçeneği (`EnableMountedCombat`):** Takipçilerin at üstünde savaşması istenmiyorsa `FollowerUnbound.ini` üzerinden `EnableMountedCombat=0` yapılarak takipçinin çatışma anında attan inip yerde savaşması, çatışma bittiğinde ise oyuncu hala at üstündeyse otomatik olarak tekrar ata binmesi sağlanabilir.
- **Senkronize İniş (Auto-Dismount):** Oyuncu atından indiğinde tüm takipçiler de atlarından senkronize şekilde inerek oyuncunun arkasındaki yaya formasyonuna geri dönerler.
- **Açma/Kapatma Ayarları (INI Desteği):** 
  - `EnableHorseMounting`: Takipçilerin ata biniş/iniş sistemini açıp kapatır (varsayılan: `1`).
  - `EnableMountedCombat`: Takipçilerin at üstünde savaşa katılmasını veya attan inerek yerde savaşmasını belirler (varsayılan: `1`).
- **OStim Sahne Koruması:** OStim sahneleri sırasında biniş ve iniş kontrolleri dondurularak sahnelerin veya animasyonların kesintiye uğraması engellendi.

---

### 5. Hızlı Seyahat, Hücre Geçişi & Işınlanma İyileştirmeleri (Fast-Travel & Catch-Up Teleport)
- **Hızlı Seyahat & Hücre Yükleme Senkronizasyonu:** `TESFastTravelEndEvent` ve `TESCellFullyLoadedEvent` olayları üzerinden, oyuncu hızlı seyahat yaptığında (`fast travel`), konsol komutuyla veya yükleme kapısıyla yeni bir alana geçtiğinde geride kalan tüm aktif takipçiler anında oyuncunun yanına taşınır.
- **PlayerFollowerPackage Yaya Takip Düzeltmesi (ESP):** `PlayerFollowerPackage` (`0x0005C84B`) paketindeki `Must Mount Horse = 1` bayrağı temizlendi (`0x08040000` $\rightarrow$ `0x08000000`). Artık yoldaşlar oyuncu at üstünde değilken at arama kilitlenmesine girmeden oyuncuyu yerde yaya olarak sorunsuz takip eder.
- **Akıllı Hücre & İç Mekan İntikali:** Takipçi at üzerindeyken oyuncu iç mekana (han, ev, zindan) girdiğinde veya at binilemeyen bir alana ışınlandığında takipçi önce attan indirilir ve oyuncunun yanına intikal ettirilir. Farklı hücre durumlarında 3 saniye içinde yetişme ışınlanması devreye girer.
- **Farklı Worldspace & Şehir İçi Ticaret İzolasyonu (TradeManager):** Kapalı şehirlerde (Whiterun, Solitude, Riften vb. `WhiterunWorld`) yer alan takipçilerin, koordinat olarak yakın görünen ancak şehir surlarının ve yükleme kapısının dışında (`Tamriel`) bulunan tüccarlara (örneğin şehir kapısı dışındaki "Millie" veya karavanlar) doğru yürümeye çalışması engellendi. Artık takipçiler yalnızca bulundukları aynı worldspace ve mekandaki tüccarlarla ticaret yapar.
- **Yuva Koruması (Slot Safety):** `IsActualFollower` fonksiyonuna `IsInFollowerSlot` kontrolü eklenerek, 10 takipçi yuvasında kayıtlı olan yoldaşların hücre geçişlerinde veya mesafe uzadığında asla yanlışlıkla takibi bırakmaması sağlandı.
