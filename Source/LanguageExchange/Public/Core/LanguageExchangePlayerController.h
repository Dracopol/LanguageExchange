#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LanguageExchangePlayerController.generated.h"

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

	// State
	UPROPERTY()
	TObjectPtr<ULanguageExchangeDetectionWidget> DetectionWidget;
};
