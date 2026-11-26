#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LanguageExchangeFrameWidget.generated.h"

// A rectangular outline filling the widget, for highlighting a detected object through a world-space widget
// component. Draws itself in C++. The line thickness is set per axis, so the outline stays even when the component is
// stretched to the object's size.
UCLASS()
class ULanguageExchangeFrameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Functions

	// Line thickness in widget pixels: X for the left and right edges, Y for the top and bottom edges
	void SetFrame(const FLinearColor& InFrameColor, const FVector2D& InLineThicknessPixels);

protected:
	// Functions
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// State
	FLinearColor FrameColor = FLinearColor::Green;
	FVector2D LineThicknessPixels = FVector2D(6.0, 6.0);
};
