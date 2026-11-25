using System.IO;
using UnrealBuildTool;

// ONNX Runtime for Android (arm64), used on the Quest because NNE has no CPU runtime for Android in UE 5.8.
// See README.md for the version and where it came from.
public class LanguageExchangeOnnxRuntimeAndroid : ModuleRules
{
	public LanguageExchangeOnnxRuntimeAndroid(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "Include"));
			PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "Lib", "arm64-v8a", "libonnxruntime.so"));
			PublicDefinitions.Add("WITH_LANGUAGEEXCHANGE_ONNXRUNTIME=1");

			// Packages the library into the APK
			AdditionalPropertiesForReceipt.Add("AndroidPlugin",
				Path.Combine(ModuleDirectory, "LanguageExchangeOnnxRuntimeAndroid_APL.xml"));
		}
		else
		{
			PublicDefinitions.Add("WITH_LANGUAGEEXCHANGE_ONNXRUNTIME=0");
		}
	}
}
