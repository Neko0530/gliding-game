using UnrealBuildTool;
public class WingsuitEditorTarget : TargetRules
{
	public WingsuitEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Wingsuit");
	}
}
