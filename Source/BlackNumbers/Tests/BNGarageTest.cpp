#include "Misc/AutomationTest.h"
#include "Garage/BNGarageTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNProgressionLogicTest, "BlackNumbers.Garage.ProgressionLogic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNProgressionLogicTest::RunTest(const FString& Parameters)
{
	FBNPlayerProfile Profile;
	TestEqual(TEXT("garage catalog has 6 fictional vehicles"), FBNUpgradeSpec::NumCars, 6);
	TestEqual(TEXT("starts with starter Kurogane M-Spec"), Profile.ActiveCar, EBNCarId::KuroganeMSpec);
	TestTrue(TEXT("starter car is owned"), Profile.IsCarOwned(EBNCarId::KuroganeMSpec));
	TestFalse(TEXT("Valken RS-4 starts locked"), Profile.IsCarOwned(EBNCarId::ValkenRS4));
	TestFalse(TEXT("Tauron V6 Rally starts locked"), Profile.IsCarOwned(EBNCarId::TauronV6Rally));
	TestFalse(TEXT("Draco RS-T starts locked"), Profile.IsCarOwned(EBNCarId::DracoRST));
	TestFalse(TEXT("Nocturne BN-01 starts locked"), Profile.IsCarOwned(EBNCarId::NocturneBN01));
	TestFalse(TEXT("Tier 6 starts locked before Tarik R6"), Profile.bTier6Unlocked);
	TestEqual(TEXT("max upgrade tier before R6 is 5"), Profile.GetMaxAllowedUpgradeTier(), 5);

	// Validate all 6 car specs: strictly ascending cost, distinct styles/kits, valid ClearCoat/Metallic.
	int32 PrevCost = -1;
	for (int32 c = 0; c < FBNUpgradeSpec::NumCars; ++c)
	{
		const EBNCarId Id = static_cast<EBNCarId>(c);
		const FBNVehicleSpec Spec = FBNVehicleSpec::ForCar(Id);
		TestEqual(TEXT("spec Id matches requested car"), Spec.Id, Id);
		TestFalse(TEXT("car has non-empty DisplayName"), Spec.DisplayName.IsEmpty());
		TestFalse(TEXT("car has non-empty Subtitle"), Spec.Subtitle.IsEmpty());
		TestTrue(TEXT("car prices strictly ascend through progression"), Spec.PurchaseCost > PrevCost);
		TestTrue(TEXT("Metallic in [0,1]"), Spec.Metallic >= 0.f && Spec.Metallic <= 1.f);
		TestTrue(TEXT("ClearCoat in [0,1]"), Spec.ClearCoat >= 0.f && Spec.ClearCoat <= 1.f);
		if (c > 0)
		{
			TestFalse(TEXT("non-starter car starts locked"), Profile.IsCarOwned(Id));
		}
		PrevCost = Spec.PurchaseCost;
	}
	TestEqual(TEXT("Tauron V6 Rally uses RallyOffroad style"), FBNVehicleSpec::ForCar(EBNCarId::TauronV6Rally).BodyStyle, EBNBodyStyle::RallyOffroad);
	TestEqual(TEXT("Nocturne BN-01 uses HyperConcept style"), FBNVehicleSpec::ForCar(EBNCarId::NocturneBN01).BodyStyle, EBNBodyStyle::HyperConcept);
	TestEqual(TEXT("Nocturne BN-01 costs $95,000"), FBNVehicleSpec::ForCar(EBNCarId::NocturneBN01).PurchaseCost, 95000);

	// Stock Tier 0 Kurogane must preserve exact original physics baseline for existing tests.
	const FBNComputedVehicleStats Stock = Profile.GetActiveStats();
	TestEqual(TEXT("stock MaxTorqueNm"), Stock.MaxTorqueNm, 400.f);
	TestEqual(TEXT("stock MaxRpm"), Stock.MaxRpm, 7000.f);
	TestEqual(TEXT("stock DragCoefficient"), Stock.DragCoefficient, 0.31f);
	TestEqual(TEXT("stock TireGripScale"), Stock.TireGripScale, 1.f);
	TestEqual(TEXT("stock DriftThrust"), Stock.DriftThrust, 1100.f);

	// Grant ample cash and test Tier 1-5 progression + strict Tier 6 gate.
	Profile.Cash = 500000;
	for (int32 t = 1; t <= 5; ++t)
	{
		TestTrue(FString::Printf(TEXT("buy Engine Tier %d"), t), Profile.BuyUpgrade(EBNUpgradeSlot::Engine));
		TestEqual(TEXT("Engine tier updated"), Profile.GetUpgradeTier(EBNUpgradeSlot::Engine), t);
	}
	TestFalse(TEXT("Tier 6 purchase blocked before Tarik reconciliation (Kanon #8)"), Profile.BuyUpgrade(EBNUpgradeSlot::Engine));

	Profile.bTier6Unlocked = true;
	TestTrue(TEXT("Tier 6 purchase succeeds once unlocked"), Profile.BuyUpgrade(EBNUpgradeSlot::Engine));
	TestEqual(TEXT("Engine reaches Tier 6"), Profile.GetUpgradeTier(EBNUpgradeSlot::Engine), 6);
	TestFalse(TEXT("cannot upgrade past Tier 6"), Profile.BuyUpgrade(EBNUpgradeSlot::Engine));

	const FBNComputedVehicleStats Upgraded = Profile.GetActiveStats();
	TestTrue(TEXT("upgraded torque exceeds stock"), Upgraded.MaxTorqueNm > Stock.MaxTorqueNm);
	TestTrue(TEXT("upgraded RPM exceeds stock"), Upgraded.MaxRpm > Stock.MaxRpm);

	// Car purchase and independent upgrade slots across all 6 cars.
	TestTrue(TEXT("buy Valken RS-4"), Profile.BuyOrSelectCar(EBNCarId::ValkenRS4));
	TestEqual(TEXT("active car switched to Valken RS-4"), Profile.ActiveCar, EBNCarId::ValkenRS4);
	TestEqual(TEXT("new car starts at stock Engine Tier 0"), Profile.GetUpgradeTier(EBNUpgradeSlot::Engine), 0);
	TestTrue(TEXT("buy Vandal 440R"), Profile.BuyOrSelectCar(EBNCarId::Vandal440R));
	TestTrue(TEXT("buy Tauron V6 Rally"), Profile.BuyOrSelectCar(EBNCarId::TauronV6Rally));
	TestTrue(TEXT("buy Draco RS-T"), Profile.BuyOrSelectCar(EBNCarId::DracoRST));
	TestTrue(TEXT("buy Nocturne BN-01"), Profile.BuyOrSelectCar(EBNCarId::NocturneBN01));
	TestEqual(TEXT("active car switched to Nocturne BN-01"), Profile.ActiveCar, EBNCarId::NocturneBN01);
	const FBNComputedVehicleStats NocturneStock = Profile.GetActiveStats();
	TestTrue(TEXT("Nocturne BN-01 stock torque exceeds Kurogane stock"), NocturneStock.MaxTorqueNm > Stock.MaxTorqueNm);
	TestTrue(TEXT("Nocturne BN-01 stock grip exceeds Kurogane stock"), NocturneStock.TireGripScale > Stock.TireGripScale);
	TestTrue(TEXT("switch back to Kurogane M-Spec"), Profile.BuyOrSelectCar(EBNCarId::KuroganeMSpec));
	TestEqual(TEXT("Kurogane keeps its Tier 6 Engine"), Profile.GetUpgradeTier(EBNUpgradeSlot::Engine), 6);

	// Race and pursuit escape rewards.
	int32 OutCash = 0, OutRep = 0;
	const int32 CashBefore = Profile.Cash;
	const int32 RepBefore = Profile.Reputation;
	Profile.AwardRaceResult(1, 4, false, 0, OutCash, OutRep);
	TestEqual(TEXT("1st place pays $5000"), OutCash, 5000);
	TestEqual(TEXT("Cash increased by reward"), Profile.Cash, CashBefore + 5000);
	TestEqual(TEXT("Reputation increased by reward"), Profile.Reputation, RepBefore + OutRep);
	TestEqual(TEXT("RacesWon incremented"), Profile.RacesWon, 1);

	int32 EscapeRep = 0;
	Profile.AwardPursuitEscape(4000, 3, EscapeRep);
	TestTrue(TEXT("Pursuit escape grants reputation"), EscapeRep >= 500);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNPinkSlipTest, "BlackNumbers.Garage.PinkSlip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNPinkSlipTest::RunTest(const FString& Parameters)
{
	FBNPlayerProfile Profile;
	const int32 CashBefore = Profile.Cash;
	TestFalse(TEXT("rival car not owned before the race"), Profile.IsCarOwned(EBNCarId::DracoRST));
	TestTrue(TEXT("pink slip hands over the rival's car"), Profile.AwardPinkSlip(EBNCarId::DracoRST));
	TestTrue(TEXT("car is now in the garage"), Profile.IsCarOwned(EBNCarId::DracoRST));
	TestEqual(TEXT("pink slip costs nothing"), Profile.Cash, CashBefore);
	TestFalse(TEXT("winning an already owned car is not a new pink slip"), Profile.AwardPinkSlip(EBNCarId::DracoRST));
	TestFalse(TEXT("invalid car id rejected"), Profile.AwardPinkSlip(static_cast<EBNCarId>(250)));
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Garage/BNGarageSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/BNPlayTestUtils.h"
#include "Vehicle/BNVehiclePawn.h"

using namespace BNPlayTest;

namespace
{
	const FVector RunwayStart(-18000.f, 0.f, 60.f);
	constexpr float SettleSeconds = 0.8f;
	constexpr float AccelMeasureSeconds = 2.5f;
	constexpr float MinUpgradeSpeedGainKmh = 6.f;

	struct FGaragePieState
	{
		enum class EPhase { PlaceStock, SettleStock, DriveStock, PlaceUpgraded, SettleUpgraded, DriveUpgraded } Phase = EPhase::PlaceStock;
		double PhaseStart = 0.0;
		float StockSpeedKmh = 0.f;
		float UpgradedSpeedKmh = 0.f;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNGarageUpgradesAndSavePieTest, "BlackNumbers.Garage.UpgradesAndSavePIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNGarageUpgradesAndSavePieTest::RunTest(const FString& Parameters)
{
	StartTestTrack(*this);

	const TSharedRef<FGaragePieState> State = MakeShared<FGaragePieState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		using EPhase = FGaragePieState::EPhase;
		ABNVehiclePawn* Car = FindPlayerCar();
		UGameInstance* GI = Car ? Car->GetGameInstance() : nullptr;
		UBNProgressionSubsystem* Prog = GI ? GI->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
		if (!Car || !Prog)
		{
			AddError(TEXT("Player car or UBNProgressionSubsystem missing"));
			return true;
		}

		UChaosWheeledVehicleMovementComponent* Move = Car->GetChaosMovement();
		const double Now = Car->GetWorld()->GetTimeSeconds();
		const float InPhase = static_cast<float>(Now - State->PhaseStart);
		auto Enter = [&](EPhase Next) { State->Phase = Next; State->PhaseStart = Now; };

		switch (State->Phase)
		{
		case EPhase::PlaceStock:
			Prog->ResetProfile();
			Prog->ApplyProfileToCar(*Car);
			PlaceStopped(*Car, RunwayStart, 0.f);
			Enter(EPhase::SettleStock);
			return false;
		case EPhase::SettleStock:
			if (InPhase >= SettleSeconds)
			{
				Move->SetThrottleInput(1.f);
				Enter(EPhase::DriveStock);
			}
			return false;
		case EPhase::DriveStock:
			if (InPhase < AccelMeasureSeconds)
			{
				return false;
			}
			State->StockSpeedKmh = Car->GetSpeedKmh();
			Move->SetThrottleInput(0.f);

			// Upgrade Engine, Transmission, and Chassis to Tier 5 and apply to live car.
			Prog->AddCash(500000);
			for (int32 t = 0; t < 5; ++t)
			{
				TestTrue(TEXT("upgrade Engine"), Prog->TryBuyUpgrade(EBNUpgradeSlot::Engine, Car));
				TestTrue(TEXT("upgrade Transmission"), Prog->TryBuyUpgrade(EBNUpgradeSlot::Transmission, Car));
				TestTrue(TEXT("upgrade Chassis"), Prog->TryBuyUpgrade(EBNUpgradeSlot::Chassis, Car));
			}
			PlaceStopped(*Car, RunwayStart, 0.f);
			Enter(EPhase::SettleUpgraded);
			return false;
		case EPhase::SettleUpgraded:
			if (InPhase >= SettleSeconds)
			{
				Move->SetThrottleInput(1.f);
				Enter(EPhase::DriveUpgraded);
			}
			return false;
		case EPhase::DriveUpgraded:
			if (InPhase < AccelMeasureSeconds)
			{
				return false;
			}
			State->UpgradedSpeedKmh = Car->GetSpeedKmh();
			Move->SetThrottleInput(0.f);
			break;
		}

		UE_LOG(LogTemp, Display, TEXT("BN garage test: stock %.1f km/h -> Tier 5 %.1f km/h after %.1fs"),
			State->StockSpeedKmh, State->UpgradedSpeedKmh, AccelMeasureSeconds);
		TestTrue(TEXT("Tier 5 upgrades measurably increase acceleration"),
			State->UpgradedSpeedKmh > State->StockSpeedKmh + MinUpgradeSpeedGainKmh);

		// Verify live 6-car switching + visuals/aero application and 3-mode camera cycling (Near / Far / Hood).
		Prog->AddCash(400000);
		for (int32 c = 1; c < FBNUpgradeSpec::NumCars; ++c)
		{
			const EBNCarId Id = static_cast<EBNCarId>(c);
			TestTrue(FString::Printf(TEXT("live buy/apply car %d"), c), Prog->TryBuyOrSelectCar(Id, Car));
		}
		TestTrue(TEXT("Nocturne BN-01 owned after live switch"), Prog->GetProfile().IsCarOwned(EBNCarId::NocturneBN01));
		TestTrue(TEXT("switch back to upgraded Kurogane before SaveGame check"), Prog->TryBuyOrSelectCar(EBNCarId::KuroganeMSpec, Car));

		TestFalse(TEXT("starts in Near Chase camera"), Car->IsHoodCamera());
		Car->CycleChaseDistance();
		TestFalse(TEXT("second mode is Far Chase camera"), Car->IsHoodCamera());
		Car->CycleChaseDistance();
		TestTrue(TEXT("third mode is Hood/Kaput camera"), Car->IsHoodCamera());
		TestEqual(TEXT("Hood camera mode label"), Car->GetCameraModeLabel(), FString(TEXT("KAPUT KAMERA")));
		Car->CycleChaseDistance();
		TestFalse(TEXT("cycles cleanly back to Near Chase"), Car->IsHoodCamera());

		// Verify SaveGame persistence round-trip on an isolated slot, then clean up.
		const FString TestSlot(TEXT("BNAutoTestSlot"));
		const int32 SavedCash = Prog->GetProfile().Cash;
		TestTrue(TEXT("SaveProfile writes SaveGame slot"), Prog->SaveProfile(TestSlot));
		Prog->ResetProfile();
		TestEqual(TEXT("ResetProfile restores default cash"), Prog->GetProfile().Cash, 4000);
		TestFalse(TEXT("ResetProfile locks Nocturne BN-01"), Prog->GetProfile().IsCarOwned(EBNCarId::NocturneBN01));
		TestTrue(TEXT("LoadProfile restores saved slot"), Prog->LoadProfile(TestSlot));
		TestEqual(TEXT("loaded profile restores saved cash"), Prog->GetProfile().Cash, SavedCash);
		TestEqual(TEXT("loaded profile restores Tier 5 Engine"), Prog->GetProfile().GetUpgradeTier(EBNUpgradeSlot::Engine), 5);
		TestTrue(TEXT("loaded profile restores 6-car ownership (Nocturne BN-01)"), Prog->GetProfile().IsCarOwned(EBNCarId::NocturneBN01));

		UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
		Prog->ResetProfile();
		Prog->ApplyProfileToCar(*Car);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
