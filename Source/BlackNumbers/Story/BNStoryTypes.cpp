#include "Story/BNStoryTypes.h"

namespace
{
	FBNComputedVehicleStats MakeBossVehicleStats(int32 Rank, EBNCarId BaseCar)
	{
		// Tier progresses smoothly from 0 (#16) to 5 (#02), then balanced Tier 6 for #01 Nocturne BN-01.
		const int32 Tier = Rank >= 13 ? 0
			: Rank >= 9 ? 2
			: Rank >= 5 ? 3
			: Rank >= 2 ? 5
			: 6;
		const int32 Tiers[FBNUpgradeSpec::NumSlots] = { Tier, Tier, Tier, Tier, Tier };
		FBNComputedVehicleStats Stats = FBNComputedVehicleStats::Compute(BaseCar, Tiers);
		if (Rank == 1)
		{
			Stats.MaxTorqueNm = 620.f;
			Stats.MaxRpm = 8300.f;
			Stats.DragCoefficient = 0.255f;
			Stats.TireGripScale = 1.24f;
			Stats.NosMaxSpeedKmh = 275.f;
		}
		return Stats;
	}

	FBNBossSpec MakeBoss(
		int32 Rank, int32 TierLayer, const TCHAR* Name, const TCHAR* OrgRole,
		const TCHAR* Personality, const TCHAR* DrivingStyle, const TCHAR* VehicleName,
		EBNRaceDiscipline Discipline, const TCHAR* DisciplineLabel, const TCHAR* RaceAssetName,
		const TCHAR* EvidenceCode, const TCHAR* EvidenceSummary,
		int32 DriftTargetScore, int32 RewardCash, int32 RewardRep, float RivalSkill,
		EBNCarId BaseCar, const FLinearColor& Tint)
	{
		FBNBossSpec S;
		S.Rank = Rank;
		S.TierLayer = TierLayer;
		S.Name = Name;
		S.OrgRole = OrgRole;
		S.Personality = Personality;
		S.DrivingStyle = DrivingStyle;
		S.VehicleName = VehicleName;
		S.Discipline = Discipline;
		S.DisciplineLabel = DisciplineLabel;
		S.RaceAssetName = RaceAssetName;
		S.BaseCar = BaseCar;
		S.EvidenceCode = EvidenceCode;
		S.EvidenceSummary = EvidenceSummary;
		S.DriftTargetScore = DriftTargetScore;
		S.RewardCash = RewardCash;
		S.RewardRep = RewardRep;
		S.RivalSkill = RivalSkill;
		S.VehicleStats = MakeBossVehicleStats(Rank, BaseCar);
		S.VehicleTint = Tint;
		return S;
	}

	FBNStoryCard MakeCard(const TCHAR* Id, const TCHAR* Title, const TCHAR* Speaker, const FString& Body)
	{
		FBNStoryCard C;
		C.CardId = Id;
		C.Title = Title;
		C.Speaker = Speaker;
		C.Body = Body;
		return C;
	}
}

