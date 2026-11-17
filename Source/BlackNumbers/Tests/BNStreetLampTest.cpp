#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

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

// Starts the level at night (regression: lamps must be lit on the first frame, not after the first
// day/night change), then flips to noon and back to night.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNStreetLampTest, "BlackNumbers.World.StreetLampsFollowNight",
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
		const int32 WrongAtStart = CountWrongLamps(*World, true);
		TestTrue(TEXT("test track has street lamps"), WrongAtStart >= 0);
		TestEqual(TEXT("lamps lit on the first frame of a night start"), WrongAtStart, 0);

		Cycle->SetTimeOfDay(NoonHour);
		TestEqual(TEXT("lamps off at noon"), CountWrongLamps(*World, false), 0);

		Cycle->SetTimeOfDay(NightHour);
		TestEqual(TEXT("lamps back on at night"), CountWrongLamps(*World, true), 0);
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
