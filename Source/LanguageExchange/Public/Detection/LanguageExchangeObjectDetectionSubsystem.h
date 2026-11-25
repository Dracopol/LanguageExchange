#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Detection/LanguageExchangeDetection.h"
#include "Subsystems/WorldSubsystem.h"
#include "LanguageExchangeObjectDetectionSubsystem.generated.h"

class ULanguageExchangeCameraSource;
class UTexture;
class UTextureRenderTarget2D;
struct FStreamableHandle;

namespace LanguageExchange::Detection
{
	class FYoloModel;
	struct FDetectionJob;
}

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDetectionsChange, TConstArrayView<FLanguageExchangeDetection> /* Detections */);

// Runs object detection on the camera image a few times per second in game and PIE worlds.
// The camera is the headset's passthrough camera when a headset is active (on the Quest, or over Link), otherwise a
// webcam; the model runs on an NNE CPU runtime. Settings: Project Settings > LanguageExchange > Object Detection.
UCLASS()
class ULanguageExchangeObjectDetectionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// Events

	// After every detection run (a few times per second), also when nothing was found
	FOnDetectionsChange OnDetectionsChange;

	// Setup
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// Update

	// Ticks only while a frame is being read back or detected
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	// Functions

	// The latest camera image, or nullptr; its size is GetCameraSizePixels()
	UTexture* GetCameraTexture() const;
	FIntPoint GetCameraSizePixels() const;

	// Latest detections; bounds are normalised to the camera image
	const TArray<FLanguageExchangeDetection>& GetDetections() const { return Detections; }
	float GetInferenceMilliseconds() const { return InferenceMilliseconds; }
	// Empty once the camera and the model are running
	FText GetStatusText() const;

protected:
	// Setup
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	// Event handlers
	void HandleModelLoad();
	void HandleDetectionTimer();

	// Functions
	void OpenCamera();
	void LoadModelAsync();
	void LoadNneModelAsync();
	void LoadOnnxModelAsync();
	void FinishModelLoad(const TSharedPtr<LanguageExchange::Detection::FYoloModel>& LoadedModel);
	void StartFrameCapture();
	void QueueReadbackPoll();
	void StartInference();
	void FinishInference();
	void RecordFrame(const TSharedRef<LanguageExchange::Detection::FDetectionJob>& Job);
	void LogDetectionsIfChanged();

	// State
	UPROPERTY()
	TObjectPtr<ULanguageExchangeCameraSource> CameraSource;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> ModelInputTarget;

	TSharedPtr<FStreamableHandle> ModelLoadHandle;
	TSharedPtr<LanguageExchange::Detection::FYoloModel> Model;
	// The model while it's being created on a worker thread (ONNX Runtime on Android); polled by the detection timer
	TFuture<TSharedPtr<LanguageExchange::Detection::FYoloModel>> PendingModel;
	TSharedPtr<LanguageExchange::Detection::FDetectionJob> CurrentJob;
	TFuture<void> InferenceFuture;
	FTimerHandle DetectionTimerHandle;
	bool bHasCapturedFrame = false;
	bool bIsInferenceRunning = false;
	FText ErrorText;

	TArray<FLanguageExchangeDetection> Detections;
	float InferenceMilliseconds = 0.0f;
	FString LastLoggedClassNames;

	// For the summary logged on shutdown; the first run is reported apart because it includes warm-up
	int32 InferenceCount = 0;
	double InferenceMillisecondsTotal = 0.0;
	float FirstInferenceMilliseconds = 0.0f;

	// For the statistics logged every few runs while the app runs
	int32 RecentInferenceCount = 0;
	double RecentInferenceMillisecondsTotal = 0.0;
	float RecentInferenceMaximumMilliseconds = 0.0f;

	// Recording (console: LanguageExchange.Detection.Record 1); the folder is created on the first recorded frame
	FString RecordingDirectory;
	int32 RecordedFrameCount = 0;
};
