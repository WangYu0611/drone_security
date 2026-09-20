// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UE5DroneControl : ModuleRules
{
	public UE5DroneControl(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;
		// Cesium's public headers use nonstd::expected. Its no-exception MSVC
		// fallback includes raw windows.h and leaks max/GetObject/UpdateResource
		// macros into consumers, so match CesiumRuntime's build contract.
		bEnableExceptions = true;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"WebBrowserWidget",
			"Sockets",
            "Networking",
			"Slate",
			"SlateCore",
			"HTTP",
			"WebSockets",
			"Json",
			"JsonUtilities",
			"CesiumRuntime",
			"ProceduralMeshComponent",
			"ImageWrapper"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd"); // Command integration automation (PIE).
        }

		PublicIncludePaths.AddRange(new string[] {
			"UE5DroneControl",
			"UE5DroneControl/Variant_Strategy",
			"UE5DroneControl/Variant_Strategy/UI",
			"UE5DroneControl/Variant_TwinStick",
			"UE5DroneControl/Variant_TwinStick/AI",
			"UE5DroneControl/Variant_TwinStick/Gameplay",
			"UE5DroneControl/Variant_TwinStick/UI",
			"UE5DroneControl/DroneOps",
			"UE5DroneControl/DroneOps/Core",
			"UE5DroneControl/DroneOps/Control",
			"UE5DroneControl/DroneOps/Drone",
			"UE5DroneControl/DroneOps/Interfaces",
			"UE5DroneControl/DroneOps/Network",
			"UE5DroneControl/PathEditor",
			"UE5DroneControl/RuntimeInteraction",
			"UE5DroneControl/TaskSystem"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
