#include "Vehicle/BNVehiclePawn.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Garage/BNGarageSubsystem.h"
#include "Garage/BNGarageTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Story/BNStorySubsystem.h"
#include "UI/BNHUD.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/BNEngineAudioComponent.h"
#include "Vehicle/BNVehicleEffects.h"
#include "Vehicle/BNVehicleVisuals.h"
#include "Vehicle/BNVehicleWheels.h"
#include "World/BNDayNightCycle.h"

namespace
{
	constexpr float CmPerSecToKmh = 0.036f;
	constexpr float AirborneAngularDamping = 3.f;
	constexpr float MinSpeedForSlipKmh = 5.f;
	constexpr int32 FirstRearWheel = 2; // wheel setups: FL, FR, BL, BR
	/** Steering needed to start a drift with the handbrake, and to count as "into the turn" while holding one. */
	constexpr float DriftStartMinSteer = 0.3f;
	constexpr float DriftHoldMinSteer = 0.1f;
	constexpr float DriftEndSpeedFraction = 0.66f;
	/** Fastest the car is turned by the assist (deg/s), so drifts steer smoothly without snapping. */
	constexpr float MaxAssistYawRate = 125.f;
	constexpr float PlayerGripAssistDegPerSec = 55.f;
	constexpr float MinNormalAssistSpeedKmh = 25.f;
	/** Rest rotation of every Phys_Wheel_* bone in component space (measured; see Vehicle.WheelsAligned). */
	const FRotator WheelBoneRest(0.f, -90.f, 90.f);
	const TCHAR* const WheelBones[] = { TEXT("Phys_Wheel_FL"), TEXT("Phys_Wheel_FR"), TEXT("Phys_Wheel_BL"), TEXT("Phys_Wheel_BR") };

	// Light placement from SM_SportsCar bounds: nose at x ~ +247 cm, tail at x ~ -223 cm. Both sit just
	// outside the body: a light inside or touching the shell floods the car itself and overexposes it.
	constexpr float HeadlightCandelas = 8000.f;
	constexpr float HeadlightRange = 5000.f;
	const FLinearColor HeadlightColour(1.f, 0.95f, 0.85f);
	constexpr float TaillightCandelas = 3.f;
	constexpr float TaillightRange = 300.f;
	const FLinearColor TaillightColour(1.f, 0.05f, 0.02f);

	constexpr float ExhaustFlameRange = 180.f;
	constexpr float NosFlameCandelas = 120.f;
	const FLinearColor NosFlameColour(0.2f, 0.75f, 1.f);

	constexpr float BaseSocketZ = 150.f;
	const FVector HoodCameraOffset(65.f, 0.f, 118.f);
	constexpr float HoodCameraPitchDeg = -2.f;
	constexpr float SpeedShakeMinKmh = 180.f;
	constexpr float SpeedShakeFullKmh = 260.f;
	constexpr float MaxSpeedShakeAmplitude = 0.8f;
	constexpr float NearMissDistanceCm = 480.f;
	constexpr float NearMissMinSpeedKmh = 80.f;
	constexpr double NearMissCooldownSeconds = 2.5;
	constexpr double CrashNearMissSuppressSeconds = 1.5;
	constexpr float CollisionSparkMinImpulse = 80000.f;
	constexpr float UpshiftBackfireMinRpm = 4000.f;
	constexpr float BackfireDurationSeconds = 0.15f;
	constexpr float LaunchBackfireDurationSeconds = 0.22f;

