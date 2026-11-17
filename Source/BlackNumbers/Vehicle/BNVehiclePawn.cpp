#include "Vehicle/BNVehiclePawn.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "UObject/ConstructorHelpers.h"
#include "Race/BNRaceEvent.h"
#include "Vehicle/BNEngineAudioComponent.h"
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
	/** Fastest the car is turned by the assist (deg/s), so a big correction never snaps. */
	constexpr float MaxAssistYawRate = 180.f;
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
		// Steering lock vs speed (MPH): calm at speed, but enough left for countersteering a drift.
		FRichCurve* Steering = Move.SteeringSetup.SteeringCurve.GetRichCurve();
		Steering->Reset();
		Steering->AddKey(0.f, 1.f);
		Steering->AddKey(25.f, 0.85f);
		Steering->AddKey(60.f, 0.6f);
		Steering->AddKey(100.f, 0.45f);
	}
}

ABNVehiclePawn::ABNVehiclePawn()
{
	// ponytail: hard-coded placeholder car from the engine's vehicle template; move to a data asset per car when real cars exist.
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
		// The Phys_Wheel_* bones rest at (P 0, Y -90, R 90) in component space, so undo that to line the
		// mesh axle (its local Y) up with the car's lateral axis; the bone's spin then turns the wheel about it.
		// SM_SportsCar_Wheel is a right-hand wheel (rim toward +Y, asset bounds origin y = +9.6 cm), so the
		// left wheels (FL, BL) also turn 180 degrees to face their rims outward. Guarded by Vehicle.WheelsAligned.
		const FQuat Outward = (i % 2 == 0) ? FRotator(0.f, 180.f, 0.f).Quaternion() : FQuat::Identity;
		Wheel->SetRelativeRotation(WheelBoneRest.Quaternion().Inverse() * Outward);
		WheelMeshes.Add(Wheel);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Body);
	CameraBoom->TargetArmLength = ChaseDistances[0];
	CameraBoom->SocketOffset.Z = 150.f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 4.f;
	CameraBoom->CameraLagMaxDistance = 50.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);
	Camera->SetFieldOfView(BaseFov);

	EngineAudio = CreateDefaultSubobject<UBNEngineAudioComponent>(TEXT("EngineAudio"));
	EngineAudio->SetupAttachment(Body);

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
	}
	for (ULocalLightComponent* Light : CarLights)
	{
		Light->SetCastShadows(false); // ponytail: shadowless car lights; a shadowed headlight is a High/Ultra option
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
	if (ChaseDistances.Num() > 0)
	{
		ChaseDistanceIndex = (ChaseDistanceIndex + 1) % ChaseDistances.Num();
		CameraBoom->TargetArmLength = ChaseDistances[ChaseDistanceIndex];
	}
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

	// Damp spin in the air so jumps land on the wheels.
	GetMesh()->SetAngularDamping(IsOnGround() ? 0.f : AirborneAngularDamping);

	Camera->SetFieldOfView(FMath::GetMappedRangeValueClamped(
		FVector2f(0.f, FovMaxSpeedKmh), FVector2f(BaseFov, MaxFov), FMath::Abs(GetSpeedKmh())));

	if (bInputLocked)
	{
		// Anything that drives the car directly (gamepad axis still held, scripts) is held off too.
		UChaosWheeledVehicleMovementComponent* Locked = GetChaosMovement();
		Locked->SetThrottleInput(0.f);
		Locked->SetBrakeInput(0.f);
		Locked->SetHandbrakeInput(true);
	}
	UpdateDrift(DeltaSeconds);

	// Ease the camera toward where the car is going while it slides, so drifts read calmly.
	const float CameraYaw = FMath::FInterpTo(CameraBoom->GetRelativeRotation().Yaw,
		bDrifting ? GetSlipAngleDegrees() * DriftCameraBlend : 0.f, DeltaSeconds, DriftCameraInterpSpeed);
	CameraBoom->SetRelativeRotation(FRotator(0.f, CameraYaw, 0.f));

	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	if (Move->GetCurrentGear() < 0 && GetSpeedKmh() < -MaxReverseSpeedKmh)
	{
		Move->SetBrakeInput(0.f); // in reverse the brake pedal is the throttle
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
		// Like a real handbrake, it bypasses ABS so it can lock the rear and start a slide.
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
		DriftDirection = Steer > 0.f ? -1.f : 1.f; // steering right turns right: the car slides out to the left
		DriftEntrySpeed = GetMesh()->GetPhysicsLinearVelocity().Size2D();
	}

	if (bDrifting)
	{
		// Wheel into the turn holds the drift, more lock = more angle; centred or countersteered unwinds it.
		const float Into = -Steer * DriftDirection;
		const float TargetAngle = Into > DriftHoldMinSteer
			? FMath::GetMappedRangeValueClamped(FVector2f(DriftHoldMinSteer, 1.f), FVector2f(Drift.MinAngleDegrees, Drift.MaxAngleDegrees), Into)
			: 0.f;
		const bool bUnwound = TargetAngle == 0.f && FMath::Abs(GetSlipAngleDegrees()) < Drift.ExitAngleDegrees;
		if (bUnwound || Speed < Drift.MinSpeedKmh * DriftEndSpeedFraction)
		{
			bDrifting = false;
		}
		else if (IsOnGround())
		{
			ApplyDriftAssist(DeltaSeconds, TargetAngle * DriftDirection, Throttle);
		}
	}
	SetRearGrip(bDrifting ? 0.f : FMath::Min(1.f, RearGripBlend + DeltaSeconds / Drift.GripRecoverSeconds));
}