const TArray<FBNBossSpec>& FBNStoryState::GetAllBosses()
{
	static const TArray<FBNBossSpec> Roster = {
		// Layer 1: Sokak Ağı (#16-#13)
		MakeBoss(16, 1, TEXT("Berkay \"Kıvılcım\" Tuna"), TEXT("Eleman Toplama"),
			TEXT("Gösterişçi çaylak"), TEXT("Agresif geç fren"), TEXT("Sora CX-200"),
			EBNRaceDiscipline::Sprint, TEXT("Sprint · Kuzey Viyadüğü"), TEXT("RACE_KuzeyViyadukSprint"), TEXT("R2"),
			TEXT("R2: BN sokak buluşma frekansları ve alt kademe yarış haritası ele geçti."),
			0, 1500, 150, 0.72f, EBNCarId::KuroganeMSpec, FLinearColor(0.75f, 0.38f, 0.08f, 1.f)),
		MakeBoss(15, 1, TEXT("Nadia \"Apex\" Kovacs"), TEXT("Yarış Org. (Alt Rota)"),
			TEXT("Disiplinli, soğuk"), TEXT("Temiz çizgi, sıfır hata"), TEXT("Valken RS-4"),
			EBNRaceDiscipline::Circuit, TEXT("Pist · Merkez Bulvar"), TEXT("RACE_KaramaraCircuit"), TEXT(""),
			TEXT("Örgütün resmi rota, hakem ve gözcü şeması ele geçti."),
			0, 2000, 200, 0.75f, EBNCarId::ValkenRS4, FLinearColor(0.14f, 0.34f, 0.62f, 1.f)),
		MakeBoss(14, 1, TEXT("Cem \"Duman\" Aksoy"), TEXT("Yasa Dışı Bahis (Sanayi)"),
			TEXT("Geveze, sinirli"), TEXT("Geniş açılı sert drift"), TEXT("Kurogane S14-R"),
			EBNRaceDiscipline::Drift, TEXT("Drift · Doğu Sanayi"), TEXT("RACE_SanayiDrift"), TEXT(""),
			TEXT("5 yıl önce Kuzey Geçidi'nde Emre aleyhine açılan yüksek oranlı tek bahis kaydı."),
			1500, 2500, 250, 0.77f, EBNCarId::Vandal440R, FLinearColor(0.55f, 0.16f, 0.18f, 1.f)),
		MakeBoss(13, 1, TEXT("Dario \"Kargo\" Moretti"), TEXT("Lojistik (Liman Girişi)"),
			TEXT("Pragmatik kaçakçı"), TEXT("Ağır şasiyle blokaj"), TEXT("Tauron V6 GT"),
			EBNRaceDiscipline::Sprint, TEXT("Sprint · Eski Liman"), TEXT("RACE_LimanSprint"), TEXT(""),
			TEXT("Eski Liman'dan gizlice sokulan gümrüksüz yarış parçası irsaliyeleri."),
			0, 3000, 300, 0.79f, EBNCarId::TauronV6Rally, FLinearColor(0.28f, 0.30f, 0.24f, 1.f)),

		// Layer 2: Operasyon Ağı (#12-#09)
		MakeBoss(12, 2, TEXT("Leyla \"Kısa Devre\" Özkan"), TEXT("Araç Hazırlığı (Sokak)"),
			TEXT("Ketum teknisyen"), TEXT("Hafif şasi, hızlı viraj"), TEXT("Aether K-7"),
			EBNRaceDiscipline::Circuit, TEXT("Pist · Yamaç Mahallesi"), TEXT("RACE_YamacCircuit"), TEXT("R3"),
			TEXT("R3 (İLK SOMUT DELİL): Emre'nin yakıt hattındaki termal valf özel sipariş kodu. Tarık kuşkulanmaya başlar."),
			0, 3800, 380, 0.82f, EBNCarId::ValkenRS4, FLinearColor(0.10f, 0.45f, 0.44f, 1.f)),
		MakeBoss(11, 2, TEXT("Ozan \"Sıfır\" Yıldız"), TEXT("Yasa Dışı Bahis (Merkez)"),
			TEXT("Hesapçı kumarbaz"), TEXT("Riskli geç sollamalar"), TEXT("Boreas M3-X"),
			EBNRaceDiscipline::Sprint, TEXT("Sprint · Çevre Yolu"), TEXT("RACE_CevreYoluSprint"), TEXT(""),
			TEXT("Kaza gecesi merkez ve geçit kameralarının bahis masası emriyle kapatıldığı nöbet kaydı."),
			0, 4500, 450, 0.84f, EBNCarId::KuroganeMSpec, FLinearColor(0.18f, 0.18f, 0.25f, 1.f)),
		MakeBoss(10, 2, TEXT("Sasha \"Ters Açı\" Volkov"), TEXT("Yarış Org. (Dağ / Drift)"),
			TEXT("Soğukkanlı şovmen"), TEXT("Sürekli bağlı drift zinciri"), TEXT("Kurogane RX-Apex"),
			EBNRaceDiscipline::Drift, TEXT("Drift · Kuzey Geçidi"), TEXT("RACE_KuzeyGecidiDrift"), TEXT(""),
			TEXT("Kaza gecesi Kuzey Geçidi start noktasında görülen kayıt dışı ikinci garaj aracı tanıklığı."),
			2500, 5200, 520, 0.86f, EBNCarId::Vandal440R, FLinearColor(0.62f, 0.48f, 0.12f, 1.f)),
		MakeBoss(9, 2, TEXT("Murat \"Balyoz\" Güneş"), TEXT("İç Disiplin (Saha)"),
			TEXT("Kaba kuvvet, korkutucu"), TEXT("Temasçı, iterek yol açar"), TEXT("Vandal 440R"),
			EBNRaceDiscipline::Circuit, TEXT("Pist · Liman & Köprü"), TEXT("RACE_LimanCircuit"), TEXT(""),
			TEXT("Konuşmaya çalışan eski yarışçıları sindirme ve sahte tanık baskısı kayıtları."),
			0, 6000, 600, 0.88f, EBNCarId::Vandal440R, FLinearColor(0.42f, 0.08f, 0.05f, 1.f)),

		// Layer 3: İstihbarat & Finans (#08-#05)
		MakeBoss(8, 3, TEXT("Hakan \"Lens\" Ertürk"), TEXT("Bilgi Yönetimi & Kamera"),
			TEXT("Paranoyak, kurnaz"), TEXT("Savunmacı, kestirmeci"), TEXT("Nyx Evo-IX"),
			EBNRaceDiscipline::Sprint, TEXT("Sprint · Kuzey Viyadüğü"), TEXT("RACE_KuzeyViyadukSprint"), TEXT("R4"),
			TEXT("R4 (KİMLİK İFŞASI): Yüz ve plaka polise sızdı -> Heat 6 Demir açıldı! Diskte foto manipülasyonu çıktı."),
			0, 7500, 750, 0.90f, EBNCarId::ValkenRS4, FLinearColor(0.22f, 0.25f, 0.30f, 1.f)),
		MakeBoss(7, 3, TEXT("Sofia \"Krom\" Lindqvist"), TEXT("Araç Hazırlığı (Üst)"),
			TEXT("Mükemmeliyetçi"), TEXT("Keskin apex, yüksek tutuş"), TEXT("Solstice GT-S"),
			EBNRaceDiscipline::Circuit, TEXT("Pist · Yamaç Mahallesi"), TEXT("RACE_YamacCircuit"), TEXT(""),
			TEXT("Özel üretim termal tetikli NOS valfinin yalnızca 2 adet üretilip üst kademeye verildiği belge."),
			0, 8800, 880, 0.92f, EBNCarId::ValkenRS4, FLinearColor(0.70f, 0.72f, 0.76f, 1.f)),
		MakeBoss(6, 3, TEXT("Miraç \"Borsa\" Alkan"), TEXT("Finans (Aklama)"),
			TEXT("Takım elbiseli kibirli"), TEXT("Yüksek düzlük hızı"), TEXT("Valen V10 Coupe"),
			EBNRaceDiscipline::Sprint, TEXT("Sprint · Çevre Yolu"), TEXT("RACE_CevreYoluSprint"), TEXT("R5"),
			TEXT("R5 (SELİN İFŞASI): Mali arşivden sızan polis takip dosyasında Selin Karahan'ın rozeti ortaya çıktı!"),
			0, 10000, 1000, 0.94f, EBNCarId::DracoRST, FLinearColor(0.12f, 0.28f, 0.20f, 1.f)),
		MakeBoss(5, 3, TEXT("Kaan \"Kurye\" Vural"), TEXT("Lojistik (Üst Kademe)"),
			TEXT("Soğuk profesyonel"), TEXT("Her zeminde dengeli drift"), TEXT("Draco RS-T"),
			EBNRaceDiscipline::Drift, TEXT("Drift · Doğu Sanayi"), TEXT("RACE_SanayiDrift"), TEXT("R6"),
			TEXT("R6 (TARIK BARIŞMASI): Kerem'in garajından tork anahtarı ve eldiven çalan kurye sevkiyat kaydı -> Tier 6 açıldı!"),
			3800, 12000, 1200, 0.95f, EBNCarId::DracoRST, FLinearColor(0.20f, 0.36f, 0.50f, 1.f)),

		// Layer 4: Komplo Çekirdeği (#04-#01)
		MakeBoss(4, 4, TEXT("Rıza \"Gölge\" Karadağ"), TEXT("İç Disiplin (Komutan)"),
			TEXT("Acımasız infazcı"), TEXT("Yüksek hız blokajı"), TEXT("Cerberus V8 GT"),
			EBNRaceDiscipline::Circuit, TEXT("Pist · Liman & Köprü"), TEXT("RACE_LimanCircuit"), TEXT("R7"),
			TEXT("R7: BLACK NUMBERS'ın kuruluş sermayesi ve ilk toplantısı Emre'nin ölümünden 11 gün sonra yapılmış."),
			0, 14500, 1450, 0.97f, EBNCarId::TauronV6Rally, FLinearColor(0.10f, 0.10f, 0.12f, 1.f)),
		MakeBoss(3, 4, TEXT("İlhan \"Valf\" Korkmaz"), TEXT("Teknik Operasyonlar (Sabotaj)"),
			TEXT("Sessiz, hesaplı mühendis"), TEXT("Cerrahi virajlar & drift"), TEXT("Apex-Zero Proto"),
			EBNRaceDiscipline::Drift, TEXT("Drift · Kuzey Geçidi"), TEXT("RACE_KuzeyGecidiDrift"), TEXT(""),
			TEXT("Sabotaj kanıtı: ikinci yedek termal valf + Kerem'in garaj anahtar kopyası + Viktor'un şifreli iş emri."),
			5000, 17500, 1750, 0.98f, EBNCarId::KuroganeMSpec, FLinearColor(0.68f, 0.52f, 0.16f, 1.f)),
		MakeBoss(2, 4, TEXT("Elena Novak"), TEXT("Dış İlişkiler & Nüfuz"),
			TEXT("Zarif ama buz gibi"), TEXT("Maksimum ortalama hız"), TEXT("Aurelia V12 Corsa"),
			EBNRaceDiscipline::Sprint, TEXT("Sprint · Eski Liman"), TEXT("RACE_LimanSprint"), TEXT("R8"),
			TEXT("R8 (SON KANIT): Savcılık/adli tıp soruşturmasını hızla kapattıran ödeme zinciri + Viktor Serez onay kaydı."),
			0, 21000, 2100, 0.99f, EBNCarId::DracoRST, FLinearColor(0.55f, 0.10f, 0.22f, 1.f)),
		MakeBoss(1, 4, TEXT("Viktor Serez"), TEXT("Kurucu & Lider (#01)"),
			TEXT("Soğukkanlı, narsist vizyoner"), TEXT("Tüm disiplinlerde kusursuz"), TEXT("Nocturne BN-01"),
			EBNRaceDiscipline::Circuit, TEXT("Karma · Karamara Büyük Turu"), TEXT("RACE_KaramaraBuyukTuru"), TEXT("FINAL"),
			TEXT("FINAL: Karamara Büyük Turu 1v1 zaferi -> Eski Liman Köprüsü'nde tutuklanma!"),
			0, 30000, 3000, 1.00f, EBNCarId::NocturneBN01, FLinearColor(0.055f, 0.06f, 0.075f, 1.f)),
	};
	return Roster;
}

