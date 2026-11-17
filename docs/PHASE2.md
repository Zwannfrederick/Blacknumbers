# Faz 2 — Yarış Çekirdeği: Durum ve Test Kaydı

**Durum:** Uygulandı. Otomatik testlerin hepsi geçiyor (19/19) ve performans ölçüldü. Proje sahibinin üç yarışı oynaması bekleniyor.

## Kabul Kriterleri

| Kriter | Durum | Kanıt |
|---|---|---|
| Başlangıç ışığı, geri sayım (kontroller kilitli), kapılar, bitiş, sonuç ekranı | ✅ otomatik | `Race.CircuitByGates`, `Race.SprintFromBeacon` |
| Sprint ve Pist (tur sayımı, sırasız kapı sayılmaz) | ✅ otomatik | `Race.Progress`, `Race.CircuitByGates` |
| Drift puanı (açı × hız × süre, kombo, çarpma/spin cezası) | ✅ otomatik | `Race.DriftScoring`, `Race.DriftScores` |
| Rakipler yarış çizgisini sürer, virajları alır, yarışı bitirir | ✅ otomatik | `Race.RivalsFinishCircuit`: 3 rakip oval pisti 2 tur, kaza ve yeniden konumlanma olmadan bitirir |
| Beceri ayarı | ✅ otomatik | `Race.RivalSafeSpeed`: viraj hızı = √(tutuş·g·R) |
| Sıralama (POS x/y, bitişte 1ST/2ND…) | ✅ otomatik | `Race.SprintFromBeacon`, `Race.RivalsFinishCircuit` |
| Yarıştan çıkma (Backspace / View) | ✅ otomatik | `Race.AbandonEndsRace` |
| Üç yarış baştan sona **oynanır** | ⏳ proje sahibi | Elle test, aşağıda |
| 1080p'de 60 FPS, rakiplerle birlikte | ✅ ölçüldü | 3 rakipli `circuit_race_dusk`: 195 FPS, p99 8,3 ms, 16,7 ms'yi aşan kare %0. Aşağıya bakın. |

## Elle Test Talimatları

1. Editörü yeniden aç (yeni kod ve harita yüklenir), Play.
2. **Oval Circuit:** iç çimdeki mavi sütuna sür. 3 rakip ızgarada yanında ve arkanda. GO'dan sonra 2 tur at.
   - Bakılacaklar: rakipler yolda kalıyor mu, virajları makul hızda alıyor mu, yavaşı solluyor mu, sana çarpıyor mu? POS doğru mu?
3. **Runway Sprint:** düzlüğün sağındaki sütun. Bitişte rakipler kayma pistinde durur.
4. **Skidpad Drift:** rakipsiz, 60 sn puan.
5. Herhangi bir yarışta **Backspace**: "RACE ABANDONED", 6 sn sonra rakipler kaybolur.

## Performans (2026-11-17, RTX 3050 Ti Laptop, 1080p, Vulkan, `Tools/benchmark.sh`)

Ölçüm sırasında aynı GPU'da Unreal Editor de açıktı. Sonuçlar bu yüzden temkinli (en kötü durum) kabul edilmeli. VRAM tepe değeri (1856 MiB, tüm cihaz) editörün yaklaşık 960 MiB'ını da içeriyor. Oyunun kendisi Faz 1'deki gibi ~900 MiB.

| Senaryo | Ort. FPS | p99 ms | >16,7 ms | Oyun ms | GPU ms |
|---|---|---|---|---|---|
| street_day | 209,9 | 8,51 | %0,1 | 2,69 | 4,22 |
| street_night | 177,5 | 11,24 | %0,2 | 2,64 | 5,17 |
| hairpin_dusk | 198,0 | 8,88 | %0 | 2,59 | 4,62 |
| runway_drive_night | 221,1 | 7,46 | %0 | 2,84 | 4,10 |
| circuit_race_dusk (**3 rakip**) | 195,2 | 8,32 | %0 | 3,42 | 4,66 |

3 rakip oyun iş parçacığına kare başına ~0,8 ms ekliyor (2,6 → 3,4 ms). 60 FPS bütçesi 16,7 ms, yani bol pay var. Ekran görüntüsünde rakipler düzlüğün sonunda, HUD `POS 4/4` gösteriyor.

