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
	// On PC (Link), MRUK's camera stop crashes inside its native library when play ends (the XR session is already
	// gone by then), so the camera is only stopped on the headset itself
#if PLATFORM_ANDROID
	if (IsValid(CameraAccess) && CameraAccess->IsCameraPlaying())
	{
		CameraAccess->Stop();
	}
#endif
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

bool ULanguageExchangeHeadsetCameraSource::TryGetCameraPose(FTransform& OutCameraPose) const
{
	if (!IsOpen() || !CameraAccess->IsCameraPlaying())
	{
		return false;
	}

	// MRUK returns the pose at the time the latest camera image was taken
	FVector Position;
	FQuat Orientation;
	CameraAccess->GetCameraPose(Position, Orientation);
	OutCameraPose = FTransform(Orientation, Position);
	return true;
}

bool ULanguageExchangeHeadsetCameraSource::TryGetImageRay(const FVector2D& NormalizedImagePoint,
	const FTransform& CameraPose, FVector& OutOrigin, FVector& OutDirection) const
{
	if (!IsOpen())
	{
		return false;
	}

	// Same as UMRUKPassthroughCameraAccessSubsystem::ViewportPointToWorldSpaceRay, but with a given pose instead of
	// the latest one: detection finishes ~0.1 s after the image was taken, and the head moves meanwhile
	const FMRUKCameraIntrinsics Intrinsics = CameraAccess->GetCameraIntrinsics();
	if (Intrinsics.FocalLength.X == 0.0 || Intrinsics.FocalLength.Y == 0.0)
	{
		return false;
	}

	// The image is a centred crop of the sensor with the image's aspect ratio
	const FVector2D SensorResolution(Intrinsics.SensorResolution.X, Intrinsics.SensorResolution.Y);
	const FVector2D ImageResolution(CameraAccess->ResolutionWidth, CameraAccess->ResolutionHeight);
	FVector2D CropScale = ImageResolution / SensorResolution;
	CropScale /= FMath::Max(CropScale.X, CropScale.Y);
	const FVector2D CropSize = SensorResolution * CropScale;
	const FVector2D CropOffset = (SensorResolution - CropSize) * 0.5;
	// MRUK's viewport points start at the image's bottom left (Unity's convention, positive Z is up); ours start at
	// the top left, as the image is stored
	const FVector2D ViewportPoint(NormalizedImagePoint.X, 1.0 - NormalizedImagePoint.Y);
	const FVector2D SensorPoint = CropOffset + CropSize * ViewportPoint;

	const FVector DirectionInCamera(1.0,
		(SensorPoint.X - Intrinsics.PrincipalPoint.X) / Intrinsics.FocalLength.X,
		(SensorPoint.Y - Intrinsics.PrincipalPoint.Y) / Intrinsics.FocalLength.Y);
	OutOrigin = CameraPose.GetLocation();
	OutDirection = CameraPose.GetRotation().RotateVector(DirectionInCamera.GetSafeNormal()).GetSafeNormal();
	return true;
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
