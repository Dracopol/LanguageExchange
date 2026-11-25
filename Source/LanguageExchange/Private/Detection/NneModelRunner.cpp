#include "Detection/NneModelRunner.h"

#include "LanguageExchange.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "NNETypes.h"

namespace LanguageExchange::Detection
{
	namespace
	{
		FString DescribeShape(const UE::NNE::FSymbolicTensorShape& Shape)
		{
			TArray<FString> Dimensions;
			for (const int32 Dimension : Shape.GetData())
			{
				Dimensions.Add(FString::FromInt(Dimension));
			}
			return FString::Join(Dimensions, TEXT(" x "));
		}
	}

	TUniquePtr<FNneModelRunner> FNneModelRunner::Create(UNNEModelData* ModelData, const FString& RuntimeName)
	{
		check(IsValid(ModelData));

		const TWeakInterfacePtr<INNERuntimeCPU> Runtime = UE::NNE::GetRuntime<INNERuntimeCPU>(RuntimeName);
		if (!Runtime.IsValid())
		{
			const FString AvailableRuntimeNames =
				FString::Join(UE::NNE::GetAllRuntimeNames<INNERuntimeCPU>(), TEXT(", "));
			UE_LOG(LogLanguageExchange, Error, TEXT("NNE runtime '%s' not found. Available CPU runtimes: %s"),
				*RuntimeName, *AvailableRuntimeNames);
			return nullptr;
		}

		if (Runtime->CanCreateModelCPU(ModelData) != INNERuntimeCPU::ECanCreateModelCPUStatus::Ok)
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("NNE runtime '%s' can't create a model from %s"),
				*RuntimeName, *ModelData->GetPathName());
			return nullptr;
		}

		TUniquePtr<FNneModelRunner> Runner = MakeUnique<FNneModelRunner>();
		Runner->Model = Runtime->CreateModelCPU(ModelData);
		if (!Runner->Model.IsValid())
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Creating the model %s failed"), *ModelData->GetPathName());
			return nullptr;
		}

		Runner->ModelInstance = Runner->Model->CreateModelInstanceCPU();
		if (!Runner->ModelInstance.IsValid())
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Creating a model instance of %s failed"),
				*ModelData->GetPathName());
			return nullptr;
		}

		const TConstArrayView<UE::NNE::FTensorDesc> InputDescriptions = Runner->ModelInstance->GetInputTensorDescs();
		const TConstArrayView<UE::NNE::FTensorDesc> OutputDescriptions =
			Runner->ModelInstance->GetOutputTensorDescs();
		if (InputDescriptions.Num() != 1 || OutputDescriptions.Num() != 1
			|| InputDescriptions[0].GetDataType() != ENNETensorDataType::Float
			|| OutputDescriptions[0].GetDataType() != ENNETensorDataType::Float)
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Expected a model with 1 float input and 1 float output"));
			return nullptr;
		}

		const UE::NNE::FSymbolicTensorShape& SymbolicInputShape = InputDescriptions[0].GetShape();
		if (!SymbolicInputShape.IsConcrete())
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Expected a fixed input shape, got %s"),
				*DescribeShape(SymbolicInputShape));
			return nullptr;
		}

		const UE::NNE::FTensorShape InputShape = UE::NNE::FTensorShape::MakeFromSymbolic(SymbolicInputShape);
		if (Runner->ModelInstance->SetInputTensorShapes(MakeArrayView(&InputShape, 1)) != UE::NNE::EResultStatus::Ok)
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Setting the input shape %s failed"),
				*DescribeShape(SymbolicInputShape));
			return nullptr;
		}

		// The output shape is either resolved by SetInputTensorShapes or fixed in the model
		UE::NNE::FTensorShape OutputShape;
		const TConstArrayView<UE::NNE::FTensorShape> ResolvedOutputShapes =
			Runner->ModelInstance->GetOutputTensorShapes();
		if (ResolvedOutputShapes.Num() == 1)
		{
			OutputShape = ResolvedOutputShapes[0];
		}
		else if (OutputDescriptions[0].GetShape().IsConcrete())
		{
			OutputShape = UE::NNE::FTensorShape::MakeFromSymbolic(OutputDescriptions[0].GetShape());
		}
		else
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("The output shape %s isn't fixed"),
				*DescribeShape(OutputDescriptions[0].GetShape()));
			return nullptr;
		}

		for (const uint32 Dimension : InputShape.GetData())
		{
			Runner->InputShape.Add(Dimension);
		}
		for (const uint32 Dimension : OutputShape.GetData())
		{
			Runner->OutputShape.Add(Dimension);
		}
		Runner->Description = FString::Printf(TEXT("NNE %s"), *RuntimeName);
		return Runner;
	}

	bool FNneModelRunner::Run(const TConstArrayView<float> Input, const TArrayView<float> Output)
	{
		// NNE's binding takes a mutable pointer but only reads the input
		const UE::NNE::FTensorBindingCPU InputBinding{
			const_cast<float*>(Input.GetData()), Input.Num() * sizeof(float) };
		const UE::NNE::FTensorBindingCPU OutputBinding{ Output.GetData(), Output.Num() * sizeof(float) };
		return ModelInstance->RunSync(MakeArrayView(&InputBinding, 1), MakeArrayView(&OutputBinding, 1))
			== UE::NNE::EResultStatus::Ok;
	}
}