const FBNBossSpec* FBNStoryState::FindBossByRank(int32 Rank)
{
	for (const FBNBossSpec& Spec : GetAllBosses())
	{
		if (Spec.Rank == Rank)
		{
			return &Spec;
		}
	}
	return nullptr;
}

const TCHAR* FBNStoryState::ActLabel(EBNStoryAct InAct)
{
	switch (InAct)
	{
	case EBNStoryAct::Perde1_Donus: return TEXT("PERDE 1: DONUS (K. VANCE)");
	case EBNStoryAct::Perde2_Yukselis: return TEXT("PERDE 2: YUKSELIS (#16-#09)");
	case EBNStoryAct::Perde3_Ifsa: return TEXT("PERDE 3: IFSA (#08-#05)");
	case EBNStoryAct::Perde4_Zirve: return TEXT("PERDE 4: ZIRVE (#04-#01)");
	case EBNStoryAct::Perde5_SonKovalamaca: return TEXT("PERDE 5: SON KOVALAMACA (OZEL KADEME)");
	case EBNStoryAct::Epilog: return TEXT("EPILOG: KARAMARA HESABI");
	default: return TEXT("PERDE");
	}
}

const TCHAR* FBNStoryState::TarikAttitudeLabel(EBNTarikAttitude Attitude)
{
	switch (Attitude)
	{
	case EBNTarikAttitude::Hostile: return TEXT("DUSMANCA (Tier 6 kilitli)");
	case EBNTarikAttitude::Suspicious: return TEXT("ILK SUPHE (R3 valf kodu)");
	case EBNTarikAttitude::DeepDoubt: return TEXT("DERIN KUSKU (R4 foto manipulasyonu)");
	case EBNTarikAttitude::Reconciled: return TEXT("BARISIK (Tier 6 ACIK)");
	case EBNTarikAttitude::Retired: return TEXT("EMEKLI (Yasal tamirhane)");
	default: return TEXT("?");
	}
}