	UStaticMeshComponent* MakeBodyPart(AActor& Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh, FName Bone = NAME_None)
	{
		UStaticMeshComponent* Part = Owner.CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent, Bone);
		Part->SetStaticMesh(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); // the physics asset is the collision
		return Part;
	}

	void SetupSportsCarMovement(UChaosWheeledVehicleMovementComponent& Move)
	{
		Move.ChassisHeight = 144.f;
		Move.DragCoefficient = 0.31f;
		Move.DownforceCoefficient = 0.25f;
		Move.bLegacyWheelFrictionPosition = true;

		Move.WheelSetups.SetNum(UE_ARRAY_COUNT(WheelBones));
		for (int32 i = 0; i < UE_ARRAY_COUNT(WheelBones); ++i)
		{
			Move.WheelSetups[i].BoneName = WheelBones[i];
			Move.WheelSetups[i].WheelClass = i < FirstRearWheel ? UBNWheelFront::StaticClass() : UBNWheelRear::StaticClass();
		}

		// Chaos disables the whole engine simulation when this curve is empty.
		FRichCurve* Torque = Move.EngineSetup.TorqueCurve.GetRichCurve();
		Torque->Reset();
		Torque->AddKey(0.f, 0.6f);
		Torque->AddKey(1500.f, 0.8f);
		Torque->AddKey(3500.f, 0.95f);
		Torque->AddKey(5500.f, 1.f);
		Torque->AddKey(7000.f, 0.8f);
		// Tuned with BlackNumbers.Vehicle.DrivesForward to a mid-2000s sports car: 0-100 km/h in about 5.5 s.
		Move.EngineSetup.MaxTorque = 400.f;
		Move.EngineSetup.MaxRPM = 7000.f;
		Move.EngineSetup.EngineIdleRPM = 900.f;
		Move.EngineSetup.EngineBrakeEffect = 0.2f;
		Move.EngineSetup.EngineRevUpMOI = 5.f;
		Move.EngineSetup.EngineRevDownRate = 600.f;

		Move.TransmissionSetup.bUseAutomaticGears = true;
		Move.TransmissionSetup.bUseAutoReverse = true;
		Move.TransmissionSetup.FinalRatio = 2.81f;
		Move.TransmissionSetup.ChangeUpRPM = 6000.f;
		Move.TransmissionSetup.ChangeDownRPM = 2000.f;
		Move.TransmissionSetup.GearChangeTime = 0.2f;
		Move.TransmissionSetup.TransmissionEfficiency = 0.9f;
		Move.TransmissionSetup.ForwardGearRatios = { 4.25f, 2.52f, 1.66f, 1.22f, 1.f };
		Move.TransmissionSetup.ReverseGearRatios = { 4.04f };

		Move.SteeringSetup.SteeringType = ESteeringType::Ackermann;
		Move.SteeringSetup.AngleRatio = 0.7f;
		FRichCurve* Steering = Move.SteeringSetup.SteeringCurve.GetRichCurve();
		Steering->Reset();
		Steering->AddKey(0.f, 1.f);
		Steering->AddKey(25.f, 0.82f);
		Steering->AddKey(60.f, 0.50f);
		Steering->AddKey(100.f, 0.35f);
		Steering->AddKey(135.f, 0.26f);
	}
}

