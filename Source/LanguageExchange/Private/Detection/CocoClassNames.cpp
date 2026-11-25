#include "Detection/CocoClassNames.h"

namespace LanguageExchange::Detection
{
	namespace
	{
		constexpr const TCHAR* CocoClassNames[CocoClassCount] =
		{
			TEXT("person"), TEXT("bicycle"), TEXT("car"), TEXT("motorcycle"), TEXT("airplane"), TEXT("bus"),
			TEXT("train"), TEXT("truck"), TEXT("boat"), TEXT("traffic light"), TEXT("fire hydrant"),
			TEXT("stop sign"), TEXT("parking meter"), TEXT("bench"), TEXT("bird"), TEXT("cat"), TEXT("dog"),
			TEXT("horse"), TEXT("sheep"), TEXT("cow"), TEXT("elephant"), TEXT("bear"), TEXT("zebra"),
			TEXT("giraffe"), TEXT("backpack"), TEXT("umbrella"), TEXT("handbag"), TEXT("tie"), TEXT("suitcase"),
			TEXT("frisbee"), TEXT("skis"), TEXT("snowboard"), TEXT("sports ball"), TEXT("kite"),
			TEXT("baseball bat"), TEXT("baseball glove"), TEXT("skateboard"), TEXT("surfboard"),
			TEXT("tennis racket"), TEXT("bottle"), TEXT("wine glass"), TEXT("cup"), TEXT("fork"), TEXT("knife"),
			TEXT("spoon"), TEXT("bowl"), TEXT("banana"), TEXT("apple"), TEXT("sandwich"), TEXT("orange"),
			TEXT("broccoli"), TEXT("carrot"), TEXT("hot dog"), TEXT("pizza"), TEXT("donut"), TEXT("cake"),
			TEXT("chair"), TEXT("couch"), TEXT("potted plant"), TEXT("bed"), TEXT("dining table"), TEXT("toilet"),
			TEXT("tv"), TEXT("laptop"), TEXT("mouse"), TEXT("remote"), TEXT("keyboard"), TEXT("cell phone"),
			TEXT("microwave"), TEXT("oven"), TEXT("toaster"), TEXT("sink"), TEXT("refrigerator"), TEXT("book"),
			TEXT("clock"), TEXT("vase"), TEXT("scissors"), TEXT("teddy bear"), TEXT("hair drier"),
			TEXT("toothbrush"),
		};
	}

	FName GetCocoClassName(const int32 ClassIndex)
	{
		if (ClassIndex < 0 || ClassIndex >= CocoClassCount)
		{
			return FName(*FString::Printf(TEXT("class %d"), ClassIndex));
		}
		return FName(CocoClassNames[ClassIndex]);
	}

	int32 FindCocoClassIndex(const FName ClassName)
	{
		for (int32 ClassIndex = 0; ClassIndex < CocoClassCount; ++ClassIndex)
		{
			if (ClassName == FName(CocoClassNames[ClassIndex]))
			{
				return ClassIndex;
			}
		}
		return INDEX_NONE;
	}
}
