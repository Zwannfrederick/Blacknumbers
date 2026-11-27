# BLACK NUMBERS — Yol Haritası

> **Durum:** Yaşayan doküman · **Son güncelleme:** 2026-11-20
> Hikâye referansı: [BLACK_NUMBERS_STORY.md](BLACK_NUMBERS_STORY.md). Bu doküman hikâyeyi değiştirmez, onu oynanabilir hâle getirme sırasını belirler.

## Mantık

Hikâye dört oynanış sistemine dayanır: **sokak yarışı (Sprint, Pist, Drift + Perfect Launch)**, **araç gelişimi (Tier 1–6 + NOS)**, **polis kovalamacası (Heat 1–10 ve Özel Kademe; Speedbreaker, Pursuit Breakers ↔ Şehir Faturası)** ve **hikâye ilerlemesi (16 BLACK NUMBERS üyesi, deliller, ifşalar)**. Hikâye bu sistemleri kilit olaylarla açar:

| Hikâye olayı | Açtığı sistem |
|---|---|
| Yarış zaferleri | Yeni BN rakipleri, delil (`R2`–`R8`) |
| Kimlik ifşası (`R4`, `#08` Hakan) | Heat 6 ve Demir |
| Tarık ile barışma (`R6`, `#05` Kaan) | Tier 6 (özel basınçlı NOS dâhil) |
| Son kovalamaca | Özel Kademe |

Bu yüzden sıra **önce sistemler, sonra hikâye** şeklindedir. Her faz kendi başına oynanabilir bir sonuç bırakır. Anlatıdaki `Q-01..Q-14` ve `S-01..S-15` kararları [BLACK_NUMBERS_STORY.md](BLACK_NUMBERS_STORY.md) içinde bağlanmıştır.

**Teknik ilkeler**
- Arabalar, yarışlar, Heat seviyeleri ve BN üyeleri **veri olarak** tanımlanır (Data Asset). Yeni içerik kod yazmadan eklenir.
- Hedef donanım düşük-orta seviyedir (4 GB VRAM). Lumen, ışın izleme ve sanal gölgeler kapalıdır.
- Her sistem en az bir otomatik testle gelir (`BlackNumbers.*`).
- Görseller önce gri kutu (greybox) olarak kurulur. Sanat yönü, şehrin kimliği (Q-02) belli olunca gelir.

---

## Faz 0 — Temel ✅

| İş | Durum |
|---|---|
| UE 5.8.4 C++ proje iskeleti, düşük grafik ayarları | ✅ |
| Gece-gündüz döngüsü (sabah/akşam sis, öğlen berrak, gece soğuk ton) | ✅ Test geçiyor |
| Chaos araç, takip kamerası, klavye + gamepad kontrolleri | ✅ Test geçiyor |
| Sürüş ayarı: 0-100 ~5,5 sn, ABS, patinaj önleyici | ✅ Test ölçüyor |
| Betikle üretilen test pisti | ✅ |

## Faz 1 — İlk Sokak ve Sürüş Hissi ✅

**Amaç:** Arabayı bitmiş bir sokakta, gece ve gündüz sürmenin iyi hissettirmesi. Ayrıntılı durum ve test kaydı: [PHASE1.md](PHASE1.md).

- [x] Asfalt, beton, toprak, çim malzemeleri ve **fiziksel malzemeleri** (tutuşu gerçekten değiştirir)
- [x] Yol çizgileri, kaldırımlar, drift'e uygun virajlı bir tur, gri kutu binalar, basit dekor
- [x] Gece kendiliğinden yanan sodyum sokak lambaları, ilk karede doğru durum
- [x] Sürüş: el freniyle drift başlatma, sürdürme, kontra, tutuşa dönüş; geri vites; hıza duyarlı direksiyon
- [x] Kamera: ayarlanabilir mesafe, hıza bağlı FOV, drift'te sakin takip
- [x] Motor sesi (devir, gaz, yük, vites) ve lastik sürtünme sesi, özgün sentez
- [x] Gece kendiliğinden yanan farlar ve stop lambaları
- [x] HUD: hız, vites, devir (araç durumundan okunur)
- [x] Bağımsız oyunda (editör dışı) performans ölçümü ve kaydı *(1080p ~210 FPS, GPU ~4,3 ms)*

