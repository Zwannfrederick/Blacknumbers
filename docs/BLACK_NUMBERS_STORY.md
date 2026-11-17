# BLACK NUMBERS — Resmî Anlatı Referansı

> **Durum:** Yaşayan doküman · **Son güncelleme:** 2026-11-02
> **Kapsam:** Yalnızca hikâye ve anlatı. Bu doküman oynanış ya da sinematik uygulaması **içermez**.
> **Hedef okuyucu:** Anlatı tasarımcıları ve projede çalışacak AI kodlama ajanları.

| Alan | Değer |
|---|---|
| Oyun adı | **BLACK NUMBERS** |
| Tür | Tek oyunculu, açık dünya, hikâye odaklı arcade/simcade sokak yarışı |
| Motor | Unreal Engine 5 |
| Ton | Olgun, acı-tatlı. Bir zafer kutlaması değil. |

---

## 0. Bu Dokümanı Kullanma Kuralları

1. **Kanon ile açık konuları ayırın.** [§17 Onaylanmış Kanon](#17-onaylanmış-kanon) bağlayıcıdır. [§18 Çözülmemiş Konular](#18-çözülmemiş-anlatı-detayları) ise henüz karara bağlanmamıştır. Açık konulara kendi başınıza kesin cevap vermeyin.
2. **Karakter isimleri kesin değildir.** Aşağıdaki tanımlayıcı yer tutucuları kullanın. "Demir" adı da geçicidir.
3. **Olay örgüsünü yeniden yazmayın.** Bir tutarsızlık görürseniz sessizce düzeltmeyin, §18'e soru olarak ekleyin.
4. **Hikâyeyle ilgisi olmayan mekanik eklemeyin.** Temel oynanış şunlardır: sokak yarışı, araç gelişimi, drift, polis kovalamacası ve hikâye ilerlemesi.

### Karakter Yer Tutucuları

| Yer tutucu | Kısaltma | Rol |
|---|---|---|
| **Kahraman** | KHR | Eski yeraltı sokak yarışçısı, oyuncu karakteri |
| **Ölen Dost** | DOST | Kahramanın en yakın arkadaşı, yetenekli yarışçı. Sabotaj sonucu ölür |
| **Mekanikçi** | MEK | Gruptan hayatta kalan arkadaş, usta otomotiv mekanikçisi |
| **Polis Memuru (Ölen Dostun Eşi)** | EŞ | Ölen Dostun eşi, sonradan polis memuru olur |
| **Lider** | LDR | BLACK NUMBERS'ın kurucusu ve lideri, asıl kötü karakter |
| **Demir** *(geçici ad)* | DMR | Yeraltı yarış sahnesine karşı operasyonları yöneten kıdemli polis amiri |

---

## 1. Hikâyeye Genel Bakış

BLACK NUMBERS, yıllar önce en yakın arkadaşının arabasını sabote etmekle haksız yere suçlanan eski bir yeraltı sokak yarışçısının şehre dönüşünü anlatır. Kahraman bu suçlamayla arkadaşının ölümüne yol açmış sayılmıştır.

Bir zamanlar dört kişilik sıkı bir arkadaş grubu vardır: iki yetenekli yarışçı (Kahraman ve Ölen Dost), usta bir mekanikçi ve Ölen Dostun eşi. İki yarışçı şehrin yeraltı yarış sahnesinde son derece başarılı ve etkilidir.

Aynı dönemde hırslı bir kişi yeraltı yarış camiasını örgütleyip tekeline almaya çalışmaktadır. Amacı **BLACK NUMBERS** adlı bir yapı kurmak, sahneyi kontrol etmek ve onu para, prestij, nüfuz ve güç için kullanmaktır. İki yarışçının bağımsızlığı ve popülerliği bu planın önündeki en büyük engeldir.

Lider iki engeli tek bir komployla ortadan kaldırır. Ölen Dostun aracı sabote edilir ve araç yarış sırasında patlar. Ardından suç, delil manipülasyonuyla Kahramanın üzerine yıkılır.

Yıllar sonra Ölen Dostun eşi soruşturmadaki tutarsızlıkları fark eder ve Kahramanı geri çağırır. Kahraman sahte bir kimlikle şehre döner, BLACK NUMBERS'ın 16 kişilik hiyerarşisinde yükselir ve komployu adım adım ortaya çıkarır.

---

## 2. Kronolojik Zaman Çizelgesi

> Olayların sırası kanondur. Kesin süreler ve bölüm numaraları açıktır (bkz. §18).

| # | Dönem | Olay |
|---|---|---|
| T0 | Geçmiş | Dört arkadaş bir aradadır. İki yarışçı yeraltı sahnesinde zirvededir. |
| T1 | Geçmiş | Lider sahneyi tekeline alma planını yürütür. İki yarışçı en büyük engeldir. |
| T2 | Geçmiş | **Orijinal Olay:** Ölen Dostun aracı gizlice sabote edilir, yarışta patlar ve sürücü ölür. |
| T3 | Geçmiş | Soruşturma manipüle edilir. Karışık deliller Kahramanı işaret eder. |
| T4 | Geçmiş | Kahraman hakkında tutuklama kararı çıkar. Kahraman tutuklanmadan önce kaçar. |
| T5 | Geçmiş | Soruşturma alışılmadık bir hızla, eldeki deliller yeterli sayılarak kapatılır. |
| T6 | Ara yıllar | Grup dağılır. Kahraman ortadan kaybolur, Mekanikçi kin besler, Eş polis olur, Lider BLACK NUMBERS'ı kurar ve genişletir. |
| T7 | Oyunun başı | Eş tutarsızlıkları keşfeder ve Kahramanla iletişime geçer. Polis olduğunu gizler. |
| T8 | Perde 1 | Kahraman sahte kimlikle döner. Eş garajı ve araçları sağlar. |
| T9 | Perde 2 | BLACK NUMBERS'ta yükseliş başlar. İlk deliller ortaya çıkar. Mekanikçi engel olur. |
| T10 | Perde 3 | **Kimlik ifşası** olur, Heat 6 açılır ve Demir olaya kişisel olarak dahil olur. |
| T11 | Perde 3–4 | Eşin polis kimliği açığa çıkar. Mekanikçi kademeli olarak barışır ve Tier 6 açılır. *(Sıralama açıktır.)* |
| T12 | Perde 4 | Kahraman zirveye ulaşır. Lider yenilir ve polise teslim edilir. BLACK NUMBERS çöker. |
| T13 | Perde 5 | **Son polis kovalamacası** başlar ve Özel Kademeye ulaşır. Eş kaçışa yardım eder. |
| T14 | Epilog | Şehirdeki hasar görünür. Demir görevden alınıp tutuklanır. Kahraman sessiz bir firari hayatına çekilir. |

---

## 3. Ana Karakterler

### 3.1 Kahraman (KHR)

- **Geçmişi:** Yetenekli bir yeraltı sokak yarışçısıdır. Ölen Dostla birlikte sahnenin en etkili iki isminden biridir.
- **Yaşadığı yara:** Arkadaşının ölümünden haksız yere suçlanır, tutuklama kararından önce kaçar ve yarışı bırakır.
- **İç çatışması:** Sabotajı **yapmamıştır**, ama derin bir hayatta kalan suçluluğu taşır. Ölümden kendini kısmen sorumlu tutar.
- **Delillerle ilişkisi:** Delillerin bir kısmını gerçekten tanır ve bunların olay yerine nasıl geldiğini açıklayamaz. Bu yüzden kendini savunması zordur.
- **Yayı:** Firari → sahte kimlikle dönüş → komployu çözme → asıl ölümden aklanma → sonradan işlediği suçların bedelini taşıyarak sessiz bir firari hayatı.
- **Sonu:** Orijinal ölümden masum olduğu kanıtlanır, ama sonraki yasa dışı yarışlardan ve verdiği tahribattan **aklanmaz**. Sokak yarışına dönmez.

### 3.2 Ölen Dost (DOST)

- Kahramanın en yakın arkadaşı ve yetenekli bir yarışçıdır. Eşle evlidir.
- Lider için Kahramanla birlikte en büyük iki engelden biridir.
- Aracı sabote edilir ve yarış sırasında patlamada ölür.
- Hikâyede doğrudan yer almaz. Varlığı anılarda, delillerde ve mezar sahnesinde hissedilir.

### 3.3 Mekanikçi (MEK)

- **Olaydan sonra:** Kahramanın suçlu olduğuna inanır ve ona derin bir kin besler. Şiddetli yeraltı yarış ortamından uzaklaşır, ama arabalarla çalışmayı sürdürür.
- **Oyunun başında:** Kahramanın dönüşüne ve özellikle Eşten yardım almasına şiddetle karşı çıkar. Mekanik desteği reddedebilir, iş birliğini engelleyebilir ve başkalarını Kahramana karşı etkileyebilir.
- **Dönüşümü:** Deliller ortaya çıktıkça soruşturmanın manipüle edildiğini **kademeli olarak** fark eder. Bu değişim **ani olmamalıdır.**
- **Barışma:** Kahramanın masumiyetini kabul eder ve onu desteklemeye başlar. Bu barışma **Tier 6 araç geliştirmelerini** açar.
- **Sonu:** Yasa dışı yarış dünyasını kalıcı olarak bırakır ve sıradan bir otomotiv işinde çalışır.

### 3.4 Polis Memuru / Ölen Dostun Eşi (EŞ)

- **Olaydan sonra:** Eşini kaybeder. Başta Kahramanın masum olup olmadığından emin değildir. Adalet arayışıyla polis memuru olur ve eşinin ölümündeki tutarsızlıkları sorgulamayı sürdürür.
- **İki ayrı motivasyonu vardır:**
  - **Resmî görevi:** Polis işi olarak BLACK NUMBERS'ı çökertmek.
  - **Kişisel hedefi:** Eşinin ölümünün ardındaki gerçeği bulmak. Bu dava ona **resmî olarak atanmamıştır.** Kendi inisiyatifiyle yürütür.
- **Yaptıkları:** Kahramanla iletişime geçer, soruşturmanın manipüle edilmiş olabileceğini söyler ve **polis olduğunu gizler.** Garajı, sahte kimliği ve dönüş imkânını sağlar.
- **Karmaşık güdüsü:** Gerçeği ister, ama Kahramanın sorumlu olup olmadığını da öğrenmek ister. Başta ona güvenmez. Güveni deliller ortaya çıktıkça büyür ve sonunda gerçek olur.
- **Kimliğinin ifşası:** Polis kimliği hikâye içinde açığa çıkar ve çatışma yaratır, çünkü hayatının ve soruşturmasının önemli bir kısmını gizlemiştir.
- **Son kovalamaca:** Kahramanın kaçmasına yardım eder. Onun eşinin ölümünden masum olduğuna inanır, ama başka suçlar işlediğini de kabul eder. Bu karar ahlaki ve mesleki bir çatışma yaratır.
- **Sonu:** Eşinin ölümünün gerçeğini öğrenir ve rahatlar. Polis olarak görevine devam eder. Kaçışa yardımının hukuki ve kariyer sonuçları tutarlı biçimde ele alınmalıdır (bkz. §18).

### 3.5 Lider (LDR)

- **Motivasyonu:** Güç ve kâr. Yeraltı yarışının tam kontrolünü ister.
- **Komploda yaptıkları:** Biri öldürülür, diğeri suçlanır. Böylece tek hamleyle iki engel ortadan kalkar.
- **Ara yıllarda:** BLACK NUMBERS'ı kurar, genişletir ve kontrolünü pekiştirir.
- **Oyunda:** Kahramanın dönüşü kurduğu her şeyi kademeli olarak yıkar.
- **Sonu:** Kahraman örgütün zirvesine ulaştıktan sonra yüzleşme olur. Lider yenilir ve polise teslim edilir.
- **Sorumluluğun kanıtlanması:** Sorumluluğu hikâye boyunca toplanan **delillerle** kurulur. Yalnızca son bir itirafa **dayanılmaz.**

### 3.6 Demir — Polis Amiri (DMR)

Ayrıntılı yay için bkz. [§7](#7-polis-amiri-demir--karakter-yayı).

- **Yozlaşmış DEĞİLDİR.** BLACK NUMBERS için gizlice **çalışmaz.**
- Son derece kararlı bir kolluk görevlisidir. Mesleki bağlılığı zamanla yıkıcı bir saplantıya dönüşür.

---

## 4. Karakter Motivasyon Tablosu

| Karakter | Başlangıçtaki motivasyon | Temel çatışma | Dönüşüm | Final durumu |
|---|---|---|---|---|
| **Kahraman** | Gerçeği bulmak, adını temizlemek | Masumiyet ↔ hayatta kalan suçluluğu; tanıdığı ama açıklayamadığı deliller | Gerçeği bulur, ama kendi seçimlerinin bedelini taşır | Asıl ölümden aklanır. Sessiz bir firaridir. Yarışa dönmez. |
| **Mekanikçi** | Kahramanı suçlu sanır, kin besler | Kin ↔ ortaya çıkan deliller | Kini **kademeli olarak** aşar ve barışır | Yasa dışı dünyayı kalıcı olarak bırakır, meşru bir otomotiv işinde çalışır |
| **Eş / Polis** | Resmî: BN'yi çökertmek. Kişisel: eşinin ölümünün gerçeği | Polis görevi ↔ gizli kişisel soruşturma ↔ Kahramana yardım | Şüpheden gerçek güvene geçer | Gerçeği öğrenir ve rahatlar. Görevine devam eder. |
| **Demir** | Şehri korumak | Görev ↔ gurur, hayal kırıklığı, saplantı | Saplantıyı **aşamaz** ve ahlaken çöker | Görevden alınır, soruşturulur, tutuklanır |
| **Lider** | Güç, kâr, tekel | Kurduğu düzen ↔ Kahramanın dönüşü | Dönüşmez | Yenilir ve polise teslim edilir. Örgüt çöker. |

---

## 5. BLACK NUMBERS Örgüt Yapısı

### 5.1 Kanon

- BLACK NUMBERS **16 sıralı büyük yarışçıdan** oluşur.
- Liste yalnızca giderek güçlenen sürücülerden ibaret değildir. **Her üye suç örgütünde belirli bir işlev üstlenir.**
- **Rütbe yükseldikçe** üyenin örgütün geçmişi ve liderliği hakkındaki bilgisi artar.
- Kahraman yarış kazanarak ve ilerleme gereksinimlerini karşılayarak yükselir.
- Her büyük zafer, orijinal komployla ilgili **bilgi, erişim veya delil** sağlar.
- **Yarışların anlatısal sonuçları olmalıdır.**
- Oyuncu, BLACK NUMBERS'ın orijinal arkadaş grubunun yıkımı üzerine kurulduğunu kademeli olarak keşfeder.
- 16 yarışçının **kişiliği, sürüş tarzı, aracı ve uzmanlığı** birbirinden farklı olmalıdır.
- **Yarış disiplinleri:** Sprint, Pist (Circuit), Drift.

### 5.2 Olası Örgütsel İşlevler *(atanmamıştır)*

Bu liste fikir havuzudur. Hiçbir işlev belirli bir bossa **atanmamıştır.**

| İşlev | Açıklama |
|---|---|
| Yarış organizasyonu | Yarışların kurulması, takvim ve rotalar |
| Finans | Para akışı ve aklama |
| Araç hazırlığı | Örgüt araçlarının bakımı ve modifikasyonu |
| Eleman toplama | Yeni yarışçıların örgüte çekilmesi |
| Bilgi yönetimi | İstihbarat, sırlar, kayıtlar |
| Lojistik | Araç, parça ve mekân temini |
| Yasa dışı bahis | Bahis sistemi |
| Dış ilişkiler | Diğer gruplarla ilişkiler |
| Teknik operasyonlar | Teknik altyapı |
| İç disiplin | Örgüt içi yaptırım ve infaz |

### 5.3 Boss Şablonu *(tasarlanacak)*

Bossların kimlikleri, rolleri ve verdikleri deliller henüz tasarlanmamıştır. Tasarım yapılırken her boss için aşağıdaki alanlar doldurulmalıdır:

| Sıra | İsim | Örgütsel işlev | Kişilik | Sürüş tarzı | Araç | Disiplin | Verdiği bilgi/delil |
|---|---|---|---|---|---|---|---|
| #16 → #1 | *Açık* | *Açık* | *Açık* | *Açık* | *Açık* | Sprint / Pist / Drift | *Açık* |

---

## 6. Ana Hikâye Perdeleri

> Perdeler kanondaki olay sırasını düzenlemek için kullanılan bir çalışma iskeletidir. Kesin bölüm sınırları ve olayların hangi boss ile tetikleneceği **açıktır.**

### Önsöz — Orijinal Olay *(geçmiş)*
- Kahraman ve Ölen Dost bir sokak yarışına katılır. Ölen Dostun aracı gizlice sabote edilmiştir.
- Araç yarış sırasında patlar ve sürücü ölür.
- Manipüle edilmiş soruşturma Kahramanı işaret eder. Tutuklama kararı çıkar ve Kahraman kaçar.
- Soruşturma alışılmadık hızda kapatılır.

### Perde 1 — Dönüş
- Eşin mesajı Kahramanı geri getirir. Eş polis olduğunu gizler ve Kahramanın masumiyetine henüz ikna olmamıştır.
- Kahraman sahte kimlikle şehre döner. Garaj hazırdır.
- Mekanikçi dönüşe ve Eşin yardımına karşı çıkar.

### Perde 2 — Yükseliş
- Kahraman BLACK NUMBERS'ın alt sıralarından yükselmeye başlar.
- Zaferler bilgi, erişim ve delil getirir. **İlk somut delil** keşfedilir.
- Mekanikçi Kahramanı engellemeye devam eder, ama içinde şüphe başlar.
- Isı (Heat) seviyeleri 1–5 arasında standart polis baskısı vardır.

### Perde 3 — İfşa *(orta-geç ilerleme)*
- **Kahramanın sahte kimliği ortaya çıkar.** Bu, büyük ve oynanabilir bir hikâye olayıdır.
- Demir, uzun süredir aranan bir firarinin başka bir kimlikle yarıştığını fark eder ve olay onun için kişiselleşir. **Heat 6 açılır.**
- Mekanikçinin sadakati giderek önem kazanır.
- Eşin polis görevi ile özel soruşturması arasındaki gerilim artar.

### Perde 4 — Zirve
- Eşin polis kimliği açığa çıkar ve güven çatışması yaşanır. *(Kesin bölüm açıktır.)*
- Mekanikçi tamamen barışır ve **Tier 6 açılır.** *(Kesin bölüm açıktır.)*
- Heat 7–10 arasında Demir giderek daha agresif ve saplantılı hâle gelir.
- Liderin komplosu delillerle ortaya çıkar.
- **Son yarış** yapılır. Lider yenilir ve polise teslim edilir. BLACK NUMBERS çöker ve üyeleri tutuklanır.

### Perde 5 — Son Kovalamaca
- Kahraman, başka yasa dışı eylemleri yüzünden hâlâ arananlar listesindedir.
- Demir son büyük kovalamacayı başlatır ve kovalamaca **Özel Kademeye** ulaşır.
- Demir kontrolünü kaybeder: **"ONU BANA GETİRİN! ÖLÜ YA DA DİRİ!"**
- Eş kaçışa yardım eder. Mekanikçi de yardım edebilir.
- Kahraman kaçmayı başarır, ama şehirde ağır bir tahribat kalır.

### Epilog
Bkz. [§12](#12-son-ve-epilog).

---

## 7. Polis Amiri Demir — Karakter Yayı

**Temel ilkeler:**
- Demir **yozlaşmış değildir** ve BLACK NUMBERS adına **çalışmaz.**
- Yayı, aşırı gurur, hayal kırıklığı ve saplantının bir insanın muhakemesini nasıl yok edebileceğini göstermelidir.
- Hikâye **polis teşkilatının tamamını yozlaşmış veya kötü niyetli göstermemelidir.**

| Aşama | Demir'in durumu |
|---|---|
| Başlangıç | Şehri koruduğuna gerçekten inanır. Kahramanı tehlikeli bir firari olarak görür. |
| Kimlik ifşası (Heat 6) | Soruşturma kişiselleşir ve Demir kovalamacaya bizzat dahil olur. Emri: **CANLI yakalayın.** |
| Heat 7–10 | Oyuncu her kaçtığında daha agresif ve mantıksız hâle gelir. Hayal kırıklığı ve saplantısı artar. **"Canlı yakalama" emri hâlâ geçerlidir.** |
| Özel Kademe | Tekrarlanan başarısızlıklardan sonra tamamen kontrolünü kaybeder ve önceki itidalini bırakır: **"ONU BANA GETİRİN! ÖLÜ YA DA DİRİ!"** Bu an ahlaki çöküşünün doruk noktasıdır. |
| Sonuç | Görevden alınır, son operasyondaki hukuka aykırı eylemleri nedeniyle soruşturulur ve tutuklanır. |

**Tematik karşıtlık:** Mekanikçi kinini aşmayı öğrenir. Demir saplantısını **aşamaz.**

---

## 8. Heat Seviyelerinin Anlatıdaki Yeri

Oyunda **10 normal Heat seviyesi** ve **1 Özel Kademe** bulunur.

| Seviye | Açılma koşulu | Anlatıdaki anlamı | Demir'in emri |
|---|---|---|---|
| **Heat 1–5** | Standart ilerleme | Kademeli artan standart polis kovalamacası | — |
| **Heat 6** | Kahramanın gerçek kimliğinin ifşası | Demir kişisel olarak dahil olur ve kovalamaca belirgin biçimde yoğunlaşır | **CANLI** |
| **Heat 7–10** | Sonraki bölümler | Giderek sofistikeleşen ve agresifleşen taktikler. Demir'in saplantısı artar. | **CANLI** (hâlâ geçerli) |
| **Özel Kademe** | **Yalnızca** son hikâye kovalamacası | Devasa bir final polis operasyonu. Heat 10'dan **belirgin biçimde** daha yoğun olmalıdır. | **ÖLÜ YA DA DİRİ** |

**Özel Kademe için anlatı kuralları:**
- Oyuncu polisin tamamını yenerek değil, **sürüş becerisi ve taktiksel rota seçimiyle** kaçmalıdır.
- Demir'in davranışının sonuçları olmalıdır (görevden alma, soruşturma, tutuklama).

---

## 9. Araç Geliştirmenin Anlatıdaki Yeri

| Kademe | Kaynak | Anlatıdaki anlamı |
|---|---|---|
| **Tier 1–5** | Sıradan mekanikçiler | Standart ilerleme |
| **Tier 6** | **Yalnızca Mekanikçi**, barışmadan sonra | Arkadaşlığın yeniden kurulmasını temsil eden bir ödül |

**Tier 6 kuralları:**
- Pahalı ve özel performans geliştirmeleri içerir.
- Anlamlı bir iyileşme sağlamalı, ama **dengeli araç fiziğini korumalıdır.**
- Yalnızca Kahraman Mekanikçinin güvenini yeniden kazandıktan sonra açılır.

**Epilogdaki karşılığı:** Tier 6, epilogda Mekanikçinin yasa dışı dünyayı reddettiğini gösteren sembol olarak geri döner (bkz. §12).

---

## 10. Delil ve İfşa İlerleyişi

### 10.1 Orijinal Delil Seti (Komplo)

Deliller ikna edici bir suçlama oluşturmak için **kasıtlı olarak karıştırılmıştır:**

| Delil türü | Kahramanın tepkisi |
|---|---|
| Kahramandan çalınmış ya da alınmış **gerçek eşyalar** | Tanır, ama olay yerine nasıl geldiğini açıklayamaz |
| Suçlayıcı yerlere stratejik olarak bırakılmış **gerçek eşyalar** | Tanır, ama açıklayamaz |
| Kahramana yanlış atfedilmiş **sahte eşyalar** | Hiç görmemiştir |
| Aracı Kahramanın sabote ettiğini ima eden **manipüle edilmiş deliller** | Masum olduğunu bilir |
| Sahip olduğunu **hatırlamadığı** eşyalar | Emin değildir |

- Soruşturma alışılmadık hızda kapatılmış ve eldeki deliller yeterli sayılmıştır. Bu, Eşin sonradan fark ettiği tutarsızlıklardan biridir.

### 10.2 İfşa Kilometre Taşları

| # | Kilometre taşı | Sonucu |
|---|---|---|
| R1 | Eş soruşturmadaki tutarsızlıkları keşfeder | Kahraman geri çağrılır |
| R2 | BN zaferleri bilgi ve erişim sağlar | Örgütün iç yapısı görünür hâle gelir |
| R3 | **İlk somut delil** bulunur | Mekanikçinin şüphesi başlar |
| R4 | Kahramanın sahte kimliği ifşa olur | Heat 6 açılır, Demir dahil olur |
| R5 | Eşin polis kimliği ifşa olur | Güven çatışması yaşanır |
| R6 | Mekanikçi tamamen barışır | Tier 6 açılır |
| R7 | BN'nin arkadaş grubunun yıkımı üzerine kurulduğu anlaşılır | Komplonun tam resmi ortaya çıkar |
| R8 | Liderin sorumluluğu **delillerle** kanıtlanır | Son yüzleşme yapılır |

> Hangi bossun hangi delili verdiği ve R3–R6'nın kesin sırası **açıktır** (bkz. §18).

---

## 11. Son Yüzleşme ve Son Kovalamaca

### 11.1 Liderle Yüzleşme
- Kahraman örgütün zirvesine ulaştıktan sonra gerçekleşir.
- Liderin sorumluluğu hikâye boyunca toplanan delillerle kurulur. **Yalnızca itirafa dayanılmaz.**
- Lider yenilir ve **polise teslim edilir.**
- BLACK NUMBERS çöker ve üyeleri tutuklanır.
- Kahramanın orijinal ölümle ilgili masumiyeti kanıtlanır.

### 11.2 Son Polis Kovalamacası
- Liderin yenilmesi Kahramanın hukuki sorunlarını **bitirmez.** Kahraman yasa dışı yarışlara katılmış, polisten kaçmış ve ciddi tahribata yol açmıştır. Hâlâ aranmaktadır.
- Demir son büyük kovalamacayı başlatır ve kovalamaca Özel Kademeye ulaşır.
- Demir: **"ONU BANA GETİRİN! ÖLÜ YA DA DİRİ!"**
- **Eş** sonunda Kahramanın kaçmasına yardım eder. **Mekanikçi** de yardım edebilir.
- Kahraman yakalanmadan kurtulur.
- **Tahribat göz ardı edilmemelidir.** Hasar gören sokaklar, araçlar, kamu altyapısı ve sivil mülkler gösterilmelidir. Bu tahribat sonuçsuz bir eğlence gibi **sunulmamalıdır.**

---

## 12. Son ve Epilog

Son **acı-tatlıdır.**

| Unsur | Durum |
|---|---|
| **BLACK NUMBERS** | Çökertilmiş, üyeleri tutuklanmıştır. Şehrin yarış sahnesi üzerindeki kontrolü sona ermiştir. |
| **Şehir** | Son kovalamacanın hasarı görünürdür. Haber yayınları ve sinematikler sonuçları gösterir. Şehir zamanla normale döner. |
| **Demir** | Görevden alınmış ve tutuklanmıştır. Saplantısı kariyerini yok etmiştir. |
| **Polis Memuru (Eş)** | Eşinin ölümünün gerçeğini öğrenmiş ve rahatlamıştır. Polis olarak görevine devam eder. |
| **Mekanikçi** | Yasa dışı yarış dünyasını kalıcı olarak bırakmıştır. Sıradan bir otomotiv işinde çalışır. |
| **Kahraman** | Son kovalamacadan kaçmıştır. Arkadaşının mezarını ziyaret eder, sonra ortadan kaybolur ve sessizce firari olarak yaşar. |

### 12.1 Mekanikçinin Dükkânı — Kritik Sahne

- Epilogda eski tanıdıkların işlettiği bir **oto tamirhanesi** ve bir **araç galerisi** yer alır.
- Biri mekanikçi olarak çalışmaya devam eder, diğeri araç satar. *(Galeri ortağının kimliği açıktır.)*
- Genç bir müşteri **Tier 6 performans modifikasyonları** hakkında soru sorar.

**Diyalog (kanon):**

> **Mekanikçi:** "O günler eskide kaldı, adamım."
>
> *(Müşteri ısrar eder ve daha fazla para teklif eder.)*
>
> **Mekanikçi:** "Dükkânımı terk et. Yoksa polisi ararım."

> ⚠️ **Bu sahne son derece önemlidir.** Bir zamanlar tehlikeli ve şiddetli bir yeraltı ortamının parçası olan kişi, artık o yaşam tarzını tamamen reddetmektedir.
> Bu an, yasa dışı yarışı nostaljik biçimde kutlayan bir sahneyle **değiştirilmemelidir.**

### 12.2 Kahramanın Kapanışı
- Arkadaşının mezarını ziyaret eder.
- Ortadan kaybolur ve sessizce yaşar.
- Asıl ölümden masum olduğu kanıtlanmıştır, ama sonraki tahribat ve yasa dışı eylemlerinden aklanmamıştır.
- Son hâli **huzurlu, ama tamamen özgür değildir.**
- Sokak yarışına **dönmez.**

---

## 13. Ana Temalar

- Suçluluk ve masumiyet
- Manipüle edilmiş deliller
- İhanet ve kırılan güven
- Dostluk ve barışma
- Adalet ile intikam
- Hırs ve nüfuzun kötüye kullanılması
- Mesleki bağlılığın saplantıya dönüşmesi
- Yasa dışı yarışın insani bedeli
- Hukuki masumiyet ile ahlaki sorumluluk arasındaki fark
- Yıkıcı hırslardan ne zaman vazgeçileceğini öğrenmek

### Merkezî Karşıtlık

| Karakter | Neyle yüzleşir | Sonuç |
|---|---|---|
| Mekanikçi | Kin | **Aşar** |
| Demir | Saplantı | **Aşamaz** |
| Kahraman | Gerçek ve sorumluluk | Gerçeği bulur, ama **kendi seçimlerinin bedelini taşır** |

Son basit bir zafer kutlaması değil, **duygusal olarak olgun** bir son olmalıdır.

---

## 14. Sinematik Gereksinimler

Büyük duygusal olaylar sinematiklerle anlatılmalıdır. Kesin diyaloglar, sahne sırası ve sunum **geliştirmeye açıktır.**

| # | Sahne | Perde |
|---|---|---|
| C01 | Orijinal yarış ve patlama | Önsöz |
| C02 | Manipüle edilmiş deliller ve Kahramanın kaçışı | Önsöz |
| C03 | Kahramanı geri getiren mesaj | Perde 1 |
| C04 | Mekanikçiyle yüzleşmeler | Perde 1–2 |
| C05 | İlk somut delilin keşfi | Perde 2 |
| C06 | Kahramanın kimliğinin ifşası | Perde 3 |
| C07 | Eşin polis kimliğinin açığa çıkması | Perde 3–4 |
| C08 | Mekanikçiyle barışma | Perde 3–4 |
| C09 | Liderin komplosunun keşfi | Perde 4 |
| C10 | Son yarış | Perde 4 |
| C11 | Liderin tutuklanması | Perde 4 |
| C12 | Demir'in psikolojik çöküşü | Perde 5 |
| C13 | Özel Kademe kovalamacası | Perde 5 |
| C14 | Hasarlı şehrin haber görüntüleri | Epilog |
| C15 | Demir'in görevden alınması ve tutuklanması | Epilog |
| C16 | Mekanikçinin Tier 6 işini son kez reddetmesi | Epilog |
| C17 | Eşin sıradan polis işine dönüşü | Epilog |
| C18 | Kahramanın arkadaşının mezarını ziyareti | Epilog |
| C19 | Kahramanın sessiz bir hayata kaybolması | Epilog |

> C11 (Liderin tutuklanması) ile C12 (Demir'in çöküşü) arasındaki sıra, olay akışına göre düzenlenmiştir. Kaynak metindeki listede Demir'in çöküşü son yarıştan hemen sonra, liderin tutuklanması ise Özel Kademeden sonra geçer. Kesin sıra açıktır (bkz. §18, S-15).

**Anlatım ilkeleri:**
- Aşırı açıklayıcı anlatımdan (exposition) kaçının.
- Çevresel hikâye anlatımı, telsiz konuşmaları, oynanış olayları, karakter etkileşimleri ve sinematik sekanslar kullanın.

---

## 15. Oynanışla Bağlantı

Hikâye ilerlemesi oyunda **mevcut** sistemleri etkilemelidir:

| Hikâye olayı | Etkilenen oynanış sistemi |
|---|---|
| Yarış zaferleri | Yeni BLACK NUMBERS rakipleri açılır |
| Boss ilerlemesi | Delil ve anlatısal ifşalar gelir |
| Mekanikçiyle barışma | **Tier 6** geliştirmeleri açılır |
| Kimlik ifşası | **Heat 6** açılır |
| Sonraki bölümler | Heat ilerlemesi 10'a kadar genişler |
| Son kovalamaca | **Özel Kademe** etkinleşir |
| Son | Kahramanın eylemlerinin gerçek dünyadaki sonuçları yansıtılır |

**Sınır:** Hikâyeyle ilgisiz mekanik eklenmez. Temel oynanış şunlardır: sokak yarışı, araç gelişimi, drift, polis kovalamacası ve hikâye ilerlemesi.

---

## 16. Kapsam Dışı

Bu doküman aşağıdakileri **yapmaz ve gerektirmez:**
- Oynanış mekaniklerinin uygulanması
- Sinematiklerin üretimi
- Final diyalog metinleri (§12.1'deki kanon replikler dışında)

---

## 17. Onaylanmış Kanon

Aşağıdaki maddeler **bağlayıcıdır** ve değiştirilmeden korunmalıdır:

1. Kahraman orijinal sabotajı **yapmamıştır.**
2. Ölen Dostun aracı sabote edilmiş ve yarış sırasında patlamıştır. Sürücü ölmüştür.
3. Komployu BLACK NUMBERS'ın lideri kurmuştur. Amacı iki yarışçı engelini tek hamlede ortadan kaldırmaktır.
4. Deliller gerçek, çalınmış, yerleştirilmiş, sahte ve manipüle edilmiş öğelerin karışımıdır. Kahraman bazılarını gerçekten tanır.
5. Kahraman hakkında tutuklama kararı çıkmış, Kahraman tutuklanmadan kaçmıştır. Soruşturma alışılmadık hızda kapatılmıştır.
6. Kahraman hayatta kalan suçluluğu taşır.
7. Mekanikçi başta Kahramanı suçlu sanır, oyunun başında onu engeller, **kademeli olarak** fikrini değiştirir ve barışır.
8. Barışma **Tier 6**'yı açar. Tier 1–5 sıradan mekanikçilerdedir. Tier 6 dengeli fiziği korur.
9. Eş polis olmuştur. Resmî görevi BN'yi çökertmektir. Eşinin davasını resmî atama olmadan, kendi inisiyatifiyle inceler.
10. Eş polis olduğunu gizleyerek Kahramanla iletişime geçer, garajı ve sahte kimliği sağlar. Başta Kahramanın masumiyetine ikna olmamıştır.
11. Eşin polis kimliği hikâye içinde açığa çıkar ve çatışma yaratır. Güveni sonunda gerçek olur.
12. BLACK NUMBERS 16 sıralı büyük yarışçıdan oluşur. Her üyenin örgütsel bir işlevi vardır. Rütbe yükseldikçe bilgi artar. Disiplinler Sprint, Pist ve Drift'tir.
13. Kimlik ifşası orta-geç ilerlemede olur, büyük ve oynanabilir bir olaydır ve Heat 6'yı açar.
14. Demir **yozlaşmış değildir.** Saplantısı onu çöküşe götürür.
15. Heat 6–10 boyunca Demir'in emri **canlı yakalamaktır.**
16. Özel Kademe yalnızca son hikâye kovalamacasında açılır. Replik: **"ONU BANA GETİRİN! ÖLÜ YA DA DİRİ!"**
17. Özel Kademeden polisin tamamını yenerek değil, beceri ve rota seçimiyle kaçılır.
18. Polis teşkilatının tamamı yozlaşmış gösterilmez.
19. Liderin sorumluluğu delillerle kanıtlanır. Lider yenilir ve polise teslim edilir. BN çöker.
20. Liderin yenilmesinden sonra Kahraman hâlâ aranır. Son kovalamacada Eş kaçışa yardım eder.
21. Son kovalamacanın tahribatı gösterilir ve sonuçsuz eğlence olarak sunulmaz.
22. Demir görevden alınır, soruşturulur ve tutuklanır.
23. Eş gerçeği öğrenir ve polis olarak görevine devam eder.
24. Mekanikçi yasa dışı dünyayı kalıcı olarak bırakır. Epilogdaki Tier 6 reddi sahnesi ve iki replik kanondur.
25. Kahraman mezarı ziyaret eder, ortadan kaybolur ve sessizce firari olarak yaşar. Yarışa dönmez. Son acı-tatlıdır.

---

## 18. Çözülmemiş Anlatı Detayları

> Bu sorulara **kendi başınıza kesin cevap vermeyin.** Karar verildiğinde ilgili maddeyi §17'ye taşıyın.

### 18.1 Kaynak Metinde Açık Bırakılan Konular

| ID | Soru |
|---|---|
| Q-01 | Karakterlerin final isimleri ve ayrıntılı geçmişleri |
| Q-02 | Şehrin adı ve coğrafi kimliği |
| Q-03 | Orijinal kaza ile dönüş arasındaki kesin süre |
| Q-04 | 16 BLACK NUMBERS üyesinin kimlikleri ve rolleri |
| Q-05 | Hangi bossun hangi delili verdiği |
| Q-06 | Sahte delillerin delil zinciri (chain of custody) ve adli açıklaması |
| Q-07 | Liderin kurbanın aracına başlangıçta nasıl eriştiği |
| Q-08 | Kahramanın kimliğinin ifşa olduğu kesin bölüm ve tetikleyen boss |
| Q-09 | Mekanikçinin tamamen barıştığı kesin bölüm |
| Q-10 | Eşin polis kimliğinin açığa çıktığı bölüm |
| Q-11 | Final yarış rotası ve kaçış sekansı |
| Q-12 | Epilogdaki tamirhane ve galeriyi işleten kişilerin kimlikleri |
| Q-13 | Eşin kaçışa yardımının hukuki sonuçları |
| Q-14 | Kahramanın son konumu |

### 18.2 Tespit Edilen Küçük Süreklilik Soruları

Bu maddeler olay örgüsünü değiştirmez. Netleştirilmesi gereken noktaları kaydeder:

| ID | Soru | Bağlam |
|---|---|---|
| S-01 | Kahramanın hayatta kalan suçluluğunun **somut kaynağı** nedir? (ör. yarışı o mu önerdi, bir şeyi fark edemedi mi?) | Kaynak metin bu suçluluğu belirtir ama nedenini tanımlamaz. |
| S-02 | Mekanikçi olaydan sonra "şiddetli yeraltı ortamını bırakmışsa" Tier 6 gibi yarış odaklı geliştirmeleri hangi bağlamda yapabilir? Epilogda "kalıcı olarak bırakması" önceki ayrılıştan nasıl farklıdır? | İki ayrılış arasındaki fark tanımlanmalıdır (ör. arada kalan gri bölge). |
| S-03 | Lider BLACK NUMBERS'ın 16 üyesinden biri midir (#1), yoksa 16'nın üstünde ayrı bir figür müdür? | "Zirveye ulaştıktan sonra yüzleşme" ifadesi iki şekilde de okunabilir. |
| S-04 | Firari olan Kahraman, Lideri polise **nasıl ve kimin aracılığıyla** teslim eder? | Eşin rolü burada belirleyici olabilir. |
| S-05 | Demir'in görevden alınıp tutuklanmasını sağlayan delil veya tanıklık kimden gelir? | Sonuç kanondur, ama mekanizması tanımlanmamıştır. |
| S-06 | Eş Demir'in komutası altında mı çalışır, yoksa ayrı bir birimde mi? | Heat 6 sonrasındaki gerilimi doğrudan etkiler. |
| S-07 | Bir polis memurunun gizlice sahte kimlik ve garaj sağlamasının, kimlik ifşasında ya da sonrasında açığa çıkıp çıkmadığı | Q-13 ile bağlantılıdır. Eşin görevine devam etmesiyle tutarlı olmalıdır. |
| S-08 | Kahramanın sahte kimliği ifşa olmadan önce (Heat 1–5) polis onu sıradan bir sokak yarışçısı olarak mı kovalar? | Heat 1–5'in anlatıdaki çerçevesi. |
| S-09 | Mekanikçinin barışması kimlik ifşasından önce mi, sonra mı tamamlanır? | İfşa sırasında "sadakatinin önem kazanması" kısmi bir yakınlaşmanın daha önce başladığını düşündürür. |
| S-10 | "Son yarış" ile "Liderle yüzleşme" aynı olay mıdır? | Sinematik listesinde "son yarış" ayrı bir madde olarak geçer. |
| S-11 | Epilogdaki "eski tanıdıklar" kimlerdir? Orijinal grupta yalnızca dört kişi vardı. | Q-12 ile bağlantılıdır. |
| S-12 | Lider teslim edildikten sonra Demir neden hâlâ Kahramanı hedef alır? Liderin suçu kanıtlanınca Demir'in Kahraman hakkındaki görüşü değişir mi? | Demir'in saplantısının motivasyonunu netleştirir. |
| S-13 | Eşin son kovalamacada **nasıl** yardım ettiği (bilgi, rota, sahadaki müdahale) | Q-13'ün ağırlığını belirler. |
| S-14 | Oyuncunun tahribatı (ör. sivil hasar) yalnızca sinematikte mi görmesi gerekir, yoksa oynanışta da mı yansıtılır? | "Sonuçları yansıtma" kuralının kapsamı. |
| S-15 | Sinematik sırası: Kaynak listede Demir'in çöküşü liderin tutuklanmasından önce geçer. Demir'in çöküşü son yarış sırasında mı başlar, yoksa yalnızca Perde 5'te mi? | §14'teki tablo olay akışına göre düzenlenmiştir. Kesin sıra açıktır. |

---

## Değişiklik Günlüğü

| Tarih | Değişiklik |
|---|---|
| 2026-11-02 | İlk sürüm. Kaynak hikâye özeti dokümana aktarıldı, kanon ile açık konular ayrıldı. |
