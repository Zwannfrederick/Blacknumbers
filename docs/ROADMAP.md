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

## Faz 1 — İlk Sokak ve Sürüş Hissi

**Amaç:** Arabayı gerçek bir sokakta, gece ve gündüz sürmenin iyi hissettirmesi.

- [ ] Asfalt, beton, şerit boyası malzemeleri (motorun gürültü dokularından, dış varlık gerektirmez)
- [ ] İlk cadde: 4 şerit, kaldırımlar, gri kutu binalar *(test pistinin yanında)*
- [~] Gece kendiliğinden yanan sodyum sokak lambaları *(sınıf yazıldı, haritaya eklenmedi)*
- [ ] HUD: hız, vites, devir
- [ ] Motor sesi (devre bağlı, en basit hâliyle)
- [ ] Drift'e uygun yol tutuş: el freniyle kontrollü kayma, kaymadan çıkış
- [ ] Kamera: hıza ve kaymaya göre ince ayar

**Bitti sayılır:** Oyuncu caddede gece sürerken lambalar yanar, hız göstergesi çalışır, el freniyle drift yapılabilir.

## Faz 2 — Yarış Çekirdeği

**Amaç:** Üç disiplinin de tek bir yarış altyapısıyla oynanması.

- [ ] Yarış altyapısı: kontrol noktaları, başlangıç geri sayımı, bitiş, sonuç ekranı
- [ ] **Sprint** (A'dan B'ye) ve **Pist** (tur sayımı)
- [ ] **Drift** puanlama: kayma açısı × hız × süre, kombo ve duvar cezası
- [ ] Rakip yapay zekâ: yarış çizgisini (spline) takip eden sürücüler, beceri ayarı
- [ ] Yarışlar veri olarak tanımlanır (rota, disiplin, rakipler, ödül)

**Bitti sayılır:** Aynı bölgede bir Sprint, bir Pist ve bir Drift yarışı baştan sona oynanır.

## Faz 3 — Şehir Dikey Dilimi

**Amaç:** Yarışların ve kovalamacaların geçeceği küçük ama gerçek bir şehir bölgesi.

- [ ] Yol ağı: kavşaklar, ana cadde, ara sokaklar, otoyol rampası
- [ ] Bölge düzeni: yarış rotaları ve kaçış yolları için kısa yollar
- [ ] Performans bütçesi: düşük donanımda 60 FPS hedefi, ölçüm testi
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

**Amaç:** Hikâyenin doruk noktası.

- [ ] Kimlik ifşası: büyük ve oynanabilir bir olay (C06)
- [ ] Heat 6–10: Demir sahaya iner, taktikler sertleşir, emir **CANLI**
- [ ] Mekanikçiyle barışma → **Tier 6** (sadece Mekanikçiden, dengeli fizik)
- [ ] Son yarış ve Liderin delillerle teslim edilmesi
- [ ] **Özel Kademe** final kovalamacası: polisi yenerek değil, **rota seçimi ve beceriyle** kaçış. Eşin yardımı.
- [ ] Tahribatın sonuçları: hasarlı sokaklar ve haber yayınları (oynanıştaki kapsamı S-14'e bağlı)

**Bitti sayılır:** Oyun Liderin teslim edilmesine ve Özel Kademeden kaçışa kadar oynanır.

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
| Q-02 Şehrin adı ve kimliği | Faz 3 sanat yönü | Mimari, dönem, palet buna bağlı |
| Q-04, Q-05 16 üyenin kimliği ve verdikleri deliller | Faz 6 | Rakip verisi ve delil zinciri |
| S-03 Lider 16'nın içinde mi (#1), üstünde mi? | Faz 6 | Liste yapısı ve final yarışı |
| S-08 Heat 1–5'te polis Kahramanı kim olarak kovalar? | Faz 4 telsiz/anlatı | Kovalamacanın anlatı çerçevesi |
| S-10 Son yarış ile Liderle yüzleşme aynı olay mı? | Faz 7 | Final kurgusu |
| S-14 Tahribat oynanışta da görünecek mi? | Faz 7 | Hasar sistemi gerekip gerekmediği |
| Q-11 Final yarış rotası ve kaçış sekansı | Faz 7 | Şehir düzeni bunu taşımalı (Faz 3'te yer ayrılır) |
| Hikâyede yer almayan sivil trafik olacak mı? | Faz 3 | Şehrin canlılığı ve performans bütçesi |

## Değişiklik Günlüğü

| Tarih | Değişiklik |
|---|---|
| 2026-11-17 | İlk sürüm. Faz 0 tamam, Faz 1 başladı (sokak lambası sınıfı). |
