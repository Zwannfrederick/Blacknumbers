#pragma once

#include "CoreMinimal.h"
#include "Framework/SlateDelegates.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SWidget.h"

class UTexture2D;

enum class EBNButtonVariant : uint8
{
	PrimaryAmber,
	SecondarySteel,
	ConfirmGreen,
	DangerCrimson,
	CyanIntel,
	ActiveTab,
	IdleTab,
};

/**
 * Shared NFS Most Wanted (2005)-inspired visual identity for BLACK NUMBERS:
 * Gunmetal/asphalt/amber/crimson palette, lazy non-streaming UI texture brushes with safe headless
 * fallbacks, and real hover/press-reactive Slate controls.
 */
namespace BNUITheme
{
	// Core Most Wanted 2005 industrial palette
	const FLinearColor AsphaltDark(0.032f, 0.035f, 0.042f, 0.96f);
	const FLinearColor GunmetalPanel(0.072f, 0.077f, 0.088f, 0.94f);
	const FLinearColor GunmetalCard(0.100f, 0.106f, 0.118f, 0.96f);
	const FLinearColor GunmetalTranslucent(0.052f, 0.056f, 0.066f, 0.86f);
	const FLinearColor AmberGold(0.95f, 0.66f, 0.08f, 1.0f);
	const FLinearColor AmberBright(1.00f, 0.79f, 0.24f, 1.0f);
	const FLinearColor AmberDarkCard(0.165f, 0.120f, 0.048f, 0.96f);
	const FLinearColor CrimsonRed(0.86f, 0.17f, 0.14f, 1.0f);
	const FLinearColor CrimsonDarkCard(0.22f, 0.055f, 0.052f, 0.96f);
	const FLinearColor NosCyan(0.16f, 0.78f, 1.00f, 1.0f);
	const FLinearColor EmeraldGreen(0.16f, 0.78f, 0.42f, 1.0f);
	const FLinearColor ChalkWhite(0.96f, 0.94f, 0.90f, 1.0f);
	const FLinearColor ObsidianInk(0.045f, 0.048f, 0.055f, 1.0f);
	const FLinearColor SteelMuted(0.68f, 0.66f, 0.61f, 1.0f);
	const FLinearColor HairlineSteel(0.28f, 0.26f, 0.22f, 0.90f);

	const FSlateBrush* WhiteBrush();
	FSlateFontInfo Font(int32 Size, bool bBold = true);

	/** Loads and roots a texture under /Game/UI/Textures/<AssetName>, returning nullptr in headless tests. */
	UTexture2D* LoadUITexture(const TCHAR* AssetName);

	const FSlateBrush* GetPanelBevelBrush();
	const FSlateBrush* GetBrushedMetalBrush();
	const FSlateBrush* GetHazardStripeBrush();
	const FSlateBrush* GetVignetteLeftBrush();
	const FSlateBrush* GetVignetteFullBrush();
	const FSlateBrush* GetArtBrush(const TCHAR* AssetName);
	UTexture2D* GetTachRingTexture();
	UTexture2D* GetRadarRingTexture();

	/** Diagonal amber/asphalt caution stripe bar. */
	TSharedRef<SWidget> MakeHazardBar(float Height = 8.f);

	/** Crisp 1px metallic rule. */
	TSharedRef<SWidget> MakeHairline(const FLinearColor& Color = HairlineSteel);

	/** Industrial section title with left amber blade and optional right status tag. */
	TSharedRef<SWidget> MakeSectionHeader(const FString& Title, const FString& RightTag = FString(),
		const FLinearColor& Accent = AmberGold);

	/** Hover-and-press-reactive Most Wanted button with illuminated left accent blade. */
	TSharedRef<SWidget> MakeMWButton(const FText& Label, EBNButtonVariant Variant, FOnClicked OnClicked,
		bool bEnabled = true, int32 FontSize = 11, FMargin Padding = FMargin(14.f, 9.f));

	/** Tall left-aligned Most Wanted title/menu navigation blade with dynamic hover bar & chevron. */
	TSharedRef<SWidget> MakeNavBladeButton(const FText& Label, const FText& SubLabel, bool bPrimary,
		FOnClicked OnClicked, bool bDanger = false);

	/** 12-segment mechanical Most Wanted performance bar (replaces thin generic progress lines). */
	TSharedRef<SWidget> MakeSegmentedStatBar(const FString& Label, const FString& ValueText,
		float Fraction, const FLinearColor& FillColor);
}
