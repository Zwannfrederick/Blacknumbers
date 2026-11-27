#include "Misc/AutomationTest.h"
#include "UI/BNUITheme.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBNTurkishUppercaseTest, "BlackNumbers.UI.TurkishUppercase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBNTurkishUppercaseTest::RunTest(const FString& Parameters)
{
	// FString::ToUpper is ASCII-only; every UPPERCASE label in the UI goes through UpperTr instead.
	TestEqual(TEXT("dotted i becomes İ"), BNUITheme::UpperTr(TEXT("Merkez Bulvar Pisti")), FString(TEXT("MERKEZ BULVAR PİSTİ")));
	TestEqual(TEXT("dotless ı becomes I"), BNUITheme::UpperTr(TEXT("Kısa Devre")), FString(TEXT("KISA DEVRE")));
	TestEqual(TEXT("ö ü ş ç ğ are uppercased"), BNUITheme::UpperTr(TEXT("Eski Liman Köprüsü Şoför Çağrı")),
		FString(TEXT("ESKİ LİMAN KÖPRÜSÜ ŞOFÖR ÇAĞRI")));
	TestEqual(TEXT("already uppercase text is unchanged"), BNUITheme::UpperTr(TEXT("HEAT 5 — İ #12")), FString(TEXT("HEAT 5 — İ #12")));
	return true;
}

#endif
