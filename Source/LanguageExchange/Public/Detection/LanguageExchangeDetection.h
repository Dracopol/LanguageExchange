#pragma once

#include "CoreMinimal.h"
#include "LanguageExchangeDetection.generated.h"

// One object found in a camera image
USTRUCT(BlueprintType)
struct FLanguageExchangeDetection
{
	GENERATED_BODY()

	// Class name as the model knows it (COCO for YOLO11n), e.g. "cup"
	UPROPERTY(BlueprintReadOnly, Category = "LanguageExchange|Detection")
	FName ClassName;

	UPROPERTY(BlueprintReadOnly, Category = "LanguageExchange|Detection")
	int32 ClassIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "LanguageExchange|Detection")
	float Confidence = 0.0f;

	// Box in image coordinates normalised to 0..1, (0, 0) is the top left corner
	UPROPERTY(BlueprintReadOnly, Category = "LanguageExchange|Detection")
	FBox2D NormalizedBounds = FBox2D(ForceInit);
};
