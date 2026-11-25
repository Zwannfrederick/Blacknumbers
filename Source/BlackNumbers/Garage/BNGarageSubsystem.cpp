#include "Garage/BNGarageSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Story/BNStorySubsystem.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr int32 SaveUserIndex = 0;
	constexpr float RewardBannerDuration = 4.0f;

	bool ShouldAutoPersist()
	{
		return !FApp::IsUnattended() && !GIsAutomationTesting;
	}
}

void UBNProgressionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Profile = FBNPlayerProfile();
	bGarageOpen = false;
	SelectedRow = 0;
	if (ShouldAutoPersist())
	{
		LoadProfile();
	}
}

void UBNProgressionSubsystem::ApplyProfileToCar(ABNVehiclePawn& Car) const
{
	Car.ApplyVehicleSpec(Profile.ActiveCar, Profile.GetActiveStats());
	if (Profile.GetActivePaintIndex() >= 0)
	{
		Car.ApplyPaintTint(Profile.GetActivePaintTint());
	}
	Car.ApplyPaintFinish(Profile.GetActivePaintFinish());
	Car.ApplyRimPaint(Profile.GetActiveRimPaintTint());
	const int32 NeonIdx = Profile.GetActiveUnderglowIndex();
	Car.ApplyUnderglow(Profile.GetActiveUnderglowColor(), NeonIdx > 0);
}

bool UBNProgressionSubsystem::TrySetCustomPaint(int32 PaletteIndex, ABNVehiclePawn* Car)
{
	Profile.SetActivePaintIndex(PaletteIndex);
	if (Car)
	{
		Car->ApplyPaintTint(Profile.GetActivePaintTint());
		Car->ApplyPaintFinish(Profile.GetActivePaintFinish());
	}
	const TArray<TPair<FString, FLinearColor>>& Palette = FBNVehicleSpec::GetPaintPalette();
	FeedbackText = Palette.IsValidIndex(PaletteIndex)
		? FString::Printf(TEXT("Boya uygulandı: %s"), *Palette[PaletteIndex].Key)
		: TEXT("Fabrika boyasına dönüldü.");
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
	return true;
}

bool UBNProgressionSubsystem::TrySetPaintFinish(EBNPaintFinish Finish, ABNVehiclePawn* Car)
{
	Profile.SetActivePaintFinish(Finish);
	if (Car)
	{
		Car->ApplyPaintFinish(Finish);
	}
	FeedbackText = FString::Printf(TEXT("Boya yüzeyi: %s"), FBNVehicleSpec::PaintFinishLabel(Finish));
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
	return true;
}

bool UBNProgressionSubsystem::TrySetRimPaint(int32 RimIndex, ABNVehiclePawn* Car)
{
	Profile.SetActiveRimPaintIndex(RimIndex);
	if (Car)
	{
		Car->ApplyRimPaint(Profile.GetActiveRimPaintTint());
	}
	const auto& Rims = FBNVehicleSpec::GetRimPaintPalette();
	const int32 Clamped = Profile.GetActiveRimPaintIndex();
	FeedbackText = Rims.IsValidIndex(Clamped)
		? FString::Printf(TEXT("Jant rengi: %s"), *Rims[Clamped].Key)
		: TEXT("Jant rengi güncellendi.");
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
	return true;
}

bool UBNProgressionSubsystem::TrySetUnderglow(int32 NeonIndex, ABNVehiclePawn* Car)
{
	Profile.SetActiveUnderglowIndex(NeonIndex);
	const int32 Clamped = Profile.GetActiveUnderglowIndex();
	if (Car)
	{
		Car->ApplyUnderglow(Profile.GetActiveUnderglowColor(), Clamped > 0);
	}
	const auto& Neons = FBNVehicleSpec::GetUnderglowPalette();
	FeedbackText = Neons.IsValidIndex(Clamped)
		? FString::Printf(TEXT("Alt neon: %s"), *Neons[Clamped].Key)
		: TEXT("Alt neon güncellendi.");
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
	return true;
}

void UBNProgressionSubsystem::SetDifficulty(EBNDifficulty NewDifficulty)
{
	Profile.Difficulty = NewDifficulty;
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
}

