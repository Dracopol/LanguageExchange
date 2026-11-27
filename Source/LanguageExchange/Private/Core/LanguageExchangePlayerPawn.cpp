#include "Core/LanguageExchangePlayerPawn.h"

#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "LanguageExchange.h"
#include "MRUtilityKitSubsystem.h"
#include "OculusXRPassthroughSubsystem.h"

ALanguageExchangePlayerPawn::ALanguageExchangePlayerPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	TrackingOriginComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TrackingOrigin"));
	SetRootComponent(TrackingOriginComponent);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComponent->SetupAttachment(TrackingOriginComponent);
	CameraComponent->bLockToHmd = true;
}

void ALanguageExchangePlayerPawn::BeginPlay()
{
	Super::BeginPlay();

	if (!UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		UE_LOG(LogLanguageExchange, Log, TEXT("No headset active; skipping passthrough"));
		return;
	}

	// Standing experience: Z = 0 is the real floor, so virtual content can sit on real tables and floors
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);
	StartPassthrough();
	StartDepthSensing();
}

void ALanguageExchangePlayerPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		StopDepthSensing();
	}

	Super::EndPlay(EndPlayReason);
}

void ALanguageExchangePlayerPawn::StartPassthrough() const
{
	// The subsystem only exists when "Passthrough Enabled" is on in Project Settings > Plugins > Meta XR
	UOculusXRPassthroughSubsystem* const PassthroughSubsystem =
		UOculusXRPassthroughSubsystem::GetPassthroughSubsystem(GetWorld());
	if (!IsValid(PassthroughSubsystem))
	{
		UE_LOG(LogLanguageExchange, Warning,
			TEXT("Passthrough isn't available; enable it in Project Settings > Plugins > Meta XR"));
		return;
	}

	// Default parameters: a visible full-view passthrough layer behind everything we render (underlay)
	PassthroughSubsystem->InitializePersistentPassthrough(FOculusXRPersistentPassthroughParameters(),
		FOculusXRPassthrough_LayerResumed_Single());
	UE_LOG(LogLanguageExchange, Log, TEXT("Passthrough started"));
}

void ALanguageExchangePlayerPawn::StartDepthSensing() const
{
	// Takes a few frames to get ready; until then, and if it fails, users of the raycaster check its status and fall
	// back (MRUK logs a failure)
	UMRUKSubsystem* const MixedRealitySubsystem = GetGameInstance()->GetSubsystem<UMRUKSubsystem>();
	if (ensureMsgf(IsValid(MixedRealitySubsystem), TEXT("Depth sensing needs MRUK's subsystem")))
	{
		MixedRealitySubsystem->CreateEnvironmentRaycaster();
	}
}

void ALanguageExchangePlayerPawn::StopDepthSensing() const
{
	// On PC (Link), MRUK's native calls crash once play has ended (the XR session is already gone); there, MRUK's own
	// cleanup releases the raycaster
#if PLATFORM_ANDROID
	if (UMRUKSubsystem* const MixedRealitySubsystem = GetGameInstance()->GetSubsystem<UMRUKSubsystem>())
	{
		MixedRealitySubsystem->DestroyEnvironmentRaycaster();
	}
#endif
}
