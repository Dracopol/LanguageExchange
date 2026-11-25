#pragma once

#include "CoreMinimal.h"
#include "Detection/LanguageExchangeDetection.h"

#include <atomic>

class FRHIGPUTextureReadback;

namespace LanguageExchange::Detection
{
	// One camera frame on its way through the detector: GPU copy, readback to the CPU, inference.
	// Shared between the game thread, the render thread and the inference task; only one of them uses it at a time.
	struct FDetectionJob
	{
		// Set by the game thread when the frame is captured
		int32 InputSizePixels = 0;
		// Where the camera image sits inside the square model input (letterbox), normalised to the input size
		FVector2D ContentOffset = FVector2D::ZeroVector;
		FVector2D ContentSize = FVector2D::UnitVector;
		// The render target is RGBA rather than BGRA (FColor's order)
		bool bIsRedFirst = false;
		TSharedPtr<FRHIGPUTextureReadback> Readback;

		// Set by the render thread when the readback is ready (Pixels stays empty if the readback failed)
		std::atomic<bool> bIsReadbackPollQueued = false;
		std::atomic<bool> bArePixelsReady = false;
		TArray<FColor> Pixels;

		// Set by the game thread before inference: the frame is saved, with detections down to the recording threshold
		bool bIsRecorded = false;

		// Set by the inference task; bounds are normalised to the model input
		TArray<FLanguageExchangeDetection> Detections;
		float InferenceMilliseconds = 0.0f;
	};
}