## Rakip Yapay Zekâ

`ABNRivalController` (`Race/BNRivalController.*`) her karede rakibin aracını sürer:

- **Direksiyon:** pure pursuit (0,6 sn ileriye nişan) + çizgiden sapmaya Stanley düzeltmesi. Hıza bağlı direksiyon eğrisi hesaba katılır.
- **Hız:** önündeki 200 m boyunca her viraj için v = √(tutuş·g·R) ve fren mesafesi. Hepsini karşılayan en yüksek hız hedeflenir. Açık çizginin sonu "dur" sayılır.
- **Beceri (0–1):** güvendiği yanal tutuş 0,50→0,68 g, fren 0,6→0,9 g, kalkış tepkisi 0,7→0,1 sn. Arabanın gerçek sınırı ~0,7 g, yani beceri 1 sınırda sürer.
- **Şerit ve sollama:** her rakip çizginin ±4 m yanında bir şeritte sürer. Önündeki yavaşsa ve öbür şerit boşsa, yalnız düzlükte, yumuşakça (2,5 m/sn) şerit değiştirir.
- **Kurtarma:** 4 sn takılı ya da ters kalırsa çizgiye geri konur. Testte bu hiç gerekmedi.
- Rakipler aynı geçici spor arabayı kullanır. Motor sesleri artık 3B: uzaktaki rakip kısık duyulur.

Test sonucu (oval, 2 tur, oyuncu pist dışında):

| Rakip | Beceri | Süre | Turlar |
|---|---|---|---|
| 1 | 0,55 | 73,5 sn | 38,6 / 34,9 |
| 2 | 0,80 | 65,4 sn | 34,7 / 30,8 |
| 3 | 1,00 | 67,3 sn | 37,2 / 30,1 (arkadan başlar, sollayarak gelir) |

Yarışlar veri olarak ayarlanır: `UBNRaceDefinition.RacingLine` (yarış çizgisi noktaları) ve `RivalSkills` (rakip başına beceri). Test pistindeki değerler `Tools/build_test_map.py` içinde.

## Otomatik Testler (Faz 2)

| Test | Ne doğrular |
|---|---|
| `Race.Progress` | Kapı sırası, tur, süre, geçilen kapı sayısı |
| `Race.DriftScoring` | Puan, çarpan, bankalama, çarpma/spin cezası |
| `Race.RivalSafeSpeed` | Viraj hızı fiziği, çizgi sonunda durma |
| `Race.CircuitByGates` | Geri sayım kilidi, sırasız kapı, 2 tur bitiş, rakip sayısı |
| `Race.SprintFromBeacon` | Sütundan başlatma, bitiş, 1. sıra |
| `Race.DriftScores` | Gerçek el freni drifti puan getirir |
| `Race.RivalsFinishCircuit` | 3 rakip 2 turu kazasız bitirir, turlar 10–60 sn |
| `Race.AbandonEndsRace` | Çıkış tuşu yarışı bitirir, rakipler sonra silinir |

## Düzeltilen Hatalar

| Hata | Düzeltme |
|---|---|
| Rakipler saç tokasında 6 m dışarı kayıp kaldırıma takılıyordu | Tutuş varsayımı ölçülen ~0,7 g'ye indirildi, çizgiden sapma düzeltmesi eklendi |
| Viraj ortasında şerit değiştiren rakip çarpışıyordu | Şerit değişimi yalnız düzlükte |
| Ani şerit değişimi aracı kaldırıma taşırıp kapıyı ıskalatıyordu | Şerit 2,5 m/sn hızla kayar |
| Oyuncunun aracı yarışta yok olursa yarış "bitti" durumunda kalıyordu (rakipler de dünyada kalırdı) | Sonuç sayacı araçtan bağımsız işler |

## Bilinen Sınırlar

- Rakipler birbirine ve sana çarpmaktan yalnızca şerit ve takip mantığıyla kaçınır. Gerçek bir çarpışma önleme yok.
- Rakipler drift yarışına katılmaz. Drift tek başına puan yarışı.
- Yarış çizgisi şimdilik yol orta çizgisi. Viraj içine kesen ideal çizgi Faz 3'teki şehirde gelir.
- Yarış yazıları İngilizce (LAP, POS, FINISHED).
