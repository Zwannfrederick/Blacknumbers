#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BNBreakableProp.generated.h"

class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FHitResult;

UENUM(BlueprintType)
enum class EBNBreakableKind : uint8
{
	Barrier,
	Bin,
	Crate,
};

/**
 * Collapsible Karamara street prop (road barrier, trash bin, industrial crate).
 * Stays static until hit at speed, then launches dynamically with vehicle collision disabled
 * and reports CollateralCostDollars to Sehir Faturasi (Kanon #21).
 */
UCLASS()
class BLACKNUMBERS_API ABNBreakableProp : public AActor
{
	GENERATED_BODY()

public:
	ABNBreakableProp();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Breakable")
	void BreakProp(const FVector& ImpactVelocity, bool bByPlayer = true);

	UFUNCTION(BlueprintPure, Category = "Breakable")
	bool IsBroken() const { return bBroken; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PropMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	EBNBreakableKind PropKind = EBNBreakableKind::Barrier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable", meta = (ClampMin = "100"))
	int32 CollateralCostDollars = 750;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable", meta = (ClampMin = "0"))
	float MinBreakSpeedKmh = 20.f;

private:
	UFUNCTION()
	void OnPropHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CylinderMesh;

	bool bBroken = false;
	float BrokenSeconds = 0.f;
};
