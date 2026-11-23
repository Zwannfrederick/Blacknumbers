---
name: blacknumbers-ue5
description: >-
  BLACK NUMBERS (UE 5.8 C++ açık dünya sokak yarışı) projesinde kod, harita, test,
  performans veya araç fiziği işi yapılırken kullan. Projenin build/test/harita/benchmark
  komutlarını, modül yapısını, UE5 C++ kurallarını ve teslim öncesi doğrulama kapısını verir.
---

# BLACK NUMBERS — UE5 çalışma rehberi

Genel UE5 C++ desenleri için `unreal-engine-cpp-pro` skill'ini de yükle; çelişirse bu dosya ve mevcut kod kazanır.
Oyun tasarımı bağlamı: `README.md`, `docs/ROADMAP.md`, `docs/PHASE*.md`, `docs/BLACK_NUMBERS_STORY.md`, `docs/ART_DIRECTION.md`.

## Ortam

- Motor: kaynaktan derlenmiş **UE 5.8.4**, `$UE` (varsayılan `~/UnrealEngine`), Linux, `EngineAssociation` boş.
- GPU: RTX 3050 Ti 4 GB (hibrit; oyun/benchmark `prime-run` ile NVIDIA'da). VRAM bütçesi gerçek bir kısıt.
- Proje yolu Türkçe karakter içeriyor (`Masaüstü`): Unreal trace/bazı araçlar non-ASCII yolu bozar → çıktıyı `/tmp`'ye yazıp taşı (bkz. `Tools/benchmark.sh`).

## Komutlar (her zaman bunları kullan, elle UBT çağırma)

| İş | Komut |
|---|---|
| Editor build + headless otomasyon testleri | `Tools/run_tests.sh [filtre=BlackNumbers]` |
| Tek alan testi | `Tools/run_tests.sh BlackNumbers.Race` |
| Harita/doku/malzeme yeniden üretimi (editor kapalıyken) | `Tools/build_map.sh` |
| Performans (1080p, frame time, VRAM, `.utrace`) | `Tools/benchmark.sh` → `Saved/Benchmark/benchmark.md` |

Log: `Saved/Logs/BlackNumbers.log`. Build hatasında önce log'daki ilk `error` satırını oku.
`build_map.sh` editor açıkken çalışmaz (bilerek); kullanıcıya editor'ü kapatmasını söyle, `pkill` yapma.

## Kod yapısı

- Tek runtime modül `Source/BlackNumbers`, alan klasörleri: `Vehicle`, `Race`, `Police`, `Garage`, `Story`, `World`, `UI`, `Debug`, `Tests`. Yeni kod ilgili alan klasörüne; yeni modül açmadan önce sor.
- Sınıf öneki `BN` (`ABNVehiclePawn`, `UBN...`, `FBN...`). Dosya adı = tip adı (öneksiz `A/U/F` hariç).
- `bUseUnity = false`: her `.cpp` kendi include'larını eksiksiz içermeli (unity build eksik include'u gizlemez, bu iyi).
- UI **Slate** ile (UMG değil). Girdi **Enhanced Input**. Araç **Chaos Vehicles** (`ChaosWheeledVehicleMovementComponent`).
- Haritalar ve içerik **Python ile üretiliyor** (`Tools/build_*_map.py`, `bn_*.py`). `/Game/Maps/KaramaraSlice` ve `TestTrack`'i editor'de elle düzenleme; değişikliği üretici script'e yaz ki `build_map.sh` tekrar üretebilsin.
- Yeni bağımlılık modülü (`Build.cs`) veya plugin (`.uproject`) eklemeden önce sor.

## UE5 C++ kuralları (bu projede)

- `UObject*` üyeleri `UPROPERTY()` ile işaretli (`TObjectPtr<>`), yoksa GC toplar. Ham `new`/`delete` UObject için yok; `NewObject`/`CreateDefaultSubobject`/`SpawnActor`.
- `Tick` varsayılan kapalı (`PrimaryActorTick.bCanEverTick = false`); gerekiyorsa timer veya olay. Fizik ayarı `Tick`'te değil `AsyncPhysicsTick`/component ayarında.
- Fizik 1/30 sn adımla sınırlı; 30 FPS altı ağır çekim beklenen davranış — "bug" diye düzeltme, frame time'ı düzelt.
- `Cast<>` sonucu ve `GetWorld()`, `GetFirstPlayerController()` null olabilir; kontrol et. `check()` yalnızca programcı hatası için, oyun verisi için `ensure`/log.
- Ayarlanabilir değerler `UPROPERTY(EditDefaultsOnly, Category="BN|...")` ya da adlandırılmış `constexpr`; sihirli sayı yok. Debug/geliştirici komutları `Debug/` altında, `bn.` önekiyle (örnek `Debug/BNBenchmark.cpp` → `bn.Benchmark`) ve shipping build'e girmez.
- Mevcut kod `UE_LOG(LogTemp, ...)` kullanıyor; tutarlılık için öyle devam et, mesajı alan önekiyle başlat (`bn.Benchmark: ...`). Test log'unda grep edilen `BN ...` / `Expected '` formatlarını bozma (`run_tests.sh` onlara bakıyor).

## Testler

- Otomasyon testleri `Source/BlackNumbers/Tests/`, adlandırma `BlackNumbers.<Alan>.<Ad>` (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`).
- PIE gerektiren testlerde `Tests/BNPlayTestUtils.h` yardımcılarını kullan (`BNPlayTest::PlayWorld`, `FindPlayerCar`, test pisti yükleme) — yeniden yazma.
- Saf mantık (yarış ilerlemesi, drift puanı, Heat, ekonomi) için PIE'siz unit test tercih et; daha hızlı ve deterministik.
- TDD: önce başarısız test → `Tools/run_tests.sh BlackNumbers.<Alan>` kırmızı → uygulama → yeşil.

## Teslim kapısı

1. `Tools/run_tests.sh` sıfır çıkış kodu; uyarı sayısı artmadı.
2. Görsel/performans etkileyen değişiklikte `Tools/benchmark.sh` önce/sonra (ortalama + 1% low FPS, tepe VRAM) — iddia ölçümsüz yazılmaz.
3. Harita/içerik değiştiyse `Tools/build_map.sh` hatasız ve script'ler idempotent.
4. Sürüş hissi (direksiyon, drift, NOS, polis AI) değişikliği testle tam doğrulanamaz: sonucu `[TEST EDİLMEDİ — sürüş testi gerekli]` diye işaretle ve kullanıcıdan sürüş testi iste.
5. `docs/PHASE*.md` / `README.md` durum tablosu ilgili fazda güncel.
