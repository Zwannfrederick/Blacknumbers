#include "Vehicle/BNVehicleVisuals.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include <initializer_list>

namespace
{
	const TCHAR* const OffroadPackagePath = TEXT("/Game/Vehicles/OffroadCar/SM_Offroad_Body");
	const TCHAR* const OffroadAssetPath = TEXT("/Game/Vehicles/OffroadCar/SM_Offroad_Body.SM_Offroad_Body");
	const TCHAR* const ConceptPackagePath = TEXT("/Game/Vehicles/ConceptCar/SM_AutomotiveTP_Car");
	const TCHAR* const ConceptAssetPath = TEXT("/Game/Vehicles/ConceptCar/SM_AutomotiveTP_Car.SM_AutomotiveTP_Car");

	const TCHAR* const BasicCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const BasicCylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* const BasicShapeMatPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	constexpr float TargetCarLengthCm = 468.f;
	constexpr float TargetCarCenterXCm = 12.f;
	constexpr float SportsCarDefaultTopZ = 128.f;
	constexpr float NosFlameCandelas = 120.f;
	constexpr float BackfireFlameCandelas = 180.f;
	const FLinearColor NosFlameColour(0.2f, 0.75f, 1.f);
	const FLinearColor BackfireFlameColour(1.f, 0.42f, 0.08f);

	void SetVectorIfPresent(UMaterialInstanceDynamic* MID, FName ParamName, const FLinearColor& Value)
	{
		if (!MID)
		{
			return;
		}
		FLinearColor Existing;
		if (MID->GetVectorParameterValue(FMaterialParameterInfo(ParamName), Existing))
		{
			MID->SetVectorParameterValue(ParamName, Value);
		}
	}

	void SetScalarIfPresent(UMaterialInstanceDynamic* MID, FName ParamName, float Value)
	{
		if (!MID)
		{
			return;
		}
		float Existing = 0.f;
		if (MID->GetScalarParameterValue(FMaterialParameterInfo(ParamName), Existing))
		{
			MID->SetScalarParameterValue(ParamName, Value);
		}
	}

	bool MeshHasBakedTires(const UStaticMesh* Mesh)
	{
		if (!Mesh)
		{
			return false;
		}
		for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
		{
			const FString SlotName = Slot.MaterialSlotName.ToString();
			const FString MatName = Slot.MaterialInterface ? Slot.MaterialInterface->GetName() : FString();
			if (SlotName.Contains(TEXT("Tire")) || MatName.Contains(TEXT("Tire"))
				|| SlotName.Contains(TEXT("Wheel")) || MatName.Contains(TEXT("Wheel")))
			{
				return true;
			}
		}
		return false;
	}

	int32 FindPaintMaterialSlot(const UStaticMeshComponent* ChassisComp)
	{
		if (!ChassisComp || ChassisComp->GetNumMaterials() <= 0)
		{
			return INDEX_NONE;
		}
		for (int32 i = 0; i < ChassisComp->GetNumMaterials(); ++i)
		{
			const UMaterialInterface* Mat = ChassisComp->GetMaterial(i);
			if (!Mat)
			{
				continue;
			}
			const FString Name = Mat->GetName();
			if (Name.Contains(TEXT("SportsCarBody")) || Name.Contains(TEXT("OffroadCar_Main"))
				|| Name.Contains(TEXT("CarPaint")) || Name.Contains(TEXT("Body")))
			{
				return i;
			}
		}
		return 0;
	}
}

UBNVehicleVisualsComponent::UBNVehicleVisualsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBNVehicleVisualsComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	ClearAeroParts();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UBNVehicleVisualsComponent::UpdateExhaustLights(const TArray<TObjectPtr<UPointLightComponent>>& ExhaustLights,
	bool bNosActive, bool bBackfireActive, float RealTimeSeconds)
{
	if (!bNosActive && !bBackfireActive)
	{
		for (UPointLightComponent* Light : ExhaustLights)
		{
			if (Light)
			{
				Light->SetVisibility(false);
			}
		}
		return;
	}
	const float Flicker = 0.85f + 0.15f * FMath::Sin(RealTimeSeconds * 48.f);
	const FLinearColor Color = bNosActive ? NosFlameColour : BackfireFlameColour;
	const float Intensity = (bNosActive ? NosFlameCandelas : BackfireFlameCandelas) * Flicker;
	for (UPointLightComponent* Light : ExhaustLights)
	{
		if (Light)
		{
			Light->SetLightColor(Color);
			Light->SetIntensity(Intensity);
			Light->SetVisibility(true);
		}
	}
}