ABNVehiclePawn::ABNVehiclePawn()
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CarMesh(TEXT("/Game/Vehicles/SportsCar/SKM_SportsCar.SKM_SportsCar"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> CarAnim(TEXT("/Game/Vehicles/SportsCar/ABP_SportsCar"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar.SM_SportsCar"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GlassMesh(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Glass.SM_SportsCar_Glass"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelMesh(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Wheel.SM_SportsCar_Wheel"));

	USkeletalMeshComponent* Body = GetMesh();
	Body->SetSkeletalMesh(CarMesh.Object);
	Body->SetAnimInstanceClass(CarAnim.Class);
	Body->SetSimulatePhysics(true);
	Body->SetCollisionProfileName(TEXT("Vehicle"));

	SetupSportsCarMovement(*GetChaosMovement());

	Chassis = MakeBodyPart(*this, Body, TEXT("Chassis"), BodyMesh.Object);
	ChassisGlass = MakeBodyPart(*this, Body, TEXT("ChassisGlass"), GlassMesh.Object);
	for (int32 i = 0; i < UE_ARRAY_COUNT(WheelBones); ++i)
	{
		UStaticMeshComponent* Wheel = MakeBodyPart(*this, Body, *FString::Printf(TEXT("Wheel_%s"), WheelBones[i] + 11), WheelMesh.Object, WheelBones[i]);
		// Undo Phys_Wheel_* rest rotation so axle (+Y) aligns laterally and left rims face outward (guarded by Vehicle.WheelsAligned).
		const FQuat Outward = (i % 2 == 0) ? FRotator(0.f, 180.f, 0.f).Quaternion() : FQuat::Identity;
		Wheel->SetRelativeRotation(WheelBoneRest.Quaternion().Inverse() * Outward);
		WheelMeshes.Add(Wheel);
	}

	Visuals = CreateDefaultSubobject<UBNVehicleVisualsComponent>(TEXT("Visuals"));
	Visuals->SetupAttachment(Body);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Body);
	CameraBoom->TargetArmLength = ChaseDistances[0];
	CameraBoom->SocketOffset.Z = BaseSocketZ;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 9.f;
	CameraBoom->CameraLagMaxDistance = 50.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);
	Camera->SetFieldOfView(BaseFov);
	Camera->PostProcessBlendWeight = 0.f;
	Camera->PostProcessSettings.bOverride_ColorSaturation = true;
	Camera->PostProcessSettings.ColorSaturation = FVector4(0.45f, 0.45f, 0.45f, 1.f);
	Camera->PostProcessSettings.bOverride_ColorGain = true;
	Camera->PostProcessSettings.ColorGain = FVector4(1.12f, 1.0f, 0.82f, 1.f);
	Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
	Camera->PostProcessSettings.VignetteIntensity = 0.65f;

	EngineAudio = CreateDefaultSubobject<UBNEngineAudioComponent>(TEXT("EngineAudio"));
	EngineAudio->SetupAttachment(Body);

	Effects = CreateDefaultSubobject<UBNVehicleEffectsComponent>(TEXT("Effects"));
	Effects->SetupAttachment(Body);

	for (const float Side : { -1.f, 1.f })
	{
		USpotLightComponent* Head = CreateDefaultSubobject<USpotLightComponent>(Side < 0.f ? TEXT("HeadlightL") : TEXT("HeadlightR"));
		Head->SetupAttachment(Body);
		Head->SetRelativeLocation(FVector(265.f, Side * 75.f, 70.f));
		Head->SetRelativeRotation(FRotator(-4.f, 0.f, 0.f));
		Head->SetIntensityUnits(ELightUnits::Candelas);
		Head->SetIntensity(HeadlightCandelas);
		Head->SetLightColor(HeadlightColour);
		Head->SetAttenuationRadius(HeadlightRange);
		Head->SetInnerConeAngle(14.f);
		Head->SetOuterConeAngle(32.f);
		CarLights.Add(Head);

		UPointLightComponent* Tail = CreateDefaultSubobject<UPointLightComponent>(Side < 0.f ? TEXT("TaillightL") : TEXT("TaillightR"));
		Tail->SetupAttachment(Body);
		Tail->SetRelativeLocation(FVector(-265.f, Side * 70.f, 60.f));
		Tail->SetIntensityUnits(ELightUnits::Candelas);
		Tail->SetIntensity(TaillightCandelas);
		Tail->SetLightColor(TaillightColour);
		Tail->SetAttenuationRadius(TaillightRange);
		CarLights.Add(Tail);

		UPointLightComponent* Exhaust = CreateDefaultSubobject<UPointLightComponent>(Side < 0.f ? TEXT("ExhaustL") : TEXT("ExhaustR"));
		Exhaust->SetupAttachment(Body);
		Exhaust->SetRelativeLocation(FVector(-255.f, Side * 42.f, 45.f));
		Exhaust->SetIntensityUnits(ELightUnits::Candelas);
		Exhaust->SetIntensity(NosFlameCandelas);
		Exhaust->SetLightColor(NosFlameColour);
		Exhaust->SetAttenuationRadius(ExhaustFlameRange);
		Exhaust->SetCastShadows(false);
		Exhaust->SetVisibility(false);
		ExhaustLights.Add(Exhaust);
	}
	for (ULocalLightComponent* Light : CarLights)
	{
		Light->SetCastShadows(false);
		Light->SetVisibility(false);
	}
}

void ABNVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	TActorIterator<ABNDayNightCycle> Cycle(GetWorld());
	if (Cycle)
	{
		Cycle->OnNightChanged.AddDynamic(this, &ABNVehiclePawn::SetLightsOn);
		SetLightsOn(Cycle->IsNight());
	}
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->OnComponentHit.AddUniqueDynamic(this, &ABNVehiclePawn::OnVehicleHit);
	}
	LastGear = GetChaosMovement()->GetCurrentGear();

	if (IsPlayerControlled())
	{
		if (const UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
		{
			Prog->ApplyProfileToCar(*this);
		}
		if (UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
		{
			Story->EnsureIntroShown(this);
		}
	}
}

void ABNVehiclePawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetTimeDilation();
	Super::EndPlay(EndPlayReason);
}

void ABNVehiclePawn::UnPossessed()
{
	ResetTimeDilation();
	Super::UnPossessed();
}

void ABNVehiclePawn::ResetTimeDilation()
{
	bSpeedbreakerWanted = false;
	BoostState.bSpeedbreakerActive = false;
	BoostState.SpeedbreakerBlend = 0.f;
	if (bOwnsTimeDilation)
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
		}
		bOwnsTimeDilation = false;
	}
}

void ABNVehiclePawn::SetLightsOn(bool bOn)
{
	for (ULocalLightComponent* Light : CarLights)
	{
		Light->SetVisibility(bOn);
	}
}

