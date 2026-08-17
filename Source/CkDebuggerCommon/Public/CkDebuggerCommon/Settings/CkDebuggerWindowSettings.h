#pragma once

#include "Engine/DeveloperSettings.h"

#include "CkDebuggerWindowSettings.generated.h"

// ====================================================================================================================
// Per-user (NOT checked into source) settings for shared Ck debugger-window behavior in Editor and
// packaged developer tools. Read via GetDefault<>; write via GetMutableDefault<> + SaveConfig().
//
// Two independent axes:
//   - Refresh MODE: always-on vs only-when-visible. Controls whether the
//     debugger ticks while its tab is behind another tab or while the editor
//     itself is unfocused.
//   - Refresh RATE cap: upper bound on refresh frequency. Primary first-aid
//     lever for the "scrunching" bug caused by every-tick rebuilds.
//
// Both axes have a global default + a per-window override map. Per-window
// overrides use the UseGlobal sentinel to fall back to the global setting.
// ====================================================================================================================

UENUM()
enum class ECkDebugger_RefreshMode : uint8
{
	UseGlobal       UMETA(DisplayName = "Use Global"),
	AlwaysRefresh   UMETA(DisplayName = "Always Refresh"),
	OnlyWhenVisible UMETA(DisplayName = "Only When Visible"),
};

UENUM()
enum class ECkDebugger_RefreshRateCap : uint8
{
	UseGlobal UMETA(DisplayName = "Use Global"),
	Unlimited UMETA(DisplayName = "Unlimited"),
	Hz60      UMETA(DisplayName = "60 Hz"),
	Hz30      UMETA(DisplayName = "30 Hz"),
	Hz15      UMETA(DisplayName = "15 Hz"),
	Hz5       UMETA(DisplayName = "5 Hz"),
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Config = GameUserSettings, meta = (DisplayName = "Ck Debugger Windows"))
class CKDEBUGGERCOMMON_API UCkDebuggerWindowSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual auto
	GetContainerName() const -> FName override
	{
		return TEXT("Editor");
	}
	virtual auto GetCategoryName() const -> FName override { return TEXT("CkGameplayDebugger"); }

	// ---- Global defaults ---------------------------------------------------
	// OnlyWhenVisible is the conservative default — don't burn frames when the
	// tab is hidden. Users can flip to AlwaysRefresh if they're looking at a
	// docked panel while working elsewhere.
	UPROPERTY(Config, EditAnywhere, Category = "Global")
	ECkDebugger_RefreshMode RefreshMode_Global = ECkDebugger_RefreshMode::OnlyWhenVisible;

	// 30 Hz is plenty for any debugger UI; 60 Hz is overkill and 15 Hz saves
	// meaningful CPU when data is expensive to gather.
	UPROPERTY(Config, EditAnywhere, Category = "Global")
	ECkDebugger_RefreshRateCap RefreshRateCap_Global = ECkDebugger_RefreshRateCap::Hz30;

	// When a CK debugger tab is already open, opening another one docks it as a tab in that
	// window instead of spawning a new floating window. Covers opens through CK surfaces (the
	// launcher, the in-window Debuggers menu, ck.* console commands); the editor's own
	// Tools-menu entries place tabs through the saved layout and are not redirected. Tabs docked
	// this way are not persisted in the layout across editor restarts.
	UPROPERTY(Config, EditAnywhere, Category = "Global")
	bool DockNewDebuggersIntoExistingWindow = false;

	UPROPERTY(Config, EditAnywhere, Category = "3D Viewport",
		meta = (DisplayName = "Fly Camera Speed", ClampMin = "0.00001", ClampMax = "10000.0",
			ToolTip = "Shared per-user flight speed for every common 3D debugger viewport. Hold RMB and use the mouse wheel to adjust it."))
	float ViewportFlyCameraSpeed = 1.0f;

	// ---- Per-window overrides ----------------------------------------------
	// Keys are the window IDs returned by SCkDebugger_WindowBase::Get_WindowId().
	// Known IDs: "EcsDebugger", "GoapDebugger", "SmDebugger", "SchedulerDebugger",
	// "AStarDebugger", "UIDebugger", "DebuggerGallery".
	UPROPERTY(Config, EditAnywhere, Category = "Per Window")
	TMap<FName, ECkDebugger_RefreshMode> RefreshMode_PerWindow;

	UPROPERTY(Config, EditAnywhere, Category = "Per Window")
	TMap<FName, ECkDebugger_RefreshRateCap> RefreshRateCap_PerWindow;
};

// ====================================================================================================================
