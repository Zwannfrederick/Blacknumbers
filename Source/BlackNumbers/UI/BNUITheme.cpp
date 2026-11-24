#include "UI/BNUITheme.h"

#include "Audio/BNAudioSubsystem.h"
#include "Engine/Texture2D.h"
#include "Misc/App.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	struct FBNBrushCache
	{
		TMap<FName, TWeakObjectPtr<UTexture2D>> Textures;
		TMap<FName, TSharedPtr<FSlateBrush>> Brushes;

		UTexture2D* GetOrLoadTexture(const TCHAR* AssetName)
		{
			if (!AssetName || FApp::IsUnattended() || GIsAutomationTesting)
			{
				return nullptr;
			}
			const FName Key(AssetName);
			if (const TWeakObjectPtr<UTexture2D>* Found = Textures.Find(Key))
			{
				return Found->Get();
			}
			const FString Path = FString::Printf(TEXT("/Game/UI/Textures/%s.%s"), AssetName, AssetName);
			UTexture2D* Loaded = LoadObject<UTexture2D>(nullptr, *Path);
			if (Loaded)
			{
				Loaded->AddToRoot();
			}
			Textures.Add(Key, Loaded);
			return Loaded;
		}

		const FSlateBrush* GetOrMakeBrush(const TCHAR* CacheKey, const TCHAR* AssetName,
			ESlateBrushDrawType::Type DrawType, ESlateBrushTileType::Type Tiling,
			const FMargin& Margin, const FVector2D& Size)
		{
			const FName Key(CacheKey);
			if (const TSharedPtr<FSlateBrush>* Found = Brushes.Find(Key))
			{
				return Found->Get();
			}
			UTexture2D* Tex = GetOrLoadTexture(AssetName);
			if (!Tex)
			{
				return FCoreStyle::Get().GetBrush("WhiteBrush");
			}
			TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
			Brush->SetResourceObject(Tex);
			Brush->DrawAs = DrawType;
			Brush->Tiling = Tiling;
			Brush->Margin = Margin;
			Brush->ImageSize = Size;
			Brushes.Add(Key, Brush);
			return Brush.Get();
		}
	};

	FBNBrushCache& BrushCache()
	{
		static FBNBrushCache Cache;
		return Cache;
	}

	struct FBNButtonPalette
	{
		FLinearColor BgIdle;
		FLinearColor BgHover;
		FLinearColor BgPress;
		FLinearColor BladeIdle;
		FLinearColor BladeHover;
		FLinearColor TextIdle;
		FLinearColor TextHover;
	};

	FBNButtonPalette PaletteFor(EBNButtonVariant Variant)
	{
		using namespace BNUITheme;
		switch (Variant)
		{
		case EBNButtonVariant::PrimaryAmber:
			return {
				AmberGold, AmberBright, FLinearColor(0.80f, 0.50f, 0.04f, 1.f),
				ObsidianInk, ChalkWhite, ObsidianInk, ObsidianInk
			};
		case EBNButtonVariant::ConfirmGreen:
			return {
				FLinearColor(0.11f, 0.44f, 0.25f, 1.f), FLinearColor(0.16f, 0.60f, 0.34f, 1.f), FLinearColor(0.08f, 0.34f, 0.19f, 1.f),
				EmeraldGreen, ChalkWhite, ChalkWhite, ChalkWhite
			};
		case EBNButtonVariant::DangerCrimson:
			return {
				FLinearColor(0.56f, 0.12f, 0.11f, 1.f), FLinearColor(0.78f, 0.16f, 0.14f, 1.f), FLinearColor(0.42f, 0.09f, 0.08f, 1.f),
				CrimsonRed, AmberBright, ChalkWhite, ChalkWhite
			};
		case EBNButtonVariant::CyanIntel:
			return {
				FLinearColor(0.10f, 0.34f, 0.52f, 1.f), FLinearColor(0.14f, 0.48f, 0.72f, 1.f), FLinearColor(0.08f, 0.26f, 0.40f, 1.f),
				NosCyan, ChalkWhite, ChalkWhite, ChalkWhite
			};
		case EBNButtonVariant::ActiveTab:
			return {
				FLinearColor(0.19f, 0.14f, 0.055f, 0.98f), FLinearColor(0.24f, 0.18f, 0.07f, 1.f), FLinearColor(0.15f, 0.11f, 0.045f, 1.f),
				AmberGold, AmberBright, AmberBright, ChalkWhite
			};
		case EBNButtonVariant::IdleTab:
			return {
				FLinearColor(0.085f, 0.090f, 0.102f, 0.92f), FLinearColor(0.155f, 0.145f, 0.115f, 0.98f), FLinearColor(0.12f, 0.11f, 0.09f, 1.f),
				HairlineSteel, AmberGold, SteelMuted, ChalkWhite
			};
		default: // SecondarySteel
			return {
				FLinearColor(0.115f, 0.122f, 0.138f, 0.96f), FLinearColor(0.195f, 0.175f, 0.130f, 1.f), FLinearColor(0.09f, 0.095f, 0.11f, 1.f),
				FLinearColor(0.36f, 0.34f, 0.29f, 1.f), AmberGold, ChalkWhite, AmberBright
			};
		}
	}

	/** Hover & press reactive Most Wanted button with illuminated edge blade. */
	class SBNMWButton : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBNMWButton)
			: _Variant(EBNButtonVariant::SecondarySteel)
			, _bEnabled(true)
			, _FontSize(11)
			, _Padding(FMargin(14.f, 9.f))
			, _Justification(ETextJustify::Center)
		{}
			SLATE_ARGUMENT(FText, Label)
			SLATE_ARGUMENT(FText, SubLabel)
			SLATE_ARGUMENT(EBNButtonVariant, Variant)
			SLATE_ARGUMENT(FOnClicked, OnClicked)
			SLATE_ARGUMENT(bool, bEnabled)
			SLATE_ARGUMENT(int32, FontSize)
			SLATE_ARGUMENT(FMargin, Padding)
			SLATE_ARGUMENT(ETextJustify::Type, Justification)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			VariantFlag = InArgs._Variant;
			Palette = PaletteFor(InArgs._Variant);
			bEnabledFlag = InArgs._bEnabled;
			const bool bHasSub = !InArgs._SubLabel.IsEmpty();

			TSharedRef<SVerticalBox> TextStack = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(InArgs._Label)
				.Font(BNUITheme::Font(InArgs._FontSize, true))
				.ColorAndOpacity(this, &SBNMWButton::GetTextColor)
				.Justification(InArgs._Justification)
			];
			if (bHasSub)
			{
				TextStack->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))
				[
					SNew(STextBlock)
					.Text(InArgs._SubLabel)
					.Font(BNUITheme::Font(9, false))
					.ColorAndOpacity(this, &SBNMWButton::GetSubTextColor)
					.Justification(InArgs._Justification)
				];
			}

			const EBNButtonVariant ChosenVariant = InArgs._Variant;
			const FOnClicked RawClicked = InArgs._OnClicked;
			FOnClicked WrappedClicked = FOnClicked::CreateLambda([ChosenVariant, RawClicked]()
			{
				const EBNSfxId Sfx = (ChosenVariant == EBNButtonVariant::PrimaryAmber || ChosenVariant == EBNButtonVariant::ConfirmGreen)
					? EBNSfxId::UiConfirm : EBNSfxId::UiClick;
				UBNAudioSubsystem::PlayUiSfx(Sfx, 0.85f);
				return RawClicked.IsBound() ? RawClicked.Execute() : FReply::Handled();
			});

			ChildSlot
			[
				SAssignNew(InnerButton, SButton)
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.IsFocusable(false)
				.IsEnabled(bEnabledFlag)
				.OnClicked(WrappedClicked)
				.OnHovered_Lambda([bEnabled = bEnabledFlag]()
				{
					if (bEnabled)
					{
						UBNAudioSubsystem::PlayUiSfx(EBNSfxId::UiHover, 0.45f);
					}
				})
				.ContentPadding(FMargin(0.f))
				[
					SNew(SBorder)
					.BorderImage(BNUITheme::WhiteBrush())
					.BorderBackgroundColor(this, &SBNMWButton::GetBgColor)
					.Padding(FMargin(0.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(4.f)
							[
								SNew(SBorder)
								.BorderImage(BNUITheme::WhiteBrush())
								.BorderBackgroundColor(this, &SBNMWButton::GetBladeColor)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(InArgs._Padding)
						[
							TextStack
						]
					]
				]
			];
		}

	private:
		bool IsBtnHovered() const { return bEnabledFlag && InnerButton.IsValid() && InnerButton->IsHovered(); }
		bool IsBtnPressed() const { return bEnabledFlag && InnerButton.IsValid() && InnerButton->IsPressed(); }

		FSlateColor GetBgColor() const
		{
			if (!bEnabledFlag)
			{
				return FLinearColor(0.08f, 0.085f, 0.095f, 0.65f);
			}
			return IsBtnPressed() ? Palette.BgPress : IsBtnHovered() ? Palette.BgHover : Palette.BgIdle;
		}

		FSlateColor GetBladeColor() const
		{
			if (!bEnabledFlag)
			{
				return FLinearColor(0.18f, 0.18f, 0.20f, 0.50f);
			}
			return (IsBtnHovered() || IsBtnPressed()) ? Palette.BladeHover : Palette.BladeIdle;
		}

		FSlateColor GetTextColor() const
		{
			if (!bEnabledFlag)
			{
				return FLinearColor(0.42f, 0.42f, 0.44f, 0.85f);
			}
			return IsBtnHovered() ? Palette.TextHover : Palette.TextIdle;
		}

		FSlateColor GetSubTextColor() const
		{
			if (!bEnabledFlag)
			{
				return FLinearColor(0.35f, 0.35f, 0.37f, 0.75f);
			}
			if (VariantFlag == EBNButtonVariant::PrimaryAmber)
			{
				return FLinearColor(0.10f, 0.09f, 0.07f, 0.92f);
			}
			return IsBtnHovered() ? BNUITheme::ChalkWhite : BNUITheme::SteelMuted;
		}

		EBNButtonVariant VariantFlag = EBNButtonVariant::SecondarySteel;
		FBNButtonPalette Palette;
		bool bEnabledFlag = true;
		TSharedPtr<SButton> InnerButton;
	};
}

