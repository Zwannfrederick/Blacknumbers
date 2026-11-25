#include "Vehicle/BNVehiclePawn.h"

#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
	constexpr float BaseSocketZ = 150.f;
	const FVector HoodCameraOffset(65.f, 0.f, 118.f);
	const FVector BumperCameraOffset(232.f, 0.f, 52.f);
	constexpr float HoodCameraPitchDeg = -2.f;
	constexpr float BumperCameraPitchDeg = -1.f;
	constexpr float SpeedShakeMinKmh = 180.f;
	constexpr float SpeedShakeFullKmh = 260.f;
	constexpr float MaxSpeedShakeAmplitude = 0.8f;
}

void ABNVehiclePawn::CycleChaseDistance()
{
	// Modes: 0=Near Chase, 1=Far Chase, 2=Hood (Kaput), 3=Bumper (Tampon)
	const int32 TotalModes = ChaseDistances.Num() + 2;
	ChaseDistanceIndex = (ChaseDistanceIndex + 1) % TotalModes;
}

FString ABNVehiclePawn::GetCameraModeLabel() const
{
	if (IsHoodCamera())
	{
		return TEXT("KAPUT KAMERA");
	}
	if (IsBumperCamera())
	{
		return TEXT("TAMPON KAMERA");
	}
	return ChaseDistanceIndex == 0 ? TEXT("YAKIN TAKİP") : TEXT("UZAK TAKİP");
}

void ABNVehiclePawn::EnsureShowcaseLights()
{
	if (ShowcaseLights.Num() > 0)
	{
		return;
	}
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body)
	{
		return;
	}
	struct FStudioSpec
	{
		const TCHAR* Name;
		FVector Offset;
		FLinearColor Color;
		float Candelas;
		float Radius;
	};
	static const FStudioSpec Specs[] = {
		{ TEXT("ShowcaseKeyLight"),  FVector(340.f,  260.f, 220.f), FLinearColor(1.00f, 0.84f, 0.62f), 580.f, 1150.f },
		{ TEXT("ShowcaseRimLight"),  FVector(-340.f, -240.f, 195.f), FLinearColor(0.54f, 0.78f, 1.00f), 460.f, 1100.f },
		{ TEXT("ShowcaseFillLight"), FVector(120.f, -290.f, 120.f), FLinearColor(0.95f, 0.92f, 0.88f), 260.f,  980.f },
	};
	for (const FStudioSpec& S : Specs)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this, S.Name);
		Light->SetupAttachment(Body);
		Light->SetRelativeLocation(S.Offset);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(S.Candelas);
		Light->SetLightColor(S.Color);
		Light->SetAttenuationRadius(S.Radius);
		Light->SetSourceRadius(36.f);
		Light->SetCastShadows(false);
		Light->SetVisibility(false);
		if (GetWorld())
		{
			Light->RegisterComponent();
		}
		ShowcaseLights.Add(Light);
	}
}

void ABNVehiclePawn::UpdateShowcaseLights(bool bActive)
{
	if (bActive)
	{
		EnsureShowcaseLights();
	}
	for (UPointLightComponent* Light : ShowcaseLights)
	{
		if (IsValid(Light))
		{
			Light->SetVisibility(bActive);
		}
	}
}

