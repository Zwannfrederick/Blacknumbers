#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ABNHUD;
class ABNRaceEvent;

/**
 * Classic NFS-style Pre-Race Briefing & Confirmation modal shown when the player
 * presses [ENTER] inside a free-roam race beacon ring ("Yarışa Girmek İster Misiniz?").
 */
class BLACKNUMBERS_API SBNRaceBriefingWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBNRaceBriefingWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ABNHUD>, OwnerHUD)
		SLATE_ARGUMENT(TWeakObjectPtr<ABNRaceEvent>, RaceEvent)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	FReply HandleEnterRace();
	FReply HandleCancel();
	TSharedRef<SWidget> MakeTile(const TCHAR* Label, const FString& Value, const FLinearColor& Color);

	TWeakObjectPtr<ABNHUD> OwnerHUD;
	TWeakObjectPtr<ABNRaceEvent> RaceEvent;
};