namespace BNUITheme
{
	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	FSlateFontInfo Font(int32 Size, bool bBold)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	UTexture2D* LoadUITexture(const TCHAR* AssetName)
	{
		return BrushCache().GetOrLoadTexture(AssetName);
	}

	const FSlateBrush* GetPanelBevelBrush()
	{
		return BrushCache().GetOrMakeBrush(TEXT("PanelBevel"), TEXT("T_UI_PanelBevel"),
			ESlateBrushDrawType::Box, ESlateBrushTileType::NoTile, FMargin(0.12f), FVector2D(256.f, 256.f));
	}

	const FSlateBrush* GetBrushedMetalBrush()
	{
		return BrushCache().GetOrMakeBrush(TEXT("BrushedMetal"), TEXT("T_UI_BrushedMetal"),
			ESlateBrushDrawType::Image, ESlateBrushTileType::Both, FMargin(0.f), FVector2D(512.f, 512.f));
	}

	const FSlateBrush* GetHazardStripeBrush()
	{
		return BrushCache().GetOrMakeBrush(TEXT("HazardStripe"), TEXT("T_UI_HazardStripe"),
			ESlateBrushDrawType::Image, ESlateBrushTileType::Horizontal, FMargin(0.f), FVector2D(512.f, 32.f));
	}

