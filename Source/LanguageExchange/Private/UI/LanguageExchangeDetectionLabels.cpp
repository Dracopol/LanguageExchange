#include "UI/LanguageExchangeDetectionLabels.h"

#include "Components/WidgetComponent.h"
#include "Detection/LanguageExchangeObjectDetectionSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "LanguageExchange.h"
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

		// Corner order of FLanguageExchangeDetection::WorldCornerDirections
		constexpr int32 TopLeft = 0;
		constexpr int32 TopRight = 1;
		constexpr int32 BottomRight = 2;
		constexpr int32 BottomLeft = 3;
	}
}

ALanguageExchangeDetectionLabels::ALanguageExchangeDetectionLabels()
{
	PrimaryActorTick.bCanEverTick = false;

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

void ALanguageExchangeDetectionLabels::HandleDetectionsChange(
	const TConstArrayView<FLanguageExchangeDetection> Detections)
{
	int32 ShownCount = 0;
	for (const FLanguageExchangeDetection& Detection : Detections)
	{
		// Needs the four corner rays (top left, top right, bottom right, bottom left)
		if (!Detection.bHasWorldRay || Detection.WorldCornerDirections.Num() != 4)
		{
			continue;
		}
		ShowDetection(ShownCount, Detection);
		++ShownCount;
	}

	for (int32 Index = ShownCount; Index < Labels.Num(); ++Index)
	{
		Labels[Index]->SetVisibility(false);
		Frames[Index]->SetVisibility(false);
	}
}

void ALanguageExchangeDetectionLabels::ShowDetection(const int32 DetectionIndex,
	const FLanguageExchangeDetection& Detection)
{
	using namespace LanguageExchange::UserInterface;

	// The frame lies in a plane facing the camera at the object's estimated distance; each corner is where its ray
	// meets that plane
	const float DistanceCentimeters = EstimateDistanceCentimeters(Detection);
	const FVector& Origin = Detection.WorldRayOrigin;
	const FVector& CentreDirection = Detection.WorldRayDirection;
	TArray<FVector, TInlineAllocator<4>> Corners;
	for (const FVector& CornerDirection : Detection.WorldCornerDirections)
	{
		const double AlongCentre = FMath::Max(FVector::DotProduct(CornerDirection, CentreDirection), 0.1);
		Corners.Add(Origin + CornerDirection * (DistanceCentimeters / AlongCentre));
	}

	// The camera image's top isn't the world's top (the headset camera image is stored upside down relative to the
	// room), so orient by the world: text stays upright and the label goes on the upper edge
	const FVector ImageTopEdge = (Corners[TopLeft] + Corners[TopRight]) * 0.5;
	const FVector ImageBottomEdge = (Corners[BottomLeft] + Corners[BottomRight]) * 0.5;
	FVector FrameUp = (ImageTopEdge - ImageBottomEdge).GetSafeNormal();
	FVector UpperEdge = ImageTopEdge;
	if (FrameUp.Z < 0.0)
	{
		FrameUp = -FrameUp;
		UpperEdge = ImageBottomEdge;
	}

	// Widgets face along their X axis
	const FVector TowardCamera = -CentreDirection;
	const FRotator FacingRotation = FRotationMatrix::MakeFromXZ(TowardCamera, FrameUp).Rotator();

	const double FrameWidth = FVector::Distance(Corners[TopLeft], Corners[TopRight]);
	const double FrameHeight = FVector::Distance(Corners[TopLeft], Corners[BottomLeft]);
	UWidgetComponent* const Frame = GetOrCreateWidget(Frames, DetectionIndex,
		ULanguageExchangeFrameWidget::StaticClass(), FrameSizePixels, FVector2D(0.5, 0.5));
	Frame->SetWorldLocationAndRotation(Origin + CentreDirection * DistanceCentimeters, FacingRotation);
	// Widget units are pixels: stretch the frame to the object's size
	Frame->SetWorldScale3D(FVector(1.0, FrameWidth / FrameSizePixels.X, FrameHeight / FrameSizePixels.Y));
	if (ULanguageExchangeFrameWidget* const FrameWidget =
		Cast<ULanguageExchangeFrameWidget>(Frame->GetUserWidgetObject()))
	{
		FrameWidget->SetFrameColor(HighlightColor);
	}
	Frame->SetVisibility(true);

	// The label sits on the frame's top edge (its pivot is its bottom centre) and keeps its apparent size
	UWidgetComponent* const Label = GetOrCreateWidget(Labels, DetectionIndex,
		ULanguageExchangeLabelWidget::StaticClass(), LabelSizePixels, FVector2D(0.5, 1.0));
	Label->SetWorldLocationAndRotation(UpperEdge, FacingRotation);
	const double LabelWidth = LabelWidthAtOneMeterCentimeters * DistanceCentimeters / 100.0;
	Label->SetWorldScale3D(FVector(LabelWidth / LabelSizePixels.X));
	if (ULanguageExchangeLabelWidget* const LabelWidget =
		Cast<ULanguageExchangeLabelWidget>(Label->GetUserWidgetObject()))
	{
		LabelWidget->SetLabel(FText::FromString(FString::Printf(TEXT("%s %.2f"), *Detection.ClassName,
			Detection.Confidence)), HighlightColor);
	}
	Label->SetVisibility(true);
}

void ALanguageExchangeDetectionLabels::ShowTestLabel()
{
	// A made-up detection 1 m in front of the start position, about 20 cm in size
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
	ShowDetection(0, Detection);
	UE_LOG(LogLanguageExchange, Log, TEXT("Showing a test label instead of detections"));
}

float ALanguageExchangeDetectionLabels::EstimateDistanceCentimeters(const FLanguageExchangeDetection& Detection) const
{
	using namespace LanguageExchange::UserInterface;

	const float* const TypicalSize = TypicalSizeCentimeters.Find(Detection.ClassName);
	if (TypicalSize == nullptr)
	{
		return DefaultDistanceCentimeters;
	}

	// Assume the object's largest dimension spans the box's larger angle
	const TArray<FVector>& Directions = Detection.WorldCornerDirections;
	const double WidthAngle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Directions[TopLeft], Directions[TopRight]),
		-1.0, 1.0));
	const double HeightAngle = FMath::Acos(FMath::Clamp(
		FVector::DotProduct(Directions[TopLeft], Directions[BottomLeft]), -1.0, 1.0));
	const double Angle = FMath::Max(WidthAngle, HeightAngle);
	if (Angle <= UE_KINDA_SMALL_NUMBER)
	{
		return DefaultDistanceCentimeters;
	}

	const double Distance = *TypicalSize / (2.0 * FMath::Tan(Angle * 0.5));
	return FMath::Clamp(static_cast<float>(Distance), MinimumDistanceCentimeters, MaximumDistanceCentimeters);
}

UWidgetComponent* ALanguageExchangeDetectionLabels::GetOrCreateWidget(TArray<TObjectPtr<UWidgetComponent>>& Pool,
	const int32 Index, const TSubclassOf<UUserWidget> WidgetClass, const FIntPoint& SizePixels, const FVector2D& Pivot)
{
	if (Pool.IsValidIndex(Index))
	{
		return Pool[Index];
	}

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
	Pool.Add(Widget);
	return Widget;
}
