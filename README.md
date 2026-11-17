# BLACK NUMBERS

Tek oyunculu, açık dünya, hikâye odaklı sokak yarışı oyunu. Unreal Engine 5 ve C++ ile geliştiriliyor.
Hikâye referansı: [docs/BLACK_NUMBERS_STORY.md](docs/BLACK_NUMBERS_STORY.md) · Yol haritası: [docs/ROADMAP.md](docs/ROADMAP.md)

## Durum

| Sistem | Durum |
|---|---|
| Proje iskeleti (uproject, modül, hedefler) | UE 5.8.4 ile derleniyor |
| Düşük grafik ayarları (Lumen/RT kapalı, VSM kapalı, FXAA, Low scalability) | Yazıldı, ekranda henüz bakılmadı |
| Gece-gündüz döngüsü (`ABNDayNightCycle`) | Test geçiyor (`BlackNumbers.World.TimeOfDay`) |
| Milestone 1: araç (`ABNVehiclePawn`), takip kamerası, kontroller, test haritası | Test geçiyor (`BlackNumbers.Vehicle.DrivesForward`: 0-100 km/s ~5,5 sn, düz zeminde zıplama yok, virajda kayma sınırı) |

Fizik 30 FPS'in altında ağır çekime düşer (Unreal fizik adımını 1/30 sn ile sınırlar). İlk açılışta shader derlemesi bitmeden (sağ alttaki sayaç) sürme; araç hem gri görünür hem süzülür.

Araba gövdesi şimdilik motorun araç şablonundaki spor araba (`Content/Vehicles/SportsCar`). Kendi arabalarımız gelince değişecek.

## Kontroller

| Eylem | Klavye | Gamepad |
|---|---|---|
| Gaz | W / Yukarı | Sağ tetik |
| Fren / geri vites (dururken basılı tut) | S / Aşağı | Sol tetik |
| Direksiyon | A, D / Sol, Sağ | Sol çubuk |
| El freni | Boşluk | A (alt tuş) |
| Arabayı düzelt | R | Y (üst tuş) |

## Kurulum (Linux)

1. https://www.unrealengine.com/linux adresinden Epic hesabıyla **Linux_Unreal_Engine_5.x.zip** dosyasını indir.
2. Zip'i aç:
   ```bash
   mkdir -p ~/UnrealEngine
   unzip -q ~/İndirilenler/Linux_Unreal_Engine_*.zip -d ~/UnrealEngine
   find ~/UnrealEngine -type f -name UnrealEditor -path '*Binaries/Linux*'
   ```
3. `UE` değişkenine motor kökünü ata. Bu, `Engine/` klasörünü içeren dizindir:
   ```bash
   export UE=~/UnrealEngine   # find çıktısına göre düzelt
   ```

## Derleme ve Çalıştırma

```bash
"$UE/Engine/Build/BatchFiles/Linux/Build.sh" BlackNumbersEditor Linux Development \
  -Project="$PWD/BlackNumbers.uproject" -WaitMutex
"$UE/Engine/Binaries/Linux/UnrealEditor" "$PWD/BlackNumbers.uproject"
```

## Testler

```bash
"$UE/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PWD/BlackNumbers.uproject" \
  -ExecCmds="Automation RunTests BlackNumbers; Quit" -unattended -nullrhi -nosplash -log
```

Hibrit ekran kartlı dizüstünde editörü NVIDIA ile aç: `prime-run "$UE/Engine/Binaries/Linux/UnrealEditor" ...`

## Test Haritası

`/Game/Maps/TestTrack` elle düzenlenmez, betikle üretilir: 400 m zemin, iki rampa, slalom direkleri ve gece-gündüz ışık düzeni. Betiği değiştirip yeniden üret:

```bash
"$UE/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PWD/BlackNumbers.uproject" \
  -run=pythonscript -script="$PWD/Tools/build_test_map.py" -unattended -nullrhi
```

## Gece-Gündüz Döngüsü Kurulumu (yeni bir haritada)

1. Seviyeye şunları ekle: **Directional Light** (Güneş), ikinci bir **Directional Light** (Ay), **Sky Atmosphere**, **Sky Light**, **Exponential Height Fog** ve **Post Process Volume** (*Infinite Extent / Unbound* açık).
2. Tüm ışıkların Mobility ayarını **Movable** yap.
3. `BNDayNightCycle` aktörünü seviyeye sürükle ve *Scene* kategorisindeki alanlara yukarıdaki aktörleri bağla.
4. *Time → TimeOfDay* ile saati, *DayLengthMinutes* ile bir günün gerçek dakika cinsinden süresini ayarla (varsayılan 24 dk). *Look → Keys* saat dilimlerinin renklerini belirler.
5. Oyun içi konsolda `bn.SetTime 21.5` saati anında değiştirir.

Sokak lambaları gibi nesneler `OnNightChanged` olayına bağlanmalı, bağlandıkları anda da `IsNight()` değerini bir kez okumalıdır.
