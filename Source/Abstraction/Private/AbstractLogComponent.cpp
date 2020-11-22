// Fill out your copyright notice in the Description page of Project Settings.


#include "AbstractLogComponent.h"

// Sets default values for this component's properties
UAbstractLogComponent::UAbstractLogComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	UE_LOG(LogTemp, Error, TEXT("Abtstract Component created"));
	// ...
}


// Called when the game starts
void UAbstractLogComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("UAbstractLogComponent::BeginPlay %s"), *DataString);
	// ...
	
}


// Called every frame
void UAbstractLogComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

