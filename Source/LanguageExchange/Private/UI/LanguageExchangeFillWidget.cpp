#include "UI/LanguageExchangeFillWidget.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void ULanguageExchangeFillWidget::SetFill(const FLinearColor& InFillColor, const bool bInIsRound)
{
	FillColor = InFillColor;
	bIsRound = bInIsRound;
}

int32 ULanguageExchangeFillWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);

	if (bIsRound)
	{
		const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
		const FSlateRoundedBoxBrush RoundBrush(FLinearColor::White,
			static_cast<float>(FMath::Min(WidgetSize.X, WidgetSize.Y) * 0.5));
		FSlateDrawElement::MakeBox(OutDrawElements, BaseLayerId + 1, AllottedGeometry.ToPaintGeometry(), &RoundBrush,
			ESlateDrawEffect::None, FillColor);
	}
	else
	{
		FSlateDrawElement::MakeBox(OutDrawElements, BaseLayerId + 1, AllottedGeometry.ToPaintGeometry(),
			FCoreStyle::Get().GetBrush("GenericWhiteBox"), ESlateDrawEffect::None, FillColor);
	}

	return BaseLayerId + 1;
}
