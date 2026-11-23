using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealProject_AEditorTarget : TargetRules
{
	public UnrealProject_AEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("UnrealProject_A");
	}
}
