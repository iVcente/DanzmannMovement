// Copyright (C) 2026 Vicente Danzmann. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DanzmannGait.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "DanzmannMovementStatics.generated.h"

class UAbilitySystemComponent;

/**
 * Static helpers for the DanzmannMovement plugin.
 */
UCLASS()
class DANZMANNMOVEMENT_API UDanzmannMovementStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	public:
		/**
		 * Resolve a Pawn's current gait from its Ability System Component.
		 * Gait is an exclusive axis: exactly one Status.Gait.* Tag should be present at a time (see
		 * DanzmannMovementGameplayTags.h). This resolves by precedence (Sprinting > Running > Walking) so the
		 * answer stays well-defined even during the single frame an inhibition cascade is settling. Falls back
		 * to EDanzmannGait::Walk -- the always-present baseline -- when the Ability System Component is null or
		 * carries no gait Tag.
		 * @param AbilitySystemComponent Ability System Component owning the gait Tags.
		 * @return The active gait, or EDanzmannGait::Walk if none is present.
		 */
		UFUNCTION(BlueprintPure, Category = "Danzmann|Movement")
		static EDanzmannGait GetCurrentGait(const UAbilitySystemComponent* AbilitySystemComponent);
};
