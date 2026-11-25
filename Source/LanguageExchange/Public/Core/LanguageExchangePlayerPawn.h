#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "LanguageExchangePlayerPawn.generated.h"

class UCameraComponent;

// The local player in mixed reality: a camera that follows the headset, with the real world (passthrough) as the
// background. Without a headset (e.g. the webcam test on PC) it's a plain camera.
UCLASS()
class ALanguageExchangePlayerPawn : public APawn
{
	GENERATED_BODY()

public:
	// Setup
	ALanguageExchangePlayerPawn();

protected:
	// Setup
	virtual void BeginPlay() override;

private:
	// Functions
	void StartPassthrough() const;

	// Components

	// At floor level; the headset's tracking origin
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> TrackingOriginComponent;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> CameraComponent;
};
