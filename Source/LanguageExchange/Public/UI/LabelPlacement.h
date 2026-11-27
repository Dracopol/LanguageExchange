#pragma once

#include "CoreMinimal.h"

namespace LanguageExchange::UserInterface
{
	// Where and how large the frame around a detected object is shown. The frame faces along the rotation's X axis;
	// its width runs along Y and its height along Z.
	struct FLabelPlacement
	{
		FVector Centre = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		FVector2D SizeCentimeters = FVector2D::ZeroVector;

		// Distance from the camera, for keeping the frame line and the label the same apparent size
		double DistanceCentimeters = 0.0;

		// The placement a part of the way (Alpha, 0..1) from this one to Target
		FLabelPlacement MoveToward(const FLabelPlacement& Target, const double Alpha) const
		{
			FLabelPlacement Result;
			Result.Centre = FMath::Lerp(Centre, Target.Centre, Alpha);
			Result.Rotation = FQuat::Slerp(Rotation, Target.Rotation, Alpha);
			Result.SizeCentimeters = FMath::Lerp(SizeCentimeters, Target.SizeCentimeters, Alpha);
			Result.DistanceCentimeters = FMath::Lerp(DistanceCentimeters, Target.DistanceCentimeters, Alpha);
			return Result;
		}
	};
}
