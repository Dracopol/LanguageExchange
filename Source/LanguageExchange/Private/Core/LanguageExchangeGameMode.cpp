#include "Core/LanguageExchangeGameMode.h"

#include "Core/LanguageExchangePlayerController.h"

ALanguageExchangeGameMode::ALanguageExchangeGameMode()
{
	PlayerControllerClass = ALanguageExchangePlayerController::StaticClass();
}
