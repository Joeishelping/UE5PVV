using UnrealBuildTool;

public class PiratesVsVikingsTarget : TargetRules
{
	public PiratesVsVikingsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PiratesVsVikings");
	}
}
