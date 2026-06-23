// Copyright (C) 2026 Vicente Danzmann. All Rights Reserved.

#include "DanzmannMovementStatics.h"

#include "AbilitySystemComponent.h"
#include "DanzmannMovementGameplayTags.h"

FGameplayTag UDanzmannMovementStatics::GetCurrentGait(const UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!IsValid(AbilitySystemComponent))
	{
		return FGameplayTag();
	}

	// Highest precedence first so the result stays deterministic if more than one gait Tag is momentarily present
	if (AbilitySystemComponent->HasMatchingGameplayTag(Danzmann::GameplayTags::Status_Gait_Sprinting))
	{
		return Danzmann::GameplayTags::Status_Gait_Sprinting;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(Danzmann::GameplayTags::Status_Gait_Running))
	{
		return Danzmann::GameplayTags::Status_Gait_Running;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(Danzmann::GameplayTags::Status_Gait_Walking))
	{
		return Danzmann::GameplayTags::Status_Gait_Walking;
	}

	return FGameplayTag();
}
