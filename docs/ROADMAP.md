# BLACK NUMBERS — Yol Haritası

> **Durum:** Yaşayan doküman · **Son güncelleme:** 2026-11-17
> Hikâye referansı: [BLACK_NUMBERS_STORY.md](BLACK_NUMBERS_STORY.md). Bu doküman hikâyeyi değiştirmez, onu oynanabilir hâle getirme sırasını belirler.

## Mantık

Hikâye dört oynanış sistemine dayanır: **sokak yarışı (Sprint, Pist, Drift)**, **araç gelişimi (Tier 1–6)**, **polis kovalamacası (Heat 1–10 ve Özel Kademe)** ve **hikâye ilerlemesi (16 BLACK NUMBERS üyesi, deliller, ifşalar)**. Hikâye bu sistemleri kilit olaylarla açar:

| Hikâye olayı | Açtığı sistem |
|---|---|
| Yarış zaferleri | Yeni BN rakipleri, delil |
| Kimlik ifşası (R4) | Heat 6 ve Demir |
| Mekanikçiyle barışma (R6) | Tier 6 |
| Son kovalamaca | Özel Kademe |

Bu yüzden sıra **önce sistemler, sonra hikâye** şeklindedir. Her faz kendi başına oynanabilir bir sonuç bırakır. Hikâyeye bağlı fazlar, §18'deki açık soruların bir kısmı karara bağlanmadan başlayamaz (bkz. [Karar bekleyenler](#karar-bekleyenler)).

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
- [ ] Rakip yapay zekâ: yarış çizgisini (spline) takip eden sürücüler, beceri ayarı
- [x] Yarışlar veri olarak tanımlanır (`UBNRaceDefinition`: rota, disiplin, tur, süre) — *rakipler ve ödül alanları rakip YZ ve Faz 5 ile eklenecek*

**Bitti sayılır:** Aynı bölgede bir Sprint, bir Pist ve bir Drift yarışı baştan sona oynanır.

## Faz 3 — Şehir Dikey Dilimi

**Amaç:** Yarışların ve kovalamacaların geçeceği küçük ama gerçek bir şehir bölgesi.

- [ ] Yol ağı: kavşaklar, ana cadde, ara sokaklar, otoyol rampası
- [ ] Bölge düzeni: yarış rotaları ve kaçış yolları için kısa yollar
- [ ] Performans bütçesi: düşük donanımda 60 FPS hedefi, ölçüm testi
- [ ] Kırılabilir dekor: sokak lambaları, bariyer, çöp kutusu çarpınca devrilir/parçalanır (performans bütçesine göre)
- [ ] Şehrin kimliği netleşince (Q-02) sanat yönü: dönem, mimari, renk paleti

**Bitti sayılır:** Faz 2'deki yarışlar bu bölgeye taşınır, bölge serbest dolaşılabilir.

## Faz 4 — Polis Kovalamacası (Heat 1–5)

**Amaç:** Standart polis baskısı. Heat 6 ve sonrası bu altyapının üstüne kurulur.

- [ ] Polis araçları ve takip yapay zekâsı: takip, kıstırma, PIT manevrası
- [ ] Heat göstergesi: dikkat çekme, takip, gözden kaybolma, soğuma süresi, yakalanma
- [ ] Heat 1–5 kademeleri **veri olarak**: araç sayısı, taktik, yol barikatı
- [ ] Polis telsiz konuşmaları (çevresel anlatım için altyapı)
- [ ] Yarış sırasında polisin karışması

**Bitti sayılır:** Oyuncu Heat 1'den 5'e kadar kovalanabilir, ya kaçar ya yakalanır.

## Faz 5 — Garaj ve Araç Gelişimi (Tier 1–5)

**Amaç:** Yarış kazanmanın arabayı geliştirmeye dönüşmesi.

- [ ] Para ve itibar, kayıt sistemi
- [ ] Garaj (Eşin sağladığı yer): araç seçimi ve parça takma
- [ ] Parçalar Tier 1–5: motor, şanzıman, süspansiyon, lastik, ağırlık. Fizik değerlerini **dengeli sınırlar içinde** değiştirir (sürüş testi sınırları korur)
- [ ] Birden fazla araç (gerçek modeller gelince geçici spor araba kalkar)

**Bitti sayılır:** Oyuncu kazandığı parayla parça alır ve fark sürüşte hissedilir.

## Faz 6 — BLACK NUMBERS Listesi ve Hikâye İskeleti

**Amaç:** Hikâye ilerlemesinin sistemlere bağlanması. *Q-04, Q-05 ve S-03 kararlarını gerektirir.*