bool ABNVehiclePawn::AreLightsOn() const
{
	return CarLights.Num() > 0 && CarLights[0]->IsVisible();
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

UChaosWheeledVehicleMovementComponent* ABNVehiclePawn::GetChaosMovement() const
{
	return CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
}

float ABNVehiclePawn::GetSpeedKmh() const
{
	return GetChaosMovement()->GetForwardSpeed() * CmPerSecToKmh;
}

void ABNVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float WorldDilation = GetWorld() && GetWorld()->GetWorldSettings()
		? GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation() : 1.f;
	const float RealDt = DeltaSeconds / FMath::Max(WorldDilation, 0.05f);

	// Damp spin in the air so jumps land on the wheels.
	GetMesh()->SetAngularDamping(IsOnGround() ? 0.f : AirborneAngularDamping);

	if (bInputLocked)
	{
		PlayerSteeringTarget = 0.f;
		PlayerSteeringSmoothed = 0.f;
		UChaosWheeledVehicleMovementComponent* Locked = GetChaosMovement();
		Locked->SetThrottleInput(0.f);
		Locked->SetBrakeInput(0.f);
		Locked->SetHandbrakeInput(true);
	}
	else if (bHasPlayerSteeringInput)
	{
		const float AbsSpeed = FMath::Abs(GetSpeedKmh());
		const bool bReturning = FMath::Abs(PlayerSteeringTarget) < 0.05f;
		const bool bReversing = PlayerSteeringTarget * PlayerSteeringSmoothed < -0.02f;
		const float RiseRate = FMath::GetMappedRangeValueClamped(FVector2f(40.f, 180.f), FVector2f(5.0f, 2.4f), AbsSpeed);
		const float Rate = bReversing ? 10.5f : bReturning ? 8.5f : RiseRate;
		PlayerSteeringSmoothed = FMath::FInterpConstantTo(PlayerSteeringSmoothed, PlayerSteeringTarget, DeltaSeconds, Rate);
		GetChaosMovement()->SetSteeringInput(PlayerSteeringSmoothed);
	}

	UpdateDrift(DeltaSeconds);
	UpdateNormalGripAssist(DeltaSeconds);
	UpdateBoostAndEffects(DeltaSeconds, RealDt);

	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	if (Move->GetCurrentGear() < 0 && GetSpeedKmh() < -MaxReverseSpeedKmh)
	{
		Move->SetBrakeInput(0.f); // in reverse the brake pedal is the throttle
	}
}

