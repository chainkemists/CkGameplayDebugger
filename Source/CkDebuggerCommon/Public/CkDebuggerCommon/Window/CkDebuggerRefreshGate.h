#pragma once

#include "CoreMinimal.h"

class SDockTab;
enum class ECkDebugger_RefreshMode : uint8;
enum class ECkDebugger_RefreshRateCap : uint8;

// ====================================================================================================================
// Shared service that debugger windows consult before doing expensive Tick
// work. Resolves UCkDebuggerWindowSettings per window + tracks visibility +
// rate-caps. One singleton instance, static API.
//
// Usage from any Tick override that does expensive rebuild work:
//
//   auto Tick(const FGeometry& AllottedGeometry, double Time, float Delta)
//       -> void override
//   {
//       SCompoundWidget::Tick(AllottedGeometry, Time, Delta);
//       if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(TEXT("EcsDebugger")))
//       { return; }
//       // ...expensive work...
//   }
//
// The window base class calls Register_Window on construct and Unregister on
// destruct to hand the gate a weak tab ref for visibility checks.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API FCkDebuggerRefreshGate
{
public:
	/** Main entry — returns true when the caller should run its refresh work this tick. */
	static auto Should_RefreshNow(FName InWindowId) -> bool;

	// ---- Window registration -----------------------------------------------
	// Called by SCkDebugger_WindowBase. Passing an invalid tab is safe — the
	// gate just treats the window as "no tab to query visibility on" and falls
	// back to AlwaysRefresh behaviour for the mode axis.
	static auto Register_Window(FName InWindowId, TWeakPtr<SDockTab> InTab) -> void;
	static auto Unregister_Window(FName InWindowId) -> void;

	// ---- Resolved-value introspection --------------------------------------
	// "Effective" = after resolving UseGlobal sentinels against the global.
	static auto Get_EffectiveMode(FName InWindowId) -> ECkDebugger_RefreshMode;
	static auto Get_EffectiveRateCap(FName InWindowId) -> ECkDebugger_RefreshRateCap;

	/** Rate as float Hz (or -1.0 for Unlimited). Used by UI to display. */
	static auto Get_EffectiveRateHz(FName InWindowId) -> float;

	/** True when the window's tab is currently foreground in its tabwell. */
	static auto Is_WindowVisible(FName InWindowId) -> bool;

private:
	struct FWindowRecord
	{
		TWeakPtr<SDockTab> Tab;

		// Time of the last frame we authorised for this window. Drives the rate cap.
		double LastAllowedFrameTime = 0.0;

		// Engine frame number on which we last authorised this window. Multiple
		// Should_RefreshNow calls within the same frame all pass once the
		// frame has been authorised — otherwise the FIRST panel in a window
		// would update the rate-cap timer and starve all sibling panels for
		// the rest of the frame, throttling each window to ONE panel per frame.
		uint64 LastAllowedFrameNumber = 0;
	};

	static auto GetRecords() -> TMap<FName, FWindowRecord>&;
};

// ====================================================================================================================
