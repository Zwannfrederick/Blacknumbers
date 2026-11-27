# BLACK NUMBERS

Tek oyunculu, açık dünya, hikâye odaklı sokak yarışı oyunu (**Karamara**). Unreal Engine 5 ve C++ ile geliştiriliyor.
Hikâye: [docs/BLACK_NUMBERS_STORY.md](docs/BLACK_NUMBERS_STORY.md) · Sanat yönü: [docs/ART_DIRECTION.md](docs/ART_DIRECTION.md) · Yol haritası: [docs/ROADMAP.md](docs/ROADMAP.md) · Faz 1: [docs/PHASE1.md](docs/PHASE1.md) · Faz 2: [docs/PHASE2.md](docs/PHASE2.md) · Öneriler: [docs/PROPOSALS.md](docs/PROPOSALS.md)

## Durum

Faz 0, Faz 1, Faz 2, **Faz 2.5 (Most Wanted Nostalji Katmanı)**, **Faz 3 (Karamara Şehir Dikey Dilimi)**, **Faz 4 (Polis Kovalamacası)**, **Faz 5 (Garaj, Ekonomi & Tier 1–6)**, **Faz 6 (16 Üyeli BLACK NUMBERS Listesi & Kanıt Defteri)** ve **Faz 7A/7B/7C (Kimlik İfşası, Tarık Barışması & Tier 6, Final & Özel Kademe Kaçış)** uygulandı. Varsayılan başlangıç haritası `/Game/Maps/KaramaraSlice`'tır.

