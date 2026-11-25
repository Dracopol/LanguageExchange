#pragma once

#include "CoreMinimal.h"
#include "LanguageExchangeDetection.generated.h"

// One object found in a camera image
USTRUCT(BlueprintType)
struct FLanguageExchangeDetection
{
	GENERATED_BODY()

	// Class name as the model knows it (COCO for YOLO11n), e.g. "cup". A string rather than an FName: FNames keep the
	// spelling the engine registered first, so "mouse" would show as the input key's "Mouse".
	UPROPERTY(BlueprintReadOnly, Category = "LanguageExchange|Detection")
	FString ClassName;

	UPROPERTY(BlueprintReadOnly, Category = "LanguageExchange|Detection")
	int32 ClassIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "LanguageExchange|Detection")
	float Confidence = 0.0f;

	// Box in image coordinates normalised to 0..1, (0, 0) is the top left corner
	UPROPERTY(BlueprintReadOnly, Category = "LanguageExchange|Detection")
	FBox2D NormalizedBounds = FBox2D(ForceInit);

	// The world-space ray from the camera through the box centre, at the time the image was taken. Only set when the
	// camera knows its pose (the headset camera); the object is somewhere along it.
	UPROPERTY(BlueprintReadOnly, Category = "LanguageExchange|Detection")
	bool bHasWorldRay = false;

	UPROPERTY(BlueprintReadOnly, Category = "LanguageExchange|Detection")
	FVector WorldRayOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "LanguageExchange|Detection")
	FVector WorldRayDirection = FVector::ForwardVector;
};