bool UBNProgressionSubsystem::RecordRacePersonalBest(const FString& RaceKey, bool bDrift, float FinishSeconds, int32 DriftPoints, int32& OutBonusCash)
{
	OutBonusCash = 0;
	bool bImproved = false;
	if (bDrift)
	{
		int32 Prev = 0;
		bImproved = Profile.RecordBestDriftScore(RaceKey, DriftPoints, &Prev);
		if (bImproved && Prev > 0)
		{
			OutBonusCash = 1000;
			Profile.Cash += OutBonusCash;
		}
	}
	else
	{
		float Prev = 0.f;
		bImproved = Profile.RecordBestRaceTime(RaceKey, FinishSeconds, &Prev);
		if (bImproved && Prev > 0.f)
		{
			OutBonusCash = 1000;
			Profile.Cash += OutBonusCash;
		}
	}
	if (bImproved && ShouldAutoPersist())
	{
		SaveProfile();
	}
	return bImproved;
}

bool UBNProgressionSubsystem::RecordSpeedTrapResult(const FString& TrapKey, const FString& TrapLabel, float SpeedKmh, float& OutPrevBest, int32& OutCash, int32& OutRep)
{
	const bool bNewRecord = Profile.RecordSpeedTrap(TrapKey, SpeedKmh, OutPrevBest, OutCash, OutRep);
	if (bNewRecord)
	{
		RewardBanner = FString::Printf(TEXT("HIZ TUZAĞI REKORU: %s — %.0f KM/H  (+$%s  +%d İTİBAR)"),
			*TrapLabel, SpeedKmh, *FString::FormatAsNumber(OutCash), OutRep);
		RewardBannerRemaining = RewardBannerDuration;
		if (ShouldAutoPersist())
		{
			SaveProfile();
		}
	}
	return bNewRecord;
}

bool UBNProgressionSubsystem::RecordStuntJumpResult(const FString& JumpKey, const FString& JumpLabel, float DistanceMeters, float& OutPrevBest, int32& OutCash, int32& OutRep)
{
	const bool bNewRecord = Profile.RecordStuntJump(JumpKey, DistanceMeters, OutPrevBest, OutCash, OutRep);
	if (bNewRecord)
	{
		RewardBanner = FString::Printf(TEXT("SIÇRAMA REKORU: %s — %.0fm  (+$%s  +%d İTİBAR)"),
			*JumpLabel, DistanceMeters, *FString::FormatAsNumber(OutCash), OutRep);
		RewardBannerRemaining = RewardBannerDuration;
		if (ShouldAutoPersist())
		{
			SaveProfile();
		}
	}
	return bNewRecord;
}

void UBNProgressionSubsystem::ShowNotificationBanner(const FString& BannerText, float DurationSeconds)
{
	RewardBanner = BannerText;
	RewardBannerRemaining = FMath::Max(0.5f, DurationSeconds);
}

bool UBNProgressionSubsystem::TryBuyOrSelectCar(EBNCarId CarId, ABNVehiclePawn* Car)
{
	const bool bAlreadyOwned = Profile.IsCarOwned(CarId);
	if (!Profile.BuyOrSelectCar(CarId))
	{
		FeedbackText = FString::Printf(TEXT("Yetersiz nakit! ($%s gerekli)"),
			*FString::FormatAsNumber(FBNVehicleSpec::ForCar(CarId).PurchaseCost));
		return false;
	}
	if (Car)
	{
		ApplyProfileToCar(*Car);
	}
	const FBNVehicleSpec Spec = FBNVehicleSpec::ForCar(CarId);
	FeedbackText = bAlreadyOwned
		? FString::Printf(TEXT("%s secildi."), *Spec.DisplayName)
		: FString::Printf(TEXT("%s satin alindi ve aktif edildi!"), *Spec.DisplayName);
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
	return true;
}

