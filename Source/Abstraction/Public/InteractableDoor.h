// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "InteractableDoor.generated.h"

class UDoorInteractionComponent;

/**
 * 
 */
UCLASS()
class ABSTRACTION_API AInteractableDoor : public AStaticMeshActor
{
	GENERATED_BODY()
	
public:
	AInteractableDoor();

protected:
	UPROPERTY(EditAnywhere, NoClear)
	UDoorInteractionComponent* DoorInteractionComponent;
};
