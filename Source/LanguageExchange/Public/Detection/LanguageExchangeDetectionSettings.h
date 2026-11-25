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
	const FString& GetOnnxModelFile() const { return OnnxModelFile; }
	int32 GetOnnxRuntimeThreadCount() const { return OnnxRuntimeThreadCount; }
	bool ShouldUseXnnpack() const { return bUseXnnpack; }
	int32 GetCameraIndex() const { return CameraIndex; }
	FIntPoint GetHeadsetCameraResolutionPixels() const { return HeadsetCameraResolutionPixels; }
	int32 GetHeadsetCameraMaxFramesPerSecond() const { return HeadsetCameraMaxFramesPerSecond; }
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

	// NNE CPU runtime that runs the model on PC (NNE has no CPU runtime for Android)
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection")
	FString RuntimeName = TEXT("NNERuntimeORTCpu");

	// On Android: the same model as an ONNX file, relative to Content/, run with ONNX Runtime for Android.
	// Packaged through "Additional Non-Asset Directories to Package" (DefaultGame.ini).
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection|Android")
	FString OnnxModelFile = TEXT("LanguageExchange/Detection/Onnx/yolo11n.onnx");

	// On Android: CPU threads for ONNX Runtime. Measured on Quest 3 (YOLO11n, 640 px): 4 threads are hardly faster
	// than 2 but take cores from rendering (3-5 stale frames per second instead of none)
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection|Android",
		meta = (ClampMin = "1", ClampMax = "8"))
	int32 OnnxRuntimeThreadCount = 2;

	// On Android: run the model with ONNX Runtime's XNNPACK provider (optimised for ARM) instead of its default CPU
	// one; about 10% faster on Quest 3
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection|Android")
	bool bUseXnnpack = true;

	// Webcam (used when no headset is active): index into the list of video capture devices (the log lists them)
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection", meta = (ClampMin = "0"))
	int32 CameraIndex = 0;

	// Headset passthrough camera resolution; 640 x 480 fills the 640 px model input without scaling.
	// Supported on Quest 3: 320 x 240, 640 x 480, 800 x 600, 1280 x 960
	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection")
	FIntPoint HeadsetCameraResolutionPixels = FIntPoint(640, 480);

	UPROPERTY(Config, EditDefaultsOnly, Category = "LanguageExchange|Detection",
		meta = (ClampMin = "1", ClampMax = "60"))
	int32 HeadsetCameraMaxFramesPerSecond = 30;

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
