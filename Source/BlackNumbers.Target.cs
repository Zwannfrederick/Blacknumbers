using UnrealBuildTool;

public class BlackNumbersTarget : TargetRules
{
	public BlackNumbersTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("BlackNumbers");
	}
}
