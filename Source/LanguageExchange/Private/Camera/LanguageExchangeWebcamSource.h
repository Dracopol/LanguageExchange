#pragma once

#include "CoreMinimal.h"
#include "Camera/LanguageExchangeCameraSource.h"
#include "LanguageExchangeWebcamSource.generated.h"

class UMediaPlayer;
class UMediaTexture;

// A video capture device (e.g. the PC's webcam) through Unreal's Media Framework.
// Which device: Project Settings > LanguageExchange > Object Detection > Camera Index.
UCLASS()
class ULanguageExchangeWebcamSource : public ULanguageExchangeCameraSource
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
	UFUNCTION()
	void HandleMediaOpen(FString OpenedUrl);

	UFUNCTION()
	void HandleMediaOpenFail(FString FailedUrl);

	// State
	UPROPERTY()
	TObjectPtr<UMediaPlayer> MediaPlayer;

	UPROPERTY()
	TObjectPtr<UMediaTexture> MediaTexture;
};