void ABNVehiclePawn::UpdateBoostAndEffects(float DeltaSeconds, float RealDt)
{
	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	const int32 Gear = Move->GetCurrentGear();
	if (Gear > LastGear && Gear > 1 && Move->GetEngineRotationSpeed() > UpshiftBackfireMinRpm)
	{
		BoostState.TriggerBackfire(BackfireDurationSeconds);
		GearShiftKick = 1.f;
	}
	LastGear = Gear;
	GearShiftKick = FMath::FInterpTo(GearShiftKick, 0.f, RealDt, 7.f);

	const float SpeedKmh = GetSpeedKmh();
	const bool bOnGround = IsOnGround();
	const UBNProgressionSubsystem* Prog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr;
	const UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr;
	const APlayerController* PC = GetController<APlayerController>();
	const ABNHUD* HUD = PC ? PC->GetHUD<ABNHUD>() : nullptr;
	const bool bInMenu = (Prog && Prog->IsGarageOpen())
		|| (Story && Story->IsBlockingGameplayInput())
		|| (HUD && HUD->IsMenuOpen());

	if (bInputLocked && !bInMenu)
	{
		BoostState.UpdateCountdownLaunch(RealDt, PlayerThrottleInput, Boost);
	}
	else if (bInMenu)
	{
		BoostState.LaunchRevFraction = 0.f;
	}
	BoostState.UpdateNos(RealDt, bNosWanted, bInputLocked, bOnGround, SpeedKmh, bDrifting, Boost);

	if (IsPlayerControlled())
	{
		BoostState.UpdateSpeedbreaker(RealDt, bSpeedbreakerWanted, bInputLocked, SpeedKmh, Boost);
	}
	else
	{
		BoostState.bSpeedbreakerActive = false;
	}
	BoostState.TickTimers(RealDt);

	if (IsPlayerControlled())
	{
		if (BoostState.bSpeedbreakerActive || BoostState.SpeedbreakerBlend > 0.01f)
		{
			UGameplayStatics::SetGlobalTimeDilation(GetWorld(), FMath::Lerp(1.f, Boost.SpeedbreakerTimeDilation, BoostState.SpeedbreakerBlend));
			bOwnsTimeDilation = true;
		}
		else if (bOwnsTimeDilation)
		{
			BoostState.SpeedbreakerBlend = 0.f;
			UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);
			bOwnsTimeDilation = false;
		}
	}

	if (!bInputLocked && bOnGround)
	{
		const float SurgeAccel = (BoostState.bNosActive ? Boost.NosAccelCmPerSec2 : 0.f)
			+ (BoostState.LaunchBoostRemaining > 0.f ? Boost.LaunchBoostAccelCmPerSec2 : 0.f);
		if (SurgeAccel > 0.f)
		{
			USkeletalMeshComponent* Body = GetMesh();
			const FVector Velocity = Body->GetPhysicsLinearVelocity();
			const float OldSpeed2D = Velocity.Size2D();
			const float Cap2D = Boost.NosMaxSpeedKmh / CmPerSecToKmh;
			const FVector Forward2D = GetActorForwardVector().GetSafeNormal2D();
			FVector NewVel2D = FVector(Velocity.X, Velocity.Y, 0.f) + Forward2D * (SurgeAccel * DeltaSeconds);
			const float TargetCap = FMath::Max(OldSpeed2D, Cap2D);
			if (NewVel2D.Size() > TargetCap)
			{
				NewVel2D = NewVel2D.GetSafeNormal() * TargetCap;
			}
			Body->SetPhysicsLinearVelocity(FVector(NewVel2D.X, NewVel2D.Y, Velocity.Z));
		}

		// Speedbreaker sharpens steering authority by swinging travel velocity smoothly toward the nose.
		if (BoostState.bSpeedbreakerActive && !bDrifting)
		{
			USkeletalMeshComponent* Body = GetMesh();
			const FVector Velocity = Body->GetPhysicsLinearVelocity();
			if (Velocity.Size2D() > 600.f)
			{
				const float Slip = GetSlipAngleDegrees();
				const float MaxTurn = Boost.SpeedbreakerGripAssistDegPerSec * DeltaSeconds;
				const float Turn = -FMath::Clamp(Slip, -MaxTurn, MaxTurn);
				const FVector Travel = FRotator(0.f, Turn, 0.f).RotateVector(FVector(Velocity.X, Velocity.Y, 0.f));
				Body->SetPhysicsLinearVelocity(FVector(Travel.X, Travel.Y, Velocity.Z));
			}
		}

		if (IsPlayerControlled())
		{
			UpdateNearMiss(SpeedKmh);
		}
	}

	const float RealTimeSeconds = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	UBNVehicleVisualsComponent::UpdateExhaustLights(ExhaustLights, BoostState.bNosActive, IsBackfireActive(), RealTimeSeconds);
	if (Effects)
	{
		Effects->UpdateEffects(DeltaSeconds, SpeedKmh, GetSlipAngleDegrees(), bDrifting, bOnGround,
			BoostState.bNosActive, IsBackfireActive());
	}
	UpdateCamera(DeltaSeconds, RealDt, SpeedKmh, RealTimeSeconds);
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
	const float NosFovScale = bHood ? 0.55f : 1.f;
	Camera->SetFieldOfView(SpeedFov + BoostState.NosBlend * Boost.NosFovBonus * NosFovScale - BoostState.SpeedbreakerBlend * 6.f);
	Camera->PostProcessBlendWeight = BoostState.SpeedbreakerBlend;

	const float BaseDistance = ChaseDistances.IsValidIndex(ChaseDistanceIndex) ? ChaseDistances[ChaseDistanceIndex] : 650.f;
	const float TargetArm = bHood ? 0.f : (BaseDistance + BoostState.NosBlend * Boost.NosArmPullback + GearShiftKick * 28.f);
	const float NewArm = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, RealDt, bHood ? 16.f : 12.f);
	CameraBoom->TargetArmLength = (bHood && NewArm < 2.f) ? 0.f : NewArm;

	const float ShakeScale = (BoostState.bSpeedbreakerActive ? 0.f : (bHood ? 0.35f : 1.f))
		* FMath::GetMappedRangeValueClamped(FVector2f(SpeedShakeMinKmh, SpeedShakeFullKmh), FVector2f(0.f, MaxSpeedShakeAmplitude), FMath::Abs(SpeedKmh));
	const float ShakeY = ShakeScale * FMath::Sin(RealTimeSeconds * 37.f);
	const float ShakeZ = ShakeScale * 0.7f * FMath::Sin(RealTimeSeconds * 53.f + 1.1f);
	CurrentSocketBaseZ = FMath::FInterpTo(CurrentSocketBaseZ, bHood ? 0.f : BaseSocketZ, RealDt, 16.f);
	CameraBoom->SocketOffset = FVector(0.f, ShakeY, CurrentSocketBaseZ + ShakeZ);
}

