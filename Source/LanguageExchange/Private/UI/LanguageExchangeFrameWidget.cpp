#include "UI/LanguageExchangeFrameWidget.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void ULanguageExchangeFrameWidget::SetFrame(const FLinearColor& InFrameColor, const FVector2D& InLineThicknessPixels)
{
	FrameColor = InFrameColor;
	LineThicknessPixels = InLineThicknessPixels;
}

int32 ULanguageExchangeFrameWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);

	// Four bars inside the widget's edges: top and bottom across the full width, left and right between them
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FVector2D Thickness(FMath::Clamp(LineThicknessPixels.X, 1.0, Size.X * 0.5),
		FMath::Clamp(LineThicknessPixels.Y, 1.0, Size.Y * 0.5));
	const FBox2D Bars[] = {
		FBox2D(FVector2D(0.0, 0.0), FVector2D(Size.X, Thickness.Y)),
		FBox2D(FVector2D(0.0, Size.Y - Thickness.Y), Size),
		FBox2D(FVector2D(0.0, Thickness.Y), FVector2D(Thickness.X, Size.Y - Thickness.Y)),
		FBox2D(FVector2D(Size.X - Thickness.X, Thickness.Y), FVector2D(Size.X, Size.Y - Thickness.Y)),
	};

	const int32 BarLayerId = BaseLayerId + 1;
	const FSlateBrush* const Brush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	for (const FBox2D& Bar : Bars)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, BarLayerId,
			AllottedGeometry.ToPaintGeometry(Bar.GetSize(), FSlateLayoutTransform(FVector2f(Bar.Min))), Brush,
			ESlateDrawEffect::None, FrameColor);
	}

	return BarLayerId;
}
