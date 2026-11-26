#pragma once

#include "CoreMinimal.h"
#include "Detection/LanguageExchangeDetection.h"
#include "GameFramework/Actor.h"
#include "LanguageExchangeDetectionLabels.generated.h"

class UWidgetComponent;

// Shows each detected object in the room: a frame over the object and a name panel above it, facing the player.
// Only detections with a world ray are shown (the headset camera; not the webcam).
// The camera image has no depth, so the distance is estimated from the object's typical size and how large it appears
// (unknown classes use a fixed distance). Placed at the object's distance, frame and panel line up with it for both
// eyes even though the camera sits a few centimetres from them.
UCLASS()
class ALanguageExchangeDetectionLabels : public AActor
{
	GENERATED_BODY()

public:
	// Setup
	ALanguageExchangeDetectionLabels();

protected:
	// Setup
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Event handlers
	void HandleDetectionsChange(TConstArrayView<FLanguageExchangeDetection> Detections);

	// Functions
	void ShowDetection(int32 DetectionIndex, const FLanguageExchangeDetection& Detection);
	void ShowTestLabel();
	float EstimateDistanceCentimeters(const FLanguageExchangeDetection& Detection) const;
	UWidgetComponent* GetOrCreateWidget(TArray<TObjectPtr<UWidgetComponent>>& Pool, int32 Index,
		TSubclassOf<UUserWidget> WidgetClass, const FIntPoint& SizePixels, const FVector2D& Pivot);

	// Components
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	// Settings

	// Distance for classes without a typical size
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels", meta = (ClampMin = "10", Units = "cm"))
	float DefaultDistanceCentimeters = 80.0f;

	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels", meta = (ClampMin = "10", Units = "cm"))
	float MinimumDistanceCentimeters = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels", meta = (ClampMin = "10", Units = "cm"))
	float MaximumDistanceCentimeters = 500.0f;

	// Rough largest dimension per class name, for estimating distance from apparent size
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels")
	TMap<FString, float> TypicalSizeCentimeters;

	// Panel width at 1 m distance; scaled with distance so the panel always looks the same size
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels", meta = (ClampMin = "1", Units = "cm"))
	float LabelWidthAtOneMeterCentimeters = 15.0f;

	// Resolution of the label panel; its aspect ratio sets the panel's height
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels")
	FIntPoint LabelSizePixels = FIntPoint(384, 96);

	// Resolution of the frame widget, which is stretched to the object's size
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels")
	FIntPoint FrameSizePixels = FIntPoint(256, 256);

	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels")
	FLinearColor HighlightColor = FLinearColor(0.2f, 1.0f, 0.3f);

	// State

	// Pooled, one per shown detection; extra ones are hidden
	UPROPERTY()
	TArray<TObjectPtr<UWidgetComponent>> Labels;

	UPROPERTY()
	TArray<TObjectPtr<UWidgetComponent>> Frames;

	FDelegateHandle DetectionsChangeHandle;
};
