// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/PBPlayerCharacter.h"
#include "AbstractionPBPlayerCharacter.generated.h"

/**
 * 
 */
UCLASS()
class ABSTRACTION_API AAbstractionPBPlayerCharacter : public APBPlayerCharacter
{
	GENERATED_BODY()

public:
	/** Called when the actor falls out of the world 'safely' (below KillZ and such) */
	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;

protected:
	void OnDeath(bool IsFellOut);
};