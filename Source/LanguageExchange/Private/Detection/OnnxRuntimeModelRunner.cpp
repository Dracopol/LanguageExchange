#include "Detection/OnnxRuntimeModelRunner.h"

#if WITH_LANGUAGEEXCHANGE_ONNXRUNTIME

#include "LanguageExchange.h"
#include "Misc/ScopeExit.h"

THIRD_PARTY_INCLUDES_START
#include "onnxruntime_c_api.h"
THIRD_PARTY_INCLUDES_END

namespace LanguageExchange::Detection
{
	TUniquePtr<FOnnxRuntimeModelRunner> FOnnxRuntimeModelRunner::Create(const TConstArrayView<uint8> ModelBytes,
		const int32 ThreadCount, const bool bUseXnnpack)
	{
		TUniquePtr<FOnnxRuntimeModelRunner> Runner = MakeUnique<FOnnxRuntimeModelRunner>();
		Runner->Api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
		if (Runner->Api == nullptr)
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("ONNX Runtime %s doesn't support API version %d"),
				UTF8_TO_TCHAR(OrtGetApiBase()->GetVersionString()), ORT_API_VERSION);
			return nullptr;
		}
		const OrtApi& Ort = *Runner->Api;

		if (!Runner->Succeeded(Ort.CreateEnv(ORT_LOGGING_LEVEL_WARNING, "LanguageExchange", &Runner->Environment),
			TEXT("creating the environment")))
		{
			return nullptr;
		}

		OrtSessionOptions* Options = nullptr;
		if (!Runner->Succeeded(Ort.CreateSessionOptions(&Options), TEXT("creating session options")))
		{
			return nullptr;
		}
		ON_SCOPE_EXIT
		{
			Ort.ReleaseSessionOptions(Options);
		};

		// Idle worker threads sleep instead of spinning, so they don't take CPU time from the game
		bool bAreOptionsSet =
			Runner->Succeeded(Ort.SetSessionGraphOptimizationLevel(Options, ORT_ENABLE_ALL), TEXT("setting options"))
			&& Runner->Succeeded(Ort.AddSessionConfigEntry(Options, "session.intra_op.allow_spinning", "0"),
				TEXT("setting options"));
		if (bUseXnnpack)
		{
			const FTCHARToUTF8 ThreadCountText(*FString::FromInt(ThreadCount));
			const char* const ProviderKeys[] = { "intra_op_num_threads" };
			const char* const ProviderValues[] = { ThreadCountText.Get() };
			bAreOptionsSet = bAreOptionsSet
				&& Runner->Succeeded(Ort.SetIntraOpNumThreads(Options, 1), TEXT("setting options"))
				&& Runner->Succeeded(Ort.SessionOptionsAppendExecutionProvider(Options, "XNNPACK", ProviderKeys,
					ProviderValues, 1), TEXT("adding XNNPACK"));
		}
		else
		{
			bAreOptionsSet = bAreOptionsSet
				&& Runner->Succeeded(Ort.SetIntraOpNumThreads(Options, ThreadCount), TEXT("setting options"));
		}
		if (!bAreOptionsSet)
		{
			return nullptr;
		}

		if (!Runner->Succeeded(Ort.CreateSessionFromArray(Runner->Environment, ModelBytes.GetData(), ModelBytes.Num(),
			Options, &Runner->Session), TEXT("loading the model")))
		{
			return nullptr;
		}

		size_t InputCount = 0;
		size_t OutputCount = 0;
		if (!Runner->Succeeded(Ort.SessionGetInputCount(Runner->Session, &InputCount), TEXT("reading the model"))
			|| !Runner->Succeeded(Ort.SessionGetOutputCount(Runner->Session, &OutputCount), TEXT("reading the model")))
		{
			return nullptr;
		}
		if (InputCount != 1 || OutputCount != 1)
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Expected a model with 1 input and 1 output, got %d and %d"),
				static_cast<int32>(InputCount), static_cast<int32>(OutputCount));
			return nullptr;
		}

		if (!Runner->ReadTensorInfo(true, Runner->InputName, Runner->InputDimensions, Runner->InputShape)
			|| !Runner->ReadTensorInfo(false, Runner->OutputName, Runner->OutputDimensions, Runner->OutputShape))
		{
			return nullptr;
		}

		if (!Runner->Succeeded(Ort.CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &Runner->MemoryInfo),
			TEXT("creating memory info")))
		{
			return nullptr;
		}

		Runner->Description = FString::Printf(TEXT("ONNX Runtime %s (%s, %d threads)"),
			UTF8_TO_TCHAR(OrtGetApiBase()->GetVersionString()), bUseXnnpack ? TEXT("XNNPACK") : TEXT("CPU"),
			ThreadCount);
		return Runner;
	}

	FOnnxRuntimeModelRunner::~FOnnxRuntimeModelRunner()
	{
		if (Api == nullptr)
		{
			return;
		}
		// The release functions accept null
		Api->ReleaseMemoryInfo(MemoryInfo);
		Api->ReleaseSession(Session);
		Api->ReleaseEnv(Environment);
	}

	bool FOnnxRuntimeModelRunner::Run(const TConstArrayView<float> Input, const TArrayView<float> Output)
	{
		const OrtApi& Ort = *Api;
		OrtValue* InputValue = nullptr;
		OrtValue* OutputValue = nullptr;
		ON_SCOPE_EXIT
		{
			Ort.ReleaseValue(InputValue);
			Ort.ReleaseValue(OutputValue);
		};

		// Both tensors wrap our buffers, so ONNX Runtime writes the result straight into Output.
		// It only reads the input; the API just takes a mutable pointer.
		const bool bAreTensorsCreated =
			Succeeded(Ort.CreateTensorWithDataAsOrtValue(MemoryInfo, const_cast<float*>(Input.GetData()),
				Input.Num() * sizeof(float), InputDimensions.GetData(), InputDimensions.Num(),
				ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &InputValue), TEXT("wrapping the input"))
			&& Succeeded(Ort.CreateTensorWithDataAsOrtValue(MemoryInfo, Output.GetData(),
				Output.Num() * sizeof(float), OutputDimensions.GetData(), OutputDimensions.Num(),
				ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &OutputValue), TEXT("wrapping the output"));
		if (!bAreTensorsCreated)
		{
			return false;
		}

		const char* const InputNames[] = { InputName.GetData() };
		const char* const OutputNames[] = { OutputName.GetData() };
		const OrtValue* const Inputs[] = { InputValue };
		return Succeeded(Ort.Run(Session, nullptr, InputNames, Inputs, 1, OutputNames, 1, &OutputValue),
			TEXT("running the model"));
	}

	bool FOnnxRuntimeModelRunner::Succeeded(OrtStatus* const Status, const TCHAR* const Action) const
	{
		if (Status == nullptr)
		{
			return true;
		}
		UE_LOG(LogLanguageExchange, Error, TEXT("ONNX Runtime failed %s: %s"), Action,
			UTF8_TO_TCHAR(Api->GetErrorMessage(Status)));
		Api->ReleaseStatus(Status);
		return false;
	}

	bool FOnnxRuntimeModelRunner::ReadTensorInfo(const bool bIsInput, TArray<ANSICHAR>& OutName,
		TArray<int64_t>& OutDimensions, TArray<int64>& OutShape) const
	{
		const OrtApi& Ort = *Api;
		OrtAllocator* Allocator = nullptr;
		if (!Succeeded(Ort.GetAllocatorWithDefaultOptions(&Allocator), TEXT("getting the allocator")))
		{
			return false;
		}

		char* Name = nullptr;
		OrtStatus* const NameStatus = bIsInput
			? Ort.SessionGetInputName(Session, 0, Allocator, &Name)
			: Ort.SessionGetOutputName(Session, 0, Allocator, &Name);
		if (!Succeeded(NameStatus, TEXT("reading a tensor name")))
		{
			return false;
		}
		OutName.Reset();
		OutName.Append(Name, FCStringAnsi::Strlen(Name) + 1);
		Succeeded(Ort.AllocatorFree(Allocator, Name), TEXT("freeing a tensor name"));

		OrtTypeInfo* TypeInfo = nullptr;
		ON_SCOPE_EXIT
		{
			Ort.ReleaseTypeInfo(TypeInfo);
		};
		OrtStatus* const TypeStatus = bIsInput
			? Ort.SessionGetInputTypeInfo(Session, 0, &TypeInfo)
			: Ort.SessionGetOutputTypeInfo(Session, 0, &TypeInfo);
		const OrtTensorTypeAndShapeInfo* TensorInfo = nullptr;
		ONNXTensorElementDataType ElementType = ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
		size_t DimensionCount = 0;
		if (!Succeeded(TypeStatus, TEXT("reading a tensor type"))
			|| !Succeeded(Ort.CastTypeInfoToTensorInfo(TypeInfo, &TensorInfo), TEXT("reading a tensor type"))
			|| TensorInfo == nullptr
			|| !Succeeded(Ort.GetTensorElementType(TensorInfo, &ElementType), TEXT("reading a tensor type"))
			|| !Succeeded(Ort.GetDimensionsCount(TensorInfo, &DimensionCount), TEXT("reading a tensor shape")))
		{
			return false;
		}
		if (ElementType != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Expected float tensors, %s '%hs' isn't"),
				bIsInput ? TEXT("input") : TEXT("output"), OutName.GetData());
			return false;
		}

		OutDimensions.SetNumZeroed(DimensionCount);
		if (!Succeeded(Ort.GetDimensions(TensorInfo, OutDimensions.GetData(), DimensionCount),
			TEXT("reading a tensor shape")))
		{
			return false;
		}
		OutShape.Reset();
		for (const int64_t Dimension : OutDimensions)
		{
			if (Dimension <= 0)
			{
				UE_LOG(LogLanguageExchange, Error, TEXT("Expected fixed tensor shapes, %s '%hs' has a variable one"),
					bIsInput ? TEXT("input") : TEXT("output"), OutName.GetData());
				return false;
			}
			OutShape.Add(Dimension);
		}
		return true;
	}
}

#endif
