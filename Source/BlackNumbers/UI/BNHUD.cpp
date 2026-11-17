#include "UI/BNHUD.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr float Margin = 40.f;
	constexpr float BarWidth = 260.f;
	constexpr float BarHeight = 10.f;
	constexpr float SpeedScale = 2.5f;
	constexpr float LabelScale = 1.2f;
	/** Rev bar turns red above this fraction of max rpm. */
	constexpr float RedlineFraction = 0.85f;
	const FLinearColor DashColour(1.f, 0.82f, 0.45f);
	const FLinearColor RedlineColour(1.f, 0.2f, 0.1f);
	const FLinearColor BarBackground(0.f, 0.f, 0.f, 0.5f);
}

FString ABNHUD::GearLabel(int32 Gear)
{
	return Gear < 0 ? TEXT("R") : Gear == 0 ? TEXT("N") : FString::FromInt(Gear);
}

FBNDashReadout ABNHUD::MakeReadout(const ABNVehiclePawn& Car)
{
	UChaosWheeledVehicleMovementComponent* Move = Car.GetChaosMovement();
	FBNDashReadout Out;
	Out.SpeedKmh = FMath::RoundToInt(FMath::Abs(Car.GetSpeedKmh()));
	Out.Gear = GearLabel(Move->GetCurrentGear());
	Out.Rpm = FMath::RoundToInt(Move->GetEngineRotationSpeed());
	const float MaxRpm = Move->GetEngineMaxRotationSpeed();
	Out.RpmFraction = MaxRpm > 0.f ? FMath::Clamp(Out.Rpm / MaxRpm, 0.f, 1.f) : 0.f;
	return Out;
}

void ABNHUD::DrawHUD()
{
	Super::DrawHUD();

	const ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetOwningPawn());
	if (!Car || !Canvas)
	{
		return;
	}
	const FBNDashReadout Dash = MakeReadout(*Car);
	UFont* Font = GEngine->GetLargeFont();

	const float Right = Canvas->ClipX - Margin;
	const float Bottom = Canvas->ClipY - Margin;

	const FString Speed = FString::Printf(TEXT("%d"), Dash.SpeedKmh);
	float SpeedW = 0.f, SpeedH = 0.f;
	GetTextSize(Speed, SpeedW, SpeedH, Font, SpeedScale);
	DrawText(Speed, DashColour, Right - BarWidth, Bottom - BarHeight - SpeedH - 8.f, Font, SpeedScale);
	DrawText(TEXT("km/h"), DashColour, Right - BarWidth + SpeedW + 8.f, Bottom - BarHeight - 8.f - SpeedH * 0.45f, Font, LabelScale);

	float GearW = 0.f, GearH = 0.f;
	GetTextSize(Dash.Gear, GearW, GearH, Font, SpeedScale);
	DrawText(Dash.Gear, DashColour, Right - GearW, Bottom - BarHeight - GearH - 8.f, Font, SpeedScale);

	DrawRect(BarBackground, Right - BarWidth, Bottom - BarHeight, BarWidth, BarHeight);
	DrawRect(Dash.RpmFraction > RedlineFraction ? RedlineColour : DashColour,
		Right - BarWidth, Bottom - BarHeight, BarWidth * Dash.RpmFraction, BarHeight);
}
