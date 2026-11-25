#pragma once

#include "CoreMinimal.h"
#include "Camera/LanguageExchangeCameraSource.h"
#include "LanguageExchangeHeadsetCameraSource.generated.h"

class UMRUKPassthroughCameraAccess;
class UMRUKPassthroughCameraAccessTexture;

// The Quest's passthrough camera (left eye) through Meta's Mixed Reality Utility Kit. On the headset it asks for the
// headset camera permission first. Needs Passthrough and Passthrough Camera Access enabled in Project Settings >
// Plugins > Meta XR. Resolution and frame rate: Project Settings > LanguageExchange > Object Detection.
UCLASS()
class ULanguageExchangeHeadsetCameraSource : public ULanguageExchangeCameraSource
{
	GENERATED_BODY()

public:
	// Functions
	virtual void Open() override;
	virtual void Close() override;
	virtual UTexture* GetTexture() const override;
	virtual FIntPoint GetSizePixels() const override;

private:
	// Event handlers
	void HandlePermissionResult(const TArray<FString>& Permissions, const TArray<bool>& GrantResults);

	// Functions
	void Play();

	// State
	UPROPERTY()
	TObjectPtr<UMRUKPassthroughCameraAccess> CameraAccess;

	UPROPERTY()
	TObjectPtr<UMRUKPassthroughCameraAccessTexture> CameraTexture;

	FDelegateHandle PermissionResultHandle;
};
