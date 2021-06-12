// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbstractionCharacterMovement.generated.h"

/**
 * 
 */
UCLASS()
class ABSTRACTION_API UAbstractionCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
private:
	/**
	* Default UObject constructor.
	*/
	UAbstractionCharacterMovement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	/** Automatic bunnyhopping */
	UPROPERTY(EditAnywhere, Category = "Z Velocity")
		bool AutoBunnyhop = true;

	/** Automatic bunnyhopping */
	UPROPERTY(EditAnywhere, Category = "Debug")
		bool DrawVelocity = false;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// Z velocity is maintained when entering falling/air state
	UPROPERTY(EditAnywhere, Category = "Z Velocity")
		bool bMaintainVerticalGroundVelocity = true;

	// Z velocity is maintained when entering ground state
	UPROPERTY(EditAnywhere, Category = "Z Velocity")
		bool bMaintainVerticalAirVelocity = true;

	// Enforce a minimum resulting jump velocity. Otherwise, "maintained" Z Velocity could prevent most/all of JumpZVelocity
	UPROPERTY(EditAnywhere, Category = "Z Velocity")
		bool bEnforceMinJump = true;

	// JumpZVelocity is scaled by this number to give the minium resulting velocity from jumping. ("Enforce Min Jump" must be enabled)
	UPROPERTY(EditAnywhere, Category = "Z Velocity", meta = (ClampMax = "1.0", UIMax = "1.0"))
		float MinJumpScale = 0.625f;

	// Initial velocity (instantaneous vertical acceleration) when fast-falling. 
	UPROPERTY(EditAnywhere, Category = "Z Velocity", BlueprintReadWrite, meta = (DisplayName = "Fast-Fall Z Velocity", ClampMax = "0", UIMax = "0"))
		float FastFallZVelocity = -700.0f;

	// The multiplier for acceleration when on ground. 
	UPROPERTY(EditAnywhere, Category = "Accelecration (Source/Quake)")
		float GroundAccelerationMultiplier = 10.0f;

	// The multiplier for acceleration when in air. 
	UPROPERTY(EditAnywhere, Category = "Accelecration (Source/Quake)")
		float AirAccelerationMultiplier = 10.0f;

	 // The vector differential magnitude cap when in air. 
	UPROPERTY(EditAnywhere, Category = "Accelecration (Source/Quake)")
		float AirSpeedCap = 57.15f;

	// The vector differential magnitude cap when in air. 
	UPROPERTY(EditAnywhere)
		float slopeSpeedScale = 10.0f;

	// If the player has already landed for a frame, and breaking may be applied. 
	bool bBrakingFrameTolerated;

	// HACK: friction applied only once in substepping due to excessive friction, but this is too little for low frame rates
	bool bAppliedFriction;

	// flag if hl2 movement is used
	bool bAppliedHL2Velocity;

	float landingFrictionCounter = 0;

	bool bShouldCatchAir = false;

	// When movement mode changes, store Floor Result for converting between XY and Z speed (ground state ignores Z speed) 
	FFindFloorResult ExitFloor;

	virtual void BeginPlay();

	// Called after MovementMode has changed. Base implementation does special handling for starting certain modes, then notifies the CharacterOwner.
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode);

public:
	/**
	 * Whether Character should go into falling mode when walking and changing position, based on an old and new floor result (both of which are considered walkable).
	 * Default implementation always returns false.
	 * @return true if Character should start falling
	 */
	virtual bool ShouldCatchAir(const FFindFloorResult& OldFloor, const FFindFloorResult& NewFloor);

	/**
	 * Returns true if current movement state allows an attempt at jumping. Used by Character::CanJump().
	 */
	virtual bool CanAttemptJump() const;

	/**
	 * Updates Velocity and Acceleration based on the current state, applying the effects of friction and acceleration or deceleration. Does not apply gravity.
	 * This is used internally during movement updates. Normally you don't need to call this from outside code, but you might want to use it for custom movement modes.
	 *
	 * @param	DeltaTime						time elapsed since last frame.
	 * @param	Friction						coefficient of friction when not accelerating, or in the direction opposite acceleration.
	 * @param	bFluid							true if moving through a fluid, causing Friction to always be applied regardless of acceleration.
	 * @param	BrakingDeceleration				deceleration applied when not accelerating, or when exceeding max velocity.
	 */
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration);

	virtual void CalcFriction(float DeltaTime, float Friction, float BrakingDeceleration, bool bZeroAcceleration, bool bZeroRequestedAcceleration, bool bVelocityOverMax, float MaxSpeed, float bFluid);
	
	virtual void CalcFrictionHL2(float DeltaTime, float Friction, float BrakingDeceleration, bool bIsGroundMove, float SurfaceFriction, float bFluid, bool bZeroAcceleration, bool bZeroRequestedAcceleration, bool bVelocityOverMax);
	
	void CalcAcceleration(float DeltaTime, bool bZeroAcceleration, bool bZeroRequestedAcceleration, float MaxSpeed, float MaxInputSpeed, float RequestedSpeed, FVector RequestedAcceleration);
	
	bool CalcAccelerationHL2(float DeltaTime, bool bZeroAcceleration, bool bZeroRequestedAcceleration, bool bIsGroundMove, float MaxSpeed, float RequestedSpeed, FVector RequestedAcceleration, float SurfaceFriction);

	float AngleBetweenTwoVectors(FVector VectorA, FVector VectorB);

	/**
	 * Checks if new capsule size fits (no encroachment), and call CharacterOwner->OnStartCrouch() if successful.
	 * In general you should set bWantsToCrouch instead to have the crouch persist during movement, or just use the crouch functions on the owning Character.
	 * @param	bClientSimulation	true when called when bIsCrouched is replicated to non owned clients, to update collision cylinder and offset.
	 */
	virtual void Crouch(bool bClientSimulation = false);

	virtual bool AttemptFastFall();

protected:
	/** Custom version of SlideAlongSurface that handles different movement modes separately; namely during walking physics we might not want to slide up slopes. */
	virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact) override;

	/** Custom version that allows upwards slides when walking if the surface is walkable. */
	virtual void TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const override;

	/**
	 * Limit the slide vector when falling if the resulting slide might boost the character faster upwards.
	 * @param SlideResult:	Vector of movement for the slide (usually the result of ComputeSlideVector)
	 * @param Delta:		Original attempted move
	 * @param Time:			Amount of move to apply (between 0 and 1).
	 * @param Normal:		Normal opposed to movement. Not necessarily equal to Hit.Normal (but usually is).
	 * @param Hit:			HitResult of the move that resulted in the slide.
	 * @return:				New slide result.
	 */
	virtual FVector HandleSlopeBoosting(const FVector& SlideResult, const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit) const;

	///** Slows towards stop. */
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration);
};