	const FSlateBrush* GetVignetteLeftBrush()
	{
		return BrushCache().GetOrMakeBrush(TEXT("VignetteLeft"), TEXT("T_UI_VignetteLeft"),
			ESlateBrushDrawType::Image, ESlateBrushTileType::NoTile, FMargin(0.f), FVector2D(1024.f, 64.f));
	}

	const FSlateBrush* GetVignetteFullBrush()
	{
		return BrushCache().GetOrMakeBrush(TEXT("VignetteFull"), TEXT("T_UI_VignetteFull"),
			ESlateBrushDrawType::Image, ESlateBrushTileType::NoTile, FMargin(0.f), FVector2D(512.f, 512.f));
	}

	const FSlateBrush* GetArtBrush(const TCHAR* AssetName)
	{
		return BrushCache().GetOrMakeBrush(AssetName, AssetName,
			ESlateBrushDrawType::Image, ESlateBrushTileType::NoTile, FMargin(0.f), FVector2D(1280.f, 720.f));
	}

	UTexture2D* GetTachRingTexture()
	{
		return LoadUITexture(TEXT("T_UI_TachRing"));
	}

	UTexture2D* GetRadarRingTexture()
	{
		return LoadUITexture(TEXT("T_UI_RadarRing"));
	}

	TSharedRef<SWidget> MakeHazardBar(float Height)
	{
		const FSlateBrush* Hazard = GetHazardStripeBrush();
		const bool bHasTex = Hazard != WhiteBrush();
		return SNew(SBox)
			.HeightOverride(Height)
			[
				SNew(SImage)
				.Image(Hazard)
				.ColorAndOpacity(bHasTex ? FLinearColor::White : AmberGold)
			];
	}