**Bitti sayılır:** [PHASE1.md](PHASE1.md) içindeki kabul kriterlerinin tamamı doğrulanmış olur.

## Faz 2 — Yarış Çekirdeği

**Amaç:** Üç disiplinin de tek bir yarış altyapısıyla oynanması.

- [x] Yarış altyapısı: başlangıç ışığı, kontrol kapıları, geri sayım (kontroller kilitli), bitiş, sonuç ekranı *(testli)*
- [x] **Sprint** (A'dan B'ye) ve **Pist** (tur sayımı) *(testli)*
- [x] **Drift** puanlama: kayma açısı × hız × süre, kombo, duvar ve spin cezası *(testli)*
- [x] Rakip yapay zekâ: yarış çizgisini (spline) takip eden sürücüler, beceri ayarı, sollama, sıralama *(testli)*
- [x] Yarışlar veri olarak tanımlanır (`UBNRaceDefinition`: rota, disiplin, tur, süre, yarış çizgisi, rakip becerileri) — *ödül alanları Faz 5 ile eklenecek*

**Bitti sayılır:** Aynı bölgede bir Sprint, bir Pist ve bir Drift yarışı baştan sona oynanır.
*Durum: otomatik testler geçiyor (rakipler oval pisti 2 tur kendi başına bitiriyor), 3 rakiple 1080p'de ~195 FPS. Proje sahibinin üç yarışı oynaması bekleniyor. Ayrıntı: [PHASE2.md](PHASE2.md).*

## Faz 2.5 — NFS Most Wanted Nostalji & Adrenalin Katmanı ✅

**Amaç:** Mevcut sürüş ve yarış çekirdeğine 2005 dönemi Most Wanted temposunu, hız hissini ve özgün Karamara sanat yönünü eklemek.

- [x] **NOS / Nitro (`Left Shift` / Gamepad B):** Drift, yüksek hız ve yakın geçişle (Near-Miss) dolan anlık itki, FOV + kamera geri çekilme darbesi, egzoz ışığı ve sentezlenmiş turbo/NOS sesi *(testli)*
- [x] **Speedbreaker (`X` / `Left Ctrl` / Gamepad RB):** Zamanı yavaşlatan, dönüş kararlılığını keskinleştiren ve görsel-işitsel odak efekti veren taktiksel mekanik *(testli)*
- [x] **Perfect Launch:** Yarış geri sayımında devir tutturup yeşil bölgede kalkana anlık çıkış patlaması *(testli)*
- [x] **MW Radyal Takometre HUD & Kamera Hissi:** 24 dilimli kavisli devir göstergesi, vites kutusu/redline, NOS & Speedbreaker barları, yüksek hız titremesi ve vites geçişinde egzoz patlaması *(testli)*
- [x] **Karamara Sonbahar-Kehribar Paleti & Konsept Sanat Yönü:** [ART_DIRECTION.md](ART_DIRECTION.md) + 4 özgün konsept görsel

## Faz 3 — Şehir Dikey Dilimi (Karamara) ✅

**Amaç:** Yarışların ve kovalamacaların geçeceği Karamara'nın ilk dikey dilimini (Doğu Sanayi + Merkez Bulvar + Liman Caddesi + Batı Bulvarı + Kuzey Viyadük bağlantısı) kurmak.

- [x] Yol ağı (`/Game/Maps/KaramaraSlice`): 2×2 ana şehir ızgarası, ara sokak kestirmesi, pürüzsüz eğimli yükseltilmiş Kuzey Viyadüğü rampaları (`Tools/build_karamara_map.py`) *(testli)*
- [x] Bölge düzeni: Merkez Bulvar pisti, Kuzey Viyadüğü sprinti, Doğu Sanayi konteyner drift avlusu (`3` şehir yarışı) *(testli)*
- [x] Selin'in Garajı (`ABNGarageZone`) ve 6 Pursuit Breaker kemeri
- [x] Kırılabilir dekor (`ABNBreakableProp`): bariyer, çöp kutusu ve sandıklar çarpınca parçalanıp **Şehir Hasar Kaydı'na (Şehir Faturası)** işlenir *(testli)*
- [x] Karamara sanat yönü: [ART_DIRECTION.md](ART_DIRECTION.md) — sonbahar kehribarı gündüz, sodyum-kobalt gece, `bn.Map Karamara` geçişi

