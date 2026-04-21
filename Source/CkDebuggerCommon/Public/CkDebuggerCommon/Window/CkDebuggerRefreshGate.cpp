#include "CkDebuggerRefreshGate.h"

#include "CkDebuggerCommon/Settings/CkDebuggerWindowSettings.h"

#include "Widgets/Docking/SDockTab.h"

// ====================================================================================================================

namespace
{
	auto Resolve_Mode(ECkDebugger_RefreshMode InPer, ECkDebugger_RefreshMode InGlobal) -> ECkDebugger_RefreshMode
	{
		return InPer == ECkDebugger_RefreshMode::UseGlobal ? InGlobal : InPer;
	}

	auto Resolve_Rate(ECkDebugger_RefreshRateCap InPer, ECkDebugger_RefreshRateCap InGlobal) -> ECkDebugger_RefreshRateCap
	{
		return InPer == ECkDebugger_RefreshRateCap::UseGlobal ? InGlobal : InPer;
	}

	auto RateCapToHz(ECkDebugger_RefreshRateCap InRate) -> float
	{
		switch (InRate)
		{
			case ECkDebugger_RefreshRateCap::Unlimited: return -1.0f;
			case ECkDebugger_RefreshRateCap::Hz60:      return 60.0f;
			case ECkDebugger_RefreshRateCap::Hz30:      return 30.0f;
			case ECkDebugger_RefreshRateCap::Hz15:      return 15.0f;
			case ECkDebugger_RefreshRateCap::Hz5:       return 5.0f;
			default:                                    return -1.0f;
		}
	}
}

// ====================================================================================================================

auto FCkDebuggerRefreshGate::GetRecords() -> TMap<FName, FWindowRecord>&
{
	static TMap<FName, FWindowRecord> Instance;
	return Instance;
}

auto FCkDebuggerRefreshGate::Register_Window(FName InWindowId, TWeakPtr<SDockTab> InTab) -> void
{
	auto& Records = GetRecords();
	auto& Record = Records.FindOrAdd(InWindowId);
	Record.Tab = InTab;
}

auto FCkDebuggerRefreshGate::Unregister_Window(FName InWindowId) -> void
{
	GetRecords().Remove(InWindowId);
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebuggerRefreshGate::Get_EffectiveMode(FName InWindowId) -> ECkDebugger_RefreshMode
{
	const auto* Settings = GetDefault<UCkDebuggerWindowSettings>();
	if (Settings == nullptr) { return ECkDebugger_RefreshMode::AlwaysRefresh; }

	const auto PerWindow = Settings->RefreshMode_PerWindow.Contains(InWindowId)
		? Settings->RefreshMode_PerWindow[InWindowId]
		: ECkDebugger_RefreshMode::UseGlobal;

	return Resolve_Mode(PerWindow, Settings->RefreshMode_Global);
}

auto FCkDebuggerRefreshGate::Get_EffectiveRateCap(FName InWindowId) -> ECkDebugger_RefreshRateCap
{
	const auto* Settings = GetDefault<UCkDebuggerWindowSettings>();
	if (Settings == nullptr) { return ECkDebugger_RefreshRateCap::Unlimited; }

	const auto PerWindow = Settings->RefreshRateCap_PerWindow.Contains(InWindowId)
		? Settings->RefreshRateCap_PerWindow[InWindowId]
		: ECkDebugger_RefreshRateCap::UseGlobal;

	return Resolve_Rate(PerWindow, Settings->RefreshRateCap_Global);
}

auto FCkDebuggerRefreshGate::Get_EffectiveRateHz(FName InWindowId) -> float
{
	return RateCapToHz(Get_EffectiveRateCap(InWindowId));
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebuggerRefreshGate::Is_WindowVisible(FName InWindowId) -> bool
{
	const auto* Record = GetRecords().Find(InWindowId);
	if (Record == nullptr) { return true; } // unknown window — don't block

	const auto Tab = Record->Tab.Pin();
	if (!Tab.IsValid()) { return true; }    // no tab info yet — don't block

	return Tab->IsForeground();
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkDebuggerRefreshGate::Should_RefreshNow(FName InWindowId) -> bool
{
	auto& Records = GetRecords();
	auto* Record = Records.Find(InWindowId);

	// ---- Mode gate: visibility ---------------------------------------------
	const auto Mode = Get_EffectiveMode(InWindowId);
	if (Mode == ECkDebugger_RefreshMode::OnlyWhenVisible && !Is_WindowVisible(InWindowId))
	{ return false; }

	// ---- Rate cap (per-frame, not per-call) --------------------------------
	// If we already authorised this engine frame for this window, all
	// subsequent calls in the same frame return true unconditionally —
	// otherwise sibling panels in the same window would be starved
	// (the first to call would update the timer and block the rest).
	if (Record == nullptr)
	{ return true; }

	const auto CurrentFrame = static_cast<uint64>(GFrameCounter);
	if (Record->LastAllowedFrameNumber == CurrentFrame)
	{ return true; }

	const auto Hz = Get_EffectiveRateHz(InWindowId);
	if (Hz > 0.0f)
	{
		const auto Now = FPlatformTime::Seconds();
		const auto Interval = 1.0 / Hz;
		if ((Now - Record->LastAllowedFrameTime) < Interval)
		{ return false; }

		Record->LastAllowedFrameTime = Now;
	}

	Record->LastAllowedFrameNumber = CurrentFrame;
	return true;
}

// ====================================================================================================================
