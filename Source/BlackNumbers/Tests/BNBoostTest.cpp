#include "Misc/AutomationTest.h"
#include "Vehicle/BNEngineAudioComponent.h"
#include "Vehicle/BNVehicleBoost.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNBoostLogicTest, "BlackNumbers.Vehicle.BoostLogic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNBoostLogicTest::RunTest(const FString& Parameters)
{
	const FBNBoostTuning Tuning;
	FBNBoostState State;

	// NOS drain and minimum activation threshold.
	State.UpdateNos(1.0f, true, false, true, 90.f, false, Tuning);
	TestTrue(TEXT("NOS activates when tank is full"), State.bNosActive);
	TestTrue(TEXT("NOS drains while active"), State.NosFraction < 1.f);

	// Empty tank below minimum threshold cannot re-engage once stopped.
	State.bNosActive = false;
	State.NosFraction = Tuning.NosMinActivateFraction * 0.5f;
	State.UpdateNos(0.5f, true, false, true, 90.f, false, Tuning);
	TestFalse(TEXT("NOS refuses to fire below minimum tank threshold"), State.bNosActive);

	// Passive NFS-style NOS auto-recharge fills tank even at low cruising speed without drifting.
	FBNBoostState PassiveState;
	PassiveState.NosFraction = 0.2f;
	PassiveState.UpdateNos(1.0f, false, false, true, 50.f, false, Tuning);
	TestTrue(TEXT("NOS recharges passively on its own while driving"), PassiveState.NosFraction > 0.25f);

	// Drift recharge outpaces high-speed straight recharge.
	FBNBoostState DriftState;
	DriftState.NosFraction = 0.2f;
	DriftState.UpdateNos(1.0f, false, false, true, 80.f, true, Tuning);

	FBNBoostState CruiseState;
	CruiseState.NosFraction = 0.2f;
	CruiseState.UpdateNos(1.0f, false, false, true, Tuning.NosRechargeMinSpeedKmh + 20.f, false, Tuning);
	TestTrue(TEXT("drifting recharges NOS faster than cruising"), DriftState.NosFraction > CruiseState.NosFraction);

	// Near-miss bonus clamps at 1.0.
	CruiseState.NosFraction = 0.95f;
	CruiseState.AddNearMissBonus(Tuning);
	TestEqual(TEXT("near-miss NOS clamps at full tank"), CruiseState.NosFraction, 1.f);
	TestTrue(TEXT("near-miss banner timer activates"), CruiseState.NearMissBannerRemaining > 0.f);

	// Speedbreaker drain, empty stop, and recharge.
	FBNBoostState SbState;
	SbState.UpdateSpeedbreaker(2.0f, true, false, 100.f, Tuning);
	TestTrue(TEXT("Speedbreaker activates above min speed"), SbState.bSpeedbreakerActive);
	TestTrue(TEXT("Speedbreaker drains over time"), SbState.SpeedbreakerFraction < 1.f);

	const float AfterDrain = SbState.SpeedbreakerFraction;
	SbState.UpdateSpeedbreaker(1.0f, false, false, Tuning.SpeedbreakerMinRechargeSpeedKmh + 10.f, Tuning);
	TestFalse(TEXT("Speedbreaker stops when released"), SbState.bSpeedbreakerActive);
	TestTrue(TEXT("Speedbreaker recharges while driving normally"), SbState.SpeedbreakerFraction > AfterDrain);

	SbState.SpeedbreakerFraction = Tuning.SpeedbreakerMinActivateFraction * 0.5f;
	SbState.UpdateSpeedbreaker(0.5f, true, false, 100.f, Tuning);
	TestFalse(TEXT("Speedbreaker cannot restart below min threshold"), SbState.bSpeedbreakerActive);

	// Perfect Launch sweet spot [0.58, 0.82].
	FBNBoostState LowLaunch;
	LowLaunch.LaunchRevFraction = Tuning.LaunchSweetSpotMin - 0.1f;
	TestFalse(TEXT("too-low countdown revs fail Perfect Launch"), LowLaunch.TriggerLaunchRelease(Tuning));

	FBNBoostState OverRevLaunch;
	OverRevLaunch.LaunchRevFraction = Tuning.LaunchSweetSpotMax + 0.1f;
	TestFalse(TEXT("over-revving fails Perfect Launch"), OverRevLaunch.TriggerLaunchRelease(Tuning));

	FBNBoostState GoodLaunch;
	GoodLaunch.LaunchRevFraction = 0.5f * (Tuning.LaunchSweetSpotMin + Tuning.LaunchSweetSpotMax);
	TestTrue(TEXT("revs inside sweet spot trigger Perfect Launch"), GoodLaunch.TriggerLaunchRelease(Tuning));
	TestTrue(TEXT("Perfect Launch grants post-launch boost"), GoodLaunch.LaunchBoostRemaining > 0.f);

	// Engine sound mapping with NOS, Speedbreaker pitch dilation, and defaults.
	const FBNEngineSoundTargets DefaultSound = BNEngineSound::Evaluate(4000.f, 7000.f, 1.f, 0.f, 100.f);
	TestEqual(TEXT("default sound has no NOS whine"), DefaultSound.NosGain, 0.f);
	TestEqual(TEXT("default sound keeps pitch scale 1.0"), DefaultSound.PitchScale, 1.f);
	TestEqual(TEXT("default sound has no backfire"), DefaultSound.BackfireGain, 0.f);

	const FBNEngineSoundTargets NosSlowSound = BNEngineSound::Evaluate(5500.f, 7000.f, 1.f, 0.f, 150.f, true, true, 0.38f, true);
	TestTrue(TEXT("NOS active produces turbine gain"), NosSlowSound.NosGain > 0.f);
	TestTrue(TEXT("Speedbreaker lowers audio pitch scale"), NosSlowSound.PitchScale < 0.8f);
	TestEqual(TEXT("backfire sets backfire gain"), NosSlowSound.BackfireGain, 1.f);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Tests/BNPlayTestUtils.h"
#include "Vehicle/BNVehiclePawn.h"

using namespace BNPlayTest;

namespace
{
	const FVector RunwayStart(-18000.f, 0.f, 60.f);
	constexpr float SettleSeconds = 0.8f;
	constexpr float AccelerateKmh = 60.f;
	constexpr float NosHoldSeconds = 1.5f;
	constexpr float SpeedbreakerHoldRealSeconds = 0.45f;
	constexpr float RecoverRealSeconds = 0.8f;
	constexpr float PhaseTimeoutRealSeconds = 15.f;

	struct FBoostPieState
	{
		enum class EPhase { Place, Settle, Accelerate, Nos, Speedbreaker, Recover } Phase = EPhase::Place;
		double PhaseStartReal = 0.0;
		float SpeedBeforeNos = 0.f;
		float SpeedAfterNos = 0.f;
		bool bSawNosActive = false;
		bool bSawNosAudio = false;
		bool bSawExhaustLight = false;
		float MinDilationSeen = 1.f;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNNosAndSpeedbreakerTest, "BlackNumbers.Vehicle.NosAndSpeedbreaker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNNosAndSpeedbreakerTest::RunTest(const FString& Parameters)
{
	StartTestTrack(*this);

	const TSharedRef<FBoostPieState> State = MakeShared<FBoostPieState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		using EPhase = FBoostPieState::EPhase;
		ABNVehiclePawn* Car = FindPlayerCar();
		if (!Car || !Car->EngineAudio)
		{
			AddError(TEXT("Player car or engine audio missing"));
			return true;
		}

		UChaosWheeledVehicleMovementComponent* Move = Car->GetChaosMovement();
		const double NowReal = FPlatformTime::Seconds();
		const float InPhaseReal = static_cast<float>(NowReal - State->PhaseStartReal);
		auto Enter = [&](EPhase Next) { State->Phase = Next; State->PhaseStartReal = NowReal; };

		if (State->Phase != EPhase::Place && InPhaseReal > PhaseTimeoutRealSeconds)
		{
			Car->SetNosWanted(false);
			Car->SetSpeedbreakerWanted(false);
			AddError(TEXT("NOS/Speedbreaker PIE test phase timed out"));
			return true;
		}

		switch (State->Phase)
		{
		case EPhase::Place:
			PlaceStopped(*Car, RunwayStart, 0.f);
			Car->SetNosWanted(false);
			Car->SetSpeedbreakerWanted(false);
			Enter(EPhase::Settle);
			return false;
		case EPhase::Settle:
			if (InPhaseReal >= SettleSeconds)
			{
				Move->SetThrottleInput(1.f);
				Enter(EPhase::Accelerate);
			}
			return false;
		case EPhase::Accelerate:
			if (Car->GetSpeedKmh() >= AccelerateKmh)
			{
				State->SpeedBeforeNos = Car->GetSpeedKmh();
				Car->SetNosWanted(true);
				Enter(EPhase::Nos);
			}
			return false;
		case EPhase::Nos:
			State->bSawNosActive |= Car->IsNosActive();
			State->bSawNosAudio |= Car->EngineAudio->GetTargets().NosGain > 0.f;
			for (const UPointLightComponent* Light : Car->ExhaustLights)
			{
				State->bSawExhaustLight |= Light && Light->IsVisible();
			}
			if (InPhaseReal >= NosHoldSeconds)
			{
				State->SpeedAfterNos = Car->GetSpeedKmh();
				Car->SetNosWanted(false);
				Car->SetSpeedbreakerWanted(true);
				Enter(EPhase::Speedbreaker);
			}
			return false;
		case EPhase::Speedbreaker:
		{
			const float Dilation = Car->GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation();
			State->MinDilationSeen = FMath::Min(State->MinDilationSeen, Dilation);
			if (InPhaseReal >= SpeedbreakerHoldRealSeconds)
			{
				TestTrue(TEXT("Speedbreaker active before release"), Car->IsSpeedbreakerActive());
				Car->SetSpeedbreakerWanted(false);
				Enter(EPhase::Recover);
			}
			return false;
		}
		case EPhase::Recover:
			if (InPhaseReal < RecoverRealSeconds)
			{
				return false;
			}
			break;
		}

		const float FinalDilation = Car->GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation();
		UE_LOG(LogTemp, Display, TEXT("BN boost test: NOS %.1f -> %.1f km/h (tank %.2f), min dilation %.2f, final dilation %.2f"),
			State->SpeedBeforeNos, State->SpeedAfterNos, Car->GetNosFraction(), State->MinDilationSeen, FinalDilation);

		TestTrue(TEXT("NOS activated during run"), State->bSawNosActive);
		TestTrue(TEXT("NOS increased car speed"), State->SpeedAfterNos > State->SpeedBeforeNos + 20.f);
		TestTrue(TEXT("NOS consumed tank"), Car->GetNosFraction() < 1.f);
		TestTrue(TEXT("NOS turbine audio engaged"), State->bSawNosAudio);
		TestTrue(TEXT("exhaust flame light became visible"), State->bSawExhaustLight);
		TestTrue(TEXT("Speedbreaker slowed world time"), State->MinDilationSeen < 0.6f);
		TestFalse(TEXT("Speedbreaker deactivated after release"), Car->IsSpeedbreakerActive());
		TestTrue(TEXT("world time dilation restored to ~1.0"), FinalDilation > 0.95f);

		Move->SetThrottleInput(0.f);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