UStaticMesh* UBNVehicleVisualsComponent::ResolveBodyMesh(EBNBodyStyle Style, bool& bOutIsCustom)
{
	bOutIsCustom = false;
	const TCHAR* PackagePath = nullptr;
	const TCHAR* AssetPath = nullptr;

	if (Style == EBNBodyStyle::RallyOffroad)
	{
		PackagePath = OffroadPackagePath;
		AssetPath = OffroadAssetPath;
	}
	else if (Style == EBNBodyStyle::HyperConcept)
	{
		PackagePath = ConceptPackagePath;
		AssetPath = ConceptAssetPath;
	}

	if (PackagePath && AssetPath && FPackageName::DoesPackageExist(PackagePath))
	{
		if (UStaticMesh* Loaded = LoadObject<UStaticMesh>(nullptr, AssetPath))
		{
			bOutIsCustom = true;
			return Loaded;
		}
	}
	return DefaultSportsCarMesh.Get();
}

void UBNVehicleVisualsComponent::ConfigureChassisTransform(UStaticMeshComponent* ChassisComp, UStaticMesh* Mesh,
	EBNBodyStyle Style, bool bIsCustom, UStaticMeshComponent* GlassComp,
	const TArray<TObjectPtr<UStaticMeshComponent>>& WheelMeshes, float& OutBodyTopZ)
{
	OutBodyTopZ = SportsCarDefaultTopZ;
	if (!ChassisComp || !Mesh)
	{
		return;
	}

	if (ChassisComp->GetStaticMesh() != Mesh)
	{
		// Drop previous mesh's dynamic material overrides before swapping meshes.
		ChassisComp->EmptyOverrideMaterials();
		BodyPaintMID = nullptr;
		ChassisComp->SetStaticMesh(Mesh);
	}

	bool bShowSeparateWheels = true;
	if (!bIsCustom)
	{
		ChassisComp->SetRelativeLocation(FVector::ZeroVector);
		ChassisComp->SetRelativeRotation(FRotator::ZeroRotator);
		ChassisComp->SetRelativeScale3D(FVector::OneVector);
		if (GlassComp)
		{
			GlassComp->SetVisibility(true);
		}
	}
	else
	{
		const FBox Bounds = Mesh->GetBoundingBox();
		const FVector Size = Bounds.IsValid ? Bounds.GetSize() : FVector(TargetCarLengthCm, 200.f, 130.f);
		const FVector Center = Bounds.IsValid ? Bounds.GetCenter() : FVector::ZeroVector;

		const bool bFacingY = Size.Y > Size.X * 1.15f;
		const float MeshLength = FMath::Max(bFacingY ? Size.Y : Size.X, 100.f);
		const float UniformScale = FMath::Clamp(TargetCarLengthCm / MeshLength, 0.72f, 1.35f);
		const FRotator MeshRot = bFacingY ? FRotator(0.f, -90.f, 0.f) : FRotator::ZeroRotator;

		const bool bHasBakedTires = MeshHasBakedTires(Mesh);
		bShowSeparateWheels = !bHasBakedTires;
		const float DesiredBottomZ = bHasBakedTires ? 2.f : (Style == EBNBodyStyle::RallyOffroad ? 28.f : 20.f);
		const FVector RotCenter = MeshRot.RotateVector(Center * UniformScale);
		const float OffsetX = TargetCarCenterXCm - RotCenter.X;
		const float OffsetY = -RotCenter.Y;
		const float OffsetZ = DesiredBottomZ - (Bounds.IsValid ? Bounds.Min.Z * UniformScale : 0.f);

		ChassisComp->SetRelativeLocation(FVector(OffsetX, OffsetY, OffsetZ));
		ChassisComp->SetRelativeRotation(MeshRot);
		ChassisComp->SetRelativeScale3D(FVector(UniformScale));
		OutBodyTopZ = Bounds.IsValid ? FMath::Clamp(Bounds.Max.Z * UniformScale + OffsetZ, 112.f, 185.f) : SportsCarDefaultTopZ;

		if (GlassComp)
		{
			GlassComp->SetVisibility(false);
		}
	}

	// Never alter WheelMeshes relative rotations so BlackNumbers.Vehicle.WheelsAligned stays exact.
	for (UStaticMeshComponent* Wheel : WheelMeshes)
	{
		if (Wheel)
		{
			Wheel->SetVisibility(bShowSeparateWheels);
		}
	}
}

