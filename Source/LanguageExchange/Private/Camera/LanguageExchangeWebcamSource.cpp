#include "Camera/LanguageExchangeWebcamSource.h"

#include "Detection/LanguageExchangeDetectionSettings.h"
#include "LanguageExchange.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/MediaBlueprintFunctionLibrary.h"

#define LOCTEXT_NAMESPACE "LanguageExchangeWebcamSource"

void ULanguageExchangeWebcamSource::Open()
{
	TArray<FMediaCaptureDevice> CameraDevices;
	UMediaBlueprintFunctionLibrary::EnumerateVideoCaptureDevices(CameraDevices);
	for (int32 DeviceIndex = 0; DeviceIndex < CameraDevices.Num(); ++DeviceIndex)
	{
		UE_LOG(LogLanguageExchange, Log, TEXT("Webcam %d: %s (%s)"), DeviceIndex,
			*CameraDevices[DeviceIndex].DisplayName.ToString(), *CameraDevices[DeviceIndex].Url);
	}

	if (CameraDevices.IsEmpty())
	{
		UE_LOG(LogLanguageExchange, Error, TEXT("No webcam found"));
		MarkFailed(LOCTEXT("NoCamera", "No camera found"));
		return;
	}

	int32 CameraIndex = GetDefault<ULanguageExchangeDetectionSettings>()->GetCameraIndex();
	if (!CameraDevices.IsValidIndex(CameraIndex))
	{
		UE_LOG(LogLanguageExchange, Warning, TEXT("Webcam %d doesn't exist; using webcam 0"), CameraIndex);
		CameraIndex = 0;
	}

	MediaPlayer = NewObject<UMediaPlayer>(this);
	MediaPlayer->PlayOnOpen = true;
	MediaPlayer->OnMediaOpened.AddDynamic(this, &ThisClass::HandleMediaOpen);
	MediaPlayer->OnMediaOpenFailed.AddDynamic(this, &ThisClass::HandleMediaOpenFail);

	MediaTexture = NewObject<UMediaTexture>(this);
	MediaTexture->SetMediaPlayer(MediaPlayer);
	MediaTexture->UpdateResource();

	const FMediaCaptureDevice& CameraDevice = CameraDevices[CameraIndex];
	UE_LOG(LogLanguageExchange, Log, TEXT("Opening webcam %d: %s"), CameraIndex,
		*CameraDevice.DisplayName.ToString());
	if (!MediaPlayer->OpenUrl(CameraDevice.Url))
	{
		UE_LOG(LogLanguageExchange, Error, TEXT("No media player can open %s"), *CameraDevice.Url);
		MarkFailed(LOCTEXT("CameraUnsupported", "The camera couldn't be opened (see the log)"));
	}
}

void ULanguageExchangeWebcamSource::Close()
{
	if (IsValid(MediaPlayer))
	{
		MediaPlayer->OnMediaOpened.RemoveAll(this);
		MediaPlayer->OnMediaOpenFailed.RemoveAll(this);
		MediaPlayer->Close();
	}
}

UTexture* ULanguageExchangeWebcamSource::GetTexture() const
{
	return MediaTexture;
}

FIntPoint ULanguageExchangeWebcamSource::GetSizePixels() const
{
	if (!IsValid(MediaTexture))
	{
		return FIntPoint::ZeroValue;
	}
	return FIntPoint(MediaTexture->GetWidth(), MediaTexture->GetHeight());
}

void ULanguageExchangeWebcamSource::HandleMediaOpen(FString OpenedUrl)
{
	UE_LOG(LogLanguageExchange, Log, TEXT("Webcam open: %s"), *OpenedUrl);
	MarkOpen();
}

void ULanguageExchangeWebcamSource::HandleMediaOpenFail(FString FailedUrl)
{
	UE_LOG(LogLanguageExchange, Error, TEXT("Opening the webcam failed: %s"), *FailedUrl);
	MarkFailed(LOCTEXT("CameraOpenFail", "The camera couldn't be opened. Is another app using it?"));
}

#undef LOCTEXT_NAMESPACE
