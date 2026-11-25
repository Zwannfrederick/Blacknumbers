#include "Vehicle/BNVehiclePawn.h"

#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Garage/BNGarageZone.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Police/BNPursuitSubsystem.h"
#include "Race/BNRaceEvent.h"
#include "Story/BNStorySubsystem.h"
#include "UI/BNHUD.h"

void ABNVehiclePawn::OnSteering(const FInputActionValue& Value)
{
	bHasPlayerSteeringInput = true;
	if (bInputLocked)
	{
		PlayerSteeringTarget = 0.f;
		return;
	}
	PlayerSteeringTarget = FMath::Clamp(Value.Get<float>(), -1.f, 1.f);
}

void ABNVehiclePawn::OnThrottle(const FInputActionValue& Value)
{
	PlayerThrottleInput = FMath::Clamp(Value.Get<float>(), 0.f, 1.f);
	if (bInputLocked)
	{
		return;
	}
	GetChaosMovement()->SetThrottleInput(PlayerThrottleInput);
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

void ABNVehiclePawn::OnNos(const FInputActionValue& Value)
{
	SetNosWanted(Value.Get<bool>());
}

void ABNVehiclePawn::OnSpeedbreaker(const FInputActionValue& Value)
{
	SetSpeedbreakerWanted(Value.Get<bool>());
}

void ABNVehiclePawn::OnLookBack(const FInputActionValue& Value)
{
	SetLookBackWanted(Value.Get<bool>());
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
	UInputAction* Nos = MakeAction(EInputActionValueType::Boolean);
	UInputAction* Speedbreaker = MakeAction(EInputActionValueType::Boolean);
	UInputAction* Reset = MakeAction(EInputActionValueType::Boolean);
	UInputAction* CameraDistance = MakeAction(EInputActionValueType::Boolean);
	UInputAction* LookBack = MakeAction(EInputActionValueType::Boolean);
	UInputAction* AbandonRaceAction = MakeAction(EInputActionValueType::Boolean);
	UInputAction* PursuitToggle = MakeAction(EInputActionValueType::Boolean);
	UInputAction* GarageToggle = MakeAction(EInputActionValueType::Boolean);
	UInputAction* DossierToggle = MakeAction(EInputActionValueType::Boolean);
	UInputAction* PauseMenuToggle = MakeAction(EInputActionValueType::Boolean);
	PauseMenuToggle->bTriggerWhenPaused = true;
	UInputAction* StoryCardAdvance = MakeAction(EInputActionValueType::Boolean);
	UInputAction* WorldMapToggle = MakeAction(EInputActionValueType::Boolean);
	UInputAction* DragShiftUp = MakeAction(EInputActionValueType::Boolean);
	UInputAction* DragShiftDown = MakeAction(EInputActionValueType::Boolean);

	InputContext->MapKey(Steering, EKeys::D);
	InputContext->MapKey(Steering, EKeys::Right);
	MapNegated(Steering, EKeys::A);
	MapNegated(Steering, EKeys::Left);
	UInputModifierDeadZone* SteeringDeadZone = NewObject<UInputModifierDeadZone>(InputContext);
	SteeringDeadZone->LowerThreshold = 0.12f;
	InputContext->MapKey(Steering, EKeys::Gamepad_LeftX).Modifiers.Add(SteeringDeadZone);

	InputContext->MapKey(Throttle, EKeys::W);
	InputContext->MapKey(Throttle, EKeys::Up);
	InputContext->MapKey(Throttle, EKeys::Gamepad_RightTriggerAxis);

	InputContext->MapKey(Brake, EKeys::S);
	InputContext->MapKey(Brake, EKeys::Down);
	InputContext->MapKey(Brake, EKeys::Gamepad_LeftTriggerAxis);

	InputContext->MapKey(Handbrake, EKeys::SpaceBar);
	InputContext->MapKey(Handbrake, EKeys::Gamepad_FaceButton_Bottom);

	InputContext->MapKey(Nos, EKeys::LeftShift);
	InputContext->MapKey(Nos, EKeys::RightShift);
	InputContext->MapKey(Nos, EKeys::LeftAlt);
	InputContext->MapKey(Nos, EKeys::N);
	InputContext->MapKey(Nos, EKeys::Gamepad_FaceButton_Right);

	InputContext->MapKey(Speedbreaker, EKeys::X);
	InputContext->MapKey(Speedbreaker, EKeys::LeftControl);
	InputContext->MapKey(Speedbreaker, EKeys::Gamepad_RightShoulder);

	InputContext->MapKey(Reset, EKeys::R);
	InputContext->MapKey(Reset, EKeys::Gamepad_FaceButton_Top);

	InputContext->MapKey(CameraDistance, EKeys::C);
	InputContext->MapKey(CameraDistance, EKeys::Gamepad_RightThumbstick);

	InputContext->MapKey(LookBack, EKeys::V);
	InputContext->MapKey(LookBack, EKeys::MiddleMouseButton);
	InputContext->MapKey(LookBack, EKeys::Gamepad_LeftThumbstick);

	InputContext->MapKey(AbandonRaceAction, EKeys::BackSpace);
	InputContext->MapKey(AbandonRaceAction, EKeys::Gamepad_Special_Left);

	InputContext->MapKey(PursuitToggle, EKeys::P);
	InputContext->MapKey(PursuitToggle, EKeys::Gamepad_DPad_Up);

	InputContext->MapKey(GarageToggle, EKeys::G);
	InputContext->MapKey(GarageToggle, EKeys::Gamepad_DPad_Down);

	InputContext->MapKey(DossierToggle, EKeys::B);
	InputContext->MapKey(DossierToggle, EKeys::Gamepad_DPad_Left);

	InputContext->MapKey(PauseMenuToggle, EKeys::Escape);
	InputContext->MapKey(PauseMenuToggle, EKeys::Gamepad_Special_Right);

	InputContext->MapKey(StoryCardAdvance, EKeys::Enter);

	InputContext->MapKey(WorldMapToggle, EKeys::M);
	InputContext->MapKey(WorldMapToggle, EKeys::Tab);

	InputContext->MapKey(DragShiftUp, EKeys::E);
	InputContext->MapKey(DragShiftUp, EKeys::Gamepad_DPad_Right);

	InputContext->MapKey(DragShiftDown, EKeys::Q);
	InputContext->MapKey(DragShiftDown, EKeys::Gamepad_LeftShoulder);

	Subsystem->AddMappingContext(InputContext, 0);

	// Completed fires with a zero value on release, which centres the wheel and lifts the pedals.
	for (const ETriggerEvent Event : { ETriggerEvent::Triggered, ETriggerEvent::Completed })
	{
		Input->BindAction(Steering, Event, this, &ABNVehiclePawn::OnSteering);
		Input->BindAction(Throttle, Event, this, &ABNVehiclePawn::OnThrottle);
		Input->BindAction(Brake, Event, this, &ABNVehiclePawn::OnBrake);
		Input->BindAction(Handbrake, Event, this, &ABNVehiclePawn::OnHandbrake);
		Input->BindAction(Nos, Event, this, &ABNVehiclePawn::OnNos);
		Input->BindAction(Speedbreaker, Event, this, &ABNVehiclePawn::OnSpeedbreaker);
		Input->BindAction(LookBack, Event, this, &ABNVehiclePawn::OnLookBack);
	}
	Input->BindAction(Reset, ETriggerEvent::Started, this, &ABNVehiclePawn::ResetVehicle);
	Input->BindAction(CameraDistance, ETriggerEvent::Started, this, &ABNVehiclePawn::CycleChaseDistance);
	Input->BindAction(AbandonRaceAction, ETriggerEvent::Started, this, &ABNVehiclePawn::AbandonRace);
	Input->BindAction(PursuitToggle, ETriggerEvent::Started, this, &ABNVehiclePawn::TogglePursuit);
	Input->BindAction(GarageToggle, ETriggerEvent::Started, this, &ABNVehiclePawn::ToggleGarage);
	Input->BindAction(DossierToggle, ETriggerEvent::Started, this, &ABNVehiclePawn::ToggleStoryDossier);
	Input->BindAction(PauseMenuToggle, ETriggerEvent::Started, this, &ABNVehiclePawn::TogglePauseMenu);
	Input->BindAction(StoryCardAdvance, ETriggerEvent::Started, this, &ABNVehiclePawn::AdvanceStoryCard);
	Input->BindAction(WorldMapToggle, ETriggerEvent::Started, this, &ABNVehiclePawn::ToggleWorldMap);
	Input->BindAction(DragShiftUp, ETriggerEvent::Started, this, &ABNVehiclePawn::TriggerManualShiftUp);
	Input->BindAction(DragShiftDown, ETriggerEvent::Started, this, &ABNVehiclePawn::TriggerManualShiftDown);
}

void ABNVehiclePawn::AdvanceStoryCard()
{
	if (APlayerController* PC = GetController<APlayerController>())
	{
		if (ABNHUD* HUD = PC->GetHUD<ABNHUD>())
		{
			if (HUD->HasStoryCardOpen())
			{
				HUD->HandleMenuCloseOrAdvanceCard();
			}
			else if (!HUD->IsMenuOpen())
			{
				if (!HUD->TryOpenNearbyRaceBriefing() && ABNGarageZone::FindNearbyGarageZone(this) != nullptr)
				{
					ToggleGarage();
				}
			}
		}
	}
}

void ABNVehiclePawn::ToggleWorldMap()
{
	if (ABNRaceEvent::FindActiveRace(GetWorld()) != nullptr)
	{
		return;
	}
	if (APlayerController* PC = GetController<APlayerController>())
	{
		if (ABNHUD* HUD = PC->GetHUD<ABNHUD>())
		{
			HUD->ToggleWorldMap();
		}
	}
}

void ABNVehiclePawn::TogglePursuit()
{
	const APlayerController* PC = GetController<APlayerController>();
	const ABNHUD* HUD = PC ? PC->GetHUD<ABNHUD>() : nullptr;
	if (HUD && HUD->IsMenuOpen())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ABNRaceEvent> It(World); It; ++It)
		{
			if (It->IsRunningFor(this))
			{
				return;
			}
		}
		if (UBNPursuitSubsystem* Pursuit = World->GetSubsystem<UBNPursuitSubsystem>())
		{
			Pursuit->EscalateOrStartPursuit();
		}
	}
}

