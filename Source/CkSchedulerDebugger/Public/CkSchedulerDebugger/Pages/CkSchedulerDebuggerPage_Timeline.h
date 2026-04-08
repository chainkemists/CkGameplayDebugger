#pragma once

#include "CkSchedulerDebugger/Pages/ICkSchedulerDebuggerPage.h"

class SCkSchedulerDebugger_Timeline;
class SCkSchedulerDebugger_Inspector;
class FCkSchedulerDebugger_ViewModel;

// --------------------------------------------------------------------------------------------------------------------
// Timeline page - waterfall timeline with pump breakdown and inspector panels.
// --------------------------------------------------------------------------------------------------------------------

class FCkSchedulerDebuggerPage_Timeline : public ICkSchedulerDebuggerPage
{
public:
    virtual auto Get_PageName() const -> FText override;
    virtual auto Build_Content(TSharedPtr<FCkSchedulerDebugger_ViewModel> InViewModel) -> TSharedRef<SWidget> override;
    virtual auto Tick(float InDeltaTime) -> void override;
    virtual auto OnSelectionChanged(int32 InProcessorIndex) -> void override;

private:
    auto DoBuildPumpBreakdownPanel() -> TSharedRef<SWidget>;
    auto DoRebuildPumpBreakdown() -> void;

    TSharedPtr<FCkSchedulerDebugger_ViewModel> _ViewModel;
    TSharedPtr<SCkSchedulerDebugger_Timeline> _Timeline;
    TSharedPtr<SScrollBox> _PumpBreakdownScrollBox;
    TSharedPtr<SVerticalBox> _PumpBreakdownContent;
    FDelegateHandle _DataRefreshedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
