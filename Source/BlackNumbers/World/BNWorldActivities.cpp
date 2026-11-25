#include "World/BNWorldActivities.h"

#include "Audio/BNAudioSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Garage/BNGarageSubsystem.h"
#include "Misc/PackageName.h"
#include "Police/BNPursuitSubsystem.h"
#include "UI/BNHUD.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/BNVehiclePawn.h"

namespace
{
	constexpr float SpeedTrapCooldownSeconds = 8.f;
	constexpr float SpeedTrapFlashDuration = 0.22f;
	constexpr float StuntJumpCooldownSeconds = 6.f;
	constexpr float StuntJumpMaxAirSeconds = 6.5f;
	constexpr float StuntJumpMinMeters = 12.f;
}

ABNSpeedTrap::ABNSpeedTrap()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	PostMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PostMesh"));
	PostMesh->SetupAttachment(RootComponent);
	PostMesh->SetStaticMesh(Cyl.Object);
	PostMesh->SetRelativeLocation(FVector(0.f, 0.f, 220.f));
	PostMesh->SetRelativeScale3D(FVector(0.18f, 0.18f, 4.4f));
	PostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PostMesh->SetCastShadow(false);

	CameraHeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CameraHeadMesh"));
	CameraHeadMesh->SetupAttachment(RootComponent);
	CameraHeadMesh->SetStaticMesh(Cube.Object);
	CameraHeadMesh->SetRelativeLocation(FVector(25.f, 0.f, 440.f));
	CameraHeadMesh->SetRelativeScale3D(FVector(0.55f, 0.42f, 0.36f));
	CameraHeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CameraHeadMesh->SetCastShadow(false);

	FlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlashLight"));
	FlashLight->SetupAttachment(RootComponent);
	FlashLight->SetRelativeLocation(FVector(55.f, 0.f, 440.f));
	FlashLight->SetIntensityUnits(ELightUnits::Candelas);
	FlashLight->SetIntensity(2400.f);
	FlashLight->SetLightColor(FLinearColor(1.f, 0.96f, 0.88f));
	FlashLight->SetAttenuationRadius(1800.f);
	FlashLight->SetCastShadows(false);
	FlashLight->SetVisibility(false);
}

void ABNSpeedTrap::BeginPlay()
{
	Super::BeginPlay();
	// Swap camera head to Poly Haven CC0 security camera if imported by Tools/fetch_cc0_models.py.
	const TCHAR* CamPkg = TEXT("/Game/World/Meshes/CC0/SM_CC0_SecurityCamera");
	const TCHAR* CamAsset = TEXT("/Game/World/Meshes/CC0/SM_CC0_SecurityCamera.SM_CC0_SecurityCamera");
	if (CameraHeadMesh && FPackageName::DoesPackageExist(CamPkg))
	{
		if (UStaticMesh* CamMesh = LoadObject<UStaticMesh>(nullptr, CamAsset))
		{
			CameraHeadMesh->SetStaticMesh(CamMesh);
			CameraHeadMesh->SetRelativeScale3D(FVector(2.2f));
		}
	}
}

void ABNSpeedTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FlashRemaining > 0.f)
	{
		FlashRemaining = FMath::Max(0.f, FlashRemaining - DeltaSeconds);
		if (FlashLight)
		{
			FlashLight->SetVisibility(FlashRemaining > 0.f);
		}
	}
	if (CooldownRemaining > 0.f)
	{
		CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaSeconds);
		return;
	}

	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ABNVehiclePawn* Car = PC ? Cast<ABNVehiclePawn>(PC->GetPawn()) : nullptr;
	if (!Car || Car->IsShowcaseMode())
	{
		return;
	}

	const FVector CarLoc = Car->GetActorLocation();
	if (FMath::Abs(CarLoc.Z - GetActorLocation().Z) > 450.f)
	{
		return;
	}
	if (FVector::Dist2D(CarLoc, GetActorLocation()) > TriggerRadiusCm)
	{
		return;
	}
	const float SpeedKmh = FMath::Abs(Car->GetSpeedKmh());
	if (SpeedKmh < MinTriggerSpeedKmh)
	{
		return;
	}

	CooldownRemaining = SpeedTrapCooldownSeconds;
	FlashRemaining = SpeedTrapFlashDuration;
	if (FlashLight)
	{
		FlashLight->SetVisibility(true);
	}

	float PrevBest = 0.f;
	int32 CashGain = 0;
	int32 RepGain = 0;
	bool bNewRecord = false;
	if (UBNProgressionSubsystem* Prog = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
	{
		bNewRecord = Prog->RecordSpeedTrapResult(TrapId, DisplayName, SpeedKmh, PrevBest, CashGain, RepGain);
	}
	if (UBNPursuitSubsystem* Pursuit = World->GetSubsystem<UBNPursuitSubsystem>(); Pursuit && Pursuit->GetState().IsPursuitRunning())
	{
		Pursuit->ReportCollateralDamage(750, TEXT("Hız Radarı İhlali"));
	}
	if (UBNAudioSubsystem* Audio = World->GetSubsystem<UBNAudioSubsystem>())
	{
		Audio->PlaySfx2D(EBNSfxId::CameraFlash, 0.95f);
		if (bNewRecord)
		{
			Audio->PlaySfx2D(EBNSfxId::NewRecord, 0.85f);
		}
	}
	if (ABNHUD* HUD = PC->GetHUD<ABNHUD>())
	{
		HUD->NotifySpeedTrapTriggered(DisplayName, SpeedKmh, bNewRecord, CashGain);
	}
}

