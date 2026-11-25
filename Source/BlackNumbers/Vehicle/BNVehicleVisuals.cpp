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
	const TCHAR* const GrandTourerPackagePath = TEXT("/Game/Vehicles/GrandTourer/SM_BN_GrandTourer");
	const TCHAR* const GrandTourerAssetPath = TEXT("/Game/Vehicles/GrandTourer/SM_BN_GrandTourer.SM_BN_GrandTourer");

	const TCHAR* const BasicCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const BasicCylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* const BasicShapeMatPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const TCHAR* const CarbonMatPkgPath = TEXT("/Game/World/Materials/MI_CarCarbon");
	const TCHAR* const CarbonMatAssetPath = TEXT("/Game/World/Materials/MI_CarCarbon.MI_CarCarbon");
	const TCHAR* const RimMatPkgPath = TEXT("/Game/World/Materials/MI_CarRim");
	const TCHAR* const RimMatAssetPath = TEXT("/Game/World/Materials/MI_CarRim.MI_CarRim");
	const TCHAR* const HeadlightMatPath = TEXT("/Game/World/Materials/MI_Vehicle_HeadlightEmissive.MI_Vehicle_HeadlightEmissive");
	const TCHAR* const BrakeLightMatPath = TEXT("/Game/World/Materials/MI_Vehicle_BrakeLightEmissive.MI_Vehicle_BrakeLightEmissive");

	const TCHAR* ExternalCarAssetKey(EBNCarId CarId)
	{
		switch (CarId)
		{
		case EBNCarId::ValkenRS4:     return TEXT("SM_Ext_ValkenRS4");
		case EBNCarId::Vandal440R:    return TEXT("SM_Ext_Vandal440R");
		case EBNCarId::TauronV6Rally: return TEXT("SM_Ext_TauronV6Rally");
		case EBNCarId::DracoRST:      return TEXT("SM_Ext_DracoRST");
		case EBNCarId::NocturneBN01:  return TEXT("SM_Ext_NocturneBN01");
		default:                      return TEXT("SM_Ext_KuroganeMSpec");
		}
	}

	constexpr float TargetCarLengthCm = 468.f;
	constexpr float TargetCarCenterXCm = 12.f;
	constexpr float SportsCarDefaultTopZ = 128.f;
	constexpr float NosFlameCandelas = 4.f;
	constexpr float BackfireFlameCandelas = 6.f;
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
				|| Name.Contains(TEXT("CarPaint")) || Name.Contains(TEXT("Paint"))
				|| Name.Contains(TEXT("Clearcoat")) || Name.Contains(TEXT("Body")))
			{
				return i;
			}
		}
		return 0;
	}

	int32 FindLightsMaterialSlot(const UStaticMeshComponent* ChassisComp)
	{
		if (!ChassisComp)
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
			if (Name.Contains(TEXT("Lights")) || Name.Contains(TEXT("Brake")) || Name.Contains(TEXT("Tail")))
			{
				return i;
			}
		}
		return INDEX_NONE;
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

UStaticMesh* UBNVehicleVisualsComponent::LoadOptionalMesh(const TCHAR* PackagePath, const TCHAR* AssetPath)
{
	if (PackagePath && AssetPath && FPackageName::DoesPackageExist(PackagePath))
	{
		return LoadObject<UStaticMesh>(nullptr, AssetPath);
	}
	return nullptr;
}

