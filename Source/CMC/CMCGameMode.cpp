// Copyright Epic Games, Inc. All Rights Reserved.

#include "CMCGameMode.h"
#include "CMCCharacter.h"
#include "UObject/ConstructorHelpers.h"

ACMCGameMode::ACMCGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
