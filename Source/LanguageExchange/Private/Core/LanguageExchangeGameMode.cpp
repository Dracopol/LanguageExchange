#include "Core/LanguageExchangeGameMode.h"

#include "Core/LanguageExchangePlayerController.h"
#include "Core/LanguageExchangePlayerPawn.h"

ALanguageExchangeGameMode::ALanguageExchangeGameMode()
{
	PlayerControllerClass = ALanguageExchangePlayerController::StaticClass();
	DefaultPawnClass = ALanguageExchangePlayerPawn::StaticClass();
}
