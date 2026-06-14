// Copyright (C) 2026 Vicente Danzmann. All Rights Reserved.

using UnrealBuildTool;

public class DanzmannMovement : ModuleRules
{
	public DanzmannMovement(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"DanzmannInput",
				"DeveloperSettings",
				"Engine",
				"EnhancedInput",
				"GameplayAbilities",
				"GameplayTags",
				"ModularGameplay",
				"Mover"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AIModule",
				"CoreUObject",
				"DanzmannAbilities",
				"NavigationSystem",
				"NetworkPrediction",
			}
		);
	}
}
