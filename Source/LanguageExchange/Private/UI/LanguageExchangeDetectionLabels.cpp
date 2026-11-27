#include "UI/LanguageExchangeDetectionLabels.h"

#include "Components/WidgetComponent.h"
#include "Detection/LanguageExchangeObjectDetectionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "LanguageExchange.h"
#include "MRUtilityKitSubsystem.h"
#include "UI/LanguageExchangeFrameWidget.h"
#include "UI/LanguageExchangeLabelWidget.h"

namespace LanguageExchange::UserInterface
{
	namespace
	{
		// For checking the rendering independently of detection (set at startup, e.g. with -DPCVars=...=1)
		TAutoConsoleVariable<bool> ShowTestLabelVariable(
			TEXT("LanguageExchange.Labels.ShowTestLabel"),
			false,
			TEXT("Show one fixed test label 1 m in front of the start position instead of the detections"));

		// For comparing measured and estimated distances while playing
		TAutoConsoleVariable<bool> UseDepthVariable(
			TEXT("LanguageExchange.Labels.UseDepth"),
			true,
			TEXT("Place objects at the distance measured by the headset's depth sensing (else: estimated from size)"));

		// Corner order of FLanguageExchangeDetection::WorldCornerDirections
		constexpr int32 TopLeft = 0;
		constexpr int32 TopRight = 1;
		constexpr int32 BottomRight = 2;
		constexpr int32 BottomLeft = 3;

		double GetAngleRadians(const FVector& DirectionA, const FVector& DirectionB)
		{
			return FMath::Acos(FMath::Clamp(FVector::DotProduct(DirectionA, DirectionB), -1.0, 1.0));
		}

		// The larger of the box's width and height, as an angle seen from the camera
		double GetAngularSizeRadians(const TConstArrayView<FVector> CornerDirections)
		{
			return FMath::Max(GetAngleRadians(CornerDirections[TopLeft], CornerDirections[TopRight]),
				GetAngleRadians(CornerDirections[TopLeft], CornerDirections[BottomLeft]));
		}
	}
}

ALanguageExchangeDetectionLabels::ALanguageExchangeDetectionLabels()
{
	// Ticks only while objects are shown, to glide their frames and hide the ones no longer detected
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootSceneComponent);

	// Rough largest dimensions of common COCO objects
	TypicalSizeCentimeters = {
		{ TEXT("cup"), 10.0f }, { TEXT("wine glass"), 18.0f }, { TEXT("bottle"), 25.0f }, { TEXT("bowl"), 16.0f },
		{ TEXT("mouse"), 11.0f }, { TEXT("keyboard"), 44.0f }, { TEXT("laptop"), 33.0f }, { TEXT("tv"), 60.0f },
		{ TEXT("remote"), 18.0f }, { TEXT("cell phone"), 15.0f }, { TEXT("book"), 23.0f }, { TEXT("scissors"), 18.0f },
		{ TEXT("spoon"), 17.0f }, { TEXT("fork"), 19.0f }, { TEXT("knife"), 22.0f }, { TEXT("toothbrush"), 18.0f },
		{ TEXT("clock"), 30.0f }, { TEXT("vase"), 25.0f }, { TEXT("apple"), 8.0f }, { TEXT("orange"), 8.0f },
		{ TEXT("banana"), 18.0f }, { TEXT("teddy bear"), 30.0f }, { TEXT("backpack"), 45.0f },
		{ TEXT("potted plant"), 40.0f }, { TEXT("chair"), 90.0f },
	};
}

void ALanguageExchangeDetectionLabels::BeginPlay()
{
	Super::BeginPlay();

	if (LanguageExchange::UserInterface::ShowTestLabelVariable.GetValueOnGameThread())
	{
		ShowTestLabel();
		return;
	}

	ULanguageExchangeObjectDetectionSubsystem* const DetectionSubsystem =
		GetWorld()->GetSubsystem<ULanguageExchangeObjectDetectionSubsystem>();
	if (ensureMsgf(IsValid(DetectionSubsystem), TEXT("Detection labels need the detection subsystem")))
	{
		DetectionsChangeHandle =
			DetectionSubsystem->OnDetectionsChange.AddUObject(this, &ThisClass::HandleDetectionsChange);
	}
}