- **Kariyer.** Açılış menüsündeki **Kariyere Başla** Black Numbers Listesi'ni açar. Her rakibin eleme ve düello sırası ayrıdır. Programlar 2, 3, 4 ve 5 yarışa çıkar. Sonuç ekranından programa dönebilir, `M` haritasından sıradaki yarışa rota çizebilirsin.
- **Araç & MW Mekanikleri:** Chaos Vehicles (`ABNVehiclePawn`), **6 kurgusal araç** (`Kurogane M-Spec`, `Valken RS-4`, `Vandal 440R`, `Tauron V6 Rally`, `Draco RS-T`, `Nocturne BN-01`), **5 özgün prosedürel motor sesi profili** (Turbo I4 / Sport I6 / Muscle V8 / Rally V6 / Exotic V10), hıza duyarlı direksiyon, kontrollü el freni drift, **NOS / Nitro**, **Speedbreaker**, **Perfect Launch**, yumuşak alfa duman/alev/kıvılcım/lastik izi VFX ve 10 renk canlı boya atölyesi.
- **Slate Yarış & Hikâye Menüleri (`ESC` / `G` / `B`):** Doğrudan **Yarışlar**, **Selin'in Garajı (Tier 1–6, 6 araç & 10 renk canlı boya seçici)**, **16'lık BLACK NUMBERS Listesi & R1–R8 Kanıtlar**, **Ayarlar (Grafik, Sivil Trafik, Motor/Müzik/SFX ses mikseri)** ve **Kontroller** sekmeleri fareyle kullanılabilir; oyun içi dinamik yol grafı radarı, semt giriş afişi ve 3B holografik kapı/kaçış işaretçileri eşlik eder.
- **2 km × 2 km Karamara Şehri (6 Semt, 9 Yarış) & Akıllı Polis/Trafik:** `UBNRoadGraphAsset` (`DA_KaramaraRoadGraph`) üzerine kurulu 6 kanon semt (*Merkez Bulvar & Çarşı*, *Doğu Sanayi*, *Eski Liman + Eski Liman Köprüsü*, *Kuzey Viyadüğü & O-5 Çevre Yolu*, *Yamaç Mahallesi + Selin'in Garajı*, *Kuzey Geçidi & Seyir Tepesi* +62 m), 27 prosedürel 3B glTF varlık, gece-gündüz pencere ışıkları, 20 araçlık 5 farklı kasa dinamik sivil trafik, A\* yol takipli polis + serbest devriyeler + Heat 5+ Ağır Rhino SUV ve Dikenli Şeritler, 6 Cooldown saklanma noktası (`3.5x`), 10 parçalanabilir Pursuit Breaker ve 14 SFX + 3 adaptif müzik döngüsü.
- **Konsept sanatlar ([docs/ART_DIRECTION.md](docs/ART_DIRECTION.md)):** [Kurogane M-Spec](docs/images/concept/hero_car_kurogane.png) · [Nocturne BN-01](docs/images/concept/boss01_nocturne.png) · [Karamara Kovalamacası](docs/images/concept/karamara_pursuit.png) · [Tarık'ın Tier 6 Atölyesi](docs/images/concept/tarik_garage_tier6.png)

| Yarış | Tür | Başlangıç ışığı |
|---|---|---|
| Oval Circuit | Pist, 2 tur | Oval caddenin iç çiminde, başlangıç çizgisinin yanında |
| Runway Sprint | Sprint | Başlangıç düzlüğünün sağında |
| Skidpad Drift | 60 sn drift puanı | Kayma pistinin köşesinde |

Fizik 30 FPS'in altında ağır çekime düşer (Unreal fizik adımını 1/30 sn ile sınırlar). İlk açılışta shader derlemesi bitmeden (sağ alttaki sayaç) sürme; araç hem gri görünür hem süzülür.

Araba gövdesi şimdilik motorun araç şablonundaki spor araba (`Content/Vehicles/SportsCar`). Kendi arabalarımız gelince değişecek.

## Kontroller

| Eylem | Klavye | Gamepad |
|---|---|---|
| Gaz / Geri sayım Launch devri | W / Yukarı | Sağ tetik |
| Fren / geri vites (dururken basılı tut) | S / Aşağı | Sol tetik |
| Direksiyon | A, D / Sol, Sağ | Sol çubuk |
| El freni (hızlıyken çek: drift başlar) | Boşluk | A (alt tuş) |
| NOS / Nitro | Sol/Sağ Shift | B (sağ tuş) |
| Speedbreaker (zaman yavaşlatma) | X / Sol Ctrl | RB (sağ omuz) |
| Polis kovalamacası başlat / Heat artır | P | D-Pad Yukarı |
| Garajı aç / kapat | G | D-Pad Aşağı |
| BLACK NUMBERS 16'lık Liste & Kanıtlar | B | D-Pad Sol |
| Ana Menü / Hikâye Kartı Geç | ESC | Start |
| Kamera mesafesi (yakın/uzak) | C | Sağ çubuğa bas |
| Arabayı düzelt | R | Y (üst tuş) |
| Yarıştan çık | Backspace | View (sol menü tuşu) |

**Drift (NFS tarzı):** 30 km/s üstünde direksiyonu kır ve Space'e dokun. Direksiyonu virajın içine doğru tuttukça drift sürer, ne kadar kırarsan açı o kadar büyür. Gaz hızı korur. Direksiyonu bırakınca ya da kontra kırınca araç toparlanıp hızlanır.

## Test Haritası

`/Game/Maps/TestTrack` elle düzenlenmez, `Tools/build_test_map.py` ile üretilir. Başlangıçta araba asfalt düzlükte, +X yönüne bakar:

- **Sol (-Y):** slalom direkleri, sonra iki saç tokası virajlı oval cadde (kaldırım, lamba, bina, bariyer)
- **Sağ (+Y):** rampalar, toprak şerit, çim şerit
- **Düzlüğün sonu (+X):** asfalt kayma pisti (serbest drift alanı)

## Kurulum (Linux)

1. https://www.unrealengine.com/linux adresinden **Linux_Unreal_Engine_5.x.zip** indir ve aç:
   ```bash
   mkdir -p ~/UnrealEngine
   unzip -q ~/İndirilenler/Linux_Unreal_Engine_*.zip -d ~/UnrealEngine
   ```
2. Motor başka bir yerdeyse `export UE=/motor/kökü` (içinde `Engine/` olan dizin). Betikler varsayılan olarak `~/UnrealEngine` kullanır.

## Komutlar

| İş | Komut |
|---|---|
| Editörü aç (NVIDIA ile) | `prime-run ~/UnrealEngine/Engine/Binaries/Linux/UnrealEditor "$PWD/BlackNumbers.uproject"` |
| Oyna (derler, editörsüz tam ekran) | `Tools/play.sh` (pencerede: `Tools/play.sh -windowed -ResX=1600 -ResY=900`) |
| Derle + tüm testler | `Tools/run_tests.sh` (tek test: `Tools/run_tests.sh BlackNumbers.Vehicle.Drift`) |
| Test haritasını ve malzemeleri yeniden üret | `Tools/build_map.sh` (editör kapalıyken) |
| Performans ölçümü (1080p, editör dışı) | `Tools/benchmark.sh` → `Saved/Benchmark/` |

Uygulama yolunu her zaman tam ver (`$PWD/...`): Unreal göreli yolu motor klasörüne göre çözer.

Oyun içi konsol: `bn.Map Karamara` / `bn.Map TestTrack` harita değiştirir, `bn.SetTime 21.5` saati değiştirir, `bn.Pursuit 3` polis kovalamacası başlatır (`bn.PursuitStop` durdurur), `bn.StorySkipTo 8` hikâyeyi seçili boss rütbesine sarar, `bn.StoryStartFinalChase` Perde 5 Özel Kademe son kovalamacayı başlatır (`bn.StoryReset` sıfırlar), `bn.AddCash 20000` nakit ekler, `bn.UnlockTier6 1` Tier 6 kilidini açar, `bn.Benchmark` performans ölçümünü başlatır.

## Gece-Gündüz Döngüsü Kurulumu (yeni bir haritada)

1. Seviyeye şunları ekle: **Directional Light** (Güneş), ikinci bir **Directional Light** (Ay), **Sky Atmosphere**, **Sky Light**, **Exponential Height Fog** ve **Post Process Volume** (*Infinite Extent / Unbound* açık).
2. Tüm ışıkların Mobility ayarını **Movable** yap.
3. `BNDayNightCycle` aktörünü seviyeye sürükle ve *Scene* kategorisindeki alanlara yukarıdaki aktörleri bağla.
4. *Time → TimeOfDay* ile saati, *DayLengthMinutes* ile bir günün gerçek dakika cinsinden süresini ayarla (varsayılan 24 dk). *Look → Keys* saat dilimlerinin renklerini belirler.
5. `BNStreetLamp` aktörleri döngüyü kendileri bulur; gece yanar, gündüz söner.
