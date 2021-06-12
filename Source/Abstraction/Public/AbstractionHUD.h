// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AbstractionHUD.generated.h"

UCLASS()
class AAbstractionHUD : public AHUD
{
	GENERATED_BODY()

public:
	AAbstractionHUD();

	/** Primary draw call for the HUD */
	virtual void DrawHUD() override;

private:
	/** Crosshair asset pointer */
	class UTexture2D* CrosshairTex;
	
	UPROPERTY(EditAnywhere)
		bool DrawCrosshair = true;
};

