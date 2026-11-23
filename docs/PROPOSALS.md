# BLACK NUMBERS — Tasarım Önerileri

> **Durum:** Karamara 6-Bölge haritalaması ve Pursuit Breakers ↔ Şehir Faturası tasarımı 2026-11-20 tarihinde kanonla hizalanmıştır ([BLACK_NUMBERS_STORY.md](BLACK_NUMBERS_STORY.md)). Sivil trafik yoğunluk bütçesi Faz 3 performans ölçümüyle kesinleşecektir.

## Karamara Şehir Bölgeleri

Hikâye üç yarış disiplini (Sprint, Pist, Drift), Heat 1–10 kovalamacaları, Pursuit Breakers ↔ Şehir Faturası takibi ve iki çıkış rotalı Özel Kademe final kaçışı ister. Karamara'nın 6 kanon bölgesi bu ihtiyaçlara göre kurgulanır:

| Kanon Bölge | Yarış için | Kovalamaca için | Çevresel anlatı |
|---|---|---|---|
| **Merkez Bulvar & Çarşı** *(Şehir merkezi)* | Kısa, keskin dönüşlü Pist turları. Taş/asfalt karma zemin, gece vitrin ışıkları | Dar sokaklar, ara geçitler, çok sayıda kavşak; polis barikatı kurmak kolay, kaçmak zor | BLACK NUMBERS'ın vitrin yüzü: bahis salonları, kamera ağı (`#08` Hakan / `#11` Ozan) |
| **Doğu Sanayi** *(Sanayi bölgesi)* | Geniş yollar, uzun Sprint düzlükleri, depo içi Drift alanları | Konteyner ve depo arası kısa yollar; Heat 1–3 için ideal ilk kaçış dersi | Tarık Kaya'nın yasal tamirhanesi, Arda Çelik'in galerisi, parça atölyeleri (`#12` Leyla / `#14` Cem) |
| **Eski Liman** *(Mara Docks)* | Rıhtım boyunca hızlı Sprint, vinç altı dönüşler | Açık alanlar yüksek Heat için tehlikeli; tek giriş-çıkışlı rıhtımlar tuzak | Kaçak parça lojistiği (`#13` Dario), finalde Viktor Serez'in yakalandığı **Eski Liman Köprüsü** |
| **Kuzey Viyadükleri & Çevre Yolu** *(Otoyollar)* | Yüksek hız Sprint'leri, bölgeleri bağlayan omurga | Yüksek Heat'te barikat, dikenli şerit ve Pursuit Breaker yapıları | Özel Kademe final kaçışının ana sahnesi ve 2 çıkış rotasına ayrılan omurga (`Q-11`) |
| **Yamaç Mahallesi** *(Yerleşim sokakları)* | Düşük hızlı, teknik Pist'ler | Sivil trafik ve dar sokaklar; polis daha temkinli (Demir'in "canlı" emri ile uyumlu) | Emre ve Selin'in mahallesi, Emre'den kalan kayıt dışı eski garaj |
| **Kuzey Geçidi & Seyir Tepesi** *(Dağ yolları)* | Drift'in evi: virajlı iniş-çıkışlar, sıkı saç tokaları | Az kavşak, toprak servis yolu kestirmesi (ikinci Özel Kademe kaçış rotası) | 5 yıl önceki kazanın yaşandığı viraj; tepede Emre'nin mezarlığı (`C18`) |

## Pursuit Breakers ↔ Şehir Faturası (Collateral Damage)

- **Most Wanted Nostaljisi + Kanon Bedeli:** Viyadük iskeleleri, liman vinçleri, su kuleleri ve benzinlik/depo tabelaları çarpıldığında polis takibini yavaşlatan yıkımlar oluşturur (Pursuit Breakers).
- **Sonuçsuz Eğlence Değildir (Kanon #21):** Yıkılan her Pursuit Breaker, devrilen lamba ve çarpılan sivil/kamu mülkü **Şehir Hasar Kaydı'na (Şehir Faturası)** dolar cinsinden işlenir. Fatura büyüdükçe Demir'in telsiz öfkesi sertleşir ve epilogdaki haber yayınları (`C14`) gerçek hasar bilançosunu gösterir.

**Önerilen yapım sırası (Faz 3'te):** Önce **Doğu Sanayi + Merkez Bulvar** ikilisi ve **Kuzey Viyadüğü** bağlantısı. Üç disiplinin üçünü de taşıyabilir ve Heat 1–5 için hem kolay hem zor kaçış alanı verir. Liman ve Kuzey Geçidi sonraki dilimlerde gelir.

**Ölçek önerisi:** Düşük donanım hedefi için bölgeler World Partition ile parçalara bölünür ve her bölge tek başına yüklenebilecek boyutta tutulur.

## Sivil Trafik

**Karar:** Önerilir, ama proje sahibi onaylamadan uygulanmaz. Hikâye trafiğe değinmez. Ancak §11.2 "sivil mülklerin hasar görmesi" ve kovalamacaların gerçekçiliği bunu doğal bir gereklilik hâline getirir.

### Mimari önerisi

| Katman | Görev | Not |
|---|---|---|
| **Yol grafı** | Şeritler ve kavşaklar veri olarak (spline + bağlantılar) | Yarış rotaları ve polis yapay zekâsı aynı grafı kullanır |
| **Trafik yöneticisi** | Oyuncu çevresinde araç doğurma ve yok etme, yoğunluk ayarı | Bir yönetici, araç başına `Tick` yok |
| **Mesafeye bağlı benzetim** | Yakın: tam Chaos fiziği. Orta: kinematik şerit takibi. Uzak: yalnızca graf üzerinde konum, görünmez | Düşük donanımda en büyük kazanç burada |
| **Kurallar** | Şerit takibi, kavşakta sıra, kırmızı ışık, önündeki araca mesafe | Basit durum makinesi, davranış ağacı gerekmez |
| **Çarpışma** | Yakın araçlar gerçek fizik; çarpılan araç tam fiziğe geçer | Hasar ve sonuç kuralı S-14 kararına bağlı |
| **Polise tepki** | Siren duyan araç sağa çeker, barikat yakınında durur | Faz 4'te eklenir |
| **Yoğunluk** | Grafik ayarından bağımsız oynanış ayarı + düşük donanım için üst sınır | Bölge ve saate göre (gece daha az) |

**Ölçülecek bütçe (öneri):** Düşük ayarda en fazla ~20 görünür trafik aracı, bunların en fazla ~6'sı tam fizikte.

### Aşamalar

1. **Faz 3:** Yol grafı, şerit takibi yapan kinematik araçlar, kavşak kuralı, yoğunluk ayarı, performans ölçümü.
2. **Faz 4:** Polis kovalamacasına tepki, çarpışmada tam fiziğe geçiş.
3. **Sonra:** Saat ve bölgeye göre yoğunluk, trafik ışıkları.
