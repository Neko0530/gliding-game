using UnrealBuildTool;
public class WingsuitTarget : TargetRules
{
	public WingsuitTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Wingsuit");
	}
}