void ABNVehiclePawn::UpdateNearMiss(float SpeedKmh)
{
	if (FMath::Abs(SpeedKmh) < NearMissMinSpeedKmh || !GetWorld())
	{
		return;
	}
	const double Now = GetWorld()->GetRealTimeSeconds();
	const FVector MyLocation = GetActorLocation();
	for (TActorIterator<ABNVehiclePawn> It(GetWorld()); It; ++It)
	{
		ABNVehiclePawn* Other = *It;
		if (!Other || Other == this || FVector::Dist2D(MyLocation, Other->GetActorLocation()) > NearMissDistanceCm)
		{
			continue;
		}
		if (const double* CooldownUntil = NearMissCooldowns.Find(Other); CooldownUntil && Now < *CooldownUntil)
		{
			continue;
		}
		NearMissCooldowns.Add(Other, Now + NearMissCooldownSeconds);
		BoostState.AddNearMissBonus(Boost);
	}
}

void ABNVehiclePawn::OnVehicleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (ABNVehiclePawn* OtherCar = Cast<ABNVehiclePawn>(OtherActor))
	{
		if (GetWorld())
		{
			NearMissCooldowns.Add(OtherCar, GetWorld()->GetRealTimeSeconds() + CrashNearMissSuppressSeconds);
		}
	}
	const float ImpulseMag = NormalImpulse.Size();
	if (Effects && ImpulseMag > CollisionSparkMinImpulse)
	{
		const float Intensity = FMath::Clamp((ImpulseMag - CollisionSparkMinImpulse) / 320000.f, 0.25f, 1.f);
		const FVector SparkLoc = Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(Hit.ImpactPoint);
		const FVector SparkNormal = Hit.ImpactNormal.IsNearlyZero() ? FVector::UpVector : FVector(Hit.ImpactNormal);
		Effects->TriggerCollisionSparks(SparkLoc, SparkNormal, Intensity);
	}
}

bool ABNVehiclePawn::IsOnGround() const
{
	const UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	for (int32 i = 0; i < Move->Wheels.Num(); ++i)
	{
		if (Move->GetWheelState(i).bInContact)
		{
			return true;
		}
	}
	return false;
}

float ABNVehiclePawn::GetSlipAngleDegrees() const
{
	const FVector Velocity = GetVelocity().GetSafeNormal2D();
	if (FMath::Abs(GetSpeedKmh()) < MinSpeedForSlipKmh || Velocity.IsNearlyZero())
	{
		return 0.f;
	}
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	return FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Forward, Velocity).Z, FVector::DotProduct(Forward, Velocity)));
}

void ABNVehiclePawn::UpdateDrift(float DeltaSeconds)
{
	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	const bool bHandbrake = Move->GetHandbrakeInput();
	if (bHandbrake != bRearAbsBypassed)
	{
		bRearAbsBypassed = bHandbrake;
		for (int32 i = FirstRearWheel; i < Move->Wheels.Num(); ++i)
		{
			Move->SetABSEnabled(i, !bHandbrake && Move->Wheels[i]->bABSEnabled);
		}
	}

	const float Speed = FMath::Abs(GetSpeedKmh());
	const float Steer = Move->GetSteeringInput();
	const float Throttle = Move->GetThrottleInput();
	if (!bDrifting && bHandbrake && Speed >= Drift.MinSpeedKmh && FMath::Abs(Steer) >= DriftStartMinSteer)
	{
		bDrifting = true;
		DriftDirection = Steer > 0.f ? -1.f : 1.f;
		DriftEntrySpeed = GetMesh()->GetPhysicsLinearVelocity().Size2D();
		CurrentDriftTargetSlip = GetSlipAngleDegrees();
	}

	if (bDrifting)
	{
		const float Into = -Steer * DriftDirection;
		const float TargetAngle = Into > DriftHoldMinSteer
			? FMath::GetMappedRangeValueClamped(FVector2f(DriftHoldMinSteer, 1.f), FVector2f(Drift.MinAngleDegrees, Drift.MaxAngleDegrees), Into)
			: 0.f;
		const float RawTarget = TargetAngle * DriftDirection;
		const float InterpRate = (TargetAngle == 0.f || RawTarget * CurrentDriftTargetSlip < 0.f) ? 8.5f : 6.5f;
		CurrentDriftTargetSlip = FMath::FInterpTo(CurrentDriftTargetSlip, RawTarget, DeltaSeconds, InterpRate);

		const bool bUnwound = TargetAngle == 0.f && FMath::Abs(GetSlipAngleDegrees()) < Drift.ExitAngleDegrees;
		if (bUnwound || Speed < Drift.MinSpeedKmh * DriftEndSpeedFraction)
		{
			bDrifting = false;
			CurrentDriftTargetSlip = 0.f;
		}
		else if (IsOnGround())
		{
			ApplyDriftAssist(DeltaSeconds, CurrentDriftTargetSlip, Throttle);
		}
	}
	SetRearGrip(bDrifting ? 0.f : FMath::Min(1.f, RearGripBlend + DeltaSeconds / Drift.GripRecoverSeconds));
}

