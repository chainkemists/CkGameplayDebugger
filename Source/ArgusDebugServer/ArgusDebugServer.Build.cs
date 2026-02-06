using System.IO;
using UnrealBuildTool;

public class ArgusDebugServer : CkModuleRules
{
    public ArgusDebugServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Networking",
            "Sockets",
            "GameplayTags",

            "CkCore",
            "CkEcs",
            "CkEcsExt",
        });
    }
}
