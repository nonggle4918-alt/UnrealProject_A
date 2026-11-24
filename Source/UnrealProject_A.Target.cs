using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealProject_ATarget : TargetRules
{
	public UnrealProject_ATarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("UnrealProject_A");
	}
}
