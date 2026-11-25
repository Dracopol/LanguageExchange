#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "LanguageExchangeDetectionSettings.generated.h"

class UNNEModelData;

// Project Settings > LanguageExchange > Object Detection
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Object Detection"))
class ULanguageExchangeDetectionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Functions
	virtual FName GetCategoryName() const override { return TEXT("LanguageExchange"); }

	const TSoftObjectPtr<UNNEModelData>& GetModel() const { return Model; }
	const FString& GetRuntimeName() const { return RuntimeName; }
	int32 GetCameraIndex() const { return CameraIndex; }
	float GetMinimumConfidence() const { return MinimumConfidence; }
	float GetMaximumBoxOverlap() const { return MaximumBoxOverlap; }
	float GetDetectionIntervalSeconds() const { return DetectionIntervalSeconds; }
	const TArray<FName>& GetIgnoredClassNames() const { return IgnoredClassNames; }
	float GetRecordingMinimumConfidence() const { return RecordingMinimumConfidence; }

private:
	// Settings

	// Detection model: an Ultralytics YOLO ONNX file imported as NNE model data
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection")
	TSoftObjectPtr<UNNEModelData> Model;

	// NNE CPU runtime that runs the model. NNERuntimeORTCpu (ONNX Runtime) is desktop only;
	// NNERuntimeIREECpu also runs on Android
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection")
	FString RuntimeName = TEXT("NNERuntimeORTCpu");

	// Index into the list of video capture devices (the log lists them at startup)
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection", meta = (ClampMin = "0"))
	int32 CameraIndex = 0;

	// Detections below this confidence are dropped
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection",
		meta = (ClampMin = "0", ClampMax = "1"))
	float MinimumConfidence = 0.6f;

	// Boxes of the same class that overlap more than this (intersection over union) count as one object
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection",
		meta = (ClampMin = "0", ClampMax = "1"))
	float MaximumBoxOverlap = 0.45f;

	// Time between two detections; detection doesn't need to run every frame
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection",
		meta = (ClampMin = "0.01", Units = "s"))
	float DetectionIntervalSeconds = 0.25f;

	// Classes the detector skips, by model class name (e.g. "person")
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection")
	TArray<FName> IgnoredClassNames;

	// While recording (console: LanguageExchange.Detection.Record 1), detections down to this confidence are
	// written to the recording, so near misses show up too. The screen still uses MinimumConfidence.
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection",
		meta = (ClampMin = "0", ClampMax = "1"))
	float RecordingMinimumConfidence = 0.25f;
};
