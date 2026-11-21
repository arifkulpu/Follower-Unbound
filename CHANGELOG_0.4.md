# Changelog 0.4 — Follower Unbound

Bu sürümde ilk katılım ikmalleri, geçici görev eskortlarının yönetimi, arayüz kararlılığı, Save/Load takipçi sürekliliği, kart tasarımı optimizasyonu, Simple Follower Framework entegrasyonu ve dahili Hızlı Etkileşim (Quick NPC Interaction) sistemi tamamlanmıştır.

---

### 1. Yeni Katılan Takipçiler İçin Başlangıç İkramiyesi & %100 İhtiyaçlar
- **Otomatik Erzak Yardımı:** Takımınıza ilk defa katılan her takipçinin envanterine otomatik olarak **2 adet İçecek** (`FoodAle`) ve **3 adet Orta Boy Yemek** (`FoodBread01` - Ekmek Somunu) eklenir.
- **Tam Dolu İhtiyaçlar:** İlk kez işe alınan takipçilerin Açlık (Hunger), Susuzluk (Thirst) ve Yorgunluk (Fatigue) seviyeleri doğrudan **%100** olarak başlatılır.

---

### 2. Görev / Refakatçi (Escort) Takipçi Düzeltmesi (Maurice Jondrelle vb.)
- **Ayrılma & Partide Takılı Kalma Sorunu:** Skyrim'in Eldergleam görevinde (`T03`) olduğu gibi, görev bittikten sonra Bethesda motorunun `SetPlayerTeammate(false)` çağrısı yapmaması sebebiyle NPC'lerin partide kalıcı takipçi olarak takılı kalması düzeltildi. Görevi tamamlanan refakatçiler otomatik olarak partiden ve yuvalardan düşürülür.
- **Can Düşüşü & İhtiyaç Koruması:** Geçici görev refakatçilerinin yolculuk esnasında açlık ve susuzluktan can (HP) kaybetmeleri ve debuff almaları engellendi.

---

### 3. Save / Load Sonrası Takipçi Sürekliliği & Mükerrer Diyalog Düzeltmesi
- **Kalıcı Takipçi Durumu:** Oyunu kaydedip tekrar yükledikten sonra takipçilerin oyuncuyu takip etmeyi bırakması ve diyaloglarında mükerrer olarak *"Beni takip et"* seçeneğinin çıkması sorunu giderildi.
- **Çift Taraflı Fraksiyon Senkronizasyonu:** Takipçiler kendi 10 yuva fraksiyonumuzun yanı sıra doğrudan motorun `CurrentFollowerFaction` (0x5C84E) fraksiyonuna da eklenerek kaydedilir; oyun yüklendiğinde (`RestoreSavedProfile`) teammate ve fraksiyon durumları anında doğrulanarak paketleri yenilenir.

---

### 4. PrismaUI Arayüz Sabitleme & Kart Tasarımı İyileştirmesi
- **Scroll Jump Giderildi:** PrismaUI menüsünde iksir, yağma, ekipman ve sistem seçenekleri arasında geçiş yapıldığında veya ayar değiştirildiğinde menünün en üste kayması / sıçraması engellendi. Sayfanın kaydırma konumu (`scrollTop`) her seçim ve yeniden çizimde hafızada tutularak sabit kalması sağlandı.
- **Takipçi Kartları Footer Optimizasyonu:** Kartların alt kısmında yer alan altın miktarı, taşıma kapasitesi ve ışınlanma butonu yeniden tasarlandı. Metinlerin taşması ve butonların üst üste binmesi engellenerek buton daha kompakt ve şık olan **"Işınla" / "Teleport"** formatına getirildi.

---

### 5. Takipçi Yetişme Işınlanması & Manuel Işınlama (Catch-Up Teleport)
- **Otomatik Yetişme Işınlanması:** Takipçiler yağma (loot) yaparken, navigasyonda takıldığında veya engebeli arazide geride kaldığında, belirlenen mesafenin ötesinde 30 saniyeden fazla kalırlarsa otomatik olarak oyuncunun yanına ışınlanır.
- **Özelleştirilebilir Mesafe ve Süre:**
  - Işınlanma özelliği ayarlardan açılıp kapatılabilir (`EnableFollowerTeleport`).
  - Mesafe eşiği ayarlanabilir (`FollowerTeleportDistance`, varsayılan 3000 birim).
  - Süre eşiği ayarlanabilir (`FollowerTeleportDelaySeconds`, varsayılan 30 saniye).
- **Tek Tıkla Yanıma Işınla:**
  - Hem **PrismaUI Genel Bakış** (Overview) kartlarına hem de **Framework** sayfasına **"Işınla" / "Teleport"** butonu eklendi.
  - Oyunun standart veya özel modlu tüm takipçileri için anında çalışır.

---

### 6. Follower Unbound Framework (Simple Follower Framework Özellikleri)
PrismaUI ve SKSEMenuFramework menülerine **Follower Unbound Framework** adında özel yeni bir sayfa/bölüm eklendi:
- **Korumalı / Ölümsüz Mod (Essential / Protected):** Aktif tüm takipçilerin düşmanlar veya tuzaklar tarafından kalıcı olarak ölmesini engelleyen motor seviyesinde koruma bayrağı.
- **Dost Ateşi Koruması (Friendly Fire Protection):** Oyuncunun kazara takipçiye vurması veya büyü isabet ettirmesi durumunda takipçinin düşmanlaşmasını veya misilleme yapmasını engelleyen koruma sistemi.
- **Güvenli Bölgelerde Gezinme (Sandbox Modu):** Oyuncu durduğunda veya beklediğinde takipçilerin şehirlerde, hanlarda ve oyuncu evlerinde serbestçe oturması, gezinmesi ve dinlenmesi.
- **İkna (Speechcraft) Yeteneğine Göre Takipçi Sınırı:** İsteğe bağlı olarak maksimum takipçi sınırının oyuncunun Speechcraft yeteneğine göre ölçeklenmesi (1 ile 10 takipçi arası dinamik kapasite).

---

### 7. Dahili Hızlı Etkileşim Sistemi (Quick NPC Interaction Entegrasyonu)
- **Ayrı Moda Gerek Kalmadan Dahili Destek:** Quick NPC Interaction (199729) özellikleri başka bir mod gerektirmeksizin doğrudan Follower Unbound'un içine entegre edildi.
- **SkyPrompt HUD Butonları:** Bir NPC'ye nişan alındığında diyalog açmadan HUD üzerinde doğrudan hızlı elmas tuşları (`F1 - F4`) belirir:
  - Takipçiler için: `Bekle (Wait)`, `Eşya Alışverişi (Exchange Items)`, `Takip Et (Follow Me)`, `Ayrıl (Dismiss)`.
  - Normal NPC'ler için: `Ticaret (Trade)`, `Dövüş (Fight)`, `Takip Et (Follow)`.
  - Savaşta düşmanlar için: `Teslim Ol / Kaç (Surrender)`, `Taraf Değiştir (Betray Allies)`.
- **Entegre Eklenti & SEQ:** Görev ve kıyafet kayıtları doğrudan `FollowerUnboundTrade.esp` içine birleştirildi ve SEQ dosyası oluşturuldu.
