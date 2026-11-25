#pragma once

#include "CoreMinimal.h"

namespace LanguageExchange::Detection
{
	// Runs a neural network with one float input and one float output tensor on the CPU. Implementations wrap a
	// runtime: NNE on PC, ONNX Runtime on Android (NNE has no CPU runtime there).
	class IModelRunner
	{
	public:
		virtual ~IModelRunner() = default;

		// Fixed tensor shapes, e.g. { 1, 3, 640, 640 } and { 1, 84, 8400 }
		virtual TConstArrayView<int64> GetInputShape() const = 0;
		virtual TConstArrayView<int64> GetOutputShape() const = 0;

		// Runtime and settings, for the log
		virtual const FString& GetDescription() const = 0;

		// Buffers must hold exactly the shapes' element counts. Not thread-safe: one run at a time.
		virtual bool Run(TConstArrayView<float> Input, TArrayView<float> Output) = 0;
	};
}
