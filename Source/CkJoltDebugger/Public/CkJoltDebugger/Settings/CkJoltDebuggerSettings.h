#pragma once

#include "CkJolt/Subsystem/CkJolt_DebugDrawTarget.h"

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
 * Mirror of ECk_Jolt_DebugDrawColorMode. The facility's enum is a plain C++ one — it never crosses a reflected
 * boundary — so a UENUM twin is what lets the choice be stored as a stable name in the per-user ini rather than
 * as an integer that would silently re-point if the facility ever reorders its modes.
 */
UENUM()
enum class ECkJoltDebugger_ColorModePref : uint8
{
	BodyClass,
	SleepState,
	ObjectLayer,
	ShapeType
};

// --------------------------------------------------------------------------------------------------------------------

/*
 * One remembered camera pose. Stored as the eye plus the projection rather than as a look-at, because that is
 * what the viewport client restores directly — a bookmark that had to re-derive a pivot could not reproduce an
 * orthographic framing at all.
 */
USTRUCT()
struct FCkJoltDebugger_CameraBookmark
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Viewport")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(Config, EditAnywhere, Category = "Viewport")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(Config, EditAnywhere, Category = "Viewport")
	float OrthoWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Viewport")
	bool IsOrthographic = false;

	/*
	 * Whether this slot was ever stored. The slots are DENSE — the digit key is the array index — so an
	 * untouched slot is a default-constructed pose sitting between two real ones, and recalling it would snap
	 * the camera to the origin. A bookmark nobody took has to be inert instead.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Viewport")
	bool IsSet = false;
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

	/*
	 * The RAW bits of ECk_Jolt_DebugDrawFlags. Stored as bits rather than as a reflected flag struct so the ini
	 * text is one stable integer: a per-flag property set would rename its own config keys every time the
	 * facility gains a flag, and silently drop the user's choice on every one of them.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Draw",
		meta = (ToolTip = "Raw bitmask of the Jolt debug-draw flags the viewport's target emits (ECk_Jolt_DebugDrawFlags)."))
	int32 DrawFlags = static_cast<int32>(ECk_Jolt_DebugDrawFlags::Shape);

	UPROPERTY(Config, EditAnywhere, Category = "Draw",
		meta = (ToolTip = "What the Jolt debugger viewport colours bodies by."))
	ECkJoltDebugger_ColorModePref ColorMode = ECkJoltDebugger_ColorModePref::BodyClass;

	UPROPERTY(Config, EditAnywhere, Category = "Selection",
		meta = (ToolTip = "Draw ONLY the selected bodies, releasing every other body's instances."))
	bool IsolateActive = false;

	UPROPERTY(Config, EditAnywhere, Category = "Selection",
		meta = (ToolTip = "Keep the camera's offset to the primary selection as it moves."))
	bool FollowSelection = false;

	UPROPERTY(Config, EditAnywhere, Category = "Viewport",
		meta = (ToolTip = "Draw a ground grid in the Jolt debugger viewport."))
	bool ShowGrid = true;

	UPROPERTY(Config, EditAnywhere, Category = "Draw",
		meta = (ToolTip = "Draw the selected probe's current overlaps: contact points, contact normals, and a line to each overlapping entity."))
	bool ShowProbeResults = false;

	UPROPERTY(Config, EditAnywhere, Category = "Diagnostics",
		meta = (ToolTip = "Linear speed, in cm/s, past which a body is reported as a runaway."))
	float RunawayVelocityCmS = 5000.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Viewport",
		meta = (ToolTip = "Camera poses recalled from the viewport, indexed by their slot."))
	TArray<FCkJoltDebugger_CameraBookmark> CameraBookmarks;
};

// --------------------------------------------------------------------------------------------------------------------
