#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LanguageExchangeFillWidget.generated.h"

// A filled shape covering the widget, for simple world-space shapes through a widget component (a pointer beam, a dot):
// a rectangle, or a circle when the widget is square and round. Draws itself in C++.
UCLASS()
class ULanguageExchangeFillWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Functions

	// Round: corners rounded by half the widget's smaller side (a circle on a square widget)
	void SetFill(const FLinearColor& InFillColor, bool bInIsRound);

protected:
	// Functions
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// State
	FLinearColor FillColor = FLinearColor::White;
	bool bIsRound = false;
};