ABNStuntJump::ABNStuntJump()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	RampMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RampMesh"));
	RampMesh->SetupAttachment(RootComponent);
	RampMesh->SetStaticMesh(Cube.Object);
	RampMesh->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
	RampMesh->SetRelativeRotation(FRotator(14.f, 0.f, 0.f));
	RampMesh->SetRelativeScale3D(FVector(6.5f, 4.2f, 0.9f));
	RampMesh->SetCollisionProfileName(TEXT("BlockAll"));
	RampMesh->SetCastShadow(true);

	BeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BeaconLight"));
	BeaconLight->SetupAttachment(RootComponent);
	BeaconLight->SetRelativeLocation(FVector(260.f, 0.f, 180.f));
	BeaconLight->SetIntensityUnits(ELightUnits::Candelas);
	BeaconLight->SetIntensity(260.f);
	BeaconLight->SetLightColor(FLinearColor(1.0f, 0.62f, 0.14f));
	BeaconLight->SetAttenuationRadius(1100.f);
	BeaconLight->SetCastShadows(false);
}

void ABNStuntJump::BeginPlay()
{
	Super::BeginPlay();
	const TCHAR* RampPkg = TEXT("/Game/World/Meshes/SM_BN_JumpRamp");
	const TCHAR* RampAsset = TEXT("/Game/World/Meshes/SM_BN_JumpRamp.SM_BN_JumpRamp");
	if (RampMesh && FPackageName::DoesPackageExist(RampPkg))
	{
		if (UStaticMesh* SculptRamp = LoadObject<UStaticMesh>(nullptr, RampAsset))
		{
			RampMesh->SetStaticMesh(SculptRamp);
			RampMesh->SetRelativeLocation(FVector::ZeroVector);
			RampMesh->SetRelativeRotation(FRotator::ZeroRotator);
			RampMesh->SetRelativeScale3D(FVector::OneVector);
		}
	}
}

void ABNStuntJump::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	ABNVehiclePawn* Car = PC ? Cast<ABNVehiclePawn>(PC->GetPawn()) : nullptr;
	if (!Car || Car->IsShowcaseMode())
	{
		return;
	}

	if (CooldownRemaining > 0.f)
	{
		CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaSeconds);
	}

	if (!bTrackingAirborne)
	{
		if (CooldownRemaining > 0.f)
		{
			return;
		}
		const FVector CarLoc = Car->GetActorLocation();
		if (FMath::Abs(CarLoc.Z - GetActorLocation().Z) <= 420.f
			&& FVector::Dist2D(CarLoc, GetActorLocation()) <= TriggerRadiusCm
			&& FMath::Abs(Car->GetSpeedKmh()) >= MinLaunchSpeedKmh)
		{
			bTrackingAirborne = true;
			bSawAirborne = !Car->IsOnGround();
			LaunchOrigin = CarLoc;
			LaunchRecoveryCount = Car->GetRecoveryCount();
			TrackingElapsed = 0.f;
		}
		return;
	}

	if (Car->GetRecoveryCount() != LaunchRecoveryCount)
	{
		bTrackingAirborne = false;
		bSawAirborne = false;
		CooldownRemaining = StuntJumpCooldownSeconds;
		return;
	}

	TrackingElapsed += DeltaSeconds;
	const bool bOnGround = Car->IsOnGround();
	if (!bOnGround)
	{
		bSawAirborne = true;
		if (TrackingElapsed < StuntJumpMaxAirSeconds)
		{
			return;
		}
	}
	else if (!bSawAirborne && TrackingElapsed < 1.0f)
	{
		// Still rolling up the ramp lip before wheels leave the surface.
		LaunchOrigin = Car->GetActorLocation();
		return;
	}

	const bool bValidJump = bSawAirborne && bOnGround;
	bTrackingAirborne = false;
	bSawAirborne = false;
	CooldownRemaining = StuntJumpCooldownSeconds;
	if (!bValidJump)
	{
		return;
	}

	const float DistanceMeters = FVector::Dist2D(LaunchOrigin, Car->GetActorLocation()) * 0.01f;
	if (DistanceMeters < StuntJumpMinMeters)
	{
		return;
	}

	float PrevBest = 0.f;
	int32 CashGain = 0;
	int32 RepGain = 0;
	bool bNewRecord = false;
	if (UBNProgressionSubsystem* Prog = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UBNProgressionSubsystem>() : nullptr)
	{
		bNewRecord = Prog->RecordStuntJumpResult(JumpId, DisplayName, DistanceMeters, PrevBest, CashGain, RepGain);
	}
	if (UBNAudioSubsystem* Audio = World->GetSubsystem<UBNAudioSubsystem>())
	{
		Audio->PlaySfx2D(bNewRecord ? EBNSfxId::NewRecord : EBNSfxId::UiConfirm, 0.90f);
	}
	if (ABNHUD* HUD = PC->GetHUD<ABNHUD>())
	{
		HUD->NotifyStuntJumpLanded(DisplayName, DistanceMeters, bNewRecord, CashGain);
	}
}
