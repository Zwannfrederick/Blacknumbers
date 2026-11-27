#pragma once

#include "CoreMinimal.h"
#include "Framework/SlateDelegates.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SWidget.h"

class UFont;
class UTexture2D;

enum class EBNButtonVariant : uint8
{
	PrimaryAmber,   // solid sodium slab, ink text — the one action that matters on the screen
	SecondarySteel, // dark slab, bone text
	ConfirmGreen,   // same look as primary (one accent only); kept for call-site intent
	DangerCrimson,  // dark red slab
	CyanIntel,      // dark slab, cyan tick (GPS / intel)
	ActiveTab,      // bone text over a sodium underline, no fill
	IdleTab,        // muted text, no fill
};

/**
 * BLACK NUMBERS "Sodyum" UI identity.
 * Night street racing seen through sodium street lamps: near-black ink, bone-white type,
 * ONE hot accent (sodium orange). Police red/blue and NOS cyan appear only when gameplay demands.
 * One type family (Noto Sans Condensed + ExtraCondensed Black Italic, always UPPERCASE for display),
 * and one shape: the 12° forward-leaning slab.
 * All colours are sRGB hex converted to linear (Slate/Canvas expect linear values).
 */
namespace BNUITheme
{
	const FLinearColor Ink(0.0030f, 0.0033f, 0.0040f, 0.92f);        // #0A0B0D
	const FLinearColor InkSolid(0.0030f, 0.0033f, 0.0040f, 1.0f);
	const FLinearColor Panel(0.0056f, 0.0065f, 0.0086f, 0.90f);      // #111317
	const FLinearColor PanelSoft(0.0056f, 0.0065f, 0.0086f, 0.72f);
	const FLinearColor Card(0.0103f, 0.0123f, 0.0160f, 0.94f);       // #1A1D22
	const FLinearColor Accent(1.0000f, 0.1946f, 0.0103f, 1.0f);      // #FF7A1A sodium
	const FLinearColor AccentHi(1.0000f, 0.3231f, 0.0595f, 1.0f);    // #FF9A45
	const FLinearColor AccentDark(0.0950f, 0.0180f, 0.0020f, 0.94f); // sodium-tinted ink for selected rows
	const FLinearColor Danger(0.7682f, 0.0194f, 0.0242f, 1.0f);      // #E3262B
	const FLinearColor DangerDark(0.0423f, 0.0040f, 0.0048f, 0.94f); // #3A0D0F
	const FLinearColor Nos(0.0284f, 0.6867f, 1.0000f, 1.0f);         // #2FD8FF
	const FLinearColor Ok(0.0782f, 0.6724f, 0.1946f, 1.0f);          // #4FD67A
	const FLinearColor Bone(0.8550f, 0.8228f, 0.7454f, 1.0f);        // #EEEAE0
	const FLinearColor Muted(0.2747f, 0.2542f, 0.2159f, 1.0f);       // #8F8A80
	const FLinearColor Hairline(0.0343f, 0.0382f, 0.0467f, 1.0f);    // #34373D

	/** Forward lean of every slab (degrees of horizontal shear). */
	constexpr float SlabLeanDeg = 12.f;

	const FSlateBrush* WhiteBrush();
	/** Condensed body font (Noto Sans Condensed Bold / Medium, full Turkish glyphs). */
	FSlateFontInfo Font(int32 Size, bool bBold = true);
	/** Racing display font (Noto Sans ExtraCondensed Black Italic). Pass UPPERCASE text. */
	FSlateFontInfo DisplayFont(int32 Size);
	/**
	 * Same faces as runtime UFonts for the Canvas HUD (fallback: engine font).
	 * Canvas caches glyphs at the font's legacy size and only scales the quads, so upscaling blurs:
	 * body = 17 px, display = 44 px (use scale <= 1), numeral = 120 px for the speedo/countdown.
	 */
	UFont* HudFont(bool bDisplay = false);
	UFont* HudNumeralFont();

	/** Loads and roots a texture under /Game/UI/Textures/<AssetName>, returning nullptr in headless tests. */
	UTexture2D* LoadUITexture(const TCHAR* AssetName);

	const FSlateBrush* GetVignetteLeftBrush();
	const FSlateBrush* GetVignetteFullBrush();
	const FSlateBrush* GetArtBrush(const TCHAR* AssetName);

	/** Thin sodium accent rule. */
	TSharedRef<SWidget> MakeAccentRule(float Height = 2.f);

	/** 1px rule. */
	TSharedRef<SWidget> MakeHairline(const FLinearColor& Color = Hairline);

	/** Uppercase display section title with optional right tag, over a hairline. */
	TSharedRef<SWidget> MakeSectionHeader(const FString& Title, const FString& RightTag = FString(),
		const FLinearColor& Accent = BNUITheme::Accent);

	/** Flat ink panel with an optional 3px accent edge on the left (transparent accent = no edge). */
	TSharedRef<SWidget> MakePanel(TSharedRef<SWidget> InnerContent, FMargin InnerPadding = FMargin(20.f, 14.f),
		const FLinearColor& Tint = Panel, const FLinearColor& EdgeAccent = FLinearColor::Transparent);

	/** Icon + caption tab (bottom Blacklist strip); selected = bone icon over a sodium underline. */
	TSharedRef<SWidget> MakeIconTab(const TCHAR* IconAssetName, const FString& Caption, bool bSelected,
		FOnClicked OnClicked, bool bEnabled = true, const FLinearColor& AccentColor = Accent);

	/** Leaning slab button. */
	TSharedRef<SWidget> MakeMWButton(const FText& Label, EBNButtonVariant Variant, FOnClicked OnClicked,
		bool bEnabled = true, int32 FontSize = 12, FMargin Padding = FMargin(14.f, 9.f));

	/** Large left-aligned menu entry for title & pause menus (text only until hovered/primary). */
	TSharedRef<SWidget> MakeNavBladeButton(const FText& Label, const FText& SubLabel, bool bPrimary,
		FOnClicked OnClicked, bool bDanger = false);

	/** 12-segment performance bar. */
	TSharedRef<SWidget> MakeSegmentedStatBar(const FString& Label, const FString& ValueText,
		float Fraction, const FLinearColor& FillColor);
}
