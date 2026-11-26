#include "UI/LanguageExchangeFrameWidget.h"

#include "Rendering/DrawElements.h"

int32 ULanguageExchangeFrameWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);

	// Inset by half the thickness so the whole line stays inside the widget
	const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
	const float Inset = LineThicknessPixels * 0.5f;
	TArray<FVector2f> Corners;
	Corners.Add(FVector2f(Inset, Inset));
	Corners.Add(FVector2f(Size.X - Inset, Inset));
	Corners.Add(FVector2f(Size.X - Inset, Size.Y - Inset));
	Corners.Add(FVector2f(Inset, Size.Y - Inset));
	Corners.Add(FVector2f(Inset, Inset));
	FSlateDrawElement::MakeLines(OutDrawElements, BaseLayerId + 1, AllottedGeometry.ToPaintGeometry(),
		MoveTemp(Corners), ESlateDrawEffect::None, FrameColor, true, LineThicknessPixels);

	return BaseLayerId + 1;
}
