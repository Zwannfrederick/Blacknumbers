#include "Police/BNPoliceHelicopter.h"

#include "CollisionQueryParams.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Police/BNPoliceTactics.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr float BladePassHz = 21.f;
	constexpr float RotorDegPerSecond = 1260.f;
	constexpr float SightCheckInterval = 0.2f;
	constexpr float LeaveClimbCms = 1500.f;
	constexpr float LeaveSecondsToDestroy = 10.f;
	constexpr float MaxBankDeg = 14.f;
	constexpr float SearchlightNightCandelas = 9000.f;
	const TCHAR* const BodyMeshPath = TEXT("/Game/World/Meshes/SM_BN_PoliceHeli.SM_BN_PoliceHeli");
	const TCHAR* const RotorMeshPath = TEXT("/Game/World/Meshes/SM_BN_PoliceHeliRotor.SM_BN_PoliceHeliRotor");
}

UBNHeliRotorAudioComponent::UBNHeliRotorAudioComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	NumChannels = 1;
	bAutoActivate = true;
	bAllowSpatialization = true;
	bOverrideAttenuation = true;
	AttenuationOverrides.bAttenuate = true;
	AttenuationOverrides.bSpatialize = true;
	AttenuationOverrides.AttenuationShapeExtents = FVector(3000.f, 0.f, 0.f);
	AttenuationOverrides.FalloffDistance = 16000.f;
}

bool UBNHeliRotorAudioComponent::Init(int32& SampleRate)
{
	NumChannels = 1;
	SampleRateHz = static_cast<float>(SampleRate);
	return true;
}

int32 UBNHeliRotorAudioComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const float Goal = TargetGain.load(std::memory_order_relaxed);
	const float Smooth = 1.f - FMath::Exp(-1.f / (0.3f * SampleRateHz));
	for (int32 i = 0; i < NumSamples; ++i)
	{
		CurrentGain += (Goal - CurrentGain) * Smooth;
		BladePhase = FMath::Frac(BladePhase + BladePassHz / SampleRateHz);
		NoiseSeed = NoiseSeed * 1664525u + 1013904223u;
		const float White = (static_cast<float>(NoiseSeed >> 9) / 4194304.f) - 1.f;
		NoiseLowpass += (White - NoiseLowpass) * 0.08f;
		const float Pulse = FMath::Pow(0.5f + 0.5f * FMath::Cos(UE_TWO_PI * BladePhase), 6.f);
		const float Thump = FMath::Sin(UE_TWO_PI * BladePhase * 3.f) * Pulse;
		OutAudio[i] = CurrentGain * (0.55f * Thump + 1.6f * NoiseLowpass * (0.35f + Pulse));
	}
	return NumSamples;
}

ABNPoliceHelicopter::ABNPoliceHelicopter()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Rotor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rotor"));
	Rotor->SetupAttachment(Root);
	Rotor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rotor->SetCastShadow(false);

	Searchlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Searchlight"));
	Searchlight->SetupAttachment(Root);
	Searchlight->SetRelativeLocation(FVector(180.f, 0.f, -230.f));
	Searchlight->SetCastShadows(false);
	Searchlight->SetAttenuationRadius(9000.f);
	Searchlight->SetInnerConeAngle(4.f);
	Searchlight->SetOuterConeAngle(9.f);
	Searchlight->SetLightColor(FLinearColor(0.85f, 0.92f, 1.f));
	Searchlight->SetIntensity(0.f);

	RotorAudio = CreateDefaultSubobject<UBNHeliRotorAudioComponent>(TEXT("RotorAudio"));
	RotorAudio->SetupAttachment(Root);
}

void ABNPoliceHelicopter::BeginPlay()
{
	Super::BeginPlay();
	// Load meshes at runtime, not in the constructor (CDO LoadObject gotcha).
	if (UStaticMesh* BodyMesh = LoadObject<UStaticMesh>(nullptr, BodyMeshPath))
	{
		Body->SetStaticMesh(BodyMesh);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("bn.Heli: missing %s (run Tools/build_map.sh)"), BodyMeshPath);
	}
	if (UStaticMesh* RotorMesh = LoadObject<UStaticMesh>(nullptr, RotorMeshPath))
	{
		Rotor->SetStaticMesh(RotorMesh);
	}
	RotorAudio->SetGain(0.32f);
}

void ABNPoliceHelicopter::Leave()
{
	bLeaving = true;
	LeaveSeconds = 0.f;
}

void ABNPoliceHelicopter::UpdateSight()
{
	const ABNVehiclePawn* Car = Target.Get();
	UWorld* World = GetWorld();
	if (!Car || !World)
	{
		bHasSight = false;
		return;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BNHeliSight), false, this);
	Params.AddIgnoredActor(Car);
	const FVector From = GetActorLocation() + FVector(0.f, 0.f, -300.f);
	const FVector To = Car->GetActorLocation() + FVector(0.f, 0.f, 80.f);
	const bool bBlocked = World->LineTraceTestByChannel(From, To, ECC_Visibility, Params);
	bHasSight = BNPoliceTactics::HeliCanSpot(GetActorLocation(), Car->GetActorLocation(), !bBlocked);
}

void ABNPoliceHelicopter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RotorAngle = FMath::Fmod(RotorAngle + RotorDegPerSecond * DeltaSeconds, 360.f);
	Rotor->SetRelativeRotation(FRotator(0.f, RotorAngle, 0.f));

	const ABNVehiclePawn* Car = Target.Get();
	const FVector Loc = GetActorLocation();
	if (bLeaving || !Car)
	{
		LeaveSeconds += DeltaSeconds;
		SetActorLocation(Loc + (GetActorForwardVector() * BNPoliceTactics::HeliMaxSpeedCms * 0.6f + FVector(0.f, 0.f, LeaveClimbCms)) * DeltaSeconds);
		Searchlight->SetIntensity(0.f);
		RotorAudio->SetGain(FMath::Max(0.f, 0.32f * (1.f - LeaveSeconds / LeaveSecondsToDestroy)));
		if (LeaveSeconds >= LeaveSecondsToDestroy)
		{
			Destroy();
		}
		return;
	}

	const FVector NewLoc = BNPoliceTactics::HeliStep(Loc, BNPoliceTactics::HeliHoverGoal(Car->GetActorLocation(), Car->GetVelocity()), DeltaSeconds);
	Velocity = DeltaSeconds > 0.f ? (NewLoc - Loc) / DeltaSeconds : FVector::ZeroVector;
	const FVector ToCar = (Car->GetActorLocation() - NewLoc).GetSafeNormal2D();
	const float Yaw = FMath::FInterpTo(GetActorRotation().Yaw, (Velocity.Size2D() > 600.f ? Velocity : ToCar).Rotation().Yaw, DeltaSeconds, 1.6f);
	const float Pitch = -MaxBankDeg * FMath::Clamp(Velocity.Size2D() / BNPoliceTactics::HeliMaxSpeedCms, 0.f, 1.f);
	SetActorLocationAndRotation(NewLoc, FRotator(Pitch, Yaw, 0.f));

	SightCheckTimer -= DeltaSeconds;
	if (SightCheckTimer <= 0.f)
	{
		SightCheckTimer = SightCheckInterval;
		UpdateSight();
	}
	Searchlight->SetWorldRotation((Car->GetActorLocation() - Searchlight->GetComponentLocation()).Rotation());
	Searchlight->SetIntensity(Car->AreLightsOn() ? SearchlightNightCandelas : 0.f);
}
