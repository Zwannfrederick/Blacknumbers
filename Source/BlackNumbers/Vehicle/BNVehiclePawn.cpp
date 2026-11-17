#include "Vehicle/BNVehiclePawn.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/BNVehicleWheels.h"

namespace
{
	constexpr float CmPerSecToKmh = 0.036f;
	constexpr float AirborneAngularDamping = 3.f;

	void SetupSportsCarMovement(UChaosWheeledVehicleMovementComponent& Move)
	{
		Move.ChassisHeight = 144.f;
		Move.DragCoefficient = 0.31f;
		Move.bLegacyWheelFrictionPosition = true;

		const TPair<FName, TSubclassOf<UChaosVehicleWheel>> Wheels[] = {
			{ TEXT("Phys_Wheel_FL"), UBNWheelFront::StaticClass() },
			{ TEXT("Phys_Wheel_FR"), UBNWheelFront::StaticClass() },
			{ TEXT("Phys_Wheel_BL"), UBNWheelRear::StaticClass() },
			{ TEXT("Phys_Wheel_BR"), UBNWheelRear::StaticClass() },
		};
		Move.WheelSetups.SetNum(UE_ARRAY_COUNT(Wheels));
		for (int32 i = 0; i < UE_ARRAY_COUNT(Wheels); ++i)
		{
			Move.WheelSetups[i].BoneName = Wheels[i].Key;
			Move.WheelSetups[i].WheelClass = Wheels[i].Value;
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
	}
}

ABNVehiclePawn::ABNVehiclePawn()
{
	// ponytail: hard-coded placeholder car from the engine's vehicle template; move to a data asset per car when real cars exist.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CarMesh(TEXT("/Game/Vehicles/SportsCar/SKM_SportsCar.SKM_SportsCar"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> CarAnim(TEXT("/Game/Vehicles/SportsCar/ABP_SportsCar"));

	USkeletalMeshComponent* Body = GetMesh();
	Body->SetSkeletalMesh(CarMesh.Object);
	Body->SetAnimInstanceClass(CarAnim.Class);
	Body->SetSimulatePhysics(true);
	Body->SetCollisionProfileName(TEXT("Vehicle"));

	SetupSportsCarMovement(*GetChaosMovement());

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Body);
	CameraBoom->TargetArmLength = 650.f;
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
	GetMesh()->SetAngularDamping(GetChaosMovement()->IsMovingOnGround() ? 0.f : AirborneAngularDamping);

	Camera->SetFieldOfView(FMath::GetMappedRangeValueClamped(
		FVector2f(0.f, FovMaxSpeedKmh), FVector2f(BaseFov, MaxFov), FMath::Abs(GetSpeedKmh())));
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
	GetChaosMovement()->SetSteeringInput(Value.Get<float>());
}

void ABNVehiclePawn::OnThrottle(const FInputActionValue& Value)
{
	GetChaosMovement()->SetThrottleInput(Value.Get<float>());
}

void ABNVehiclePawn::OnBrake(const FInputActionValue& Value)
{
	// With auto-reverse, holding brake at a standstill reverses.
	GetChaosMovement()->SetBrakeInput(Value.Get<float>());
}

void ABNVehiclePawn::OnHandbrake(const FInputActionValue& Value)
{
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
}