**Bitti sayılır:** Faz 2'deki 3 disiplin Karamara'da oynanabilir, yükseltilmiş viyadük çıkışı ve kırılabilir dekor otomatik testle doğrulandı (`BlackNumbers.City.KaramaraSliceAndBreakables`).

## Faz 4 — Polis Kovalamacası (Heat 1–5) ✅ (Çekirdek)

**Amaç:** Standart polis baskısı. Heat 6 ve sonrası bu altyapının üstüne kurulur.

- [x] Polis araçları ve takip yapay zekâsı (`ABNPoliceController`): takip, formasyon kıstırma, tepe lambası, prosedürel 3B Wail/Yelp siren *(testli)*
- [x] Heat göstergesi ve durum makinesi (`FBNPursuitState` / `UBNPursuitSubsystem`): takip, yakalanma (`BUSTED`), kaçış (`EVADE`), soğuma (`COOLDOWN`) ve iz kaybettirme *(testli)*
- [x] Heat 1–11 kademeleri **veri olarak** (`FBNHeatTierSpec`): araç sayısı, hız, agresiflik, Heat 3+ yol barikatları; Kanon §8 gereği Heat 6+ kimlik ifşasına (`R4`) kadar kilitli *(testli)*
- [x] **Şehir Faturası (Collateral Damage) & Pursuit Breakers (`ABNPursuitBreaker`):** Yıkılabilir yapılar çarpılınca devrilip takip eden polisleri devre dışı bırakır ve oturum hasar kaydına işler *(testli)*
- [x] Polis telsiz konuşmaları ve HUD (`ABNHUD::DrawPursuit`): Heat/Busted/Cooldown barı, Karamara Merkez/Birim telsiz satırı ve kalıcı Şehir Faturası
- [x] Yarış sırasında polisin devreye girmesi (daima açık, kapatılamaz; bitiş çizgisi sonrası serbest sürüşte devam eden takip) + sivil trafik çarpışma hasarı *(testli)*

**Bitti sayılır:** Oyuncu Heat 1'den 5'e kadar kovalanabilir, Pursuit Breaker yıkabilir, ya kaçar ya yakalanır. *Otomatik testler (`BlackNumbers.Police.*`) geçiyor; test pistinde 4 Pursuit Breaker yerleştirildi.*

## Faz 5 — Garaj ve Araç Gelişimi (Tier 1–5 / Tier 6 Kilidi) ✅

**Amaç:** Yarış kazanmanın ve kovalamacadan kaçmanın arabayı geliştirmeye ve koleksiyonu büyütmeye dönüşmesi.

- [x] Para (`Cash`), itibar (`Reputation`) ve `UBNSaveGame` kayıt sistemi (`UBNProgressionSubsystem`) *(testli)*
- [x] Garaj (`ABNGarageZone` + `G` tuşu ile Canvas garaj menüsü): araç seçimi ve parça takma
- [x] Parçalar Tier 1–5 (ve `R6` sonrası açılan Tier 6): Motor, Şanzıman, Lastik, Şasi, NOS (`FBNComputedVehicleStats`). Fizik değerlerini Chaos üzerinde canlı günceller ve sürüş test sınırlarını korur *(testli)*
- [x] Üç özgün kurgusal araç (`Kurogane M-Spec`, `Valken RS-4`, `Vandal 440R`): farklı tork, tutuş, drift itkisi ve boya tonları

**Bitti sayılır:** Oyuncu kazandığı parayla parça/araç alır, fark sürüşte ölçülür ve kayıt dosyasında saklanır (`BlackNumbers.Garage.*`).

## Faz 6 — BLACK NUMBERS Listesi ve Hikâye İskeleti ✅

