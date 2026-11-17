#include "Misc/AutomationTest.h"
#include "Vehicle/BNEngineAudioComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNEngineSoundMappingTest, "BlackNumbers.Vehicle.EngineSoundMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNEngineSoundMappingTest::RunTest(const FString& Parameters)
{
	constexpr float MaxRpm = 7000.f;
	const FBNEngineSoundTargets Idle = BNEngineSound::Evaluate(900.f, MaxRpm, 0.f, 0.f, 0.f);
	const FBNEngineSoundTargets Revving = BNEngineSound::Evaluate(6000.f, MaxRpm, 1.f, 0.f, 80.f);
	const FBNEngineSoundTargets Coasting = BNEngineSound::Evaluate(6000.f, MaxRpm, 0.f, 0.f, 80.f);
	const FBNEngineSoundTargets Drifting = BNEngineSound::Evaluate(5000.f, MaxRpm, 0.6f, 25.f, 60.f);
	const FBNEngineSoundTargets SpinningStill = BNEngineSound::Evaluate(3000.f, MaxRpm, 1.f, 25.f, 0.f);
	const FBNEngineSoundTargets Crawling = BNEngineSound::Evaluate(1500.f, MaxRpm, 0.2f, 25.f, 8.f);
	const FBNEngineSoundTargets Airborne = BNEngineSound::Evaluate(5000.f, MaxRpm, 0.6f, 25.f, 60.f, false);
	const FBNEngineSoundTargets SmallSlip = BNEngineSound::Evaluate(4000.f, MaxRpm, 0.5f, 8.f, 60.f);

	TestEqual(TEXT("six-cylinder idle fires at rpm / 20"), Idle.FiringHz, 45.f);
	TestTrue(TEXT("pitch rises with rpm"), Revving.FiringHz > Idle.FiringHz);
	TestTrue(TEXT("throttle (load) is louder than coasting at the same rpm"), Revving.EngineGain > Coasting.EngineGain);
	TestTrue(TEXT("idle is audible"), Idle.EngineGain > 0.f);
	TestEqual(TEXT("no tyre noise when gripping"), Revving.SkidGain, 0.f);
	TestTrue(TEXT("tyres scrub in a drift"), Drifting.SkidGain > 0.5f);
	TestEqual(TEXT("no skid sound when not moving"), SpinningStill.SkidGain, 0.f);
	TestEqual(TEXT("no skid sound when crawling (slip angle is meaningless at walking pace)"), Crawling.SkidGain, 0.f);
	TestEqual(TEXT("no skid sound in the air"), Airborne.SkidGain, 0.f);
	TestEqual(TEXT("no skid sound for ordinary cornering slip"), SmallSlip.SkidGain, 0.f);
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Tests/BNPlayTestUtils.h"
#include "UI/BNHUD.h"

using namespace BNPlayTest;

namespace
{
	constexpr float DashDriveSeconds = 3.f;
	constexpr float LiftSeconds = 0.5f;
	constexpr float MaxSpeedErrorKmh = 1.5f;
	constexpr float FrequencyTolerance = 0.05f;

	void CheckDashAndAudio(FAutomationTestBase& Test, const ABNVehiclePawn& Car, const TCHAR* When)
	{
		UChaosWheeledVehicleMovementComponent* Move = Car.GetChaosMovement();
		const FBNDashReadout Dash = ABNHUD::MakeReadout(Car);
		const float PhysicsKmh = Car.GetVelocity().Size() * 0.036f;
		const float Rpm = Move->GetEngineRotationSpeed();
		const FBNEngineSoundTargets& Sound = Car.EngineAudio->GetTargets();
		UE_LOG(LogTemp, Display, TEXT("BN dash test (%s): dash %d km/h gear %s %d rpm | physics %.1f km/h gear %d %.0f rpm | sound %.1f Hz gain %.2f"),
			When, Dash.SpeedKmh, *Dash.Gear, Dash.Rpm, PhysicsKmh, Move->GetCurrentGear(), Rpm, Sound.FiringHz, Sound.EngineGain);

		Test.TestTrue(FString::Printf(TEXT("speedometer matches body speed (%s)"), When), FMath::Abs(Dash.SpeedKmh - PhysicsKmh) <= MaxSpeedErrorKmh);
		Test.TestEqual(FString::Printf(TEXT("gear shown is the gearbox gear (%s)"), When), Dash.Gear, ABNHUD::GearLabel(Move->GetCurrentGear()));
		Test.TestEqual(FString::Printf(TEXT("rpm shown is engine rpm (%s)"), When), Dash.Rpm, FMath::RoundToInt(Rpm));
		Test.TestTrue(FString::Printf(TEXT("engine pitch follows rpm (%s)"), When),
			FMath::IsNearlyEqual(Sound.FiringHz, Rpm / 60.f * BNEngineSound::FiringsPerRevolution, Rpm / 20.f * FrequencyTolerance + 1.f));
	}
}

// Drives the runway and checks the HUD and engine sound read the car's real physics state.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNDashAudioTest, "BlackNumbers.Vehicle.DashAndAudioFollowCar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNDashAudioTest::RunTest(const FString& Parameters)
{
	StartTestTrack(*this);

	struct FState { double Start = -1.0; float GainOnThrottle = 0.f; bool bLifted = false; };
	const TSharedRef<FState> State = MakeShared<FState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		ABNVehiclePawn* Car = FindPlayerCar();
		if (!Car || !Car->EngineAudio)
		{
			AddError(TEXT("Player car or its engine audio is missing"));
			return true;
		}
		const double Now = Car->GetWorld()->GetTimeSeconds();
		if (State->Start < 0.0)
		{
			State->Start = Now;
			Car->GetChaosMovement()->SetThrottleInput(1.f);
			return false;
		}
		if (!State->bLifted)
		{
			if (Now - State->Start < DashDriveSeconds)
			{
				return false;
			}
			CheckDashAndAudio(*this, *Car, TEXT("full throttle"));
			State->GainOnThrottle = Car->EngineAudio->GetTargets().EngineGain;
			Car->GetChaosMovement()->SetThrottleInput(0.f);
			State->bLifted = true;
			State->Start = Now;
			return false;
		}
		if (Now - State->Start < LiftSeconds)
		{
			return false;
		}
		CheckDashAndAudio(*this, *Car, TEXT("lifted off"));
		TestTrue(TEXT("engine gets quieter when lifting off"), Car->EngineAudio->GetTargets().EngineGain < State->GainOnThrottle);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
