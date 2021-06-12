   // Fill out your copyright notice in the Description page of Project Settings.


#include "AbstractionCharacterMovement.h"
#include "AbstractionCharacter.h"

#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/CapsuleComponent.h"

#include "DrawDebugHelpers.h"

UAbstractionCharacterMovement::UAbstractionCharacterMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Start out braking
	bBrakingFrameTolerated = true;

	// Friction not yet applied during substep
	bAppliedFriction = false;
}

void UAbstractionCharacterMovement::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (bAppliedHL2Velocity)
	{
		bAppliedFriction = false;
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
		bBrakingFrameTolerated = IsMovingOnGround();
	}
	else
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	}
	
	if (UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	if (DrawVelocity)
	{
		DrawDebugLine(
			GetWorld(),
			GetOwner()->GetActorLocation(),
			GetOwner()->GetActorLocation() + (Velocity * 0.1),
			FColor(255, 0, 0),
			false, -1, 0, 2
		);
	}	
}

void UAbstractionCharacterMovement::BeginPlay()
{
	Super::BeginPlay();
}

void UAbstractionCharacterMovement::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (!HasValidData())
	{
		return;
	}

	// Update collision settings if needed
	if (MovementMode == MOVE_NavWalking)
	{
		SetGroundMovementMode(MovementMode);
		// Walking uses only XY velocity
		Velocity.Z = 0.f;
		SetNavWalkingPhysics(true);
	}
	else if (PreviousMovementMode == MOVE_NavWalking)
	{
		if (MovementMode == DefaultLandMovementMode || IsWalking())
		{
			const bool bSucceeded = TryToLeaveNavWalking();
			if (!bSucceeded)
			{
				return;
			}
		}
		else
		{
			SetNavWalkingPhysics(false);
		}
	}

	// React to changes in the movement mode.
	if (MovementMode == MOVE_Walking)
	{
		landingFrictionCounter = 1;
		bCrouchMaintainsBaseLocation = true;
		SetGroundMovementMode(MovementMode);
		// make sure we update our new floor/base on initial entry of the walking physics
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
		AdjustFloorHeight();
		SetBaseFromFloor(CurrentFloor);

		if (bMaintainVerticalAirVelocity)
		{
			float VerticalForce = Velocity.Z; //start with current vertical velocity as force
			VerticalForce *= 1 - FVector::DotProduct(CurrentFloor.HitResult.ImpactNormal, GetOwner()->GetActorUpVector()); //scale force based on slope angle

			FVector AddVector = CurrentFloor.HitResult.ImpactNormal.GetSafeNormal2D(); //vector for converting current Z speed into XY speed (ground state doesn't use Z speed)		
			AddVector *= VerticalForce;

			Velocity.Z = 0.f;
			Velocity += (VerticalForce < 0) ? -AddVector : AddVector; //apply velocity, inversed travelling up/down slopes
		}
		else
		{
			// Walking uses only XY velocity, and must be on a walkable floor, with a Base.
			Velocity.Z = 0.f;
		}
	}
	else
	{
		if (PreviousMovementMode != MOVE_Falling)
		{
			ExitFloor = CurrentFloor;
		}		
		CurrentFloor.Clear();
		bCrouchMaintainsBaseLocation = false;

		if (MovementMode == MOVE_Falling)
		{
			Velocity += GetImpartedMovementBaseVelocity();
			GetCharacterOwner()->Falling();
		}

		SetBase(NULL);

		if (MovementMode == MOVE_None)
		{
			// Kill velocity and clear queued up events
			StopMovementKeepPathing();
			GetCharacterOwner()->ResetJumpState();
			ClearAccumulatedForces();
		}
	}

	if (MovementMode == MOVE_Falling && PreviousMovementMode != MOVE_Falling)
	{
		bool walkableHit = (MaxStepHeight == 0.0) ? false : true;
		if (bMaintainVerticalGroundVelocity)
		{
			FVector Velocity2D = Velocity;
			Velocity2D.Z = 0; // remove Z component, player may have gained jump velocity this game tick
			float SlopeRatio = -FVector::DotProduct(ExitFloor.HitResult.ImpactNormal, Velocity.GetSafeNormal2D());

			Velocity.Z += Velocity2D.Size() * SlopeRatio;

			if (bEnforceMinJump && CharacterOwner->bPressedJump && Velocity.Z < JumpZVelocity * MinJumpScale)
			{
				Velocity.Z = JumpZVelocity * MinJumpScale;
			}
		}
		IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
		if (PFAgent)
		{
			PFAgent->OnStartedFalling();
		}
	}

	CharacterOwner->OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
};