void ABNVehiclePawn::ApplyDriftAssist(float DeltaSeconds, float TargetSlip, float Throttle)
{
	USkeletalMeshComponent* Body = GetMesh();
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	const float Slip = GetSlipAngleDegrees(); // travel direction minus heading, degrees
	// Travel swings toward the nose (the drift line curves), the nose is turned to keep the target angle.
	const float TravelYawRate = -Drift.LineTightness * Slip;
	const float HeadingYawRate = FMath::Clamp(TravelYawRate - Drift.AngleGain * (TargetSlip - Slip), -MaxAssistYawRate, MaxAssistYawRate);

	// Throttle holds the speed the drift started with (momentum), it does not add to it.
	const float Push = FMath::Max(Throttle, 0.f) * Drift.Thrust * DeltaSeconds;
	const float Speed2D = FMath::Max(Velocity.Size2D(), FMath::Min(Velocity.Size2D() + Push, DriftEntrySpeed));
	const float TravelYaw = FMath::RadiansToDegrees(FMath::Atan2(Velocity.Y, Velocity.X)) + TravelYawRate * DeltaSeconds;
	const FVector Travel = FRotator(0.f, TravelYaw, 0.f).Vector() * Speed2D;
	Body->SetPhysicsLinearVelocity(FVector(Travel.X, Travel.Y, Velocity.Z));
	const FVector Spin = Body->GetPhysicsAngularVelocityInDegrees();
	Body->SetPhysicsAngularVelocityInDegrees(FVector(Spin.X, Spin.Y, HeadingYawRate)); // +Z spin = yaw to the right
}

void ABNVehiclePawn::SetRearGrip(float Blend)
{
	const bool bTractionControl = !bDrifting;
	if (Blend == RearGripBlend && bTractionControl == bRearTractionControl)
	{
		return; // no physics-thread commands while nothing changes
	}
	RearGripBlend = Blend;
	bRearTractionControl = bTractionControl;
	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	for (int32 i = FirstRearWheel; i < Move->Wheels.Num(); ++i)
	{
		const UChaosVehicleWheel* Wheel = Move->Wheels[i];
		Move->SetWheelFrictionMultiplier(i, Wheel->FrictionForceMultiplier * FMath::Lerp(Drift.RearGripScale, 1.f, Blend));
		// Traction control would cut the wheelspin in a drift; back on once it is over.
		Move->SetTractionControlEnabled(i, bTractionControl && Wheel->bTractionControlEnabled);
	}
}

