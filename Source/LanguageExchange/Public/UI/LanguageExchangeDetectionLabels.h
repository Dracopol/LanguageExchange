#pragma once

#include "CoreMinimal.h"
#include "Detection/LanguageExchangeDetection.h"
#include "GameFramework/Actor.h"
#include "LanguageExchangeDetectionLabels.generated.h"

class UTextRenderComponent;

// Shows each detected object's name as 3D text in the room, along the ray from the camera to the object, facing the
// player. Only detections with a world ray get a label (the headset camera; not the webcam). The distance along the
// ray is a fixed setting for now, as the camera image gives no depth.
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
	UTextRenderComponent* GetOrCreateLabel(int32 LabelIndex);

	// Components
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	// Settings

	// How far along the ray from the camera to the object the label is placed
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels", meta = (ClampMin = "10", Units = "cm"))
	float LabelDistanceCentimeters = 80.0f;

	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels", meta = (ClampMin = "0.5", Units = "cm"))
	float TextHeightCentimeters = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Labels")
	FColor TextColor = FColor(80, 255, 80);

	// State

	// Pooled; labels beyond the current detection count are hidden
	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> Labels;

	FDelegateHandle DetectionsChangeHandle;
};
