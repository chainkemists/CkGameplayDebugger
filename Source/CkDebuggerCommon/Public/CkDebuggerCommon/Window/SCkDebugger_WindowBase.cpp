#include "SCkDebugger_WindowBase.h"

#include "CkDebuggerRefreshGate.h"

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
}

// ====================================================================================================================
