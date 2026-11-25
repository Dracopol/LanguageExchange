#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "LanguageExchangeDetectionWidget.generated.h"

class ULanguageExchangeObjectDetectionSubsystem;

// Full-screen overlay for the detection prototype: the camera image, a box and name around each detected object,
// and a status line. Draws everything itself, so it needs no Widget Blueprint.
UCLASS()
class ULanguageExchangeDetectionWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// Setup
	virtual void NativeConstruct() override;

	// Functions
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// Functions
	void PaintBox(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32 LayerId,
		const FBox2D& Bounds) const;
	void PaintLabel(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, int32 LayerId,
		const FText& Text, const FVector2D& TopLeft, const FLinearColor& BackgroundColor) const;
	FVector2D MeasureLabel(const FText& Text) const;
	FSlateFontInfo GetLabelFont() const;

	// Settings
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|UI")
	FLinearColor BoxColor = FLinearColor(0.0f, 0.8f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|UI", meta = (ClampMin = "1"))
	float BoxThickness = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|UI", meta = (ClampMin = "6"))
	int32 LabelFontSize = 20;

	// State
	TWeakObjectPtr<ULanguageExchangeObjectDetectionSubsystem> DetectionSubsystem;
	FSlateBrush CameraBrush;
};
