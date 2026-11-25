#include "UI/LanguageExchangeDetectionLabels.h"

#include "Components/TextRenderComponent.h"
#include "Detection/LanguageExchangeObjectDetectionSubsystem.h"
#include "Engine/World.h"

ALanguageExchangeDetectionLabels::ALanguageExchangeDetectionLabels()
{
	PrimaryActorTick.bCanEverTick = false;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootSceneComponent);
}

void ALanguageExchangeDetectionLabels::BeginPlay()
{
	Super::BeginPlay();

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
	int32 LabelCount = 0;
	for (const FLanguageExchangeDetection& Detection : Detections)
	{
		if (!Detection.bHasWorldRay)
		{
			continue;
		}

		const FVector LabelLocation =
			Detection.WorldRayOrigin + Detection.WorldRayDirection * LabelDistanceCentimeters;
		// Text faces along its X axis, so point X back at the camera
		const FRotator LabelRotation = (Detection.WorldRayOrigin - LabelLocation).Rotation();

		UTextRenderComponent* const Label = GetOrCreateLabel(LabelCount);
		Label->SetWorldLocationAndRotation(LabelLocation, LabelRotation);
		Label->SetText(FText::FromString(FString::Printf(TEXT("%s %.2f"), *Detection.ClassName,
			Detection.Confidence)));
		Label->SetVisibility(true);
		++LabelCount;
	}

	for (int32 LabelIndex = LabelCount; LabelIndex < Labels.Num(); ++LabelIndex)
	{
		Labels[LabelIndex]->SetVisibility(false);
	}
}

UTextRenderComponent* ALanguageExchangeDetectionLabels::GetOrCreateLabel(const int32 LabelIndex)
{
	if (Labels.IsValidIndex(LabelIndex))
	{
		return Labels[LabelIndex];
	}

	UTextRenderComponent* const Label = NewObject<UTextRenderComponent>(this);
	Label->SetupAttachment(RootSceneComponent);
	Label->SetAbsolute(true, true, true);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(TextHeightCentimeters);
	Label->SetTextRenderColor(TextColor);
	Label->RegisterComponent();
	Labels.Add(Label);
	return Label;
}