bool UAbstractionCharacterMovement::ShouldCatchAir(const FFindFloorResult& OldFloor, const FFindFloorResult& NewFloor)
{
	//float ZDiff = NewFloor.HitResult.ImpactNormal.Z - OldFloor.HitResult.ImpactNormal.Z;
	if (bShouldCatchAir)
	{
		bShouldCatchAir = false;
		return true;
	}
	return Super::ShouldCatchAir(OldFloor, NewFloor);
}

bool UAbstractionCharacterMovement::CanAttemptJump() const
{
	return IsJumpAllowed() && (IsMovingOnGround() || IsFalling()); // Falling included for double-jump and non-zero jump hold time, but validated by character.
}

void UAbstractionCharacterMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{	
	// Do not update velocity when using root motion or when SimulatedProxy and not simulating root motion - SimulatedProxy are repped their Velocity
	if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME || (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && !bWasSimulatingRootMotion))
	{
		return;
	}

	Friction = FMath::Max(0.f, Friction);
	const float MaxAccel = GetMaxAcceleration();
	float MaxSpeed = GetMaxSpeed();

	float slopeAccel = 0;
	if (IsMovingOnGround())
	{
		float DotVal = -FVector::DotProduct(Velocity.GetSafeNormal2D(), CurrentFloor.HitResult.ImpactNormal);
		MaxSpeed += slopeSpeedScale * DotVal * (GetGravityZ() * DeltaTime);
	}

	// Check if path following requested movement
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.0f;
	if (ApplyRequestedMove(DeltaTime, MaxAccel + slopeAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	{
		RequestedAcceleration = RequestedAcceleration.GetClampedToMaxSize(MaxAccel); //PB
		bZeroRequestedAcceleration = false;
	}

	if (bForceMaxAccel)
	{
		// Force acceleration at full speed.
		// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
		if (Acceleration.SizeSquared() > SMALL_NUMBER)
		{
			Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
		}
		else
		{
			Acceleration = MaxAccel * (Velocity.SizeSquared() < SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
		}

		AnalogInputModifier = 1.f;
	}

	// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
	// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
	float MaxInputSpeed = FMath::Max(MaxSpeed * AnalogInputModifier, GetMinAnalogSpeed());
	MaxSpeed = FMath::Max(RequestedSpeed, MaxInputSpeed);

	// Apply braking or deceleration
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);
	const bool bIsGroundMove = IsMovingOnGround() && bBrakingFrameTolerated;

	float SurfaceFriction = 1.0f;
	UPhysicalMaterial* PhysMat = CurrentFloor.HitResult.PhysMaterial.Get();
	if (PhysMat)
	{
		SurfaceFriction = FMath::Min(1.0f, PhysMat->Friction * 1.25f);
	}

	FVector VelocityStart = Velocity;
	CalcFrictionHL2(DeltaTime, Friction, BrakingDeceleration, bIsGroundMove, SurfaceFriction, bFluid, bZeroAcceleration, bZeroRequestedAcceleration, bVelocityOverMax);
	bool PositiveAccelHL2 = CalcAccelerationHL2(DeltaTime, bZeroAcceleration, bZeroRequestedAcceleration, bIsGroundMove, MaxSpeed, RequestedSpeed, RequestedAcceleration, SurfaceFriction);
	FVector VelocityHL2 = Velocity;

	Velocity = VelocityStart;
	CalcFriction(DeltaTime, Friction, BrakingDeceleration, bZeroAcceleration, bZeroRequestedAcceleration, bVelocityOverMax, MaxSpeed, bFluid);
	CalcAcceleration(DeltaTime, bZeroAcceleration, bZeroRequestedAcceleration, MaxSpeed, MaxInputSpeed, RequestedSpeed, RequestedAcceleration);
	
	bAppliedHL2Velocity = false;
	if (!bZeroAcceleration)
	{
		float VelRotationHL2 = FMath::Abs(FMath::RadiansToDegrees(AngleBetweenTwoVectors(VelocityHL2.GetSafeNormal(), VelocityStart.GetSafeNormal())));
		float VelRotation = FMath::Abs(FMath::RadiansToDegrees(AngleBetweenTwoVectors(Velocity.GetSafeNormal(), VelocityStart.GetSafeNormal())));
		if ((VelRotationHL2 > VelRotation) && (!bIsGroundMove || IsCrouching())
			&& (FMath::RadiansToDegrees(AngleBetweenTwoVectors(Velocity.GetSafeNormal(), Acceleration.GetSafeNormal())) <= 112.5))
		{
			Velocity = VelocityHL2;
			bAppliedHL2Velocity = true;
		}
	}

	if (bUseRVOAvoidance)
	{
		CalcAvoidanceVelocity(DeltaTime);
	}
}

void UAbstractionCharacterMovement::CalcFriction(float DeltaTime, float Friction, float BrakingDeceleration, bool bZeroAcceleration, bool bZeroRequestedAcceleration, bool bVelocityOverMax, float MaxSpeed, float bFluid)
{
	// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		if (landingFrictionCounter > 0 || IsCrouching())
		{
			ActualBrakingFriction = 0.0f;
			BrakingDeceleration = 0.0f;
			if (landingFrictionCounter > 0)
			{
				landingFrictionCounter -= 1;
			}			
		}
		ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);

		// Don't allow braking to lower us below max speed if we started above it.
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
	}
	// Apply fluid friction
	if (bFluid)
	{
		Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
	}
}