bool UBNProgressionSubsystem::TryBuyUpgrade(EBNUpgradeSlot Slot, ABNVehiclePawn* Car)
{
	const int32 CurrentTier = Profile.GetUpgradeTier(Slot);
	if (CurrentTier >= Profile.GetMaxAllowedUpgradeTier())
	{
		FeedbackText = Profile.bTier6Unlocked
			? TEXT("Bu parca maksimum Tier 6 seviyesinde.")
			: TEXT("Tier 6 kilitli! (Tarik Kaya ile R6 barismasi gerekir)");
		return false;
	}
	if (!Profile.BuyUpgrade(Slot))
	{
		FeedbackText = FString::Printf(TEXT("Yetersiz nakit! ($%s gerekli)"),
			*FString::FormatAsNumber(FBNUpgradeSpec::CostForTier(CurrentTier + 1)));
		return false;
	}
	if (Car)
	{
		ApplyProfileToCar(*Car);
	}
	FeedbackText = FString::Printf(TEXT("%s -> Tier %d takildi!"), FBNUpgradeSpec::SlotLabel(Slot), CurrentTier + 1);
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
	return true;
}

void UBNProgressionSubsystem::GrantRaceReward(int32 Place, int32 TotalRacers, bool bDrift, int32 DriftPoints)
{
	int32 CashGain = 0;
	int32 RepGain = 0;
	Profile.AwardRaceResult(Place, TotalRacers, bDrift, DriftPoints, CashGain, RepGain);
	RewardBanner = FString::Printf(TEXT("YARIS ODULU: +$%s   +%d ITIBAR"), *FString::FormatAsNumber(CashGain), RepGain);
	RewardBannerRemaining = RewardBannerDuration;
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
}

void UBNProgressionSubsystem::GrantPursuitEscapeReward(int32 Bounty, int32 HeatLevel)
{
	int32 RepGain = 0;
	Profile.AwardPursuitEscape(Bounty, HeatLevel, RepGain);
	RewardBanner = FString::Printf(TEXT("KACIS ODULU: +%d ITIBAR"), RepGain);
	RewardBannerRemaining = RewardBannerDuration;
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
}

void UBNProgressionSubsystem::AddCash(int32 Amount)
{
	Profile.Cash = FMath::Max(0, Profile.Cash + Amount);
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
}

void UBNProgressionSubsystem::SetTier6Unlocked(bool bUnlocked)
{
	Profile.bTier6Unlocked = bUnlocked;
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
}

void UBNProgressionSubsystem::GrantStoryReward(int32 CashGain, int32 RepGain, const FString& Headline)
{
	Profile.Cash = FMath::Max(0, Profile.Cash + FMath::Max(0, CashGain));
	Profile.Reputation = FMath::Max(0, Profile.Reputation + FMath::Max(0, RepGain));
	++Profile.RacesWon;
	++Profile.RacesCompleted;
	RewardBanner = Headline;
	RewardBannerRemaining = RewardBannerDuration;
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
}

