# BLACK NUMBERS — Resmî Anlatı Referansı

> **Durum:** Kanon (Temel İsimler, Şehir, 16 Üyeli Kadro ve Süreklilik Çözümleri Bağlandı) · **Son güncelleme:** 2026-11-20
> **Kapsam:** Anlatı, dünya kimliği, 16 kişilik örgüt kadrosu, süreklilik çözümleri ve oynanış bağlantıları.
> **Hedef okuyucu:** Anlatı tasarımcıları, sistem tasarımcıları ve projede çalışacak geliştiriciler / AI ajanları.

| Alan | Değer |
|---|---|
| Oyun adı | **BLACK NUMBERS** |
| Tür | Tek oyunculu, açık dünya, hikâye odaklı arcade/simcade sokak yarışı |
| Motor | Unreal Engine 5 |
| Şehir | **Karamara** *(liman ve uluslararası kayıtlarda **Port Mara**)* |
| Zaman aralığı | Orijinal kaza ile dönüş arası **5 yıl** |
| Ton | Olgun, acı-tatlı. Bir zafer kutlaması değil. |

---

## 0. Bu Dokümanı Kullanma Kuralları

1. **[§17 Onaylanmış Kanon](#17-onaylanmış-kanon) bağlayıcıdır.** Kodlanan her sistem, diyalog, görev ve sinematik buradaki kurallarla tutarlı olmalıdır.
2. **Özgün araç ve mekân ilkesi:** Şehir ve tüm araç markaları **%100 kurgusal ve özgündür**; gerçek dünya otomobil markaları veya logoları kullanılmaz.
3. **Olay örgüsü kilitlidir:** Ana omurga ve süreklilik düğümleri §17 ve §18 arşivinde karara bağlanmıştır.
4. **Hikâyeyle ilgisi olmayan mekanik eklenmez.** Temel oynanış şunlardır: sokak yarışı, araç gelişimi (NOS ve Tier 1–6 dâhil), drift, Speedbreaker taktiksel sürüş, polis kovalamacası (Pursuit Breakers ↔ Şehir Faturası) ve 16'lık liste / kanıt ilerlemesi.

### Kesinleşmiş Karakter Kimlikleri (`Q-01`, `Q-12`, `S-11`)

| Rol / Kısaltma | Kesin Ad | Anlatıdaki Yeri |
|---|---|---|
| **Kahraman** (`KHR`) | **Kerem Yalçın** *(sahte kimlik / listede: **K. Vance**)* | Eski yeraltı sokak yarışçısı, oyuncu karakteri |
| **Ölen Dost** (`DOST`) | **Emre Karahan** *(eski lakabı: **"Kuzey"**)* | Kerem'in en yakın arkadaşı, yetenekli yarışçı. Sabotaj sonucu ölür |
| **Mekanikçi** (`MEK`) | **Tarık Kaya** | Gruptan hayatta kalan usta otomotiv mekanikçisi, Doğu Sanayi atölye sahibi |
| **Polis Memuru / Eş** (`EŞ`) | **Selin Karahan** | Emre'nin eşi; Karamara Emniyeti Sokak Yarışı Özel Birimi'nde **Demir'in komutası altında** polis memuru |
| **Polis Amiri** (`DMR`) | **Başkomiser Demir Arslan** | Yeraltı yarış sahnesine karşı özel birimi yöneten kıdemli polis amiri |
| **Lider** (`LDR`) | **Viktor Serez** *(BLACK NUMBERS `#01`)* | Örgütün kurucusu, `#01` numaralı sürücüsü ve asıl kötü karakter |
| **Epilog Galeri Ortağı** (`ORT`) | **Arda Çelik** | 5 yıl önce ekibin Doğu Sanayi garajında çıraklık yapan, yeraltı yarışına hiç bulaşmamış genç tanıdık; epilogda Tarık'ın tamirhanesinin bitişiğinde yasal araç galerisi işletir |

### Şehrin Kimliği ve Bölgeleri (`Q-02`)

**Karamara** *(Port Mara)*, Rockport esintili yüksek beton viyadükler, sanayi koridorları ve sonbahar kehribarı gündüzleriyle Akdeniz/Avrupa liman şehirlerinin dar taş merkezi, oto sanayi sitesi kültürü, konteyner limanı ve sisli dağ geçitlerinin özgün birleşimidir.

| Bölge | Yarış ve Anlatı Karakteri |
|---|---|
| **Eski Liman *(Mara Docks)*** | Konteyner vinçleri, ıslak rıhtım betonları, gümrüksüz parça kaçakçılığı ve Viktor'un finaldeki yakalanma köprüsü *(Eski Liman Köprüsü)* |
| **Doğu Sanayi *(Sanayi Sitesi)*** | Tarık Kaya'nın yasal oto tamirhanesi, depolar, bahis kulüpleri, geniş sprint düzlükleri ve depo içi drift avluları |
| **Merkez Bulvar & Çarşı** | Taş/asfalt karma dar merkez sokakları, bahis salonları, kamera ağı; polis barikatlarının en sık kurulduğu vitrin bölgesi |
| **Kuzey Viyadükleri & Çevre Yolu** | Yüksek hızlı otoyol ve viyadük omurgası; Pursuit Breaker yapıları ve Özel Kademe final kaçışının ana sahnesi |
| **Yamaç Mahallesi** | Selin'in Emre'den kalan kayıt dışı garajını gizlediği eski yerleşim sokakları ve teknik pist rotaları |
| **Kuzey Geçidi & Seyir Tepesi** | Virajlı dağ iniş-çıkışları, drift rotaları, 5 yıl önceki kazanın yaşandığı viraj ve tepedeki mezarlık |

---

## 1. Hikâyeye Genel Bakış

BLACK NUMBERS, **5 yıl önce** en yakın arkadaşı **Emre Karahan'ın** arabasını sabote etmekle haksız yere suçlanan eski yeraltı sokak yarışçısı **Kerem Yalçın'ın** Karamara'ya dönüşünü anlatır. Kerem bu suçlamayla arkadaşının ölümüne yol açmış sayılmıştır.

Bir zamanlar dört kişilik sıkı bir arkadaş grubu vardır: iki yetenekli yarışçı (**Kerem** ve **Emre**), usta mekanikçi **Tarık Kaya** ve Emre'nin eşi **Selin Karahan**. Kerem ve Emre, Karamara'nın yeraltı yarış sahnesinde son derece başarılı ve bağımsız iki isimdir.

Aynı dönemde hırslı bir figür olan **Viktor Serez**, yeraltı yarış camiasını örgütleyip tekeline almaya çalışmaktadır. Amacı **BLACK NUMBERS** adlı 16 kişilik hiyerarşik yapıyı kurmak, sahneyi kontrol etmek ve onu para, prestij, nüfuz ve güç için kullanmaktır. Kerem ve Emre'nin bağımsızlığı ve popülerliği bu planın önündeki en büyük engeldir.

Viktor iki engeli tek bir komployla ortadan kaldırır. Kerem, Viktor'un çevresine meydan okumak için Kuzey Geçidi'nde büyük bir yarış ayarlar. Yarıştan önce teknik operatör `#03` **İlhan Korkmaz**, Kerem'in garajından Kerem'e ait gerçek aletleri ve eldivenleri çalarak Emre'nin yakıt/NOS hattına **termal tetikli bir sabotaj valfi** yerleştirir. Emre starttan önce *"yakıt ve basınç kokusu garip"* diye şüphelense de Kerem yarış heyecanıyla *"yeni ayar, iki turda oturur"* deyip geçiştirir. Araç yarış sırasında patlar ve Emre ölür. Ardından olay yerine bırakılan gerçek ve sahte delillerle suç Kerem'in üzerine yıkılır; `#02` **Elena Novak** üzerinden savcılık ve adli tıp katında dosya olağanüstü hızla onaylatılır. Genç komiser **Demir Arslan** kusursuz görünen bu dosyaya inanır ve soruşturmayı kapatır.

Beş yıl sonra Emre'nin eşi ve artık Demir'in biriminde görev yapan polis memuru **Selin**, eski soruşturma dosyasındaki tutarsızlıkları fark eder ve Kerem'e ulaşır. Kerem **"K. Vance"** sahte kimliğiyle şehre döner, BLACK NUMBERS'ın 16 kişilik hiyerarşisinde yükselir ve komployu belge belge ortaya çıkarır.

---

## 2. Kronolojik Zaman Çizelgesi

| # | Dönem | Olay |
|---|---|---|
| T0 | 5+ yıl önce | Kerem, Emre, Tarık ve Selin bir aradadır. Kerem ve Emre Karamara yeraltı sahnesinin zirvesindedir. |
| T1 | 5 yıl önce | Viktor Serez sahneyi tekeline alma planını yürütür. Kerem ve Emre en büyük iki engeldir. |
| T2 | 5 yıl önce | **Orijinal Olay (`C01`):** Kerem'in ayarladığı yarış öncesi İlhan Korkmaz (`#03`), Kerem'in çalınan aletleriyle Emre'nin aracına termal sabotaj valfi takar. Araç Kuzey Geçidi'nde patlar ve Emre ölür. |
| T3 | 5 yıl önce | Olay yeri delilleri kasıtlı olarak karıştırılır (`C02`). Deliller Kerem'i işaret eder. |
| T4 | 5 yıl önce | Kerem hakkında tutuklama kararı çıkar. Kerem tutuklanmadan önce şehirden kaçar. |
| T5 | 5 yıl önce | Elena Novak (`#02`) adli süreci hızla kapattırır; genç komiser Demir Arslan dosyayı yeterli sayıp kapatır. |
| T6 | 5 yıllık ara | Grup dağılır. Kerem kaybolur; Tarık kin besleyip yasal tamirhaneye çekilir (Emre'yle yarım kalan Tier 6 projesi kilitli kalır); Selin polis olur ve Demir'in birimine girer; Viktor BLACK NUMBERS'ı kurar ve genişletir. |
| T7 | Oyunun başı | Selin soruşturma tutarsızlıklarını fark eder ve Kerem'e mesaj atar (`C03`). Polis olduğunu gizler. |
| T8 | Perde 1 | Kerem **"K. Vance"** kimliğiyle Karamara'ya döner. Selin, Yamaç Mahallesi'nde Emre'den kalan kayıt dışı garajı ve aracı sağlar. |
| T9 | Perde 2 | BLACK NUMBERS alt sıralarından (`#16`–`#09`) yükseliş başlar. `#12` Leyla'nın zaferiyle **ilk somut delil (`R3`, `C05`)** çıkar. Tarık önce engeller (`C04`), sonra şüphelenmeye başlar. |
| T10 | Perde 3 | `#08` Hakan Ertürk yenilince intikam için polise sızdırır; **Kerem'in gerçek kimliği ifşa olur (`R4`, `C06`)**. **Heat 6 açılır** ve Başkomiser Demir olaya kişisel olarak el koyar. |
| T11 | Perde 3–4 | `#06` Miraç'ın dosyasında **Selin'in polis kimliği açığa çıkar (`R5`, `C07`)** ve güven çatışması yaşanır. `#05` Kaan'dan gelen hırsızlık sevkiyat kaydıyla **Tarık tamamen barışır (`R6`, `C08`)** ve **Tier 6 açılır**. |
| T12 | Perde 4 | `#04`–`#02` yenilerek komplo belgelenir (`R7`–`R8`, `C09`). Final yarışı **Karamara Büyük Turu'nda (`C10`)** Viktor yenilir ve Eski Liman Köprüsü kordonunda polise teslim edilir (`C11`). BLACK NUMBERS çöker. |
| T13 | Perde 5 | Yanlış adamı avladığını gören ve şehirdeki büyük hasarla egosu kırılan Demir kontrolü kaybeder (`C12`): **"ONU BANA GETİRİN! ÖLÜ YA DA DİRİ!"** **Özel Kademe** son kovalamaca başlar (`C13`). Selin telsiz emrini İçişleri'ne iletip bir kordonda 15 sn boşluk bırakır; Kerem iki kaçış rotasından biriyle şehirden çıkar. |
| T14 | Epilog | Şehir hasarı haberlerde görünür (`C14`). Demir görevden alınıp tutuklanır (`C15`). Tarık, Arda Çelik'in galerisinin yanında Tier 6 teklifini reddeder (`C16`). Selin görevine devam eder (`C17`). Kerem Kuzey Geçidi mezarlığını ziyaret edip (`C18`) adsız bir kıyı kasabasında sessiz bir firari hayatına çekilir (`C19`). |

---

## 3. Ana Karakterler

### 3.1 Kahraman — Kerem Yalçın / "K. Vance" (`KHR`)

- **Geçmişi:** Emre Karahan'la birlikte Karamara yeraltı sahnesinin en hızlı iki sürücüsünden biridir.
- **Yaşadığı yara:** Arkadaşının ölümünden haksız yere suçlanır, tutuklama kararından önce kaçar ve 5 yıl yarıştan uzak kalır.
- **Hayatta kalan suçluluğunun somut kaynağı (`S-01`):** O geceki yarışı Viktor'un çevresine meydan okumak için **Kerem ayarlamıştır**. Starttan 10 dakika önce Emre *"yakıt ve basınç kokusu garip"* diye şüphelendiğinde Kerem yarış heyecanıyla *"yeni ayar, iki turda oturur"* diyerek geçiştirmiştir. Sabotajı yapmamıştır, ama o uyarıyı ciddiye almadığı için derin bir vicdan yükü taşır.
- **Delillerle ilişkisi:** Olay yerindeki tork anahtarı ve yarış eldivenleri gerçekten kendi garajından çalınmıştır; bu yüzden delilleri tanır ama olay yerine nasıl geldiğini açıklayamaz.
- **Yayı:** Firari → **K. Vance** sahte kimliğiyle dönüş → komployu çözme → Emre'nin ölümünden aklanma → geri dönüşte işlediği yasa dışı yarış ve tahribatın bedelini taşıyarak sessiz bir firari hayatı.
- **Sonu:** Orijinal ölümden masumiyeti kanıtlanır, ama sonraki sokak yarışları ve tahribat yüzünden aklanmaz. Yarışa dönmez.

### 3.2 Ölen Dost — Emre Karahan / "Kuzey" (`DOST`)

- Kerem'in en yakın arkadaşı ve yetenekli bir yarışçıdır. Selin Karahan ile evlidir.
- Viktor'un tekeli için Kerem'le birlikte en büyük iki engelden biridir.
- İlhan Korkmaz'ın (`#03`) yerleştirdiği termal tetikli valf yüzünden aracı Kuzey Geçidi yarışında patlar ve ölür.
- Hikâyede doğrudan yer almaz; varlığı anılarda, kilitli kalan Tier 6 projesinde, delillerde ve Kuzey Geçidi mezar sahnesinde hissedilir.

### 3.3 Mekanikçi — Tarık Kaya (`MEK`)

- **Olaydan sonra (`S-02`):** Kerem'in suçlu olduğuna inanır ve ona derin bir kin besler. Şiddetli yeraltı yarış dünyasından çekilip Doğu Sanayi'de yasal bir oto tamirhanesi açar. Ancak Emre'yle üzerinde çalıştıkları yarım kalmış **özel yarış mühendisliği projesini (Tier 6)** kilitli bir dolapta saklamıştır.
- **Oyunun başında:** Kerem'in dönüşüne ve özellikle Selin'in ona garaj açmasına şiddetle karşı çıkar; teknik destek vermeyi reddeder ve eski çevreyi Kerem'e karşı uyarır.
- **Dönüşümü (`S-09`):** `#12` Leyla'dan çıkan özel sipariş valf koduyla ilk kez şüphelenir (`R3`). `#08` Hakan'ın ele geçirdiği olay yeri fotoğraf manipülasyonuyla şüphesi derinleşir. `#05` Kaan'dan Kerem'in garajına gece girildiğini kanıtlayan kurye sevkiyat belgesi çıkınca (`R6`) **kademeli** dönüşümü tamamlanır.
- **Barışma ve Tier 6 (`S-02`):** Kerem'in masumiyetini kabul eder ve kilitli dolaptaki Tier 6 mühendislik paketini Emre'nin anısına **son bir kez** tamamlayıp Kerem'in aracına uygular.
- **Sonu:** Bu son projeyle eski dünyayla tüm bağını tamamen ve kalıcı olarak kapatır; sıradan tamirhanesinde Arda Çelik'in galerisinin yanında namuslu bir hayat sürer. Epilogdaki Tier 6 reddi önceki mesafeli ayrılıştan değil, defteri kesin olarak kapatmasından doğar.

### 3.4 Polis Memuru / Ölen Dostun Eşi — Selin Karahan (`EŞ`)

- **Olaydan sonra (`S-06`):** Eşini kaybeder. Adalet arayışıyla polis akademisine girer ve **Başkomiser Demir Arslan'ın komutası altındaki** Sokak Yarışı Özel Birimi'ne atanır.
- **İki ayrı motivasyonu:**
  - **Resmî görevi:** Demir'in ekibinde BLACK NUMBERS'ı çökertmek.
  - **Kişisel hedefi:** Emre'nin ölümündeki tutarsızlıkları **resmî atama olmadan**, kendi inisiyatifiyle çözmek.
- **Yaptıkları (`S-07`):** Kerem'le iletişime geçer, polis olduğunu gizler. Resmî polis veritabanına dokunmadan, **Yamaç Mahallesi'ndeki Emre'den kalan kayıt dışı eski depoyu** garaj olarak açar ve eski sokak bağlantılarıyla **"K. Vance"** sahte kimliğini sağlar.
- **Kimlik ifşası ve güven çatışması:** Başta Kerem'in masumiyetinden tam emin değildir. `#06` Miraç'ın dosyalarından Kerem'in kendi takip dosyası çıkınca Kerem Selin'in polis olduğunu öğrenir (`R5`, `C07`). Gerilime rağmen toplanan adli deliller ikisini yeniden aynı cephede buluşturur.
- **Son kovalamaca ve hukuki sonuç (`S-05`, `S-13`, `Q-13`):** Özel Kademe sırasında Demir'in telsizden verdiği **"ÖLÜ YA DA DİRİ"** emrini anında kayda alıp İçişleri'ne (Internal Affairs) ihbar eder; sahada ise Kuzey Viyadüğü'ndeki tek bir kordon noktasının kapanışını 15 saniye geciktirerek Kerem'e kaçış aralığı bırakır. Hukuksuz emri engelleyip teşkilatı koruduğu ve sahte kimlik izi polis sistemine girmediği için soruşturmadan temiz çıkar ve görevine devam eder.

### 3.5 Lider — Viktor Serez (`#01`, `LDR`)

- **Motivasyonu:** Güç, prestij, kâr ve yeraltı sahnesinde mutlak tekel.
- **Konumu (`S-03`):** BLACK NUMBERS listesinin dışından değil, doğrudan **`#01`** numaralı kurucusu ve son yarışçısıdır.
- **Komploda yaptıkları:** İlhan (`#03`) ve Kaan (`#05`) eliyle Kerem'in garajından alet çaldırıp Emre'nin aracını sabote ettirir; Elena (`#02`) eliyle savcılık/adli tıp onayını hızlandırıp dosyayı kapattırır. Tek hamlede iki engeli temizleyip 5 yılda BN imparatorluğunu kurar.
- **Sonu:** Kerem `#02` Elena'dan aldığı son ödeme ve onay zinciriyle Viktor'un suçunu **somut belgelerle** tamamladıktan sonra (`R8`), final yarışı Karamara Büyük Turu'nda Viktor'u yener ve Selin'in koordine ettiği Eski Liman Köprüsü kordonunda polise teslim eder (`C10`–`C11`).

### 3.6 Başkomiser Demir Arslan (`DMR`)

Ayrıntılı yay için bkz. [§7](#7-polis-amiri-demir-arslan--karakter-yayı).

- **Yozlaşmış DEĞİLDİR.** BLACK NUMBERS için gizlice **çalışmaz**.
- 5 yıl önce Elena'nın dış nüfuzla hızlandırdığı dosyayı "eksiksiz" sanıp kapatmış dürüst ama gururlu bir amirdir.
- Mesleki bağlılığı ve 5 yıl önceki hatasının açığa çıkışıyla birleşen gururu, kontrolsüz bir saplantıya dönüşür.

---

## 4. Karakter Motivasyon Tablosu

| Karakter | Başlangıçtaki motivasyon | Temel çatışma | Dönüşüm | Final durumu |
|---|---|---|---|---|
| **Kerem Yalçın** | Gerçeği bulmak, adını temizlemek | Hayatta kalan suçluluğu (`S-01`) ↔ çalınan deliller | Gerçeği kanıtlar, ama dönüşteki seçimlerinin bedelini taşır | Emre'nin ölümünden aklanır. Sessiz bir firaridir. Yarışa dönmez |
| **Tarık Kaya** | Kerem'i suçlu sanır, kin besler | Kin ↔ ortaya çıkan adli deliller | Kini **kademeli olarak** aşar, Emre anısına Tier 6'yı tamamlar | Yasa dışı dünyayı kalıcı olarak bırakır; yasal tamirhanesinde çalışır |
| **Selin Karahan** | Resmî: BN'yi çökertmek. Kişisel: Emre'nin gerçeği | Demir'in birimi ↔ gizli garaj/kimlik ↔ Kerem'e yardım | Şüpheden gerçek güvene geçer; hukuksuz emre karşı çıkar | Gerçeği öğrenir, Demir'in emrini ihbar ettiği için görevde kalır |
| **Demir Arslan** | Şehri korumak, kanunu uygulamak | Görev ↔ 5 yıllık hata, gurur, tahribat öfkesi | Saplantıyı **aşamaz** ve ahlaken çöker | Görevden alınır, soruşturulur, tutuklanır |
| **Viktor Serez** | Güç, kâr, yeraltı tekeli | Kurduğu düzen ↔ Kerem'in dönüşü | Dönüşmez | Büyük Tur'da yenilir ve Liman Köprüsü'nde teslim edilir. BN çöker |

---

## 5. BLACK NUMBERS Örgüt Yapısı

### 5.1 Kanon

- BLACK NUMBERS **16 sıralı büyük yarışçıdan** oluşur (`#16` → `#01`).
- Liste yalnızca giderek güçlenen sürücülerden ibaret değildir; **her üye suç örgütünde belirli bir işlev üstlenir**.
- **Rütbe yükseldikçe** üyenin örgütün geçmişi ve Viktor'un komplosu hakkındaki bilgisi artar.
- Her büyük zafer, orijinal komployla ilgili **somut bilgi, erişim veya adli delil** sağlar.
- Tüm araçlar **özgün kurgusal** markalardır (`Kurogane`, `Sora`, `Valken`, `Tauron`, `Aether`, `Boreas`, `Vandal`, `Nyx`, `Solstice`, `Valen`, `Draco`, `Cerberus`, `Apex-Zero`, `Aurelia`, `Nocturne`).
- **Yarış disiplinleri:** Sprint, Pist (Circuit), Drift (finalde Karamara Büyük Turu karma rota).

### 5.2 Örgütsel İşlev Dağılımı

Yarış organizasyonu, finans/aklama, araç hazırlığı (alt & üst kademe), eleman toplama, bilgi yönetimi ve kamera ağı, lojistik (liman & kurye), yasa dışı bahis (sanayi & merkez), dış ilişkiler & nüfuz, teknik operasyonlar (sabotaj) ve iç disiplin (saha & komuta) 16 üyeye dağıtılmıştır.

### 5.3 Kesinleşmiş 16 Kişilik BLACK NUMBERS Kadrosu (`Q-04`, `Q-05`, `S-03`)

#### Katman 1 — Sokak Ağı (`#16`–`#13`, Perde 2 Başlangıcı, Heat 1–3)

| Sıra | İsim | Örgütsel İşlev | Kişilik | Sürüş Tarzı | Özgün Araç | Disiplin | Verdiği Bilgi / Delil |
|---|---|---|---|---|---|---|---|
| **#16** | **Berkay "Kıvılcım" Tuna** | Eleman Toplama | Gösterişçi çaylak | Agresif geç fren | *Sora CX-200* *(Kompakt Spor Coupe)* | Sprint | BN sokak buluşma frekansları ve alt kademe yarış haritası (`R2` başlangıcı) |
| **#15** | **Nadia "Apex" Kovacs** | Yarış Organizasyonu *(Alt Rota)* | Disiplinli, soğuk | Temiz çizgi, sıfır hata | *Valken RS-4* *(AWD Hot Hatch)* | Pist | Örgütün resmi rota, hakem ve gözcü şeması |
| **#14** | **Cem "Duman" Aksoy** | Yasa Dışı Bahis *(Sanayi)* | Geveze, sinirli | Geniş açılı sert drift | *Kurogane S14-R* *(Arkadan İtişli Drift Coupe)* | Drift | 5 yıl önceki Kuzey Geçidi yarışında Emre aleyhine açılan olağandışı yüksek oranlı tek bahis kaydı |
| **#13** | **Dario "Kargo" Moretti** | Lojistik *(Liman Girişi)* | Pragmatik kaçakçı | Ağır şasiyle blokaj | *Tauron V6 GT* *(Ağır Kaslı Coupe)* | Sprint | Eski Liman'dan gizlice sokulan gümrüksüz yarış parçası irsaliyeleri |

#### Katman 2 — Operasyon Ağı (`#12`–`#09`, Perde 2 Zirve, Heat 3–5)

| Sıra | İsim | Örgütsel İşlev | Kişilik | Sürüş Tarzı | Özgün Araç | Disiplin | Verdiği Bilgi / Delil |
|---|---|---|---|---|---|---|---|
| **#12** | **Leyla "Kısa Devre" Özkan** | Araç Hazırlığı *(Sokak Atölyesi)* | Ketum teknisyen | Hafif şasi, hızlı viraj çıkışı | *Aether K-7* *(Hafif Orta-Motor Roadster)* | Pist | **`R3` — İlk somut delil (`C05`):** Emre'nin patlayan yakıt hattındaki valfin standart yarış parçası olmadığını gösteren özel sipariş kodu → Tarık'ta ilk şüphe başlar |
| **#11** | **Ozan "Sıfır" Yıldız** | Yasa Dışı Bahis *(Merkez)* | Hesapçı kumarbaz | Riskli geç sollamalar | *Boreas M3-X* *(Sport Sedan)* | Sprint | Kaza gecesi merkez ve geçit kameralarının bahis masası emriyle kapatıldığı nöbet kaydı |
| **#10** | **Sasha "Ters Açı" Volkov** | Yarış Organizasyonu *(Dağ / Drift)* | Soğukkanlı şovmen | Sürekli bağlı drift zinciri | *Kurogane RX-Apex* *(Rotary Drift Coupe)* | Drift | Kaza gecesi Kuzey Geçidi start noktasında görülen kayıt dışı ikinci garaj aracının tanıklığı |
| **#09** | **Murat "Balyoz" Güneş** | İç Disiplin *(Saha)* | Kaba kuvvet, korkutucu | Temasçı, iterek yol açar | *Vandal 440R* *(V8 Muscle Coupe)* | Pist | Örgütün konuşmaya çalışan eski yarışçıları nasıl sindirdiği ve sahte tanık baskısı kayıtları |

#### Katman 3 — İstihbarat & Finans (`#08`–`#05`, Perde 3, Heat 6–8)

| Sıra | İsim | Örgütsel İşlev | Kişilik | Sürüş Tarzı | Özgün Araç | Disiplin | Verdiği Bilgi / Delil |
|---|---|---|---|---|---|---|---|
| **#08** | **Hakan "Lens" Ertürk** | Bilgi Yönetimi & Kamera Ağı | Paranoyak, kurnaz | Savunmacı, kestirme kullanan | *Nyx Evo-IX* *(Ralli Kökenli AWD)* | Sprint | **`R4` — Kimlik ifşası (`C06`):** Yenilince polise yüz/plaka eşleşmesi sızdırır → Kerem deşifre olur, **Heat 6 & Demir açılır**; ele geçen diskte olay yeri fotoğraf manipülasyonu bulunur |
| **#07** | **Sofia "Krom" Lindqvist** | Araç Hazırlığı *(Üst Kademe)* | Mükemmeliyetçi | Keskin apex, yüksek tutuş | *Solstice GT-S* *(Avrupa Spor Coupe)* | Pist | Özel üretim termal tetikli NOS valfinin yalnızca 2 adet üretildiği ve üst kademeye teslim edildiği belgesi |
| **#06** | **Miraç "Borsa" Alkan** | Finans *(Aklama)* | Takım elbiseli kibirli | Yüksek düzlük hızı, erken gaz | *Valen V10 Coupe* *(Lüks GT)* | Sprint | **`R5` — Selin'in polis kimliği açığa çıkar (`C07`):** Şifreli mali kayıtlarla birlikte polisin BN takip dosyasından sızan evrak çıkar; Kerem Selin'in Demir'in biriminde polis olduğunu öğrenir |
| **#05** | **Kaan "Kurye" Vural** | Lojistik *(Üst Kademe)* | Soğuk profesyonel | Her zeminde dengeli drift-grip | *Draco RS-T* *(Hafif Twin-Turbo Coupe)* | Drift | **`R6` — Tarık ile tam barışma (`C08`):** 5 yıl önce Kerem'in garajına gece girip tork anahtarı ve eldiven çalan kuryenin sevkiyat kaydı → Tarık gerçeği kabul eder, **Tier 6 açılır** |

#### Katman 4 — Komplo Çekirdeği (`#04`–`#01`, Perde 4, Heat 8–10 → Final)

| Sıra | İsim | Örgütsel İşlev | Kişilik | Sürüş Tarzı | Özgün Araç | Disiplin | Verdiği Bilgi / Delil |
|---|---|---|---|---|---|---|---|
| **#04** | **Rıza "Gölge" Karadağ** | İç Disiplin *(Komutan)* | Acımasız infazcı | Yüksek hızda blokaj ve geç fren | *Cerberus V8 GT* *(Ağır Süper-Sedan)* | Pist | **`R7`:** BN'nin kuruluş sermayesi ve ilk toplantısının Emre'nin ölümünden 11 gün sonra yapıldığını gösteren kurucu belgeler |
| **#03** | **İlhan "Valf" Korkmaz** | Teknik Operasyonlar *(Sabotajcı)* | Sessiz, hesaplı mühendis | Cerrahi virajlar, kusursuz drift çıkışı | *Apex-Zero Proto* *(Özel Karbon Coupe)* | Drift | Sabotajın adli kanıtı: ikinci yedek termal valf + Kerem'in garaj anahtar kopyası + Viktor'un şifreli iş emri |
| **#02** | **Elena Novak** | Dış İlişkiler & Nüfuz | Zarif ama buz gibi | Maksimum ortalama hız, sıfır tereddüt | *Aurelia V12 Corsa* *(Egzotik GT)* | Sprint | **`R8` (Son Kanıt):** 5 yıl önceki soruşturmayı savcılık/adli tıp katında hızla kapattıran ödeme zinciri ve Viktor Serez onay kayıtları |
| **#01** | **Viktor Serez** | Kurucu & Lider | Soğukkanlı, narsist vizyoner | Tüm disiplinlerde kusursuz tempo | *Nocturne BN-01* *(Özgün Karbon Hyper-GT)* | Karma *(Karamara Büyük Turu — Final)* | Final yüzleşmesi (`C10`–`C11`): Yenilir, Eski Liman Köprüsü kordonuna sıkıştırılıp tutuklanır → Demir'in çöküşü (`C12`) → **Özel Kademe** (`C13`) |

---

## 6. Ana Hikâye Perdeleri

### Önsöz — Orijinal Olay *(5 yıl önce)*
- Kerem, Viktor'un çevresine meydan okumak için Kuzey Geçidi'nde bir sokak yarışı ayarlar. İlhan (`#03`) önceden Kerem'in aletlerini çalarak Emre'nin yakıt/NOS hattına termal tetikli sabotaj valfi yerleştirmiştir.
- Emre yarış öncesi yakıt kokusundan şüphelenir; Kerem heyecanla geçiştirir. Araç yarış sırasında patlar ve Emre ölür (`C01`).
- Karışık deliller Kerem'i işaret eder; tutuklama kararı çıkar ve Kerem kaçar (`C02`). Elena (`#02`) adli süreci hızla kapattırır; genç komiser Demir dosyayı yeterli görüp kapatır.

### Perde 1 — Dönüş
- Selin'in mesajı Kerem'i 5 yıl sonra Karamara'ya getirir (`C03`). Selin polis olduğunu gizler ve henüz Kerem'in masumiyetine tamamen ikna olmamıştır.
- Kerem **"K. Vance"** kimliğiyle döner; Selin Yamaç Mahallesi'ndeki Emre'den kalan kayıt dışı eski depoyu garaj olarak verir.
- Tarık bu dönüşe ve Selin'in yardımına karşı çıkar (`C04`).

### Perde 2 — Yükseliş (`#16`–`#09`)
- Kerem Katman 1–2 yarışçılarıyla yarışarak bilgi ve erişim toplar (`R2`).
- `#12` Leyla'dan **ilk somut delil** (özel sipariş valf kodu, `R3`, `C05`) ele geçer. Tarık engellemeyi sürdürse de içinde ilk şüphe başlar.
- Heat 1–5 arasında polis Kerem'i yalnızca **"K. Vance"** adlı kayıtsız sokak yarışçısı olarak kovalar (`S-08`).

### Perde 3 — İfşa (`#08`–`#05`)
- `#08` Hakan Ertürk yenilince polise yüz ve plaka eşleşmesini sızdırır; **Kerem'in gerçek kimliği ifşa olur (`R4`, `C06`)**. Büyük ve oynanabilir bir kaçış olayı yaşanır.
- Başkomiser Demir, 5 yıldır aranan firarinin geri döndüğünü öğrenir ve **Heat 6 açılır**.
- `#06` Miraç'ın mali evraklarından sızan takip dosyasıyla **Selin'in polis kimliği ortaya çıkar (`R5`, `C07`)** ve güven çatışması yaşanır.
- `#05` Kaan'ın gece garaj hırsızlığı sevkiyat kaydıyla **Tarık tamamen barışır (`R6`, `C08`)** ve **Tier 6 açılır** (`S-09`).

### Perde 4 — Zirve (`#04`–`#01`)
- Heat 7–10 arasında Demir giderek daha agresif ve saplantılı hâle gelir.
- `#04` Rıza, `#03` İlhan ve `#02` Elena'dan gelen belgelerle Viktor'un komplosu tüm delil zinciriyle kanıtlanır (`R7`–`R8`, `C09`).
- **Son yarış (`S-10`, `C10`)** olan Karamara Büyük Turu'nda Kerem, Viktor Serez'i 1v1 yener ve onu Selin'in koordine ettiği **Eski Liman Köprüsü** kordonuna sıkıştırıp polise teslim eder (`S-04`, `C11`). BLACK NUMBERS çöker.

### Perde 5 — Son Kovalamaca
- Viktor'un suçunun kanıtlanması Kerem'in dönüşte işlediği yasa dışı yarışları ve **Şehir Faturası**nda biriken tahribatı silmez (`S-12`).
- Hem 5 yıl önce yanlış dosyayı kapattığını hazmedemeyen hem de şehirdeki yıkımı kişisel hakaret sayan Demir kontrolünü kaybeder (`C12`, `S-15`): **"ONU BANA GETİRİN! ÖLÜ YA DA DİRİ!"**
- **Özel Kademe** son kovalamaca başlar (`C13`). Selin bu hukuksuz telsiz emrini İçişleri'ne iletir ve Kuzey Viyadüğü'ndeki bir kordonu 15 saniye geciktirir. Tarık ise telsiz/kaçış tüneli bilgisini paylaşabilir.
- Kerem Speedbreaker manevraları ve rota seçimiyle Kuzey Viyadüğü üzerinden **iki taktiksel çıkıştan birini** (**Liman Tünelleri** veya **Kuzey Geçidi toprak servis yolu**, `Q-11`) kullanarak sınırı geçer.

### Epilog
Bkz. [§12](#12-son-ve-epilog).

---

## 7. Polis Amiri Demir Arslan — Karakter Yayı

**Temel ilkeler:**
- Demir **yozlaşmış değildir** ve BLACK NUMBERS adına **çalışmaz**.
- Yayı, aşırı gurur, 5 yıl önceki kapatılmış dosya hatasının utancı ve şehir tahribatının doğurduğu saplantının bir insanın muhakemesini nasıl yok edebileceğini gösterir.
- Hikâye **polis teşkilatının tamamını yozlaşmış veya kötü niyetli göstermemelidir**.

| Aşama | Demir'in Durumu |
|---|---|
| Başlangıç (Heat 1–5) | Şehri koruduğuna gerçekten inanır. Ekibi sokaktaki kayıtsız **"K. Vance"** sürücüsünü standart prosedürle kovalar (`S-08`). |
| Kimlik ifşası (Heat 6) | Hakan'ın sızıntısıyla K. Vance'in 5 yıldır aranan Kerem Yalçın olduğunu öğrenir. İş kişiselleşir; sahaya bizzat iner. Emri: **CANLI yakalayın.** |
| Heat 7–10 | Oyuncu her kaçtığında ve Şehir Faturası büyüdüğünde öfkesi ve saplantısı artar. **"Canlı yakalama" emri hâlâ geçerlidir.** |
| Özel Kademe (`C12`–`C13`) | Viktor tutuklanıp 5 yıl önceki dosyanın manipüle edildiği ortaya çıkınca Demir yanlış adamı avladığı gerçeğiyle ve onca tahribatı durduramamış olmanın utancıyla kırılır: **"ONU BANA GETİRİN! ÖLÜ YA DA DİRİ!"** |
| Sonuç (`C15`) | Selin'in İçişleri'ne sunduğu telsiz kaydı ve hukuka aykırı ölüm emri tanıklığıyla (`S-05`) görevden alınır, soruşturulur ve tutuklanır. |

**Tematik karşıtlık:** Tarık kinini aşmayı öğrenir. Demir saplantısını **aşamaz**.

---

## 8. Heat Seviyelerinin Anlatıdaki Yeri

Oyunda **10 normal Heat seviyesi** ve **1 Özel Kademe** bulunur.

| Seviye | Açılma Koşulu | Anlatıdaki Anlamı | Demir'in Emri |
|---|---|---|---|
| **Heat 1–5** | Standart ilerleme | Polis Kerem'i **"K. Vance"** adlı kayıtsız sokak yarışçısı olarak kovalar (`S-08`) | — |
| **Heat 6** | `#08` Hakan sonrası kimlik ifşası | Demir gerçek kimliği öğrenip sahaya iner; takip sertleşir | **CANLI** |
| **Heat 7–10** | Katman 3–4 ilerlemesi | Barikatlar, ağır interceptor'lar, Şehir Faturası takibi | **CANLI** *(hâlâ geçerli)* |
| **Özel Kademe** | **Yalnızca** Perde 5 final kovalamacası | Tüm Karamara birimleriyle dev kapanış operasyonu | **ÖLÜ YA DA DİRİ** |

**Özel Kademe için anlatı ve oynanış kuralları:**
- Oyuncu polisin tamamını çarparak yenemez; **Speedbreaker zaman yavaşlatma, sürüş becerisi ve taktiksel rota seçimiyle** (Liman Tünelleri veya Kuzey Geçidi toprak servis yolu) kaçmalıdır.
- Demir'in emrinin hukuki ve mesleki sonuçları gösterilir (`C15`).

---

## 9. Araç Geliştirmenin Anlatıdaki Yeri

| Kademe | Kaynak | Anlatıdaki Anlamı |
|---|---|---|
| **Tier 1–5** | Doğu Sanayi ve Liman'daki sıradan atölyeler | Standart mekanik ve NOS ilerlemesi |
| **Tier 6** | **Yalnızca Tarık Kaya**, Perde 3–4'te tam barışma sonrası (`R6`) | Emre'yle yarım bıraktıkları projenin dostluk anısına **son bir kez** tamamlanması |

**Tier 6 kuralları:**
- Özel basınçlı NOS ve yüksek verimli performans parçaları içerir; **dengeli araç fiziğini korur**.
- Yalnızca Tarık barıştıktan sonra açılır; epilogda yasa dışı dünyayı kalıcı reddinin simgesi olarak geri döner.

---

## 10. Delil ve İfşa İlerleyişi

### 10.1 Orijinal Delil Seti ve Adli Açıklaması (`Q-06`, `Q-07`)

| Delil Türü | Gerçek Kaynağı ve Kahramanın Tepkisi |
|---|---|
| Kerem'den çalınmış **gerçek eşyalar** *(tork anahtarı, eldiven)* | `#05` Kaan gece garajdan çalıp `#03` İlhan'a teslim etmiştir. Kerem eşyaları tanır ama açıklayamaz |
| Stratejik noktalara bırakılmış **gerçek parçalar** | Olay yeri çevresine kasten bırakılmıştır |
| Kerem'e yanlış atfedilmiş **sahte eşyalar** | İlhan'ın laboratuvarından gelen özel valf artıkları; Kerem hiç görmemiştir |
| **Manipüle edilmiş adli kayıt ve fotoğraflar** | `#08` Hakan'ın kamera kesintisi ve fotoğraf manipülasyonu + `#02` Elena'nın hızlandırılmış savcılık kapatması |
| Sahip olduğunu **hatırlamadığı** eşyalar | Çalıntı kopya anahtar ve kayıt dışı fatura izleri; Kerem emin değildir |

### 10.2 İfşa Kilometre Taşları

| # | Kilometre Taşı | Tetikleyen Boss / Olay | Sonucu |
|---|---|---|---|
| **R1** | Selin dosyadaki hızlı kapatma tutarsızlığını fark eder | Önsöz → Perde 1 | Kerem Karamara'ya çağrılır |
| **R2** | Alt kademe zaferleri iletişim ve rota ağını açar | `#16`–`#13` | Örgütün iç işlevi görünür hâle gelir |
| **R3** | **İlk somut delil:** özel sipariş valf kodu (`C05`) | `#12` Leyla Özkan | Tarık'ın kuşkusu başlar |
| **R4** | **Kerem'in kimliği ifşa olur** (`C06`) | `#08` Hakan Ertürk | **Heat 6 açılır**, Demir sahaya iner |
| **R5** | **Selin'in polis kimliği açığa çıkar** (`C07`) | `#06` Miraç Alkan | Güven çatışması yaşanır |
| **R6** | Garaj hırsızlığı sevkiyat kaydıyla **Tarık barışır** (`C08`) | `#05` Kaan Vural | **Tier 6 açılır** |
| **R7** | BN'nin Emre'nin ölümünden 11 gün sonra kurulduğu belgelenir | `#04` Rıza Karadağ | Komplonun kuruluş amacı kanıtlanır |
| **R8** | Sabotaj emri ve adli kapatma ödeme zinciri tamamlanır (`C09`) | `#03` İlhan & `#02` Elena | Viktor'un suçu belgelenir, final yüzleşmesi açılır |

---

## 11. Son Yüzleşme ve Son Kovalamaca

### 11.1 Viktor Serez ile Yüzleşme (`S-04`, `S-10`)
- Karamara Büyük Turu'nda 1v1 gerçekleşir (`C10`).
- Viktor'un sorumluluğu R3–R8 arası toplanan **adli delillerle** kurulur; **yalnızca itirafa dayanılmaz**.
- Kerem yarışı kazanıp Viktor'un aracını Selin'in koordine ettiği **Eski Liman Köprüsü** kordonuna sıkıştırır; Viktor polise teslim edilir (`C11`). BLACK NUMBERS çöker. Kerem'in Emre'nin ölümündeki masumiyeti resmen kanıtlanır.

### 11.2 Son Polis Kovalamacası ve Şehir Faturası (`S-14`, `Q-11`)
- Viktor'un yakalanması Kerem'in dönüşte işlediği yasa dışı yarışları ve **Şehir Hasar Kaydı (Şehir Faturası)** içinde biriken tahribatı silmez.
- Demir psikolojik olarak kırılır (`C12`): **"ONU BANA GETİRİN! ÖLÜ YA DA DİRİ!"**
- **Özel Kademe** başlar (`C13`). Selin emri İçişleri'ne ihbar edip tek bir kordonda kısa boşluk bırakır; Kerem iki kaçış rotasından biriyle Karamara sınırını geçer.
- **Tahribat gizlenmez:** Kırılan Pursuit Breaker yapıları, kamu altyapısı ve sivil hasar hem oynanışta **Şehir Faturası** olarak ölçülür hem de Epilog haberlerinde sonuçsuz bir eğlence değil, ağır bir bedel olarak gösterilir.

---

## 12. Son ve Epilog

Son **acı-tatlıdır.**

| Unsur | Final Durumu |
|---|---|
| **BLACK NUMBERS** | Çökertilmiş, Viktor Serez dâhil tüm üyeleri tutuklanmıştır. |
| **Karamara Şehri** | Şehir Faturası'ndaki yıkım haber yayınlarında gösterilir (`C14`); şehir zamanla yaralarını sarar. |
| **Demir Arslan** | Selin'in ilettiği telsiz kaydıyla görevden alınmış ve tutuklanmıştır (`C15`). |
| **Selin Karahan** | Emre'nin ölümünün gerçeğini öğrenmiş, hukuksuz emri ihbar ettiği için mesleğinde temiz kalmıştır (`C17`). |
| **Tarık Kaya & Arda Çelik** | Doğu Sanayi'de yasal tamirhane ve bitişiğindeki yasal araç galerisi düzeninde namuslu hayatlarına devam ederler. |
| **Kerem Yalçın** | Kuzey Geçidi mezarlığında Emre'nin mezarını ziyaret eder (`C18`), ardından Karamara dışında **adsız küçük bir kıyı kasabasına** çekilir (`Q-14`, `C19`). Yarışa bir daha dönmez. |

### 12.1 Tarık'ın Dükkânı — Kritik Sahne (`C16`)

- Epilogda Doğu Sanayi'de **Tarık Kaya'nın oto tamirhanesi** ile eski çırak **Arda Çelik'in** işlettiği **yasal araç galerisi** yan yanadır.
- Genç bir müşteri **Tier 6 performans modifikasyonları** hakkında soru sorar.

**Diyalog (kanon):**

> **Tarık (Mekanikçi):** "O günler eskide kaldı, adamım."
>
> *(Müşteri ısrar eder ve daha fazla para teklif eder.)*
>
> **Tarık (Mekanikçi):** "Dükkânımı terk et. Yoksa polisi ararım."

> ⚠️ **Bu sahne dokunulmazdır.** Yasa dışı yarışı nostaljik biçimde kutlayan bir sahneyle **değiştirilmemelidir**.

### 12.2 Kerem'in Kapanışı
- Emre'nin mezarını ziyaret eder, arabasının anahtarlarını bırakır ve adsız küçük bir kıyı kasabasında sessizce yaşar.
- Asıl ölümden masumdur, ama sonraki tahribattan aklanmamıştır; hâli **huzurlu, ama tamamen özgür değildir**.

---

## 13. Ana Temalar

- Suçluluk ile masumiyet; hukuki aklanma ile vicdani sorumluluk arasındaki çizgi
- Manipüle edilmiş deliller ve hızlı kapatılan adalet
- İhanet, kırılan güven ve kademeli barışma (Tarık) ↔ aşılamayan saplantı (Demir)
- Yıkıcı hırslardan ne zaman vazgeçileceğini öğrenmek (Tier 6 reddi)
- Sokak yarışının gerçek ve ölçülebilir insani/kentsel bedeli (Şehir Faturası)

---

## 14. Sinematik Gereksinimler

| # | Sahne | Perde |
|---|---|---|
| C01 | Kuzey Geçidi yarışı ve Emre'nin aracındaki patlama | Önsöz |
| C02 | Karışık deliller, hızlı kapanan dosya ve Kerem'in kaçışı | Önsöz |
| C03 | 5 yıl sonra Selin'in Kerem'i geri getiren mesajı | Perde 1 |
| C04 | Doğu Sanayi'de Tarık'la yüzleşmeler | Perde 1–2 |
| C05 | `#12` Leyla'dan ilk somut delilin (özel valf kodu, `R3`) çıkması | Perde 2 |
| C06 | `#08` Hakan sonrası Kerem'in kimliğinin ifşası (`R4`, Heat 6) | Perde 3 |
| C07 | `#06` Miraç dosyasıyla Selin'in polis kimliğinin açığa çıkması (`R5`) | Perde 3 |
| C08 | `#05` Kaan belgesiyle Tarık'la barışma ve Tier 6 kilidinin açılması (`R6`) | Perde 3–4 |
| C09 | `#04`–`#02` belgeleriyle Viktor'un komplosunun kanıtlanması (`R7`–`R8`) | Perde 4 |
| C10 | Karamara Büyük Turu — Viktor Serez ile final yarışı | Perde 4 |
| C11 | Eski Liman Köprüsü kordonunda Viktor'un tutuklanması | Perde 4 |
| C12 | Demir'in psikolojik çöküşü (*"ÖLÜ YA DA DİRİ!"*) | Perde 5 |
| C13 | Özel Kademe final kovalamacası (Kuzey Viyadüğü → iki çıkış rotası) | Perde 5 |
| C14 | Şehir Faturası ve hasarlı Karamara'nın haber görüntüleri | Epilog |
| C15 | Demir'in Selin'in kaydıyla görevden alınması ve tutuklanması | Epilog |
| C16 | Tarık'ın Tier 6 işini reddetmesi (kanon replikler) | Epilog |
| C17 | Selin'in sıradan polis işine temiz dönüşü | Epilog |
| C18 | Kerem'in Kuzey Geçidi mezarlığında Emre'yi ziyareti | Epilog |
| C19 | Kerem'in adsız kıyı kasabasında sessiz bir hayata kaybolması | Epilog |

---

## 15. Oynanışla Bağlantı

| Hikâye / Sistem Unsuru | Oynanış Karşılığı |
|---|---|
| **Sokak Yarışı & Perfect Launch** | Sprint, Pist, Drift disiplinleri; geri sayımda devir tutturma (`PERFECT LAUNCH`) ile agresif kalkış |
| **NOS & Near-Miss** | Drift, yüksek hız ve yakın geçişle dolan nitro; **Tier 6** ile Tarık'ın özel basınçlı ünitesi açılır |
| **Speedbreaker** | Zamanı yavaşlatıp viraj tutuşunu artıran odak mekaniği; **Özel Kademe** kaçışında barikat/rota seçiminin kilit aracı |
| **16'lık Liste & Kanıt Avı** | `#16`–`#01` boss yarışları, zaferde R2–R8 kanıt dosyalarının açılması |
| **Kimlik İfşası (`R4`) & Yükseliş** | **Heat 6** ve Demir'in sahaya inişi; Perde 4'te **Heat 7–10** |
| **Pursuit Breakers ↔ Şehir Faturası** | Kırılabilir yapılar ve sivil hasar **Şehir Hasar Kaydı**na işler; Demir'in öfkesini ve Epilog haberlerini besler |
| **Son Kovalamaca** | **Özel Kademe**; 2 alternatif kaçış rotası ile şehri terk etme |

---

## 16. Kapsam Dışı

Bu doküman final seslendirme metinlerini (§12.1'deki kanon replikler hariç) ve Sequencer kamera açılarını yazmaz; ancak tüm anlatı kararları bağlayıcı olarak kilitlenmiştir.

---

## 17. Onaylanmış Kanon

Aşağıdaki maddeler **bağlayıcıdır**:

1. Kerem Yalçın orijinal sabotajı **yapmamıştır.**
2. Emre Karahan'ın aracı İlhan Korkmaz (`#03`) tarafından termal tetikli valfle sabote edilmiş ve yarış sırasında patlamıştır. Sürücü ölmüştür.
3. Komployu BLACK NUMBERS'ın kurucusu ve `#01` üyesi Viktor Serez kurmuştur. Amacı iki yarışçı engelini tek hamlede ortadan kaldırmaktır.
4. Deliller gerçek, çalınmış (`#05` Kaan eliyle Kerem'in garajından), yerleştirilmiş, sahte ve manipüle edilmiş (`#08` Hakan & `#02` Elena) öğelerin karışımıdır. Kerem kendi eşyalarını gerçekten tanır.
5. Kerem hakkında tutuklama kararı çıkmış, Kerem tutuklanmadan kaçmıştır. Soruşturma Elena'nın müdahalesiyle alışılmadık hızda kapatılmıştır.
6. Kerem hayatta kalan suçluluğu taşır (yarışı Viktor'a meydan okumak için ayarlamış, Emre'nin yarış öncesi kokudan şüphelenmesini heyecanla geçiştirmiştir).
7. Tarık Kaya başta Kerem'i suçlu sanır, oyunun başında onu engeller, `#12` Leyla'dan `#05` Kaan'a **kademeli olarak** fikrini değiştirir ve barışır.
8. Barışma **Tier 6**'yı açar (Emre'yle yarım bıraktıkları projeyi son kez tamamlar). Tier 1–5 sıradan mekanikçilerdedir. Tier 6 dengeli fiziği korur.
9. Selin Karahan polis olmuştur; Demir'in kendi biriminde görev yapar. Resmî görevi BN'yi çökertmektir. Eşinin davasını resmî atama olmadan, kendi inisiyatifiyle inceler.
10. Selin polis olduğunu gizleyerek 5 yıl sonra Kerem'le iletişime geçer; Yamaç Mahallesi'ndeki Emre'den kalan kayıt dışı garajı ve polis sistemine dokunmadan **"K. Vance"** sahte kimliğini sağlar. Başta masumiyetine tamamen ikna olmamıştır.
11. Selin'in polis kimliği `#06` Miraç dosyasında açığa çıkar ve çatışma yaratır. Güveni sonunda gerçek olur.
12. BLACK NUMBERS `#16`'dan `#01` Viktor Serez'e kadar 16 sıralı büyük yarışçıdan oluşur. Her üyenin örgütsel bir işlevi vardır. Rütbe yükseldikçe bilgi artar. Disiplinler Sprint, Pist ve Drift'tir.
13. Kimlik ifşası `#08` Hakan sonrası orta-geç ilerlemede olur (`R4`), büyük ve oynanabilir bir olaydır ve Heat 6'yı açar.
14. Başkomiser Demir Arslan **yozlaşmış değildir.** Saplantısı, 5 yıl önceki hatası ve Şehir Faturası öfkesi onu çöküşe götürür.
15. Heat 6–10 boyunca Demir'in emri **canlı yakalamaktır.**
16. Özel Kademe yalnızca son hikâye kovalamacasında açılır. Replik: **"ONU BANA GETİRİN! ÖLÜ YA DA DİRİ!"**
17. Özel Kademeden polisin tamamını yenerek değil, Speedbreaker becerisi ve rota seçimiyle (Liman Tünelleri veya Kuzey Geçidi servis yolu) kaçılır.
18. Polis teşkilatının tamamı yozlaşmış gösterilmez.
19. Viktor'un sorumluluğu R3–R8 arası toplanan delillerle kanıtlanır. Viktor Karamara Büyük Turu'nda yenilir ve Eski Liman Köprüsü'nde polise teslim edilir. BN çöker.
20. Viktor'un yenilmesinden sonra Kerem hâlâ aranır. Son kovalamacada Selin emri İçişleri'ne ihbar edip kordonda kısa boşluk bırakarak kaçışa yardım eder.
21. Son kovalamacanın tahribatı oynanışta **Şehir Faturası** ve epilogda haberlerle gösterilir; sonuçsuz eğlence olarak sunulmaz.
22. Demir görevden alınır, Selin'in kaydettiği telsiz emriyle soruşturulur ve tutuklanır.
23. Selin gerçeği öğrenir ve hukuksuz emri ihbar ettiği için polis olarak görevine devam eder.
24. Tarık yasa dışı dünyayı kalıcı olarak bırakır. Epilogda Arda Çelik'in galerisinin yanındaki Tier 6 reddi sahnesi ve iki replik kanondur.
25. Kerem Kuzey Geçidi'nde Emre'nin mezarını ziyaret eder, adsız bir kıyı kasabasına çekilir ve sessizce firari olarak yaşar. Yarışa dönmez. Son acı-tatlıdır.

---

## 18. Karara Bağlanmış Anlatı Detayları Arşivi (2026-11-20)

Önceki açık soruların (`Q-01..Q-14`) ve süreklilik maddelerinin (`S-01..S-15`) tamamı yukarıdaki kanona bağlanmıştır:

| ID | Bağlanan Karar | Referans |
|---|---|---|
| `Q-01` | Karakter isimleri: Kerem Yalçın (`K. Vance`), Emre Karahan, Tarık Kaya, Selin Karahan, Demir Arslan, Viktor Serez, Arda Çelik | §0 |
| `Q-02` | Şehir adı ve 6 bölgesi: **Karamara** *(Port Mara)* — Eski Liman, Doğu Sanayi, Merkez Bulvar, Kuzey Viyadükleri, Yamaç Mahallesi, Kuzey Geçidi | §0 |
| `Q-03` | Kaza ile dönüş arası süre: **5 yıl** | §1, §2 |
| `Q-04`, `Q-05` | 16 BN üyesinin kimliği, kurgusal araçları, işlevleri ve R2–R8 kanıt dağılımı | §5.3 |
| `Q-06`, `Q-07` | `#05` Kaan'ın alet çalması, `#03` İlhan'ın termal valf montajı, `#02` Elena'nın hızlı adli kapatması | §1, §10.1 |
| `Q-08`–`Q-10` | İfşa (`#08` Hakan, `R4`), Selin kimliği (`#06` Miraç, `R5`), Tarık barışması (`#05` Kaan, `R6`) | §5.3, §10.2 |
| `Q-11` | Final yarışı (Karamara Büyük Turu → Eski Liman Köprüsü) ve 2 kaçış rotalı Özel Kademe | §6, §11 |
| `Q-12`, `S-11` | Epilog galeri ortağı: Eski çırak **Arda Çelik** | §0, §12.1 |
| `Q-13`, `S-05`–`S-07`, `S-13` | Selin Demir'in biriminde; kayıt dışı garaj & kimlik; telsiz ihbarı + 15 sn kordon boşluğu | §3.4 |
| `Q-14` | Kerem'in son konumu: Karamara dışında adsız küçük kıyı kasabası | §12 |
| `S-01`–`S-04`, `S-08`–`S-10`, `S-12`, `S-14`, `S-15` | Suçluluk sebebi, Tier 6 gri bölgesi, Viktor `#01`, Heat 1–5 K. Vance çerçevesi, Şehir Faturası ve C11→C12→C13 sırası | §3, §6–§11, §14 |

---

## Değişiklik Günlüğü

| Tarih | Değişiklik |
|---|---|
| 2026-11-20 | Q-01..Q-14 ve S-01..S-15'in tamamı kanona bağlandı: Karamara şehri, kesin karakter isimleri, 16 kişilik BN kadrosu + özgün kurgusal araçlar, süreklilik çözümleri ve NFS MW oynanış bağlantıları eklendi. |
| 2026-11-02 | İlk sürüm. Kaynak hikâye özeti dokümana aktarıldı, kanon ile açık konular ayrıldı. |

---

## Görsel ve Sanat Referansları

Detaylı görsel ilkeler için bkz. [ART_DIRECTION.md](ART_DIRECTION.md):
- Kahraman aracı (*Kurogane M-Spec*): [hero_car_kurogane.png](images/concept/hero_car_kurogane.png)
- Lider aracı (*Nocturne BN-01*): [boss01_nocturne.png](images/concept/boss01_nocturne.png)
- Karamara Özel Kademe kovalamacası: [karamara_pursuit.png](images/concept/karamara_pursuit.png)
- Tarık'ın Doğu Sanayi atölyesi (Tier 6): [tarik_garage_tier6.png](images/concept/tarik_garage_tier6.png)