UStaticMesh* UBNVehicleVisualsComponent::ResolveBodyMesh(EBNCarId CarId, EBNBodyStyle Style, bool& bOutIsCustom)
{
	bOutIsCustom = false;

	// 1. Optional drop-in CC-BY / Epic Fab custom body imported via Tools/import_external_vehicles.py
	const FString ExtKey = ExternalCarAssetKey(CarId);
	const FString ExtPkg = FString::Printf(TEXT("/Game/Vehicles/External/%s"), *ExtKey);
	const FString ExtAsset = FString::Printf(TEXT("%s.%s"), *ExtPkg, *ExtKey);
	if (UStaticMesh* ExtLoaded = LoadOptionalMesh(*ExtPkg, *ExtAsset))
	{
		bOutIsCustom = true;
		return ExtLoaded;
	}

	// 2. Canonical body style mesh
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
	else if (Style == EBNBodyStyle::GrandTourer)
	{
		PackagePath = GrandTourerPackagePath;
		AssetPath = GrandTourerAssetPath;
	}

	if (UStaticMesh* Loaded = LoadOptionalMesh(PackagePath, AssetPath))
	{
		bOutIsCustom = true;
		return Loaded;
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
		BodyLightsMID = nullptr;
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
	UStaticMesh* TargetMesh = ResolveBodyMesh(Spec.Id, Spec.BodyStyle, bIsCustom);
	bUsingCustomBodyMesh = bIsCustom;
	AppliedBodyStyle = Spec.BodyStyle;

	float BodyTopZ = SportsCarDefaultTopZ;
	ConfigureChassisTransform(ChassisComp, TargetMesh, Spec.BodyStyle, bIsCustom, GlassComp, WheelMeshes, BodyTopZ);
	LastRoughnessOverride = -1.f;
	ApplyPaintFinish(Spec.PaintTint, Spec.Metallic, Spec.ClearCoat, ChassisComp);
	BindBodyLightsSlot(ChassisComp);

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

void UBNVehicleVisualsComponent::ApplyPaintFinishPreset(EBNPaintFinish Finish, UStaticMeshComponent* ChassisComp)
{
	float Metallic = 0.84f;
	float ClearCoat = 0.92f;
	float RoughOverride = -1.f;
	switch (Finish)
	{
	case EBNPaintFinish::Gloss:
		Metallic = 0.22f;
		ClearCoat = 1.00f;
		RoughOverride = 0.08f;
		break;
	case EBNPaintFinish::Satin:
		Metallic = 0.48f;
		ClearCoat = 0.24f;
		RoughOverride = 0.36f;
		break;
	case EBNPaintFinish::MatteCarbon:
		Metallic = 0.14f;
		ClearCoat = 0.04f;
		RoughOverride = 0.62f;
		break;
	default: // Metallic
		Metallic = 0.86f;
		ClearCoat = 0.94f;
		RoughOverride = 0.16f;
		break;
	}
	LastRoughnessOverride = RoughOverride;
	ApplyPaintFinish(LastPaintTint, Metallic, ClearCoat, ChassisComp);
}

void UBNVehicleVisualsComponent::ApplyPaintFinish(const FLinearColor& PaintTint, float Metallic, float ClearCoat,
	UStaticMeshComponent* ChassisComp)
{
	LastPaintTint = PaintTint;
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

	// Support UE5 SportsCar/Offroad (Paint Tint), ConceptCar (PaintColor), glTF (BaseColorFactor), and custom materials (BaseColor).
	SetVectorIfPresent(BodyPaintMID, TEXT("Paint Tint"), PaintTint);
	SetVectorIfPresent(BodyPaintMID, TEXT("PaintColor"), PaintTint);
	SetVectorIfPresent(BodyPaintMID, TEXT("BaseColor"), PaintTint);
	SetVectorIfPresent(BodyPaintMID, TEXT("BaseColorFactor"), PaintTint);

	const float CoatRough = LastRoughnessOverride >= 0.f
		? FMath::Clamp(LastRoughnessOverride * 0.45f, 0.03f, 0.45f)
		: FMath::Lerp(0.26f, 0.04f, LastClearCoat);
	const float BaseRough = LastRoughnessOverride >= 0.f
		? LastRoughnessOverride
		: FMath::Lerp(0.42f, 0.14f, LastClearCoat);
	SetScalarIfPresent(BodyPaintMID, TEXT("Metallic"), LastMetallic);
	SetScalarIfPresent(BodyPaintMID, TEXT("MetallicFactor"), LastMetallic);
	SetScalarIfPresent(BodyPaintMID, TEXT("MetalPaintMetallic"), LastMetallic);
	SetScalarIfPresent(BodyPaintMID, TEXT("ClearCoat"), LastClearCoat);
	SetScalarIfPresent(BodyPaintMID, TEXT("ClearCoatRoughness"), CoatRough);
	SetScalarIfPresent(BodyPaintMID, TEXT("Roughness"), BaseRough);
	SetScalarIfPresent(BodyPaintMID, TEXT("RoughnessFactor"), BaseRough);
	SetScalarIfPresent(BodyPaintMID, TEXT("BaseRough"), BaseRough);
	SetScalarIfPresent(BodyPaintMID, TEXT("FlakeIntensity"), FMath::Lerp(0.15f, 0.85f, LastMetallic));
}

void UBNVehicleVisualsComponent::ApplyRimPaint(const FLinearColor& RimTint,
	const TArray<TObjectPtr<UStaticMeshComponent>>& WheelMeshes)
{
	if (!RimBaseMaterial && FPackageName::DoesPackageExist(RimMatPkgPath))
	{
		RimBaseMaterial = LoadObject<UMaterialInterface>(nullptr, RimMatAssetPath);
	}
	if (WheelRimMIDs.Num() != WheelMeshes.Num())
	{
		WheelRimMIDs.SetNum(WheelMeshes.Num());
	}
	for (int32 w = 0; w < WheelMeshes.Num(); ++w)
	{
		UStaticMeshComponent* Wheel = WheelMeshes[w];
		if (!Wheel)
		{
			continue;
		}
		if (!WheelRimMIDs[w])
		{
			int32 RimSlot = INDEX_NONE;
			for (int32 s = 0; s < Wheel->GetNumMaterials(); ++s)
			{
				const UMaterialInterface* Mat = Wheel->GetMaterial(s);
				const FString Name = Mat ? Mat->GetName() : FString();
				if (Name.Contains(TEXT("Tire")) || Name.Contains(TEXT("Rubber")))
				{
					continue;
				}
				if (Name.Contains(TEXT("Wheel")) || Name.Contains(TEXT("Rim")) || RimSlot == INDEX_NONE)
				{
					RimSlot = s;
					if (Name.Contains(TEXT("Wheel")) || Name.Contains(TEXT("Rim")))
					{
						break;
					}
				}
			}
			if (RimSlot != INDEX_NONE)
			{
				UMaterialInterface* SourceMat = RimBaseMaterial ? RimBaseMaterial.Get() : Wheel->GetMaterial(RimSlot);
				if (SourceMat)
				{
					WheelRimMIDs[w] = Wheel->CreateDynamicMaterialInstance(RimSlot, SourceMat);
				}
			}
		}
		if (UMaterialInstanceDynamic* RimMID = WheelRimMIDs[w])
		{
			SetVectorIfPresent(RimMID, TEXT("BaseColor"), RimTint);
			SetVectorIfPresent(RimMID, TEXT("Paint Tint"), RimTint);
			SetVectorIfPresent(RimMID, TEXT("Color"), RimTint);
			SetScalarIfPresent(RimMID, TEXT("Metallic"), 0.86f);
			SetScalarIfPresent(RimMID, TEXT("Roughness"), 0.22f);
		}
	}
}

void UBNVehicleVisualsComponent::BindBodyLightsSlot(UStaticMeshComponent* ChassisComp)
{
	if (!ChassisComp || BodyLightsMID)
	{
		return;
	}
	const int32 LightSlot = FindLightsMaterialSlot(ChassisComp);
	if (LightSlot != INDEX_NONE)
	{
		if (UMaterialInterface* Mat = ChassisComp->GetMaterial(LightSlot))
		{
			BodyLightsMID = ChassisComp->CreateDynamicMaterialInstance(LightSlot, Mat);
		}
	}
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
	if (!CarbonBaseMaterial && FPackageName::DoesPackageExist(CarbonMatPkgPath))
	{
		CarbonBaseMaterial = LoadObject<UMaterialInterface>(nullptr, CarbonMatAssetPath);
	}
	if (!AeroBaseMaterial)
	{
		return;
	}

	if (!AeroCarbonMID)
	{
		UMaterialInterface* CarbonParent = CarbonBaseMaterial ? CarbonBaseMaterial.Get() : AeroBaseMaterial.Get();
		AeroCarbonMID = UMaterialInstanceDynamic::Create(CarbonParent, this);
		SetVectorIfPresent(AeroCarbonMID, TEXT("Color"), FLinearColor(0.035f, 0.038f, 0.045f, 1.f));
		SetVectorIfPresent(AeroCarbonMID, TEXT("BaseColor"), FLinearColor(0.05f, 0.055f, 0.065f, 1.f));
		SetScalarIfPresent(AeroCarbonMID, TEXT("Roughness"), 0.22f);
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
	if (!HeadlightStripMID && FPackageName::DoesPackageExist(TEXT("/Game/World/Materials/MI_Vehicle_HeadlightEmissive")))
	{
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, HeadlightMatPath))
		{
			HeadlightStripMID = UMaterialInstanceDynamic::Create(Mat, this);
		}
	}
	if (!BrakeLightStripMID && FPackageName::DoesPackageExist(TEXT("/Game/World/Materials/MI_Vehicle_BrakeLightEmissive")))
	{
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, BrakeLightMatPath))
		{
			BrakeLightStripMID = UMaterialInstanceDynamic::Create(Mat, this);
		}
	}
}

