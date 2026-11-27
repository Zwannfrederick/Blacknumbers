#include "UI/BNUITheme.h"

#include "Audio/BNAudioSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Rendering/SlateRenderTransform.h"
#include "Styling/CoreStyle.h"
#include "UObject/Package.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr const TCHAR* BodyBoldFile = TEXT("NotoSans-CondensedBold.ttf");
	constexpr const TCHAR* BodyMediumFile = TEXT("NotoSans-CondensedMedium.ttf");
	constexpr const TCHAR* DisplayFile = TEXT("NotoSans-ExtraCondensedBlackItalic.ttf");

	FString BundledFontPath(const TCHAR* FileName)
	{
		const FString Candidate = FPaths::ProjectContentDir() / TEXT("UI/Fonts") / FileName;
		return IFileManager::Get().FileExists(*Candidate) ? Candidate : FString();
	}

	TSharedPtr<const FCompositeFont> LoadBundledCompositeFont(const TCHAR* FileName)
	{
		const FString Path = BundledFontPath(FileName);
		if (Path.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("BN UI: bundled font %s missing, falling back to engine font"), FileName);
			return nullptr;
		}
		return MakeShared<FCompositeFont>(FName(FileName), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	}

	UFont* MakeRuntimeHudFont(const TCHAR* FileName, int32 LegacySize)
	{
		const FString Path = BundledFontPath(FileName);
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		// Unique name per face+size: NewObject with an existing name would replace the other cached font in place.
		const FName ObjName = MakeUniqueObjectName(GetTransientPackage(), UFont::StaticClass(),
			FName(*FString::Printf(TEXT("BNHudFont_%s_%d"), *FPaths::GetBaseFilename(FileName), LegacySize)));
		UFont* NewFont = NewObject<UFont>(GetTransientPackage(), ObjName);
		NewFont->FontCacheType = EFontCacheType::Runtime;
		NewFont->RuntimeFontSource = ERuntimeFontSource::Asset;
		NewFont->LegacyFontSize = LegacySize;
		NewFont->LegacyFontName = FName(TEXT("Default"));
		NewFont->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add(
			FTypefaceEntry(FName(TEXT("Default")), Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad));
		NewFont->AddToRoot();
		return NewFont;
	}

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

		const FSlateBrush* GetOrMakeBrush(const TCHAR* CacheKey, const TCHAR* AssetName, const FVector2D& Size)
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
			Brush->DrawAs = ESlateBrushDrawType::Image;
			Brush->Tiling = ESlateBrushTileType::NoTile;
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

	FSlateRenderTransform SlabLean()
	{
		return FSlateRenderTransform(FShear2D::FromShearAngles(FVector2f(-BNUITheme::SlabLeanDeg, 0.f)));
	}

	struct FBNButtonPalette
	{
		FLinearColor BgIdle;
		FLinearColor BgHover;
		FLinearColor TextIdle;
		FLinearColor TextHover;
		FLinearColor Tick; // left tick / underline shown on hover (or always for tabs/active)
		bool bUnderline = false;
	};

	FBNButtonPalette PaletteFor(EBNButtonVariant Variant)
	{
		using namespace BNUITheme;
		const FLinearColor None = FLinearColor::Transparent;
		switch (Variant)
		{
		case EBNButtonVariant::PrimaryAmber:
		case EBNButtonVariant::ConfirmGreen:
			return { Accent, AccentHi, InkSolid, InkSolid, None };
		case EBNButtonVariant::DangerCrimson:
			return { DangerDark, FLinearColor(0.16f, 0.008f, 0.010f, 0.96f), Bone, Bone, Danger };
		case EBNButtonVariant::CyanIntel:
			return { Card, FLinearColor(0.020f, 0.040f, 0.055f, 0.96f), Bone, Bone, Nos };
		case EBNButtonVariant::ActiveTab:
			return { None, None, Bone, Bone, Accent, true };
		case EBNButtonVariant::IdleTab:
			return { None, None, Muted, Bone, Hairline, true };
		default: // SecondarySteel
			return { Card, FLinearColor(0.022f, 0.025f, 0.031f, 0.97f), Bone, AccentHi, Accent };
		}
	}

	/** Leaning slab button: sheared background, upright text, hover tick. */
	class SBNSlabButton : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBNSlabButton)
			: _Variant(EBNButtonVariant::SecondarySteel)
			, _bEnabled(true)
			, _FontSize(12)
			, _Padding(FMargin(14.f, 9.f))
			, _Justification(ETextJustify::Center)
			, _bDisplayFont(false)
			, _bGhost(false)
		{}
			SLATE_ARGUMENT(FText, Label)
			SLATE_ARGUMENT(FText, SubLabel)
			SLATE_ARGUMENT(EBNButtonVariant, Variant)
			SLATE_ARGUMENT(FOnClicked, OnClicked)
			SLATE_ARGUMENT(bool, bEnabled)
			SLATE_ARGUMENT(int32, FontSize)
			SLATE_ARGUMENT(FMargin, Padding)
			SLATE_ARGUMENT(ETextJustify::Type, Justification)
			SLATE_ARGUMENT(bool, bDisplayFont)
			SLATE_ARGUMENT(bool, bGhost) // no slab until hovered (menu lists)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Variant = InArgs._Variant;
			Palette = PaletteFor(Variant);
			bEnabledFlag = InArgs._bEnabled;
			if (InArgs._bGhost)
			{
				Palette.BgIdle = FLinearColor::Transparent;
				Palette.BgHover = BNUITheme::Ink;
				Palette.TextIdle = FLinearColor(BNUITheme::Bone.R, BNUITheme::Bone.G, BNUITheme::Bone.B, 0.78f);
			}

			const FText Label = InArgs._bDisplayFont ? FText::FromString(BNUITheme::UpperTr(InArgs._Label.ToString())) : InArgs._Label;
			TSharedRef<SVerticalBox> TextStack = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(InArgs._bDisplayFont ? BNUITheme::DisplayFont(InArgs._FontSize) : BNUITheme::Font(InArgs._FontSize, true))
				.ColorAndOpacity(this, &SBNSlabButton::GetTextColor)
				.Justification(InArgs._Justification)
			];
			if (!InArgs._SubLabel.IsEmpty())
			{
				TextStack->AddSlot().AutoHeight().Padding(FMargin(0.f, 1.f, 0.f, 0.f))
				[
					SNew(STextBlock)
					.Text(InArgs._SubLabel)
					.Font(BNUITheme::Font(11, false))
					.ColorAndOpacity(this, &SBNSlabButton::GetSubTextColor)
					.Justification(InArgs._Justification)
				];
			}

			const EBNButtonVariant ChosenVariant = Variant;
			const FOnClicked RawClicked = InArgs._OnClicked;
			const FOnClicked WrappedClicked = FOnClicked::CreateLambda([ChosenVariant, RawClicked]()
			{
				const bool bConfirm = ChosenVariant == EBNButtonVariant::PrimaryAmber || ChosenVariant == EBNButtonVariant::ConfirmGreen;
				UBNAudioSubsystem::PlayUiSfx(bConfirm ? EBNSfxId::UiConfirm : EBNSfxId::UiClick, 0.85f);
				return RawClicked.IsBound() ? RawClicked.Execute() : FReply::Handled();
			});

			const bool bUnderline = Palette.bUnderline;
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
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SBorder)
						.BorderImage(BNUITheme::WhiteBrush())
						.BorderBackgroundColor(this, &SBNSlabButton::GetBgColor)
						.RenderTransform(SlabLean())
						.RenderTransformPivot(FVector2D(0.5f, 0.5f))
					]
					+ SOverlay::Slot().HAlign(bUnderline ? HAlign_Fill : HAlign_Left).VAlign(bUnderline ? VAlign_Bottom : VAlign_Fill)
					[
						SNew(SBox)
						.WidthOverride(bUnderline ? FOptionalSize() : FOptionalSize(3.f))
						.HeightOverride(bUnderline ? FOptionalSize(3.f) : FOptionalSize())
						.RenderTransform(SlabLean())
						.RenderTransformPivot(FVector2D(0.5f, 0.5f))
						[
							SNew(SBorder)
							.BorderImage(BNUITheme::WhiteBrush())
							.BorderBackgroundColor(this, &SBNSlabButton::GetTickColor)
						]
					]
					+ SOverlay::Slot().VAlign(VAlign_Center).Padding(InArgs._Padding)
					[
						TextStack
					]
				]
			];
		}

	private:
		bool IsHot() const { return bEnabledFlag && InnerButton.IsValid() && InnerButton->IsHovered(); }

		FSlateColor GetBgColor() const
		{
			if (!bEnabledFlag)
			{
				return Palette.BgIdle.A > 0.f ? FLinearColor(0.008f, 0.009f, 0.011f, 0.70f) : FLinearColor::Transparent;
			}
			return IsHot() ? Palette.BgHover : Palette.BgIdle;
		}

		FSlateColor GetTickColor() const
		{
			if (!bEnabledFlag)
			{
				return FLinearColor::Transparent;
			}
			if (Palette.bUnderline)
			{
				return IsHot() && Variant == EBNButtonVariant::IdleTab ? BNUITheme::Muted : Palette.Tick;
			}
			return IsHot() ? Palette.Tick : FLinearColor::Transparent;
		}

		FSlateColor GetTextColor() const
		{
			if (!bEnabledFlag)
			{
				return FLinearColor(BNUITheme::Muted.R, BNUITheme::Muted.G, BNUITheme::Muted.B, 0.75f);
			}
			return IsHot() ? Palette.TextHover : Palette.TextIdle;
		}

		FSlateColor GetSubTextColor() const
		{
			if (Variant == EBNButtonVariant::PrimaryAmber || Variant == EBNButtonVariant::ConfirmGreen)
			{
				return FLinearColor(0.f, 0.f, 0.f, 0.72f);
			}
			return BNUITheme::Muted;
		}

		EBNButtonVariant Variant = EBNButtonVariant::SecondarySteel;
		FBNButtonPalette Palette;
		bool bEnabledFlag = true;
		TSharedPtr<SButton> InnerButton;
	};
}

