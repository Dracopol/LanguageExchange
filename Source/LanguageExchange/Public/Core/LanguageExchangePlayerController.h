#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LanguageExchangePlayerController.generated.h"

class ALanguageExchangeDetectionLabels;
class ALanguageExchangePointer;
class ULanguageExchangeDetectionWidget;

UCLASS()
class ALanguageExchangePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Setup
	ALanguageExchangePlayerController();

protected:
	// Setup
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Settings
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|UI")
	TSubclassOf<ULanguageExchangeDetectionWidget> DetectionWidgetClass;

	// Spawned when a headset is active
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|UI")
	TSubclassOf<ALanguageExchangeDetectionLabels> DetectionLabelsClass;

	// Spawned when a headset is active
	UPROPERTY(EditDefaultsOnly, Category = "LanguageExchange|Pointing")
	TSubclassOf<ALanguageExchangePointer> PointerClass;

	// State
	UPROPERTY()
	TObjectPtr<ULanguageExchangeDetectionWidget> DetectionWidget;

	UPROPERTY()
	TObjectPtr<ALanguageExchangeDetectionLabels> DetectionLabels;

	UPROPERTY()
	TObjectPtr<ALanguageExchangePointer> Pointer;
};
