#include "Police/BNPoliceController.h"

#include "ChaosVehicleWheel.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/WorldSettings.h"
#include "Police/BNPoliceSirenComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr float CmPerSecToKmh = 0.036f;
	constexpr float CmPerSecToMph = 0.022369f;
	constexpr float MinChaseSpeedKmh = 65.f;
	constexpr float SearchCruiseSpeedKmh = 95.f;
	constexpr float FullThrottleError = 300.f;
	constexpr float FullBrakeError = 550.f;
	constexpr float MinBrakingSpeed = 200.f;
	constexpr float GripAssistDegPerSec = 135.f;
	constexpr float MinGripAssistSpeed = 450.f;
	constexpr float StuckSpeed = 220.f;
	constexpr float UpsideDownZ = 0.35f;
	constexpr float StuckSecondsToRecover = 4.f;
	constexpr float MinRecoverDistCm = 2500.f;
	constexpr float YelpDistanceCm = 3000.f;

	constexpr float StrobeCandelas = 260.f;
	constexpr float DisabledStrobeCandelas = 80.f;
	constexpr float StrobeRadius = 900.f;
	constexpr float StrobeRateHz = 7.f;
	const FLinearColor RedStrobeColor(1.f, 0.06f, 0.04f);
	const FLinearColor BlueStrobeColor(0.08f, 0.35f, 1.f);
}

ABNPoliceController::ABNPoliceController()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	CubeMesh = Cube.Object;
}

void ABNPoliceController::SetupPursuitUnit(ABNVehiclePawn* InTarget, const FBNHeatTierSpec& InSpec, int32 InFormationSlot, bool bInRoadblockUnit)
{
	TargetCar = InTarget;
	HeatSpec = InSpec;
	FormationSlot = InFormationSlot;
	bRoadblockUnit = bInRoadblockUnit;
	bDisabled = false;
	bSearchMode = false;
	DisabledSeconds = 0.f;
	StuckSeconds = 0.f;

	if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetPawn()))
	{
		AttachPoliceGear(*Car);
		if (bRoadblockUnit)
		{
			Car->SetInputLocked(true);
		}
	}
	UpdateLightsAndSiren();
}

void ABNPoliceController::UpdateHeatSpec(const FBNHeatTierSpec& InSpec)
{
	HeatSpec = InSpec;
}

void ABNPoliceController::SetSearchMode(bool bInSearchMode, const FVector& InSearchPoint)
{
	bSearchMode = bInSearchMode;
	SearchPoint = InSearchPoint;
}

void ABNPoliceController::DisableUnit()
{
	if (bDisabled)
	{
		return;
	}
	bDisabled = true;
	DisabledSeconds = 0.f;
	if (ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetPawn()))
	{
		Car->SetInputLocked(true);
	}
	UpdateLightsAndSiren();
}

bool ABNPoliceController::IsSirenActive() const
{
	return Siren && Siren->IsSirenActive();
}

void ABNPoliceController::AttachPoliceGear(ABNVehiclePawn& Car)
{
	if (bGearAttached)
	{
		return;
	}
	USkeletalMeshComponent* Body = Car.GetMesh();
	if (!Body)
	{
		return;
	}
	bGearAttached = true;

	if (CubeMesh)
	{
		LightbarMesh = NewObject<UStaticMeshComponent>(&Car, TEXT("PoliceLightbar"));
		LightbarMesh->SetStaticMesh(CubeMesh);
		LightbarMesh->SetupAttachment(Body);
		LightbarMesh->SetRelativeLocation(FVector(-10.f, 0.f, 148.f));
		LightbarMesh->SetRelativeScale3D(FVector(0.35f, 1.15f, 0.14f));
		LightbarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LightbarMesh->SetCastShadow(false);
		LightbarMesh->RegisterComponent();
	}

	auto MakeStrobe = [&](const TCHAR* Name, float Y, const FLinearColor& Color) -> UPointLightComponent*
	{
		UPointLightComponent* Strobe = NewObject<UPointLightComponent>(&Car, Name);
		Strobe->SetupAttachment(Body);
		Strobe->SetRelativeLocation(FVector(-10.f, Y, 165.f));
		Strobe->SetIntensityUnits(ELightUnits::Candelas);
		Strobe->SetIntensity(StrobeCandelas);
		Strobe->SetLightColor(Color);
		Strobe->SetAttenuationRadius(StrobeRadius);
		Strobe->SetCastShadows(false);
		Strobe->RegisterComponent();
		return Strobe;
	};

	RedLight = MakeStrobe(TEXT("PoliceRedStrobe"), -45.f, RedStrobeColor);
	BlueLight = MakeStrobe(TEXT("PoliceBlueStrobe"), 45.f, BlueStrobeColor);

	Siren = NewObject<UBNPoliceSirenComponent>(&Car, TEXT("PoliceSiren"));
	Siren->SetupAttachment(Body);
	Siren->RegisterComponent();
	Car.ApplyPaintTint(FLinearColor(0.03f, 0.03f, 0.04f, 1.f));
}

