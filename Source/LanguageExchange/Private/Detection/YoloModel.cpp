#include "Detection/YoloModel.h"

#include "Detection/CocoClassNames.h"
#include "HAL/PlatformTime.h"
#include "LanguageExchange.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "NNETypes.h"

namespace LanguageExchange::Detection
{
	namespace
	{
		// Each anchor starts with centre x, centre y, width, height; the class scores follow
		constexpr int32 BoxValueCount = 4;

		float GetIntersectionOverUnion(const FBox2D& First, const FBox2D& Second)
		{
			const FBox2D Intersection = First.Overlap(Second);
			if (!Intersection.bIsValid)
			{
				return 0.0f;
			}
			const double IntersectionArea = Intersection.GetArea();
			const double UnionArea = First.GetArea() + Second.GetArea() - IntersectionArea;
			if (UnionArea <= 0.0)
			{
				return 0.0f;
			}
			return static_cast<float>(IntersectionArea / UnionArea);
		}

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

	TSharedPtr<FYoloModel> FYoloModel::Create(UNNEModelData* ModelData, const FString& RuntimeName)
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

		const TSharedPtr<UE::NNE::IModelCPU> Model = Runtime->CreateModelCPU(ModelData);
		if (!Model.IsValid())
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Creating the model %s failed"), *ModelData->GetPathName());
			return nullptr;
		}

