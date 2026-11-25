#include "Core/LanguageExchangePlayerPawn.h"

#include "Camera/CameraComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "LanguageExchange.h"
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