void ABNVehiclePawn::UpdateNormalGripAssist(float DeltaSeconds)
{
	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	const float SpeedKmh = FMath::Abs(GetSpeedKmh());
	if (bDrifting || bInputLocked || !IsPlayerControlled() || !IsOnGround()
		|| Move->GetHandbrakeInput() || SpeedKmh < MinNormalAssistSpeedKmh)
	{
		return;
	}
	USkeletalMeshComponent* Body = GetMesh();
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	if (Velocity.Size2D() < 600.f)
	{
		return;
	}
	const float Slip = GetSlipAngleDegrees();
	const float AbsSlip = FMath::Abs(Slip);
	if (AbsSlip > 0.5f && AbsSlip < 35.f)
	{
		const float MaxTurn = PlayerGripAssistDegPerSec * DeltaSeconds;
		const float Turn = -FMath::Clamp(Slip, -MaxTurn, MaxTurn);
		const FVector Travel = FRotator(0.f, Turn, 0.f).RotateVector(FVector(Velocity.X, Velocity.Y, 0.f));
		Body->SetPhysicsLinearVelocity(FVector(Travel.X, Travel.Y, Velocity.Z));
	}
	if (FMath::Abs(Move->GetSteeringInput()) < 0.08f)
	{
		const FVector Spin = Body->GetPhysicsAngularVelocityInDegrees();
		Body->SetPhysicsAngularVelocityInDegrees(FVector(Spin.X, Spin.Y, FMath::FInterpTo(Spin.Z, 0.f, DeltaSeconds, 5.f)));
	}
}

void ABNVehiclePawn::ApplyDriftAssist(float DeltaSeconds, float TargetSlip, float Throttle)
{
	USkeletalMeshComponent* Body = GetMesh();
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	const float Slip = GetSlipAngleDegrees();

	if (Throttle < 0.15f || GetChaosMovement()->GetBrakeInput() > 0.1f)
	{
		DriftEntrySpeed = FMath::Max(0.f, DriftEntrySpeed - 650.f * DeltaSeconds);
	}

	const float SpeedScale = FMath::GetMappedRangeValueClamped(FVector2f(80.f, 200.f), FVector2f(1.0f, 0.55f), FMath::Abs(GetSpeedKmh()));
	const float TravelYawRate = -Drift.LineTightness * SpeedScale * Slip;
	const float DesiredYawRate = FMath::Clamp(TravelYawRate - Drift.AngleGain * (TargetSlip - Slip), -MaxAssistYawRate, MaxAssistYawRate);

	const float Push = FMath::Max(Throttle, 0.f) * Drift.Thrust * DeltaSeconds;
	const float Speed2D = FMath::Max(Velocity.Size2D(), FMath::Min(Velocity.Size2D() + Push, DriftEntrySpeed));
	const float TravelYaw = FMath::RadiansToDegrees(FMath::Atan2(Velocity.Y, Velocity.X)) + TravelYawRate * DeltaSeconds;
	const FVector Travel = FRotator(0.f, TravelYaw, 0.f).Vector() * Speed2D;
	Body->SetPhysicsLinearVelocity(FVector(Travel.X, Travel.Y, Velocity.Z));
	const FVector Spin = Body->GetPhysicsAngularVelocityInDegrees();
	Body->SetPhysicsAngularVelocityInDegrees(FVector(Spin.X, Spin.Y, FMath::FInterpTo(Spin.Z, DesiredYawRate, DeltaSeconds, 14.f)));
}

