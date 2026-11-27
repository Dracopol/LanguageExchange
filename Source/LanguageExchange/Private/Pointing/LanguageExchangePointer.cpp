#include "Pointing/LanguageExchangePointer.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "HeadMountedDisplayTypes.h"
#include "IXRTrackingSystem.h"
#include "MRUtilityKitSubsystem.h"
#include "OculusXRInputFunctionLibrary.h"
#include "UI/LanguageExchangeDetectionLabels.h"
#include "UI/LanguageExchangeFillWidget.h"

ALanguageExchangePointer::ALanguageExchangePointer()
{
	// Follows the hands every frame
	PrimaryActorTick.bCanEverTick = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootSceneComponent);

	LeftBeamComponent = CreateFillComponent(TEXT("LeftBeam"));
	RightBeamComponent = CreateFillComponent(TEXT("RightBeam"));
	LeftEndComponent = CreateFillComponent(TEXT("LeftEnd"));
	RightEndComponent = CreateFillComponent(TEXT("RightEnd"));
}

void ALanguageExchangePointer::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ALanguageExchangeDetectionLabels* const Labels = DetectionLabels.Get();
	TArray<int32, TInlineAllocator<2>> PointedLabelIds;
	for (const EControllerHand Hand : { EControllerHand::Left, EControllerHand::Right })
	{
		UWidgetComponent* const Beam = Hand == EControllerHand::Left ? LeftBeamComponent : RightBeamComponent;
		UWidgetComponent* const End = Hand == EControllerHand::Left ? LeftEndComponent : RightEndComponent;

		FVector Origin;
		FVector Direction;
		if (!TryGetPointerRay(Hand, Origin, Direction))
		{
			Beam->SetVisibility(false);
			End->SetVisibility(false);
			continue;
		}

		// The beam ends on the shown object it points at, else on the real surface it hits, else in the air
		int32 PointedLabelId = INDEX_NONE;
		double ObjectDistance = 0.0;
		if (Labels != nullptr && Labels->TryFindPointedLabel(Origin, Direction, PointedLabelId, ObjectDistance))
		{
			PointedLabelIds.Add(PointedLabelId);
			ShowBeam(Beam, End, Origin, Direction, ObjectDistance, true);
		}
		else if (const TOptional<double> SurfaceDistance = TraceSurfaceCentimeters(Origin, Direction))
		{
			ShowBeam(Beam, End, Origin, Direction, SurfaceDistance.GetValue(), true);
		}
		else
		{
			ShowBeam(Beam, End, Origin, Direction, UnendedBeamLengthCentimeters, false);
		}
	}

	if (Labels != nullptr)
	{
		Labels->SetPointedLabels(PointedLabelIds);
	}
}

void ALanguageExchangePointer::SetDetectionLabels(ALanguageExchangeDetectionLabels* const InDetectionLabels)
{
	DetectionLabels = InDetectionLabels;
}

void ALanguageExchangePointer::BeginPlay()
{
	Super::BeginPlay();

	// Beams start at their bottom edge (the hand) and run up the widget; dots are centred on the hit
	for (UWidgetComponent* const Beam : { LeftBeamComponent.Get(), RightBeamComponent.Get() })
	{
		SetUpFillComponent(Beam, BeamSizePixels, FVector2D(0.5, 1.0), BeamColor, false);
	}
	for (UWidgetComponent* const End : { LeftEndComponent.Get(), RightEndComponent.Get() })
	{
		SetUpFillComponent(End, EndSizePixels, FVector2D(0.5, 0.5), EndColor, true);
	}
}

bool ALanguageExchangePointer::TryGetPointerRay(const EControllerHand Hand, FVector& OutOrigin,
	FVector& OutDirection) const
{
	// A controller in this hand: its aim pose
	FXRMotionControllerState ControllerState;
	UHeadMountedDisplayFunctionLibrary::GetMotionControllerState(GetWorld(), EXRSpaceType::UnrealWorldSpace, Hand,
		EXRControllerPoseType::Aim, ControllerState);
	if (ControllerState.bValid && ControllerState.TrackingStatus == ETrackingStatus::Tracked)
	{
		OutOrigin = ControllerState.ControllerLocation;
		OutDirection = ControllerState.ControllerRotation.GetForwardVector();
		return true;
	}

	// Else the tracked hand: Meta's pointing pose (from the shoulder through the hand), in tracking space
	const EOculusXRHandType HandType =
		Hand == EControllerHand::Left ? EOculusXRHandType::HandLeft : EOculusXRHandType::HandRight;
	if (!GEngine->XRSystem.IsValid() || !UOculusXRInputFunctionLibrary::IsPointerPoseValid(HandType))
	{
		return false;
	}
	const FTransform PointerPose =
		UOculusXRInputFunctionLibrary::GetPointerPose(HandType) * GEngine->XRSystem->GetTrackingToWorldTransform();
	OutOrigin = PointerPose.GetLocation();
	OutDirection = PointerPose.GetUnitAxis(EAxis::X);
	return true;
}

