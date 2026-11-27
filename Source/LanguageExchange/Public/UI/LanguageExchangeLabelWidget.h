#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LanguageExchangeLabelWidget.generated.h"

// One detection label for a world-space widget component: coloured text centred on a dark, semi-transparent panel,
// readable over the passthrough image. Draws itself in C++, so it needs no Widget Blueprint.
UCLASS()
class ULanguageExchangeLabelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Functions
	void SetLabel(const FText& InText, const FLinearColor& InTextColor);

protected:
	// Functions
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// Settings
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|UI")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);

	// Relative to the widget's height, so the text fills the panel at any draw size; text wider than the widget is
	// made smaller
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|UI", meta = (ClampMin = "0.1", ClampMax = "1"))
	float FontSizeToHeightRatio = 0.55f;

	// Space between the text and the panel's left and right edges, relative to the widget's height. The panel is as
	// wide as its text, centred in the widget.
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|UI", meta = (ClampMin = "0", ClampMax = "1"))
	float PaddingToHeightRatio = 0.25f;

	// State
	FText Text;
	FLinearColor TextColor = FLinearColor::White;
};
