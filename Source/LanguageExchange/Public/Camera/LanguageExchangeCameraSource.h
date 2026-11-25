#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LanguageExchangeCameraSource.generated.h"

class UTexture;

// A camera that delivers images for object detection, e.g. a PC webcam or the headset's passthrough camera.
// The latest image is a GPU texture, which the detection reads from.
UCLASS(Abstract)
class ULanguageExchangeCameraSource : public UObject
{
	GENERATED_BODY()

public:
	// Functions

	// Starts the camera; IsOpen() turns true once it runs. Failures are logged and reported by GetErrorText().
	virtual void Open()
		PURE_VIRTUAL(ULanguageExchangeCameraSource::Open, );

	virtual void Close()
		PURE_VIRTUAL(ULanguageExchangeCameraSource::Close, );

	// The latest camera image, or nullptr before the camera is open
	virtual UTexture* GetTexture() const
		PURE_VIRTUAL(ULanguageExchangeCameraSource::GetTexture, return nullptr;);

	// Image size in pixels; zero until the camera delivers images
	virtual FIntPoint GetSizePixels() const
		PURE_VIRTUAL(ULanguageExchangeCameraSource::GetSizePixels, return FIntPoint::ZeroValue;);

	// The camera's pose in the world for the latest image. False if the camera has no known pose (e.g. a webcam).
	virtual bool TryGetCameraPose(FTransform& OutCameraPose) const { return false; }

	// The world-space ray through a point of the image (normalised 0..1, (0, 0) top left), seen from CameraPose
	// (as returned by TryGetCameraPose when that image was captured). False if the camera can't tell.
	virtual bool TryGetImageRay(const FVector2D& NormalizedImagePoint, const FTransform& CameraPose,
		FVector& OutOrigin, FVector& OutDirection) const
	{
		return false;
	}

	bool IsOpen() const { return bIsOpen; }
	const FText& GetErrorText() const { return ErrorText; }

protected:
	// Functions
	void MarkOpen() { bIsOpen = true; }

	void MarkFailed(const FText& InErrorText)
	{
		bIsOpen = false;
		ErrorText = InErrorText;
	}

private:
	// State
	bool bIsOpen = false;
	FText ErrorText;
};
