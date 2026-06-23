// Copyright (C) 2026 Vicente Danzmann. All Rights Reserved.

#pragma once

#include "DanzmannGait.generated.h"

/**
 * Locomotion gait -- the speed tier a Pawn is moving at. Enum mirror of the exclusive Status.Gait.*
 * Gameplay Tag axis (exactly one Tag present at a time; see DanzmannMovementGameplayTags.h). Resolved
 * from the owner's Ability System Component by UDanzmannMovementStatics::GetCurrentGait, which applies
 * the precedence Sprint > Run > Walk. Exposed as an enum so consumers (e.g. Anim Blueprints driving a
 * Blend Poses by Enum) don't compare Tags.
 */
UENUM(BlueprintType)
enum class EDanzmannGait : uint8
{
	Walk,
	Run,
	Sprint
};