void ALanguageExchangeDetectionLabels::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULanguageExchangeObjectDetectionSubsystem* const DetectionSubsystem =
		GetWorld()->GetSubsystem<ULanguageExchangeObjectDetectionSubsystem>())
	{
		DetectionSubsystem->OnDetectionsChange.Remove(DetectionsChangeHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ALanguageExchangeDetectionLabels::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The same share of the remaining way per unit of time, whatever the frame rate
	const double NowSeconds = GetWorld()->GetTimeSeconds();
	const double Alpha = 1.0 - FMath::Exp(-DeltaSeconds / GlideSeconds);
	for (int32 Index = TrackedLabels.Num() - 1; Index >= 0; --Index)
	{
		FLanguageExchangeTrackedLabel& TrackedLabel = TrackedLabels[Index];
		if (NowSeconds - TrackedLabel.LastSeenSeconds > HoldSeconds)
		{
			ReleaseTrackedLabel(TrackedLabel);
			TrackedLabels.RemoveAtSwap(Index);
			continue;
		}
		TrackedLabel.ShownPlacement = TrackedLabel.ShownPlacement.MoveToward(TrackedLabel.TargetPlacement, Alpha);
		ShowTrackedLabel(TrackedLabel);
	}

	if (TrackedLabels.IsEmpty())
	{
		SetActorTickEnabled(false);
	}
}

bool ALanguageExchangeDetectionLabels::TryFindPointedLabel(const FVector& RayOrigin, const FVector& RayDirection,
	int32& OutLabelId, double& OutDistanceCentimeters) const
{
	const FLanguageExchangeTrackedLabel* PointedLabel = nullptr;
	double PointedAreaSquareCentimeters = 0.0;
	for (const FLanguageExchangeTrackedLabel& TrackedLabel : TrackedLabels)
	{
		// Where the ray meets the plane of the frame as it is shown (the frame faces along its X axis)
		const LanguageExchange::UserInterface::FLabelPlacement& Placement = TrackedLabel.ShownPlacement;
		const FVector FrameNormal = Placement.Rotation.GetForwardVector();
		const double Approach = FVector::DotProduct(RayDirection, FrameNormal);
		if (FMath::Abs(Approach) < UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const double Distance = FVector::DotProduct(Placement.Centre - RayOrigin, FrameNormal) / Approach;
		if (Distance <= 0.0)
		{
			continue;
		}

		const FVector Offset = RayOrigin + RayDirection * Distance - Placement.Centre;
		const double Margin = PointingMarginAtOneMeterCentimeters * Distance / 100.0;
		const FVector2D MaximumOffset = Placement.SizeCentimeters * 0.5 + FVector2D(Margin, Margin);
		if (FMath::Abs(FVector::DotProduct(Offset, Placement.Rotation.GetRightVector())) > MaximumOffset.X
			|| FMath::Abs(FVector::DotProduct(Offset, Placement.Rotation.GetUpVector())) > MaximumOffset.Y)
		{
			continue;
		}

		const double AreaSquareCentimeters = Placement.SizeCentimeters.X * Placement.SizeCentimeters.Y;
		if (PointedLabel == nullptr || AreaSquareCentimeters < PointedAreaSquareCentimeters)
		{
			PointedLabel = &TrackedLabel;
			PointedAreaSquareCentimeters = AreaSquareCentimeters;
			OutDistanceCentimeters = Distance;
		}
	}

	if (PointedLabel == nullptr)
	{
		return false;
	}
	OutLabelId = PointedLabel->Id;
	return true;
}

void ALanguageExchangeDetectionLabels::SetPointedLabels(const TConstArrayView<int32> LabelIds)
{
	for (FLanguageExchangeTrackedLabel& TrackedLabel : TrackedLabels)
	{
		TrackedLabel.bIsPointed = LabelIds.Contains(TrackedLabel.Id);
	}
}

void ALanguageExchangeDetectionLabels::HandleDetectionsChange(
	const TConstArrayView<FLanguageExchangeDetection> Detections)
{
	const double NowSeconds = GetWorld()->GetTimeSeconds();

	// Needs the four corner rays (top left, top right, bottom right, bottom left)
	TArray<const FLanguageExchangeDetection*, TInlineAllocator<16>> UsableDetections;
	for (const FLanguageExchangeDetection& Detection : Detections)
	{
		if (Detection.bHasWorldRay && Detection.WorldCornerDirections.Num() == 4)
		{
			UsableDetections.Add(&Detection);
		}
	}

	// Measured while the image's camera pose still fits (the objects stay put while the head moves on); logged next to
	// the size estimate for comparison
	TArray<TOptional<float>, TInlineAllocator<16>> MeasuredDistances;
	FString DistancesText;
	for (const FLanguageExchangeDetection* const Detection : UsableDetections)
	{
		const TOptional<float>& MeasuredDistance = MeasuredDistances.Add_GetRef(MeasureDistanceCentimeters(*Detection));
		const FString MeasuredText = MeasuredDistance.IsSet()
			? FString::Printf(TEXT("%.0f cm"), MeasuredDistance.GetValue())
			: FString(TEXT("none"));
		const float EstimatedDistance =
			EstimateDistanceCentimeters(Detection->ClassName, Detection->WorldCornerDirections);
		DistancesText += FString::Printf(TEXT("%s%s depth %s, size %.0f cm"),
			DistancesText.IsEmpty() ? TEXT("") : TEXT("; "), *Detection->ClassName, *MeasuredText, EstimatedDistance);
	}
	if (!DistancesText.IsEmpty())
	{
		UE_LOG(LogLanguageExchange, Log, TEXT("Distances: %s"), *DistancesText);
	}

	// Pair detections with shown objects, closest directions first; each takes part in at most one pair
	TArray<TTuple<double, int32, int32>> Candidates;
	for (int32 DetectionIndex = 0; DetectionIndex < UsableDetections.Num(); ++DetectionIndex)
	{
		const FLanguageExchangeDetection& Detection = *UsableDetections[DetectionIndex];
		const double MaximumAngle = GetMaximumMatchAngleRadians(Detection);
		for (int32 TrackedIndex = 0; TrackedIndex < TrackedLabels.Num(); ++TrackedIndex)
		{
			const double Angle = GetMatchAngleRadians(TrackedLabels[TrackedIndex], Detection);
			if (Angle <= MaximumAngle)
			{
				Candidates.Add(MakeTuple(Angle, DetectionIndex, TrackedIndex));
			}
		}
	}
	Candidates.Sort([](const TTuple<double, int32, int32>& A, const TTuple<double, int32, int32>& B)
		{ return A.Get<0>() < B.Get<0>(); });

	TArray<bool> IsDetectionMatched;
	IsDetectionMatched.Init(false, UsableDetections.Num());
	TArray<bool> IsTrackedLabelMatched;
	IsTrackedLabelMatched.Init(false, TrackedLabels.Num());
	for (const TTuple<double, int32, int32>& Candidate : Candidates)
	{
		const int32 DetectionIndex = Candidate.Get<1>();
		const int32 TrackedIndex = Candidate.Get<2>();
		if (!IsDetectionMatched[DetectionIndex] && !IsTrackedLabelMatched[TrackedIndex])
		{
			UpdateTrackedLabel(TrackedLabels[TrackedIndex], *UsableDetections[DetectionIndex],
				MeasuredDistances[DetectionIndex], NowSeconds);
			IsDetectionMatched[DetectionIndex] = true;
			IsTrackedLabelMatched[TrackedIndex] = true;
		}
	}

	// New objects appear right where they were detected; unmatched shown ones stay until they time out in Tick
	for (int32 DetectionIndex = 0; DetectionIndex < UsableDetections.Num(); ++DetectionIndex)
	{
		if (!IsDetectionMatched[DetectionIndex])
		{
			FLanguageExchangeTrackedLabel& TrackedLabel = TrackedLabels.Add_GetRef(CreateTrackedLabel());
			UpdateTrackedLabel(TrackedLabel, *UsableDetections[DetectionIndex], MeasuredDistances[DetectionIndex],
				NowSeconds);
			TrackedLabel.ShownPlacement = TrackedLabel.TargetPlacement;
			ShowTrackedLabel(TrackedLabel);
		}
	}

	if (!TrackedLabels.IsEmpty())
	{
		SetActorTickEnabled(true);
	}
}

double ALanguageExchangeDetectionLabels::GetMatchAngleRadians(const FLanguageExchangeTrackedLabel& TrackedLabel,
	const FLanguageExchangeDetection& Detection) const
{
	using namespace LanguageExchange::UserInterface;

	const FVector TowardTrackedLabel = (TrackedLabel.TargetPlacement.Centre - Detection.WorldRayOrigin).GetSafeNormal();
	return GetAngleRadians(TowardTrackedLabel, Detection.WorldRayDirection);
}

double ALanguageExchangeDetectionLabels::GetMaximumMatchAngleRadians(const FLanguageExchangeDetection& Detection) const
{
	using namespace LanguageExchange::UserInterface;

	return FMath::Max(FMath::DegreesToRadians(static_cast<double>(MinimumMatchAngleDegrees)),
		GetAngularSizeRadians(Detection.WorldCornerDirections) * 0.5);
}

void ALanguageExchangeDetectionLabels::UpdateTrackedLabel(FLanguageExchangeTrackedLabel& TrackedLabel,
	const FLanguageExchangeDetection& Detection, const TOptional<float>& MeasuredDistanceCentimeters,
	const double NowSeconds) const
{
	// Older detections fade, so the name follows what the object was detected as over the last moment
	const float Fade = FMath::Exp(-static_cast<float>(NowSeconds - TrackedLabel.LastSeenSeconds) / ClassMemorySeconds);
	for (TPair<FString, float>& ClassScore : TrackedLabel.ClassScores)
	{
		ClassScore.Value *= Fade;
	}
	TrackedLabel.ClassScores.FindOrAdd(Detection.ClassName) += Detection.Confidence;
	TrackedLabel.LastSeenSeconds = NowSeconds;

	const TPair<FString, float>* StrongestClassScore = nullptr;
	for (const TPair<FString, float>& ClassScore : TrackedLabel.ClassScores)
	{
		if (StrongestClassScore == nullptr || ClassScore.Value > StrongestClassScore->Value)
		{
			StrongestClassScore = &ClassScore;
		}
	}
	TrackedLabel.ShownClassName = StrongestClassScore->Key;
	if (Detection.ClassName == TrackedLabel.ShownClassName)
	{
		TrackedLabel.ShownConfidence = Detection.Confidence;
	}

	// Without a measurement, the shown class sets the distance estimate, so a brief misdetection doesn't throw the
	// frame off
	const float DistanceCentimeters = MeasuredDistanceCentimeters.Get(
		EstimateDistanceCentimeters(TrackedLabel.ShownClassName, Detection.WorldCornerDirections));
	TrackedLabel.TargetPlacement = ComputePlacement(Detection, DistanceCentimeters);

	// For checking the distance: measured as is, estimated with a tilde
	const TCHAR* const EstimateMark = MeasuredDistanceCentimeters.IsSet() ? TEXT("") : TEXT("~");
	TrackedLabel.LabelText = FText::FromString(FString::Printf(TEXT("%s %.2f  %s%.0f cm"),
		*TrackedLabel.ShownClassName, TrackedLabel.ShownConfidence, EstimateMark, DistanceCentimeters));
}

void ALanguageExchangeDetectionLabels::ShowTrackedLabel(const FLanguageExchangeTrackedLabel& TrackedLabel) const
{
	const LanguageExchange::UserInterface::FLabelPlacement& Placement = TrackedLabel.ShownPlacement;
	const FLinearColor& Color = TrackedLabel.bIsPointed ? PointedColor : HighlightColor;

	// Widget units are pixels: stretch the frame to the object's size, and give the line the same thickness in the
	// room on all four edges
	const FVector2D FrameScale = Placement.SizeCentimeters / FVector2D(FrameSizePixels);
	TrackedLabel.Frame->SetWorldLocationAndRotation(Placement.Centre, Placement.Rotation);
	TrackedLabel.Frame->SetWorldScale3D(FVector(1.0, FrameScale.X, FrameScale.Y));
	if (ULanguageExchangeFrameWidget* const FrameWidget =
		Cast<ULanguageExchangeFrameWidget>(TrackedLabel.Frame->GetUserWidgetObject()))
	{
		const double LineThickness = FrameLineThicknessAtOneMeterCentimeters * Placement.DistanceCentimeters / 100.0;
		FrameWidget->SetFrame(Color, FVector2D(LineThickness / FrameScale.X, LineThickness / FrameScale.Y));
	}

	// The label sits on the frame's top edge (its pivot is its bottom centre) and keeps its apparent size
	const FVector FrameTopCentre =
		Placement.Centre + Placement.Rotation.GetUpVector() * (Placement.SizeCentimeters.Y * 0.5);
	TrackedLabel.Label->SetWorldLocationAndRotation(FrameTopCentre, Placement.Rotation);
	const double LabelWidth = LabelWidthAtOneMeterCentimeters * Placement.DistanceCentimeters / 100.0;
	TrackedLabel.Label->SetWorldScale3D(FVector(LabelWidth / LabelSizePixels.X));
	if (ULanguageExchangeLabelWidget* const LabelWidget =
		Cast<ULanguageExchangeLabelWidget>(TrackedLabel.Label->GetUserWidgetObject()))
	{
		LabelWidget->SetLabel(TrackedLabel.LabelText, Color);
	}
}

LanguageExchange::UserInterface::FLabelPlacement ALanguageExchangeDetectionLabels::ComputePlacement(
	const FLanguageExchangeDetection& Detection, const float DistanceCentimeters) const
{
	using namespace LanguageExchange::UserInterface;

	// The frame lies in a plane facing the camera at the object's distance; each corner is where its ray meets that
	// plane
	const FVector& Origin = Detection.WorldRayOrigin;
	const FVector& CentreDirection = Detection.WorldRayDirection;
	const FVector PlaneCentre = Origin + CentreDirection * DistanceCentimeters;
	TArray<FVector, TInlineAllocator<4>> Corners;
	for (const FVector& CornerDirection : Detection.WorldCornerDirections)
	{
		const double AlongCentre = FMath::Max(FVector::DotProduct(CornerDirection, CentreDirection), 0.1);
		Corners.Add(Origin + CornerDirection * (DistanceCentimeters / AlongCentre));
	}

	// Frame and text stay upright in the room even when the head was tilted: up is the world's up within the plane,
	// or the image's up when looking almost straight up or down
	FVector UpInPlane = FVector::VectorPlaneProject(FVector::UpVector, CentreDirection);
	if (UpInPlane.SizeSquared() < 0.01)
	{
		UpInPlane = (Corners[TopLeft] + Corners[TopRight]) - (Corners[BottomLeft] + Corners[BottomRight]);
	}

	// Widgets face along their X axis
	const FMatrix Facing = FRotationMatrix::MakeFromXZ(-CentreDirection, UpInPlane);
	const FVector FrameRight = Facing.GetUnitAxis(EAxis::Y);
	const FVector FrameUp = Facing.GetUnitAxis(EAxis::Z);

	// The upright rectangle around the corners (larger than the camera's box when the head was tilted)
	FBox2D Extent(ForceInit);
	for (const FVector& Corner : Corners)
	{
		const FVector Offset = Corner - PlaneCentre;
		Extent += FVector2D(FVector::DotProduct(Offset, FrameRight), FVector::DotProduct(Offset, FrameUp));
	}

	FLabelPlacement Placement;
	const FVector2D ExtentCentre = Extent.GetCenter();
	Placement.Centre = PlaneCentre + FrameRight * ExtentCentre.X + FrameUp * ExtentCentre.Y;
	Placement.Rotation = Facing.ToQuat();
	Placement.SizeCentimeters = FVector2D::Max(Extent.GetSize(), FVector2D(0.1, 0.1));
	Placement.DistanceCentimeters = DistanceCentimeters;
	return Placement;
}

TOptional<float> ALanguageExchangeDetectionLabels::MeasureDistanceCentimeters(
	const FLanguageExchangeDetection& Detection) const
{
	UMRUKSubsystem* const MixedRealitySubsystem = GetMixedRealitySubsystem();
	if (!LanguageExchange::UserInterface::UseDepthVariable.GetValueOnGameThread() || !IsValid(MixedRealitySubsystem)
		|| MixedRealitySubsystem->EnvironmentRaycasterStatus() != EMRUKEnvironmentRaycasterStatus::Ready)
	{
		return {};
	}

	// Rays through the box centre and halfway to each corner. Some may pass the object (through gaps, or past its
	// edge) or hit something in front of it (a hand); the median outvotes them.
	const FVector& Origin = Detection.WorldRayOrigin;
	const FVector& CentreDirection = Detection.WorldRayDirection;
	TArray<FVector, TInlineAllocator<5>> Directions = { CentreDirection };
	for (const FVector& CornerDirection : Detection.WorldCornerDirections)
	{
		Directions.Add((CentreDirection + CornerDirection).GetSafeNormal());
	}

	TArray<double, TInlineAllocator<5>> Distances;
	for (const FVector& Direction : Directions)
	{
		const FMRUKEnvironmentRaycastHit Hit =
			MixedRealitySubsystem->RaycastEnvironment(Origin, Direction, MaximumDistanceCentimeters);
		if (Hit.status == EMRUKEnvironmentRaycastHitStatus::Hit)
		{
			// Along the centre ray, which is how the frame's distance is measured
			Distances.Add(FVector::DotProduct(Hit.point - Origin, CentreDirection));
		}
	}
	if (Distances.IsEmpty())
	{
		return {};
	}

	Distances.Sort();
	const double MedianDistance = Distances[Distances.Num() / 2];
	return FMath::Clamp(static_cast<float>(MedianDistance), MinimumDistanceCentimeters, MaximumDistanceCentimeters);
}

float ALanguageExchangeDetectionLabels::EstimateDistanceCentimeters(const FString& ClassName,
	const TConstArrayView<FVector> CornerDirections) const
{
	using namespace LanguageExchange::UserInterface;

	const float* const TypicalSize = TypicalSizeCentimeters.Find(ClassName);
	if (TypicalSize == nullptr)
	{
		return DefaultDistanceCentimeters;
	}

	// Assume the object's largest dimension spans the box's larger angle
	const double Angle = GetAngularSizeRadians(CornerDirections);
	if (Angle <= UE_KINDA_SMALL_NUMBER)
	{
		return DefaultDistanceCentimeters;
	}

	const double Distance = *TypicalSize / (2.0 * FMath::Tan(Angle * 0.5));
	return FMath::Clamp(static_cast<float>(Distance), MinimumDistanceCentimeters, MaximumDistanceCentimeters);
}

UMRUKSubsystem* ALanguageExchangeDetectionLabels::GetMixedRealitySubsystem() const
{
	const UGameInstance* const GameInstance = GetGameInstance();
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMRUKSubsystem>() : nullptr;
}

FLanguageExchangeTrackedLabel ALanguageExchangeDetectionLabels::CreateTrackedLabel()
{
	FLanguageExchangeTrackedLabel TrackedLabel;
	TrackedLabel.Id = NextTrackedLabelId++;
	if (SpareFrames.IsEmpty())
	{
		TrackedLabel.Frame = CreateWidgetComponent(ULanguageExchangeFrameWidget::StaticClass(), FrameSizePixels,
			FVector2D(0.5, 0.5));
	}
	else
	{
		TrackedLabel.Frame = SpareFrames.Pop();
	}

	if (SpareLabels.IsEmpty())
	{
		TrackedLabel.Label = CreateWidgetComponent(ULanguageExchangeLabelWidget::StaticClass(), LabelSizePixels,
			FVector2D(0.5, 1.0));
	}
	else
	{
		TrackedLabel.Label = SpareLabels.Pop();
	}

	TrackedLabel.Frame->SetVisibility(true);
	TrackedLabel.Label->SetVisibility(true);
	return TrackedLabel;
}

void ALanguageExchangeDetectionLabels::ReleaseTrackedLabel(const FLanguageExchangeTrackedLabel& TrackedLabel)
{
	TrackedLabel.Frame->SetVisibility(false);
	TrackedLabel.Label->SetVisibility(false);
	SpareFrames.Add(TrackedLabel.Frame);
	SpareLabels.Add(TrackedLabel.Label);
}

UWidgetComponent* ALanguageExchangeDetectionLabels::CreateWidgetComponent(const TSubclassOf<UUserWidget> WidgetClass,
	const FIntPoint& SizePixels, const FVector2D& Pivot)
{
	UWidgetComponent* const Widget = NewObject<UWidgetComponent>(this);
	Widget->SetupAttachment(RootSceneComponent);
	Widget->SetAbsolute(true, true, true);
	Widget->SetWidgetSpace(EWidgetSpace::World);
	Widget->SetWidgetClass(WidgetClass);
	Widget->SetDrawSize(FVector2D(SizePixels));
	Widget->SetPivot(Pivot);
	Widget->SetBlendMode(EWidgetBlendMode::Transparent);
	Widget->SetTwoSided(true);
	Widget->RegisterComponent();
	Widget->InitWidget();
	return Widget;
}

void ALanguageExchangeDetectionLabels::ShowTestLabel()
{
	// A made-up detection 1 m in front of the start position, about 20 cm in size, that never times out
	FLanguageExchangeDetection Detection;
	Detection.ClassName = TEXT("test");
	Detection.Confidence = 1.0f;
	Detection.bHasWorldRay = true;
	Detection.WorldRayOrigin = FVector(0.0, 0.0, 150.0);
	Detection.WorldRayDirection = FVector::ForwardVector;
	for (const FVector2D& Corner : { FVector2D(-1, 1), FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, -1) })
	{
		Detection.WorldCornerDirections.Add(FVector(1.0, Corner.X * 0.1, Corner.Y * 0.1).GetSafeNormal());
	}

	FLanguageExchangeTrackedLabel& TrackedLabel = TrackedLabels.Add_GetRef(CreateTrackedLabel());
	UpdateTrackedLabel(TrackedLabel, Detection, {}, static_cast<double>(TNumericLimits<float>::Max()));
	TrackedLabel.ShownPlacement = TrackedLabel.TargetPlacement;
	ShowTrackedLabel(TrackedLabel);
	UE_LOG(LogLanguageExchange, Log, TEXT("Showing a test label instead of detections"));
}