void UBNVehicleVisualsComponent::ApplyCarVisuals(const FBNVehicleSpec& Spec, UStaticMeshComponent* ChassisComp,
	UStaticMeshComponent* GlassComp, const TArray<TObjectPtr<UStaticMeshComponent>>& WheelMeshes)
{
	if (!DefaultSportsCarMesh && ChassisComp)
	{
		DefaultSportsCarMesh = ChassisComp->GetStaticMesh();
	}

	bool bIsCustom = false;
	UStaticMesh* TargetMesh = ResolveBodyMesh(Spec.BodyStyle, bIsCustom);
	bUsingCustomBodyMesh = bIsCustom;
	AppliedBodyStyle = Spec.BodyStyle;

	float BodyTopZ = SportsCarDefaultTopZ;
	ConfigureChassisTransform(ChassisComp, TargetMesh, Spec.BodyStyle, bIsCustom, GlassComp, WheelMeshes, BodyTopZ);
	ApplyPaintFinish(Spec.PaintTint, Spec.Metallic, Spec.ClearCoat, ChassisComp);

	const bool bNeedAeroRebuild = !bHasBuiltAeroKit || AppliedAeroKit != Spec.AeroKit
		|| !FMath::IsNearlyEqual(LastBodyTopZ, BodyTopZ, 1.f);
	LastBodyTopZ = BodyTopZ;
	EnsureAeroAssets(Spec);
	if (bNeedAeroRebuild)
	{
		RebuildAeroKit(Spec, BodyTopZ);
		AppliedAeroKit = Spec.AeroKit;
		bHasBuiltAeroKit = true;
	}
}

void UBNVehicleVisualsComponent::ApplyPaintTint(const FLinearColor& PaintTint, UStaticMeshComponent* ChassisComp)
{
	ApplyPaintFinish(PaintTint, LastMetallic, LastClearCoat, ChassisComp);
}

void UBNVehicleVisualsComponent::ApplyPaintFinish(const FLinearColor& PaintTint, float Metallic, float ClearCoat,
	UStaticMeshComponent* ChassisComp)
{
	LastMetallic = FMath::Clamp(Metallic, 0.f, 1.f);
	LastClearCoat = FMath::Clamp(ClearCoat, 0.f, 1.f);
	if (!ChassisComp)
	{
		return;
	}
	if (!DefaultSportsCarMesh)
	{
		DefaultSportsCarMesh = ChassisComp->GetStaticMesh();
	}

	if (!BodyPaintMID)
	{
		const int32 PaintSlot = FindPaintMaterialSlot(ChassisComp);
		if (PaintSlot != INDEX_NONE)
		{
			if (UMaterialInterface* Mat = ChassisComp->GetMaterial(PaintSlot))
			{
				BodyPaintMID = ChassisComp->CreateDynamicMaterialInstance(PaintSlot, Mat);
			}
		}
	}
	if (!BodyPaintMID)
	{
		return;
	}

	// Support UE5 SportsCar/Offroad (Paint Tint), ConceptCar (PaintColor), and custom materials (BaseColor).
	SetVectorIfPresent(BodyPaintMID, TEXT("Paint Tint"), PaintTint);
	SetVectorIfPresent(BodyPaintMID, TEXT("PaintColor"), PaintTint);
	SetVectorIfPresent(BodyPaintMID, TEXT("BaseColor"), PaintTint);

	const float CoatRough = FMath::Lerp(0.26f, 0.04f, LastClearCoat);
	const float BaseRough = FMath::Lerp(0.42f, 0.14f, LastClearCoat);
	SetScalarIfPresent(BodyPaintMID, TEXT("Metallic"), LastMetallic);
	SetScalarIfPresent(BodyPaintMID, TEXT("MetalPaintMetallic"), LastMetallic);
	SetScalarIfPresent(BodyPaintMID, TEXT("ClearCoat"), LastClearCoat);
	SetScalarIfPresent(BodyPaintMID, TEXT("ClearCoatRoughness"), CoatRough);
	SetScalarIfPresent(BodyPaintMID, TEXT("Roughness"), BaseRough);
	SetScalarIfPresent(BodyPaintMID, TEXT("BaseRough"), BaseRough);
	SetScalarIfPresent(BodyPaintMID, TEXT("FlakeIntensity"), FMath::Lerp(0.15f, 0.85f, LastMetallic));
}

