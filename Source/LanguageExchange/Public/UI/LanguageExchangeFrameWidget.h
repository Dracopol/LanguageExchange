#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LanguageExchangeFrameWidget.generated.h"

// A rectangular outline filling the widget, for highlighting a detected object through a world-space widget
// component (which renders in both eyes, unlike line batches with mobile multiview). Draws itself in C++.
UCLASS()
class ULanguageExchangeFrameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Functions
	void SetFrameColor(const FLinearColor& InFrameColor) { FrameColor = InFrameColor; }

protected:
	// Functions
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// Settings
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|UI", meta = (ClampMin = "1"))
	float LineThicknessPixels = 6.0f;

	// State
	FLinearColor FrameColor = FLinearColor::Green;
};
