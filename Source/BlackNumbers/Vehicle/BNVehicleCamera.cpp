#include "Vehicle/BNVehiclePawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
	constexpr float BaseSocketZ = 150.f;
	const FVector HoodCameraOffset(65.f, 0.f, 118.f);
	constexpr float HoodCameraPitchDeg = -2.f;
	constexpr float SpeedShakeMinKmh = 180.f;
	constexpr float SpeedShakeFullKmh = 260.f;
	constexpr float MaxSpeedShakeAmplitude = 0.8f;
}

void ABNVehiclePawn::CycleChaseDistance()
{
	const int32 TotalModes = ChaseDistances.Num() + 1;
	ChaseDistanceIndex = (ChaseDistanceIndex + 1) % TotalModes;
}

FString ABNVehiclePawn::GetCameraModeLabel() const
{
	if (IsHoodCamera())
	{
		return TEXT("KAPUT KAMERA");
	}
	return ChaseDistanceIndex == 0 ? TEXT("YAKIN TAKİP") : TEXT("UZAK TAKİP");
}

void ABNVehiclePawn::SetShowcaseMode(bool bActive, float LateralOffsetCm)
{
	if (bActive && !bShowcaseMode)
	{
		ShowcaseYawDeg = 148.f;
	}
	bShowcaseMode = bActive;
	ShowcaseLateralOffsetCm = LateralOffsetCm;
	if (!bActive && CameraBoom)
	{
		CameraBoom->SetRelativeRotation(FRotator::ZeroRotator);
	}
}

void ABNVehiclePawn::UpdateShowcaseCamera(float RealDt)
{
	if (!CameraBoom || !Camera)
	{
		return;
	}
	ShowcaseYawDeg = FMath::Fmod(ShowcaseYawDeg + RealDt * 12.5f, 360.f);
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->SetRelativeLocation(FMath::VInterpTo(CameraBoom->GetRelativeLocation(), FVector(0.f, 0.f, 18.f), RealDt, 10.f));
	CameraBoom->SetRelativeRotation(FMath::RInterpTo(CameraBoom->GetRelativeRotation(), FRotator(-9.f, ShowcaseYawDeg, 0.f), RealDt, 8.f));
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, 510.f, RealDt, 8.f);
	CurrentSocketBaseZ = FMath::FInterpTo(CurrentSocketBaseZ, 68.f, RealDt, 8.f);
	const FVector TargetSocket(0.f, ShowcaseLateralOffsetCm, CurrentSocketBaseZ);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, TargetSocket, RealDt, 8.f);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, 64.f, RealDt, 8.f));
	Camera->PostProcessBlendWeight = 0.f;
}