bool UBNProgressionSubsystem::SaveProfile(const FString& SlotName) const
{
	UBNSaveGame* SaveObj = Cast<UBNSaveGame>(UGameplayStatics::CreateSaveGameObject(UBNSaveGame::StaticClass()));
	if (!SaveObj)
	{
		return false;
	}
	SaveObj->Cash = Profile.Cash;
	SaveObj->Reputation = Profile.Reputation;
	SaveObj->RacesWon = Profile.RacesWon;
	SaveObj->RacesCompleted = Profile.RacesCompleted;
	SaveObj->bTier6Unlocked = Profile.bTier6Unlocked;
	SaveObj->ActiveCarIndex = static_cast<int32>(Profile.ActiveCar);

	SaveObj->OwnedCars.SetNum(FBNUpgradeSpec::NumCars);
	SaveObj->FlatUpgrades.SetNum(FBNUpgradeSpec::NumCars * FBNUpgradeSpec::NumSlots);
	SaveObj->CustomPaintIndices.SetNum(FBNUpgradeSpec::NumCars);
	SaveObj->PaintFinishes.SetNum(FBNUpgradeSpec::NumCars);
	SaveObj->RimPaintIndices.SetNum(FBNUpgradeSpec::NumCars);
	SaveObj->UnderglowIndices.SetNum(FBNUpgradeSpec::NumCars);
	SaveObj->Difficulty = static_cast<uint8>(Profile.Difficulty);
	SaveObj->BestRaceTimes = Profile.BestRaceTimes;
	SaveObj->BestDriftScores = Profile.BestDriftScores;
	SaveObj->BestSpeedTrapKmh = Profile.BestSpeedTrapKmh;
	SaveObj->BestJumpMeters = Profile.BestJumpMeters;
	for (int32 c = 0; c < FBNUpgradeSpec::NumCars; ++c)
	{
		SaveObj->OwnedCars[c] = Profile.OwnedCars[c];
		SaveObj->CustomPaintIndices[c] = Profile.CustomPaintIndices[c];
		SaveObj->PaintFinishes[c] = static_cast<uint8>(Profile.PaintFinishes[c]);
		SaveObj->RimPaintIndices[c] = Profile.RimPaintIndices[c];
		SaveObj->UnderglowIndices[c] = Profile.UnderglowIndices[c];
		for (int32 s = 0; s < FBNUpgradeSpec::NumSlots; ++s)
		{
			SaveObj->FlatUpgrades[c * FBNUpgradeSpec::NumSlots + s] = Profile.Upgrades[c][s];
		}
	}
	if (const UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
	{
		Story->WriteToSaveGame(*SaveObj);
	}
	return UGameplayStatics::SaveGameToSlot(SaveObj, SlotName, SaveUserIndex);
}

bool UBNProgressionSubsystem::LoadProfile(const FString& SlotName)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex))
	{
		return false;
	}
	const UBNSaveGame* Loaded = Cast<UBNSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, SaveUserIndex));
	if (!Loaded)
	{
		return false;
	}
	LoadedSaveSnapshot = Loaded;
	Profile.Cash = Loaded->Cash;
	Profile.Reputation = Loaded->Reputation;
	Profile.RacesWon = Loaded->RacesWon;
	Profile.RacesCompleted = Loaded->RacesCompleted;
	Profile.bTier6Unlocked = Loaded->bTier6Unlocked;
	Profile.ActiveCar = static_cast<EBNCarId>(FMath::Clamp(Loaded->ActiveCarIndex, 0, FBNUpgradeSpec::NumCars - 1));
	Profile.Difficulty = static_cast<EBNDifficulty>(FMath::Clamp<int32>(Loaded->Difficulty, 0, static_cast<int32>(EBNDifficulty::Count) - 1));
	Profile.BestRaceTimes = Loaded->BestRaceTimes;
	Profile.BestDriftScores = Loaded->BestDriftScores;
	Profile.BestSpeedTrapKmh = Loaded->BestSpeedTrapKmh;
	Profile.BestJumpMeters = Loaded->BestJumpMeters;
	for (int32 c = 0; c < FBNUpgradeSpec::NumCars; ++c)
	{
		Profile.OwnedCars[c] = Loaded->OwnedCars.IsValidIndex(c) ? Loaded->OwnedCars[c] : false;
		Profile.CustomPaintIndices[c] = Loaded->CustomPaintIndices.IsValidIndex(c) ? Loaded->CustomPaintIndices[c] : -1;
		if (Loaded->PaintFinishes.IsValidIndex(c))
		{
			Profile.PaintFinishes[c] = static_cast<EBNPaintFinish>(FMath::Clamp<int32>(Loaded->PaintFinishes[c], 0, static_cast<int32>(EBNPaintFinish::Count) - 1));
		}
		Profile.RimPaintIndices[c] = Loaded->RimPaintIndices.IsValidIndex(c)
			? FMath::Clamp(Loaded->RimPaintIndices[c], 0, FBNVehicleSpec::GetRimPaintPalette().Num() - 1) : 0;
		Profile.UnderglowIndices[c] = Loaded->UnderglowIndices.IsValidIndex(c)
			? FMath::Clamp(Loaded->UnderglowIndices[c], 0, FBNVehicleSpec::GetUnderglowPalette().Num() - 1) : 0;
		for (int32 s = 0; s < FBNUpgradeSpec::NumSlots; ++s)
		{
			const int32 FlatIdx = c * FBNUpgradeSpec::NumSlots + s;
			Profile.Upgrades[c][s] = Loaded->FlatUpgrades.IsValidIndex(FlatIdx)
				? FMath::Clamp(Loaded->FlatUpgrades[FlatIdx], 0, 6)
				: 0;
		}
	}
	Profile.OwnedCars[0] = true; // starter Kurogane M-Spec is always owned
	if (!Profile.IsCarOwned(Profile.ActiveCar))
	{
		Profile.ActiveCar = EBNCarId::KuroganeMSpec;
	}
	if (UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
	{
		Story->ReadFromSaveGame(*Loaded);
	}
	return true;
}

