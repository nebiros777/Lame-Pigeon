// Source file name: LamePigeonDemo.Build.cs
// Author: Igor Matiushin
// Brief description: Configures Unreal build rules for the demo gameplay module.

using UnrealBuildTool;

public class LamePigeonDemo : ModuleRules
{
	public LamePigeonDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"LamePigeon"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"LamePigeonDemo"
		});
	}
}
