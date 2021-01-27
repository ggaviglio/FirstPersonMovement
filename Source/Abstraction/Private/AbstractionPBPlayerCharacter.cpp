// Fill out your copyright notice in the Description page of Project Settings.


#include "AbstractionPBPlayerCharacter.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"

/** Called when the actor falls out of the world 'safely' (below KillZ and such) */
void AAbstractionPBPlayerCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
	OnDeath(true);
}

void AAbstractionPBPlayerCharacter::OnDeath(bool IsFellOut)
{
	APlayerController* PlayerController = GetController<APlayerController>();
	if (PlayerController)
	{
		PlayerController->RestartLevel();
	}
}