void ABNVehiclePawn::ToggleGarage()
{
	const UBNPursuitSubsystem* Pursuit = GetWorld() ? GetWorld()->GetSubsystem<UBNPursuitSubsystem>() : nullptr;
	if (Pursuit && Pursuit->GetState().IsPursuitRunning())
	{
		return;
	}
	for (TActorIterator<ABNRaceEvent> It(GetWorld()); It; ++It)
	{
		if (It->IsRunningFor(this))
		{
			return;
		}
	}
	if (APlayerController* PC = GetController<APlayerController>())
	{
		if (ABNHUD* HUD = PC->GetHUD<ABNHUD>())
		{
			HUD->ToggleMenu(EBNMenuTab::Garage);
		}
	}
}

void ABNVehiclePawn::ToggleStoryDossier()
{
	const UBNPursuitSubsystem* Pursuit = GetWorld() ? GetWorld()->GetSubsystem<UBNPursuitSubsystem>() : nullptr;
	if (Pursuit && Pursuit->GetState().IsPursuitRunning())
	{
		return;
	}
	for (TActorIterator<ABNRaceEvent> It(GetWorld()); It; ++It)
	{
		if (It->IsRunningFor(this))
		{
			return;
		}
	}
	if (APlayerController* PC = GetController<APlayerController>())
	{
		if (ABNHUD* HUD = PC->GetHUD<ABNHUD>())
		{
			HUD->ToggleMenu(EBNMenuTab::BlackNumbers);
		}
	}
}

void ABNVehiclePawn::TogglePauseMenu()
{
	if (APlayerController* PC = GetController<APlayerController>())
	{
		if (ABNHUD* HUD = PC->GetHUD<ABNHUD>())
		{
			HUD->HandleMenuCloseOrAdvanceCard();
		}
	}
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
