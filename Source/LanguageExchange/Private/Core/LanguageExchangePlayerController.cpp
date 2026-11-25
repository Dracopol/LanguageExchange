#include "Core/LanguageExchangePlayerController.h"

#include "UI/LanguageExchangeDetectionWidget.h"

ALanguageExchangePlayerController::ALanguageExchangePlayerController()
{
	DetectionWidgetClass = ULanguageExchangeDetectionWidget::StaticClass();
}

void ALanguageExchangePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController() && DetectionWidgetClass != nullptr)
	{
		DetectionWidget = CreateWidget<ULanguageExchangeDetectionWidget>(this, DetectionWidgetClass);
		DetectionWidget->AddToViewport();
	}
}

void ALanguageExchangePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(DetectionWidget))
	{
		DetectionWidget->RemoveFromParent();
		DetectionWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
