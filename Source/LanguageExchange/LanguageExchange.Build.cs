using UnrealBuildTool;

public class LanguageExchange : ModuleRules
{
	public LanguageExchange(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"DeveloperSettings",
			"UMG",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AndroidPermission",
			"HeadMountedDisplay",
			"ImageCore",
			"MediaAssets",
			"MRUtilityKit",
			"NNE",
			// OculusXRPassthrough's public headers include OculusXRHMD's
			"OculusXRHMD",
			"OculusXRPassthrough",
			"RenderCore",
			"RHI",
			"Slate",
			"SlateCore",
			"XRBase",
		});
	}
}
