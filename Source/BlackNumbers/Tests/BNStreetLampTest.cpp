#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "Tests/BNPlayTestUtils.h"
#include "World/BNDayNightCycle.h"
#include "World/BNStreetLamp.h"

using namespace BNPlayTest;

namespace
{
	constexpr float NightHour = 22.f;
	constexpr float NoonHour = 12.f;
	constexpr int32 MinLampsOnTrack = 20;
	/** Longest switch delay + warm-up, plus slack. */
	constexpr float WarmedUpAfterSeconds = 8.f;

	ABNDayNightCycle* FindCycle(UWorld& World)
	{
		TActorIterator<ABNDayNightCycle> It(&World);
		return It ? *It : nullptr;
	}

	/** Number of lamps whose lit state differs from bExpectLit; -1 if there are too few lamps. */
	int32 CountWrongLamps(UWorld& World, bool bExpectLit)
	{
		int32 Lamps = 0;
		int32 Wrong = 0;
		for (TActorIterator<ABNStreetLamp> It(&World); It; ++It, ++Lamps)
		{
			Wrong += It->IsLit() != bExpectLit;
		}
		return Lamps < MinLampsOnTrack ? -1 : Wrong;
	}
}

// Starts the level at night (regression: lamps and car lights must be on from the first frame, not after
// the first day/night change), then flips to noon and back to night.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNStreetLampTest, "BlackNumbers.World.LightsFollowNight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNStreetLampTest::RunTest(const FString& Parameters)
{
	const TSharedRef<float> SavedHour = MakeShared<float>(-1.f);
	StartTestTrack(*this, [SavedHour](UWorld& EditorWorld)
	{
		if (ABNDayNightCycle* Cycle = FindCycle(EditorWorld))
		{
			*SavedHour = Cycle->TimeOfDay;
			Cycle->TimeOfDay = NightHour;
		}
	});

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]()
	{
		UWorld* World = PlayWorld();
		ABNDayNightCycle* Cycle = World ? FindCycle(*World) : nullptr;
		if (!Cycle)
		{
			AddError(TEXT("Test track has no day/night cycle"));
			return true;
		}
		// Equal priorities make the renderer pick the brighter of sun and moon as its main light, which flips at
		// dusk (a visible pop) and puts a "multiple directional lights are competing" warning on screen.
		const UDirectionalLightComponent* SunLight = Cycle->Sun ? Cast<UDirectionalLightComponent>(Cycle->Sun->GetLightComponent()) : nullptr;
		const UDirectionalLightComponent* MoonLight = Cycle->Moon ? Cast<UDirectionalLightComponent>(Cycle->Moon->GetLightComponent()) : nullptr;
		TestTrue(TEXT("sun outranks moon as the main directional light"),
			SunLight && MoonLight && SunLight->ForwardShadingPriority > MoonLight->ForwardShadingPriority);

		const int32 WrongAtStart = CountWrongLamps(*World, true);
		TestTrue(TEXT("test track has street lamps"), WrongAtStart >= 0);
		TestEqual(TEXT("lamps lit on the first frame of a night start"), WrongAtStart, 0);
		const ABNVehiclePawn* Car = FindPlayerCar();
		TestTrue(TEXT("car lights on at a night start"), Car && Car->AreLightsOn());

		Cycle->SetTimeOfDay(NoonHour);
		TestEqual(TEXT("lamps off at noon"), CountWrongLamps(*World, false), 0);
		TestTrue(TEXT("car lights off at noon"), Car && !Car->AreLightsOn());
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(WarmedUpAfterSeconds));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]()
	{
		UWorld* World = PlayWorld();
		ABNDayNightCycle* Cycle = World ? FindCycle(*World) : nullptr;
		const ABNVehiclePawn* Car = FindPlayerCar();
		if (!Cycle)
		{
			AddError(TEXT("Day/night cycle gone"));
			return true;
		}
		int32 StillGlowing = 0;
		for (TActorIterator<ABNStreetLamp> It(World); It; ++It)
		{
			StillGlowing += It->GetBrightness() > 0.f;
		}
		TestEqual(TEXT("lamps fully dark a few seconds after noon"), StillGlowing, 0);

		Cycle->SetTimeOfDay(NightHour);
		TestEqual(TEXT("lamps back on at night"), CountWrongLamps(*World, true), 0);
		TestTrue(TEXT("car lights back on at night"), Car && Car->AreLightsOn());
		bool bAnyAtFull = false;
		for (TActorIterator<ABNStreetLamp> It(World); It; ++It)
		{
			bAnyAtFull |= It->GetBrightness() >= 1.f;
		}
		TestFalse(TEXT("lamps warm up instead of popping on"), bAnyAtFull);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(WarmedUpAfterSeconds));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]()
	{
		UWorld* World = PlayWorld();
		int32 Dim = 0;
		for (TActorIterator<ABNStreetLamp> It(World); It; ++It)
		{
			Dim += It->GetBrightness() < 1.f;
		}
		TestEqual(TEXT("every lamp fully warmed up a few seconds later"), Dim, 0);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([SavedHour]()
	{
		// Leave the editor copy of the map as it was loaded.
		UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
		if (ABNDayNightCycle* Cycle = EditorWorld ? FindCycle(*EditorWorld) : nullptr; Cycle && *SavedHour >= 0.f)
		{
			Cycle->TimeOfDay = *SavedHour;
		}
		return true;
	}));
	return true;
}

#endif