const TCHAR* FBNStoryState::EscapeRouteName(int32 RouteIndex)
{
	return RouteIndex == 1 ? TEXT("ROTA B: KUZEY GECIDI SERVIS YOLU") : TEXT("ROTA A: LIMAN TUNELLERI");
}

void FBNStoryState::Reset()
{
	Act = EBNStoryAct::Perde1_Donus;
	TarikAttitude = EBNTarikAttitude::Hostile;
	NextBossRank = 16;
	DefeatedMask = 0;
	bIntroShown = false;
	bIdentityExposed = false;
	bSelinExposed = false;
	bTarikReconciled = false;
	bEvidenceComplete = false;
	bViktorArrested = false;
	bFinalChaseActive = false;
	bCampaignCompleted = false;
	EscapedRouteIndex = -1;
	FinalCityDamageDollars = 0;
	RebuildEvidenceNotes();
}

void FBNStoryState::RebuildEvidenceNotes()
{
	UnlockedEvidenceNotes.Reset();
	UnlockedEvidenceNotes.Add(TEXT("R1: 5 yil once Kuzey Gecidi'ndeki kaza dosyasi olaganustu hizla kapatildi (Selin ihbari)."));

	for (const FBNBossSpec& Spec : GetAllBosses())
	{
		if (IsBossDefeated(Spec.Rank))
		{
			UnlockedEvidenceNotes.Add(FString::Printf(TEXT("[#%02d %s] %s"),
				Spec.Rank, *Spec.Name, *Spec.EvidenceSummary));
		}
	}
	if (bViktorArrested)
	{
		UnlockedEvidenceNotes.Add(TEXT("FINAL: Viktor Serez Eski Liman Koprusu'nde polise teslim edildi. Kerem Emre'nin olumunden aklandi."));
	}
	if (bCampaignCompleted)
	{
		UnlockedEvidenceNotes.Add(FString::Printf(TEXT("EPILOG: %s ile cikis | Sehir Faturasi: $%s | Demir tutuklandi."),
			EscapeRouteName(EscapedRouteIndex), *FString::FormatAsNumber(FinalCityDamageDollars)));
	}
}