void ABNVehiclePawn::UpdateCamera(float DeltaSeconds, float RealDt, float SpeedKmh, float RealTimeSeconds)
{
	if (bShowcaseMode)
	{
		UpdateShowcaseCamera(RealDt);
		return;
	}
	const bool bHood = IsHoodCamera();
	CameraBoom->bEnableCameraRotationLag = !bHood;
	CameraBoom->bInheritPitch = bHood;

	const FVector TargetBoomLoc = bHood ? HoodCameraOffset : FVector::ZeroVector;
	CameraBoom->SetRelativeLocation(FMath::VInterpTo(CameraBoom->GetRelativeLocation(), TargetBoomLoc, RealDt, 16.f));

	const FRotator CurRot = CameraBoom->GetRelativeRotation();
	if (bHood)
	{
		CameraBoom->SetRelativeRotation(FMath::RInterpTo(CurRot, FRotator(HoodCameraPitchDeg, 0.f, 0.f), RealDt, 16.f));
	}
	else
	{
		const float TargetYaw = bDrifting ? GetSlipAngleDegrees() * DriftCameraBlend : 0.f;
		const float CameraYaw = FMath::FInterpTo(CurRot.Yaw, TargetYaw, DeltaSeconds, DriftCameraInterpSpeed);
		const float CameraPitch = FMath::FInterpTo(CurRot.Pitch, 0.f, RealDt, 12.f);
		CameraBoom->SetRelativeRotation(FRotator(CameraPitch, CameraYaw, 0.f));
	}

	const float SpeedFov = FMath::GetMappedRangeValueClamped(
		FVector2f(0.f, FovMaxSpeedKmh), FVector2f(BaseFov, MaxFov), FMath::Abs(SpeedKmh));
	const float NosFovScale = bHood ? 0.62f : 1.f;
	const float DriftBoostFov = (DriftExitBoostRemaining > 0.f ? 3.5f : 0.f) * NosFovScale;
	const float ShiftFovKick = GearShiftKick * (bHood ? 1.6f : 3.2f);
	const float ImpactFovKick = -CameraImpactShake * 2.8f;
	Camera->SetFieldOfView(SpeedFov + BoostState.NosBlend * (Boost.NosFovBonus + 5.f) * NosFovScale + DriftBoostFov
		+ ShiftFovKick + ImpactFovKick - BoostState.SpeedbreakerBlend * 6.f);

	// Blend dynamic NFS camera post-process for NOS (tunnel motion blur + chromatic fringe + vignette) and Speedbreaker.
	const float NosP = BoostState.NosBlend;
	const float SpdP = BoostState.SpeedbreakerBlend;
	const float CombinedPost = FMath::Clamp(FMath::Max(NosP, SpdP), 0.f, 1.f);
	Camera->PostProcessBlendWeight = CombinedPost;
	if (CombinedPost > 0.01f)
	{
		FPostProcessSettings& PP = Camera->PostProcessSettings;
		PP.bOverride_MotionBlurAmount = true;
		PP.MotionBlurAmount = FMath::Lerp(0.10f, 0.42f, NosP) * (1.f - 0.55f * SpdP);
		PP.bOverride_SceneFringeIntensity = true;
		PP.SceneFringeIntensity = NosP * 1.05f + SpdP * 0.45f + GearShiftKick * 0.55f;
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = FMath::Lerp(0.28f, 0.56f, FMath::Max(NosP, SpdP));
		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FMath::Lerp(
			FVector4(1.0f + NosP * 0.16f, 1.0f + NosP * 0.12f, 1.0f + NosP * 0.22f, 1.f),
			FVector4(0.62f, 0.72f, 0.88f, 1.f), SpdP);
	}

	const float BaseDistance = ChaseDistances.IsValidIndex(ChaseDistanceIndex) ? ChaseDistances[ChaseDistanceIndex] : 650.f;
	const float TargetArm = bHood ? 0.f : (BaseDistance + BoostState.NosBlend * (Boost.NosArmPullback + 34.f) + GearShiftKick * 44.f);
	const float NewArm = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, RealDt, bHood ? 16.f : 11.f);
	CameraBoom->TargetArmLength = (bHood && NewArm < 2.f) ? 0.f : NewArm;

	const float ShakeScale = (BoostState.bSpeedbreakerActive ? 0.f : (bHood ? 0.38f : 1.f))
		* (FMath::GetMappedRangeValueClamped(FVector2f(SpeedShakeMinKmh, SpeedShakeFullKmh), FVector2f(0.f, MaxSpeedShakeAmplitude), FMath::Abs(SpeedKmh))
			+ BoostState.NosBlend * 1.65f + GearShiftKick * 1.85f);
	const float ImpactAmp = CameraImpactShake * (bHood ? 4.5f : 11.5f);
	const float ShakeX = ImpactAmp * 0.6f * FMath::Sin(RealTimeSeconds * 79.f) - GearShiftKick * 5.f;
	const float ShakeY = ShakeScale * FMath::Sin(RealTimeSeconds * 43.f) + ImpactAmp * FMath::Sin(RealTimeSeconds * 67.f + 0.7f);
	const float ShakeZ = ShakeScale * 0.7f * FMath::Sin(RealTimeSeconds * 59.f + 1.1f) + ImpactAmp * 0.8f * FMath::Cos(RealTimeSeconds * 89.f);
	CurrentSocketBaseZ = FMath::FInterpTo(CurrentSocketBaseZ, bHood ? 0.f : BaseSocketZ, RealDt, 16.f);
	CameraBoom->SocketOffset = FVector(ShakeX, ShakeY, CurrentSocketBaseZ + ShakeZ);
}
