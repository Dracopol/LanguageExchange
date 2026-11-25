#include "Detection/YoloModel.h"

#include "Detection/CocoClassNames.h"
#include "HAL/PlatformTime.h"
#include "LanguageExchange.h"

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

		FString DescribeShape(const TConstArrayView<int64> Shape)
		{
			TArray<FString> Dimensions;
			for (const int64 Dimension : Shape)
			{
				Dimensions.Add(LexToString(Dimension));
			}
			return FString::Join(Dimensions, TEXT(" x "));
		}
	}

	TSharedPtr<FYoloModel> FYoloModel::Create(TUniquePtr<IModelRunner> Runner)
	{
		check(Runner.IsValid());

		const TConstArrayView<int64> InputShape = Runner->GetInputShape();
		const bool bIsInputSupported = InputShape.Num() == 4
			&& InputShape[0] == 1
			&& InputShape[1] == 3
			&& InputShape[2] == InputShape[3];
		if (!bIsInputSupported)
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Expected an input of 1 x 3 x size x size, got %s"),
				*DescribeShape(InputShape));
			return nullptr;
		}

		const TConstArrayView<int64> OutputShape = Runner->GetOutputShape();
		const bool bIsOutputSupported = OutputShape.Num() == 3
			&& OutputShape[0] == 1
			&& OutputShape[1] > BoxValueCount;
		if (!bIsOutputSupported)
		{
			UE_LOG(LogLanguageExchange, Error, TEXT("Expected an output of 1 x (4 + classes) x anchors, got %s"),
				*DescribeShape(OutputShape));
			return nullptr;
		}

		const int32 InputSizePixels = static_cast<int32>(InputShape[2]);
		const int32 ClassCount = static_cast<int32>(OutputShape[1]) - BoxValueCount;
		const int32 AnchorCount = static_cast<int32>(OutputShape[2]);
		if (ClassCount != CocoClassCount)
		{
			UE_LOG(LogLanguageExchange, Warning,
				TEXT("The model has %d classes, not the %d COCO classes; names will show as 'class <index>'"),
				ClassCount, CocoClassCount);
		}

		UE_LOG(LogLanguageExchange, Log, TEXT("Detection model ready on %s: input %d x %d px, %d classes, %d anchors"),
			*Runner->GetDescription(), InputSizePixels, InputSizePixels, ClassCount, AnchorCount);

		return MakeShared<FYoloModel>(MoveTemp(Runner), InputSizePixels, ClassCount, AnchorCount);
	}

	FYoloModel::FYoloModel(TUniquePtr<IModelRunner> InRunner, const int32 InInputSizePixels, const int32 InClassCount,
		const int32 InAnchorCount)
		: Runner(MoveTemp(InRunner))
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

		const double StartSeconds = FPlatformTime::Seconds();
		const bool bHasRun = Runner->Run(InputTensor, OutputTensor);
		OutInferenceMilliseconds = static_cast<float>((FPlatformTime::Seconds() - StartSeconds) * 1000.0);

		if (!bHasRun)
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
