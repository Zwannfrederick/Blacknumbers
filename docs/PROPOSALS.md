# BLACK NUMBERS — Tasarım Önerileri

> **Durum:** Öneri. **Kanon değildir.** Proje sahibi onaylayana kadar uygulanmaz.
> Şehrin adı ve kimliği hikâyede açıktır (Q-02). Bu doküman isim vermez, sadece oynanışa hizmet eden bölge tiplerini önerir.

## Şehir Bölgeleri

Hikâye üç yarış disiplini (Sprint, Pist, Drift), Heat 1–10 kovalamacaları ve rota seçimiyle kazanılan bir final kaçışı ister. Bölgeler bu ihtiyaçlara göre seçilir. Her bölgenin hem bir yarış görevi hem bir kaçış karakteri vardır.

| Bölge | Yarış için | Kovalamaca için | Çevresel anlatı |
|---|---|---|---|
| **Şehir merkezi** | Kısa, keskin dönüşlü Pist turları. Gece ışıklarıyla en "vitrin" bölge | Dar sokaklar, ara geçitler, çok sayıda kavşak; polis barikatı kurmak kolay, kaçmak zor | BLACK NUMBERS'ın vitrin yüzü: bahis salonları, gece kulüpleri, örgütün parası |
| **Sanayi bölgesi** | Geniş yollar, uzun Sprint düzlükleri, depo içi Drift alanları | Konteyner ve depo arası kısa yollar; Heat 1–3 için ideal ilk kaçış dersi | Mekanikçinin geçmişi, parça kaçakçılığı, araç hazırlık atölyeleri |
| **Liman** | Rıhtım boyunca hızlı Sprint, vinç altı dönüşler | Açık alanlar helikopter (yüksek Heat) için tehlikeli; tek giriş-çıkışlı rıhtımlar tuzak | Lojistik işlevli BN üyesi, kaçak araç ve parça trafiği |
| **Otoyollar** | Yüksek hız Sprint'leri, bölgeleri bağlayan omurga | Yüksek Heat'te barikat ve dikenli şerit; çıkış rampaları rota seçimi yaratır | Final kaçışının (Özel Kademe) ana sahnesi olabilir (Q-11) |
| **Yerleşim sokakları** | Düşük hızlı, teknik Pist'ler | Sivil trafik ve dar sokaklar; polis daha temkinli (Demir'in "canlı" emri ile uyumlu) | Eşin evi/garaj, Ölen Dostun mahallesi, anılar |
| **Tepe / dağ yolları** | Drift'in evi: virajlı iniş-çıkışlar, sıkı saç tokaları | Az kavşak, tek yol: kaçış zor ama takip edenler de zorlanır | Orijinal kazanın yeri olabilir; mezarlık burada (C18) |
| **Virajlı drift rotaları** | Drift yarışları ve serbest drift | Kovalamacada riskli ama kestirme | Yeraltı sahnesinin buluşma noktaları |

**Önerilen yapım sırası (Faz 3'te):** Önce **sanayi + şehir merkezi** ikilisi. Üç disiplinin üçünü de taşıyabilir ve Heat 1–5 için hem kolay hem zor kaçış alanı verir. Otoyol bağlantısı ve tepe yolları sonraki dilimlerde gelir.

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
