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
			// ONNX Runtime for Android (Source/ThirdParty); only adds anything on Android
			"LanguageExchangeOnnxRuntimeAndroid",
			"MediaAssets",
			"MRUtilityKit",
			"NNE",
			// MRUtilityKitSubsystem.h includes OculusXRAnchors' headers
			"OculusXRAnchors",
			// Hand tracking's pointing pose
			"OculusXRInput",
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