void ABNPoliceController::CleanupPoliceGear()
{
	if (LightbarMesh)
	{
		LightbarMesh->DestroyComponent();
		LightbarMesh = nullptr;
	}
	if (RedLight)
	{
		RedLight->DestroyComponent();
		RedLight = nullptr;
	}
	if (BlueLight)
	{
		BlueLight->DestroyComponent();
		BlueLight = nullptr;
	}
	if (Siren)
	{
		Siren->SetSirenState(false, false, 1.f);
		Siren->DestroyComponent();
		Siren = nullptr;
	}
	bGearAttached = false;
}

void ABNPoliceController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupPoliceGear();
	Super::EndPlay(EndPlayReason);
}

void ABNPoliceController::UpdateLightsAndSiren()
{
	const UWorld* World = GetWorld();
	const double Time = World ? World->GetTimeSeconds() : 0.0;
	const bool bRedPhase = FMath::Fmod(Time * StrobeRateHz, 1.0) < 0.5;
	const float Dilation = World && World->GetWorldSettings() ? World->GetWorldSettings()->GetEffectiveTimeDilation() : 1.f;

	if (bDisabled)
	{
		const bool bHazard = FMath::Fmod(Time * 2.0, 1.0) < 0.5;
		if (RedLight)
		{
			RedLight->SetIntensity(DisabledStrobeCandelas);
			RedLight->SetVisibility(bHazard);
		}
		if (BlueLight)
		{
			BlueLight->SetIntensity(DisabledStrobeCandelas);
			BlueLight->SetVisibility(false);
		}
		if (Siren)
		{
			Siren->SetSirenState(false, false, Dilation);
		}
		return;
	}

	if (RedLight)
	{
		RedLight->SetIntensity(StrobeCandelas);
		RedLight->SetVisibility(bRedPhase);
	}
	if (BlueLight)
	{
		BlueLight->SetIntensity(StrobeCandelas);
		BlueLight->SetVisibility(!bRedPhase);
	}

	if (Siren)
	{
		const ABNVehiclePawn* MyCar = Cast<ABNVehiclePawn>(GetPawn());
		const ABNVehiclePawn* Target = TargetCar.Get();
		const float Dist = (MyCar && Target) ? FVector::Dist2D(MyCar->GetActorLocation(), Target->GetActorLocation()) : 99999.f;
		const bool bYelp = Dist < YelpDistanceCm;
		Siren->SetSirenState(!bRoadblockUnit, bYelp, Dilation);
	}
}

void ABNPoliceController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ABNVehiclePawn* Car = Cast<ABNVehiclePawn>(GetPawn());
	if (!Car)
	{
		return;
	}

	if (bDisabled)
	{
		DisabledSeconds += DeltaSeconds;
		UpdateLightsAndSiren();
		return;
	}

	UpdateLightsAndSiren();

	if (bRoadblockUnit || Car->IsInputLocked())
	{
		return;
	}

	DriveChase(*Car, DeltaSeconds);
}

