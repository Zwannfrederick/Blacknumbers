#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ABNHUD;
class SVerticalBox;

/**
 * Full-screen interactive Karamara City Map opened with [M] / [TAB]:
 * Renders the complete 2 km x 2 km UBNRoadGraphSubsystem road network, Mara Sea,
 * all 6 districts, 11 Races, Selin's Garage, 6 Cooldown Spots, and player heading.
 * Left-clicking any icon or road plots an instant A* GPS route back to the live HUD radar.
 */
class BLACKNUMBERS_API SBNWorldMapWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBNWorldMapWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ABNHUD>, OwnerHUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void RefreshGpsSidebar();

private:
	void SetDestination(const FVector& WorldPos, const FString& Label);
	FReply HandleClearGps();
	FReply HandleClose();

	TWeakObjectPtr<ABNHUD> OwnerHUD;
	TSharedPtr<SVerticalBox> GpsStatusBox;
};
