#pragma once

#include "CoreMinimal.h"
#include "Detection/ModelRunner.h"

class UNNEModelData;

namespace UE::NNE
{
	class IModelCPU;
	class IModelInstanceCPU;
}

namespace LanguageExchange::Detection
{
	// Runs a model through one of NNE's CPU runtimes (e.g. NNERuntimeORTCpu on PC)
	class FNneModelRunner : public IModelRunner
	{
	public:
		// Setup

		// Returns nullptr and logs the reason when the model can't be created
		static TUniquePtr<FNneModelRunner> Create(UNNEModelData* ModelData, const FString& RuntimeName);

		// Functions
		virtual TConstArrayView<int64> GetInputShape() const override { return InputShape; }
		virtual TConstArrayView<int64> GetOutputShape() const override { return OutputShape; }
		virtual const FString& GetDescription() const override { return Description; }
		virtual bool Run(TConstArrayView<float> Input, TArrayView<float> Output) override;

	private:
		// State
		TSharedPtr<UE::NNE::IModelCPU> Model;
		TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance;
		TArray<int64> InputShape;
		TArray<int64> OutputShape;
		FString Description;
	};
}
