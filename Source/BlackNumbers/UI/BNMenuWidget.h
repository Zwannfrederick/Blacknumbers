#pragma once

#include "CoreMinimal.h"
#include "Garage/BNGarageTypes.h"
#include "Widgets/SCompoundWidget.h"

class ABNHUD;
class ABNVehiclePawn;
class SBorder;
class SVerticalBox;
class UGameInstance;
class UWorld;

UENUM(BlueprintType)
enum class EBNMenuTab : uint8
{
	Races = 0,
	Garage,
	BlackNumbers,
	Controls,
	Settings,
};

/**
 * NFS Most Wanted-inspired interactive Slate menu & live 3D Safehouse Garage shell:
 * Races, transparent-center 360-degree Selin'in Garaji, interactive 16-member BLACK NUMBERS
 * Blacklist dossier with concept-art hero cards, RTX 3050 Ti Settings, and keybindings.
 */
class BLACKNUMBERS_API SBNGameMenuWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBNGameMenuWidget)
		: _InitialTab(EBNMenuTab::Races)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<ABNHUD>, OwnerHUD)
		SLATE_ARGUMENT(EBNMenuTab, InitialTab)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetActiveTab(EBNMenuTab NewTab);
	EBNMenuTab GetActiveTab() const { return ActiveTab; }
	void Refresh();

	static const FLinearColor BackdropColor;
	static const FLinearColor PanelBgColor;
	static const FLinearColor CardBgColor;
	static const FLinearColor CardActiveBgColor;
	static const FLinearColor AccentAmberColor;
	static const FLinearColor AccentCyanColor;
	static const FLinearColor AccentGreenColor;
	static const FLinearColor MutedTextColor;
	static const FLinearColor WhiteTextColor;

	static const FSlateBrush* WhiteBrush();
	static FSlateFontInfo MenuFont(int32 Size, bool bBold = true);
	static UWorld* GetHUDWorld(const TWeakObjectPtr<ABNHUD>& HUD);
	static UGameInstance* GetGI(const TWeakObjectPtr<ABNHUD>& HUD);
	static ABNVehiclePawn* GetPlayerCar(const TWeakObjectPtr<ABNHUD>& HUD);
	static TSharedRef<SWidget> MakeStyledButton(const FText& Label, FLinearColor BgColor, FOnClicked OnClicked, bool bEnabled = true);

private:
	TSharedRef<SWidget> BuildHeaderBar();
	TSharedRef<SWidget> BuildRacesTab();
	TSharedRef<SWidget> BuildGarageTab();
	TSharedRef<SWidget> BuildBlackNumbersTab();
	TSharedRef<SWidget> BuildSettingsTab();
	TSharedRef<SWidget> BuildControlsTab();
	TSharedRef<SWidget> MakeActionButton(const FText& Label, FLinearColor BgColor, FOnClicked OnClicked, bool bEnabled = true);
	TSharedRef<SWidget> MakeStatBar(const FString& Label, const FString& ValueText, float Fraction, FLinearColor FillColor);

	TWeakObjectPtr<ABNHUD> OwnerHUD;
	EBNMenuTab ActiveTab = EBNMenuTab::Races;
	TOptional<EBNCarId> PreviewedCar;
	int32 SelectedBlacklistRank = 0;
	TSharedPtr<SVerticalBox> RootSlot;
	FString ActionFeedback;
};

/** Compact Most Wanted incoming-dispatch style storyboard card for narrative beats & epilogue. */
class BLACKNUMBERS_API SBNStoryCardWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBNStoryCardWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ABNHUD>, OwnerHUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void Refresh();

private:
	TWeakObjectPtr<ABNHUD> OwnerHUD;
	TSharedPtr<SVerticalBox> CardSlot;
};
