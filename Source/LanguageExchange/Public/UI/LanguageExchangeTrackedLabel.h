#pragma once

#include "CoreMinimal.h"
#include "UI/LabelPlacement.h"
#include "LanguageExchangeTrackedLabel.generated.h"

class UWidgetComponent;

// One object shown by ALanguageExchangeDetectionLabels: its widgets, where its frame is shown and where it glides to,
// and which classes it was detected as recently
USTRUCT()
struct FLanguageExchangeTrackedLabel
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UWidgetComponent> Frame;

	UPROPERTY()
	TObjectPtr<UWidgetComponent> Label;

	// Where the frame is shown now, and where its latest detection puts it
	LanguageExchange::UserInterface::FLabelPlacement ShownPlacement;
	LanguageExchange::UserInterface::FLabelPlacement TargetPlacement;

	// Confidences per class name, added up over the detections and fading over time; the strongest one is shown
	TMap<FString, float> ClassScores;
	FString ShownClassName;
	float ShownConfidence = 0.0f;

	double LastSeenSeconds = 0.0;
};