void UBNVehicleVisualsComponent::EnsureAeroAssets(const FBNVehicleSpec& Spec)
{
	if (!CubeMesh)
	{
		CubeMesh = LoadObject<UStaticMesh>(nullptr, BasicCubePath);
	}
	if (!CylinderMesh)
	{
		CylinderMesh = LoadObject<UStaticMesh>(nullptr, BasicCylinderPath);
	}
	if (!AeroBaseMaterial)
	{
		AeroBaseMaterial = LoadObject<UMaterialInterface>(nullptr, BasicShapeMatPath);
	}
	if (!AeroBaseMaterial)
	{
		return;
	}

	if (!AeroCarbonMID)
	{
		AeroCarbonMID = UMaterialInstanceDynamic::Create(AeroBaseMaterial, this);
		SetVectorIfPresent(AeroCarbonMID, TEXT("Color"), FLinearColor(0.035f, 0.038f, 0.045f, 1.f));
		SetScalarIfPresent(AeroCarbonMID, TEXT("Roughness"), 0.24f);
	}
	if (!AeroAccentMID)
	{
		AeroAccentMID = UMaterialInstanceDynamic::Create(AeroBaseMaterial, this);
	}
	SetVectorIfPresent(AeroAccentMID, TEXT("Color"), Spec.AccentTint);
	SetScalarIfPresent(AeroAccentMID, TEXT("Roughness"), FMath::Lerp(0.32f, 0.14f, LastClearCoat));

	if (!ExhaustChromeMID)
	{
		ExhaustChromeMID = UMaterialInstanceDynamic::Create(AeroBaseMaterial, this);
		SetVectorIfPresent(ExhaustChromeMID, TEXT("Color"), FLinearColor(0.38f, 0.40f, 0.44f, 1.f));
		SetScalarIfPresent(ExhaustChromeMID, TEXT("Roughness"), 0.20f);
	}
}

void UBNVehicleVisualsComponent::ClearAeroParts()
{
	for (UStaticMeshComponent* Part : AeroParts)
	{
		if (IsValid(Part))
		{
			Part->DestroyComponent();
		}
	}
	AeroParts.Reset();
}

UStaticMeshComponent* UBNVehicleVisualsComponent::AddAeroPart(const TCHAR* Name, UStaticMesh* Mesh,
	const FVector& Loc, const FRotator& Rot, const FVector& Scale, UMaterialInterface* Mat, bool bCastShadow)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Mesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Owner, MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), Name));
	Part->SetupAttachment(this);
	Part->SetStaticMesh(Mesh);
	Part->SetRelativeLocation(Loc);
	Part->SetRelativeRotation(Rot);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCastShadow(bCastShadow);
	if (Mat)
	{
		Part->SetMaterial(0, Mat);
	}
	if (GetWorld() && !Part->IsRegistered())
	{
		Part->RegisterComponent();
	}
	AeroParts.Add(Part);
	return Part;
}