void UBNVehicleVisualsComponent::UpdateLightStrips(bool bHeadlightsOn, bool bBraking)
{
	if (BodyLightsMID)
	{
		const float BrakeBoost = bBraking ? 12.f : (bHeadlightsOn ? 2.5f : 0.25f);
		const float HeadBoost = bHeadlightsOn ? 8.0f : 0.6f;
		SetScalarIfPresent(BodyLightsMID, TEXT("Brake"), bBraking ? 1.f : 0.f);
		SetScalarIfPresent(BodyLightsMID, TEXT("BrakeLights"), BrakeBoost);
		SetScalarIfPresent(BodyLightsMID, TEXT("TailLight"), bHeadlightsOn ? 1.f : 0.f);
		SetScalarIfPresent(BodyLightsMID, TEXT("HeadLights"), HeadBoost);
		SetScalarIfPresent(BodyLightsMID, TEXT("EmissiveBoost"), FMath::Max(BrakeBoost, HeadBoost));
	}
	if (HeadlightStripMID)
	{
		const FLinearColor HeadEm = bHeadlightsOn
			? FLinearColor(8.5f, 7.8f, 6.2f, 1.f)
			: FLinearColor(0.7f, 0.68f, 0.62f, 1.f);
		SetVectorIfPresent(HeadlightStripMID, TEXT("Emissive"), HeadEm);
	}
	if (BrakeLightStripMID)
	{
		const FLinearColor BrakeEm = bBraking
			? FLinearColor(11.0f, 0.25f, 0.08f, 1.f)
			: (bHeadlightsOn ? FLinearColor(2.4f, 0.06f, 0.03f, 1.f) : FLinearColor(0.22f, 0.01f, 0.01f, 1.f));
		SetVectorIfPresent(BrakeLightStripMID, TEXT("Emissive"), BrakeEm);
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

	// Prefer sculpted curved glTF carbon aero meshes generated by Tools/bn_mesh_vehicles_landmarks.py when present.
	UStaticMesh* SculptSplitter = LoadOptionalMesh(TEXT("/Game/World/Meshes/SM_BN_Aero_Splitter"), TEXT("/Game/World/Meshes/SM_BN_Aero_Splitter.SM_BN_Aero_Splitter"));
	UStaticMesh* SculptDiffuser = LoadOptionalMesh(TEXT("/Game/World/Meshes/SM_BN_Aero_Diffuser"), TEXT("/Game/World/Meshes/SM_BN_Aero_Diffuser.SM_BN_Aero_Diffuser"));
	UStaticMesh* SculptGTWing = LoadOptionalMesh(TEXT("/Game/World/Meshes/SM_BN_Aero_GTWing"), TEXT("/Game/World/Meshes/SM_BN_Aero_GTWing.SM_BN_Aero_GTWing"));
	UStaticMesh* SculptRallyWing = LoadOptionalMesh(TEXT("/Game/World/Meshes/SM_BN_Aero_RallyWing"), TEXT("/Game/World/Meshes/SM_BN_Aero_RallyWing.SM_BN_Aero_RallyWing"));
	UStaticMesh* SculptBlower = LoadOptionalMesh(TEXT("/Game/World/Meshes/SM_BN_Aero_Blower"), TEXT("/Game/World/Meshes/SM_BN_Aero_Blower.SM_BN_Aero_Blower"));
	UStaticMesh* SculptOffroadRack = LoadOptionalMesh(TEXT("/Game/World/Meshes/SM_BN_Aero_OffroadRack"), TEXT("/Game/World/Meshes/SM_BN_Aero_OffroadRack.SM_BN_Aero_OffroadRack"));

	auto AddDiffuser = [&](float X, float Z, float WidthScale, const std::initializer_list<float>& OffsetsY)
	{
		if (SculptDiffuser)
		{
			AddAeroPart(TEXT("AeroDiffuser"), SculptDiffuser, FVector(X, 0.f, Z),
				FRotator::ZeroRotator, FVector(1.f, WidthScale, 1.f), AeroCarbonMID, false);
			return;
		}
		AddAeroPart(TEXT("AeroDiffuserTray"), CubeMesh, FVector(X + 6.f, 0.f, Z - 2.f),
			FRotator::ZeroRotator, FVector(0.20f, 1.06f, 0.028f), AeroCarbonMID, false);
		for (const float Y : OffsetsY)
		{
			AddAeroPart(TEXT("AeroDiffuserFin"), CubeMesh, FVector(X, Y, Z),
				FRotator::ZeroRotator, FVector(0.22f, 0.026f, 0.11f), AeroCarbonMID, false);
		}
	};

	auto AddSplitter = [&](const TCHAR* Name, float X, float Z, float WidthScale)
	{
		if (SculptSplitter)
		{
			AddAeroPart(Name, SculptSplitter, FVector(X, 0.f, Z),
				FRotator::ZeroRotator, FVector(1.f, WidthScale, 1.f), AeroCarbonMID, false);
		}
		else
		{
			AddAeroPart(Name, CubeMesh, FVector(X, 0.f, Z),
				FRotator::ZeroRotator, FVector(0.20f, 1.56f * WidthScale, 0.028f), AeroCarbonMID, false);
		}
	};

	switch (Spec.AeroKit)
	{
	case EBNAeroKit::StreetLip:
		AddSplitter(TEXT("StreetFrontLip"), 222.f, 18.f, 0.96f);
		for (const float Side : { -1.f, 1.f })
		{
			AddAeroPart(TEXT("StreetSkirt"), CubeMesh, FVector(6.f, Side * 92.f, 19.f), FRotator::ZeroRotator, FVector(1.90f, 0.052f, 0.030f), AeroCarbonMID, false);
		}
		AddDiffuser(-210.f, 22.f, 0.94f, { -32.f, 0.f, 32.f });
		break;

	case EBNAeroKit::RallyWing:
		AddSplitter(TEXT("RallySplitter"), 226.f, 18.f, 1.02f);
		if (SculptRallyWing)
		{
			AddAeroPart(TEXT("RallyWingSculpt"), SculptRallyWing, FVector(-198.f, 0.f, 94.f),
				FRotator::ZeroRotator, FVector::OneVector, AeroCarbonMID, true);
		}
		else
		{
			for (const float Side : { -1.f, 1.f })
			{
				AddAeroPart(TEXT("RallyPylon"), CubeMesh, FVector(-196.f, Side * 54.f, 108.f), FRotator::ZeroRotator, FVector(0.12f, 0.032f, 0.32f), AeroCarbonMID);
				AddAeroPart(TEXT("RallyEndplate"), CubeMesh, FVector(-200.f, Side * 88.f, 125.f), FRotator::ZeroRotator, FVector(0.30f, 0.025f, 0.20f), AeroAccentMID);
			}
			AddAeroPart(TEXT("RallyWingBlade"), CubeMesh, FVector(-200.f, 0.f, 125.f), FRotator(-4.f, 0.f, 0.f), FVector(0.24f, 1.76f, 0.036f), AeroAccentMID);
		}
		AddDiffuser(-212.f, 22.f, 1.00f, { -36.f, 0.f, 36.f });
		break;

	case EBNAeroKit::MuscleBlower:
		if (SculptBlower)
		{
			AddAeroPart(TEXT("MuscleBlowerSculpt"), SculptBlower, FVector(116.f, 0.f, 91.f),
				FRotator::ZeroRotator, FVector::OneVector, ExhaustChromeMID, true);
		}
		else
		{
			AddAeroPart(TEXT("BlowerBlock"), CubeMesh, FVector(114.f, 0.f, 94.f), FRotator::ZeroRotator, FVector(0.56f, 0.38f, 0.16f), ExhaustChromeMID);
			AddAeroPart(TEXT("BlowerScoop"), CubeMesh, FVector(120.f, 0.f, 106.f), FRotator::ZeroRotator, FVector(0.42f, 0.30f, 0.09f), AeroAccentMID);
		}
		AddSplitter(TEXT("MuscleChin"), 224.f, 19.f, 1.02f);
		for (const float Side : { -1.f, 1.f })
		{
			AddAeroPart(TEXT("MuscleSkirt"), CubeMesh, FVector(8.f, Side * 94.f, 20.f), FRotator::ZeroRotator, FVector(2.00f, 0.060f, 0.048f), AeroCarbonMID, false);
		}
		AddDiffuser(-212.f, 22.f, 1.04f, { -38.f, 0.f, 38.f });
		break;

	case EBNAeroKit::OffroadRack:
	{
		const float RackZ = BodyTopZ + 4.f;
		if (SculptOffroadRack)
		{
			AddAeroPart(TEXT("OffroadRackSculpt"), SculptOffroadRack, FVector(-18.f, 0.f, RackZ),
				FRotator::ZeroRotator, FVector::OneVector, AeroCarbonMID, true);
		}
		else
		{
			for (const float Side : { -1.f, 1.f })
			{
				AddAeroPart(TEXT("RoofRail"), CubeMesh, FVector(-20.f, Side * 54.f, RackZ), FRotator::ZeroRotator, FVector(1.34f, 0.055f, 0.055f), AeroAccentMID);
			}
			AddAeroPart(TEXT("RoofCrossF"), CubeMesh, FVector(38.f, 0.f, RackZ + 2.f), FRotator::ZeroRotator, FVector(0.09f, 1.12f, 0.06f), AeroCarbonMID);
			AddAeroPart(TEXT("RoofCrossR"), CubeMesh, FVector(-76.f, 0.f, RackZ + 2.f), FRotator::ZeroRotator, FVector(0.09f, 1.12f, 0.06f), AeroCarbonMID);
		}
		AddAeroPart(TEXT("SkidPlate"), CubeMesh, FVector(230.f, 0.f, 26.f), FRotator(-28.f, 0.f, 0.f), FVector(0.28f, 1.06f, 0.036f), ExhaustChromeMID);
		break;
	}

	case EBNAeroKit::WidebodyGT:
		AddSplitter(TEXT("GtSplitter"), 226.f, 18.f, 1.06f);
		if (SculptGTWing)
		{
			AddAeroPart(TEXT("GtWingSculpt"), SculptGTWing, FVector(-198.f, 0.f, FMath::Max(90.f, BodyTopZ - 34.f)),
				FRotator::ZeroRotator, FVector::OneVector, AeroCarbonMID, true);
		}
		else
		{
			for (const float Side : { -1.f, 1.f })
			{
				AddAeroPart(TEXT("GtPylon"), CubeMesh, FVector(-196.f, Side * 46.f, 114.f), FRotator::ZeroRotator, FVector(0.14f, 0.032f, 0.44f), AeroCarbonMID);
				AddAeroPart(TEXT("GtEndplate"), CubeMesh, FVector(-202.f, Side * 96.f, 136.f), FRotator::ZeroRotator, FVector(0.36f, 0.028f, 0.24f), AeroAccentMID);
			}
			AddAeroPart(TEXT("GtWingBlade"), CubeMesh, FVector(-202.f, 0.f, 136.f), FRotator(-6.f, 0.f, 0.f), FVector(0.28f, 1.92f, 0.038f), AeroCarbonMID);
		}
		AddDiffuser(-212.f, 21.f, 1.08f, { -44.f, -15.f, 15.f, 44.f });
		break;

	case EBNAeroKit::HyperCarbon:
	{
		const float WingZ = FMath::Max(112.f, BodyTopZ - 14.f);
		AddSplitter(TEXT("HyperSplitter"), 230.f, 17.f, 1.08f);
		if (SculptGTWing)
		{
			AddAeroPart(TEXT("HyperWingSculpt"), SculptGTWing, FVector(-202.f, 0.f, WingZ - 26.f),
				FRotator::ZeroRotator, FVector(1.05f, 1.04f, 0.88f), AeroCarbonMID, true);
		}
		else
		{
			for (const float Side : { -1.f, 1.f })
			{
				AddAeroPart(TEXT("HyperPylon"), CubeMesh, FVector(-200.f, Side * 44.f, WingZ - 14.f), FRotator::ZeroRotator, FVector(0.14f, 0.03f, 0.28f), AeroCarbonMID);
				AddAeroPart(TEXT("HyperEndplate"), CubeMesh, FVector(-205.f, Side * 92.f, WingZ), FRotator::ZeroRotator, FVector(0.34f, 0.026f, 0.20f), AeroAccentMID);
			}
			AddAeroPart(TEXT("HyperWing"), CubeMesh, FVector(-205.f, 0.f, WingZ), FRotator(-5.f, 0.f, 0.f), FVector(0.26f, 1.84f, 0.035f), AeroCarbonMID);
		}
		AddDiffuser(-214.f, 20.f, 1.10f, { -44.f, -15.f, 15.f, 44.f });
		break;
	}
	}
}
