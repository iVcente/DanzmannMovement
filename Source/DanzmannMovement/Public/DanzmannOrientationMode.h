// Copyright (C) 2026 Vicente Danzmann. All Rights Reserved.

#pragma once

#include "DanzmannOrientationMode.generated.h"

/**
 * How a Pawn orients while moving. Enum mirror of the exclusive Movement.Orientation.* Gameplay Tag
 * axis that UDanzmannMoverComponent owns and publishes (see DanzmannMovementGameplayTags.h).
 *   - OrientToControl: faces the Control Rotation (camera) and strafes.
 *   - OrientToMovement: faces its movement direction.
 */
UENUM(BlueprintType)
enum class EDanzmannOrientationMode : uint8
{
	OrientToControl,
	OrientToMovement
};
