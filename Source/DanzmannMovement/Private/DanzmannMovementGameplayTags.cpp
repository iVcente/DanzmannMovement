// Copyright (C) 2026 Vicente Danzmann. All Rights Reserved.

#include "DanzmannMovementGameplayTags.h"

namespace Danzmann::GameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Gait_Walking, "Status.Gait.Walking", "Actor's current gait is walking. Baseline gait, supplied by the always-applied GE_Walk and inhibited while a higher gait is active. Exactly one Status.Gait.* tag is present at a time.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Gait_Running, "Status.Gait.Running", "Actor's current gait is running. Granted by GE_Run; inhibits Walking and is itself inhibited by Sprinting via Ongoing Tag Requirements.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Gait_Sprinting, "Status.Gait.Sprinting", "Actor's current gait is sprinting. Top of the gait precedence (Sprint > Run > Walk); inhibits the lower gaits and is inhibited by none.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Orientation_OrientToControl, "Movement.Orientation.OrientToControl", "Pawn faces its Control Rotation (camera) and strafes. Published by UDanzmannMoverComponent; set via SetOrientationOverride(), do not grant directly through a Gameplay Effect.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Orientation_OrientToMovement, "Movement.Orientation.OrientToMovement", "Pawn faces its movement direction. Published by UDanzmannMoverComponent; set via SetOrientationOverride(), do not grant directly through a Gameplay Effect.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Mode_Walking, "Movement.Mode.Walking", "Pawn is currently walking on the ground. Published by UDanzmannMoverComponent to mirror the Mover sim's current mode.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Mode_Falling, "Movement.Mode.Falling", "Pawn is currently airborne/falling. Published by UDanzmannMoverComponent to mirror the Mover sim's current mode.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Mode_Flying, "Movement.Mode.Flying", "Pawn is currently flying. Published by UDanzmannMoverComponent to mirror the Mover sim's current mode.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Mode_Swimming, "Movement.Mode.Swimming", "Pawn is currently swimming. Published by UDanzmannMoverComponent to mirror the Mover sim's current mode.");
}