bool FBNStoryState::IsBossDefeated(int32 Rank) const
{
	return Rank >= 1 && Rank <= 16 && (DefeatedMask & (1u << Rank)) != 0u;
}

int32 FBNStoryState::GetDefeatedCount() const
{
	int32 Count = 0;
	for (int32 r = 1; r <= 16; ++r)
	{
		Count += IsBossDefeated(r) ? 1 : 0;
	}
	return Count;
}

bool FBNStoryState::CanChallengeBoss(int32 Rank) const
{
	if (bFinalChaseActive || bCampaignCompleted || Rank < 1 || Rank > 16)
	{
		return false;
	}
	if (Rank != NextBossRank || IsBossDefeated(Rank))
	{
		return false;
	}
	if (Rank == 1)
	{
		return bEvidenceComplete && IsBossDefeated(2) && IsBossDefeated(3) && IsBossDefeated(4);
	}
	return true;
}

int32 FBNStoryState::GetMaxAllowedHeat() const
{
	if (bFinalChaseActive)
	{
		return 11; // Ozel Kademe (Perde 5 only)
	}
	if (bIdentityExposed)
	{
		return 10; // Heat 6-10 Baskomiser Demir
	}
	return 5; // Heat 1-5 K. Vance
}

void FBNStoryState::BeginStory(TArray<FBNStoryCard>& OutCards)
{
	if (bIntroShown)
	{
		return;
	}
	bIntroShown = true;
	OutCards.Add(MakeCard(TEXT("C03"), TEXT("C03 - BES YIL SONRA KARAMARA"), TEXT("SELIN KARAHAN"),
		TEXT("5 yil once Emre'nin araci Kuzey Gecidi'nde patladi. Adli dosya hizla kapatildi.\n"
			"Simdi 'K. Vance' sahte kimligiyle Yamac Mahallesi garajindasin. [B] ile 16'lik Listeyi ac.")));
	OutCards.Add(MakeCard(TEXT("C04"), TEXT("C04 - DOGU SANAYI UYARISI"), TEXT("TARIK KAYA"),
		TEXT("\"Geri donmen kimsenin umurunda degil, Vance. Emre'yi korumak icin yanlis adamdin.\n"
			"Atolyemden uzak dur. Eski cevre seni istemiyor.\" (Tier 6 kilitli)")));
}

