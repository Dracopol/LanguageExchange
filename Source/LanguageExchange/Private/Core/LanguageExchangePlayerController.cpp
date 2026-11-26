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

	// A 2D overlay isn't visible in the headset (on Quest, Slate draws it into the left eye's image and blanks the
	// scene there), so in VR the detections are shown as labels in the room instead
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		if (DetectionLabelsClass != nullptr)
		{
			DetectionLabels = GetWorld()->SpawnActor<ALanguageExchangeDetectionLabels>(DetectionLabelsClass);
		}
	}
	else if (DetectionWidgetClass != nullptr)
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

	if (IsValid(DetectionLabels))
	{
		DetectionLabels->Destroy();
		DetectionLabels = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
