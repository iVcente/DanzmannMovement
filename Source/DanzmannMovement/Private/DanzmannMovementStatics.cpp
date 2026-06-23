// Copyright (C) 2026 Vicente Danzmann. All Rights Reserved.

#include "DanzmannMovementStatics.h"

#include "AbilitySystemComponent.h"
#include "DanzmannMovementGameplayTags.h"

EDanzmannGait UDanzmannMovementStatics::GetCurrentGait(const UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!IsValid(AbilitySystemComponent))
	{
		return EDanzmannGait::Walk;
	}

	// Highest precedence first so the result stays deterministic if more than one gait Tag is momentarily present
	if (AbilitySystemComponent->HasMatchingGameplayTag(Danzmann::GameplayTags::Status_Gait_Sprinting))
	{
		return EDanzmannGait::Sprint;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(Danzmann::GameplayTags::Status_Gait_Running))
	{
		return EDanzmannGait::Run;
	}

	// Status.Gait.Walking is the always-present baseline; treat "no gait Tag" as Walk too
	return EDanzmannGait::Walk;
}
