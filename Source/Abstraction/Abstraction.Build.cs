// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Abstraction : ModuleRules
{
	public Abstraction(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay" });
	}
}
