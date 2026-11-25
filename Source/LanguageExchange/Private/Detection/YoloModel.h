#pragma once

#include "CoreMinimal.h"
#include "Detection/LanguageExchangeDetection.h"

class UNNEModelData;

namespace UE::NNE
{
	class IModelCPU;
	class IModelInstanceCPU;
}

namespace LanguageExchange::Detection
{
	// Runs an Ultralytics YOLO detection model on the CPU through NNE.
	// Model input: 1 x 3 x Size x Size, RGB in 0..1. Output: 1 x (4 + class count) x anchor count, where each anchor
	// holds a box (centre x, centre y, width, height in input pixels) followed by one score per class.
	// Not thread-safe: run one detection at a time (from any thread).
	class FYoloModel
	{
	public:
		// Setup

		// Returns nullptr and logs the reason when the model can't be created
		static TSharedPtr<FYoloModel> Create(UNNEModelData* ModelData, const FString& RuntimeName);

		FYoloModel(const TSharedPtr<UE::NNE::IModelCPU>& InModel,
			const TSharedPtr<UE::NNE::IModelInstanceCPU>& InModelInstance, int32 InInputSizePixels,
			int32 InClassCount, int32 InAnchorCount);

		// Functions

		// Pixels are InputSizePixels x InputSizePixels, row by row. Returned bounds are normalised to the input image.
		TArray<FLanguageExchangeDetection> Detect(TConstArrayView<FColor> Pixels, float MinimumConfidence,
			float MaximumBoxOverlap, float& OutInferenceMilliseconds);

		int32 GetInputSizePixels() const { return InputSizePixels; }

		// Classes the model then never reports (boxes whose best class is one of them are dropped).
		// Call before the first Detect.
		void SetIgnoredClassIndices(TConstArrayView<int32> ClassIndices);

	private:
		// Functions
		void FillInputTensor(TConstArrayView<FColor> Pixels);
		TArray<FLanguageExchangeDetection> DecodeOutputTensor(float MinimumConfidence) const;

		// State
		TSharedPtr<UE::NNE::IModelCPU> Model;
		TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance;
		int32 InputSizePixels = 0;
		int32 ClassCount = 0;
		int32 AnchorCount = 0;
		TBitArray<> IgnoredClasses;
		TArray<float> InputTensor;
		TArray<float> OutputTensor;
	};
}