**Amaç:** Hikâye ilerlemesinin sistemlere bağlanması ([BLACK_NUMBERS_STORY.md](BLACK_NUMBERS_STORY.md) §5.3 kadrosu ve §10.2 kanıt zinciri).

- [x] 16 BN üyesi **veri olarak**: §5.3 boss şablonu (`FBNBossSpec` #16–#01: işlev, kişilik, sürüş tarzı, özgün araç, disiplin, verdiği delil) *(testli)*
- [x] Rütbe ilerlemesi: sıralı boss yarışları (`#16 -> #01`), `#01` için `R8` kanıt zinciri kilidi *(testli)*
- [x] Delil defteri: toplanan deliller ve ifşa kilometre taşları R1–R8 (`UBNStorySubsystem`) *(testli)*
- [x] Hikâye durum makinesi: Perde 1–5, bayraklar (`bIdentityExposed` → Heat 6, `bTarikReconciled` → Tier 6) *(testli)*
- [x] Geçici sinematikler: Slate storyboard kartları (`C03–C19`; Sequencer sinematikleri Faz 8'de)
- [x] Mekanikçinin **kademeli** tutum değişimi: `Hostile` → `Suspicious` (`R3`) → `DeepDoubt` (`R4`) → `Reconciled` (`R6`) → `Retired` (`Epilog`) *(testli)*

**Bitti sayılır:** Perde 1 ve Perde 2 baştan sona oynanır. İlk somut delil (R3) bulunur (`BlackNumbers.Story.*`).

## Faz 7 — Perde 3–5: Demir ve Son Kovalamaca ✅

Üç bağımsız doğrulanabilir alt faza bölünür. Her biri bir öncekinin üzerine kurulur.

### Faz 7A — Kimlik İfşası ve Heat 6 ✅

- [x] Kahramanın kimliğinin ifşası: `#08` Hakan Ertürk zaferiyle oynanabilir olay (`R4`, `C06`) *(testli)*
- [x] Heat 6 açılır. Demir sahaya iner, emri **CANLI** *(testli)*
- [x] Hikâye durum makinesine bağlanır (ifşa bayrağı → `SetMaxAllowedHeat(10)`) *(testli)*
- [x] Polis telsizinde Demir'in kişisel katılımı duyulur

**Kabul:** Olaydan önce Heat 6'ya çıkılamaz, sonrasında çıkılabilir (`BlackNumbers.Story.Tier6AndFinalChasePIE`).

### Faz 7B — Heat 7–10, Barışma ve Tier 6 ✅

- [x] Heat 7–10 kademeleri: giderek sofistikeleşen taktikler, emir hâlâ **CANLI** *(testli)*
- [x] Mekanikçiyle tam barışma (`R6`, `#05` Kaan, `C08`); öncesinde kademeli tutum değişimi *(testli)*
- [x] **Tier 6** parçaları yalnızca Tarık barışınca açılır *(testli)*
- [x] Tier 6 dengeli fiziği korur (sürüş testlerinin sınırları içinde kalır) *(testli)*

**Kabul:** Barışmadan önce Tier 6 satın alınamaz, sonra alınabilir (`BlackNumbers.Story.*`).

### Faz 7C — Final Yarışı, Özel Kademe ve Sonuçlar ✅

- [x] Son yarış ve `#01` Viktor Serez'in **R3–R8 delillerle** teslim edilmesi (`C09–C11`) *(testli)*
- [x] **Özel Kademe** final kovalamacası (`bn.StoryStartFinalChase`): Demir'in **"ONU BANA GETİRİN! ÖLÜ YA DA DİRİ!"** emri ve **2 alternatif kaçış rotası** (`Liman Tünelleri` / `Kuzey Geçidi Servis Yolu`) *(testli)*
- [x] Selin'in kaçış yardımı ve Epilog storyboard çekirdeği (`C14–C19` — gerçek **Şehir Faturası** ve Tarık kanon replikleri) *(testli)*

## Faz 8 — Görsel Yenileme, 2 km Karamara, Akıllı Polis/Trafik, Ses & Paketleme

- [x] **3B Varlık & Görsel Kalite Yenilemesi:** 27 prosedürel glTF 2.0 (`.glb`) 3B mesh (`Tools/bn_meshes.py`), gece/gündüz `MPC_BN_World.NightFactor` pencere ışıkları, makro asfalt su birikintileri, okunabilir Türkçe otoyol/dükkân tabelaları, temiz araç aero kitleri + LED far/fren şeritleri, 10 renk garaj boya seçici, yumuşak alfa duman/alev/kıvılcım/lastik izi VFX ve holografik yarış halkaları
- [x] **2 km × 2 km 6 Semtli Karamara & 9 Yarış:** Tek kaynaklı `UBNRoadGraphAsset` / `UBNRoadGraphSubsystem` (A\* yol bulma, 6 kanon semt, 6 Cooldown saklanma cebi, +62 m Kuzey Geçidi dağ saç tokaları, Eski Liman Köprüsü, 16 boss'un 9 benzersiz rotaya ve özgün kasa araçlara dağıtılması, dinamik yol grafı radarı & semt afişi)
- [x] **Akıllı Polis YZ, Serbest Devriyeler & Dinamik Trafik:** A\* yol takipli polis YZ, 3 organik serbest devriye, yola hizalı barikatlar, Heat 5+ Ağır Rhino SUV & Dikenli Şeritler, enkaz saçan 10 Pursuit Breaker, 20 araçlık 5 farklı kasa yol-grafı sivil trafiği
- [x] **Prosedürel Ses & Adaptif Müzik:** 14 SFX + 3 kesintisiz adaptif müzik döngüsü (`Tools/bn_audio_assets.py` + `UBNAudioSubsystem`), 5 araç sınıfına özgü motor sesi profili (`TurboI4 / SportI6 / MuscleV8 / RallyV6 / ExoticV10`) ve Ayarlar ses mikseri
- [x] **Pro Görsel Cila, Telifsiz 3B Varlıklar, Bölge Hükümdarları & Pro Mekanikler (2026-11-24):**
  - 19 Poly Haven CC0 1.0 3B glTF sokak/sanayi/dağ modeli (`Tools/fetch_cc0_models.py`), 17 Poly Haven CC0 PBR doku seti + PureSky bulut kubbesi, Khronos `CarConcept.glb` CC-BY 4.0 birleşik GrandTourer (`Draco RS-T` / `SM_BN_GrandTourer`), `Saved/ExternalVehicles/` harici araç bırak-al hattı, 6 kavisli karbon aero kiti + `SM_BN_Traffic_SUV` + `SM_BN_JumpRamp`
  - Konuma göre dinamik asfalt ıslaklığı (`MPC_BN_World.Wetness`) ve bölgesel güneş açısı/sis tonu + `BÖLGESEL OTO / ISLAK ASFALT / HAFİF YAĞMUR` (`ABNDayNightCycle` + `bn.Weather`)
  - 6 isimli semt ve **6 Bölge Hükümdarı / Bölge Hakimiyeti (`0/6 → 6/6`)**: Altın Çekirdek (`#06`), Paslı Demir (`#05`), Mara Rıhtımı (`#01`), Çelik Kuşak (`#08`), Eski Karamara (`#07`), Çam Zirvesi (`#03`) — `[M]` harita bölge katmanları, hover bilgi kartı, Hızlı Seyahat ve tek tıkla sıradaki boss'a GPS
  - Pro sürüş & yarış mekanikleri: Hava Akımı (Slipstream), Tampon Kamera + `[V]` Arka Bakış + viraj kamera yatışı, canlı Split Delta + Kişisel Rekorlar (`BestRaceTimes/Drift/Trap/Jump`), keskin viraj holografik uyarıları, 6 Hız Tuzağı + 4 Sıçrama Rampası, 3 kademeli Rakip Zorluğu (`RAHAT / SOKAK OTO-TİER / EFSANE`), Garaj Boya Yüzeyi (4) / Jant Rengi (5) / Alt Neon (5) + 3 noktalı stüdyo ışığı, ücretsiz garaj geçiş tamiri
  - Doğrulama: **43/43 test yeşil**, 1080p RTX 3050 Ti **135.0–170.6 FPS** (`0.0–0.4% >16.7 ms`, **991 MiB** net oyun VRAM), 9 canlı ekran görüntüsü incelendi
- [x] **Eski Liman Köprüsü:** Doğu çıkışındaki iki rampanın kotu ayrımda hizalandı ve Sanayi inişi kıyı platformunun üzerinde kaldı. Beş açıklığa korkuluk çarpışması ve köprü çeliğine Poly Haven CC0 kaplama eklendi.
- [x] **Araç ve arazi fiziği:** Tümseklerden gelen süspansiyon geri tepmesi zemin eğimine göre sınırlanır. Kuzey Geçidi ve Yamaç arazisi fiziksel zemin sağlar. Mara Denizi'ne düşen araç son sağlam zemine döner.
- [x] **"Sodyum" arayüz kimliği (2026-11-27):** tek vurgu rengi (sodyum turuncusu), tek font ailesi, 12° eğik butonlar; menüler, Kara Liste, Garaj, Harita, Yarış Özeti ve HUD yeniden tasarlandı. Radarda kaçış/soğuma fazında her polisin gerçek görme yarıçapı halkası. Türkçe büyük harf (`BNUITheme::UpperTr`) tüm etiketlerde. `Tools/play.sh` başlatıcısı; paketlemede `UI/Fonts` ham `.ttf` olarak kopyalanır.
- [ ] Tam Sequencer sinematikleri C01–C19 *(metin/storyboard çekirdeği + Epilog kartları Faz 6–7'de eklendi)*
- [ ] VRAM/FPS derin optimizasyon ve paketleme (Linux + Windows)

---

## Karar Bekleyenler

Anlatı ve dünya kimliğine ait `Q-01..Q-14` ve `S-01..S-15` maddelerinin tamamı 2026-11-20 tarihinde [BLACK_NUMBERS_STORY.md](BLACK_NUMBERS_STORY.md) §0, §5.3 ve §17'de karara bağlanmıştır. Yalnızca uygulama bütçesi kararı bekleyen teknik kalem aşağıdadır:

| Soru | İlgili faz | Durum / Not |
|---|---|---|
| Sivil trafik yoğunluk bütçesi (`PROPOSALS.md#sivil-trafik`) | Faz 3 (temel), Faz 4 (polise tepki) | Önerilen tavan: düşük ayarda en fazla ~20 görünür araç (~6 tam fizik); Faz 3 profillemesinde kesinleşir |

## Değişiklik Günlüğü

| Tarih | Değişiklik |
|---|---|
| 2026-11-20 | Hikâye boşlukları (Karamara şehri, 16 üye, süreklilik) kilitlendi; Faz 2.5 (NOS, Speedbreaker, Perfect Launch, MW HUD, konsept sanat) ve Şehir Faturası bağlantıları eklendi. |
| 2026-11-17 | İlk sürüm. Faz 0 tamam, Faz 1 başladı (sokak lambası sınıfı). |
| 2026-11-17 | Sahip geri bildirimi: NFS tarzı drift, daha hızlı rakipler, gün batımı geçişi yumuşatıldı, ışık önceliği uyarısı giderildi. |
| 2026-11-17 | Faz 2 ikinci parça: rakip YZ (3 rakip, beceri, sollama), sıralama, yarıştan çıkma tuşu. Sahibin oyun testi bekleniyor. |
| 2026-11-17 | Faz 2 ilk parça: yarış altyapısı, Sprint/Pist/Drift, test pistinde 3 yarış. Rakip YZ sırada. |
| 2026-11-17 | Faz 1 tamamlandı: otomatik testler + proje sahibinin sürüş testi. |
| 2026-11-17 | Faz 1 uygulandı; 3 kabul kriteri elle test bekliyor (bkz. PHASE1.md). |
| 2026-11-17 | Faz 1 kapsamı genişletildi (fiziksel malzemeler, drift, ses, HUD, performans ölçümü). Faz 7, 7A/7B/7C olarak bölündü. Şehir ve trafik önerileri eklendi. |
