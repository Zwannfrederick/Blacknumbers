#include "Vehicle/BNVehiclePawn.h"

#include "Animation/AnimInstance.h"
#include "Audio/BNAudioSubsystem.h"
#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "CollisionQueryParams.h"
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
#include "Police/BNPoliceController.h"
#include "Police/BNPursuitSubsystem.h"
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
	constexpr float GroundedAngularDamping = 0.15f;
	constexpr double AirborneTiltDamping = 2.8;
	constexpr double AirborneTiltStrength = 125.0;
	constexpr double AirborneTiltSpeedLimit = 70.0;
	constexpr double TakeoffSpeedAllowance = 155.0;
	constexpr double ImpactLiftAllowance = 65.0;
	constexpr float ImpactReboundSeconds = 0.20f;
	constexpr float RecoverySampleInterval = 0.35f;
	constexpr double RecoveryLiftCm = 45.0;
	/** Rest rotation of every Phys_Wheel_* bone in component space (measured; see Vehicle.WheelsAligned). */
	const FRotator WheelBoneRest(0.f, -90.f, 90.f);
	const TCHAR* const WheelBones[] = { TEXT("Phys_Wheel_FL"), TEXT("Phys_Wheel_FR"), TEXT("Phys_Wheel_BL"), TEXT("Phys_Wheel_BR") };

	// Light placement from SM_SportsCar bounds: nose at x ~ +247 cm, tail at x ~ -223 cm. Both sit just
	// outside the body: a light inside or touching the shell floods the car itself and overexposes it.
	constexpr float HeadlightCandelas = 3400.f;
	constexpr float AiHeadlightCandelas = 1050.f;
	constexpr float HeadlightRange = 4600.f;
	const FLinearColor HeadlightColour(1.f, 0.94f, 0.84f);
	constexpr float TaillightCandelas = 2.6f;
	constexpr float TaillightRange = 280.f;
	const FLinearColor TaillightColour(1.f, 0.05f, 0.02f);

	constexpr float ExhaustFlameRange = 70.f;
	constexpr float NosFlameCandelas = 4.f;
	const FLinearColor NosFlameColour(0.2f, 0.75f, 1.f);

	constexpr float BaseSocketZ = 150.f;
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
			Move.WheelSetups[i].WheelClass = i < ABNVehiclePawn::FirstRearWheelIndex ? UBNWheelFront::StaticClass() : UBNWheelRear::StaticClass();
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
		Head->SetRelativeRotation(FRotator(-6.f, 0.f, 0.f));
		Head->SetIntensityUnits(ELightUnits::Candelas);
		Head->SetIntensity(HeadlightCandelas);
		Head->SetSpecularScale(0.60f);
		Head->SetLightColor(HeadlightColour);
		Head->SetAttenuationRadius(HeadlightRange);
		Head->SetSourceRadius(18.f);
		Head->SetInnerConeAngle(18.f);
		Head->SetOuterConeAngle(42.f);
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
		Exhaust->SetRelativeLocation(FVector(-262.f, Side * 62.f, 50.f));
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

	UnderglowLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("UnderglowLight"));
	UnderglowLight->SetupAttachment(Body);
	UnderglowLight->SetRelativeLocation(FVector(0.f, 0.f, 34.f));
	UnderglowLight->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	UnderglowLight->SetIntensityUnits(ELightUnits::Candelas);
	UnderglowLight->SetIntensity(220.f);
	UnderglowLight->SetAttenuationRadius(260.f);
	UnderglowLight->SetSourceRadius(65.f);
	UnderglowLight->SetInnerConeAngle(54.f);
	UnderglowLight->SetOuterConeAngle(80.f);
	UnderglowLight->SetCastShadows(false);
	UnderglowLight->SetVisibility(false);
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
		Body->SetAngularDamping(GroundedAngularDamping);
		Body->SetAllUseCCD(true);
		Body->SetAllBodiesNotifyRigidBodyCollision(true);
		Body->OnComponentHit.AddUniqueDynamic(this, &ABNVehiclePawn::OnVehicleHit);
	}
	GroundedSample = RecoveryPose = GetActorTransform();
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
	else
	{
		for (ULocalLightComponent* Light : CarLights)
		{
			if (USpotLightComponent* Spot = Cast<USpotLightComponent>(Light))
			{
				Spot->SetIntensity(AiHeadlightCandelas);
			}
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

	UpdateChassisStability(DeltaSeconds);

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

void ABNVehiclePawn::UpdateChassisStability(float DeltaSeconds)
{
	USkeletalMeshComponent* Body = GetMesh();
	const UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	const int32 WheelCount = Move->Wheels.Num();
	int32 Contacts = 0;
	FVector ContactPoints[UE_ARRAY_COUNT(WheelBones)] = {};
	for (int32 Index = 0; Index < FMath::Min<int32>(WheelCount, UE_ARRAY_COUNT(WheelBones)); ++Index)
	{
		const FWheelStatus& Wheel = Move->GetWheelState(Index);
		Contacts += Wheel.bInContact;
		ContactPoints[Index] = Wheel.ContactPoint;
	}

	const FVector Up = GetActorUpVector();
	const bool bGrounded = Contacts > 0;
	FVector Vel = Body->GetPhysicsLinearVelocity();
	if (Contacts == UE_ARRAY_COUNT(WheelBones))
	{
		const FVector Axles = ContactPoints[0] + ContactPoints[1] - ContactPoints[2] - ContactPoints[3];
		const FVector Track = ContactPoints[1] + ContactPoints[3] - ContactPoints[0] - ContactPoints[2];
		SupportNormal = FVector::CrossProduct(Axles, Track).GetSafeNormal();
	}

	// Trim suspension rebound at takeoff against the last supported plane. Once airborne, gravity owns the arc.
	double AllowedRise = TNumericLimits<double>::Max();
	if ((bGrounded || bWasGrounded) && SupportNormal.Z > 0.6 && Vel.Z > TakeoffSpeedAllowance)
	{
		const double RampRise = -(Vel.X * SupportNormal.X + Vel.Y * SupportNormal.Y) / SupportNormal.Z;
		AllowedRise = FMath::Max(0.0, RampRise) + TakeoffSpeedAllowance;
	}
	if (ImpactReboundRemaining > 0.f)
	{
		AllowedRise = FMath::Min(AllowedRise, ImpactRiseLimit);
		ImpactReboundRemaining -= DeltaSeconds;
	}
	if (Vel.Z > AllowedRise)
	{
		Vel.Z = AllowedRise;
		Body->SetPhysicsLinearVelocity(Vel);
	}
	LastVerticalSpeed = Vel.Z;
	bWasGrounded = bGrounded;

	if (!bGrounded)
	{
		FVector Spin = Body->GetPhysicsAngularVelocityInDegrees();
		const FVector Axis = FVector::CrossProduct(Up, FVector::UpVector);
		const FVector Tilt = (FVector(Spin.X, Spin.Y, 0.f) * FMath::Exp(-AirborneTiltDamping * DeltaSeconds)
			+ Axis * AirborneTiltStrength * DeltaSeconds).GetClampedToMaxSize(AirborneTiltSpeedLimit);
		Body->SetPhysicsAngularVelocityInDegrees(FVector(Tilt.X, Tilt.Y, Spin.Z));
	}

	if (!bGrounded || Up.Z < 0.85)
	{
		GroundedSampleSeconds = 0.f;
		return;
	}
	GroundedSampleSeconds += DeltaSeconds;
	if (GroundedSampleSeconds >= RecoverySampleInterval)
	{
		GroundedSampleSeconds = 0.f;
		const FVector Location = GetActorLocation();
		FHitResult Ground;
		if (GetWorld()->LineTraceSingleByObjectType(Ground, Location, Location - FVector(0.f, 0.f, 135.f),
			FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(BNRecoveryGround), false, this))
			&& Ground.ImpactNormal.Z > 0.65)
		{
			RecoveryPose = GroundedSample;
			GroundedSample = GetActorTransform();
		}
	}
}

void ABNVehiclePawn::UpdateBoostAndEffects(float DeltaSeconds, float RealDt)
{
	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	const int32 Gear = Move->GetCurrentGear();
	if (!DragState.bActive && Gear > LastGear && Gear > 1 && Move->GetEngineRotationSpeed() > UpshiftBackfireMinRpm)
	{
		BoostState.TriggerBackfire(0.24f);
		GearShiftKick = 1.55f;
		ShiftFlashRemaining = 0.24f;
		if (Effects)
		{
			Effects->TriggerExhaustSparks(BoostState.bNosActive);
		}
		if (IsPlayerControlled())
		{
			if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
			{
				Audio->PlaySfx2D(EBNSfxId::ShiftPop, 0.72f, FMath::FRandRange(0.95f, 1.08f));
			}
		}
	}
	LastGear = Gear;
	GearShiftKick = FMath::FInterpTo(GearShiftKick, 0.f, RealDt, 7.5f);

	// Turbo blow-off "pssh" when the player lifts off the throttle at high revs.
	const float Throttle = Move->GetThrottleInput();
	BlowoffCooldown = FMath::Max(0.f, BlowoffCooldown - RealDt);
	if (IsPlayerControlled() && BlowoffCooldown <= 0.f && LastThrottleInput >= 0.75f && Throttle <= 0.15f
		&& Move->GetEngineRotationSpeed() > Move->EngineSetup.MaxRPM * 0.55f)
	{
		BlowoffCooldown = 0.8f;
		if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
		{
			Audio->PlaySfx2D(EBNSfxId::TurboBlowoff, 0.55f, FMath::FRandRange(0.94f, 1.1f));
		}
	}
	LastThrottleInput = Throttle;

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
	const bool bWasNosActive = BoostState.bNosActive;
	BoostState.UpdateNos(RealDt, bNosWanted, bInputLocked, bOnGround, SpeedKmh, bDrifting, Boost);
	if (!bWasNosActive && BoostState.bNosActive && IsPlayerControlled())
	{
		GearShiftKick = FMath::Max(GearShiftKick, 1.25f);
		if (Effects)
		{
			Effects->TriggerExhaustSparks(true);
		}
		if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
		{
			Audio->PlaySfx2D(EBNSfxId::NosIgnite, 0.95f);
		}
	}

	if (IsPlayerControlled())
	{
		BoostState.UpdateSpeedbreaker(RealDt, bSpeedbreakerWanted, bInputLocked, SpeedKmh, Boost);
		UpdateSlipstream(RealDt, SpeedKmh, bOnGround);
	}
	else
	{
		BoostState.bSpeedbreakerActive = false;
	}
	BoostState.TickTimers(RealDt);
	DriftExitBoostRemaining = FMath::Max(0.f, DriftExitBoostRemaining - RealDt);
	DriftExitBannerRemaining = FMath::Max(0.f, DriftExitBannerRemaining - RealDt);
	CameraImpactShake = FMath::FInterpTo(CameraImpactShake, 0.f, RealDt, 5.5f);
	if (NearMissStreakTimer > 0.f)
	{
		NearMissStreakTimer = FMath::Max(0.f, NearMissStreakTimer - RealDt);
		if (NearMissStreakTimer <= 0.f)
		{
			NearMissStreak = 0;
		}
	}

	UpdateDamageAndDrag(DeltaSeconds, RealDt, SpeedKmh);

	if (IsPlayerControlled() && TotaledSlowMoRemaining <= 0.f)
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
			+ (BoostState.LaunchBoostRemaining > 0.f ? Boost.LaunchBoostAccelCmPerSec2 : 0.f)
			+ (DriftExitBoostRemaining > 0.f ? 850.f : 0.f)
			+ BoostState.GetSlipstreamAccelCmPerSec2(Boost);
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
	if (Visuals)
	{
		const bool bBraking = (Move->GetBrakeInput() > 0.1f && SpeedKmh > -2.f) || Move->GetHandbrakeInput();
		Visuals->UpdateLightStrips(AreLightsOn(), bBraking);
	}
	if (Effects)
	{
		Effects->UpdateEffects(DeltaSeconds, SpeedKmh, GetSlipAngleDegrees(), bDrifting, bOnGround,
			BoostState.bNosActive, IsBackfireActive(), VehicleHealth, IsEngineBlown());
	}
	UpdateCamera(DeltaSeconds, RealDt, SpeedKmh, RealTimeSeconds);
}

void ABNVehiclePawn::GrantNearMissBonus()
{
	NearMissStreak = FMath::Min(NearMissStreak + 1, 5);
	NearMissStreakTimer = 4.5f;
	BoostState.AddNearMissBonus(Boost);
	if (NearMissStreak > 1)
	{
		BoostState.NosFraction = FMath::Min(1.f, BoostState.NosFraction + 0.02f * (NearMissStreak - 1));
	}
}

void ABNVehiclePawn::AddCameraImpulse(float Strength)
{
	if (IsPlayerControlled())
	{
		CameraImpactShake = FMath::Clamp(FMath::Max(CameraImpactShake, Strength), 0.f, 1.4f);
	}
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
		GrantNearMissBonus();
		if (UBNAudioSubsystem* Audio = GetWorld()->GetSubsystem<UBNAudioSubsystem>())
		{
			Audio->PlaySfx2D(EBNSfxId::NearMissWhoosh, 0.85f);
		}
	}
}

