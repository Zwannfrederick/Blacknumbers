using UnrealBuildTool;

public class BlackNumbersEditorTarget : TargetRules
{
	public BlackNumbersEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("BlackNumbers");
	}
}
