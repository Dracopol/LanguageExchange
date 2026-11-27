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
	const TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	// As large as the height allows, smaller if the text would be wider than the widget
	const float FullFontSize =
		FMath::Max(1.0f, FMath::RoundToFloat(static_cast<float>(WidgetSize.Y) * FontSizeToHeightRatio));
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FullFontSize);
	FVector2D TextSize(FontMeasure->Measure(Text, Font));
	const double TextPadding = WidgetSize.Y * PaddingToHeightRatio;
	const double MaximumTextWidth = FMath::Max(1.0, WidgetSize.X - 2.0 * TextPadding);
	if (TextSize.X > MaximumTextWidth)
	{
		const float FittingFontSize = static_cast<float>(FullFontSize * MaximumTextWidth / TextSize.X);
		Font.Size = FMath::Max(1.0f, FMath::FloorToFloat(FittingFontSize));
		TextSize = FVector2D(FontMeasure->Measure(Text, Font));
	}

	// The panel fits the text
	const FVector2D PanelSize(FMath::Min(TextSize.X + 2.0 * TextPadding, WidgetSize.X), WidgetSize.Y);
	const FVector2D PanelPosition((WidgetSize.X - PanelSize.X) * 0.5, 0.0);
	FSlateDrawElement::MakeBox(OutDrawElements, BaseLayerId + 1,
		AllottedGeometry.ToPaintGeometry(PanelSize, FSlateLayoutTransform(FVector2f(PanelPosition))),
		FCoreStyle::Get().GetBrush("GenericWhiteBox"), ESlateDrawEffect::None, BackgroundColor);

	const FVector2D TextPosition = (WidgetSize - TextSize) * 0.5;
	FSlateDrawElement::MakeText(OutDrawElements, BaseLayerId + 2,
		AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(FVector2f(TextPosition))), Text, Font,
		ESlateDrawEffect::None, TextColor);

	return BaseLayerId + 2;
}
