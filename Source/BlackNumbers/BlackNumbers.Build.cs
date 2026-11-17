using UnrealBuildTool;

public class BlackNumbers : ModuleRules
{
	public BlackNumbers(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ChaosVehicles", "AudioMixer" });

		// bn.Benchmark reads frame timing globals.
		PrivateDependencyModuleNames.AddRange(new[] { "RenderCore", "RHI" });

		if (Target.bBuildEditor)
		{
			// Driving smoke test runs the map in PIE.
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
