#include "UI/LanguageExchangeDetectionWidget.h"

#include "Detection/LanguageExchangeObjectDetectionSubsystem.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Texture.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "LanguageExchangeDetectionWidget"

namespace LanguageExchange::UserInterface
{
	namespace
	{
		const FVector2D LabelPadding(6.0, 2.0);
		const FVector2D StatusPosition(16.0, 16.0);
	}
}

void ULanguageExchangeDetectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Only draws; clicks go through to the game
	SetVisibility(ESlateVisibility::HitTestInvisible);

	DetectionSubsystem = GetWorld()->GetSubsystem<ULanguageExchangeObjectDetectionSubsystem>();
	ensureMsgf(DetectionSubsystem.IsValid(),
		TEXT("The detection widget needs a game world with the detection subsystem"));
	if (DetectionSubsystem.IsValid() && IsValid(DetectionSubsystem->GetCameraTexture()))
	{
		CameraBrush.SetResourceObject(DetectionSubsystem->GetCameraTexture());
		CameraBrush.DrawAs = ESlateBrushDrawType::Image;
	}
}

int32 ULanguageExchangeDetectionWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
		InWidgetStyle, bParentEnabled);

	const ULanguageExchangeObjectDetectionSubsystem* const Subsystem = DetectionSubsystem.Get();
	if (!IsValid(Subsystem))
	{
		return BaseLayerId;
	}

	const int32 ImageLayerId = BaseLayerId + 1;
	const int32 BoxLayerId = BaseLayerId + 2;
	const int32 LabelLayerId = BaseLayerId + 3;

	// Camera image, as large as fits and centred; detection bounds are relative to it
	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
	FBox2D ImageRect(FVector2D::ZeroVector, WidgetSize);
	const FIntPoint CameraSizePixels = Subsystem->GetCameraSizePixels();
	if (IsValid(Subsystem->GetCameraTexture()) && CameraSizePixels.X > 0 && CameraSizePixels.Y > 0)
	{
		const FVector2D CameraSize(CameraSizePixels);
		const double Scale = FMath::Min(WidgetSize.X / CameraSize.X, WidgetSize.Y / CameraSize.Y);
		const FVector2D ImageSize = CameraSize * Scale;
		const FVector2D ImageOffset = (WidgetSize - ImageSize) * 0.5;
		ImageRect = FBox2D(ImageOffset, ImageOffset + ImageSize);
		FSlateDrawElement::MakeBox(OutDrawElements, ImageLayerId,
			AllottedGeometry.ToPaintGeometry(ImageSize, FSlateLayoutTransform(FVector2f(ImageOffset))),
			&CameraBrush);
	}

	FNumberFormattingOptions ConfidenceFormat;
	ConfidenceFormat.MinimumFractionalDigits = 2;
	ConfidenceFormat.MaximumFractionalDigits = 2;
	for (const FLanguageExchangeDetection& Detection : Subsystem->GetDetections())
	{
		const FBox2D Bounds(ImageRect.Min + Detection.NormalizedBounds.Min * ImageRect.GetSize(),
			ImageRect.Min + Detection.NormalizedBounds.Max * ImageRect.GetSize());
		PaintBox(OutDrawElements, AllottedGeometry, BoxLayerId, Bounds);

		// Name above the box, or inside it when the box touches the top
		const FText Label = FText::Format(LOCTEXT("DetectionLabel", "{0} {1}"),
			FText::FromName(Detection.ClassName), FText::AsNumber(Detection.Confidence, &ConfidenceFormat));
		const double LabelHeight = MeasureLabel(Label).Y;
		const FVector2D LabelTopLeft(Bounds.Min.X, FMath::Max(Bounds.Min.Y - LabelHeight, ImageRect.Min.Y));
		PaintLabel(OutDrawElements, AllottedGeometry, LabelLayerId, Label, LabelTopLeft, BoxColor);
	}

	FText StatusLine = Subsystem->GetStatusText();
	if (StatusLine.IsEmpty())
	{
		StatusLine = FText::Format(LOCTEXT("InferenceTime", "Inference {0} ms"),
			FText::AsNumber(FMath::RoundToInt(Subsystem->GetInferenceMilliseconds())));
	}
	PaintLabel(OutDrawElements, AllottedGeometry, LabelLayerId, StatusLine,
		LanguageExchange::UserInterface::StatusPosition, FLinearColor::White);

	// Labels use two layers: background and text
	return LabelLayerId + 1;
}

void ULanguageExchangeDetectionWidget::PaintBox(FSlateWindowElementList& OutDrawElements,
	const FGeometry& AllottedGeometry, const int32 LayerId, const FBox2D& Bounds) const
{
	TArray<FVector2f> Corners;
	Corners.Add(FVector2f(Bounds.Min.X, Bounds.Min.Y));
	Corners.Add(FVector2f(Bounds.Max.X, Bounds.Min.Y));
	Corners.Add(FVector2f(Bounds.Max.X, Bounds.Max.Y));
	Corners.Add(FVector2f(Bounds.Min.X, Bounds.Max.Y));
	Corners.Add(FVector2f(Bounds.Min.X, Bounds.Min.Y));
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), MoveTemp(Corners),
		ESlateDrawEffect::None, BoxColor, true, BoxThickness);
}

void ULanguageExchangeDetectionWidget::PaintLabel(FSlateWindowElementList& OutDrawElements,
	const FGeometry& AllottedGeometry, const int32 LayerId, const FText& Text, const FVector2D& TopLeft,
	const FLinearColor& BackgroundColor) const
{
	const FVector2D LabelSize = MeasureLabel(Text);
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
		AllottedGeometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(FVector2f(TopLeft))),
		FCoreStyle::Get().GetBrush("GenericWhiteBox"), ESlateDrawEffect::None, BackgroundColor);

	const FVector2D TextPosition = TopLeft + LanguageExchange::UserInterface::LabelPadding;
	FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1,
		AllottedGeometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(FVector2f(TextPosition))),
		Text, GetLabelFont(), ESlateDrawEffect::None, FLinearColor::Black);
}

FVector2D ULanguageExchangeDetectionWidget::MeasureLabel(const FText& Text) const
{
	const TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D TextSize(FontMeasure->Measure(Text, GetLabelFont()));
	return TextSize + LanguageExchange::UserInterface::LabelPadding * 2.0;
}

FSlateFontInfo ULanguageExchangeDetectionWidget::GetLabelFont() const
{
	return FCoreStyle::GetDefaultFontStyle("Bold", LabelFontSize);
}

#undef LOCTEXT_NAMESPACE