TOptional<double> ALanguageExchangePointer::TraceSurfaceCentimeters(const FVector& Origin,
	const FVector& Direction) const
{
	UMRUKSubsystem* const MixedRealitySubsystem = GetMixedRealitySubsystem();
	if (!IsValid(MixedRealitySubsystem)
		|| MixedRealitySubsystem->EnvironmentRaycasterStatus() != EMRUKEnvironmentRaycasterStatus::Ready)
	{
		return {};
	}

	const FMRUKEnvironmentRaycastHit Hit =
		MixedRealitySubsystem->RaycastEnvironment(Origin, Direction, MaximumBeamLengthCentimeters);
	if (Hit.status != EMRUKEnvironmentRaycastHitStatus::Hit)
	{
		return {};
	}
	return FVector::Distance(Origin, Hit.point);
}

void ALanguageExchangePointer::ShowBeam(UWidgetComponent* const Beam, UWidgetComponent* const End,
	const FVector& Origin, const FVector& Direction, const double LengthCentimeters, const bool bShowEnd) const
{
	// A thin strip from the hand along the ray (the widget's up axis), turned around the ray to face the eyes
	const FVector ViewLocation = GetViewLocation();
	FVector TowardView = FVector::VectorPlaneProject(ViewLocation - Origin, Direction);
	if (TowardView.SizeSquared() < UE_KINDA_SMALL_NUMBER)
	{
		TowardView = FVector::VectorPlaneProject(FVector::UpVector, Direction);
	}
	Beam->SetWorldLocationAndRotation(Origin, FRotationMatrix::MakeFromXZ(TowardView, Direction).ToQuat());
	Beam->SetWorldScale3D(
		FVector(1.0, BeamWidthCentimeters / BeamSizePixels.X, LengthCentimeters / BeamSizePixels.Y));
	Beam->SetVisibility(true);

	End->SetVisibility(bShowEnd);
	if (bShowEnd)
	{
		// Faces the eyes and keeps its apparent size
		const FVector EndLocation = Origin + Direction * LengthCentimeters;
		const FVector ToView = ViewLocation - EndLocation;
		End->SetWorldLocationAndRotation(EndLocation, FRotationMatrix::MakeFromX(ToView).ToQuat());
		const double EndSizeCentimeters = EndSizeAtOneMeterCentimeters * ToView.Size() / 100.0;
		End->SetWorldScale3D(FVector(EndSizeCentimeters / EndSizePixels.X));
	}
}

FVector ALanguageExchangePointer::GetViewLocation() const
{
	// The owner is the local player controller, which spawns the pointer
	const APlayerController* const PlayerController = Cast<APlayerController>(GetOwner());
	if (!ensureMsgf(IsValid(PlayerController) && IsValid(PlayerController->PlayerCameraManager),
		TEXT("The pointer needs the player controller as its owner")))
	{
		return GetActorLocation();
	}
	return PlayerController->PlayerCameraManager->GetCameraLocation();
}

UMRUKSubsystem* ALanguageExchangePointer::GetMixedRealitySubsystem() const
{
	const UGameInstance* const GameInstance = GetGameInstance();
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UMRUKSubsystem>() : nullptr;
}

UWidgetComponent* ALanguageExchangePointer::CreateFillComponent(const FName& Name)
{
	// Placed in the room each frame; hidden while its hand doesn't point
	UWidgetComponent* const Component = CreateDefaultSubobject<UWidgetComponent>(Name);
	Component->SetupAttachment(RootSceneComponent);
	Component->SetAbsolute(true, true, true);
	Component->SetWidgetSpace(EWidgetSpace::World);
	Component->SetWidgetClass(ULanguageExchangeFillWidget::StaticClass());
	Component->SetBlendMode(EWidgetBlendMode::Transparent);
	Component->SetTwoSided(true);
	Component->SetVisibility(false);
	return Component;
}

void ALanguageExchangePointer::SetUpFillComponent(UWidgetComponent* const Component, const FIntPoint& SizePixels,
	const FVector2D& Pivot, const FLinearColor& Color, const bool bIsRound) const
{
	Component->SetDrawSize(FVector2D(SizePixels));
	Component->SetPivot(Pivot);
	Component->InitWidget();
	if (ULanguageExchangeFillWidget* const FillWidget =
		Cast<ULanguageExchangeFillWidget>(Component->GetUserWidgetObject()))
	{
		FillWidget->SetFill(Color, bIsRound);
	}
}
