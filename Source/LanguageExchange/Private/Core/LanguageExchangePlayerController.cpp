#include "Core/LanguageExchangePlayerController.h"

#include "Engine/World.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "UI/LanguageExchangeDetectionLabels.h"
#include "UI/LanguageExchangeDetectionWidget.h"

ALanguageExchangePlayerController::ALanguageExchangePlayerController()
{
	DetectionWidgetClass = ULanguageExchangeDetectionWidget::StaticClass();
	DetectionLabelsClass = ALanguageExchangeDetectionLabels::StaticClass();
}

void ALanguageExchangePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (DetectionWidgetClass != nullptr)
	{
		DetectionWidget = CreateWidget<ULanguageExchangeDetectionWidget>(this, DetectionWidgetClass);
		DetectionWidget->AddToViewport();
	}

	// A 2D overlay isn't visible in the headset, so in VR the detections are shown as labels in the room
	if (DetectionLabelsClass != nullptr && UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		DetectionLabels = GetWorld()->SpawnActor<ALanguageExchangeDetectionLabels>(DetectionLabelsClass);
	}
}

void ALanguageExchangePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(DetectionWidget))
	{
		DetectionWidget->RemoveFromParent();
		DetectionWidget = nullptr;
	}

	if (IsValid(DetectionLabels))
	{
		DetectionLabels->Destroy();
		DetectionLabels = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
