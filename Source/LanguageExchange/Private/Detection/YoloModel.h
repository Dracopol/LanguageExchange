#pragma once

#include "CoreMinimal.h"
#include "Detection/LanguageExchangeDetection.h"
#include "Detection/ModelRunner.h"

namespace LanguageExchange::Detection
{
	// An Ultralytics YOLO detection model: prepares the input, runs it with a model runner (NNE or ONNX Runtime)
	// and decodes the output. Model input: 1 x 3 x Size x Size, RGB in 0..1. Output: 1 x (4 + class count) x anchor
	// count, where each anchor holds a box (centre x, centre y, width, height in input pixels) and one score per class.
	// Not thread-safe: run one detection at a time (from any thread).
	class FYoloModel
	{
	public:
		// Setup

		// Returns nullptr and logs the reason when the runner's shapes don't fit a YOLO detection model
		static TSharedPtr<FYoloModel> Create(TUniquePtr<IModelRunner> Runner);

		FYoloModel(TUniquePtr<IModelRunner> InRunner, int32 InInputSizePixels, int32 InClassCount, int32 InAnchorCount);

		// Functions

		// Pixels are InputSizePixels x InputSizePixels, row by row. Returned bounds are normalised to the input image.
		TArray<FLanguageExchangeDetection> Detect(TConstArrayView<FColor> Pixels, float MinimumConfidence,
			float MaximumBoxOverlap, float& OutInferenceMilliseconds);

		int32 GetInputSizePixels() const { return InputSizePixels; }
		const FString& GetRunnerDescription() const { return Runner->GetDescription(); }

		// Classes the model then never reports (boxes whose best class is one of them are dropped).
		// Call before the first Detect.
		void SetIgnoredClassIndices(TConstArrayView<int32> ClassIndices);

	private:
		// Functions
		void FillInputTensor(TConstArrayView<FColor> Pixels);
		TArray<FLanguageExchangeDetection> DecodeOutputTensor(float MinimumConfidence) const;

		// State
		TUniquePtr<IModelRunner> Runner;
		int32 InputSizePixels = 0;
		int32 ClassCount = 0;
		int32 AnchorCount = 0;
		TBitArray<> IgnoredClasses;
		TArray<float> InputTensor;
		TArray<float> OutputTensor;
	};
}