		const TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance = Model->CreateModelInstanceCPU();
		if (!ModelInstance.IsValid())
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Creating a model instance of %s failed"),
				*ModelData->GetPathName());
			return nullptr;
		}

		const TConstArrayView<UE::NNE::FTensorDesc> InputDescriptions = ModelInstance->GetInputTensorDescs();
		const TConstArrayView<UE::NNE::FTensorDesc> OutputDescriptions = ModelInstance->GetOutputTensorDescs();
		if (InputDescriptions.Num() != 1 || OutputDescriptions.Num() != 1)
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Expected a model with 1 input and 1 output, got %d and %d"),
				InputDescriptions.Num(), OutputDescriptions.Num());
			return nullptr;
		}

		const UE::NNE::FSymbolicTensorShape& SymbolicInputShape = InputDescriptions[0].GetShape();
		const TConstArrayView<int32> InputDimensions = SymbolicInputShape.GetData();
		const bool bIsInputSupported = SymbolicInputShape.IsConcrete()
			&& InputDimensions.Num() == 4
			&& InputDimensions[0] == 1
			&& InputDimensions[1] == 3
			&& InputDimensions[2] == InputDimensions[3]
			&& InputDescriptions[0].GetDataType() == ENNETensorDataType::Float;
		if (!bIsInputSupported)
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Expected a float input of 1 x 3 x size x size, got %s"),
				*DescribeShape(SymbolicInputShape));
			return nullptr;
		}

		const UE::NNE::FTensorShape InputShape = UE::NNE::FTensorShape::MakeFromSymbolic(SymbolicInputShape);
		if (ModelInstance->SetInputTensorShapes(MakeArrayView(&InputShape, 1)) != UE::NNE::EResultStatus::Ok)
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Setting the input shape %s failed"),
				*DescribeShape(SymbolicInputShape));
			return nullptr;
		}

		// The output shape is either resolved by SetInputTensorShapes or fixed in the model
		UE::NNE::FTensorShape OutputShape;
		const TConstArrayView<UE::NNE::FTensorShape> ResolvedOutputShapes = ModelInstance->GetOutputTensorShapes();
		if (ResolvedOutputShapes.Num() == 1)
		{
			OutputShape = ResolvedOutputShapes[0];
		}
		else if (OutputDescriptions[0].GetShape().IsConcrete())
		{
			OutputShape = UE::NNE::FTensorShape::MakeFromSymbolic(OutputDescriptions[0].GetShape());
		}

		const TConstArrayView<uint32> OutputDimensions = OutputShape.GetData();
		const bool bIsOutputSupported = OutputDimensions.Num() == 3
			&& OutputDimensions[0] == 1
			&& OutputDimensions[1] > BoxValueCount
			&& OutputDescriptions[0].GetDataType() == ENNETensorDataType::Float;
		if (!bIsOutputSupported)
		{
			UE_LOG(LogLanguageExchange, Error,
				TEXT("Expected a float output of 1 x (4 + classes) x anchors, got %s"),
				*DescribeShape(OutputDescriptions[0].GetShape()));
			return nullptr;
		}

		const int32 InputSizePixels = InputDimensions[2];
		const int32 ClassCount = static_cast<int32>(OutputDimensions[1]) - BoxValueCount;
		const int32 AnchorCount = static_cast<int32>(OutputDimensions[2]);
		if (ClassCount != CocoClassCount)
		{
			UE_LOG(LogLanguageExchange, Warning,
				TEXT("The model has %d classes, not the %d COCO classes; names will show as 'class <index>'"),
				ClassCount, CocoClassCount);
		}

		UE_LOG(LogLanguageExchange, Log,
			TEXT("Detection model %s ready on %s: input %d x %d px, %d classes, %d anchors"),
			*ModelData->GetName(), *RuntimeName, InputSizePixels, InputSizePixels, ClassCount, AnchorCount);

		return MakeShared<FYoloModel>(Model, ModelInstance, InputSizePixels, ClassCount, AnchorCount);
	}

	FYoloModel::FYoloModel(const TSharedPtr<UE::NNE::IModelCPU>& InModel,
		const TSharedPtr<UE::NNE::IModelInstanceCPU>& InModelInstance, const int32 InInputSizePixels,
		const int32 InClassCount, const int32 InAnchorCount)
		: Model(InModel)
		, ModelInstance(InModelInstance)
		, InputSizePixels(InInputSizePixels)
		, ClassCount(InClassCount)
		, AnchorCount(InAnchorCount)
	{
		IgnoredClasses.Init(false, ClassCount);
		InputTensor.SetNumZeroed(3 * InputSizePixels * InputSizePixels);
		OutputTensor.SetNumZeroed((BoxValueCount + ClassCount) * AnchorCount);
	}

	void FYoloModel::SetIgnoredClassIndices(const TConstArrayView<int32> ClassIndices)
	{
		IgnoredClasses.Init(false, ClassCount);
		for (const int32 ClassIndex : ClassIndices)
		{
			if (ClassIndex >= 0 && ClassIndex < ClassCount)
			{
				IgnoredClasses[ClassIndex] = true;
			}
		}
	}

	TArray<FLanguageExchangeDetection> FYoloModel::Detect(const TConstArrayView<FColor> Pixels,
		const float MinimumConfidence, const float MaximumBoxOverlap, float& OutInferenceMilliseconds)
	{
		FillInputTensor(Pixels);

		const UE::NNE::FTensorBindingCPU InputBinding{ InputTensor.GetData(), InputTensor.Num() * sizeof(float) };
		const UE::NNE::FTensorBindingCPU OutputBinding{ OutputTensor.GetData(), OutputTensor.Num() * sizeof(float) };

		const double StartSeconds = FPlatformTime::Seconds();
		const UE::NNE::EResultStatus Status =
			ModelInstance->RunSync(MakeArrayView(&InputBinding, 1), MakeArrayView(&OutputBinding, 1));
		OutInferenceMilliseconds = static_cast<float>((FPlatformTime::Seconds() - StartSeconds) * 1000.0);

		if (Status != UE::NNE::EResultStatus::Ok)
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Running the detection model failed"));
			return {};
		}

		TArray<FLanguageExchangeDetection> Candidates = DecodeOutputTensor(MinimumConfidence);
		Candidates.Sort([](const FLanguageExchangeDetection& First, const FLanguageExchangeDetection& Second)
		{
			return First.Confidence > Second.Confidence;
		});

		// Non-maximum suppression: of overlapping boxes of the same class, keep only the most confident one
		TArray<FLanguageExchangeDetection> Detections;
		for (const FLanguageExchangeDetection& Candidate : Candidates)
		{
			bool bOverlapsKeptDetection = false;
			for (const FLanguageExchangeDetection& Detection : Detections)
			{
				if (Detection.ClassIndex == Candidate.ClassIndex
					&& GetIntersectionOverUnion(Detection.NormalizedBounds, Candidate.NormalizedBounds)
						> MaximumBoxOverlap)
				{
					bOverlapsKeptDetection = true;
					break;
				}
			}
			if (!bOverlapsKeptDetection)
			{
				Detections.Add(Candidate);
			}
		}
		return Detections;
	}

	void FYoloModel::FillInputTensor(const TConstArrayView<FColor> Pixels)
	{
		const int32 PlaneSize = InputSizePixels * InputSizePixels;
		check(Pixels.Num() == PlaneSize);

		// Interleaved 8-bit RGBA to planar float RGB in 0..1
		constexpr float ByteToUnit = 1.0f / 255.0f;
		for (int32 PixelIndex = 0; PixelIndex < PlaneSize; ++PixelIndex)
		{
			const FColor& Pixel = Pixels[PixelIndex];
			InputTensor[PixelIndex] = Pixel.R * ByteToUnit;
			InputTensor[PlaneSize + PixelIndex] = Pixel.G * ByteToUnit;
			InputTensor[2 * PlaneSize + PixelIndex] = Pixel.B * ByteToUnit;
		}
	}

	TArray<FLanguageExchangeDetection> FYoloModel::DecodeOutputTensor(const float MinimumConfidence) const
	{
		// Find each anchor's best class, reading the output row by row (one row per class) to stay cache friendly
		TArray<float> BestScores;
		BestScores.Init(0.0f, AnchorCount);
		TArray<int32> BestClassIndices;
		BestClassIndices.Init(INDEX_NONE, AnchorCount);
		for (int32 ClassIndex = 0; ClassIndex < ClassCount; ++ClassIndex)
		{
			const float* Scores = &OutputTensor[(BoxValueCount + ClassIndex) * AnchorCount];
			for (int32 AnchorIndex = 0; AnchorIndex < AnchorCount; ++AnchorIndex)
			{
				if (Scores[AnchorIndex] > BestScores[AnchorIndex])
				{
					BestScores[AnchorIndex] = Scores[AnchorIndex];
					BestClassIndices[AnchorIndex] = ClassIndex;
				}
			}
		}

		const double InputSize = InputSizePixels;
		TArray<FLanguageExchangeDetection> Candidates;
		for (int32 AnchorIndex = 0; AnchorIndex < AnchorCount; ++AnchorIndex)
		{
			// Drop ignored classes only after picking the best class over all of them (as Ultralytics does);
			// otherwise a clear "person" box would turn into its runner-up, e.g. "dog"
			if (BestScores[AnchorIndex] < MinimumConfidence || IgnoredClasses[BestClassIndices[AnchorIndex]])
			{
				continue;
			}

			const double CentreX = OutputTensor[AnchorIndex];
			const double CentreY = OutputTensor[AnchorCount + AnchorIndex];
			const double HalfWidth = OutputTensor[2 * AnchorCount + AnchorIndex] * 0.5;
			const double HalfHeight = OutputTensor[3 * AnchorCount + AnchorIndex] * 0.5;

			FLanguageExchangeDetection& Detection = Candidates.AddDefaulted_GetRef();
			Detection.ClassIndex = BestClassIndices[AnchorIndex];
			Detection.ClassName = GetCocoClassName(Detection.ClassIndex);
			Detection.Confidence = BestScores[AnchorIndex];
			Detection.NormalizedBounds = FBox2D(
				FVector2D(CentreX - HalfWidth, CentreY - HalfHeight) / InputSize,
				FVector2D(CentreX + HalfWidth, CentreY + HalfHeight) / InputSize);
		}
		return Candidates;
	}
}
