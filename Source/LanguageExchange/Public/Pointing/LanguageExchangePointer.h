#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "LanguageExchangePointer.generated.h"

class ALanguageExchangeDetectionLabels;
class UMRUKSubsystem;
class UWidgetComponent;

// The player's pointing: a beam from each hand (hand tracking) or controller, with a dot where it hits a shown object
// or a real surface (the headset's depth sensing). The shown objects the beams point at are highlighted by the
// detection labels. Spawned by the player controller when a headset is active.
UCLASS()
class ALanguageExchangePointer : public AActor
{
	GENERATED_BODY()

public:
	// Setup
	ALanguageExchangePointer();

	// Update
	virtual void Tick(float DeltaSeconds) override;

	// Functions

	// The objects to point at
	void SetDetectionLabels(ALanguageExchangeDetectionLabels* InDetectionLabels);

protected:
	// Setup
	virtual void BeginPlay() override;

private:
	// Functions
	bool TryGetPointerRay(EControllerHand Hand, FVector& OutOrigin, FVector& OutDirection) const;
	TOptional<double> TraceSurfaceCentimeters(const FVector& Origin, const FVector& Direction) const;
	void ShowBeam(UWidgetComponent* Beam, UWidgetComponent* End, const FVector& Origin, const FVector& Direction,
		double LengthCentimeters, bool bShowEnd) const;
	FVector GetViewLocation() const;
	UMRUKSubsystem* GetMixedRealitySubsystem() const;
	UWidgetComponent* CreateFillComponent(const FName& Name);
	void SetUpFillComponent(UWidgetComponent* Component, const FIntPoint& SizePixels, const FVector2D& Pivot,
		const FLinearColor& Color, bool bIsRound) const;

	// Components
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UWidgetComponent> LeftBeamComponent;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UWidgetComponent> RightBeamComponent;

	// The dots where the beams hit something
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UWidgetComponent> LeftEndComponent;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UWidgetComponent> RightEndComponent;

	// Settings
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Pointing", meta = (ClampMin = "0.01", Units = "cm"))
	float BeamWidthCentimeters = 0.3f;

	// How far the beam looks for a real surface to end on
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Pointing", meta = (ClampMin = "10", Units = "cm"))
	float MaximumBeamLengthCentimeters = 400.0f;

	// When it hits nothing
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Pointing", meta = (ClampMin = "1", Units = "cm"))
	float UnendedBeamLengthCentimeters = 100.0f;

	// Dot size at 1 m from the eyes; scaled with distance so the dot always looks the same size
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Pointing", meta = (ClampMin = "0.1", Units = "cm"))
	float EndSizeAtOneMeterCentimeters = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Pointing")
	FLinearColor BeamColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);

	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Pointing")
	FLinearColor EndColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

	// Resolutions of the beam and dot widgets, which are stretched to their size in the room
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Pointing")
	FIntPoint BeamSizePixels = FIntPoint(8, 256);

	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Pointing")
	FIntPoint EndSizePixels = FIntPoint(64, 64);

	// State
	TWeakObjectPtr<ALanguageExchangeDetectionLabels> DetectionLabels;
};
