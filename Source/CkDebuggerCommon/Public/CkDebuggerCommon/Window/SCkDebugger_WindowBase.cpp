#include "SCkDebugger_WindowBase.h"

#include "CkDebuggerRefreshGate.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"

// ====================================================================================================================

SCkDebugger_WindowBase::~SCkDebugger_WindowBase()
{
	// Use the cached id — Get_WindowId() is pure-virtual and the derived vtable
	// has already been torn down by this point.
	if (_RegisteredWithGate && !_CachedWindowId.IsNone())
	{
		FCkDebuggerRefreshGate::Unregister_Window(_CachedWindowId);
	}
}

auto SCkDebugger_WindowBase::Get_WindowDisplayName() const -> FText
{
	return FText::FromName(Get_WindowId());
}

auto SCkDebugger_WindowBase::Set_OwningTab(TWeakPtr<SDockTab> InTab) -> void
{
	_OwningTab = InTab;

	// Refresh the gate's tab ref if we're already registered (module typically
	// calls Set_OwningTab after Construct has already registered us).
	if (_RegisteredWithGate && !_CachedWindowId.IsNone())
	{
		FCkDebuggerRefreshGate::Register_Window(_CachedWindowId, _OwningTab);
	}
}

auto SCkDebugger_WindowBase::Register_WithGate() -> void
{
	// Cache at registration time — derived vtable is still valid here.
	_CachedWindowId = Get_WindowId();
	FCkDebuggerRefreshGate::Register_Window(_CachedWindowId, _OwningTab);
	_RegisteredWithGate = true;

	// Seed the baseline so the first gated poll can only report a change the user actually made.
	if (const auto* Settings = UCkDebuggerStyleSettings::Get())
	{ _LastSeenStyleRevision = Settings->Get_Revision(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebugger_WindowBase::Tick(
	const FGeometry& InAllottedGeometry,
	double           InCurrentTime,
	float            InDeltaTime)
	-> void
{
	SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

	Poll_StyleRevision();
}

auto SCkDebugger_WindowBase::OnStyleRevisionChanged() -> void
{
	// Visual axes apply live through attribute lambdas; only a window with STRUCTURAL axis
	// consumers needs to do anything here.
}

auto SCkDebugger_WindowBase::Poll_StyleRevision() -> void
{
	if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(_CachedWindowId))
	{ return; }

	const auto* Settings = UCkDebuggerStyleSettings::Get();

	if (Settings == nullptr)
	{ return; }

	const auto Revision = Settings->Get_Revision();

	if (Revision == _LastSeenStyleRevision)
	{ return; }

	_LastSeenStyleRevision = Revision;

	OnStyleRevisionChanged();
}

// ====================================================================================================================