void ABNPoliceController::DriveChase(ABNVehiclePawn& Car, float DeltaSeconds)
{
	const ABNVehiclePawn* Target = TargetCar.Get();
	if (!Target)
	{
		return;
	}

	UChaosWheeledVehicleMovementComponent* Move = Car.GetChaosMovement();
	const FVector MyLoc = Car.GetActorLocation();
	const float Speed = Move->GetForwardSpeed();

	const bool bStuck = FMath::Abs(Speed) < StuckSpeed || Car.GetActorUpVector().Z < UpsideDownZ;
	StuckSeconds = bStuck ? StuckSeconds + DeltaSeconds : 0.f;
	if (StuckSeconds >= StuckSecondsToRecover)
	{
		if (FVector::Dist2D(MyLoc, Target->GetActorLocation()) > MinRecoverDistCm)
		{
			RecoverStuck(Car);
			return;
		}
		StuckSeconds = 0.f;
	}

	FVector AimPoint = SearchPoint;
	float DesiredSpeedKmh = SearchCruiseSpeedKmh;

	if (!bSearchMode)
	{
		const FVector TargetLoc = Target->GetActorLocation();
		const FVector TargetVel = Target->GetVelocity();
		const FVector TargetForward = Target->GetActorForwardVector().GetSafeNormal2D();
		const FVector TargetRight = Target->GetActorRightVector().GetSafeNormal2D();
		const float DistToTarget = FVector::Dist2D(MyLoc, TargetLoc);

		// Slots fan out behind and around the player: tail, left flank, right flank, aggressive box-in.
		float LateralOffset = 0.f;
		float LongitudinalOffset = -200.f;
		if (FormationSlot % 4 == 1)
		{
			LateralOffset = -340.f;
		}
		else if (FormationSlot % 4 == 2)
		{
			LateralOffset = 340.f;
		}
		else if (FormationSlot % 4 == 3 && DistToTarget < 3500.f)
		{
			LongitudinalOffset = 480.f;
			LateralOffset = 280.f;
		}

		const float LeadSeconds = FMath::Clamp(DistToTarget / 3500.f, 0.15f, 0.7f);
		AimPoint = TargetLoc + TargetVel * LeadSeconds + TargetForward * LongitudinalOffset + TargetRight * LateralOffset;

		const float PlayerSpeedKmh = FMath::Abs(Target->GetSpeedKmh());
		const float CatchupBonus = DistToTarget > 2500.f ? 42.f : (DistToTarget > 900.f ? 22.f : 6.f);
		DesiredSpeedKmh = FMath::Clamp(PlayerSpeedKmh + CatchupBonus, MinChaseSpeedKmh, HeatSpec.MaxPursuitSpeedKmh);
		if (DistToTarget < 650.f && HeatSpec.Aggression < 0.6f)
		{
			DesiredSpeedKmh = FMath::Min(DesiredSpeedKmh, PlayerSpeedKmh + 5.f);
		}
	}

	const FVector LocalAim = Car.GetActorTransform().InverseTransformPosition(AimPoint);
	const USkeletalMeshComponent* Mesh = Car.GetMesh();
	const float Wheelbase = FVector::Dist(Mesh->GetSocketLocation(TEXT("Phys_Wheel_FL")), Mesh->GetSocketLocation(TEXT("Phys_Wheel_BL")));
	const float WheelAngle = FMath::RadiansToDegrees(FMath::Atan(Wheelbase * 2.f * LocalAim.Y / FMath::Max(LocalAim.SizeSquared2D(), 1.f)));
	const FRichCurve* SteerCurve = Move->SteeringSetup.SteeringCurve.GetRichCurveConst();
	const float LockScale = SteerCurve && SteerCurve->GetNumKeys() > 0 ? SteerCurve->Eval(FMath::Abs(Speed) * CmPerSecToMph) : 1.f;
	const float MaxWheelAngle = FMath::Max(Move->Wheels[0]->MaxSteerAngle * LockScale, 1.f);

	// Reverse steer correction if target is directly behind while crawling.
	float SteerInput = FMath::Clamp(WheelAngle / MaxWheelAngle, -1.f, 1.f);
	if (LocalAim.X < -200.f && FMath::Abs(Speed) < 600.f)
	{
		SteerInput = LocalAim.Y >= 0.f ? 1.f : -1.f;
	}
	Move->SetSteeringInput(SteerInput);

	const float TargetSpeedCms = DesiredSpeedKmh / CmPerSecToKmh;
	const float Error = TargetSpeedCms - Speed;
	Move->SetThrottleInput(FMath::Clamp(Error / FullThrottleError, 0.f, 1.f));
	Move->SetBrakeInput(Speed > MinBrakingSpeed ? FMath::Clamp(-Error / FullBrakeError, 0.f, 1.f) : 0.f);
	Move->SetHandbrakeInput(false);

	ApplyGripAssist(Car, DeltaSeconds);
}

void ABNPoliceController::ApplyGripAssist(ABNVehiclePawn& Car, float DeltaSeconds) const
{
	USkeletalMeshComponent* Body = Car.GetMesh();
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	if (!Car.IsOnGround() || Velocity.Size2D() < MinGripAssistSpeed)
	{
		return;
	}
	const float Slip = Car.GetSlipAngleDegrees();
	const float MaxTurn = GripAssistDegPerSec * FMath::Lerp(0.85f, 1.15f, HeatSpec.UnitSkill) * DeltaSeconds;
	const float Turn = -FMath::Clamp(Slip, -MaxTurn, MaxTurn);
	const FVector Travel = FRotator(0.f, Turn, 0.f).RotateVector(FVector(Velocity.X, Velocity.Y, 0.f));
	Body->SetPhysicsLinearVelocity(FVector(Travel.X, Travel.Y, Velocity.Z));
}

void ABNPoliceController::RecoverStuck(ABNVehiclePawn& Car)
{
	const ABNVehiclePawn* Target = TargetCar.Get();
	if (!Target)
	{
		StuckSeconds = 0.f;
		return;
	}
	const FVector Forward = Target->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Target->GetActorRightVector().GetSafeNormal2D();
	const float Side = (FormationSlot % 2 == 0 ? 1.f : -1.f) * 350.f;
	const FVector SpawnLoc = Target->GetActorLocation() - Forward * 3400.f + Right * Side + FVector(0.f, 0.f, 60.f);
	const float Yaw = Forward.Rotation().Yaw;
	Car.SetActorLocationAndRotation(SpawnLoc, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	Car.GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Car.GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	StuckSeconds = 0.f;
}