void ABNVehiclePawn::SetInputLocked(bool bLocked)
{
	bInputLocked = bLocked;
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

void ABNVehiclePawn::OnSteering(const FInputActionValue& Value)
{
	if (bInputLocked)
	{
		return;
	}
	GetChaosMovement()->SetSteeringInput(Value.Get<float>());
}

void ABNVehiclePawn::OnThrottle(const FInputActionValue& Value)
{
	if (bInputLocked)
	{
		return;
	}
	GetChaosMovement()->SetThrottleInput(Value.Get<float>());
}

void ABNVehiclePawn::OnBrake(const FInputActionValue& Value)
{
	if (bInputLocked)
	{
		return;
	}
	// With auto-reverse, holding brake at a standstill reverses.
	GetChaosMovement()->SetBrakeInput(Value.Get<float>());
}

void ABNVehiclePawn::OnHandbrake(const FInputActionValue& Value)
{
	if (bInputLocked)
	{
		return;
	}
	GetChaosMovement()->SetHandbrakeInput(Value.Get<bool>());
}

void ABNVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	const APlayerController* PC = GetController<APlayerController>();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Input || !Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Enhanced Input is not available, the car cannot be driven."), *GetName());
		return;
	}

	// Runs again on every possess/restart; drop the previous context instead of stacking duplicates.
	if (InputContext)
	{
		Subsystem->RemoveMappingContext(InputContext);
	}
	InputContext = NewObject<UInputMappingContext>(this);
	auto MakeAction = [this](EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this);
		Action->ValueType = Type;
		return Action;
	};
	auto MapNegated = [this](const UInputAction* Action, const FKey& Key)
	{
		InputContext->MapKey(Action, Key).Modifiers.Add(NewObject<UInputModifierNegate>(InputContext));
	};

	UInputAction* Steering = MakeAction(EInputActionValueType::Axis1D);
	UInputAction* Throttle = MakeAction(EInputActionValueType::Axis1D);
	UInputAction* Brake = MakeAction(EInputActionValueType::Axis1D);
	UInputAction* Handbrake = MakeAction(EInputActionValueType::Boolean);
	UInputAction* Reset = MakeAction(EInputActionValueType::Boolean);
	UInputAction* CameraDistance = MakeAction(EInputActionValueType::Boolean);
	UInputAction* AbandonRace = MakeAction(EInputActionValueType::Boolean);

	InputContext->MapKey(Steering, EKeys::D);
	InputContext->MapKey(Steering, EKeys::Right);
	MapNegated(Steering, EKeys::A);
	MapNegated(Steering, EKeys::Left);
	InputContext->MapKey(Steering, EKeys::Gamepad_LeftX);

	InputContext->MapKey(Throttle, EKeys::W);
	InputContext->MapKey(Throttle, EKeys::Up);
	InputContext->MapKey(Throttle, EKeys::Gamepad_RightTriggerAxis);

	InputContext->MapKey(Brake, EKeys::S);
	InputContext->MapKey(Brake, EKeys::Down);
	InputContext->MapKey(Brake, EKeys::Gamepad_LeftTriggerAxis);

	InputContext->MapKey(Handbrake, EKeys::SpaceBar);
	InputContext->MapKey(Handbrake, EKeys::Gamepad_FaceButton_Bottom);

	InputContext->MapKey(Reset, EKeys::R);
	InputContext->MapKey(Reset, EKeys::Gamepad_FaceButton_Top);

	InputContext->MapKey(CameraDistance, EKeys::C);
	InputContext->MapKey(CameraDistance, EKeys::Gamepad_RightThumbstick);

	InputContext->MapKey(AbandonRace, EKeys::BackSpace);
	InputContext->MapKey(AbandonRace, EKeys::Gamepad_Special_Left);

	Subsystem->AddMappingContext(InputContext, 0);

	// Completed fires with a zero value on release, which centres the wheel and lifts the pedals.
	for (const ETriggerEvent Event : { ETriggerEvent::Triggered, ETriggerEvent::Completed })
	{
		Input->BindAction(Steering, Event, this, &ABNVehiclePawn::OnSteering);
		Input->BindAction(Throttle, Event, this, &ABNVehiclePawn::OnThrottle);
		Input->BindAction(Brake, Event, this, &ABNVehiclePawn::OnBrake);
		Input->BindAction(Handbrake, Event, this, &ABNVehiclePawn::OnHandbrake);
	}
	Input->BindAction(Reset, ETriggerEvent::Started, this, &ABNVehiclePawn::ResetVehicle);
	Input->BindAction(CameraDistance, ETriggerEvent::Started, this, &ABNVehiclePawn::CycleChaseDistance);
	Input->BindAction(AbandonRace, ETriggerEvent::Started, this, &ABNVehiclePawn::AbandonRace);
}

void ABNVehiclePawn::AbandonRace()
{
	for (TActorIterator<ABNRaceEvent> It(GetWorld()); It; ++It)
	{
		if (It->IsRunningFor(this))
		{
			It->EndRace();
		}
	}
}
