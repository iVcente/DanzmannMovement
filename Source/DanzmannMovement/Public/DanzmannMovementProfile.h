// Copyright (C) 2026 Vicente Danzmann. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "DanzmannMovementProfile.generated.h"

/**
 * Data Asset describing a movement profile (speeds, accelerations, modifier multipliers, default orientation, etc.)
 * for a Pawn.
 */
UCLASS()
class DANZMANNMOVEMENT_API UDanzmannMovementProfile : public UDataAsset
{
	GENERATED_BODY()

	public:
		/**
		 * Default Movement Mode requested for a Pawn when this profile is loaded. An unset (invalid) 
		 * Gameplay Tag is treated as Movement.Mode.Walking. The Mover simulation remains authoritative over
		 * the actual mode -- this only seeds the requested/suggested mode.
		 */
		UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Movement", Meta = (Categories = "Movement.Mode"))
		FGameplayTag DefaultMovementMode;
		
		/**
		 * Maximum ground walk speed in cm/s. Drives the base MaxSpeed in UCommonLegacyMovementSettings.
		 */
		UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Movement|Walking", Meta = (ClampMin = 0.0f, UIMin = 0.0f, ForceUnits = "cm/s"))
		float MaxWalkSpeed = 600.0f;

		/**
		 * Maximum acceleration applied while moving on the ground in cm/s^2.
		 */
		UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Movement|Walking", Meta = (ClampMin = 0.0f, UIMin = 0.0f, ForceUnits = "cm/s^2"))
		float MaxAcceleration = 2048.0f;

		/**
		 * Deceleration applied when no input is active while walking in cm/s^2.
		 */
		UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Movement|Walking", Meta = (ClampMin = 0.0f, UIMin = 0.0f, ForceUnits = "cm/s^2"))
		float BrakingDeceleration = 2000.0f;

		/**
		 * Vertical impulse applied when jumping in cm/s.
		 */
		UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Movement|Jumping", Meta = (ClampMin = 0.0f, UIMin = 0.0f, ForceUnits = "cm/s"))
		float JumpVelocityZ = 500.0f;
		
		/**
		 * Default orientation applied to a Pawn when this profile is loaded. The Mover adds this as an always-present
		 * loose Gameplay Tag on the owner's Ability System Component; orientation override tags (e.g. aim's
		 * Movement.Orientation.OrientToControl) win over it via precedence and revert to it when removed. An unset
		 * (invalid) Gameplay Tag is treated as Movement.Orientation.OrientToControl.
		 */
		UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Movement|Orientation", Meta = (Categories = "Movement.Orientation"))
		FGameplayTag DefaultOrientationMode;

		/**
		 * Maximum rate the Pawn rotates toward its orientation intent, in degrees/s. Lower values produce a
		 * smoother, less robotic turn (the Pawn eases toward the camera/movement direction instead of snapping).
		 * A negative value snaps instantly to the desired direction.
		 */
		UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Movement|Orientation", Meta = (ClampMin = -1.0f, UIMin = -1.0f, ForceUnits = "degrees/s"))
		float TurningRate = 500.0f;

		/**
		 * Speeds velocity-direction changes while turning, to reduce sliding. Higher values make the velocity
		 * follow the new facing more eagerly during a turn.
		 */
		UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Movement|Orientation", Meta = (ClampMin = 0.0f, UIMin = 0.0f, ForceUnits = "Multiplier"))
		float TurningBoost = 8.0f;

		/**
		 * Smooths the Main Skeletal Mesh's vertical motion. The Mover simulation snaps the collision shape up/down in
		 * discrete per-tick jumps over uneven terrain and stairs (floor snapping, step-ups). The shape must snap to
		 * stay grounded, but with this enabled the Skeletal Mesh lags and eases toward the shape height instead of
		 * inheriting every jump, so the character glides. Horizontal movement is unaffected.
		 */
		UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Movement|Vertical Mesh Smoothing")
		bool bEnableMeshVerticalSmoothing = true;

		/**
		 * How quickly the mesh catches up to the shape's vertical position. Higher values are snappier (less
		 * smoothing); lower values are floatier.
		 */
		UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Movement|Vertical Mesh Smoothing", Meta = (ClampMin = 0.0f, UIMin = 0.0f, EditCondition = bEnableMeshVerticalSmoothing))
		float MeshVerticalSmoothingInterpSpeed = 15.0f;

		/**
		 * Maximum distance, in cm, the mesh is allowed to lag behind the capsule. Vertical jumps larger than this
		 * (big drops, teleports) snap instead of smearing. Keep this at or above the tallest stair step you want smoothed.
		 */
		UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Movement|Vertical Mesh Smoothing", Meta = (ClampMin = 0.0f, UIMin = 0.0f, ForceUnits = "cm", EditCondition = bEnableMeshVerticalSmoothing))
		float MeshVerticalSmoothingMaxDistance = 50.0f;
};
