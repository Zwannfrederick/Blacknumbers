#include "Vehicle/BNVehiclePawn.h"

#include "Audio/BNAudioSubsystem.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Race/BNRaceEvent.h"
#include "Story/BNStorySubsystem.h"
#include "UI/BNHUD.h"
#include "Vehicle/BNVehicleEffects.h"

namespace
{
	constexpr float CmPerSecToKmhDrag = 0.036f;
	constexpr float DamageHitCooldownSeconds = 0.45f;
}

void ABNVehiclePawn::RepairVehicle()
{
	VehicleHealth = 1.f;
	bTotaled = false;
	bBlownEngine = false;
	TotaledSlowMoRemaining = 0.f;
	DragState.Reset(false);
	ResetTimeDilation();
}

void ABNVehiclePawn::SetDragMode(bool bEnable)
{
	DragState.Reset(bEnable);
	if (bEnable)
	{
		bBlownEngine = false;
	}
}

void ABNVehiclePawn::TriggerManualShiftUp()
{
	if (!DragState.bActive || bInputLocked || bTotaled || bBlownEngine)
	{
		return;
	}
	EBNDragShiftGrade Grade = EBNDragShiftGrade::None;
	const float ImpulseCmPerSec = DragState.ShiftUp(&Grade);
	if (Grade == EBNDragShiftGrade::None)
	{
		return;
	}

	ShiftFlashRemaining = 0.24f;
	GearShiftKick = Grade == EBNDragShiftGrade::Perfect ? 1.85f : 1.25f;
	BoostState.TriggerBackfire(Grade == EBNDragShiftGrade::Perfect ? 0.30f : 0.20f);

	if (Effects)
	{
		Effects->TriggerExhaustSparks(Grade == EBNDragShiftGrade::Perfect);
	}
	if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
	{
		const float Pitch = Grade == EBNDragShiftGrade::Perfect ? 1.12f : 0.96f;
		Audio->PlaySfx2D(EBNSfxId::ShiftPop, Grade == EBNDragShiftGrade::Perfect ? 1.0f : 0.80f, Pitch);
	}

	if (FMath::Abs(ImpulseCmPerSec) > 1.f && IsOnGround())
	{
		if (USkeletalMeshComponent* Body = GetMesh())
		{
			const FVector Vel = Body->GetPhysicsLinearVelocity();
			const FVector Fwd2D = GetActorForwardVector().GetSafeNormal2D();
			FVector NewVel2D = FVector(Vel.X, Vel.Y, 0.f) + Fwd2D * ImpulseCmPerSec;
			if (FVector::DotProduct(NewVel2D, Fwd2D) < 0.f)
			{
				NewVel2D = FVector::ZeroVector;
			}
			Body->SetPhysicsLinearVelocity(FVector(NewVel2D.X, NewVel2D.Y, Vel.Z));
		}
	}
}

void ABNVehiclePawn::TriggerManualShiftDown()
{
	if (!DragState.bActive || bInputLocked || bTotaled || bBlownEngine)
	{
		return;
	}
	if (DragState.ShiftDown())
	{
		ShiftFlashRemaining = 0.18f;
		GearShiftKick = 0.85f;
		if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
		{
			Audio->PlaySfx2D(EBNSfxId::ShiftPop, 0.65f, 0.82f);
		}
	}
}