void ABNVehiclePawn::SetRearGrip(float Blend)
{
	const bool bTractionControl = !bDrifting;
	if (Blend == RearGripBlend && bTractionControl == bRearTractionControl && LastAppliedTireGripScale == ActiveTireGripScale)
	{
		return;
	}
	RearGripBlend = Blend;
	bRearTractionControl = bTractionControl;
	LastAppliedTireGripScale = ActiveTireGripScale;
	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	for (int32 i = FirstRearWheel; i < Move->Wheels.Num(); ++i)
	{
		const UChaosVehicleWheel* Wheel = Move->Wheels[i];
		Move->SetWheelFrictionMultiplier(i, Wheel->FrictionForceMultiplier * ActiveTireGripScale * FMath::Lerp(Drift.RearGripScale, 1.f, Blend));
		Move->SetTractionControlEnabled(i, bTractionControl && Wheel->bTractionControlEnabled);
	}
}

void ABNVehiclePawn::ApplyPhysicsStats(const FBNComputedVehicleStats& Stats)
{
	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	ActiveTireGripScale = Stats.TireGripScale;

	Move->EngineSetup.MaxTorque = Stats.MaxTorqueNm;
	Move->EngineSetup.MaxRPM = Stats.MaxRpm;
	Move->SetMaxEngineTorque(Stats.MaxTorqueNm);
	Move->SetDragCoefficient(Stats.DragCoefficient);
	Move->SetDownforceCoefficient(Stats.DownforceCoefficient);
	Move->TransmissionSetup.GearChangeTime = Stats.GearChangeTime;
	Move->TransmissionSetup.TransmissionEfficiency = Stats.TransmissionEfficiency;
	Move->SteeringSetup.AngleRatio = Stats.SteeringAngleRatio;

	for (int32 i = 0; i < Move->Wheels.Num(); ++i)
	{
		if (const UChaosVehicleWheel* Wheel = Move->Wheels[i])
		{
			Move->SetWheelMaxBrakeTorque(i, Wheel->MaxBrakeTorque * Stats.BrakeScale);
			if (i < FirstRearWheel)
			{
				Move->SetWheelFrictionMultiplier(i, Wheel->FrictionForceMultiplier * ActiveTireGripScale);
			}
		}
	}
	LastAppliedTireGripScale = -1.f;
	SetRearGrip(RearGripBlend);

	Drift.Thrust = Stats.DriftThrust;
	Boost.NosAccelCmPerSec2 = Stats.NosAccelCmPerSec2;
	Boost.NosMaxSeconds = Stats.NosMaxSeconds;
	Boost.NosMaxSpeedKmh = Stats.NosMaxSpeedKmh;
}

void ABNVehiclePawn::ApplyVehicleSpec(EBNCarId CarId, const FBNComputedVehicleStats& Stats)
{
	ActiveCarId = CarId;
	ApplyPhysicsStats(Stats);
	if (Visuals)
	{
		Visuals->ApplyCarVisuals(FBNVehicleSpec::ForCar(CarId), Chassis, ChassisGlass, WheelMeshes);
	}
}

void ABNVehiclePawn::ApplyVehicleBuild(const FBNComputedVehicleStats& Stats, const FLinearColor& PaintTint)
{
	ApplyPhysicsStats(Stats);
	ApplyPaintTint(PaintTint);
}

void ABNVehiclePawn::ApplyPaintTint(const FLinearColor& PaintTint)
{
	if (Visuals)
	{
		Visuals->ApplyPaintTint(PaintTint, Chassis);
	}
}

void ABNVehiclePawn::SetInputLocked(bool bLocked)
{
	const bool bWasLocked = bInputLocked;
	bInputLocked = bLocked;
	if (bLocked)
	{
		bNosWanted = false;
		BoostState.bNosActive = false;
		ResetTimeDilation();
	}
	else if (bWasLocked && BoostState.TriggerLaunchRelease(Boost))
	{
		BoostState.TriggerBackfire(LaunchBackfireDurationSeconds);
	}
	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	Move->SetThrottleInput(0.f);
	Move->SetBrakeInput(0.f);
	Move->SetSteeringInput(0.f);
	Move->SetHandbrakeInput(bLocked);
}

void ABNVehiclePawn::ResetVehicle()
{
	FRotator Heading = GetActorRotation();
	Heading.Pitch = 0.f;
	Heading.Roll = 0.f;
	SetActorTransform(FTransform(Heading, GetActorLocation() + FVector(0.f, 0.f, 50.f)),
		false, nullptr, ETeleportType::TeleportPhysics);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
}