void ABNVehiclePawn::SetShowcaseMode(bool bActive, float LateralOffsetCm)
{
	if (bActive && !bShowcaseMode)
	{
		ShowcaseYawDeg = 148.f;
	}
	bShowcaseMode = bActive;
	ShowcaseLateralOffsetCm = LateralOffsetCm;
	UpdateShowcaseLights(bActive);
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
	const bool bBumper = IsBumperCamera();
	const bool bFirstPerson = bHood || bBumper;
	const bool bLookingBack = bLookBackWanted;

	CameraBoom->bEnableCameraRotationLag = !bFirstPerson && !bLookingBack;
	CameraBoom->bInheritPitch = bFirstPerson && !bLookingBack;

	const FVector TargetBoomLoc = bLookingBack
		? FVector::ZeroVector
		: (bBumper ? BumperCameraOffset : (bHood ? HoodCameraOffset : FVector::ZeroVector));
	CameraBoom->SetRelativeLocation(FMath::VInterpTo(CameraBoom->GetRelativeLocation(), TargetBoomLoc, RealDt, 18.f));

	const FRotator CurRot = CameraBoom->GetRelativeRotation();
	if (bLookingBack)
	{
		CameraBoom->SetRelativeRotation(FMath::RInterpTo(CurRot, FRotator(-5.f, 180.f, 0.f), RealDt, 24.f));
		CurrentCameraRollDeg = FMath::FInterpTo(CurrentCameraRollDeg, 0.f, RealDt, 14.f);
		CurrentCameraLateralCm = FMath::FInterpTo(CurrentCameraLateralCm, 0.f, RealDt, 14.f);
	}
	else if (bFirstPerson)
	{
		const float TargetPitch = bBumper ? BumperCameraPitchDeg : HoodCameraPitchDeg;
		const float TargetRoll = FMath::Clamp(GetSlipAngleDegrees() * -0.045f, -1.6f, 1.6f);
		CurrentCameraRollDeg = FMath::FInterpTo(CurrentCameraRollDeg, TargetRoll, RealDt, 9.f);
		CurrentCameraLateralCm = FMath::FInterpTo(CurrentCameraLateralCm, 0.f, RealDt, 14.f);
		CameraBoom->SetRelativeRotation(FMath::RInterpTo(CurRot, FRotator(TargetPitch, 0.f, CurrentCameraRollDeg), RealDt, 16.f));
	}
	else
	{
		const float SlipDeg = GetSlipAngleDegrees();
		const float TargetYaw = bDrifting ? SlipDeg * DriftCameraBlend : 0.f;
		const float TargetRoll = FMath::Clamp(SlipDeg * -0.07f - PlayerSteeringSmoothed * FMath::Clamp(FMath::Abs(SpeedKmh) / 140.f, 0.f, 1.f) * 1.4f, -2.2f, 2.2f);
		const float TargetLateral = FMath::Clamp(SlipDeg * 1.35f + PlayerSteeringSmoothed * 22.f, -46.f, 46.f);
		CurrentCameraRollDeg = FMath::FInterpTo(CurrentCameraRollDeg, TargetRoll, RealDt, 7.f);
		CurrentCameraLateralCm = FMath::FInterpTo(CurrentCameraLateralCm, TargetLateral, RealDt, 6.f);
		const float CameraYaw = FMath::FInterpTo(CurRot.Yaw, TargetYaw, DeltaSeconds, DriftCameraInterpSpeed);
		const float CameraPitch = FMath::FInterpTo(CurRot.Pitch, 0.f, RealDt, 12.f);
		CameraBoom->SetRelativeRotation(FRotator(CameraPitch, CameraYaw, CurrentCameraRollDeg));
	}

	const float SpeedFov = FMath::GetMappedRangeValueClamped(
		FVector2f(0.f, FovMaxSpeedKmh), FVector2f(BaseFov, MaxFov), FMath::Abs(SpeedKmh));
	const float NosFovScale = bFirstPerson ? 0.65f : 1.f;
	const float SlipstreamFov = BoostState.SlipstreamCharge * 3.8f * NosFovScale;
	const float DriftBoostFov = (DriftExitBoostRemaining > 0.f ? 3.5f : 0.f) * NosFovScale;
	const float ShiftFovKick = GearShiftKick * (bFirstPerson ? 1.6f : 3.2f);
	const float ImpactFovKick = -CameraImpactShake * 2.8f;
	const float BumperBaseBonus = bBumper && !bLookingBack ? 6.f : 0.f;
	Camera->SetFieldOfView(SpeedFov + BumperBaseBonus + SlipstreamFov + BoostState.NosBlend * (Boost.NosFovBonus + 5.f) * NosFovScale
		+ DriftBoostFov + ShiftFovKick + ImpactFovKick - BoostState.SpeedbreakerBlend * 6.f);

	// Blend dynamic NFS camera post-process for NOS (tunnel motion blur + chromatic fringe + vignette) and Speedbreaker.
	const float NosP = FMath::Max(BoostState.NosBlend, BoostState.SlipstreamCharge * 0.35f);
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
	const float TargetArm = bLookingBack
		? 560.f
		: (bFirstPerson ? 0.f : (BaseDistance + BoostState.NosBlend * (Boost.NosArmPullback + 34.f) + GearShiftKick * 44.f));
	const float NewArm = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, RealDt, (bFirstPerson || bLookingBack) ? 20.f : 11.f);
	CameraBoom->TargetArmLength = (bFirstPerson && !bLookingBack && NewArm < 2.f) ? 0.f : NewArm;

	const float ShakeScale = (BoostState.bSpeedbreakerActive ? 0.f : (bFirstPerson ? 0.42f : 1.f))
		* (FMath::GetMappedRangeValueClamped(FVector2f(SpeedShakeMinKmh, SpeedShakeFullKmh), FVector2f(0.f, MaxSpeedShakeAmplitude), FMath::Abs(SpeedKmh))
			+ BoostState.NosBlend * 1.65f + BoostState.SlipstreamCharge * 0.55f + GearShiftKick * 1.85f);
	const float ImpactAmp = CameraImpactShake * (bFirstPerson ? 4.5f : 11.5f);
	const float ShakeX = ImpactAmp * 0.6f * FMath::Sin(RealTimeSeconds * 79.f) - GearShiftKick * 5.f;
	const float ShakeY = CurrentCameraLateralCm + ShakeScale * FMath::Sin(RealTimeSeconds * 43.f) + ImpactAmp * FMath::Sin(RealTimeSeconds * 67.f + 0.7f);
	const float ShakeZ = ShakeScale * 0.7f * FMath::Sin(RealTimeSeconds * 59.f + 1.1f) + ImpactAmp * 0.8f * FMath::Cos(RealTimeSeconds * 89.f);
	CurrentSocketBaseZ = FMath::FInterpTo(CurrentSocketBaseZ, (bFirstPerson && !bLookingBack) ? 0.f : BaseSocketZ, RealDt, 16.f);
	CameraBoom->SocketOffset = FVector(ShakeX, ShakeY, CurrentSocketBaseZ + ShakeZ);
}