void ABNVehiclePawn::UpdateDamageAndDrag(float DeltaSeconds, float RealDt, float SpeedKmh)
{
	ShiftFlashRemaining = FMath::Max(0.f, ShiftFlashRemaining - RealDt);

	// Totaled slow-motion dramatic beat in free roam -> open ARAÇ PERT overlay when beat completes.
	if (TotaledSlowMoRemaining > 0.f && IsPlayerControlled())
	{
		TotaledSlowMoRemaining = FMath::Max(0.f, TotaledSlowMoRemaining - RealDt);
		if (TotaledSlowMoRemaining > 0.f)
		{
			UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.35f);
			bOwnsTimeDilation = true;
		}
		else
		{
			ResetTimeDilation();
			const bool bInRace = ABNRaceEvent::FindActiveRace(GetWorld()) != nullptr;
			if (!bInRace)
			{
				if (APlayerController* PC = GetController<APlayerController>())
				{
					if (ABNHUD* HUD = PC->GetHUD<ABNHUD>(); HUD && !HUD->IsMenuOpen())
					{
						HUD->ShowFreeRoamTotaledOverlay();
					}
				}
			}
		}
	}

	if (!DragState.bActive || bInputLocked || bTotaled)
	{
		return;
	}

	const bool bWasBlown = DragState.bEngineBlown;
	DragState.Tick(DeltaSeconds, SpeedKmh, PlayerThrottleInput);

	if (!bWasBlown && DragState.bEngineBlown)
	{
		bBlownEngine = true;
		SetInputLocked(true);
		AddCameraImpulse(1.15f);
		if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
		{
			Audio->PlaySfx2D(EBNSfxId::EngineBlown, 1.0f);
		}
		if (UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
		{
			Story->NotifyTotaledOrBlown(true);
		}
		return;
	}

	// Enforce current manual gear speed ceiling so player must upshift to accelerate down the Drag strip.
	if (IsOnGround())
	{
		const float CapCmPerSec = DragState.GetCurrentGearMaxSpeedKmh() / CmPerSecToKmhDrag;
		if (USkeletalMeshComponent* Body = GetMesh())
		{
			const FVector Vel = Body->GetPhysicsLinearVelocity();
			const float Speed2D = Vel.Size2D();
			if (Speed2D > CapCmPerSec)
			{
				const FVector Clamped2D = FVector(Vel.X, Vel.Y, 0.f).GetSafeNormal() * FMath::FInterpTo(Speed2D, CapCmPerSec, DeltaSeconds, 10.f);
				Body->SetPhysicsLinearVelocity(FVector(Clamped2D.X, Clamped2D.Y, Vel.Z));
			}
		}
	}
}

void ABNVehiclePawn::ApplyCollisionDamage(float ImpulseMag, float SpeedKmh, bool bWallOrVehicleHit)
{
	if (!IsPlayerControlled() || bTotaled || !bWallOrVehicleHit || GIsAutomationTesting || FApp::IsUnattended())
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	if (Now - LastDamageHitTime < DamageHitCooldownSeconds)
	{
		return;
	}

	const float AbsSpeed = FMath::Abs(SpeedKmh);
	const bool bDrag = DragState.bActive;
	const float MinImpulse = bDrag ? 250000.f : 310000.f;
	const float MinSpeed = bDrag ? 125.f : 95.f;
	if (ImpulseMag < MinImpulse || AbsSpeed < MinSpeed)
	{
		return;
	}

	LastDamageHitTime = Now;
	const float ImpulseFactor = FMath::Clamp((ImpulseMag - MinImpulse) / 380000.f, 0.15f, 1.0f);
	const float SpeedFactor = FMath::Clamp((AbsSpeed - MinSpeed) / 130.f, 0.25f, 1.4f);
	const float Damage = FMath::Clamp(ImpulseFactor * SpeedFactor * (bDrag ? 0.85f : 0.44f), 0.12f, bDrag ? 1.0f : 0.56f);
	VehicleHealth = FMath::Clamp(VehicleHealth - Damage, 0.f, 1.f);

	if (VehicleHealth <= 0.001f)
	{
		VehicleHealth = 0.f;
		bTotaled = true;
		TotaledSlowMoRemaining = 1.15f;
		SetInputLocked(true);
		AddCameraImpulse(1.35f);
		if (UBNAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UBNAudioSubsystem>() : nullptr)
		{
			Audio->PlaySfx2D(EBNSfxId::EngineBlown, 0.95f, 0.88f);
		}
		if (UBNStorySubsystem* Story = GetGameInstance() ? GetGameInstance()->GetSubsystem<UBNStorySubsystem>() : nullptr)
		{
			Story->NotifyTotaledOrBlown(false);
		}
	}
}