void ABNVehiclePawn::OnVehicleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	ABNVehiclePawn* OtherCar = Cast<ABNVehiclePawn>(OtherActor);
	if (OtherCar && GetWorld())
	{
		NearMissCooldowns.Add(OtherCar, GetWorld()->GetRealTimeSeconds() + CrashNearMissSuppressSeconds);
	}
	const float ImpulseMag = NormalImpulse.Size();

	// Anti-launch & wall-glance physics stabilizer: kill absurd Chaos vertical rocket bounces & flip spins
	// on wall/barrier/curb/car impacts so crashes slide cleanly like Most Wanted instead of catapulting into the air.
	if ((OtherCar || FMath::Abs(Hit.ImpactNormal.Z) < 0.65f) && ImpulseMag > 25000.f)
	{
		if (USkeletalMeshComponent* Body = GetMesh())
		{
			FVector Vel = Body->GetPhysicsLinearVelocity();
			bool bAdjustedVel = false;
			if (ImpactReboundRemaining <= 0.f)
			{
				ImpactRiseLimit = FMath::Max(LastVerticalSpeed, ImpactLiftAllowance);
			}
			ImpactReboundRemaining = ImpactReboundSeconds;
			if (Vel.Z > ImpactRiseLimit)
			{
				Vel.Z = ImpactRiseLimit;
				bAdjustedVel = true;
			}
			const FVector Normal2D = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();
			if (!Normal2D.IsNearlyZero())
			{
				const float BounceOut = FVector::DotProduct(FVector(Vel.X, Vel.Y, 0.f), Normal2D);
				if (BounceOut > 200.f)
				{
					const FVector Subtract = Normal2D * (BounceOut - 160.f);
					Vel.X -= Subtract.X;
					Vel.Y -= Subtract.Y;
					bAdjustedVel = true;
				}
			}
			if (bAdjustedVel)
			{
				Body->SetPhysicsLinearVelocity(Vel);
			}
			FVector Ang = Body->GetPhysicsAngularVelocityInDegrees();
			Ang.X = FMath::Clamp(Ang.X, -40.f, 40.f);
			Ang.Y = FMath::Clamp(Ang.Y, -40.f, 40.f);
			Ang.Z = FMath::Clamp(Ang.Z, -135.f, 135.f);
			Body->SetPhysicsAngularVelocityInDegrees(Ang);
		}
	}

	if (ImpulseMag > CollisionSparkMinImpulse)
	{
		const float Intensity = FMath::Clamp((ImpulseMag - CollisionSparkMinImpulse) / 320000.f, 0.25f, 1.f);
		const FVector SparkLoc = Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(Hit.ImpactPoint);
		const FVector SparkNormal = Hit.ImpactNormal.IsNearlyZero() ? FVector::UpVector : FVector(Hit.ImpactNormal);
		if (Effects)
		{
			Effects->TriggerCollisionSparks(SparkLoc, SparkNormal, Intensity);
		}
		AddCameraImpulse(Intensity * 1.05f);
		if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
		{
			const EBNSfxId CrashId = ImpulseMag > 260000.f ? EBNSfxId::CrashHeavy : EBNSfxId::CrashLight;
			Audio->PlaySfxAtLocation(CrashId, SparkLoc, FMath::Clamp(0.45f + Intensity * 0.6f, 0.4f, 1.f));
		}
		ApplyCollisionDamage(ImpulseMag, GetSpeedKmh(), FMath::Abs(Hit.ImpactNormal.Z) < 0.65f || OtherCar != nullptr);
		if (IsPlayerControlled() && OtherCar)
		{
			if (ABNPoliceController* PitCtrl = Cast<ABNPoliceController>(OtherCar->GetController()); PitCtrl && PitCtrl->IsAttemptingPit())
			{
				PitCtrl->ApplyPitContact(*this);
			}
		}
		// High-speed player slam against an active police cruiser can damage or disable the pursuing unit!
		if (IsPlayerControlled() && OtherCar && ImpulseMag > 280000.f && FMath::Abs(GetSpeedKmh()) >= 100.f)
		{
			if (const ABNPoliceController* CopCtrl = Cast<ABNPoliceController>(OtherCar->GetController());
				CopCtrl && !CopCtrl->IsUnitDisabled())
			{
				if (UBNPursuitSubsystem* Pursuit = GetWorld() ? GetWorld()->GetSubsystem<UBNPursuitSubsystem>() : nullptr)
				{
					Pursuit->OnPlayerRammedPolice(OtherCar, FMath::Abs(GetSpeedKmh()));
				}
			}
		}
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
			if (i < FirstRearWheelIndex)
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
	if (EngineAudio)
	{
		EngineAudio->SetEngineProfileForCar(CarId);
	}
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

void ABNVehiclePawn::ApplyPaintFinish(EBNPaintFinish Finish)
{
	if (Visuals)
	{
		Visuals->ApplyPaintFinishPreset(Finish, Chassis);
	}
}

void ABNVehiclePawn::ApplyRimPaint(const FLinearColor& RimTint)
{
	if (Visuals)
	{
		Visuals->ApplyRimPaint(RimTint, WheelMeshes);
	}
}

void ABNVehiclePawn::ApplyUnderglow(const FLinearColor& NeonColor, bool bEnabled)
{
	bUnderglowEnabled = bEnabled;
	UnderglowColor = NeonColor;
	if (UnderglowLight)
	{
		UnderglowLight->SetLightColor(NeonColor);
		UnderglowLight->SetIntensity(AreLightsOn() || bShowcaseMode ? 240.f : 120.f);
		UnderglowLight->SetVisibility(bEnabled);
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
	if (bTotaled || bBlownEngine)
	{
		RepairVehicle();
		SetInputLocked(false);
	}
	FRotator Heading = GetActorRotation();
	Heading.Pitch = 0.f;
	Heading.Roll = 0.f;
	StopAndPlaceVehicle(GetActorLocation() + FVector(0.f, 0.f, 50.f), Heading.Quaternion());
}

void ABNVehiclePawn::FellOutOfWorld(const UDamageType& DamageType)
{
	if (!HasAuthority() && GetLocalRole() != ROLE_None)
	{
		return;
	}
	++RecoveryCount;
	ResetTimeDilation();
	StopAndPlaceVehicle(RecoveryPose.GetLocation() + FVector(0.f, 0.f, RecoveryLiftCm), RecoveryPose.GetRotation());
}

void ABNVehiclePawn::StopAndPlaceVehicle(const FVector& Location, const FQuat& Rotation)
{
	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	bWasGrounded = false;
	SupportNormal = FVector::UpVector;
	LastVerticalSpeed = 0.0;
	ImpactReboundRemaining = 0.f;
	GroundedSampleSeconds = 0.f;
}
