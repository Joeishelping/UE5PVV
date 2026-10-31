using UnrealBuildTool;

public class PiratesVsVikingsEditorTarget : TargetRules
{
	public PiratesVsVikingsEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PiratesVsVikings");
	}
}