void UAbstractionCharacterMovement::CalcAcceleration(float DeltaTime, bool bZeroAcceleration, bool bZeroRequestedAcceleration, float MaxSpeed, float MaxInputSpeed, float RequestedSpeed, FVector RequestedAcceleration)
{
	// Apply input acceleration
	if (!bZeroAcceleration)
	{
		const float NewMaxInputSpeed = IsExceedingMaxSpeed(MaxInputSpeed) ? Velocity.Size() : MaxInputSpeed;
		Velocity += Acceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);
	}
	// Apply additional requested acceleration
	if (!bZeroRequestedAcceleration)
	{
		const float NewMaxRequestedSpeed = IsExceedingMaxSpeed(RequestedSpeed) ? Velocity.Size() : RequestedSpeed;
		Velocity += RequestedAcceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxRequestedSpeed);
	}
}

void UAbstractionCharacterMovement::CalcFrictionHL2(float DeltaTime, float Friction, float BrakingDeceleration, bool bIsGroundMove, float SurfaceFriction, float bFluid, bool bZeroAcceleration, bool bZeroRequestedAcceleration, bool bVelocityOverMax)
{
	// Apply friction
	// TODO: HACK: friction applied only once in substepping due to excessive friction, but this is too little for low frame rates
	if (bIsGroundMove && !bAppliedFriction)
	{
		float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction) * SurfaceFriction;
		if (IsCrouching())
		{
			ApplyVelocityBraking(DeltaTime, ActualBrakingFriction * .01f, BrakingDeceleration * .01f);
		}
		else
		{
			ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);
		}
		bAppliedFriction = true;
	}
	// Apply fluid friction
	if (bFluid)
	{
		Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
	}
}

bool UAbstractionCharacterMovement::CalcAccelerationHL2(float DeltaTime, bool bZeroAcceleration, bool bZeroRequestedAcceleration, bool bIsGroundMove, float MaxSpeed, float RequestedSpeed, FVector RequestedAcceleration, float SurfaceFriction)
{
	FVector VelStart = Velocity;
	//float AddSpeed;
	if (!bZeroAcceleration)
	{
		// Clamp acceleration to max speed
		FVector WishAccel = Acceleration.GetClampedToMaxSize2D(MaxSpeed);
		// Find veer
		const FVector AccelDir = WishAccel.GetSafeNormal2D();
		const float Veer = Velocity.X * AccelDir.X + Velocity.Y * AccelDir.Y;
		const float AddSpeed = ((bIsGroundMove && !IsCrouching()) ? WishAccel : WishAccel.GetClampedToMaxSize2D(AirSpeedCap)).Size2D() - Veer;
		if (AddSpeed > 0.0f)
		{
			// Apply acceleration
			float AccelerationMultiplier = (bIsGroundMove && !IsCrouching()) ? GroundAccelerationMultiplier : AirAccelerationMultiplier;
			WishAccel *= AccelerationMultiplier * SurfaceFriction * DeltaTime;
			WishAccel = WishAccel.GetClampedToMaxSize2D(AddSpeed);
			Velocity += WishAccel; // Cap accel
		}
	}
	// Apply additional requested acceleration
	if (!bZeroRequestedAcceleration)
	{
		Velocity += RequestedAcceleration * DeltaTime;
	}
	Velocity = Velocity.GetClampedToMaxSize2D(13470.4f);

	if (Velocity.Size() >= VelStart.Size())
	{
		return true;
	}
	return false;
}

