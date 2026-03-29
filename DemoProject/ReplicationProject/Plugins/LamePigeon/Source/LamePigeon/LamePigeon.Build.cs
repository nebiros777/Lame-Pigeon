// Source file name: LamePigeon.Build.cs
// Author: Igor Matiushin
// Brief description: Configures Unreal build rules for the LamePigeon plugin module and local ENet sources.

using UnrealBuildTool;
using System.IO;

public class LamePigeon : ModuleRules
{
    public LamePigeon(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "DeveloperSettings", "InputCore"
        });

        string ENetRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/enet"));

        PublicIncludePaths.Add(Path.Combine(ENetRoot, "include"));

        PrivateIncludePaths.Add(ENetRoot);
        PublicSystemLibraries.AddRange(new string[] { "ws2_32.lib", "winmm.lib" });
    }
}