void UBNProgressionSubsystem::ResetProfile()
{
	LoadedSaveSnapshot = nullptr;
	Profile = FBNPlayerProfile();
	FeedbackText.Reset();
	RewardBanner.Reset();
	RewardBannerRemaining = 0.f;
	if (UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
	{
		Story->ResetStory(false);
	}
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
}

void UBNProgressionSubsystem::TickFeedback(float DeltaSeconds)
{
	if (RewardBannerRemaining > 0.f)
	{
		RewardBannerRemaining = FMath::Max(0.f, RewardBannerRemaining - FMath::Max(DeltaSeconds, 0.f));
	}
}

void UBNProgressionSubsystem::OpenGarage(ABNVehiclePawn* Car)
{
	bGarageOpen = true;
	SelectedRow = 0;
	FeedbackText = TEXT("W/S veya Yukari/Asagi ile sec, ENTER ile satin al / tak.");
	if (Car)
	{
		Car->SetInputLocked(true);
	}
}

void UBNProgressionSubsystem::CloseGarage(ABNVehiclePawn* Car)
{
	bGarageOpen = false;
	if (Car)
	{
		Car->SetInputLocked(false);
		ApplyProfileToCar(*Car);
	}
	if (ShouldAutoPersist())
	{
		SaveProfile();
	}
}

void UBNProgressionSubsystem::MoveSelection(int32 Delta)
{
	SelectedRow = (SelectedRow + Delta % GarageRowCount + GarageRowCount) % GarageRowCount;
}

void UBNProgressionSubsystem::ConfirmSelection(ABNVehiclePawn* Car)
{
	if (SelectedRow < FBNUpgradeSpec::NumSlots)
	{
		TryBuyUpgrade(static_cast<EBNUpgradeSlot>(SelectedRow), Car);
	}
	else
	{
		const int32 CarIdx = SelectedRow - FBNUpgradeSpec::NumSlots;
		TryBuyOrSelectCar(static_cast<EBNCarId>(CarIdx), Car);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBNAddCashCmd(
	TEXT("bn.AddCash"),
	TEXT("bn.AddCash <amount>  Adds cash to the player's BLACK NUMBERS garage profile."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UBNProgressionSubsystem* Prog = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
		{
			if (Args.Num() == 1 && Args[0].IsNumeric())
			{
				Prog->AddCash(FCString::Atoi(*Args[0]));
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GBNUnlockTier6Cmd(
	TEXT("bn.UnlockTier6"),
	TEXT("bn.UnlockTier6 <0|1>  Unlocks or locks Tarik's Tier 6 upgrades."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UBNProgressionSubsystem* Prog = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
		{
			const bool bUnlock = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0;
			Prog->SetTier6Unlocked(bUnlock);
		}
	}));

static FAutoConsoleCommandWithWorld GBNSaveCmd(
	TEXT("bn.Save"),
	TEXT("Saves the BLACK NUMBERS garage profile."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UBNProgressionSubsystem* Prog = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
		{
			Prog->SaveProfile();
		}
	}));

static FAutoConsoleCommandWithWorld GBNResetSaveCmd(
	TEXT("bn.ResetSave"),
	TEXT("Resets the BLACK NUMBERS garage profile back to stock."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UBNProgressionSubsystem* Prog = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
		{
			Prog->ResetProfile();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GBNMapCmd(
	TEXT("bn.Map"),
	TEXT("bn.Map <Karamara|TestTrack>  Opens the Karamara city slice or test track."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() != 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("Usage: bn.Map <Karamara|TestTrack>"));
			return;
		}
		const FName LevelName = Args[0].Equals(TEXT("Karamara"), ESearchCase::IgnoreCase)
			? FName(TEXT("/Game/Maps/KaramaraSlice"))
			: FName(TEXT("/Game/Maps/TestTrack"));
		UGameplayStatics::OpenLevel(World, LevelName);
	}));
