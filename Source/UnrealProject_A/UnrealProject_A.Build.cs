using UnrealBuildTool;

public class UnrealProject_A : ModuleRules
{
	public UnrealProject_A(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"DeveloperSettings",
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "NetCore", "Slate", "SlateCore", "Niagara" });
	}
}
