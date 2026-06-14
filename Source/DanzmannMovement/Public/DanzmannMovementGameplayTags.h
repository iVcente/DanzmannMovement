// Copyright (C) 2026 Vicente Danzmann. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Gameplay Tags representing movement-related states.
 */
namespace Danzmann::GameplayTags
{
	DANZMANNMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Sprinting);
	
	/**
	 * Orientation states. UDanzmannMoverComponent is the SOLE writer of these tags and keeps exactly one of them
	 * present on the owner's Ability System Component at a time, mirroring the Pawn's effective orientation (read-only
	 * for everyone else -- query it to know whether the Pawn is strafing or facing its movement direction).
	 *   - OrientToControl: faces the Control Rotation (camera) and strafes.
	 *   - OrientToMovement: faces its movement direction.
	 *
	 * Drive orientation through UDanzmannMoverComponent::SetOrientationOverride() / ClearOrientationOverride(), NOT by
	 * granting these tags via a Gameplay Effect -- a second writer would break the single-tag invariant.
	 */
	DANZMANNMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Orientation_OrientToControl);
	DANZMANNMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Orientation_OrientToMovement);

	/**
	 * Current movement mode, published by UDanzmannMoverComponent onto the owner's Ability System Component as an
	 * always-present loose tag that mirrors the Mover simulation's authoritative mode (read-only -- the sim owns the
	 * truth, this is just a queryable reflection). Exactly one is present at a time. Also used as the value passed to
	 * UDanzmannMoverComponent::SetMovementMode() and the Movement Profile's default to request a mode.
	 */
	DANZMANNMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Walking);
	DANZMANNMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Falling);
	DANZMANNMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Flying);
	DANZMANNMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Mode_Swimming);
}
