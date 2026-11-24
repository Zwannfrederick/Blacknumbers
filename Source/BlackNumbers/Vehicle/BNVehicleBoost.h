#pragma once

#include "CoreMinimal.h"
#include "BNVehicleBoost.generated.h"

/**
 * Tuning for Most Wanted-style tactical adrenaline mechanics: NOS, Speedbreaker slow-motion,
 * and countdown Perfect Launch. Kept actor-free so unit tests verify the math directly.
 */
USTRUCT(BlueprintType)
struct BLACKNUMBERS_API FBNBoostTuning
{
	GENERATED_BODY()

	/** Seconds of continuous NOS boost stored in a full tank. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NOS", meta = (ClampMin = "0.5", Units = "Seconds"))
	float NosMaxSeconds = 4.5f;

	/** Minimum tank fraction required to start firing NOS (prevents stuttering at empty). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NOS", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float NosMinActivateFraction = 0.1f;

	/** Forward boost acceleration applied while NOS fires (cm/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NOS", meta = (ClampMin = "0"))
	float NosAccelCmPerSec2 = 750.f;

	/** Top speed cap for NOS/Launch velocity assist (km/h). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NOS", meta = (ClampMin = "50"))
	float NosMaxSpeedKmh = 275.f;

	/** Passive tank fraction recharged per second whenever NOS is not firing (classic NFS auto-refill). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NOS", meta = (ClampMin = "0"))
	float NosRechargePassivePerSecond = 0.11f;

	/** Extra tank fraction recharged per second while drifting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NOS", meta = (ClampMin = "0"))
	float NosRechargeDriftPerSecond = 0.16f;

	/** Tank fraction recharged per second above NosRechargeMinSpeedKmh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NOS", meta = (ClampMin = "0"))
	float NosRechargeHighSpeedPerSecond = 0.045f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NOS", meta = (ClampMin = "0"))
	float NosRechargeMinSpeedKmh = 140.f;

	/** Instant NOS tank bonus for a clean near-miss past another car. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NOS", meta = (ClampMin = "0", ClampMax = "1"))
	float NosRechargeNearMissBonus = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NOS", meta = (ClampMin = "0", Units = "Degrees"))
	float NosFovBonus = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NOS", meta = (ClampMin = "0", Units = "Centimeters"))
	float NosArmPullback = 95.f;

	/** Seconds of real-time Speedbreaker slow-motion stored in a full gauge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speedbreaker", meta = (ClampMin = "0.5", Units = "Seconds"))
	float SpeedbreakerMaxSeconds = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speedbreaker", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float SpeedbreakerMinActivateFraction = 0.12f;

	/** World time dilation while Speedbreaker is engaged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speedbreaker", meta = (ClampMin = "0.15", ClampMax = "0.95"))
	float SpeedbreakerTimeDilation = 0.38f;

	/** Gauge fraction recharged per real second while driving normally above MinRechargeSpeedKmh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speedbreaker", meta = (ClampMin = "0"))
	float SpeedbreakerRechargePerSecond = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speedbreaker", meta = (ClampMin = "0"))
	float SpeedbreakerMinRechargeSpeedKmh = 40.f;

	/** Extra lateral velocity alignment rate (deg/s) while Speedbreaker is active so tight evasive lines hold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speedbreaker", meta = (ClampMin = "0"))
	float SpeedbreakerGripAssistDegPerSec = 150.f;

	/** Countdown rev window [Min, Max] that triggers a Perfect Launch when GO drops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ClampMax = "1"))
	float LaunchSweetSpotMin = 0.58f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", ClampMax = "1"))
	float LaunchSweetSpotMax = 0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0", Units = "Seconds"))
	float LaunchBoostDuration = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0"))
	float LaunchBoostAccelCmPerSec2 = 550.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0.1"))
	float LaunchRevRiseRate = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Launch", meta = (ClampMin = "0.1"))
	float LaunchRevFallRate = 1.6f;
};

/** Pure runtime state for NOS, Speedbreaker, countdown Perfect Launch, Near-Miss, and exhaust backfire. */
struct BLACKNUMBERS_API FBNBoostState
{
	float NosFraction = 1.f;
	bool bNosActive = false;

	float SpeedbreakerFraction = 1.f;
	bool bSpeedbreakerActive = false;

	float LaunchRevFraction = 0.f;
	float LaunchBoostRemaining = 0.f;

	float LaunchBannerRemaining = 0.f;
	float NearMissBannerRemaining = 0.f;
	float BackfireFlashRemaining = 0.f;

	float NosBlend = 0.f;
	float SpeedbreakerBlend = 0.f;

	void UpdateNos(float RealDt, bool bWanted, bool bInputLocked, bool bOnGround, float SpeedKmh, bool bDrifting, const FBNBoostTuning& Tuning);
	void UpdateSpeedbreaker(float RealDt, bool bWanted, bool bInputLocked, float SpeedKmh, const FBNBoostTuning& Tuning);
	void UpdateCountdownLaunch(float RealDt, float PlayerThrottle, const FBNBoostTuning& Tuning);
	bool TriggerLaunchRelease(const FBNBoostTuning& Tuning);
	void AddNearMissBonus(const FBNBoostTuning& Tuning);
	void TriggerBackfire(float Duration = 0.15f);
	void TickTimers(float RealDt);
	bool IsInLaunchSweetSpot(const FBNBoostTuning& Tuning) const;
};
