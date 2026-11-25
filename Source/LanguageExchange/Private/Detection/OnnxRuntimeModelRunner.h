#pragma once

#include "CoreMinimal.h"
#include "Detection/ModelRunner.h"

struct OrtApi;
struct OrtEnv;
struct OrtMemoryInfo;
struct OrtSession;
struct OrtStatus;

namespace LanguageExchange::Detection
{
	// Runs an ONNX model with Microsoft's ONNX Runtime (C API), which comes from
	// Source/ThirdParty/LanguageExchangeOnnxRuntimeAndroid. Only usable where WITH_LANGUAGEEXCHANGE_ONNXRUNTIME is 1
	// (Android); NNE covers the other platforms.
	class FOnnxRuntimeModelRunner : public IModelRunner
	{
	public:
		// Setup

		// Returns nullptr and logs the reason when the model can't be created. With XNNPACK, ONNX Runtime's own
		// thread pool is set to one thread and XNNPACK gets ThreadCount threads.
		static TUniquePtr<FOnnxRuntimeModelRunner> Create(TConstArrayView<uint8> ModelBytes, int32 ThreadCount,
			bool bUseXnnpack);

		virtual ~FOnnxRuntimeModelRunner() override;

		// Functions
		virtual TConstArrayView<int64> GetInputShape() const override { return InputShape; }
		virtual TConstArrayView<int64> GetOutputShape() const override { return OutputShape; }
		virtual const FString& GetDescription() const override { return Description; }
		virtual bool Run(TConstArrayView<float> Input, TArrayView<float> Output) override;

	private:
		// Functions

		// Logs and releases a failed status
		bool Succeeded(OrtStatus* Status, const TCHAR* Action) const;
		bool ReadTensorInfo(bool bIsInput, TArray<ANSICHAR>& OutName, TArray<int64_t>& OutDimensions,
			TArray<int64>& OutShape) const;

		// State
		const OrtApi* Api = nullptr;
		OrtEnv* Environment = nullptr;
		OrtSession* Session = nullptr;
		OrtMemoryInfo* MemoryInfo = nullptr;
		// Null-terminated, as ONNX Runtime wants them
		TArray<ANSICHAR> InputName;
		TArray<ANSICHAR> OutputName;
		TArray<int64> InputShape;
		TArray<int64> OutputShape;
		// The same shapes as ONNX Runtime's int64_t, which is a different type than int64 on Android
		TArray<int64_t> InputDimensions;
		TArray<int64_t> OutputDimensions;
		FString Description;
	};
}
