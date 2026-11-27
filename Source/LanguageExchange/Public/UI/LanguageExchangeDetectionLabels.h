#pragma once

#include "CoreMinimal.h"
#include "Detection/LanguageExchangeDetection.h"
#include "GameFramework/Actor.h"
#include "UI/LanguageExchangeTrackedLabel.h"
#include "LanguageExchangeDetectionLabels.generated.h"

class UUserWidget;
class UWidgetComponent;

// Shows each detected object in the room: a frame over the object and a name panel on its top edge, facing the player.
// Only detections with a world ray are shown (the headset camera; not the webcam).
// Detections arrive a few times per second. Each is matched to an object already shown (by its direction from the
// camera), whose frame then glides to the new place; the name shown is the class the object was detected as most over
// the last moment. Objects stay shown for a moment after their last detection, so a missed detection doesn't make
// them flicker.
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

	// Update
	virtual void Tick(float DeltaSeconds) override;

protected:
	// Setup
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Event handlers
	void HandleDetectionsChange(TConstArrayView<FLanguageExchangeDetection> Detections);

	// Functions
	double GetMatchAngleRadians(const FLanguageExchangeTrackedLabel& TrackedLabel,
		const FLanguageExchangeDetection& Detection) const;
	double GetMaximumMatchAngleRadians(const FLanguageExchangeDetection& Detection) const;
	void UpdateTrackedLabel(FLanguageExchangeTrackedLabel& TrackedLabel, const FLanguageExchangeDetection& Detection,
		double NowSeconds) const;
	void ShowTrackedLabel(const FLanguageExchangeTrackedLabel& TrackedLabel) const;
	LanguageExchange::UserInterface::FLabelPlacement ComputePlacement(const FLanguageExchangeDetection& Detection,
		const FString& ClassName) const;
	float EstimateDistanceCentimeters(const FString& ClassName, TConstArrayView<FVector> CornerDirections) const;
	FLanguageExchangeTrackedLabel CreateTrackedLabel();
	void ReleaseTrackedLabel(const FLanguageExchangeTrackedLabel& TrackedLabel);
	UWidgetComponent* CreateWidgetComponent(TSubclassOf<UUserWidget> WidgetClass, const FIntPoint& SizePixels,
		const FVector2D& Pivot);
	void ShowTestLabel();

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

	// How long an object stays shown after its last detection
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels", meta = (ClampMin = "0", Units = "s"))
	float HoldSeconds = 1.0f;

	// How quickly a frame glides to its latest detection: the time to cover about two thirds of the way
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels", meta = (ClampMin = "0.01", Units = "s"))
	float GlideSeconds = 0.1f;

	// Past detections count towards the shown name with a weight that fades over about this time
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels", meta = (ClampMin = "0.05", Units = "s"))
	float ClassMemorySeconds = 1.0f;

	// A detection belongs to a shown object when their directions from the camera are within this angle, or within
	// half the detection's size if that is larger
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels", meta = (ClampMin = "0", Units = "deg"))
	float MinimumMatchAngleDegrees = 4.0f;

	// Panel width at 1 m distance; scaled with distance so the panel always looks the same size
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels", meta = (ClampMin = "1", Units = "cm"))
	float LabelWidthAtOneMeterCentimeters = 15.0f;

	// Resolution of the label panel; its aspect ratio sets the panel's height
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels")
	FIntPoint LabelSizePixels = FIntPoint(384, 96);

	// Resolution of the frame widget, which is stretched to the object's size
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels")
	FIntPoint FrameSizePixels = FIntPoint(256, 256);

	// Frame line thickness at 1 m distance; scaled with distance so the line always looks equally thin
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels", meta = (ClampMin = "0.05", Units = "cm"))
	float FrameLineThicknessAtOneMeterCentimeters = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels")
	FLinearColor HighlightColor = FLinearColor(0.2f, 1.0f, 0.3f);

	// State

	// The objects shown now
	UPROPERTY()
	TArray<FLanguageExchangeTrackedLabel> TrackedLabels;

	// Widgets of objects no longer shown, reused for new ones
	UPROPERTY()
	TArray<TObjectPtr<UWidgetComponent>> SpareFrames;

	UPROPERTY()
	TArray<TObjectPtr<UWidgetComponent>> SpareLabels;

	FDelegateHandle DetectionsChangeHandle;
};
