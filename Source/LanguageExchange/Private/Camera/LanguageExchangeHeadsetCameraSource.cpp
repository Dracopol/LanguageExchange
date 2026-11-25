#include "Camera/LanguageExchangeHeadsetCameraSource.h"

#include "Detection/LanguageExchangeDetectionSettings.h"
#include "LanguageExchange.h"
#include "MRUKPassthroughCameraAccess.h"

#if PLATFORM_ANDROID
#include "AndroidPermissionCallbackProxy.h"
#include "AndroidPermissionFunctionLibrary.h"
#endif

#define LOCTEXT_NAMESPACE "LanguageExchangeHeadsetCameraSource"

namespace LanguageExchange::Camera
{
	namespace
	{
		const FString HeadsetCameraPermission = TEXT("horizonos.permission.HEADSET_CAMERA");
	}
}

void ULanguageExchangeHeadsetCameraSource::Open()
{
	const ULanguageExchangeDetectionSettings* const Settings = GetDefault<ULanguageExchangeDetectionSettings>();

	CameraAccess = NewObject<UMRUKPassthroughCameraAccess>(this);
	CameraAccess->CameraEye = EMRUKCameraEye::Left;
	CameraAccess->ResolutionWidth = Settings->GetHeadsetCameraResolutionPixels().X;
	CameraAccess->ResolutionHeight = Settings->GetHeadsetCameraResolutionPixels().Y;
	CameraAccess->MaxFramerate = Settings->GetHeadsetCameraMaxFramesPerSecond();

	// Creating the GPU resource registers the texture with MRUK, which Play() needs
	CameraTexture = NewObject<UMRUKPassthroughCameraAccessTexture>(this);
	CameraTexture->PassthroughCameraAccess = CameraAccess;
	CameraTexture->UpdateResource();

#if PLATFORM_ANDROID
	const FString& Permission = LanguageExchange::Camera::HeadsetCameraPermission;
	if (!UAndroidPermissionFunctionLibrary::CheckPermission(Permission))
	{
		UE_LOG(LogLanguageExchange, Log, TEXT("Requesting %s"), *Permission);
		UAndroidPermissionCallbackProxy* const PermissionProxy =
			UAndroidPermissionFunctionLibrary::AcquirePermissions(TArray<FString>{ Permission });
		PermissionResultHandle =
			PermissionProxy->OnPermissionsGrantedDelegate.AddUObject(this, &ThisClass::HandlePermissionResult);
		return;
	}
#endif

	Play();
}

void ULanguageExchangeHeadsetCameraSource::Close()
{
	if (IsValid(CameraAccess) && CameraAccess->IsCameraPlaying())
	{
		CameraAccess->Stop();
	}
}

UTexture* ULanguageExchangeHeadsetCameraSource::GetTexture() const
{
	return CameraTexture;
}

FIntPoint ULanguageExchangeHeadsetCameraSource::GetSizePixels() const
{
	if (!IsOpen())
	{
		return FIntPoint::ZeroValue;
	}
	return FIntPoint(CameraTexture->GetWidth(), CameraTexture->GetHeight());
}

void ULanguageExchangeHeadsetCameraSource::HandlePermissionResult(const TArray<FString>& Permissions,
	const TArray<bool>& GrantResults)
{
#if PLATFORM_ANDROID
	UAndroidPermissionCallbackProxy::GetInstance()->OnPermissionsGrantedDelegate.Remove(PermissionResultHandle);
	PermissionResultHandle.Reset();
#endif

	const int32 PermissionIndex = Permissions.IndexOfByKey(LanguageExchange::Camera::HeadsetCameraPermission);
	const bool bIsGranted = GrantResults.IsValidIndex(PermissionIndex) && GrantResults[PermissionIndex];
	if (!bIsGranted)
	{
		UE_LOG(LogLanguageExchange, Error, TEXT("Headset camera permission denied"));
		MarkFailed(LOCTEXT("PermissionDenied",
			"Camera access denied. Allow it in the headset's settings (Privacy & Safety > App Permissions)."));
		return;
	}

	UE_LOG(LogLanguageExchange, Log, TEXT("Headset camera permission granted"));
	Play();
}

void ULanguageExchangeHeadsetCameraSource::Play()
{
	if (!CameraAccess->Play())
	{
		// MRUK logs the reason (LogMRUK)
		UE_LOG(LogLanguageExchange, Error, TEXT("Starting the headset camera failed"));
		MarkFailed(LOCTEXT("PlayFail", "The headset camera couldn't be started (see the log)"));
		return;
	}

	UE_LOG(LogLanguageExchange, Log, TEXT("Headset camera running at %d x %d, up to %d frames per second"),
		CameraAccess->ResolutionWidth, CameraAccess->ResolutionHeight, CameraAccess->MaxFramerate);
	MarkOpen();
}

#undef LOCTEXT_NAMESPACE