float UAbstractionCharacterMovement::AngleBetweenTwoVectors(FVector VectorA, FVector VectorB)
{
	// Angle between two vectors:
	// Angle = ArcCosine( ( vectorA dot VectorB ) / ( VectorLengthA x VectorLengthB ) )

	float angle = 0;

	// Dot Product
	float dotProduct = FVector::DotProduct(VectorA, VectorB);
	float lengthProduct = VectorA.Size() * VectorB.Size();

	// Angle
	angle = FMath::Acos(dotProduct / lengthProduct);

	return angle;
}

void UAbstractionCharacterMovement::Crouch(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation && !CanCrouchInCurrentState())
	{
		return;
	}

	AttemptFastFall();

	// See if collision is already at desired size.
	if (CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == CrouchedHalfHeight)
	{
		if (!bClientSimulation)
		{
			CharacterOwner->bIsCrouched = true;
		}
		CharacterOwner->OnStartCrouch(0.f, 0.f);
		return;
	}

	if (bClientSimulation && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		// restore collision size before crouching
		ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
		CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		bShrinkProxyCapsule = true;
	}

	// Change collision size to crouching dimensions
	const float ComponentScale = CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const float OldUnscaledHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float OldUnscaledRadius = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	// Height is not allowed to be smaller than radius.
	const float ClampedCrouchedHalfHeight = FMath::Max3(0.f, OldUnscaledRadius, CrouchedHalfHeight);
	CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(OldUnscaledRadius, ClampedCrouchedHalfHeight);
	float HalfHeightAdjust = (OldUnscaledHalfHeight - ClampedCrouchedHalfHeight);
	float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	if (!bClientSimulation)
	{
		// Crouching to a larger height? (this is rare)
		if (ClampedCrouchedHalfHeight > OldUnscaledHalfHeight)
		{
			FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CrouchTrace), false, CharacterOwner);
			FCollisionResponseParams ResponseParam;
			InitCollisionParams(CapsuleParams, ResponseParam);
			const bool bEncroached = GetWorld()->OverlapBlockingTestByChannel(UpdatedComponent->GetComponentLocation() - FVector(0.f, 0.f, ScaledHalfHeightAdjust), FQuat::Identity,
				UpdatedComponent->GetCollisionObjectType(), GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleParams, ResponseParam);

			// If encroached, cancel
			if (bEncroached)
			{
				CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(OldUnscaledRadius, OldUnscaledHalfHeight);
				return;
			}
		}

		if (bCrouchMaintainsBaseLocation)
		{
			// Intentionally not using MoveUpdatedComponent, where a horizontal plane constraint would prevent the base of the capsule from staying at the same spot.
			UpdatedComponent->MoveComponent(FVector(0.f, 0.f, -ScaledHalfHeightAdjust), UpdatedComponent->GetComponentQuat(), true, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
		}

		CharacterOwner->bIsCrouched = true;
	}

	bForceNextFloorCheck = true;

	// OnStartCrouch takes the change from the Default size, not the current one (though they are usually the same).
	const float MeshAdjust = ScaledHalfHeightAdjust;
	ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
	HalfHeightAdjust = (DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - ClampedCrouchedHalfHeight);
	ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	AdjustProxyCapsuleSize();
	CharacterOwner->OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// Don't smooth this change in mesh position
	if ((bClientSimulation && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy) || (IsNetMode(NM_ListenServer) && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy))
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (ClientData)
		{
			ClientData->MeshTranslationOffset -= FVector(0.f, 0.f, MeshAdjust);
			ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
		}
	}
}

bool UAbstractionCharacterMovement::AttemptFastFall()
{
	if (IsFalling() && Velocity.Z <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("FAST FALL"));
		Velocity.Z = FMath::Min(Velocity.Z, FastFallZVelocity);
		return true;
	}
	return false;
}

