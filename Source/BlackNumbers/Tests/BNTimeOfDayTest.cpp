#include "Misc/AutomationTest.h"
#include "World/BNTimeOfDay.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNTimeOfDayTest, "BlackNumbers.World.TimeOfDay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNTimeOfDayTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("wrap above 24"), BNTimeOfDay::WrapHour(25.f), 1.f);
	TestEqual(TEXT("wrap negative"), BNTimeOfDay::WrapHour(-1.f), 23.f);

	TestEqual(TEXT("noon elevation"), BNTimeOfDay::SunElevationDegrees(12.f), 90.f);
	TestEqual(TEXT("sunrise elevation"), BNTimeOfDay::SunElevationDegrees(6.f), 0.f);
	TestEqual(TEXT("midnight elevation"), BNTimeOfDay::SunElevationDegrees(0.f), -90.f);

	TestTrue(TEXT("midnight is night"), BNTimeOfDay::IsNight(0.f));
	TestFalse(TEXT("noon is day"), BNTimeOfDay::IsNight(12.f));
	TestFalse(TEXT("sunrise is twilight, not night"), BNTimeOfDay::IsNight(6.f));
	TestTrue(TEXT("5am is night"), BNTimeOfDay::IsNight(5.f));

	TestEqual(TEXT("fade below horizon"), BNTimeOfDay::HorizonFade(-10.f), 0.f);
	TestEqual(TEXT("fade well above horizon"), BNTimeOfDay::HorizonFade(10.f), 1.f);

	FBNTimeOfDayKey A;
	A.Hour = 20.f;
	A.SunIntensity = 0.f;
	FBNTimeOfDayKey B;
	B.Hour = 0.f;
	B.SunIntensity = 10.f;
	const TArray<FBNTimeOfDayKey> Keys = { B, A }; // sorted: 0h, 20h

	TestEqual(TEXT("interpolates across midnight"), BNTimeOfDay::Evaluate(Keys, 22.f).SunIntensity, 5.f);
	TestEqual(TEXT("exact key"), BNTimeOfDay::Evaluate(Keys, 20.f).SunIntensity, 0.f);
	TestEqual(TEXT("between keys"), BNTimeOfDay::Evaluate(Keys, 10.f).SunIntensity, 5.f);
	TestEqual(TEXT("empty keys give default"), BNTimeOfDay::Evaluate(TConstArrayView<FBNTimeOfDayKey>(), 3.f).SunIntensity, FBNTimeOfDayKey().SunIntensity);

	return true;
}

#endif