namespace BNUITheme
{
	FString UpperTr(const FString& In)
	{
		FString Out = In;
		for (TCHAR& C : Out)
		{
			switch (C)
			{
			case TEXT('i'): C = TEXT('İ'); break;
			case TEXT('ı'): C = TEXT('I'); break;
			case TEXT('ö'): C = TEXT('Ö'); break;
			case TEXT('ü'): C = TEXT('Ü'); break;
			case TEXT('ş'): C = TEXT('Ş'); break;
			case TEXT('ç'): C = TEXT('Ç'); break;
			case TEXT('ğ'): C = TEXT('Ğ'); break;
			default: C = FChar::ToUpper(C); break;
			}
		}
		return Out;
	}

	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	FSlateFontInfo Font(int32 Size, bool bBold)
	{
		static const TSharedPtr<const FCompositeFont> BoldFont = LoadBundledCompositeFont(BodyBoldFile);
		static const TSharedPtr<const FCompositeFont> MedFont = LoadBundledCompositeFont(BodyMediumFile);
		const TSharedPtr<const FCompositeFont>& Chosen = bBold ? BoldFont : MedFont;
		if (Chosen.IsValid())
		{
			return FSlateFontInfo(Chosen, static_cast<float>(Size));
		}
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	FSlateFontInfo DisplayFont(int32 Size)
	{
		static const TSharedPtr<const FCompositeFont> Display = LoadBundledCompositeFont(DisplayFile);
		if (Display.IsValid())
		{
			return FSlateFontInfo(Display, static_cast<float>(Size));
		}
		return Font(Size, true);
	}

	UFont* HudFont(bool bDisplay)
	{
		static UFont* const Body = MakeRuntimeHudFont(BodyBoldFile, 17);
		static UFont* const Display = MakeRuntimeHudFont(DisplayFile, 44);
		UFont* Chosen = bDisplay ? Display : Body;
		return Chosen ? Chosen : GEngine->GetLargeFont();
	}

	UFont* HudNumeralFont()
	{
		static UFont* const Numeral = MakeRuntimeHudFont(DisplayFile, 120);
		return Numeral ? Numeral : GEngine->GetLargeFont();
	}

	UTexture2D* LoadUITexture(const TCHAR* AssetName)
	{
		return BrushCache().GetOrLoadTexture(AssetName);
	}

	const FSlateBrush* GetVignetteLeftBrush()
	{
		return BrushCache().GetOrMakeBrush(TEXT("VignetteLeft"), TEXT("T_UI_VignetteLeft"), FVector2D(640.f, 1080.f));
	}

	const FSlateBrush* GetVignetteFullBrush()
	{
		return BrushCache().GetOrMakeBrush(TEXT("VignetteFull"), TEXT("T_UI_VignetteFull"), FVector2D(512.f, 512.f));
	}

	const FSlateBrush* GetArtBrush(const TCHAR* AssetName)
	{
		return BrushCache().GetOrMakeBrush(AssetName, AssetName, FVector2D(1280.f, 720.f));
	}

	TSharedRef<SWidget> MakePanel(TSharedRef<SWidget> InnerContent, FMargin InnerPadding,
		const FLinearColor& Tint, const FLinearColor& EdgeAccent)
	{
		TSharedRef<SOverlay> Root = SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(Tint)
				.Padding(InnerPadding)
				[
					InnerContent
				]
			];
		if (EdgeAccent.A > 0.01f)
		{
			Root->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Fill)
			[
				SNew(SBox).WidthOverride(3.f)
				[
					SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(EdgeAccent)
				]
			];
		}
		return Root;
	}

	TSharedRef<SWidget> MakeIconTab(const TCHAR* IconAssetName, const FString& Caption, bool bSelected,
		FOnClicked OnClicked, bool bEnabled, const FLinearColor& AccentColor)
	{
		const FSlateBrush* IconBrush = BrushCache().GetOrMakeBrush(IconAssetName, IconAssetName, FVector2D(34.f, 34.f));
		const bool bHasIcon = IconBrush != WhiteBrush();
		const FLinearColor TextTint = !bEnabled ? FLinearColor(Muted.R, Muted.G, Muted.B, 0.5f) : bSelected ? Bone : Muted;

		const FOnClicked Wrapped = FOnClicked::CreateLambda([OnClicked, bSelected]()
		{
			UBNAudioSubsystem::PlayUiSfx(bSelected ? EBNSfxId::UiConfirm : EBNSfxId::UiClick, 0.80f);
			return OnClicked.IsBound() ? OnClicked.Execute() : FReply::Handled();
		});

		TSharedRef<SVerticalBox> Stack = SNew(SVerticalBox);
		if (bHasIcon)
		{
			Stack->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 4.f))
			[
				SNew(SImage).Image(IconBrush).ColorAndOpacity(TextTint)
			];
		}
		Stack->AddSlot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(BNUITheme::UpperTr(Caption))).Font(Font(11, true)).ColorAndOpacity(TextTint)
		];
		Stack->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))
		[
			SNew(SBox).HeightOverride(3.f)
			[
				SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(bSelected ? AccentColor : FLinearColor::Transparent)
			]
		];

		return SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.IsFocusable(false)
			.IsEnabled(bEnabled)
			.ContentPadding(FMargin(10.f, 4.f))
			.OnClicked(Wrapped)
			[
				Stack
			];
	}

	TSharedRef<SWidget> MakeAccentRule(float Height)
	{
		return SNew(SBox)
			.HeightOverride(FMath::Clamp(Height, 1.f, 3.f))
			[
				SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(Accent)
			];
	}

	TSharedRef<SWidget> MakeHairline(const FLinearColor& Color)
	{
		return SNew(SBox)
			.HeightOverride(1.f)
			[
				SNew(SBorder).BorderImage(WhiteBrush()).BorderBackgroundColor(Color)
			];
	}

	TSharedRef<SWidget> MakeSectionHeader(const FString& Title, const FString& RightTag, const FLinearColor& AccentColor)
	{
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(BNUITheme::UpperTr(Title)))
			.Font(DisplayFont(17))
			.ColorAndOpacity(Bone)
		];
		if (!RightTag.IsEmpty())
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 2.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(BNUITheme::UpperTr(RightTag)))
				.Font(Font(11, true))
				.ColorAndOpacity(AccentColor)
			];
		}

		return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Row]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(28.f)[MakeAccentRule(2.f)]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom)
			[
				MakeHairline()
			]
		];
	}

	TSharedRef<SWidget> MakeMWButton(const FText& Label, EBNButtonVariant Variant, FOnClicked OnClicked,
		bool bEnabled, int32 FontSize, FMargin Padding)
	{
		return SNew(SBNSlabButton)
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
		return SNew(SBNSlabButton)
			.Label(Label)
			.SubLabel(SubLabel)
			.Variant(Variant)
			.OnClicked(OnClicked)
			.bEnabled(true)
			.FontSize(bPrimary ? 26 : 22)
			.Padding(FMargin(18.f, bPrimary ? 8.f : 5.f))
			.Justification(ETextJustify::Left)
			.bDisplayFont(true)
			.bGhost(!bPrimary && !bDanger);
	}

	TSharedRef<SWidget> MakeSegmentedStatBar(const FString& Label, const FString& ValueText,
		float Fraction, const FLinearColor& FillColor)
	{
		constexpr int32 NumSegments = 12;
		const int32 LitCount = FMath::Clamp(FMath::RoundToInt(FMath::Clamp(Fraction, 0.f, 1.f) * NumSegments), 0, NumSegments);

		TSharedRef<SHorizontalBox> SegRow = SNew(SHorizontalBox);
		for (int32 i = 0; i < NumSegments; ++i)
		{
			SegRow->AddSlot()
			.FillWidth(1.f)
			.Padding(FMargin(0.f, 0.f, i < NumSegments - 1 ? 2.f : 0.f, 0.f))
			[
				SNew(SBox).HeightOverride(6.f)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush())
					.BorderBackgroundColor(i < LitCount ? FillColor : Hairline)
					.RenderTransform(SlabLean())
					.RenderTransformPivot(FVector2D(0.5f, 0.5f))
				]
			];
		}

		return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Text(FText::FromString(BNUITheme::UpperTr(Label))).Font(Font(11, true)).ColorAndOpacity(Muted)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(STextBlock).Text(FText::FromString(ValueText)).Font(Font(12, true)).ColorAndOpacity(Bone)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 8.f))
		[
			SegRow
		];
	}
}
