#pragma once

#include "CoreMinimal.h"

namespace LanguageExchange::Detection
{
	constexpr int32 CocoClassCount = 80;

	// Name of a COCO class in the order YOLO models trained on COCO output them, e.g. 41 is "cup"
	FString GetCocoClassName(int32 ClassIndex);

	// INDEX_NONE if there's no COCO class with that name
	int32 FindCocoClassIndex(FName ClassName);
}
