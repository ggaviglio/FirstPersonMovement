// Fill out your copyright notice in the Description page of Project Settings.


#include "FireDamageActor.h"
#include "DealDamageComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/CapsuleComponent.h"

// Sets default values
AFireDamageActor::AFireDamageActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	DealDamageComponent = CreateDefaultSubobject<UDealDamageComponent>(TEXT("Deal Damage"));
	if (DealDamageComponent->GetTriggerCapsule())
	{
		RootComponent = DealDamageComponent->GetTriggerCapsule();
	}
	ParticleSystemComponent = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("Particle System"));
	ParticleSystemComponent->SetupAttachment(RootComponent);

}

// Called when the game starts or when spawned
void AFireDamageActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AFireDamageActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (DealDamageComponent)
	{
		CurrentTimer += DeltaTime;
		if (CurrentTimer >= ToggleTime)
		{
			if (ParticleSystemComponent)
			{
				ParticleSystemComponent->ToggleActive();
			}
			DealDamageComponent->SetActive(!DealDamageComponent->IsActive());
			CurrentTimer = 0.0f;
		}
	}
}