float UAbstractionCharacterMovement::SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact)
{
	float DotVal = 90 - FMath::RadiansToDegrees(FVector::DotProduct(GetOwner()->GetActorUpVector(), Hit.ImpactNormal));
	if (DotVal > GetWalkableFloorAngle() && DotVal < 89.9)
	{
		UE_LOG(LogTemp, Warning, TEXT("catchair: %f"), DotVal);
		bShouldCatchAir = true;
	}
	return Super::SlideAlongSurface(Delta, Time, InNormal, Hit, bHandleImpact);
	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	FVector Normal(InNormal);
	if (IsMovingOnGround())
	{
		// We don't want to be pushed up an unwalkable surface.
		if (Normal.Z > 0.f)
		{			

		}
		else if (Normal.Z < -KINDA_SMALL_NUMBER)
		{
			// Don't push down into the floor when the impact is on the upper portion of the capsule.
			if (CurrentFloor.FloorDist < MIN_FLOOR_DIST && CurrentFloor.bBlockingHit)
			{
				const FVector FloorNormal = CurrentFloor.HitResult.Normal;
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && (FloorNormal.Z < 1.f - DELTA);
				if (bFloorOpposedToMovement)
				{
					Normal = FloorNormal;
				}
			}
		}
	}

	return Super::Super::SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
}

void UAbstractionCharacterMovement::TwoWallAdjust(FVector& OutDelta, const FHitResult& Hit, const FVector& OldHitNormal) const
{
	const FVector InDelta = OutDelta;
	Super::Super::TwoWallAdjust(OutDelta, Hit, OldHitNormal);

	if (IsMovingOnGround())
	{
		// Allow slides up walkable surfaces, but not unwalkable ones (treat those as vertical barriers).
		if (OutDelta.Z > 0.f)
		{
			// Feels better when stepping up stairs, while running against wall
			if (true)
			{
				// Maintain horizontal velocity
				const float Time = (1.f - Hit.Time);
				const FVector ScaledDelta = OutDelta.GetSafeNormal() * InDelta.Size();
				OutDelta = FVector(InDelta.X, InDelta.Y, ScaledDelta.Z / Hit.Normal.Z) * Time;

				// Should never exceed MaxStepHeight in vertical component, so rescale if necessary.
				// This should be rare (Hit.Normal.Z above would have been very small) but we'd rather lose horizontal velocity than go too high.
				if (OutDelta.Z > MaxStepHeight)
				{
					const float Rescale = MaxStepHeight / OutDelta.Z;
					OutDelta *= Rescale;
				}
			}
			else
			{
				OutDelta.Z = 0.f;
			}
		}
		else if (OutDelta.Z < 0.f)
		{
			// Don't push down into the floor.
			if (CurrentFloor.FloorDist < MIN_FLOOR_DIST && CurrentFloor.bBlockingHit)
			{
				OutDelta.Z = 0.f;
			}
		}
	}
}

FVector UAbstractionCharacterMovement::HandleSlopeBoosting(const FVector& SlideResult, const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit) const
{
	return SlideResult;
}

void UAbstractionCharacterMovement::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (Velocity.IsZero() || !HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	const float FrictionFactor = FMath::Max(0.f, BrakingFrictionFactor);
	Friction = FMath::Max(0.f, Friction * FrictionFactor);
	BrakingDeceleration = FMath::Max(0.f, BrakingDeceleration);
	const bool bZeroFriction = (Friction == 0.f);
	const bool bZeroBraking = (BrakingDeceleration == 0.f);

	if (bZeroFriction && bZeroBraking)
	{
		return;
	}

	const FVector OldVel = Velocity;

	// subdivide braking to get reasonably consistent results at lower frame rates
	// (important for packet loss situations w/ networking)
	float RemainingTime = DeltaTime;
	const float MaxTimeStep = FMath::Clamp(BrakingSubStepTime, 1.0f / 75.0f, 1.0f / 20.0f);

	// Decelerate to brake to a stop
	const FVector RevAccel = (bZeroBraking ? FVector::ZeroVector : (-BrakingDeceleration * Velocity.GetSafeNormal()));
	while (RemainingTime >= MIN_TICK_TIME)
	{
		// Zero friction uses constant deceleration, so no need for iteration.
		const float dt = ((RemainingTime > MaxTimeStep && !bZeroFriction) ? FMath::Min(MaxTimeStep, RemainingTime * 0.5f) : RemainingTime);
		RemainingTime -= dt;

		// apply friction and braking
		Velocity = Velocity + ((-Friction) * Velocity + RevAccel) * dt;

		// Don't reverse direction
		if ((Velocity | OldVel) <= 0.f)
		{
			Velocity = FVector::ZeroVector;
			return;
		}
	}

	// Clamp to zero if nearly zero, or if below min threshold and braking.
	const float VSizeSq = Velocity.SizeSquared();
	if (VSizeSq <= KINDA_SMALL_NUMBER || (!bZeroBraking && VSizeSq <= FMath::Square(BRAKE_TO_STOP_VELOCITY)))
	{
		Velocity = FVector::ZeroVector;
	}
}