bool FBNStoryState::RecordBossVictory(int32 Rank, TArray<FBNStoryCard>& OutCards)
{
	if (!CanChallengeBoss(Rank))
	{
		return false;
	}

	bIntroShown = true;
	DefeatedMask |= (1u << Rank);
	NextBossRank = Rank > 1 ? Rank - 1 : 0;

	if (Rank == 16)
	{
		Act = EBNStoryAct::Perde2_Yukselis;
		OutCards.Add(MakeCard(TEXT("C04B"), TEXT("R2 - SOKAK AGI ACILDI"), TEXT("KEREM YALCIN"),
			TEXT("#16 Berkay 'Kivilcim' Tuna yenildi. BLACK NUMBERS alt rota haritasi ele gecti.\n"
				"Simdilik polis seni yalnizca 'K. Vance' olarak taniyor (Heat 1-5).")));
	}
	else if (Rank == 12)
	{
		TarikAttitude = EBNTarikAttitude::Suspicious;
		OutCards.Add(MakeCard(TEXT("C05"), TEXT("C05 - R3 ILK SOMUT DELIL"), TEXT("TARIK KAYA"),
			TEXT("#12 Leyla Ozkan'in garajindan ozel siparis termal valf kodu cikti.\n"
				"Tarik: \"Standart yaris parcasi degil... Yakit hattinda bu kodun isi neydi?\"")));
	}
	else if (Rank == 8)
	{
		Act = EBNStoryAct::Perde3_Ifsa;
		bIdentityExposed = true;
		TarikAttitude = EBNTarikAttitude::DeepDoubt;
		OutCards.Add(MakeCard(TEXT("C06"), TEXT("C06 - R4 KIMLIK IFSASI & HEAT 6"), TEXT("BASKOMISER DEMIR"),
			TEXT("#08 Hakan 'Lens' Erturk yenilince yuz ve plaka eslesmesini polise sizdirdi.\n"
				"Demir: \"K. Vance dedikleri 5 yildir aradigimiz Kerem Yalcin! CANLI yakalayin!\" (Heat 6-10 acildi)")));
	}
	else if (Rank == 6)
	{
		bSelinExposed = true;
		OutCards.Add(MakeCard(TEXT("C07"), TEXT("C07 - R5 SELIN'IN ROZETI"), TEXT("KEREM YALCIN"),
			TEXT("#06 Mirac Alkan'in mali dosyalarindan Demir'in birimine ait gizli takip evraki cikti.\n"
				"Selin, Baskomiser Demir'in ozel biriminde polis memuru! Guven catismasi derinlesiyor.")));
	}
	else if (Rank == 5)
	{
		Act = EBNStoryAct::Perde4_Zirve;
		bTarikReconciled = true;
		TarikAttitude = EBNTarikAttitude::Reconciled;
		OutCards.Add(MakeCard(TEXT("C08"), TEXT("C08 - R6 TARIK ILE BARISMA (TIER 6)"), TEXT("TARIK KAYA"),
			TEXT("#05 Kaan 'Kurye' Vural'in sevkiyat kaydi Kerem'in aletlerinin gece calindigini kanitladi.\n"
				"Tarik: \"Seni 5 yil yanlis sucladim. Emre'nin yarim kalan Tier 6 projesini son kez aciyorum.\"")));
	}
	else if (Rank == 2)
	{
		bEvidenceComplete = true;
		OutCards.Add(MakeCard(TEXT("C09"), TEXT("C09 - R8 KOMPLO ZINCIRI TAMAM"), TEXT("SELIN KARAHAN"),
			TEXT("#04-#02 belgeleriyle Ilhan'in sabit valfi, Kaan'in hirsizligi ve Elena'nin savcilik odemesi tamam.\n"
				"Viktor Serez'i kanitlamak icin Karamara Buyuk Turu (#01) acildi!")));
	}
	else if (Rank == 1)
	{
		bViktorArrested = true;
		NextBossRank = 0;
		Act = EBNStoryAct::Perde5_SonKovalamaca;
		bFinalChaseActive = true;
		OutCards.Add(MakeCard(TEXT("C11"), TEXT("C10/C11 - ESKI LIMAN KOPRUSU"), TEXT("SELIN KARAHAN"),
			TEXT("Viktor Serez Karamara Buyuk Turu'nde yenildi ve Eski Liman Koprusu'nde polise teslim edildi.\n"
				"BLACK NUMBERS coktu. Emre'nin olumunde Kerem Yalcin'in masumiyeti kanitlandi.")));
		OutCards.Add(MakeCard(TEXT("C12"), TEXT("C12/C13 - OZEL KADEME: OLU YA DA DIRI"), TEXT("BASKOMISER DEMIR"),
			TEXT("Demir 5 yil onceki yanlis karari ve Sehir Faturasi'ni hazmedemeyip kontrolu kaybetti:\n"
				"\"ONU BANA GETIRIN! OLU YA DA DIRI!\" - Selin kordonu 15 sn araliyor; A veya B rotasina kac!")));
	}

	RebuildEvidenceNotes();
	return true;
}

