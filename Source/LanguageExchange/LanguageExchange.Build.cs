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
			"ImageCore",
			"MediaAssets",
			"NNE",
			"RenderCore",
			"RHI",
			"Slate",
			"SlateCore",
		});
	}
}
