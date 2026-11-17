# BLACK NUMBERS

Tek oyunculu, açık dünya, hikâye odaklı sokak yarışı oyunu. Unreal Engine 5 ve C++ ile geliştiriliyor.
Hikâye: [docs/BLACK_NUMBERS_STORY.md](docs/BLACK_NUMBERS_STORY.md) · Yol haritası: [docs/ROADMAP.md](docs/ROADMAP.md) · Faz 1 durumu ve test kaydı: [docs/PHASE1.md](docs/PHASE1.md) · Öneriler: [docs/PROPOSALS.md](docs/PROPOSALS.md)

## Durum

Faz 0 ve Faz 1 tamam ([docs/PHASE1.md](docs/PHASE1.md)). Faz 2 sürüyor: yarışlar oynanabilir, rakip yapay zekâ sırada.

- **Araç:** Chaos Vehicles (`ABNVehiclePawn`), 0-100 km/s ~5,5 sn, ABS, patinaj önleyici, el freniyle drift, geri vites sınırı 35 km/s
- **Dünya:** gece-gündüz döngüsü, gece yanan sodyum lambalar, asfalt/beton/toprak/çim yüzeyleri (tutuşu gerçekten değiştirir)
- **Arayüz ve ses:** hız/vites/devir göstergesi, devre ve gaza bağlı özgün motor sesi, lastik sürtünme sesi
- **Yarışlar:** mavi ışık sütununa sür → geri sayım → yarış. Sıradaki kapı turuncu iki dikmeyle gösterilir.

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
| Gaz | W / Yukarı | Sağ tetik |
| Fren / geri vites (dururken basılı tut) | S / Aşağı | Sol tetik |
| Direksiyon | A, D / Sol, Sağ | Sol çubuk |
| El freni (hızlıyken çek: drift başlar) | Boşluk | A (alt tuş) |
| Kamera mesafesi (yakın/uzak) | C | Sağ çubuğa bas |
| Arabayı düzelt | R | Y (üst tuş) |

**Drift:** 30 km/s üstünde direksiyonu kır, Space'e bas, bırak. Kayma yönüne kontra kırdıkça drift sürer. Direksiyonu düzeltip gaza basınca araç toparlanır ve hızlanır.

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
| Derle + tüm testler | `Tools/run_tests.sh` (tek test: `Tools/run_tests.sh BlackNumbers.Vehicle.Drift`) |
| Test haritasını ve malzemeleri yeniden üret | `Tools/build_map.sh` (editör kapalıyken) |
| Performans ölçümü (1080p, editör dışı) | `Tools/benchmark.sh` → `Saved/Benchmark/` |

Uygulama yolunu her zaman tam ver (`$PWD/...`): Unreal göreli yolu motor klasörüne göre çözer.

Oyun içi konsol: `bn.SetTime 21.5` saati değiştirir, `bn.Benchmark` performans ölçümünü başlatır.

## Gece-Gündüz Döngüsü Kurulumu (yeni bir haritada)

1. Seviyeye şunları ekle: **Directional Light** (Güneş), ikinci bir **Directional Light** (Ay), **Sky Atmosphere**, **Sky Light**, **Exponential Height Fog** ve **Post Process Volume** (*Infinite Extent / Unbound* açık).
2. Tüm ışıkların Mobility ayarını **Movable** yap.
3. `BNDayNightCycle` aktörünü seviyeye sürükle ve *Scene* kategorisindeki alanlara yukarıdaki aktörleri bağla.
4. *Time → TimeOfDay* ile saati, *DayLengthMinutes* ile bir günün gerçek dakika cinsinden süresini ayarla (varsayılan 24 dk). *Look → Keys* saat dilimlerinin renklerini belirler.
5. `BNStreetLamp` aktörleri döngüyü kendileri bulur; gece yanar, gündüz söner.