- [ ] 16 BN üyesi **veri olarak**: §5.3 boss şablonu (işlev, kişilik, sürüş tarzı, araç, disiplin, verdiği delil)
- [ ] Rütbe ilerlemesi: üyeye meydan okuma koşulları (yarış sayısı, itibar, Heat başarısı)
- [ ] Delil defteri: toplanan deliller ve ifşa kilometre taşları R1–R8
- [ ] Hikâye durum makinesi: Perde 1–5, bayraklar (kimlik ifşası → Heat 6, barışma → Tier 6)
- [ ] Geçici sinematikler: storyboard/metin ekranları (gerçek sinematikler Faz 8'de)
- [ ] Mekanikçinin **kademeli** tutum değişimi: engelleme → şüphe → barışma

**Bitti sayılır:** Perde 1 ve Perde 2 baştan sona oynanır. İlk somut delil (R3) bulunur.

## Faz 7 — Perde 3–5: Demir ve Son Kovalamaca

Üç bağımsız doğrulanabilir alt faza bölünür. Her biri bir öncekinin üzerine kurulur.

### Faz 7A — Kimlik İfşası ve Heat 6

- [ ] Kahramanın kimliğinin ifşası: büyük ve oynanabilir bir olay (R4, C06)
- [ ] Heat 6 açılır. Demir sahaya iner, emri **CANLI**
- [ ] Hikâye durum makinesine bağlanır (ifşa bayrağı → Heat 6)
- [ ] Polis telsizinde Demir'in kişisel katılımı duyulur

**Kabul:** İfşa olayı oynanır. Olaydan önce Heat 6'ya çıkılamaz, sonra çıkılabilir (otomatik test). Heat 6 kovalamacası Heat 5'ten ölçülebilir biçimde zordur (araç sayısı, taktik).

### Faz 7B — Heat 7–10, Barışma ve Tier 6

- [ ] Heat 7–10 kademeleri: giderek sofistikeleşen taktikler, emir hâlâ **CANLI**
- [ ] Mekanikçiyle tam barışma (R6, C08); öncesinde kademeli tutum değişimi
- [ ] **Tier 6** parçaları yalnızca Mekanikçiden, yalnızca barışmadan sonra
- [ ] Tier 6 dengeli fiziği korur (sürüş testlerinin sınırları içinde kalır)

**Kabul:** Barışmadan önce Tier 6 satın alınamaz, sonra alınabilir (otomatik test). Tier 6 takılı araç sürüş testlerini geçer. Heat 7–10 veri olarak tanımlıdır ve her kademe bir öncekinden zordur.

### Faz 7C — Final Yarışı, Özel Kademe ve Sonuçlar

- [ ] Son yarış ve Liderin **delillerle** teslim edilmesi (R8, C09–C11)
- [ ] **Özel Kademe** final kovalamacası: polisi yenerek değil, **rota seçimi ve beceriyle** kaçış; Demir'in "ÖLÜ YA DA DİRİ" emri
- [ ] Eşin kaçışa yardımı (biçimi S-13'e bağlı)
- [ ] Tahribatın anlatı sonuçları (oynanıştaki kapsamı S-14'e bağlı)

**Kabul:** Özel Kademe yalnızca final kovalamacasında açılır (otomatik test). Kaçış en az iki farklı rota ile tamamlanabilir. Kovalamaca Heat 10'dan ölçülebilir biçimde yoğundur. Liderin teslimi, toplanan delillere bağlıdır (delil eksikse yüzleşme açılmaz).

## Faz 8 — Epilog, Sinematikler ve Cila

- [ ] Sinematikler C01–C19 (Sequencer)
- [ ] Epilog: mezar ziyareti, Mekanikçinin Tier 6 reddi (kanon replikler), haber yayınları
- [ ] Ses, müzik, arayüz cilası
- [ ] Performans ve paketleme (Linux + Windows)

---

## Karar Bekleyenler

Bu sorular cevaplanmadan ilgili faz tamamlanamaz. Cevaplar hikâye dokümanının §17 bölümüne taşınır.

| Soru | Engellediği faz | Neden |
|---|---|---|
| Q-02 Şehrin adı ve kimliği. Bölge önerisi: [PROPOSALS.md](PROPOSALS.md#şehir-bölgeleri) | Faz 3 sanat yönü | Mimari, dönem, palet buna bağlı |
| Q-04, Q-05 16 üyenin kimliği ve verdikleri deliller | Faz 6 | Rakip verisi ve delil zinciri |
| S-03 Lider 16'nın içinde mi (#1), üstünde mi? | Faz 6 | Liste yapısı ve final yarışı |
| S-08 Heat 1–5'te polis Kahramanı kim olarak kovalar? | Faz 4 telsiz/anlatı | Kovalamacanın anlatı çerçevesi |
| S-10 Son yarış ile Liderle yüzleşme aynı olay mı? | Faz 7 | Final kurgusu |
| S-14 Tahribat oynanışta da görünecek mi? | Faz 7 | Hasar sistemi gerekip gerekmediği |
| Q-11 Final yarış rotası ve kaçış sekansı | Faz 7 | Şehir düzeni bunu taşımalı (Faz 3'te yer ayrılır) |
| Sivil trafik olacak mı? Öneri: [PROPOSALS.md](PROPOSALS.md#sivil-trafik) | Faz 3 (temel), Faz 4 (polise tepki) | Şehrin canlılığı ve performans bütçesi |

## Değişiklik Günlüğü

| Tarih | Değişiklik |
|---|---|
| 2026-11-17 | İlk sürüm. Faz 0 tamam, Faz 1 başladı (sokak lambası sınıfı). |
| 2026-11-17 | Faz 2 ilk parça: yarış altyapısı, Sprint/Pist/Drift, test pistinde 3 yarış. Rakip YZ sırada. |
| 2026-11-17 | Faz 1 tamamlandı: otomatik testler + proje sahibinin sürüş testi. |
| 2026-11-17 | Faz 1 uygulandı; 3 kabul kriteri elle test bekliyor (bkz. PHASE1.md). |
| 2026-11-17 | Faz 1 kapsamı genişletildi (fiziksel malzemeler, drift, ses, HUD, performans ölçümü). Faz 7, 7A/7B/7C olarak bölündü. Şehir ve trafik önerileri eklendi. |
