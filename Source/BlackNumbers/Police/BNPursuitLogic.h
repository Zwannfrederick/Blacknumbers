#pragma once

#include "CoreMinimal.h"
#include "BNPursuitLogic.generated.h"

UENUM(BlueprintType)
enum class EBNPursuitPhase : uint8
{
	None,
	Pursuit,
	Evade,
	Cooldown,
	Busted,
	Escaped,
};

/** Tuning and escalation data per Heat level (1..5 normal, 6..10 Demir, 11 Special). */
USTRUCT(BlueprintType)
struct BLACKNUMBERS_API FBNHeatTierSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	FString Label = TEXT("HEAT 1");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	int32 MaxActiveUnits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	float UnitSkill = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	float MaxPursuitSpeedKmh = 175.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	float Aggression = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	float CooldownSeconds = 12.f;

	/** Cumulative pursuit seconds needed to escalate TO this level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	float HeatThresholdSeconds = 0.f;

	/** Cumulative pursuit collateral damage ($) needed to escalate TO this level early. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	int32 CollateralThresholdDollars = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	bool bRoadblocksEnabled = false;

	/** True from Heat 6 onward (after identity exposure R4, Kanon §8). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	bool bDemirInvolved = false;

	static FBNHeatTierSpec ForLevel(int32 HeatLevel);
};

struct BLACKNUMBERS_API FBNDispatchLine
{
	FString Speaker;
	FString Message;
	float RemainingSeconds = 0.f;
};

/**
 * Pure pursuit state machine: Heat 1-5 escalation (capped by MaxAllowedHeatLevel until R4 unlocks Heat 6+),
 * sight break -> Evade -> Cooldown -> Escaped / Busted, Pursuit Breakers, and Sehir Faturasi (collateral).
 * Kept free of actors so it can be unit tested directly.
 */
struct BLACKNUMBERS_API FBNPursuitState
{
	EBNPursuitPhase Phase = EBNPursuitPhase::None;
	int32 HeatLevel = 1;
	/** Default 5 per Kanon §8 until identity reveal (R4) unlocks Heat 6-10 and Special Tier (11). */
	int32 MaxAllowedHeatLevel = 5;
	float PursuitTimeSeconds = 0.f;
	/** 0..1 fill in Cooldown; remaining banner seconds in Busted / Escaped. */
	float PhaseProgress = 0.f;
	float BustMeter = 0.f;
	float EvadeMeter = 0.f;
	/** Cumulative city damage across the session (never wiped when a pursuit ends, Kanon #21). */
	int32 CostToStateDollars = 0;
	int32 PursuitCostDollars = 0;
	int32 UnitsDisabled = 0;
	int32 BreakersTriggered = 0;
	int32 Bounty = 0;
	FBNDispatchLine ActiveRadio;

	void StartPursuit(int32 InitialHeat = 1);
	void SetMaxAllowedHeat(int32 MaxHeat);
	void Update(float DeltaSeconds, float PlayerSpeedKmh, float NearestActiveUnitDistCm, int32 ActiveUnitCount);
	void RecordCollateralDamage(int32 Dollars, const TCHAR* ReasonTag = nullptr);
	void RecordUnitDisabled(int32 BonusDollars = 15000);
	void RecordPursuitBreaker(int32 CollateralDollars, int32 DisabledCount);
	void EndPursuitImmediate();
	bool IsPursuitRunning() const;
	void PostDispatch(const FString& Speaker, const FString& Message, float Duration = 4.5f);

private:
	void TryEscalateHeat();
};