	TSharedRef<SWidget> MakeHairline(const FLinearColor& Color)
	{
		return SNew(SBox)
			.HeightOverride(1.f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(Color)
			];
	}

	TSharedRef<SWidget> MakeSectionHeader(const FString& Title, const FString& RightTag, const FLinearColor& Accent)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 2.f, 8.f, 2.f))
		[
			SNew(SBox).WidthOverride(4.f)
			[
				SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(Accent)
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Title))
			.Font(Font(12, true))
			.ColorAndOpacity(ChalkWhite)
		];
		if (!RightTag.IsEmpty())
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(RightTag))
				.Font(Font(10, true))
				.ColorAndOpacity(Accent)
			];
		}

		return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Row]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 5.f, 0.f, 0.f))[MakeHairline()];
	}

	TSharedRef<SWidget> MakeMWButton(const FText& Label, EBNButtonVariant Variant, FOnClicked OnClicked,
		bool bEnabled, int32 FontSize, FMargin Padding)
	{
		return SNew(SBNMWButton)
			.Label(Label)
			.Variant(Variant)
			.OnClicked(OnClicked)
			.bEnabled(bEnabled)
			.FontSize(FontSize)
			.Padding(Padding)
			.Justification(ETextJustify::Center);
	}

	TSharedRef<SWidget> MakeNavBladeButton(const FText& Label, const FText& SubLabel, bool bPrimary,
		FOnClicked OnClicked, bool bDanger)
	{
		const EBNButtonVariant Variant = bDanger ? EBNButtonVariant::DangerCrimson
			: bPrimary ? EBNButtonVariant::PrimaryAmber
			: EBNButtonVariant::SecondarySteel;
		return SNew(SBNMWButton)
			.Label(Label)
			.SubLabel(SubLabel)
			.Variant(Variant)
			.OnClicked(OnClicked)
			.bEnabled(true)
			.FontSize(bPrimary ? 14 : 12)
			.Padding(bPrimary ? FMargin(18.f, 13.f) : FMargin(16.f, 10.f))
			.Justification(ETextJustify::Left);
	}

	TSharedRef<SWidget> MakeSegmentedStatBar(const FString& Label, const FString& ValueText,
		float Fraction, const FLinearColor& FillColor)
	{
		constexpr int32 NumSegments = 12;
		const float Clamped = FMath::Clamp(Fraction, 0.f, 1.f);
		const int32 LitCount = FMath::Clamp(FMath::RoundToInt(Clamped * NumSegments), 0, NumSegments);

		TSharedRef<SHorizontalBox> SegRow = SNew(SHorizontalBox);
		for (int32 i = 0; i < NumSegments; ++i)
		{
			const bool bLit = i < LitCount;
			const FLinearColor SegCol = bLit ? FillColor : FLinearColor(0.16f, 0.16f, 0.18f, 0.85f);
			SegRow->AddSlot()
			.FillWidth(1.f)
			.Padding(FMargin(0.f, 0.f, i < NumSegments - 1 ? 3.f : 0.f, 0.f))
			[
				SNew(SBox).HeightOverride(7.f)
				[
					SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(SegCol)
				]
			];
		}

		return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(9, false)).ColorAndOpacity(SteelMuted)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(STextBlock).Text(FText::FromString(ValueText)).Font(Font(10, true)).ColorAndOpacity(ChalkWhite)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 7.f))
		[
			SegRow
		];
	}
}
