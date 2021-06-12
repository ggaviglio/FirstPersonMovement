// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbstractionGameMode.h"
#include "AbstractionHUD.h"
#include "AbstractionCharacter.h"
#include "UObject/ConstructorHelpers.h"

AAbstractionGameMode::AAbstractionGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = AAbstractionHUD::StaticClass();
}
