(() => {
  const $ = (id) => document.getElementById(id);
  const send = (payload) => {
    if (typeof window.fuAction === 'function') window.fuAction(JSON.stringify(payload));
  };
  let state = { followers: [], settings: {} };
  let page = 'overview';
  let selectedFollower = 0;
  let gearTab = 'weapons';
  let economyTab = 'buy';
  let buyCategory = 'overview';
  let pickerOpen = false;
  let selectedGear = new Set();
  const pageTitles = { overview: 'Overview', equipment: 'Equipment', loot: 'Loot rules', economy: 'Trade & supplies', settings: 'Systems' };
  const language = () => state.settings?.uiLanguage === 'tr' ? 'tr' : 'en';
  // The UI was originally authored in English with a few Turkish loadout labels.
  // Keep the markup readable and translate rendered text centrally, including
  // controls created by every tab and the fixed navigation chrome.
  const trText = {
    'Overview':'Genel bakış','Equipment':'Ekipman','Loot rules':'Yağma kuralları','Trade & supplies':'Ticaret ve malzemeler','Systems':'Sistemler',
    'MANAGEMENT':'YÖNETİM','YOUR COMPANY':'YOLDAŞLARIN','FOLLOWER ECONOMY':'TAKİPÇİ EKONOMİSİ','FOLLOWER PREFERENCES':'TAKİPÇİ TERCİHLERİ','GLOBAL BEHAVIOR':'GENEL DAVRANIŞ','THE PEOPLE AT YOUR SIDE':'YANINDAKİLER','LOADOUT CONTROL':'EKİPMAN YÖNETİMİ',
    'SELECT COMPANION':'TAKİPÇİ SEÇ','No companion':'Takipçi yok','No follower selected.':'Takipçi seçilmedi.','No active followers found. Press Scan Party to refresh.':'Aktif takipçi bulunamadı. Yenilemek için grubu tara.',
    'Companions, in one place.':'Tüm takipçiler tek yerde.','Review their condition, equipment and daily routines.':'Durumlarını, ekipmanlarını ve günlük rutinlerini incele.','SCAN PARTY':'GRUBU TARA','Active companions':'Aktif takipçiler','Party gold':'Grup altını','Spendable gold':'Harcanabilir altın','Average hunger':'Ortalama açlık','Average thirst':'Ortalama susuzluk','Average fatigue':'Ortalama yorgunluk','Your followers':'Takipçilerin',
    'Followers and animal allies':'Takipçiler ve hayvan yoldaşlar','Available for supplies & gear':'Malzeme ve ekipman için kullanılabilir','Across tracked companions':'Takip edilen tüm yoldaşlarda','companion tracked':'takipçi takip ediliyor','companions tracked':'takipçi takip ediliyor','spendable':'harcanabilir','total':'toplam','Carry':'Taşıma',
    'Automatic loot':'Otomatik yağma','Automatic equipment upgrades':'Otomatik ekipman yükseltmeleri','Protect starting outfit':'Başlangıç kıyafetini koru','Combat potions':'Savaş iksirleri','Needs system':'İhtiyaç sistemi','Poison system':'Zehir sistemi','Shopping':'Alışveriş','Selling':'Satış','Ask before equipping looted gear':'Yağmalanan ekipmanı kuşanmadan önce sor','Offer better gear from merchants':'Tüccarlardan daha iyi ekipman öner','Mage robe preference':'Büyücü cübbe tercihi','Spell tome learning':'Büyü kitabı öğrenme',
    'NOTIFICATIONS & ALERTS':'BİLDİRİMLER VE UYARILAR','Enable all notifications':'Tüm bildirimleri aç','Looting alerts':'Yağma bildirimleri','Shopping & stock alerts':'Alışveriş ve stok bildirimleri','Trade & sales alerts':'Ticaret ve satış bildirimleri','Needs alerts':'İhtiyaç bildirimleri','Combat potion & poison alerts':'Savaş iksiri ve zehir bildirimleri','Equipment upgrade alerts':'Ekipman yükseltme bildirimleri','ENABLE ALL':'TÜMÜNÜ AÇ','DISABLE ALL':'TÜMÜNÜ KAPAT','Notification delivery style':'Bildirim gösterim biçimi','Batched (Toplu Özet)':'Toplu özet','Detailed (Ayrıntılı)':'Ayrıntılı','Critical Only (Sadece Kritik)':'Yalnızca kritik',
    'GLOBAL LOOT CATEGORIES':'GENEL YAĞMA KATEGORİLERİ','Hostile corpses only':'Yalnızca düşman cesetleri','Gold':'Altın','Weapons':'Silahlar','Armor':'Zırhlar','Arrows & bolts':'Oklar ve cıvatalar','Potions':'İksirler','Food':'Yiyecek','Ingredients':'Malzemeler','Soul gems':'Ruh taşları','Other valuables':'Diğer değerli eşyalar',
    'EQUIPMENT UPGRADES':'EKİPMAN YÜKSELTMELERİ','Gear upgrade check chance (%)':'Ekipman yükseltme kontrol şansı (%)','FOLLOWER GOLD SAVINGS':'TAKİPÇİ ALTIN BİRİKİMİ','Saved':'Birikmiş','Target gold (1–10,000)':'Hedef altın (1–10.000)','LOOT TIMING':'YAĞMA ZAMANLAMASI','Post-combat loot delay':'Savaş sonrası yağma gecikmesi','MENU HOTKEY':'MENÜ KISAYOLU','REFRESH STATE':'DURUMU YENİLE','Close':'Kapat','Refresh followers':'Takipçileri yenile',
    'Equipment upgrade available':'Ekipman yükseltmesi mevcut','Corpse loot equipment offer':'Ceset yağması ekipman önerisi','A better item was found':'Daha iyi bir eşya bulundu','Current equipment':'Mevcut ekipman','Currently equipped':'Şu an kuşanılan','Purchase offer':'Satın alma önerisi','Recovered from corpse':'Cesetten alındı','Loot item':'Yağma eşyası','Price':'Fiyat','Follower spendable gold':'Takipçinin harcanabilir altını','KEEP CURRENT':'MEVCUDU KORU','EQUIP & PROTECT':'KUŞAN VE KORU','BUY, EQUIP & PROTECT':'SATIN AL, KUŞAN VE KORU','NO — NEVER ASK FOR THIS ITEM AGAIN':'HAYIR — BU EŞYAYI TEKRAR SORMA',
    'WEAR & PROTECT SELECTED':'SEÇİLENLERİ KUŞAN VE KORU','PROTECTED':'KORUMALI','EQUIPPED':'KUŞANILMIŞ','Apparel & armor':'Kıyafet ve zırh','WEAPONS':'SİLAHLAR','Crafting preferences':'Üretim tercihleri','Purchase targets':'Satın alma hedefleri','SELL':'SAT','BUY':'SATIN AL','Maximum':'En fazla','Ingredient stock cap':'Malzeme stok sınırı','Personal gold savings':'Kişisel altın birikimi','Income share':'Gelir payı',
    'INTERFACE LANGUAGE':'ARAYÜZ DİLİ','Language / Dil':'Dil','Choose the language used by Follower Unbound’s Prisma interface and alerts.':'Follower Unbound Prisma arayüzü ve bildirimleri için dili seçin.',
    'Control companion automation, notifications and global loot behavior.':'Takipçi otomasyonunu, bildirimleri ve genel yağma davranışını yönetin.','Share eligible leftovers after you close a container.':'Konteyneri kapattıktan sonra uygun kalan eşyaları paylaş.','Automatically equip eligible gear received through Follower Unbound.':'Follower Unbound üzerinden alınan uygun ekipmanı otomatik kuşan.','Capture equipped clothing when a follower is first registered.':'Takipçi ilk kez kaydedildiğinde üzerindeki kıyafetleri korumaya al.','Use potions when combat health or magicka is low.':'Savaşta can veya büyü gücü azaldığında iksir kullan.','Track hunger, thirst and fatigue.':'Açlık, susuzluk ve yorgunluğu takip et.','Apply poison to compatible weapons.':'Uygun silahlara zehir sür.','Buy supplies from nearby merchants.':'Yakındaki tüccarlardan malzeme satın al.','Sell excess items to nearby merchants.':'Fazla eşyaları yakındaki tüccarlara sat.','Show a prompt before equipping a better item found in loot.':'Yağmada bulunan daha iyi eşyayı kuşanmadan önce sor.','Ask before a follower buys a better weapon or armor from a merchant.':'Takipçi tüccardan daha iyi silah veya zırh almadan önce sor.','Prefer robes for mage-class followers.':'Büyücü sınıfı takipçiler cübbe tercih etsin.','Followers read and learn spells from spell tomes in safe places (inns, homes, towns).':'Takipçiler güvenli yerlerde (han, ev ve şehirlerde) büyü kitaplarını okuyup öğrenir.',
    'Master switch for all on-screen companion messages and alerts.':'Takipçi mesajları ve uyarıları için ana anahtar.','Show messages when companions collect items or gold.':'Takipçiler eşya veya altın topladığında bildir.','Show messages for merchant purchases and low stock warnings.':'Satın alımları ve düşük stok uyarılarını bildir.','Show messages when excess items are sold to merchants.':'Fazla eşya tüccara satıldığında bildir.','Show messages when companions eat, drink or become starved.':'Takipçiler yiyip içtiğinde veya aç kaldığında bildir.','Show messages when potions or weapon poisons are used.':'İksir veya silah zehri kullanıldığında bildir.','Show messages when companions equip upgraded weapons or armor.':'Takipçiler daha iyi silah veya zırh kuşandığında bildir.',
    'Ignore neutral and friendly actors.':'Tarafsız ve dost karakterleri atla.','Chance (1–100%) that followers look for or offer better weapons and armor.':'Takipçilerin daha iyi silah ve zırh arama/önerme şansı (1–100%).','Seconds before followers begin looting.':'Takipçilerin yağmaya başlamadan önce bekleyeceği saniye.','Set what each companion collects, sells, buys and crafts.':'Her takipçinin toplayacağı, satacağı, alacağı ve üreteceği şeyleri ayarlayın.',
    'Purchase targets':'Satın alma hedefleri','Items to sell':'Satılacak eşyalar','Choose which categories followers may sell. Worn and protected equipment stays with them; supply limits from Purchases are kept in reserve.':'Takipçilerin satabileceği kategorileri seçin. Kuşanılan ve korunan ekipman kalır; satın alma stok hedefleri ayrılır.','Select a category to jump straight to its settings. Each enabled supply is stocked up to its target.':'Ayarlarına gitmek için kategori seçin. Etkin her ürün hedef stoğa tamamlanır.','Meals, water and beverages':'Yemekler, su ve içecekler','Ammunition supplies':'Mühimmat malzemeleri','Six soul gem types':'Altı ruh taşı türü','Torches, oil and soap':'Meşale, yağ ve sabun','Invisibility potions':'Görünmezlik iksirleri','Health, magicka & stamina potions':'Can, büyü ve dayanıklılık iksirleri','Choose potion strengths and target stock.':'İksir gücünü ve hedef stoğu seçin.','Choose poison effects and target stock.':'Zehir etkilerini ve hedef stoğu seçin.','Potion ingredients':'İksir malzemeleri','Bought from alchemists; harvested ingredients are shared.':'Simyacılardan alınır; toplanan malzemeler paylaşılır.','Ingredients are bought from alchemists (wheat may also come from general merchants). Ingredients you harvest are shared among followers.':'Malzemeler simyacılardan alınır (buğday genel tüccarlarda da bulunabilir). Topladığınız malzemeler takipçiler arasında paylaşılır.',
    'Target stock':'Hedef stok','Target':'Hedef','OWNED':'MEVCUT','Enable poison purchases':'Zehir satın alımını etkinleştir','Buy selected poison types':'Seçilen zehir türlerini al','Large food':'Büyük yiyecek','Medium food':'Orta boy yiyecek','Small food':'Küçük yiyecek','Water & milk':'Su ve süt','Alcohol':'Alkol','Crossbow bolts':'Arbalet cıvataları','Petty soul gems':'Ufak ruh taşları','Lesser soul gems':'Küçük ruh taşları','Common soul gems':'Orta ruh taşları','Greater soul gems':'Büyük ruh taşları','Grand soul gems':'Görkemli ruh taşları','Black soul gems':'Siyah ruh taşları','Lantern oil':'Fener yağı','Invisibility potions':'Görünmezlik iksirleri','Cure Disease potion':'Hastalık Tedavi İksiri','Cure Poison potion':'Zehir Tedavi İksiri',
    'Craft arrows':'Ok üret','Craft crossbow bolts':'Arbalet cıvatası üret','Craft torches':'Meşale üret','Craft potions':'İksir üret','Ingredient stock cap':'Malzeme stok sınırı','PER INGREDIENT / FOLLOWER':'HER MALZEME / TAKİPÇİ BAŞINA','Hover over a crafting option to see its recipe. Arrows, bolts and torches require a forge within 400 game units. Potions require an alchemy lab within 400 units. Ingredients harvested by you are shared among followers.':'Tarifi görmek için üretim seçeneğinin üzerine gelin. Ok, cıvata ve meşale için 400 oyun birimi içinde demirci ocağı; iksir için 400 birim içinde simya laboratuvarı gerekir. Topladığınız malzemeler takipçilerle paylaşılır.',
    'Excess weapons':'Fazla silahlar','Excess armor':'Fazla zırhlar','Clothing':'Kıyafetler','Jewelry':'Takılar','SUPPLIES':'MALZEMELER','VALUABLES':'DEĞERLİ EŞYALAR','Ammunition':'Mühimmat','Food & drink':'Yiyecek ve içecek','Travel & utility':'Seyahat ve araç gereç','Special potions':'Özel iksirler','Treatment potions':'Tedavi iksirleri','Arrows & bolts':'Oklar ve cıvatalar','Stock limits':'Stok sınırları','ALL CATEGORIES':'TÜM KATEGORİLER','OPEN':'AÇ','Currently equipped:':'Şu an kuşanılan:','Right hand:':'Sağ el:','Left hand:':'Sol el:'
    ,'Allow looting':'Yağmaya izin ver','Clothing & robes':'Kıyafet ve cübbeler','Potions & poisons':'İksirler ve zehirler','Drinks':'İçecekler','Gems & valuables':'Mücevherler ve değerli eşyalar','Books':'Kitaplar','Scrolls':'Parşömenler','Allow Looting':'Yağmaya izin ver',
    'Health potions':'Can iksirleri','Magicka potions':'Büyü iksirleri','Stamina potions':'Dayanıklılık iksirleri','Poisons':'Zehirler','Large food':'Büyük yiyecek','Medium food':'Orta boy yiyecek','Small food':'Küçük yiyecek','Alcohol':'Alkol','Arrows':'Oklar','Bolts':'Cıvatalar','Torches':'Meşaleler','Lantern oil':'Fener yağı','Soap':'Sabun',
    'Minor':'Zayıf','Standard':'Standart','Plentiful':'Güçlü','Vigorous':'Çok güçlü','Extreme':'Aşırı güçlü','Ultimate':'En güçlü','Health':'Can','Magicka':'Büyü','Stamina':'Dayanıklılık','Health potions':'Can iksirleri','Magicka potions':'Büyü iksirleri','Stamina potions':'Dayanıklılık iksirleri','Health damage poisons':'Can hasarı zehirleri','Magicka damage poisons':'Büyü hasarı zehirleri','Stamina damage poisons':'Dayanıklılık hasarı zehirleri','Other effects poisons':'Diğer etkili zehirler',
    'Buy selected poison types':'Seçilen zehir türlerini satın al','Target':'Hedef','Target stock':'Hedef stok','Maximum':'En fazla','Buy':'Satın al','Buy selected':'Seçilenleri satın al','Select only the pieces you want equipped and protected.':'Yalnızca kuşanıp korumak istediğiniz parçaları seçin.','Select the left box to choose items. Protection keeps gear from sale and restores protected apparel after outfit changes.':'Eşya seçmek için soldaki kutuyu işaretleyin. Koruma, eşyanın satılmasını önler ve kıyafet değişikliğinden sonra korunan eşyayı geri kuşandırır.',
    'APPAREL & ARMOR':'KIYAFET VE ZIRH','INVENTORY':'ENVANTER','WORN':'GİYİLİ','DAMAGE':'HASAR','ARMOR':'ZIRH','VALUE':'DEĞER','PROTECTED':'KORUMALI','EQUIPPED':'KUŞANILMIŞ','WEAR & PROTECT SELECTED':'SEÇİLENLERİ KUŞAN VE KORU','No items found.':'Eşya bulunamadı.','No weapons found.':'Silah bulunamadı.','No apparel or armor found.':'Kıyafet veya zırh bulunamadı.',
    'Batched (Toplu Özet)':'Toplu özet','Detailed (Ayrıntılı)':'Ayrıntılı','Critical Only (Sadece Kritik)':'Yalnızca kritik','Her İkisi (Any)':'Her ikisi','Boş (Empty)':'Boş','Dolu (Full)':'Dolu','Target stock':'Hedef stok','FOOD':'YİYECEK','DRINKS':'İÇECEKLER','SOUL GEMS':'RUH TAŞLARI','WEAPONS & ARMOR':'SİLAHLAR VE ZIRHLAR','SUPPLIES':'MALZEMELER','VALUABLES':'DEĞERLİ EŞYALAR',
    'Your followers':'Takipçilerin','Follower':'Takipçi','No active followers detected.':'Aktif takipçi bulunamadı.','Animal companion':'Hayvan yoldaş','Companion':'Yoldaş','Male':'Erkek','Female':'Kadın','FOLLOWER':'TAKİPÇİ','ANIMAL':'HAYVAN','spendable':'harcanabilir','saved':'birikmiş','total':'toplam','Save preferences':'Tercihleri kaydet','Select one or more items first.':'Önce bir veya daha fazla eşya seçin.','Selected equipment applied.':'Seçilen ekipman uygulandı.','Equipment assigned.':'Ekipman atandı.','Equipment removed.':'Ekipman çıkarıldı.','Party scan requested.':'Grup taraması istendi.',
    'WALK TO MERCHANTS':'SATICILARA YÜRÜME','Walk to merchants':'Satıcılara yürü','Followers walk up to a nearby merchant before selling or buying.':'Takipçiler satış veya alışveriş yapmadan önce yakındaki satıcıya yürür.',
    'WALK TO CORPSES':'CESETLERE YÜRÜME','Walk to corpses':'Cesetlere yürü','Followers walk up to a body before looting it, then search it.':'Takipçiler bir cesedi yağmalamadan önce yanına yürür, sonra üstünü arar.','Wait before walking (seconds)':'Yürümeden önce bekleme (saniye)','How long after a fight or a closed corpse before followers set off.':'Savaştan ya da cesedin kapatılmasından sonra takipçilerin yola çıkmasına kadar geçen süre.','Voice lines':'Seslendirme','Followers and merchants greet and thank each other with the game\'s own voice lines.':'Takipçiler ve satıcılar birbirini oyunun kendi ses satırlarıyla karşılar ve teşekkür eder.','Hold followers while walking':'Yürürken takipçiyi beklemede tut','Stops other follower mods from teleporting a walking companion back. Leave it off unless you see teleporting.':'Diğer takipçi modlarının yürüyen takipçiyi geri ışınlamasını engeller. Işınlanma görmüyorsan kapalı bırak.',
    'Notice distance':'Fark etme mesafesi','A merchant this close (game units) counts as nearby.':'Bu kadar yakındaki (oyun birimi) satıcı yakında sayılır.',
    'Stay-near time (seconds)':'Yakında kalma süresi (saniye)','How long a follower stays near a merchant before walking over.':'Takipçinin satıcıya yürümeden önce yakında kalacağı süre.',
    'Talk time (seconds)':'Konuşma süresi (saniye)','How long they stand with the merchant after the deal.':'İşlemden sonra satıcıyla birlikte durdukları süre.',
    'Wait after a deal (game hours)':'İşlemden sonra bekleme (oyun saati)','Before the same follower visits the same merchant again after a sale or purchase.':'Aynı takipçi, satış veya alıştan sonra aynı satıcıya tekrar gitmeden önce bekler.',
    'Wait after an empty visit (game hours)':'Boş ziyaretten sonra bekleme (oyun saati)','Before trying the same merchant again when nothing was traded.':'Hiçbir şey alınıp satılmadıysa aynı satıcıyı tekrar denemeden önce bekler.',
    'FollowerUnboundTrade.esp is not loaded. Followers trade from a distance as before.':'FollowerUnboundTrade.esp yüklü değil. Takipçiler eskisi gibi uzaktan ticaret yapar.'
  };
  const enText = {
    ...Object.fromEntries(Object.entries(trText).map(([en,tr]) => [tr,en])),
    'ZIRH':'ARMOR','ENVANTER':'INVENTORY','Mevcut Kuşanılan:':'Currently equipped:','Sağ El:':'Right hand:','Sol El:':'Left hand:',
    'ASLİ SİLAH (Primary)':'PRIMARY WEAPON','YARDIMCI SİLAH (Secondary)':'SECONDARY WEAPON','Silah Tarzı:':'Weapon style:','Serbest / Otomatik (Any)':'Any / Automatic','Tek Elli (One-Handed)':'One-handed','Çift Elli (Two-Handed)':'Two-handed','Yay (Bow)':'Bow','Arbalet (Crossbow)':'Crossbow','Asa (Staff)':'Staff','Herhangi':'Any','Kılıç (Sword)':'Sword','Hançer (Dagger)':'Dagger','Savaş Baltası (War Axe)':'War axe','Gürz (Mace)':'Mace','Büyü / Boş (Spell/Empty)':'Spell / Empty','Çift El Türü:':'Two-handed type:','Büyük Kılıç (Greatsword)':'Greatsword','Büyük Balta (Battleaxe)':'Battleaxe','Savaş Çekici (Warhammer)':'Warhammer',
    'Takipçilerinizin silah, kıyafet bütünlüğü ve envanter koruma tercihlerini yönetin.':'Manage follower weapons, outfit integrity and inventory protection.',
    'Takipçinin genel savaşlarda öncelikli kullandığı ana silah tarzı.':'The primary weapon style this follower prefers in combat.','Duruma veya mesafeye göre geçiş yapacağı alternatif silah (Örn: Yay/Kılıç Kalkan).':'Alternative weapons used depending on situation or distance (e.g. bow or sword and shield).',
    'Ekipman atandı.':'Equipment assigned.','Ekipman çıkarıldı.':'Equipment removed.','Grup taraması istendi.':'Party scan requested.','Tüm bildirimler açıldı.':'All notifications enabled.','Tüm bildirimler kapatıldı.':'All notifications disabled.','Menü tuşu güncellendi: ':'Menu hotkey updated: ',
    'Boş':'Empty','Korumalı':'Protected','Kuşanılmış':'Equipped','Yıpranmış':'Worn','Zırh':'Armor','Silah':'Weapon','Kıyafet':'Clothing','Eldiven':'Gloves','Bot':'Boots','Kask':'Helmet','Kalkan':'Shield','Yüzük':'Ring','Kolye':'Necklace','Hasar':'Damage','Değer':'Value','Zırh puanı':'Armor rating',
    'Takipçinin envanterinde silah bulunmuyor.':'No weapons in this follower’s inventory.','Takipçinin envanterinde giysi bulunmuyor.':'No apparel in this follower’s inventory.','Envanterde eşya bulunmuyor.':'No items in inventory.',
    'Mevcut Kuşanılan:':'Currently equipped:','Sağ El:':'Right hand:','Sol El:':'Left hand:','ASLİ SİLAH TERCİHİ':'PRIMARY WEAPON PREFERENCE','ASLİ SİLAH (Primary)':'PRIMARY WEAPON (Primary)','YARDIMCI SİLAH TERCİHİ':'SECONDARY WEAPON PREFERENCE','YARDIMCI SİLAH (Secondary)':'SECONDARY WEAPON (Secondary)',
    'Takipçinin genel savaşlarda öncelikli kullandığı ana silah tarzı.':'The weapon style this follower prefers in combat.','Duruma veya mesafeye göre geçiş yapacağı alternatif silah (Örn: Yay/Kılıç Kalkan).':'Alternative weapon used depending on situation or distance (e.g. bow or sword and shield).','Silah Tarzı:':'Weapon style:','Sağ El:':'Right hand:','Sol El:':'Left hand:','Çift El Türü:':'Two-handed type:',
    'Yok / Devre Dışı (None)':'None / Disabled','Kılıç':'Sword','Hançer':'Dagger','Savaş Baltası':'War axe','Gürz':'Mace','Büyü / Boş':'Spell / Empty','Büyük Kılıç':'Greatsword','Büyük Balta':'Battleaxe','Savaş Çekici':'Warhammer','Herhangi':'Any','Asli İki El':'Primary Two-Handed','Yardımcı İki El':'Secondary Two-Handed','Asli Sağ':'Primary Right','Asli Sol':'Primary Left','Yardımcı Sağ':'Secondary Right','Yardımcı Sol':'Secondary Left','KUŞAN':'EQUIP','ÇIKAR':'UNEQUIP','Takipçinin envanterinde silah bulunmuyor.':'No weapons in follower inventory.',
    'KIYAFET BÜTÜNLÜĞÜ SİSTEMİ (Armor Integrity)':'ARMOR INTEGRITY SYSTEM','Takipçinizin hangi zırh sınıfını giyeceğini belirleyin. Ağır zırh seçilirse takipçi ':'Choose which armor class your follower can wear. If heavy armor is selected, they will wear ','sadece ağır zırhları':'only heavy armor',' hafif seçilirse ':'; if light armor is selected, ','sadece hafif zırhları':'only light armor',' giyer ve tüccarlardan yalnızca seçilen türde teklif alır. Bu kural ':' and only receive offers for the selected type. This rule applies to ','kafa, gövde, eldiven ve ayakkabı':'head, body, hands and feet',' için geçerlidir.':' slots.',
    'Zırh Türü Tercihi:':'Armor type preference:','⚖️ Serbest / En Yüksek Koruma (Any)':'Any / Highest protection','🛡️ Ağır Zırh (Heavy Armor Only)':'Heavy armor only','🪶 Hafif Zırh (Light Armor Only)':'Light armor only','🧙 Kıyafet & Cübbe (Clothing / Robes Only)':'Clothing / robes only','Envanterdeki Zırhlar ve Giysiler':'Inventory armor and clothing','Ağır':'Heavy','Hafif':'Light','Kıyafet':'Clothing','ENVANTER SEKMESİ (Tüm Eşyalar & Koruma Yönetimi)':'INVENTORY TAB (All items & protection)','Otomatik Koruma:':'Automatic protection:','Oyuncunun takipçiye verdiği tüm silah ve zırhlar otomatik olarak ':'All weapons and armor given to a follower are automatically marked as ',' (Korumalı) sayılır ve satılmaz. İsterseniz buradan korumayı kaldırarak bir sonraki satışta satılabilir yapabilirsiniz. Kart seçimiyle topluca kuşanabilirsiniz.':' and will not be sold. You can remove protection here to make an item eligible for the next sale. Select cards to equip multiple items.',
    'Sadece I':'I only','Sadece U':'U only','Sadece O':'O only','Sadece P':'P only','Sadece K':'K only','Sadece L':'L only','Sadece N':'N only','Sadece M':'M only','Sadece B':'B only','Sadece H':'H only','Sadece J':'J only','Varsayılan':'Default','Menüyü açma tuşu':'Menu hotkey','Menüyü açmak istediğiniz kısayol tuşunu veya kombinasyonu seçin. Seçtiğiniz anda ayar otomatik olarak kaydedilir.':'Choose the shortcut key or combination to open the menu. Your selection is saved automatically.','Tuş Değiştir':'Change key','Shift Kombinasyonları':'Shift combinations','Ctrl Kombinasyonları':'Ctrl combinations','Alt Kombinasyonları':'Alt combinations','Tek Tuşlar':'Single keys','Fonksiyon Tuşları (F1-F12)':'Function keys (F1–F12)'
  };
  const translateNodeText = (text) => {
    const map = language()==='tr' ? trText : enText;
    const normalized = text.trim();
    const exact = map[normalized] ?? Object.entries(map).find(([key]) => key.trim() === normalized)?.[1];
    if (exact) return text.replace(text.trim(), exact);
    // Dynamic labels retain game names/numbers while translating their UI part.
    const replacements = language()==='tr'
      ? [[' gold available',' altın kullanılabilir'],[' gold',' altın'],[' selected.',' seçildi.'],['Equipment assigned.','Ekipman atandı.'],['Equipment removed.','Ekipman çıkarıldı.'],['Party scan requested.','Grup taraması istendi.'],['All notifications enabled.','Tüm bildirimler açıldı.'],['All notifications disabled.','Tüm bildirimler kapatıldı.']]
      : [[' altın kullanılabilir',' gold available'],[' altın',' gold'],[' seçildi.',' selected.'],['Ekipman atandı.','Equipment assigned.'],['Ekipman çıkarıldı.','Equipment removed.'],['Grup taraması istendi.','Party scan requested.'],['Tüm bildirimler açıldı.','All notifications enabled.'],['Tüm bildirimler kapatıldı.','All notifications disabled.']];
    return replacements.reduce((value,[from,to]) => value.split(from).join(to), text);
  };
  const localizeRenderedUI = () => {
    document.documentElement.lang = language();
    const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
    const nodes=[]; while(walker.nextNode()) nodes.push(walker.currentNode);
    nodes.forEach(node => { if(node.parentElement?.tagName !== 'SCRIPT') node.nodeValue=translateNodeText(node.nodeValue); });
    document.querySelectorAll('[title],[aria-label],[data-tooltip],[placeholder]').forEach(el => {
      ['title','aria-label','data-tooltip','placeholder'].forEach(attr => { if(el.hasAttribute(attr)) el.setAttribute(attr,translateNodeText(el.getAttribute(attr))); });
    });
  };
  const esc = (v) => String(v ?? '').replace(/[&<>"']/g, (c) => ({ '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;' }[c]));
  const pct = (v) => Math.max(0, Math.min(100, Number(v) || 0));
  const follower = () => state.followers.find((f) => Number(f.id) === Number(selectedFollower)) || state.followers[0] || null;
  const money = (n) => Number(n || 0).toLocaleString();
  const notify = (message) => { const t = document.createElement('div'); t.className = 'toast'; t.textContent = translateNodeText(message); document.body.append(t); setTimeout(() => t.remove(), 2450); };
  window.fuShowEquipPrompt = (prompt) => {
    const tr = language() === 'tr';
    const purchase = prompt?.mode === 'purchase';
    const lootUpgrade = prompt?.mode === 'loot' && Boolean(prompt?.currentStats || prompt?.candidateStats);
    if ($('equip-prompt-slot')) {
      const slot = prompt?.slotLabel;
      $('equip-prompt-slot').textContent = slot ? slot.toUpperCase() : '';
      $('equip-prompt-slot').hidden = !slot;
    }
    $('equip-prompt-title').textContent = purchase ? (tr ? 'Ekipman yükseltmesi mevcut' : 'Equipment upgrade available') : (lootUpgrade ? (tr ? 'Ceset yağması ekipman önerisi' : 'Corpse loot equipment offer') : (tr ? 'Daha iyi bir eşya bulundu' : 'A better item was found'));
    $('equip-prompt-summary').textContent = purchase
      ? tr ? `${prompt?.followerName || 'Takipçin'} yakındaki bir satıcıdan ${prompt?.itemName || 'bu eşyayı'} satın alabilir.` : `${prompt?.followerName || 'Your follower'} can buy ${prompt?.itemName || 'this item'} from a nearby seller.`
      : (lootUpgrade
        ? tr ? `${prompt?.followerName || 'Takipçin'} yağmadan ${prompt?.itemName || 'bu eşyayı'} alacak.` : `${prompt?.followerName || 'Your follower'} will receive ${prompt?.itemName || 'this item'} from the loot.`
        : tr ? `${prompt?.followerName || 'Takipçin'} ${prompt?.itemName || 'daha iyi bir eşya'} buldu.` : `${prompt?.followerName || 'Your follower'} found ${prompt?.itemName || 'a better item'}.`);
    $('equip-prompt-reason').textContent = purchase
      ? (tr ? 'Bu uyumlu yükseltme yakındaki bir satıcıda mevcut.' : 'This compatible upgrade is available from a nearby seller.')
      : (lootUpgrade ? (tr ? 'Takipçinin mevcut ekipmanını cesetten alınan eşyayla karşılaştır.' : 'Compare the follower’s current gear with the item recovered from the corpse.') : (prompt?.reason || ''));
    $('equip-prompt-vendor').hidden = !purchase;
    $('equip-prompt-vendor').textContent = purchase ? `${tr ? 'Satıcı' : 'Seller'}: ${prompt.vendorName || (tr ? 'Yakındaki tüccar' : 'Nearby merchant')}` : '';
    $('equipment-offer-comparison').hidden = !(purchase || lootUpgrade);
    $('equipment-offer-comparison').classList.toggle('loot-upgrade', lootUpgrade);
    $('equip-prompt-price-row').hidden = !purchase;
    $('equip-prompt-funds-row').hidden = !purchase;
    $('equipment-offer-comparison').querySelector('div:first-child span').textContent = lootUpgrade ? (tr ? 'Şu an kuşanılan' : 'Currently equipped') : (tr ? 'Mevcut ekipman' : 'Current equipment');
    $('equipment-offer-comparison').querySelector('div:nth-child(2) span').textContent = purchase ? (tr ? 'Satın alma önerisi' : 'Purchase offer') : (lootUpgrade ? (tr ? 'Cesetten alındı' : 'Recovered from corpse') : (tr ? 'Yağma eşyası' : 'Loot item'));
    const renderGearStats = (id, value) => {
      const parts = String(value || 'None').split(' · ');
      const name = parts.shift() || 'Unknown item';
      $(id).innerHTML = `<span class="gear-stat-name">${iconImg(iconFor(name), 'inline-icon')} ${esc(name)}</span>${parts.length ? `<span class="gear-stat-details">${esc(parts.join(' · '))}</span>` : ''}`;
    };
    renderGearStats('equip-prompt-current-stats', prompt?.currentStats);
    renderGearStats('equip-prompt-candidate-stats', prompt?.candidateStats);
    $('equip-prompt-price').textContent = purchase ? `${money(prompt.price)} ${tr ? 'altın' : 'gold'}` : '';
    $('equip-prompt-funds').textContent = purchase
      ? `${prompt?.followerName || (tr ? 'Takipçi' : 'Follower')}: ${money(prompt.spendableGold)} ${tr ? 'altın kullanılabilir' : 'gold available'}${Number(prompt.spendableGold || 0) < Number(prompt.price || 0) ? (tr ? ' — yeterli değil' : ' — not enough') : ''}`
      : '';
    $('equip-prompt-copy').textContent = purchase
      ? (tr ? 'Takipçinin kendi harcanabilir altınıyla satın alınsın mı? Yeni eşya kuşanılır ve korunur. Mevcut eşya çıkarılır, koruması kaldırılır ve satılabilir olur.' : 'Buy using this follower’s own spendable gold? The new item will be equipped and protected. Their current item will be unequipped, unprotected and available for sale.')
      : (lootUpgrade
        ? (tr ? 'Bu eşya adı yazan takipçiye verilsin, kuşanılsın ve korunsun mu? Önceki eşyasının koruması kaldırılır ve satılabilir olur.' : 'Give this item to the named follower, equip it and protect it? Their previous item will be unprotected and can be sold.')
        : (tr ? 'Bu eşya kuşanılsın ve korunsun mu? Önceki eşya satılabilir hale gelebilir.' : 'Equip and protect this item? The previous item may become available for sale.'));
    $('equip-prompt-no').textContent = purchase ? (tr ? 'HAYIR — BU EŞYAYI TEKRAR SORMA' : 'NO — NEVER ASK FOR THIS ITEM AGAIN') : (tr ? 'MEVCUDU KORU' : 'KEEP CURRENT');
    $('equip-prompt-yes').textContent = purchase ? (tr ? 'SATIN AL, KUŞAN VE KORU' : 'BUY, EQUIP & PROTECT') : (tr ? 'KUŞAN VE KORU' : 'EQUIP & PROTECT');
    $('equip-prompt').hidden = false;
  };
  window.fuHideEquipPrompt = () => { $('equip-prompt').hidden = true; };
  const resolveEquipPrompt = (accept) => {
    window.fuHideEquipPrompt();
    send({kind:'equipPromptDecision',accept});
  };
  $('equip-prompt-yes').addEventListener('click', () => resolveEquipPrompt(true));
  $('equip-prompt-no').addEventListener('click', () => resolveEquipPrompt(false));
  const iconFor = (value='') => {
    const s=String(value).toLowerCase();
    if(/poison/.test(s))return 'icon-poison';
    if(/invisibility|invisible/.test(s))return 'icon-invisibility';
    if(/magicka/.test(s))return 'icon-magicka';
    if(/stamina/.test(s))return 'icon-stamina';
    if(/health|potion|cure|alchemy/.test(s))return 'icon-health';
    if(/necklace|amulet/.test(s))return 'icon-necklace';
    if(/circlet/.test(s))return 'icon-circlet';
    if(/boot/.test(s))return 'icon-boots';
    if(/glove/.test(s))return 'icon-gloves';
    if(/helmet|helm/.test(s))return 'icon-helmet';
    if(/oil/.test(s))return 'icon-oil';
    if(/soap/.test(s))return 'icon-soap';
    if(/lantern/.test(s))return 'icon-lantern';
    if(/upgrade|better gear/.test(s))return 'icon-upgrade';
    if(/weapon|sword|dagger|axe|mace|warhammer|combat/.test(s))return 'icon-combat';
    if(/bow|bolt|arrow|ammo/.test(s))return 'icon-arrows';
    if(/armor|armour|clothing|robe|apparel|shield|outfit/.test(s))return /shield/.test(s)?'icon-shield':'icon-armor';
    if(/gold|coin|saving/.test(s))return 'icon-coin';
    if(/jewel|gem|valuable|soul|ring/.test(s))return 'icon-ring';
    if(/food|meat|bread|fruit|vegetable|stew|hunger/.test(s))return 'icon-food';
    if(/drink|water|milk|alcohol|thirst/.test(s))return 'icon-water';
    if(/book|scroll/.test(s))return 'icon-book';
    if(/ingredient|flower|plant/.test(s))return 'icon-leaf';
    if(/torch|fire/.test(s))return 'icon-fire';
    if(/craft|forge|anvil/.test(s))return 'icon-anvil';
    if(/sleep|fatigue|rest/.test(s))return 'icon-sleep';
    if(/lock|protect/.test(s))return 'icon-protected';
    if(/loot|corpse/.test(s))return 'icon-skull';
    if(/shop|trade|buy|sell|merchant/.test(s))return /sell/.test(s)?'icon-sell':'icon-cart';
    if(/bag|supply|utility|container/.test(s))return 'icon-sack';
    if(/notification|notify|alert|bell|message/.test(s))return 'icon-info';
    if(/follower|companion|party/.test(s))return 'icon-followers';
    if(/settings|system|gear/.test(s))return 'icon-settings';
    return 'icon-person';
  };
  const iconImg = (name,cls='') => `<img class="${cls}" src="assets/icons/${name}.png" alt="" aria-hidden="true">`;
  const itemLabel = (label,key=label) => `${iconImg(iconFor(`${key} ${label}`),'item-icon')}<span>${label}</span>`;
  const currentFollower = (id=selectedFollower) => state.followers.find(f=>Number(f.id)===Number(id)) || state.followers[0] || null;
  const followerPicker = (selected) => { const f=currentFollower(selected); const race=f?.portraitRace||'nord'; const portrait=f&&!f.animal?`<img class="picker-portrait" src="assets/portraits/${race}_${f.portraitFemale?'female':'male'}.jpg" alt="" onerror="this.classList.add('portrait-missing');this.removeAttribute('src')">`:'<span class="picker-portrait picker-placeholder">♟</span>'; return `<div class="follower-picker-wrap"><span>SELECT COMPANION</span><button type="button" class="follower-picker-control follower-picker-trigger" data-picker-toggle aria-expanded="${pickerOpen}">${portrait}<span class="follower-picker-name">${esc(f?.name||'No companion')}</span><span class="picker-chevron">${pickerOpen?'▲':'▼'}</span></button>${pickerOpen?`<div class="follower-picker-menu" role="listbox" aria-label="Select companion">${state.followers.map(option=>{const optionRace=option.portraitRace||'nord';const optionPortrait=!option.animal?`<img class="picker-option-portrait" src="assets/portraits/${optionRace}_${option.portraitFemale?'female':'male'}.jpg" alt="" onerror="this.classList.add('portrait-missing');this.removeAttribute('src')">`:'<span class="picker-option-portrait picker-placeholder">♟</span>';return `<button type="button" class="picker-option ${Number(option.id)===Number(selected)?'active':''}" role="option" aria-selected="${Number(option.id)===Number(selected)}" data-picker-option="${option.id}">${optionPortrait}<span><b>${esc(option.name)}</b><small>${esc(option.role||(option.animal?'Animal companion':'Companion'))}</small></span></button>`}).join('')}</div>`:''}</div>`; };
  const stat = (label, value, hint) => `<div class="stat-card"><div class="stat-label">${label}</div><div class="stat-value">${value}</div><div class="stat-hint">${hint}</div></div>`;
  const bar = (label, value, type='') => `<div><div class="bar-label"><span>${label}</span><span>${Math.round(pct(value))}%</span></div><div class="bar ${type}"><i style="width:${pct(value)}%"></i></div></div>`;
  const check = (title, key, obj, copy='') => { const values=obj==='settings'?state.settings:(follower()?.prefs||{}); return `<label class="toggle-row"><span class="toggle-copy-wrap"><span class="toggle-title">${itemLabel(title,key)}</span>${copy ? `<span class="toggle-copy">${copy}</span>` : ''}</span><span class="toggle"><input type="checkbox" data-${obj === 'settings' ? 'setting' : 'pref'}="${key}" ${values?.[key] ? 'checked' : ''}></span></label>`; };
  const globalCheck = (title,key,copy='') => {
    if (key === 'uiLanguage') return `<label class="toggle-row"><span class="toggle-copy-wrap"><span class="toggle-title">${itemLabel(title,key)}</span>${copy?`<span class="toggle-copy">${copy}</span>`:''}</span><select class="select" data-setting="uiLanguage" style="min-width:145px;height:38px;padding:4px 10px;font-size:12px"><option value="en" ${language()==='en'?'selected':''}>English</option><option value="tr" ${language()==='tr'?'selected':''}>Türkçe</option></select></label>`;
    return `<label class="toggle-row"><span class="toggle-copy-wrap"><span class="toggle-title">${itemLabel(title,key)}</span>${copy?`<span class="toggle-copy">${copy}</span>`:''}</span><span class="toggle"><input type="checkbox" data-setting="${key}" ${state.settings?.[key]?'checked':''}></span></label>`;
  };
  const renderOverview = () => {
    const humanoids=state.followers.filter(f=>!f.animal), avgNeeds=(key)=>state.followers.length?Math.round(state.followers.reduce((n,f)=>n+Number(f[key]||0),0)/state.followers.length):0;
    const avgHunger=avgNeeds('hunger'), avgThirst=avgNeeds('thirst'), avgFatigue=avgNeeds('fatigue');
    const totalGold = humanoids.reduce((n,f)=>n+Number(f.gold||0),0);
    const totalSpendable = humanoids.reduce((n,f)=>n+Number(f.spendableGold!=null?f.spendableGold:f.gold||0),0);
    const totalSaved = humanoids.reduce((n,f)=>n+Number(f.savedGold||0),0);
    $('content').innerHTML=`<div class="hero"><div><span class="eyebrow">THE PEOPLE AT YOUR SIDE</span><h1>Companions, in one place.</h1><p>Review their condition, equipment and daily routines.</p></div><button class="button secondary" data-refresh>SCAN PARTY</button></div>
      <div class="stats">${stat('Active companions',state.followers.length,'Followers and animal allies')}${stat('Party gold',money(totalGold),`${money(totalSpendable)} spendable${totalSaved>0?` · ${money(totalSaved)} saved`:''}`)}${stat('Spendable gold',money(totalSpendable),'Available for supplies & gear')}${stat('Average hunger',`${avgHunger}%`,'Across tracked companions')}${stat('Average thirst',`${avgThirst}%`,'Across tracked companions')}${stat('Average fatigue',`${avgFatigue}%`,'Across tracked companions')}</div>
      <div class="section-title">Your followers</div><div class="follower-grid">${state.followers.length?state.followers.map(f=>{const race=f.portraitRace||'nord';const portrait=!f.animal?`<img class="follower-portrait" src="assets/portraits/${race}_${f.portraitFemale?'female':'male'}.jpg" alt="${esc(race)} portrait" onerror="this.classList.add('portrait-missing');this.removeAttribute('src')">`:'';return `<article class="follower-card ${Number(f.id)===Number(selectedFollower)?'selected':''}" data-select-follower="${f.id}">${portrait}<div class="follower-card-body"><div class="follower-top"><div><div class="follower-name">${esc(f.name)}</div><div class="follower-sub">${esc(f.role|| (f.animal?'Animal companion':'Companion'))}${f.portraitRace?` · ${esc(f.portraitRace.replace('_',' '))} · ${f.portraitFemale?'Female':'Male'}`:''}</div></div><span class="role-pill">${f.animal?'ANIMAL':'FOLLOWER'}</span></div><div class="bars">${bar('Hunger',f.hunger)}${bar('Thirst',f.thirst,'thirst')}${bar('Fatigue',f.fatigue,'fatigue')}</div><div class="card-foot"><span class="follower-gold-foot" title="Spendable: ${money(f.spendableGold ?? f.gold)} | Saved: ${money(f.savedGold || 0)} | Total: ${money(f.gold)}">${iconImg(iconFor('gold'),'inline-icon')} <b>${money(f.spendableGold ?? f.gold)}</b> <small>spendable</small>${Number(f.savedGold || 0) > 0 ? ` <small class="gold-sub-detail">(${money(f.gold)} total)</small>` : ''}</span><span>Carry ${Math.round(f.carry||0)} / ${Math.round(f.capacity||0)}</span><span class="gold-actions"><button data-gold="100" data-follower="${f.id}">+100</button><button data-gold="500" data-follower="${f.id}">+500</button></span></div></div></article>`}).join(''):`<div class="empty">No active followers found. Press Scan Party to refresh.</div>`}</div>`;
  };
  const renderEquipment = () => {
    const f=follower(); if(!f){$('content').innerHTML='<div class="empty">No follower selected.</div>';return;}
    const weapons = f.weapons || [];
    const apparel = f.apparel || [];
    const p = f.prefs || {};
    const eqR = f.equippedRight || 'Boş';
    const eqL = f.equippedLeft || 'Boş';

    let bodyHtml = '';
    if (gearTab === 'weapons') {
      const pStyle = Number(p.primaryWeaponStyle ?? 0);
      const sStyle = Number(p.secondaryWeaponStyle ?? 0);
      const pRHand = Number(p.primaryRightHand ?? 0);
      const pLHand = Number(p.primaryLeftHand ?? 0);
      const p2H = Number(p.primaryTwoHandType ?? 0);
      const sRHand = Number(p.secondaryRightHand ?? 0);
      const sLHand = Number(p.secondaryLeftHand ?? 0);
      const s2H = Number(p.secondaryTwoHandType ?? 0);

      bodyHtml = `
        <div style="background:#1b1d22;border:1px solid #333;border-radius:8px;padding:14px;margin-bottom:16px">
          <div style="display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:10px;margin-bottom:12px;border-bottom:1px solid #2a2d34;padding-bottom:10px">
            <div><strong style="color:#dcdcdc;font-size:14px">⚔️ Mevcut Kuşanılan:</strong> <span style="color:#4fa3e3;margin-left:8px">Sağ El: <b>${esc(eqR)}</b></span> | <span style="color:#81c784;margin-left:6px">Sol El: <b>${esc(eqL)}</b></span></div>
          </div>
          <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:14px">
            <!-- ASLİ SİLAH TERCİHİ -->
            <div style="background:#22252c;border:1px solid #3a3e47;border-radius:6px;padding:12px">
              <div style="font-weight:600;font-size:13px;color:#f0b429;margin-bottom:6px">👑 ASLİ SİLAH (Primary)</div>
              <div style="font-size:11px;color:#aaa;margin-bottom:8px">Takipçinin genel savaşlarda öncelikli kullandığı ana silah tarzı.</div>
              <label style="display:block;margin-bottom:8px"><span style="font-size:12px">Silah Tarzı:</span>
                <select class="select" data-pref="primaryWeaponStyle" style="width:100%;height:32px;padding:2px 8px;font-size:12px;margin-top:3px">
                  <option value="0" ${pStyle===0?'selected':''}>Serbest / Otomatik (Any)</option>
                  <option value="1" ${pStyle===1?'selected':''}>🗡️ Tek Elli (One-Handed)</option>
                  <option value="2" ${pStyle===2?'selected':''}>🪓 Çift Elli (Two-Handed)</option>
                  <option value="3" ${pStyle===3?'selected':''}>🏹 Yay (Bow)</option>
                  <option value="4" ${pStyle===4?'selected':''}>🎯 Arbalet (Crossbow)</option>
                  <option value="5" ${pStyle===5?'selected':''}>🪄 Asa (Staff)</option>
                </select>
              </label>
              ${(pStyle===1 || pStyle===5)?`
                <div style="display:grid;grid-template-columns:1fr 1fr;gap:8px">
                  <label><span style="font-size:11px">Sağ El:</span>
                    <select class="select" data-pref="primaryRightHand" style="width:100%;height:30px;padding:2px 6px;font-size:11px;margin-top:2px">
                      <option value="0" ${pRHand===0?'selected':''}>Herhangi</option>
                      <option value="1" ${pRHand===1?'selected':''}>Kılıç (Sword)</option>
                      <option value="2" ${pRHand===2?'selected':''}>Hançer (Dagger)</option>
                      <option value="3" ${pRHand===3?'selected':''}>Savaş Baltası (War Axe)</option>
                      <option value="4" ${pRHand===4?'selected':''}>Gürz (Mace)</option>
                      <option value="5" ${pRHand===5?'selected':''}>Asa (Staff)</option>
                    </select>
                  </label>
                  <label><span style="font-size:11px">Sol El:</span>
                    <select class="select" data-pref="primaryLeftHand" style="width:100%;height:30px;padding:2px 6px;font-size:11px;margin-top:2px">
                      <option value="0" ${pLHand===0?'selected':''}>Herhangi</option>
                      <option value="1" ${pLHand===1?'selected':''}>Kalkan (Shield)</option>
                      <option value="2" ${pLHand===2?'selected':''}>Kılıç (Dual Sword)</option>
                      <option value="3" ${pLHand===3?'selected':''}>Hançer (Dual Dagger)</option>
                      <option value="4" ${pLHand===4?'selected':''}>Savaş Baltası (Dual Axe)</option>
                      <option value="5" ${pLHand===5?'selected':''}>Gürz (Dual Mace)</option>
                      <option value="6" ${pLHand===6?'selected':''}>Asa (Dual Staff)</option>
                      <option value="7" ${pLHand===7?'selected':''}>Büyü / Boş (Spell/Empty)</option>
                    </select>
                  </label>
                </div>`:''
              }
              ${pStyle===2?`
                <label style="display:block;margin-top:6px"><span style="font-size:11px">Çift El Türü:</span>
                  <select class="select" data-pref="primaryTwoHandType" style="width:100%;height:30px;padding:2px 6px;font-size:11px;margin-top:2px">
                    <option value="0" ${p2H===0?'selected':''}>Herhangi</option>
                    <option value="1" ${p2H===1?'selected':''}>Büyük Kılıç (Greatsword)</option>
                    <option value="2" ${p2H===2?'selected':''}>Büyük Balta (Battleaxe)</option>
                    <option value="3" ${p2H===3?'selected':''}>Savaş Çekici (Warhammer)</option>
                  </select>
                </label>`:''
              }
            </div>

            <!-- YARDIMCI SİLAH TERCİHİ -->
            <div style="background:#22252c;border:1px solid #3a3e47;border-radius:6px;padding:12px">
              <div style="font-weight:600;font-size:13px;color:#38bec9;margin-bottom:6px">🔄 YARDIMCI SİLAH (Secondary)</div>
              <div style="font-size:11px;color:#aaa;margin-bottom:8px">Duruma veya mesafeye göre geçiş yapacağı alternatif silah (Örn: Yay/Kılıç Kalkan).</div>
              <label style="display:block;margin-bottom:8px"><span style="font-size:12px">Silah Tarzı:</span>
                <select class="select" data-pref="secondaryWeaponStyle" style="width:100%;height:32px;padding:2px 8px;font-size:12px;margin-top:3px">
                  <option value="0" ${sStyle===0?'selected':''}>Yok / Devre Dışı (None)</option>
                  <option value="3" ${sStyle===3?'selected':''}>🏹 Yay (Bow)</option>
                  <option value="4" ${sStyle===4?'selected':''}>🎯 Arbalet (Crossbow)</option>
                  <option value="1" ${sStyle===1?'selected':''}>🗡️ Tek Elli (One-Handed)</option>
                  <option value="2" ${sStyle===2?'selected':''}>🪓 Çift Elli (Two-Handed)</option>
                  <option value="5" ${sStyle===5?'selected':''}>🪄 Asa (Staff)</option>
                </select>
              </label>
              ${(sStyle===1 || sStyle===5)?`
                <div style="display:grid;grid-template-columns:1fr 1fr;gap:8px">
                  <label><span style="font-size:11px">Sağ El:</span>
                    <select class="select" data-pref="secondaryRightHand" style="width:100%;height:30px;padding:2px 6px;font-size:11px;margin-top:2px">
                      <option value="0" ${sRHand===0?'selected':''}>Herhangi</option>
                      <option value="1" ${sRHand===1?'selected':''}>Kılıç (Sword)</option>
                      <option value="2" ${sRHand===2?'selected':''}>Hançer (Dagger)</option>
                      <option value="3" ${sRHand===3?'selected':''}>Savaş Baltası (War Axe)</option>
                      <option value="4" ${sRHand===4?'selected':''}>Gürz (Mace)</option>
                      <option value="5" ${sRHand===5?'selected':''}>Asa (Staff)</option>
                    </select>
                  </label>
                  <label><span style="font-size:11px">Sol El:</span>
                    <select class="select" data-pref="secondaryLeftHand" style="width:100%;height:30px;padding:2px 6px;font-size:11px;margin-top:2px">
                      <option value="0" ${sLHand===0?'selected':''}>Herhangi</option>
                      <option value="1" ${sLHand===1?'selected':''}>Kalkan (Shield)</option>
                      <option value="2" ${sLHand===2?'selected':''}>Kılıç</option>
                      <option value="3" ${sLHand===3?'selected':''}>Hançer</option>
                      <option value="4" ${sLHand===4?'selected':''}>Savaş Baltası</option>
                      <option value="5" ${sLHand===5?'selected':''}>Gürz</option>
                      <option value="6" ${sLHand===6?'selected':''}>Asa</option>
                      <option value="7" ${sLHand===7?'selected':''}>Büyü / Boş</option>
                    </select>
                  </label>
                </div>`:''
              }
              ${sStyle===2?`
                <label style="display:block;margin-top:6px"><span style="font-size:11px">Çift El Türü:</span>
                  <select class="select" data-pref="secondaryTwoHandType" style="width:100%;height:30px;padding:2px 6px;font-size:11px;margin-top:2px">
                    <option value="0" ${s2H===0?'selected':''}>Herhangi</option>
                    <option value="1" ${s2H===1?'selected':''}>Büyük Kılıç</option>
                    <option value="2" ${s2H===2?'selected':''}>Büyük Balta</option>
                    <option value="3" ${s2H===3?'selected':''}>Savaş Çekici</option>
                  </select>
                </label>`:''
              }
            </div>
          </div>
        </div>
        <div class="section-title" style="margin-bottom:8px">Envanterdeki Silahlar (${weapons.length})</div>
        <div class="gear-list equipment-gear-list">${weapons.length?weapons.map(w=>`
          <div class="gear-card ${w.worn?'chosen':''}" style="cursor:default">
            <span class="gear-name">${itemLabel(esc(w.name),'weapon')}</span>
            <span class="gear-meta">DMG ${w.damage}${w.count>1?` · ×${w.count}`:''}</span>
            ${w.slot?`<span class="tag slot-tag">${esc(w.slot)}</span>`:''}
            ${w.worn?'<span class="tag equipped-tag">EQUIPPED</span>':''}
            <div style="display:flex;flex-wrap:wrap;gap:6px;align-items:center;margin-top:8px">
              <button type="button" class="button mini-btn" data-quick-equip="${w.id}" style="font-size:11px;padding:3px 8px">KUŞAN</button>
              <button type="button" class="button secondary mini-btn" data-unequip-item="${w.id}" style="font-size:11px;padding:3px 8px">ÇIKAR</button>
              <button type="button" class="button secondary mini-btn" data-transfer-backpack="${w.id}" data-to-backpack="true" style="font-size:11px;padding:3px 8px">ÇANTAYA AT</button>
              <div style="display:flex;gap:4px;flex-wrap:wrap">
              ${w.isTwoHanded?`
                <button type="button" class="button secondary mini-btn" data-equip-hand="${w.id}" data-hand="right" data-secondary="false" style="font-size:11px;padding:3px 8px;background:${f.assignedPrimaryRight===w.id?'#b7791f':''}">Asli İki El</button>
                <button type="button" class="button secondary mini-btn" data-equip-hand="${w.id}" data-hand="right" data-secondary="true" style="font-size:11px;padding:3px 8px;background:${f.assignedSecondaryRight===w.id?'#2c7a7b':''}">Yardımcı İki El</button>
              `:`
                <button type="button" class="button secondary mini-btn" data-equip-hand="${w.id}" data-hand="right" data-secondary="false" style="font-size:11px;padding:3px 6px;background:${f.assignedPrimaryRight===w.id?'#b7791f':''}">Asli Sağ</button>
                <button type="button" class="button secondary mini-btn" data-equip-hand="${w.id}" data-hand="left" data-secondary="false" style="font-size:11px;padding:3px 6px;background:${f.assignedPrimaryLeft===w.id?'#b7791f':''}">Asli Sol</button>
                <button type="button" class="button secondary mini-btn" data-equip-hand="${w.id}" data-hand="right" data-secondary="true" style="font-size:11px;padding:3px 6px;background:${f.assignedSecondaryRight===w.id?'#2c7a7b':''}">Yardımcı Sağ</button>
                <button type="button" class="button secondary mini-btn" data-equip-hand="${w.id}" data-hand="left" data-secondary="true" style="font-size:11px;padding:3px 6px;background:${f.assignedSecondaryLeft===w.id?'#2c7a7b':''}">Yardımcı Sol</button>
              `}
              </div>
              <label class="gear-state" data-protect-wrap style="margin-left:auto;white-space:nowrap;font-size:11px"><input type="checkbox" data-protect="${w.id}" ${w.protected?'checked':''}> PROTECTED</label>
            </div>
          </div>`).join(''):'<div class="empty">Takipçinin envanterinde silah bulunmuyor.</div>'}
        </div>`;
    } else if (gearTab === 'armor') {
      const armPref = Number(p.preferredArmorType ?? 0);
      bodyHtml = `
        <div style="background:#1b1d22;border:1px solid #333;border-radius:8px;padding:14px;margin-bottom:16px">
          <div style="font-weight:600;font-size:14px;color:#e2e8f0;margin-bottom:6px">🛡️ KIYAFET BÜTÜNLÜĞÜ SİSTEMİ (Armor Integrity)</div>
          <div style="font-size:12px;color:#a0aec0;margin-bottom:12px;line-height:1.5">
            Takipçinizin hangi zırh sınıfını giyeceğini belirleyin. Ağır zırh seçilirse takipçi <b>sadece ağır zırhları</b>, hafif seçilirse <b>sadece hafif zırhları</b> giyer ve tüccarlardan yalnızca seçilen türde teklif alır. Bu kural <b>kafa, gövde, eldiven ve ayakkabı</b> için geçerlidir.
          </div>
          <label style="display:inline-flex;flex-direction:column;gap:6px;min-width:280px"><span style="font-size:12px;font-weight:600">Zırh Türü Tercihi:</span>
            <select class="select" data-pref="preferredArmorType" style="min-width:260px;height:36px;padding:4px 10px;font-size:13px">
              <option value="0" ${armPref===0?'selected':''}>⚖️ Serbest / En Yüksek Koruma (Any)</option>
              <option value="1" ${armPref===1?'selected':''}>🛡️ Ağır Zırh (Heavy Armor Only)</option>
              <option value="2" ${armPref===2?'selected':''}>🪶 Hafif Zırh (Light Armor Only)</option>
              <option value="3" ${armPref===3?'selected':''}>🧙 Kıyafet & Cübbe (Clothing / Robes Only)</option>
            </select>
          </label>
        </div>
        <div class="section-title" style="margin-bottom:8px">Envanterdeki Zırhlar ve Giysiler (${apparel.length})</div>
        <div class="gear-list equipment-gear-list">${apparel.length?apparel.map(a=>`
          <div class="gear-card ${a.worn?'chosen':''}" data-gear-card="${a.id}" role="button" tabindex="0" aria-pressed="${a.worn}">
            <span class="gear-name">${itemLabel(esc(a.name),'armor')}</span>
            <span class="gear-meta">ARMOR ${a.armor}${a.count>1?` · ×${a.count}`:''} · ${a.armorType===1?'Ağır':(a.armorType===2?'Hafif':'Kıyafet')}</span>
            ${a.slot?`<span class="tag slot-tag">${esc(a.slot)}</span>`:''}
            ${a.worn?'<span class="tag equipped-tag">EQUIPPED</span>':''}
            <div style="display:flex;gap:6px;margin-top:8px">
              <button type="button" class="button mini-btn" data-quick-equip="${a.id}" style="font-size:11px;padding:3px 8px">KUŞAN</button>
              <button type="button" class="button secondary mini-btn" data-unequip-item="${a.id}" style="font-size:11px;padding:3px 8px">ÇIKAR</button>
              <button type="button" class="button secondary mini-btn" data-transfer-backpack="${a.id}" data-to-backpack="true" style="font-size:11px;padding:3px 8px">ÇANTAYA AT</button>
            </div>
            <label class="gear-state" data-protect-wrap><input type="checkbox" data-protect="${a.id}" ${a.protected?'checked':''}> PROTECTED</label>
          </div>`).join(''):'<div class="empty">Takipçinin envanterinde giysi bulunmuyor.</div>'}
        </div>`;
    } else if (gearTab === 'backpack') {
      const bpList = f.backpack || [];
      bodyHtml = `
        <div class="toolbar equipment-toolbar">
          <div>
            <div class="section-title">${esc(f.name)}’s Hidden Backpack (${bpList.length})</div>
            <div class="notice">🎒 <b>Gizli Sırt Çantası Sistemi:</b> Yağmalanan veya çantaya konulan silah ve zırhlar burada saklanır. Skyrim\'in oyun motoru bu eşyaları görmediği için takipçiniz kafasına göre uygunsuz kıyafetleri giymez! Büyülü, temperli veya özel adlı eşyalar da (✦ büyülü, ⚒ temperli) özellikleri korunarak burada saklanır. Buradaki eşyalar tüccarlara satılabilir, KUŞAN ile doğrudan seçilebilir veya envantere alınabilir. Takipçi bırakılırsa (ölüm hariç) çantası 1 oyun günü sonra size geri verilir.</div>
          </div>
        </div>
        <div class="gear-list equipment-gear-list">${bpList.length?bpList.map(item=>`
          <div class="gear-card" style="cursor:default">
            <span class="gear-name">${itemLabel(esc(item.name),item.type)}</span>
            <span class="gear-meta">${item.type==='weapon'?`DMG ${item.damage}`:(item.type==='apparel'?`ARMOR ${item.armor}`:'')} ${item.count>1?` · ×${item.count}`:''} · ${item.weight} kg · ${item.value} altın</span>
            ${item.slot?`<span class="tag slot-tag">${esc(item.slot)}</span>`:''}
            <div style="display:flex;gap:6px;margin-top:8px">
              ${(item.type==='weapon'||item.type==='apparel')?`<button type="button" class="button mini-btn" data-quick-equip="${item.id}" style="font-size:11px;padding:3px 8px">${language()==='tr'?'KUŞAN':'EQUIP'}</button>`:''}
              <button type="button" class="button secondary mini-btn" data-transfer-backpack="${item.id}" data-to-backpack="false" ${item.custom?`data-bp-key="${esc(item.key)}"`:''} style="font-size:11px;padding:3px 8px">${language()==='tr'?'ENVANTERE AL':'TAKE OUT'}</button>
            </div>
          </div>`).join(''):'<div class="empty">Sırt çantasında eşya bulunmuyor.</div>'}
        </div>`;
    } else {
      // ENVANTER SEKMESİ (Tüm Eşyalar & Koruma Yönetimi)
      const allItems = [...apparel.map(x=>({...x, kind:'armor'})), ...weapons.map(x=>({...x, kind:'weapon'}))];
      bodyHtml = `
        <div class="toolbar equipment-toolbar">
          <div>
            <div class="section-title">${esc(f.name)}’s Complete Inventory (${allItems.length})</div>
            <div class="notice">💡 <b>Otomatik Koruma:</b> Oyuncunun takipçiye verdiği tüm silah ve zırhlar otomatik olarak <b>PROTECTED</b> (Korumalı) sayılır ve satılmaz. İsterseniz buradan korumayı kaldırarak bir sonraki satışta satılabilir yapabilirsiniz. Kart seçimiyle topluca kuşanabilirsiniz.</div>
          </div>
          <button class="button" data-equip-selected>WEAR & PROTECT SELECTED <span>(${selectedGear.size})</span></button>
        </div>
        <div class="gear-list equipment-gear-list">${allItems.length?allItems.map(item=>`
          <div class="gear-card ${selectedGear.has(Number(item.id))?'chosen':''}" data-gear-card="${item.id}" role="button" tabindex="0" aria-pressed="${selectedGear.has(Number(item.id))}">
            <span class="gear-check" aria-hidden="true">${selectedGear.has(Number(item.id))?'✓':''}</span>
            <span class="gear-name">${itemLabel(esc(item.name),item.kind)}</span>
            <span class="gear-meta">${item.kind==='weapon'?`DMG ${item.damage}`:`ARMOR ${item.armor}`}${item.count>1?` · ×${item.count}`:''}</span>
            ${item.slot?`<span class="tag slot-tag">${esc(item.slot)}</span>`:''}
            ${item.worn?'<span class="tag equipped-tag">EQUIPPED</span>':''}
            <label class="gear-state" data-protect-wrap><input type="checkbox" data-protect="${item.id}" ${item.protected?'checked':''}> PROTECTED</label>
          </div>`).join(''):'<div class="empty">Envanterde eşya bulunmuyor.</div>'}
        </div>`;
    }

    $('content').innerHTML=`<div class="hero"><div><span class="eyebrow">LOADOUT CONTROL</span><h1>Equipment</h1><p>Takipçilerinizin silah, kıyafet bütünlüğü ve envanter koruma tercihlerini yönetin.</p></div>${followerPicker(f.id)}</div>
      <div class="panel equipment-panel">
        <div class="tabs">
          <button class="tab ${gearTab==='weapons'?'active':''}" data-gear-tab="weapons">⚔️ SİLAH (${weapons.length})</button>
          <button class="tab ${gearTab==='armor'?'active':''}" data-gear-tab="armor">🛡️ ZIRH (${apparel.length})</button>
          <button class="tab ${gearTab==='backpack'?'active':''}" data-gear-tab="backpack">🎒 SIRT ÇANTASI (${(f.backpack||[]).length})</button>
          <button class="tab ${gearTab==='inventory'?'active':''}" data-gear-tab="inventory">📦 TÜM ENVANTER (${weapons.length + apparel.length})</button>
        </div>
        ${bodyHtml}
      </div>`;
  };
  const prefGroup = (title, keys, f) => `<div class="group-label">${title}</div><div class="toggle-grid">${keys.map(([key,label])=>check(label,key,f.prefs?'prefs':'prefs')).join('')}</div>`;
  const prefDefs = {
    loot:[['allowLoot','Allow looting'],['lootGold','Gold'],['lootWeapons','Weapons'],['lootArmor','Armor'],['lootClothing','Clothing & robes'],['lootPotions','Potions & poisons'],['lootFood','Food'],['lootDrinks','Drinks'],['lootArrows','Arrows & bolts'],['lootIngredients','Ingredients'],['lootSoulGems','Soul gems'],['lootGems','Gems & valuables'],['lootBooks','Books'],['lootScrolls','Scrolls']],
    economy:[]
  };
  const purchaseItems = [
    ['buyTreatmentPotions','maxTreatmentPotions','treatmentPotions','Cure Disease potion',25],
    ['buyCurePoisonPotions','maxCurePoisonPotions','curePoisonPotions','Cure Poison potion',25],
    ['buyFoodLarge','maxFoodLarge','foodLarge','Large food',25],
    ['buyFoodMedium','maxFoodMedium','foodMedium','Medium food',25],
    ['buyFoodSmall','maxFoodSmall','foodSmall','Small food',25],
    ['buyDrinkNormal','maxDrinkNormal','drinks','Water & milk',25],
    ['buyDrinkAlcohol','maxDrinkAlcohol','alcohol','Alcohol',25],
    ['buyArrows','maxArrows','arrows','Arrows',250],
    ['buyBolts','maxBolts','bolts','Crossbow bolts',250],
    ['buyPettySoulGems','maxPettySoulGems','pettySoulGems','Petty soul gems',20,'soulGemFillPetty'],
    ['buyLesserSoulGems','maxLesserSoulGems','lesserSoulGems','Lesser soul gems',20,'soulGemFillLesser'],
    ['buyCommonSoulGems','maxCommonSoulGems','commonSoulGems','Common soul gems',20,'soulGemFillCommon'],
    ['buyGreaterSoulGems','maxGreaterSoulGems','greaterSoulGems','Greater soul gems',20,'soulGemFillGreater'],
    ['buyGrandSoulGems','maxGrandSoulGems','grandSoulGems','Grand soul gems',20,'soulGemFillGrand'],
    ['buyBlackSoulGems','maxBlackSoulGems','blackSoulGems','Black soul gems',20,'soulGemFillBlack'],
    ['buyTorches','maxTorches','torches','Torches',20],
    ['buyLanternOil','maxLanternOil','lanternOil','Lantern oil',20],
    ['buySoap','maxSoap','soap','Soap',20],
    ['buyInvisibilityPotions','maxInvisibilityPotions','invisibilityPotions','Invisibility potions',25]
  ];
  const saleGroups = [
    ['WEAPONS & ARMOR',[['sellWeapons','Excess weapons'],['sellArmor','Excess armor'],['sellClothing','Clothing'],['sellJewelry','Jewelry']]],
    ['SUPPLIES',[['sellAmmo','Ammunition'],['sellPotions','Potions & poisons'],['sellFood','Food'],['sellDrinks','Drinks'],['sellIngredients','Ingredients']]],
    ['VALUABLES',[['sellSoulGems','Soul gems'],['sellGems','Gems & valuables'],['sellBooks','Books'],['sellScrolls','Scrolls']]]
  ];
  const renderEconomy = (f) => {
    const tabs=`<div class="tabs economy-tabs"><button class="tab ${economyTab==='buy'?'active':''}" data-economy-tab="buy">BUY SUPPLIES</button><button class="tab ${economyTab==='sell'?'active':''}" data-economy-tab="sell">SELL ITEMS</button><button class="tab ${economyTab==='craft'?'active':''}" data-economy-tab="craft">CRAFTING</button></div>`;
    const savings=`<section class="savings-panel"><div class="savings-heading">${iconImg('icon-coin','savings-icon')}<div><div class="section-title">Personal gold savings</div><p>Reserve part of new income. Set this follower’s target in Systems.</p></div><label class="toggle savings-switch"><input type="checkbox" data-pref="savingsEnabled" ${f.prefs.savingsEnabled?'checked':''} aria-label="Enable gold savings"></label></div><div class="savings-controls"><div class="savings-balance"><span>Saved</span><strong>${money(f.prefs.savedGold||0)} / ${money(f.prefs.savingsTarget||1000)} gold</strong></div><label><span>Income share</span><span class="percent-input"><input class="number-input" type="number" min="0" max="100" data-pref="savingsPercent" value="${Number(f.prefs.savingsPercent??30)}"><b>%</b></span></label></div></section>`;
    let panel='';
    if(economyTab==='buy'){
      const potionTiers=[
        ['HEALTH POTIONS','maxHealthPotions',[
          ['buyHealthMinor','healthMinor','Minor'],['buyHealthStandard','healthStandard','Standard'],['buyHealthPlentiful','healthPlentiful','Plentiful'],['buyHealthVigorous','healthVigorous','Vigorous'],['buyHealthExtreme','healthExtreme','Extreme'],['buyHealthUltimate','healthUltimate','Ultimate']]],
        ['MAGICKA POTIONS','maxMagickaPotions',[
          ['buyMagickaMinor','magickaMinor','Minor'],['buyMagickaStandard','magickaStandard','Standard'],['buyMagickaPlentiful','magickaPlentiful','Plentiful'],['buyMagickaVigorous','magickaVigorous','Vigorous'],['buyMagickaExtreme','magickaExtreme','Extreme'],['buyMagickaUltimate','magickaUltimate','Ultimate']]],
        ['STAMINA POTIONS','maxStaminaPotions',[
          ['buyStaminaMinor','staminaMinor','Minor'],['buyStaminaStandard','staminaStandard','Standard'],['buyStaminaPlentiful','staminaPlentiful','Plentiful'],['buyStaminaVigorous','staminaVigorous','Vigorous'],['buyStaminaExtreme','staminaExtreme','Extreme'],['buyStaminaUltimate','staminaUltimate','Ultimate']]]
      ];
      const potionSections=potionTiers.map(([title,targetKey,tiers])=>`<section class="purchase-category"><h3 class="group-label">${title}</h3>${tiers.map(([buyKey,stockKey,tier])=>`<div class="trade-row"><div class="trade-item"><span class="trade-name">${itemLabel(`${tier} ${title.split(' ')[0]} potion`,buyKey)}</span><span class="stock-badge">OWNED <b>${Number(f.stock?.[stockKey]||0)}</b></span></div><label class="trade-target"><span>Target</span><input class="number-input" type="number" min="0" max="25" data-pref="${targetKey}" value="${Number(f.prefs[targetKey]??5)}"></label><label class="toggle"><input type="checkbox" data-pref="${buyKey}" ${f.prefs[buyKey]?'checked':''} aria-label="Buy ${tier} ${title.toLowerCase()}"></label></div>`).join('')}</section>`).join('');
      const poisonTiers=[['buyPoisonHealth','poisonHealth','Health damage'],['buyPoisonMagicka','poisonMagicka','Magicka damage'],['buyPoisonStamina','poisonStamina','Stamina damage'],['buyPoisonOther','poisonOther','Other effects']];
      const poisonRows=poisonTiers.map(([buyKey,stockKey,label])=>`<div class="trade-row"><div class="trade-item"><span class="trade-name">${itemLabel(label+' poisons',buyKey)}</span><span class="stock-badge">OWNED <b>${Number(f.stock?.[stockKey]||0)}</b></span></div><label class="trade-target"><span>Target</span><input class="number-input" type="number" min="0" max="25" data-pref="maxPoisons" value="${Number(f.prefs.maxPoisons??5)}"></label><label class="toggle"><input type="checkbox" data-pref="${buyKey}" ${f.prefs[buyKey]?'checked':''} aria-label="Buy ${label} poisons"></label></div>`).join('');
      const poisonSection=`<section class="purchase-category"><h3 class="group-label">POISONS</h3><div class="sale-row"><span class="item-label">${itemLabel('Buy selected poison types','buyPoisons')}</span><label class="toggle"><input type="checkbox" data-pref="buyPoisons" ${f.prefs.buyPoisons?'checked':''} aria-label="Enable poison purchases"></label></div>${poisonRows}</section>`;
      const purchaseCategories=[
        ['treatment','Treatment potions','Cure disease and poison','icon-health',purchaseItems.slice(0,2)],
        ['food','Food & drink','Meals, water and beverages','icon-food',purchaseItems.slice(2,7)],
        ['ammo','Arrows & bolts','Ammunition supplies','icon-arrows',purchaseItems.slice(7,9)],
        ['soulgems','Soul gems','Six soul gem types','icon-ring',purchaseItems.slice(9,15)],
        ['utility','Travel & utility','Torches, oil and soap','icon-lantern',purchaseItems.slice(15,18)],
        ['special','Special potions','Invisibility potions','icon-invisibility',purchaseItems.slice(18)]
      ];
      const purchaseTooltips={
        buyFoodLarge:'Large food includes cooked meat, steak, chops, roasts, poultry and fish such as salmon. The item must also be flagged as food by the game.',
        buyFoodMedium:'Medium food includes bread, cheese, soups and stews, desserts, and other edible foods not classified as large or small.',
        buyFoodSmall:'Small food includes apples, carrots, cabbage, potatoes, leeks, tomatoes, berries, gourds, mushrooms, nuts and garlic.',
        buyDrinkNormal:'Non-alcoholic drinks include water, milk, juice, tea, cider and other drinks recognized by the mod. Waterskins are treated as non-alcoholic.',
        buyDrinkAlcohol:'Alcohol includes ale, mead, wine, brandy, beer, rum, vodka and liquor. Water or waterskins are excluded.'
      };
      const rowsFor=(items)=>items.map(([buyKey,maxKey,stockKey,label,max,fillKey])=>{
        const tip=purchaseTooltips[buyKey];
        const fillHtml=fillKey ? `<select class="select" data-pref="${fillKey}" style="margin:0 6px;padding:3px 6px;font-size:12px;height:32px;background:#1a1c23;color:#e2e8f0;border:1px solid #334155;border-radius:4px;"><option value="0" ${Number(f.prefs[fillKey]??0)===0?'selected':''}>Her İkisi (Any)</option><option value="1" ${Number(f.prefs[fillKey]??0)===1?'selected':''}>Boş (Empty)</option><option value="2" ${Number(f.prefs[fillKey]??0)===2?'selected':''}>Dolu (Full)</option></select>` : '';
        return `<div class="trade-row" ${tip?`data-tooltip="${esc(tip)}" aria-label="${esc(label)}. ${esc(tip)}"`:''}><div class="trade-item"><span class="trade-name">${itemLabel(label,buyKey||maxKey)}${tip?'<span class="info-hint" aria-hidden="true">i</span>':''}</span><span class="stock-badge">OWNED <b>${Number(f.stock?.[stockKey]||0)}</b></span></div>${fillHtml}<label class="trade-target"><span>Target stock</span><input class="number-input" type="number" min="${buyKey.includes('SoulGems')?'1':'0'}" max="${max}" data-pref="${maxKey}" value="${Number(f.prefs[maxKey]??0)}"></label><label class="toggle"><input type="checkbox" data-pref="${buyKey}" ${f.prefs[buyKey]?'checked':''} aria-label="Buy ${label}"></label></div>`;
      }).join('');
      const ingredients=[['blueMountainFlowers','Blue Mountain Flower'],['wheat','Wheat'],['redMountainFlowers','Red Mountain Flower'],['whiteCaps','White Cap'],['purpleMountainFlowers','Purple Mountain Flower'],['pineThrushEggs','Pine Thrush Egg']];
      const materialRows=ingredients.map(([key,label])=>`<div class="sale-row"><span class="item-label">${itemLabel(label,key)}</span><span class="stock-badge">OWNED <b>${Number(f.stock?.[key]||0)}</b></span></div>`).join('');
      const categoryContent={
        restorative:{title:'Health, magicka & stamina potions',icon:'icon-health',description:'Choose potion strengths and target stock.',html:potionSections},
        poisons:{title:'Poisons',icon:'icon-poison',description:'Choose poison effects and target stock.',html:poisonSection},
        ingredients:{title:'Potion ingredients',icon:'icon-leaf',description:'Bought from alchemists; harvested ingredients are shared.',html:`<section class="purchase-category ingredient-category"><p>Ingredients are bought from alchemists (wheat may also come from general merchants). Ingredients you harvest are shared among followers.</p>${materialRows}</section>`}
      };
      purchaseCategories.forEach(([id,title,description,icon,items])=>categoryContent[id]={title,icon,description,html:`<section class="purchase-category">${rowsFor(items)}</section>`});
      const buyContent=buyCategory==='overview'
        ? `<div class="buy-category-grid">${Object.entries(categoryContent).map(([id,c])=>`<button class="buy-category-card" data-buy-category="${id}">${iconImg(c.icon,'buy-category-icon')}<span class="buy-category-title">${c.title}</span><span class="buy-category-description">${c.description}</span><span class="buy-category-arrow">OPEN <b>→</b></span></button>`).join('')}</div>`
        : (()=>{const c=categoryContent[buyCategory]||categoryContent.restorative;return `<div class="buy-category-view"><div class="buy-category-view-head"><button class="button secondary" data-buy-back>← ALL CATEGORIES</button><div><span class="section-title">${iconImg(c.icon,'inline-icon')} ${c.title}</span><p>${c.description}</p></div></div><div class="trade-list buy-supplies-list">${c.html}</div></div>`;})();
      panel=`<section class="trade-panel"><div class="trade-panel-head"><div><div class="section-title">Purchase targets</div><p>Select a category to jump straight to its settings. Each enabled supply is stocked up to its target.</p></div><span class="trade-col-label">BUY</span></div>${buyContent}</section>`;
    } else if(economyTab==='sell'){
      panel=`<section class="trade-panel"><div class="trade-panel-head"><div><div class="section-title">Items to sell</div><p>Choose which categories followers may sell. Worn and protected equipment stays with them; supply limits from Purchases are kept in reserve.</p></div><span class="trade-col-label">SELL</span></div><div class="trade-list sale-grid">${saleGroups.map(([title,items])=>`<div class="sale-group"><div class="group-label">${itemLabel(title,title)}</div>${items.map(([key,label])=>`<div class="sale-row"><span class="item-label">${itemLabel(label,key)}</span><label class="toggle"><input type="checkbox" data-pref="${key}" ${f.prefs[key]?'checked':''} aria-label="${label}"></label></div>`).join('')}</div>`).join('')}</div></section>`;
    } else {
      const craftOptions=[
        ['allowCraftArrows','Craft arrows','Steel arrows: 1 Firewood + 1 Steel Ingot → 24 arrows. If no Steel Ingot is available: 1 Firewood + 1 Iron Ingot → 24 Iron Arrows.'],
        ['allowCraftBolts','Craft crossbow bolts','Steel Bolts: 1 Firewood + 1 Steel Ingot → 10 bolts.'],
        ['allowCraftTorches','Craft torches','1 Firewood + 1 Linen Wrap → 1 torch.'],
        ['allowCraftPotions','Craft potions','Healing: Blue Mountain Flower + Wheat. Magicka: Red Mountain Flower + White Cap. Stamina: Purple Mountain Flower + Pine Thrush Egg.']
      ];
      panel=`<section class="trade-panel"><div class="trade-panel-head"><div><div class="section-title">Crafting preferences</div><p>Hover over a crafting option to see its recipe. Arrows, bolts and torches require a forge within 400 game units. Potions require an alchemy lab within 400 units. Ingredients harvested by you are shared among followers.</p></div></div><div class="trade-list craft-list"><div class="trade-row"><div class="trade-item"><span class="trade-name">Ingredient stock cap</span><span class="stock-badge">PER INGREDIENT / FOLLOWER</span></div><label class="trade-target"><span>Maximum</span><input class="number-input" type="number" min="1" max="50" data-pref="maxPotionFlowers" value="${Number(f.prefs.maxPotionFlowers??30)}"></label></div>${craftOptions.map(([key,label,recipe])=>`<div class="sale-row craft-option" data-tooltip="${esc(recipe)}" aria-label="${esc(label)}. ${esc(recipe)}"><span class="item-label">${itemLabel(label,key)}</span><label class="toggle"><input type="checkbox" data-pref="${key}" ${f.prefs[key]?'checked':''} aria-label="${label}"></label></div>`).join('')}</div></section>`;
    }
    $('content').innerHTML=`<div class="hero"><div><span class="eyebrow">FOLLOWER ECONOMY</span><h1>Trade & supplies</h1><p>Configure savings, purchases, stock targets and sale categories for each companion.</p></div>${followerPicker(f.id)}</div><div class="panel economy-panel"><div class="section-title">${esc(f.name)}</div>${savings}${tabs}${panel}</div>`;
  };
  const renderPrefs = (which) => {
    const f=follower(); if(!f){$('content').innerHTML='<div class="empty">No follower selected.</div>';return;}
    if(which==='economy'){renderEconomy(f);return;}
    const title=which==='loot'?'Loot rules':'Trade & supplies';
    const keys=prefDefs[which];
    $('content').innerHTML=`<div class="hero"><div><span class="eyebrow">FOLLOWER PREFERENCES</span><h1>${title}</h1><p>Set what each companion collects, sells, buys and crafts.</p></div>${followerPicker(f.id)}</div><div class="panel"><div class="section-title">${esc(f.name)}</div><div class="toggle-grid">${keys.map(([key,label])=>check(label,key,'prefs')).join('')}</div>${which==='economy'?`<div class="group-label">STOCK LIMITS</div><div class="toggle-grid">${[['maxHealthPotions','Health potions'],['maxMagickaPotions','Magicka potions'],['maxStaminaPotions','Stamina potions'],['maxPoisons','Poisons'],['maxFoodLarge','Large food'],['maxFoodMedium','Medium food'],['maxFoodSmall','Small food'],['maxDrinkNormal','Water & milk'],['maxDrinkAlcohol','Alcohol'],['maxArrows','Arrows'],['maxBolts','Bolts'],['maxPettySoulGems','Petty soul gems'],['maxLesserSoulGems','Lesser soul gems'],['maxCommonSoulGems','Common soul gems'],['maxGreaterSoulGems','Greater soul gems'],['maxGrandSoulGems','Grand soul gems'],['maxBlackSoulGems','Black soul gems'],['maxTorches','Torches'],['maxLanternOil','Lantern oil'],['maxSoap','Soap']].map(([key,label])=>`<label class="toggle-row"><span class="toggle-title">${label}</span><input class="number-input" type="number" min="${key.includes('SoulGems')?'1':'0'}" max="${key.includes('SoulGems')?'20':'250'}" data-pref="${key}" value="${f.prefs[key]||0}"></label>`).join('')}</div>`:''}</div>`;
  };
  // Walk-to-merchant settings (TradeManager). Cooldowns are in game hours; the plugin keeps them in its own json file.
  const tradeNumber = (title,key,copy,min,max,step,fallback) => `<label class="toggle-row"><span><span class="toggle-title">${title}</span><span class="toggle-copy">${copy}</span></span><input class="number-input" style="width:78px" type="number" min="${min}" max="${max}" step="${step}" data-setting="${key}" value="${state.settings?.[key]??fallback}"></label>`;
  const renderTradeWalk = () => `<section class="panel"><div class="group-label">WALK TO MERCHANTS</div>${state.settings?.tradePluginFound===false?'<p class="notice">FollowerUnboundTrade.esp is not loaded. Followers trade from a distance as before.</p>':''}<div class="toggle-grid">${globalCheck('Walk to merchants','tradeWalk','Followers walk up to a nearby merchant before selling or buying.')}${globalCheck('Voice lines','tradeVoice','Followers and merchants greet and thank each other with the game\'s own voice lines.')}${globalCheck('Hold followers while walking','tradeHold','Stops other follower mods from teleporting a walking companion back. Leave it off unless you see teleporting.')}</div>${tradeNumber('Notice distance','tradeNoticeDistance','A merchant this close (game units) counts as nearby.',300,1500,50,1000)}${tradeNumber('Stay-near time (seconds)','tradeStaySeconds','How long a follower stays near a merchant before walking over.',2,20,1,6)}${tradeNumber('Talk time (seconds)','tradeLingerSeconds','How long they stand with the merchant after the deal.',1,10,0.5,3.5)}${tradeNumber('Wait after a deal (game hours)','tradeCooldownHours','Before the same follower visits the same merchant again after a sale or purchase.',1,72,1,6)}${tradeNumber('Wait after an empty visit (game hours)','tradeNoDealCooldownHours','Before trying the same merchant again when nothing was traded.',1,24,1,6)}</section>`;
  const renderLootWalk = () => `<section class="panel"><div class="group-label">WALK TO CORPSES</div><div class="toggle-grid">${globalCheck('Walk to corpses','lootWalk','Followers walk up to a body before looting it, then search it.')}</div>${tradeNumber('Wait before walking (seconds)','lootWalkDelay','How long after a fight or a closed corpse before followers set off.',2,30,1,10)}</section>`;
  const renderSettings = () => {
    const f=follower();
    const groups=[
      ['INTERFACE LANGUAGE',[
        ['uiLanguage','Language / Dil','Choose the language used by Follower Unbound’s Prisma interface and alerts.']
      ]],
      ['CORE SYSTEMS',[
        ['autoLoot','Automatic loot','Share eligible leftovers after you close a container.'],
        ['autoEquip','Automatic equipment upgrades','Automatically equip eligible gear received through Follower Unbound.'],
        ['protectOutfit','Protect starting outfit','Capture equipped clothing when a follower is first registered.'],
        ['combatPotions','Combat potions','Use potions when combat health or magicka is low.'],
        ['needs','Needs system','Track hunger, thirst and fatigue.'],
        ['poison','Poison system','Apply poison to compatible weapons.'],
        ['shopping','Shopping','Buy supplies from nearby merchants.'],
        ['selling','Selling','Sell excess items to nearby merchants.'],
        ['promptBetterGear','Ask before equipping looted gear','Show a prompt before equipping a better item found in loot.'],
        ['equipmentPurchaseOffers','Offer better gear from merchants','Ask before a follower buys a better weapon or armor from a merchant.'],
        ['mageRobes','Mage robe preference','Prefer robes for mage-class followers.'],
        ['spellTomes','Spell tome learning','Followers read and learn spells from spell tomes in safe places (inns, homes, towns).']
      ]],
      ['NOTIFICATIONS & ALERTS',[
        ['notifications','Enable all notifications','Master switch for all on-screen companion messages and alerts.'],
        ['notifyLoot','Looting alerts','Show messages when companions collect items or gold.'],
        ['notifyShopping','Shopping & stock alerts','Show messages for merchant purchases and low stock warnings.'],
        ['notifySelling','Trade & sales alerts','Show messages when excess items are sold to merchants.'],
        ['notifyNeeds','Needs alerts','Show messages when companions eat, drink or become starved.'],
        ['notifyCombatPotions','Combat potion & poison alerts','Show messages when potions or weapon poisons are used.'],
        ['notifyAutoEquip','Equipment upgrade alerts','Show messages when companions equip upgraded weapons or armor.']
      ]],
      ['GLOBAL LOOT CATEGORIES',[
        ['lootHostilesOnly','Hostile corpses only','Ignore neutral and friendly actors.'],
        ['lootGold','Gold'],
        ['lootWeapons','Weapons'],
        ['lootArmor','Armor'],
        ['lootArrows','Arrows & bolts'],
        ['lootPotions','Potions'],
        ['lootFood','Food'],
        ['lootIngredients','Ingredients'],
        ['lootSoulGems','Soul gems'],
        ['lootMisc','Other valuables']
      ]]
    ];
    $('content').innerHTML=`<div class="hero"><div><span class="eyebrow">GLOBAL BEHAVIOR</span><h1>Systems</h1><p>Control companion automation, notifications and global loot behavior.</p></div><div class="settings-actions">${followerPicker(f?.id)}<button class="button secondary" data-refresh>REFRESH STATE</button></div></div><div class="settings-columns">${groups.map(([title,items])=>`<section class="panel"><div class="group-label">${title}</div>${title.includes('NOTIFICATIONS')?`<div class="notification-actions"><button type="button" class="button secondary mini-btn" data-notif-bulk="true">ENABLE ALL</button><button type="button" class="button secondary mini-btn" data-notif-bulk="false">DISABLE ALL</button></div><label class="toggle-row" style="margin-bottom:12px"><span class="toggle-copy-wrap"><span class="toggle-title">${itemLabel('Notification delivery style','notification')}</span><span class="toggle-copy">Consolidate frequent events into single summary alerts to prevent stutter and screen clutter.</span></span><select class="select" data-setting="notificationMode" style="min-width:175px;height:38px;padding:4px 10px;font-size:12px"><option value="0" ${Number(state.settings.notificationMode??0)===0?'selected':''}>Batched (Toplu Özet)</option><option value="1" ${Number(state.settings.notificationMode??0)===1?'selected':''}>Detailed (Ayrıntılı)</option><option value="2" ${Number(state.settings.notificationMode??0)===2?'selected':''}>Critical Only (Sadece Kritik)</option></select></label>`:''}<div class="toggle-grid">${items.map(([key,label,copy])=>globalCheck(label,key,copy)).join('')}</div></section>`).join('')}<section class="panel"><div class="group-label">EQUIPMENT UPGRADES</div><label class="toggle-row"><span><span class="toggle-title">Gear upgrade check chance (%)</span><span class="toggle-copy">Chance (1–100%) that followers look for or offer better weapons and armor.</span></span><input class="number-input" type="number" min="1" max="100" step="1" data-setting="equipmentUpgradeChance" value="${state.settings.equipmentUpgradeChance||35}"></label></section><section class="panel"><div class="group-label">FOLLOWER GOLD SAVINGS</div>${f?`<div class="savings-setting"><div><strong>${esc(f.name)}</strong><span>Saved ${money(f.prefs.savedGold||0)} of ${money(f.prefs.savingsTarget||1000)} gold</span></div><label><span>Target gold (1–10,000)</span><input class="number-input savings-target-input" type="number" min="1" max="10000" step="1" data-pref="savingsTarget" value="${Math.max(1,Math.min(10000,Number(f.prefs.savingsTarget??1000)))}"></label></div>`:'<div class="empty">No follower selected.</div>'}</section><section class="panel"><div class="group-label">LOOT TIMING</div><label class="toggle-row"><span><span class="toggle-title">Post-combat loot delay</span><span class="toggle-copy">Seconds before followers begin looting.</span></span><input class="number-input" type="number" min="0" max="60" step="1" data-setting="lootDelay" value="${state.settings.lootDelay||0}"></label></section>${renderTradeWalk()}${renderLootWalk()}<section class="panel"><div class="group-label">MENU HOTKEY</div><div class="toggle-row" style="flex-direction:column;align-items:flex-start;gap:10px"><span><span class="toggle-title">Menüyü açma tuşu</span><span class="toggle-copy">Menüyü açmak istediğiniz kısayol tuşunu veya kombinasyonu seçin. Seçtiğiniz anda ayar otomatik olarak kaydedilir.</span></span><div style="display:flex;align-items:center;gap:10px;flex-wrap:wrap"><span id="fu-hotkey-label" style="font-size:13px;font-weight:600;padding:6px 16px;border-radius:6px;background:#181a1f;border:1px solid #4a5568;color:#ecc94b;min-width:100px;text-align:center">${state.settings.menuHotkeyText||'Shift + I'}</span><select class="select" id="fu-hotkey-select" data-hotkey-select style="min-width:220px;height:36px;padding:4px 10px;font-size:13px"><option value="">-- Tuş Değiştir --</option><optgroup label="Shift Kombinasyonları"><option value="0x17,1,Shift + I">Shift + I (Varsayılan)</option><option value="0x16,1,Shift + U">Shift + U</option><option value="0x18,1,Shift + O">Shift + O</option><option value="0x19,1,Shift + P">Shift + P</option><option value="0x25,1,Shift + K">Shift + K</option><option value="0x26,1,Shift + L">Shift + L</option><option value="0x31,1,Shift + N">Shift + N</option><option value="0x32,1,Shift + M">Shift + M</option><option value="0x30,1,Shift + B">Shift + B</option><option value="0x23,1,Shift + H">Shift + H</option><option value="0x24,1,Shift + J">Shift + J</option></optgroup><optgroup label="Ctrl Kombinasyonları"><option value="0x17,2,Ctrl + I">Ctrl + I</option><option value="0x16,2,Ctrl + U">Ctrl + U</option><option value="0x18,2,Ctrl + O">Ctrl + O</option><option value="0x19,2,Ctrl + P">Ctrl + P</option><option value="0x25,2,Ctrl + K">Ctrl + K</option><option value="0x26,2,Ctrl + L">Ctrl + L</option><option value="0x31,2,Ctrl + N">Ctrl + N</option><option value="0x32,2,Ctrl + M">Ctrl + M</option><option value="0x30,2,Ctrl + B">Ctrl + B</option><option value="0x23,2,Ctrl + H">Ctrl + H</option><option value="0x24,2,Ctrl + J">Ctrl + J</option></optgroup><optgroup label="Alt Kombinasyonları"><option value="0x17,4,Alt + I">Alt + I</option><option value="0x16,4,Alt + U">Alt + U</option><option value="0x18,4,Alt + O">Alt + O</option><option value="0x25,4,Alt + K">Alt + K</option><option value="0x26,4,Alt + L">Alt + L</option><option value="0x31,4,Alt + N">Alt + N</option></optgroup><optgroup label="Tek Tuşlar"><option value="0x17,0,I">Sadece I</option><option value="0x16,0,U">Sadece U</option><option value="0x18,0,O">Sadece O</option><option value="0x19,0,P">Sadece P</option><option value="0x25,0,K">Sadece K</option><option value="0x26,0,L">Sadece L</option><option value="0x31,0,N">Sadece N</option><option value="0x32,0,M">Sadece M</option><option value="0x30,0,B">Sadece B</option><option value="0x23,0,H">Sadece H</option><option value="0x24,0,J">Sadece J</option></optgroup><optgroup label="Fonksiyon Tuşları (F1-F12)"><option value="0x3B,0,F1">F1</option><option value="0x3C,0,F2">F2</option><option value="0x3D,0,F3">F3</option><option value="0x3E,0,F4">F4</option><option value="0x40,0,F6">F6</option><option value="0x41,0,F7">F7</option><option value="0x42,0,F8">F8</option><option value="0x43,0,F9">F9</option><option value="0x44,0,F10">F10</option><option value="0x57,0,F11">F11</option><option value="0x58,0,F12">F12</option></optgroup></select></div></div></section></div>`;
  };
  const render = () => {
    if(!state.followers.some(f=>Number(f.id)===Number(selectedFollower))) selectedFollower=state.followers[0]?.id||0;
    $('page-title').textContent=pageTitles[page];
    $('follower-count').textContent=language()==='tr'?`${state.followers.length} takipçi takip ediliyor`:`${state.followers.length} companion${state.followers.length===1?'':'s'} tracked`;
    document.querySelectorAll('.nav-item').forEach(b=>b.classList.toggle('active',b.dataset.page===page));
    if(page==='overview')renderOverview();else if(page==='equipment')renderEquipment();else if(page==='loot'||page==='economy')renderPrefs(page);else renderSettings();
    localizeRenderedUI();
    if (typeof window.fuSetLanguage === 'function') window.fuSetLanguage(language());
  };
  window.fuSetState=(next)=>{state={...state,...next};if(!selectedFollower&&state.followers.length)selectedFollower=state.followers[0].id;render();};
  $('navigation').addEventListener('click',e=>{const b=e.target.closest('[data-page]');if(!b)return;page=b.dataset.page;pickerOpen=false;render();});
  const toggleGearCard=(gear)=>{const id=Number(gear.dataset.gearCard);if(selectedGear.has(id))selectedGear.delete(id);else selectedGear.add(id);render();};
  $('content').addEventListener('change',e=>{
    const p=e.target.closest('[data-pref]'); if(p){const val=p.type==='checkbox'?p.checked:Number(p.value);send({kind:'preference',follower:Number(follower()?.id),key:p.dataset.pref,value:val});return;}
    const s=e.target.closest('[data-setting]');if(s){const value=s.dataset.setting==='uiLanguage'?s.value:(s.type==='checkbox'?s.checked:Number(s.value));if(s.dataset.setting==='uiLanguage'){state.settings.uiLanguage=value;render();}send({kind:'setting',key:s.dataset.setting,value});return;}
    const protect=e.target.closest('[data-protect]');if(protect){send({kind:'protect',follower:Number(follower()?.id),item:Number(protect.dataset.protect),value:protect.checked});return;}
    const hkSel=e.target.closest('[data-hotkey-select]');
    if(hkSel && hkSel.value){
      const [dxStr, modStr, text]=hkSel.value.split(',');
      const dx=Number(dxStr);
      const mod=Number(modStr);
      state.settings.menuHotkey=dx;
      state.settings.menuHotkeyModifier=mod;
      state.settings.menuHotkeyText=text;
      send({kind:'setHotkey',hotkey:dx,modifier:mod,text});
      const lbl=document.getElementById('fu-hotkey-label');if(lbl)lbl.textContent=text;
      notify(language()==='tr'?'Menü tuşu güncellendi: '+text:'Menu hotkey updated: '+text);
      hkSel.value='';
      return;
    }
  });
  $('content').addEventListener('click',e=>{
    const notifBulk=e.target.closest('[data-notif-bulk]');if(notifBulk){const enable=notifBulk.dataset.notifBulk==='true';send({kind:'notificationsBulk',enable});notify(enable?'All notifications enabled.':'All notifications disabled.');return;}
    const pickerToggle=e.target.closest('[data-picker-toggle]');if(pickerToggle){pickerOpen=!pickerOpen;render();return;}
    const pickerOption=e.target.closest('[data-picker-option]');if(pickerOption){selectedFollower=Number(pickerOption.dataset.pickerOption);selectedGear.clear();pickerOpen=false;render();return;}
    const tfBp=e.target.closest('[data-transfer-backpack]');
    if(tfBp){
      e.stopPropagation();
      const itemId=Number(tfBp.dataset.transferBackpack);
      const toBp=tfBp.dataset.toBackpack==='true';
      send({kind:'transferBackpack',follower:Number(follower()?.id),item:itemId,toBackpack:toBp,key:tfBp.dataset.bpKey||''});
      notify(toBp?(language()==='tr'?'Eşya sırt çantasına aktarıldı.':'Item stashed into backpack.'):(language()==='tr'?'Eşya sırt çantasından envantere alındı.':'Item retrieved from backpack.'));
      return;
    }
    const quickEquip=e.target.closest('[data-quick-equip]');if(quickEquip){e.stopPropagation();send({kind:'equipItem',follower:Number(follower()?.id),item:Number(quickEquip.dataset.quickEquip)});notify('Equipment assigned.');return;}
    const unequip=e.target.closest('[data-unequip-item]');if(unequip){e.stopPropagation();send({kind:'unequipItem',follower:Number(follower()?.id),item:Number(unequip.dataset.unequipItem)});notify('Equipment removed.');return;}
    const gear=e.target.closest('[data-gear-card]');if(gear){if(e.target.closest('[data-protect-wrap]'))return;toggleGearCard(gear);return;}
    const gold=e.target.closest('[data-gold]');if(gold){e.stopPropagation();send({kind:'gold',follower:Number(gold.dataset.follower),amount:Number(gold.dataset.gold)});notify(`${gold.dataset.gold} gold given.`);return;}
    const pick=e.target.closest('[data-select-follower]');if(pick){selectedFollower=Number(pick.dataset.selectFollower);selectedGear.clear();render();return;}
    const tab=e.target.closest('[data-gear-tab]');if(tab){gearTab=tab.dataset.gearTab;selectedGear.clear();render();return;}
    const economy=e.target.closest('[data-economy-tab]');if(economy){economyTab=economy.dataset.economyTab;if(economyTab==='buy')buyCategory='overview';render();return;}
    const buyCategoryButton=e.target.closest('[data-buy-category]');if(buyCategoryButton){buyCategory=buyCategoryButton.dataset.buyCategory;render();return;}
    if(e.target.closest('[data-buy-back]')){buyCategory='overview';render();return;}
    if(e.target.closest('[data-refresh]')){send({kind:'refresh'});notify('Party scan requested.');return;}
    if(e.target.closest('[data-equip-selected]')){if(!selectedGear.size){notify('Select one or more items first.');return;}send({kind:'equipSelected',follower:Number(follower()?.id),items:[...selectedGear]});selectedGear.clear();notify('Selected equipment applied.');return;}
    const eqHandBtn=e.target.closest('[data-equip-hand]');
    if(eqHandBtn){
      const itemId=Number(eqHandBtn.dataset.equipHand);
      const isLeft=eqHandBtn.dataset.hand==='left';
      const isSec=eqHandBtn.dataset.secondary==='true';
      const f=follower();
      const currentSlot = isSec ? (isLeft ? f?.assignedSecondaryLeft : f?.assignedSecondaryRight) : (isLeft ? f?.assignedPrimaryLeft : f?.assignedPrimaryRight);
      const isAlreadyAssigned = currentSlot === itemId;
      send({kind:'equipHand',follower:Number(f?.id),item:itemId,leftHand:isLeft,secondary:isSec});
      if(isAlreadyAssigned){
        notify(language()==='tr'
          ? `${isSec?'Yardımcı':'Asli'} ${isLeft?'sol':'sağ'} el ataması kaldırıldı.`
          : `${isSec?'Secondary':'Primary'} ${isLeft?'left':'right'} hand assignment cleared.`);
      } else {
        notify(language()==='tr'
          ? (isSec?`Yardımcı ${isLeft?'sol':'sağ'} el silahı kaydedildi (savaşta geçiş yapabilir).`:`Asli ${isLeft?'sol':'sağ'} ele kuşandı ve kaydedildi.`)
          : (isSec?`Secondary ${isLeft?'left':'right'} hand weapon assigned (may switch during combat).`:`Primary ${isLeft?'left':'right'} hand weapon equipped and assigned.`));
      }
      return;
    }
  });
  document.addEventListener('pointerdown',e=>{if(!pickerOpen||e.target.closest('.follower-picker-wrap'))return;pickerOpen=false;document.querySelectorAll('.follower-picker-menu').forEach(menu=>menu.remove());document.querySelectorAll('[data-picker-toggle]').forEach(button=>button.setAttribute('aria-expanded','false'));});
  const recipeTooltip=document.createElement('div');recipeTooltip.id='fu-recipe-tooltip';recipeTooltip.className='recipe-tooltip';recipeTooltip.setAttribute('role','tooltip');document.body.append(recipeTooltip);
  $('content').addEventListener('pointerover',e=>{const option=e.target.closest('[data-tooltip]');if(!option)return;recipeTooltip.textContent=option.dataset.tooltip;recipeTooltip.classList.add('visible');const r=option.getBoundingClientRect();const left=Math.max(12,Math.min(r.left,window.innerWidth-432));const top=(r.bottom+recipeTooltip.offsetHeight+18>window.innerHeight)?Math.max(12,r.top-recipeTooltip.offsetHeight-8):r.bottom+8;recipeTooltip.style.left=`${left}px`;recipeTooltip.style.top=`${top}px`;});
  $('content').addEventListener('pointerout',e=>{const option=e.target.closest('[data-tooltip]');if(option&&!option.contains(e.relatedTarget))recipeTooltip.classList.remove('visible');});
  $('content').addEventListener('focusin',e=>{const option=e.target.closest('[data-tooltip]');if(!option)return;recipeTooltip.textContent=option.dataset.tooltip;recipeTooltip.classList.add('visible');const r=option.getBoundingClientRect();recipeTooltip.style.left=`${Math.max(12,Math.min(r.left,window.innerWidth-432))}px`;recipeTooltip.style.top=`${Math.min(r.bottom+8,window.innerHeight-120)}px`;});
  $('content').addEventListener('focusout',e=>{if(e.target.closest('[data-tooltip]'))recipeTooltip.classList.remove('visible');});
  $('content').addEventListener('keydown',e=>{const gear=e.target.closest('[data-gear-card]');if(gear&&!e.target.closest('[data-protect-wrap]')&&(e.key==='Enter'||e.key===' ')){e.preventDefault();toggleGearCard(gear);}});
  $('refresh').addEventListener('click',()=>send({kind:'refresh'}));
  $('close').addEventListener('click',()=>send({kind:'close'}));
  window.addEventListener('keydown',e=>{if(e.key==='Escape'){e.preventDefault();if(pickerOpen){pickerOpen=false;render();return;}send({kind:'close'});}});
  render();
})();
