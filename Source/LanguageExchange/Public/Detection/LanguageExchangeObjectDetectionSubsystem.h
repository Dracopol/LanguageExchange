#pragma once

#include "CoreMinimal.h"
#include "Detection/LanguageExchangeDetection.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tasks/Task.h"
#include "LanguageExchangeObjectDetectionSubsystem.generated.h"

class UMediaPlayer;
class UMediaTexture;
class UTextureRenderTarget2D;
struct FStreamableHandle;

namespace LanguageExchange::Detection
{
	class FYoloModel;
	struct FDetectionJob;
}

// Runs object detection on the camera image a few times per second in game and PIE worlds.
// The camera goes through Unreal's Media Framework (a webcam on PC); the model runs on an NNE CPU runtime.
// Settings: Project Settings > LanguageExchange > Object Detection.
UCLASS()
class ULanguageExchangeObjectDetectionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// Setup
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// Update

	// Ticks only while a frame is being read back or detected
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	// Functions
	UMediaTexture* GetCameraTexture() const { return CameraTexture; }
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
	UFUNCTION()
	void HandleCameraOpen(FString OpenedUrl);

	UFUNCTION()
	void HandleCameraOpenFail(FString FailedUrl);

	void HandleModelLoad();
	void HandleDetectionTimer();

	// Functions
	void OpenCamera();
	void LoadModelAsync();
	void StartFrameCapture();
	void QueueReadbackPoll();
	void StartInference();
	void FinishInference();
	void RecordFrame(const TSharedRef<LanguageExchange::Detection::FDetectionJob>& Job);
	void LogDetectionsIfChanged();

	// State
	UPROPERTY()
	TObjectPtr<UMediaPlayer> CameraPlayer;

	UPROPERTY()
	TObjectPtr<UMediaTexture> CameraTexture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> ModelInputTarget;

	TSharedPtr<FStreamableHandle> ModelLoadHandle;
	TSharedPtr<LanguageExchange::Detection::FYoloModel> Model;
	TSharedPtr<LanguageExchange::Detection::FDetectionJob> CurrentJob;
	UE::Tasks::FTask InferenceTask;
	FTimerHandle DetectionTimerHandle;
	bool bIsCameraOpen = false;
	bool bIsInferenceRunning = false;
	FText ErrorText;

	TArray<FLanguageExchangeDetection> Detections;
	float InferenceMilliseconds = 0.0f;
	FString LastLoggedClassNames;

	// For the summary logged on shutdown; the first run is reported apart because it includes warm-up
	int32 InferenceCount = 0;
	double InferenceMillisecondsTotal = 0.0;
	float FirstInferenceMilliseconds = 0.0f;

	// Recording (console: LanguageExchange.Detection.Record 1); the folder is created on the first recorded frame
	FString RecordingDirectory;
	int32 RecordedFrameCount = 0;
};
