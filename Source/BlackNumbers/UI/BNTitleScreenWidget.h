#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ABNHUD;
class SVerticalBox;

/**
 * Fullscreen interactive Title / Start Screen overlaying the live Karamara city viewport.
 * Shown once per interactive game session (never during headless/automation tests).
 */
class BLACKNUMBERS_API SBNTitleScreenWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBNTitleScreenWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ABNHUD>, OwnerHUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void Refresh();

private:
	TSharedRef<SWidget> BuildSidebarContent();
	TSharedRef<SWidget> BuildCampaignStatusCard();

	TWeakObjectPtr<ABNHUD> OwnerHUD;
	TSharedPtr<SVerticalBox> SidebarSlot;
	bool bConfirmNewCareer = false;
	FString StatusNotice;
};
