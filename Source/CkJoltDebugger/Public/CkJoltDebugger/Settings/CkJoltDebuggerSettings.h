#pragma once

#include "Engine/DeveloperSettings.h"

#include "CkJoltDebuggerSettings.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM()
enum class ECkJoltDebugger_RenderModePref : uint8
{
	Solid,
	Wireframe
};

// --------------------------------------------------------------------------------------------------------------------

/*
 * The camera orientations worth remembering across sessions. Framing presets are deliberately absent: they are
 * actions against whatever is in the world right now, not a state a window can be restored into.
 */
UENUM()
enum class ECkJoltDebugger_CameraPref : uint8
{
	Perspective,
	Top,
	Bottom,
	Left,
	Right,
	Front,
	Back
};

// --------------------------------------------------------------------------------------------------------------------

/*
 * Per-user Jolt debugger preferences. Config=GameUserSettings so they are per-user and available in packaged
 * developer tools, presented under Editor Preferences through GetContainerName — the split this plugin uses for
 * every per-user debugger preference (CkGameplayDebugger/CLAUDE.md § "Settings split").
 *
 * Written through GetMutableDefault<> + SaveConfig() at the moment the user flips a control, read through
 * GetDefault<> when the window constructs.
 */
UCLASS(Config = GameUserSettings, meta = (DisplayName = "Ck Jolt Debugger"))
class CKJOLTDEBUGGER_API UCkJoltDebuggerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual auto
	GetContainerName() const -> FName override
	{
		return TEXT("Editor");
	}
	virtual auto GetCategoryName() const -> FName override { return TEXT("CkGameplayDebugger"); }

	UPROPERTY(Config, EditAnywhere, Category = "Viewport",
		meta = (ToolTip = "How the Jolt debugger viewport draws bodies. Wireframe swaps the facility's materials on the same instances."))
	ECkJoltDebugger_RenderModePref RenderMode = ECkJoltDebugger_RenderModePref::Solid;

	UPROPERTY(Config, EditAnywhere, Category = "Viewport",
		meta = (ToolTip = "The camera orientation the Jolt debugger viewport opens with."))
	ECkJoltDebugger_CameraPref CameraPreset = ECkJoltDebugger_CameraPref::Perspective;

	UPROPERTY(Config, EditAnywhere, Category = "Populations",
		meta = (ToolTip = "Draw rigid bodies composed through CkJoltBody — static, kinematic, and dynamic."))
	bool ShowJoltBodies = true;

	UPROPERTY(Config, EditAnywhere, Category = "Populations",
		meta = (ToolTip = "Draw the baked level geometry extracted into the Jolt static world."))
	bool ShowBakedStaticWorld = true;

	UPROPERTY(Config, EditAnywhere, Category = "Populations",
		meta = (ToolTip = "Draw sensor bodies — the trigger volumes behind CkSpatialQuery probes."))
	bool ShowSensors = true;

	UPROPERTY(Config, EditAnywhere, Category = "Populations",
		meta = (ToolTip = "Draw CkJoltCharacter capsules."))
	bool ShowCharacters = true;
};

// --------------------------------------------------------------------------------------------------------------------
