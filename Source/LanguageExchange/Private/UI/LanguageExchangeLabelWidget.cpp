#include "UI/LanguageExchangeLabelWidget.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

void ULanguageExchangeLabelWidget::SetLabel(const FText& InText, const FLinearColor& InTextColor)
{
	Text = InText;
	TextColor = InTextColor;
}

int32 ULanguageExchangeLabelWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);

	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
	FSlateDrawElement::MakeBox(OutDrawElements, BaseLayerId + 1, AllottedGeometry.ToPaintGeometry(),
		FCoreStyle::Get().GetBrush("GenericWhiteBox"), ESlateDrawEffect::None, BackgroundColor);

	const FSlateFontInfo Font =
		FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(1, FMath::RoundToInt(WidgetSize.Y * FontSizeToHeightRatio)));
	const TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D TextSize(FontMeasure->Measure(Text, Font));
	const FVector2D TextPosition = (WidgetSize - TextSize) * 0.5;
	FSlateDrawElement::MakeText(OutDrawElements, BaseLayerId + 2,
		AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(FVector2f(TextPosition))), Text, Font,
		ESlateDrawEffect::None, TextColor);

	return BaseLayerId + 2;
}