bool FBNStoryState::CompleteFinalChaseEscape(int32 RouteIndex, int32 CityDamageDollars, TArray<FBNStoryCard>& OutCards)
{
	if (!bFinalChaseActive && bCampaignCompleted)
	{
		return false;
	}
	bFinalChaseActive = false;
	bCampaignCompleted = true;
	EscapedRouteIndex = RouteIndex == 1 ? 1 : 0;
	FinalCityDamageDollars = FMath::Max(0, CityDamageDollars);
	Act = EBNStoryAct::Epilog;
	TarikAttitude = EBNTarikAttitude::Retired;

	OutCards.Add(MakeCard(TEXT("C15"), TEXT("C14/C15/C17 - SEHIR FATURASI & DEMIR"), TEXT("KARAMARA HABER"),
		FString::Printf(TEXT("%s asildi. Sehir Faturasi: $%s hasar kayda gecti.\n"
			"Selin'in Icislere ilettigi telsiz kaydiyla Demir Arslan tutuklandi. Selin gorevine temiz devam ediyor."),
			EscapeRouteName(EscapedRouteIndex), *FString::FormatAsNumber(FinalCityDamageDollars))));

	OutCards.Add(MakeCard(TEXT("C16"), TEXT("C16 - DOGU SANAYI ATOLYESI"), TEXT("TARIK KAYA"),
		TEXT("Arda Celik'in yasal galerisinin yaninda genc bir musteri Tier 6 modifiye ister:\n"
			"Tarik: \"O gunler eskide kaldi, adamim.\" ... \"Dukkanimi terk et. Yoksa polisi ararim.\"")));

	OutCards.Add(MakeCard(TEXT("C19"), TEXT("C18/C19 - KUZEY GECIDI & KIYI"), TEXT("KEREM YALCIN"),
		TEXT("Kerem, Kuzey Gecidi mezarliginda Emre'nin mezari basina anahtarlarini birakir.\n"
			"Sabotajdan masum, donusteki yikimin agirligiyla adsiz bir kiyi kasabasina cekilir. Yarisa donmez.")));

	RebuildEvidenceNotes();
	return true;
}

void FBNStoryState::SkipToRank(int32 TargetRank)
{
	Reset();
	const int32 Clamped = FMath::Clamp(TargetRank, 0, 16);
	TArray<FBNStoryCard> Dummy;
	bIntroShown = true;
	for (int32 r = 16; r > Clamped; --r)
	{
		RecordBossVictory(r, Dummy);
	}
}

FString FBNStoryState::GetCurrentObjectiveLine() const
{
	if (bCampaignCompleted)
	{
		return FString::Printf(TEXT("EPILOG TAMAMLANDI  |  %s  |  SEHIR FATURASI: $%s"),
			EscapeRouteName(EscapedRouteIndex), *FString::FormatAsNumber(FinalCityDamageDollars));
	}
	if (bFinalChaseActive)
	{
		return TEXT("SON KOVALAMACA (OZEL KADEME): LIMAN TUNELLERI VEYA KUZEY GECIDI'NE KAC!");
	}
	if (const FBNBossSpec* Next = FindBossByRank(NextBossRank))
	{
		return FString::Printf(TEXT("SIRADAKI HEDEF #%02d: %s (%s / %s)  -  [B] DOSYA"),
			Next->Rank, *Next->Name, *Next->VehicleName, *Next->DisciplineLabel);
	}
	return TEXT("[B] BLACK NUMBERS DOSYASI");
}