void UBNVehicleVisualsComponent::RebuildAeroKit(const FBNVehicleSpec& Spec, float BodyTopZ)
{
	ClearAeroParts();
	if (!CubeMesh || !CylinderMesh)
	{
		return;
	}

	auto AddExhaustPair = [&](float X, float HalfSpanY, float Z, float RadiusScale)
	{
		for (const float Side : { -1.f, 1.f })
		{
			AddAeroPart(TEXT("AeroExhaust"), CylinderMesh, FVector(X, Side * HalfSpanY, Z),
				FRotator(90.f, 0.f, 0.f), FVector(RadiusScale, RadiusScale, 0.25f), ExhaustChromeMID, false);
		}
	};

	auto AddDiffuserFins = [&](float X, float Z, const std::initializer_list<float>& OffsetsY)
	{
		AddAeroPart(TEXT("AeroDiffuserTray"), CubeMesh, FVector(X + 6.f, 0.f, Z - 2.f),
			FRotator::ZeroRotator, FVector(0.24f, 1.18f, 0.035f), AeroCarbonMID, false);
		for (const float Y : OffsetsY)
		{
			AddAeroPart(TEXT("AeroDiffuserFin"), CubeMesh, FVector(X, Y, Z),
				FRotator::ZeroRotator, FVector(0.26f, 0.032f, 0.14f), AeroCarbonMID, false);
		}
	};

	switch (Spec.AeroKit)
	{
	case EBNAeroKit::StreetLip:
		AddAeroPart(TEXT("StreetFrontLip"), CubeMesh, FVector(242.f, 0.f, 24.f), FRotator::ZeroRotator, FVector(0.24f, 1.88f, 0.045f), AeroCarbonMID);
		AddAeroPart(TEXT("StreetDucktail"), CubeMesh, FVector(-208.f, 0.f, 92.f), FRotator(16.f, 0.f, 0.f), FVector(0.20f, 1.54f, 0.055f), AeroCarbonMID);
		AddExhaustPair(-226.f, 44.f, 34.f, 0.11f);
		break;

	case EBNAeroKit::RallyWing:
		AddAeroPart(TEXT("RallySplitter"), CubeMesh, FVector(244.f, 0.f, 23.f), FRotator::ZeroRotator, FVector(0.28f, 1.94f, 0.05f), AeroCarbonMID);
		for (const float Side : { -1.f, 1.f })
		{
			AddAeroPart(TEXT("RallyPylon"), CubeMesh, FVector(-198.f, Side * 56.f, 108.f), FRotator::ZeroRotator, FVector(0.14f, 0.04f, 0.36f), AeroCarbonMID);
			AddAeroPart(TEXT("RallyEndplate"), CubeMesh, FVector(-202.f, Side * 91.f, 127.f), FRotator::ZeroRotator, FVector(0.34f, 0.03f, 0.22f), AeroAccentMID);
		}
		AddAeroPart(TEXT("RallyWingBlade"), CubeMesh, FVector(-202.f, 0.f, 127.f), FRotator(-4.f, 0.f, 0.f), FVector(0.28f, 1.82f, 0.045f), AeroAccentMID);
		AddDiffuserFins(-222.f, 25.f, { -36.f, 0.f, 36.f });
		AddExhaustPair(-226.f, 52.f, 33.f, 0.12f);
		break;

	case EBNAeroKit::MuscleBlower:
		AddAeroPart(TEXT("BlowerBlock"), CubeMesh, FVector(120.f, 0.f, 96.f), FRotator::ZeroRotator, FVector(0.60f, 0.42f, 0.18f), ExhaustChromeMID);
		AddAeroPart(TEXT("BlowerScoop"), CubeMesh, FVector(126.f, 0.f, 109.f), FRotator::ZeroRotator, FVector(0.46f, 0.34f, 0.10f), AeroAccentMID);
		AddAeroPart(TEXT("MuscleChin"), CubeMesh, FVector(240.f, 0.f, 24.f), FRotator::ZeroRotator, FVector(0.24f, 1.90f, 0.075f), AeroCarbonMID);
		AddAeroPart(TEXT("MuscleDucktail"), CubeMesh, FVector(-210.f, 0.f, 94.f), FRotator(20.f, 0.f, 0.f), FVector(0.26f, 1.66f, 0.065f), AeroAccentMID);
		for (const float Side : { -1.f, 1.f })
		{
			AddAeroPart(TEXT("MuscleSkirt"), CubeMesh, FVector(10.f, Side * 98.f, 25.f), FRotator::ZeroRotator, FVector(2.20f, 0.08f, 0.075f), AeroCarbonMID, false);
		}
		AddExhaustPair(-227.f, 44.f, 33.f, 0.105f);
		AddExhaustPair(-227.f, 58.f, 33.f, 0.105f);
		break;

	case EBNAeroKit::OffroadRack:
	{
		const float RackZ = BodyTopZ + 5.f;
		AddAeroPart(TEXT("BullbarMain"), CubeMesh, FVector(248.f, 0.f, 48.f), FRotator::ZeroRotator, FVector(0.10f, 1.48f, 0.10f), AeroAccentMID);
		AddAeroPart(TEXT("SkidPlate"), CubeMesh, FVector(236.f, 0.f, 26.f), FRotator(-28.f, 0.f, 0.f), FVector(0.32f, 1.10f, 0.04f), ExhaustChromeMID);
		for (const float Side : { -1.f, 1.f })
		{
			AddAeroPart(TEXT("BullbarPost"), CubeMesh, FVector(244.f, Side * 52.f, 42.f), FRotator::ZeroRotator, FVector(0.10f, 0.08f, 0.28f), AeroAccentMID);
			AddAeroPart(TEXT("RoofRail"), CubeMesh, FVector(-20.f, Side * 54.f, RackZ), FRotator::ZeroRotator, FVector(1.36f, 0.06f, 0.06f), AeroAccentMID);
		}
		AddAeroPart(TEXT("RoofCrossF"), CubeMesh, FVector(38.f, 0.f, RackZ + 2.f), FRotator::ZeroRotator, FVector(0.10f, 1.14f, 0.07f), AeroCarbonMID);
		AddAeroPart(TEXT("RoofCrossR"), CubeMesh, FVector(-76.f, 0.f, RackZ + 2.f), FRotator::ZeroRotator, FVector(0.10f, 1.14f, 0.07f), AeroCarbonMID);
		AddExhaustPair(-226.f, 54.f, 40.f, 0.125f);
		break;
	}

	case EBNAeroKit::WidebodyGT:
		AddAeroPart(TEXT("GtSplitter"), CubeMesh, FVector(248.f, 0.f, 22.f), FRotator::ZeroRotator, FVector(0.36f, 2.04f, 0.05f), AeroCarbonMID);
		for (const float Side : { -1.f, 1.f })
		{
			AddAeroPart(TEXT("GtSkirt"), CubeMesh, FVector(12.f, Side * 104.f, 23.f), FRotator::ZeroRotator, FVector(2.40f, 0.14f, 0.065f), AeroCarbonMID);
			AddAeroPart(TEXT("GtPylon"), CubeMesh, FVector(-198.f, Side * 46.f, 114.f), FRotator::ZeroRotator, FVector(0.18f, 0.04f, 0.48f), AeroCarbonMID);
			AddAeroPart(TEXT("GtEndplate"), CubeMesh, FVector(-205.f, Side * 101.f, 138.f), FRotator::ZeroRotator, FVector(0.42f, 0.035f, 0.28f), AeroAccentMID);
		}
		AddAeroPart(TEXT("GtWingBlade"), CubeMesh, FVector(-205.f, 0.f, 138.f), FRotator(-6.f, 0.f, 0.f), FVector(0.34f, 2.02f, 0.045f), AeroCarbonMID);
		AddDiffuserFins(-222.f, 24.f, { -48.f, -16.f, 16.f, 48.f });
		AddExhaustPair(-228.f, 42.f, 33.f, 0.11f);
		AddExhaustPair(-228.f, 56.f, 33.f, 0.11f);
		break;

	case EBNAeroKit::HyperCarbon:
	{
		const float WingZ = FMath::Max(118.f, BodyTopZ - 6.f);
		AddAeroPart(TEXT("HyperSplitter"), CubeMesh, FVector(248.f, 0.f, 20.f), FRotator::ZeroRotator, FVector(0.34f, 2.00f, 0.04f), AeroCarbonMID);
		AddAeroPart(TEXT("HyperSplitterEdge"), CubeMesh, FVector(262.f, 0.f, 21.f), FRotator::ZeroRotator, FVector(0.05f, 1.92f, 0.025f), AeroAccentMID, false);
		AddAeroPart(TEXT("HyperSharkFin"), CubeMesh, FVector(-118.f, 0.f, BodyTopZ - 6.f), FRotator::ZeroRotator, FVector(1.05f, 0.03f, 0.22f), AeroCarbonMID);
		for (const float Side : { -1.f, 1.f })
		{
			AddAeroPart(TEXT("HyperPylon"), CubeMesh, FVector(-202.f, Side * 44.f, WingZ - 14.f), FRotator::ZeroRotator, FVector(0.16f, 0.035f, 0.30f), AeroCarbonMID);
			AddAeroPart(TEXT("HyperEndplate"), CubeMesh, FVector(-208.f, Side * 96.f, WingZ), FRotator::ZeroRotator, FVector(0.38f, 0.03f, 0.22f), AeroAccentMID);
		}
		AddAeroPart(TEXT("HyperWing"), CubeMesh, FVector(-208.f, 0.f, WingZ), FRotator(-5.f, 0.f, 0.f), FVector(0.30f, 1.92f, 0.04f), AeroCarbonMID);
		AddDiffuserFins(-222.f, 23.f, { -48.f, -16.f, 16.f, 48.f });
		AddExhaustPair(-228.f, 10.f, 38.f, 0.095f);
		AddExhaustPair(-228.f, 24.f, 38.f, 0.095f);
		break;
	}
	}
}
