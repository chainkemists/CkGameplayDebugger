#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkSchedulerDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------

class ICkSchedulerDebuggerPage
{
public:
	virtual ~ICkSchedulerDebuggerPage() = default;

	virtual auto Get_PageName() const -> FText = 0;
	virtual auto Build_Content(TSharedPtr<FCkSchedulerDebugger_ViewModel> InViewModel) -> TSharedRef<SWidget> = 0;
	virtual auto Tick(float InDeltaTime) -> void {}
	virtual auto IsActive() const -> bool { return _IsActive; }
	virtual auto Set_IsActive(bool InActive) -> void { _IsActive = InActive; }
	virtual auto OnSelectionChanged(int32 InProcessorIndex) -> void {}

	/**
	 * The Layer-B style selection changed. A page whose sub-widgets are entirely attribute-bound
	 * needs nothing (that is why the default is a no-op); a page that emits rows imperatively routes
	 * this to its own rebuild entry point.
	 */
	virtual auto OnStyleRevisionChanged() -> void {}

protected:
	bool _IsActive = false;
};

// --------------------------------------------------------------------------------------------------------------------
